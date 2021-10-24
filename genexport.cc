//#pragma GCC optimize ("O0") // optimize on demand

/*
 * genexport.cc
 *      Export the main sections of the database to text files that can be imported by `genimport`.
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2021, xyzzy@rockingship.org
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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "genport.h"
#include "metrics.h"

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

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {genexportContext_t} Application context
 */
genportContext_t app(ctx);

/**
 * @date 2021-07-18 12:18:00
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int __attribute__ ((unused)) sig) {
	if (app.arg_jsonName) {
		remove(app.arg_jsonName);
	}
	exit(1);
}

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <export.json> <database.db>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --[no-]depr       Export depreciated members [default=%s]\n", app.opt_depr ? "enabled" : "disabled");
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
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
			LO_HELP     = 1, LO_DEBUG, LO_DEPR, LO_FORCE, LO_NODEPR, LO_TIMER, LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",   1, 0, LO_DEBUG},
			{"depr",    0, 0, LO_DEPR},
			{"force",   0, 0, LO_FORCE},
			{"help",    0, 0, LO_HELP},
			{"no-depr", 0, 0, LO_NODEPR},
			{"quiet",   2, 0, LO_QUIET},
			{"timer",   1, 0, LO_TIMER},
			{"verbose", 2, 0, LO_VERBOSE},
			{NULL,      0, 0, 0}
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
		case LO_DEBUG:
			ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
			break;
		case LO_DEPR:
			app.opt_depr = 1;
			break;
		case LO_FORCE:
			app.opt_force++;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_NODEPR:
			app.opt_depr = 0;
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

		case '?':
			ctx.fatal("Try `%s --help' for more information.\n", argv[0]);
		default:
			ctx.fatal("getopt returned character code %d\n", c);
		}
	}

	if (argc - optind >= 1)
		app.arg_jsonName = argv[optind++];
	if (argc - optind >= 1)
		app.arg_databaseName = argv[optind++];

	if (!app.arg_jsonName || !*app.arg_jsonName || !app.arg_databaseName || !*app.arg_databaseName) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!app.opt_force) {
		struct stat sbuf;
		if (!stat(app.arg_jsonName, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", app.arg_jsonName);
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

	db.open(app.arg_databaseName);
	app.pStore   = &db;

	// display system flags when database was created
	if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] DB FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(db.creationFlags).c_str());

	/*
	 * Main code
	 */

	FILE *f = fopen(app.arg_jsonName, "w");
	if (!f)
		ctx.fatal("fopen(%s) returned: %m\n", app.arg_jsonName);

	// unexpected termination should unlink the outputs
	signal(SIGINT, sigintHandler);
	signal(SIGHUP, sigintHandler);

	app.headersAsJson(f);
	app.signaturesAsJson(f);
	app.swapsAsJson(f);
	app.membersAsJson(f);
	fprintf(f, "}\n");

	// close
	if (fclose(f)) {
		unlink(app.arg_jsonName);
		ctx.fatal("[fclose(%s,\"w\") returned: %m]\n", app.arg_jsonName);
	}

	return 0;
}
