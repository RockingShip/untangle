//#pragma GCC optimize ("O0") // optimize on demand

/*
 * gload.cc
 *      Create a tree file based on json meta data
 *      Load the optional 'data' tag to populate the nodes.
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2017-2021, xyzzy@rockingship.org
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <fstream>
#include <string>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <stdlib.h>
#include <unistd.h>

#include "context.h"
#include "grouptree.h"

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

/**
 * @date 2021-05-17 22:45:37
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int __attribute__ ((unused)) sig) {
	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}


/**
 * @date 2021-05-13 15:30:14
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct gloadContext_t {

	/// @var {string} name of database
	const char *opt_databaseName;
	/// @var {number} header flags
	uint32_t   opt_flagsSet;
	/// @var {number} header flags
	uint32_t   opt_flagsClr;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned   opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `groupTree_t`.
	unsigned   opt_maxNode;

	/// @var {database_t} - Database store to place results
	database_t *pStore;

	gloadContext_t() {
		opt_databaseName = "untangle.db";
		opt_flagsSet     = 0;
		opt_flagsClr     = 0;
		opt_force        = 0;
		opt_maxNode      = groupTree_t::DEFAULT_MAXNODE;
		pStore           = NULL;
	}

	/**
	 * @date 2021-05-20 23:15:36
	 *
	 * Main entrypoint.
	 * NOTE: Most code taken from `validate.cc`.
	 */
	int main(const char *outputFilename, const char *inputFilename) {

		/*
		 * Load json
		 */

		// load json
		FILE *f = fopen(inputFilename, "r");
		if (!f) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("fopen()"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "errno", json_integer(errno));
			json_object_set_new_nocheck(jError, "errtxt", json_string(strerror(errno)));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		json_error_t jLoadError;
		json_t *jInput = json_loadf(f, 0, &jLoadError);
		if (jInput == 0) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to decode json"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "line", json_integer(jLoadError.line));
			json_object_set_new_nocheck(jError, "text", json_string(jLoadError.text));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}
		fclose(f);

		/*
		 * Create an incomplete tree based on json
		 */
		groupTree_t jsonTree(ctx, *pStore);

		jsonTree.loadFileJson(jInput, inputFilename);

		/*
		 * Import the roots
		 */
		json_t *jData = json_object_get(jInput, "data");
		if (!jData) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: `data' tag not available\n", ctx.timeAsString());
			return 0;
		}

		const char *dataValue = json_string_value(jData);

		/*
		 * Create a real tree
		 */

		groupTree_t newTree(ctx, *pStore, dataValue, opt_maxNode, ctx.flags);

		newTree.flags |= jsonTree.flags & context_t::MAGICMASK_SYSTEM;

		newTree.entryNames = jsonTree.entryNames;
		newTree.rootNames  = jsonTree.rootNames;

		if (newTree.numRoots != jsonTree.numRoots) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("numRoots mismatch"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "expected", json_integer(jsonTree.numRoots));
			json_object_set_new_nocheck(jError, "encountered", json_integer(newTree.numRoots));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		/*
		 * Save data
		 */
		newTree.saveFile(outputFilename);

		if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(outputFilename));
			newTree.headerInfo(jResult);
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		json_delete(jInput);
		return 0;
	}


};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {gloadContext_t} Application context
 */
gloadContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.dat> <input.json>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename>   Database to query [default=%s]\n", app.opt_databaseName);
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --[no-]paranoid [default=%s]\n", ctx.flags & ctx.MAGICMASK_PARANOID ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure [default=%s]\n", ctx.flags & ctx.MAGICMASK_PURE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]rewrite [default=%s]\n", ctx.flags & ctx.MAGICMASK_REWRITE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]cascade [default=%s]\n", ctx.flags & ctx.MAGICMASK_CASCADE ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]shrink [default=%s]\n", ctx.flags &  ctx.MAGICMASK_SHRINK ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]pivot3 [default=%s]\n", ctx.flags &  ctx.MAGICMASK_PIVOT3 ? "enabled" : "disabled");
	}
}

/**
 * @date 2021-05-13 15:28:31
 *
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context.
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 */
int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	for (;;) {
		enum {
			LO_HELP = 1, LO_DEBUG, LO_TIMER, LO_FORCE, LO_MAXNODE,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_DATABASE = 'D', LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"debug",       1, 0, LO_DEBUG},
			{"force",       0, 0, LO_FORCE},
			{"help",        0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"quiet",       2, 0, LO_QUIET},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",     2, 0, LO_VERBOSE},
			//
			{"paranoid",    0, 0, LO_PARANOID},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"pure",        0, 0, LO_PURE},
			{"no-pure",     0, 0, LO_NOPURE},
			{"rewrite",     0, 0, LO_REWRITE},
			{"no-rewrite",  0, 0, LO_NOREWRITE},
			{"cascade",     0, 0, LO_CASCADE},
			{"no-cascade",  0, 0, LO_NOCASCADE},
//			{"shrink",      0, 0, LO_SHRINK},
//			{"no-shrink",   0, 0, LO_NOSHRINK},
//			{"pivot3",      0, 0, LO_PIVOT3},
//			{"no-pivot3",   0, 0, LO_NOPIVOT3},
			//
			{NULL,          0, 0, 0}
		};

		char optstring[64];
		char *cp          = optstring;
		int  option_index = 0;

		for (int i = 0; long_options[i].name; i++) {
			if (isalpha(long_options[i].val)) {
				*cp++ = (char) long_options[i].val;

				if (long_options[i].has_arg)
					*cp++ = ':';
				if (long_options[i].has_arg == 2)
					*cp++ = ':';
			}
		}

		*cp = '\0';

		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_DATABASE:
			app.opt_databaseName = optarg;
			break;
		case LO_DEBUG:
			ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
			break;
		case LO_FORCE:
			app.opt_force++;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_MAXNODE:
			app.opt_maxNode = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose + 1;
			break;

		case LO_PARANOID:
			app.opt_flagsSet |= ctx.MAGICMASK_PARANOID;
			app.opt_flagsClr &= ~ctx.MAGICMASK_PARANOID;
			break;
		case LO_NOPARANOID:
			app.opt_flagsSet &= ~ctx.MAGICMASK_PARANOID;
			app.opt_flagsClr |= ctx.MAGICMASK_PARANOID;
			break;
		case LO_PURE:
			app.opt_flagsSet |= ctx.MAGICMASK_PURE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_PURE;
			break;
		case LO_NOPURE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_PURE;
			app.opt_flagsClr |= ctx.MAGICMASK_PURE;
			break;
		case LO_REWRITE:
			app.opt_flagsSet |= ctx.MAGICMASK_REWRITE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_REWRITE;
			break;
		case LO_NOREWRITE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_REWRITE;
			app.opt_flagsClr |= ctx.MAGICMASK_REWRITE;
			break;
		case LO_CASCADE:
			app.opt_flagsSet |= ctx.MAGICMASK_CASCADE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_CASCADE;
			break;
		case LO_NOCASCADE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_CASCADE;
			app.opt_flagsClr |= ctx.MAGICMASK_CASCADE;
			break;
//			case LO_SHRINK:
//				app.opt_flags |=  ctx.MAGICMASK_SHRINK;
//				break;
//			case LO_NOSHRINK:
//				app.opt_flags &=  ~ctx.MAGICMASK_SHRINK;
//				break;
//			case LO_PIVOT3:
//				app.opt_flags |=  ctx.MAGICMASK_PIVOT3;
//				break;
//			case LO_NOPIVOT3:
//				app.opt_flags &=  ~ctx.MAGICMASK_PIVOT3;
//				break;

		case '?':
			ctx.fatal("Try `%s --help' for more information.\n", argv[0]);
		default:
			ctx.fatal("getopt returned character code %d\n", c);
		}
	}

	char *outputFilename;
	char *inputFilename;

	if (argc - optind >= 2) {
		outputFilename = argv[optind++];
		inputFilename  = argv[optind++];
	} else {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!app.opt_force) {
		struct stat sbuf;
		if (!stat(outputFilename, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", outputFilename);
	}

	/*
	 * Main
	 */

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	// Open database
	database_t db(ctx);

	db.open(app.opt_databaseName);
	app.pStore = &db;

	// set flags
	ctx.flags = db.creationFlags;
	ctx.flags |= app.opt_flagsSet;
	ctx.flags &= ~app.opt_flagsClr;

	// display system flags when database was created
	if ((ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) || (ctx.flags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY))
		fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(ctx.flags).c_str());

	return app.main(outputFilename, inputFilename);
}
