#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-13 12:56:11
 *
 * `tlookup` queries the database with supplied arguments.
 *
 * If an argument is numeric, either decimal, prefixed hexadecimal or or octal,
 * it will show the database entry indexed by id. Otherwise it will perform a named lookup.
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2017-2020, xyzzy@rockingship.org
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

#include <stdint.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include "database.h"

/**
 * @date 2020-03-13 13:00:48
 *
 * Main program logic as application context
 *
 * @typedef {object}
 */
struct tlookupContext_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {string} name of output database
	const char *arg_database;

	/**
	 * Constructor
	 */
	tlookupContext_t(context_t &ctx) : ctx(ctx) {
		// arguments and options
		arg_database = "untangle.db";
	}

	/**
	 * Main entrypoint
	 *
	 * @param {database_t} pDb - database
	 * @param {string} pArg - Argument to lookup
	 */
	void main(database_t *pDb, const char *pArg) {
		/*
		 * Convert argument into number if possible
		 */
		char *endptr;
		unsigned tid;

		errno = 0; // To distinguish success/failure after call
		tid = strtol(pArg, &endptr, 0);

		if (errno != 0) {
			// in case of error point to start of string
			endptr = (char *) pArg;
		} else {
			// strip trailing spaces
			while (*endptr && isspace(*endptr))
				endptr++;
		}

		if (*endptr == '\0') {

			/*
			 * Argument is a number
			 */

			if (tid >= pDb->numTransform) {
				printf("tid=%u not found\n", tid);
			} else {
				unsigned rid = pDb->revTransformIds[tid]; // get reverse id
				printf("fwd=%u:%s rev=%u:%s\n", tid, pDb->fwdTransformNames[tid], rid, pDb->fwdTransformNames[rid]);
			}

		} else {

			/*
			 * Argument is a string
			 */

			// validate
			for (const char *p=pArg; *p; p++) {
				// check if all lowercase
				if (!islower(*p)) {
					printf("invalid transform: \"%s\"\n", pArg);
					return;
				}
				if (*p < 'a' || *p >= (char)('a' + MAXSLOTS)) {
					printf("transform out-of-bounds: \"%s\"\n", pArg);
					return;
				}
			}

			// lookup
			tid = pDb->lookupFwdTransform(pArg);

			if (tid == IBIT) {
				printf("tid=%u not found\n", tid);
			} else {
				unsigned rid = pDb->revTransformIds[tid]; // get reverse id
				printf("fwd=%u:%s rev=%u:%s\n", tid, pDb->fwdTransformNames[tid], rid, pDb->fwdTransformNames[rid]);
			}
		}

#if 0
                /*
                 * Copy name to working area
                 */
                char trimmedName[MAXSLOTS + 1];

                strncpy(trimmedName, pName, MAXSLOTS);
                trimmedName[MAXSLOTS] = '\0';

                /*
                 * Normalise to maximum length
                 */
                database_t::trimTransform(trimmedName, (unsigned) strlen(trimmedName));

                /*
                 * Find the transform
                 */
                int tread = -1, twrite = -1;
                for (unsigned i = 0; i < db.maxTransform; i++) {
                        if (!strcmp((*db.colTransformString)[i], trimmedName))
                                tread = i;
                        if (!strcmp((*db.rowTransformString)[i], trimmedName))
                                twrite = i;
                }

                printf("%s: read:%x write:%x\n", trimmedName, tread, twrite);
#endif
	}

};

/*
 *
 * I/O context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} I/O context
 */
context_t ctx;

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {tlookupContext_t} Application context
 */
tlookupContext_t app(ctx);

/**
 * @date 2020-03-13 13:38:55
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int sig) {
	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

/**
 * @date 2020-03-13 13:37:44
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s <output.db>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename> [default=%s]\n", app.arg_database);
		fprintf(stderr, "\t-h --help            This list\n");
		fprintf(stderr, "\t-q --quiet           Say more\n");
		fprintf(stderr, "\t-v --verbose         Say less\n");
	}
}

/**
 * @date 2020-03-13 13:30:32
 *
 * Program main entry point
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context for each argument
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 */
int main(int argc, char *const *argv) {
	setlinebuf(stdout);

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_DEBUG = 1,
			// short opts
			LO_DATABASE = 'D',
			LO_HELP = 'h',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database", 1, 0, LO_DATABASE},
			{"help",     0, 0, LO_HELP},
			{"quiet",    2, 0, LO_QUIET},
			{"verbose",  2, 0, LO_VERBOSE},
			//
			{NULL,       0, 0, 0}
		};

		char optstring[128], *cp;
		cp = optstring;

		/* construct optarg */
		for (int i = 0; long_options[i].name; i++) {
			if (isalpha(long_options[i].val)) {
				*cp++ = (char) long_options[i].val;

				if (long_options[i].has_arg != 0)
					*cp++ = ':';
				if (long_options[i].has_arg == 2)
					*cp++ = ':';
			}
		}
		*cp = '\0';

		// parse long options
		int option_index = 0;
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case LO_DATABASE:
				app.arg_database = optarg;
				break;
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_VERBOSE:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
				break;

			case '?':
				fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
				exit(1);
			default:
				fprintf(stderr, "getopt returned character code %d\n", c);
				exit(1);
		}
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Open database
	 */

	database_t db(ctx);

	// open database
	db.open(app.arg_database, true);

        if (db.maxTransform == 0)
	        ctx.fatal("Missing transform section: %s\n", app.arg_database);

        /*
         * Invoke main entrypoint of application context for every argument
         */

        while (argc - optind > 0) {
                const char *pName = argv[optind++];

	        app.main(&db, pName);
        }

	return 0;
}
