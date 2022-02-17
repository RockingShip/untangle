//#pragma GCC optimize ("O0") // optimize on demand

/*
 * gsave.cc
 *      Export a groupTree_t` file as textual json file (or C code)
 *      Store the data nodes as 'data' tag.
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
struct gsaveContext_t {

	/// @var {string} name of database
	const char *opt_databaseName;
	/// @var {number} header flags
	uint32_t   opt_flagsSet;
	/// @var {number} header flags
	uint32_t   opt_flagsClr;
	/// @var {number} --code, Output as C code
	unsigned opt_code;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;

	/// @var {database_t} - Database store to place results
	database_t *pStore;

	gsaveContext_t() {
		opt_databaseName = "untangle.db";
		opt_flagsSet     = 0;
		opt_flagsClr     = 0;
		opt_code         = 0;
		opt_force        = 0;
		pStore           = NULL;
	}

	/**
	 * @date 2021-05-20 23:15:36
	 *
	 * Main entrypoint
	 */
	int main(const char *outputFilename, const char *inputFilename) {

		/*
		 * Open input tree
		 */
		groupTree_t *pTree = new groupTree_t(ctx, *pStore);

		if (pTree->loadFile(inputFilename)) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to load"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(inputFilename));
			pTree->headerInfo(jResult);
			pTree->extraInfo(jResult);
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
			json_delete(jResult);
		}

		/*
		 * Save the tree
		 */
		if (!opt_code) {
			json_t *jOutput = json_object();

			// add tree meta
			pTree->headerInfo(jOutput);
			// add names/history
			pTree->extraInfo(jOutput);

			// add data as strings
			json_object_set_new_nocheck(jOutput, "data", json_string_nocheck(pTree->saveString(0, NULL, true).c_str()));

			FILE *f = fopen(outputFilename, "w");
			if (!f)
				ctx.fatal("fopen(%s) returned: %m\n", outputFilename);

			fprintf(f, "%s\n", json_dumps(jOutput, JSON_PRESERVE_ORDER | JSON_COMPACT));

			if (fclose(f))
				ctx.fatal("fclose(%s) returned: %m\n", outputFilename);

			return 0;
		}

		FILE *f = fopen(outputFilename, "w");
		if (!f)
			ctx.fatal("fopen(%s) returned: %m\n", outputFilename);

		/*
		 * Save the tree as C code
		 */
		fprintf(f, "({\n");
		fprintf(f, "unsigned\n");

		fprintf(f, "kstart=%d,\n", pTree->kstart);
		fprintf(f, "ostart=%d,\n", pTree->ostart);
		fprintf(f, "estart=%d,\n", pTree->estart);
		fprintf(f, "nstart=%d,\n", pTree->nstart);
		fprintf(f, "ncount=%d,\n", pTree->ncount);
		fprintf(f, "numRoots=%d,\n", pTree->numRoots);

		/*
		 * Perform a node reference count
		 */
		uint32_t *pRootRef = pTree->allocMap();

		for (uint32_t iNode = 0; iNode < pTree->ncount; iNode++)
			pRootRef[iNode] = 0;

		for (unsigned iRoot = 0; iRoot < pTree->numRoots; iRoot++)
			pRootRef[pTree->roots[iRoot] & ~IBIT]++;

		fprintf(f, "N[]=");
		for (unsigned iEntry = 0; iEntry < pTree->kstart; iEntry++)
			fprintf(f, "%c%d", (iEntry ? ',' : '{'), iEntry);
		fprintf(f, ",\n");

		for (unsigned iEntry = pTree->kstart; iEntry < pTree->nstart; iEntry++) {
			fprintf(f, "%s,", pTree->entryNames[iEntry].c_str());
		}
		fprintf(f, "\n");

		for (uint32_t iNode = pTree->nstart; iNode < pTree->ncount; iNode++) {
			// write labels
			if (pRootRef[iNode]) {
				fprintf(f, "// ");
				// scan roots
				for (unsigned iRoot = 0; iRoot < pTree->numRoots; iRoot++) {
					int32_t R = pTree->roots[iRoot];

					if ((R & ~IBIT) == iNode) {
						fprintf(f, "%s", pTree->rootNames[iRoot].c_str());
						if (R & IBIT)
							fprintf(f, "~");
						fprintf(f, ":");
					}
				}
				fprintf(f, "\n");
			}

#if 0
			const groupNode_t *pNode = pTree->N + iNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			if (Ti) {
				fprintf(f, "/*%d*/N[%d]?!N[%d]:N[%d],\n", iNode, Q, Tu, F);
			} else {
				fprintf(f, "/*%d*/N[%d]?N[%d]:N[%d],\n", iNode, Q, Tu, F);
			}
#endif
		}
		fprintf(f, "}");

		// roots
		for (unsigned iRoot = 0; iRoot < pTree->numRoots; iRoot++) {
			uint32_t R = pTree->roots[iRoot];

			if (R != iRoot) {
				fprintf(f, ",\n");

				if (R & IBIT) {
					fprintf(f, "%s=N[%d]^0x80000000", pTree->rootNames[iRoot].c_str(), R & ~IBIT);
				} else {
					fprintf(f, "%s=N[%d]", pTree->rootNames[iRoot].c_str(), R);
				}
			}
		}

		fprintf(f, "\n})\n");

		pTree->freeMap(pRootRef);
		delete pTree;

		if (fclose(f))
			ctx.fatal("fclose(%s) returned: %m\n", outputFilename);

		return 0;
	}


};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {gsaveContext_t} Application context
 */
gsaveContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json> <input.dat>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename>   Database to query [default=%s]\n", app.opt_databaseName);
		fprintf(stderr, "\t-c --code\n");
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
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
			LO_HELP = 1, LO_DEBUG, LO_TIMER, LO_FORCE,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_DATABASE = 'D', LO_CODE = 'c', LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"code",        0, 0, LO_CODE},
			{"debug",       1, 0, LO_DEBUG},
			{"force",       0, 0, LO_FORCE},
			{"help",        0, 0, LO_HELP},
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
		char *cp                            = optstring;
		int  option_index                   = 0;

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
		case LO_CODE:
			app.opt_code++;
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
