//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-11 21:53:16
 *
 * `gentransform` creates the initial database containing transforms for forward and reverse skin mappings.
 *
 * Historically, skins were called transforms because they instruct how to connect endpoints
 * to ordered structures, basically transforming them to the structure being requested.
 * In code the variable `tid` represents the enumerated id of skins/transforms.
 *
 * The collection of transforms are all the endpoint permutations a 4-node/9-endpoint structure can have.
 *
 * This version of `untangle` focuses on transforms with 9 endpoints (`MAXSLOTS`==9).
 * There are 9! different transforms (`MAXTRANSFORM`==362880)
 *
 * Each transforms also has a reverse mapping. This is used to "undo" the effect of an
 * applied transforms. For example `"bca?/bca"` would have the effect `"a->b, b->c, c->a"`
 * resulting in "cab?". The reverse transforms would be `"cab?/cab"`. Determining a reverse
 * transforms is not trivial and therefore pre-determined separately.
 *
 * Skins are stored as LSB hexadecimal words where each nibble represents an endpoint
 * and a textual string.
 *
 * Basically, `gentransform` provides answers to 3 types of questions:
 * - Given a structure and skin, how would the result look like?
 * - How would a structure look like before a given skin was applied?
 * - Which skin should be put around a structure so that the structure looks ordered?
 *
 * @date 2020-05-01 19:18:15
 *
 * Text modes:
 *
 * `--test`   Verbose
 *
 *              <tid> <forwardName> <reverseName> <reverseId>
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

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gentransform.h"

/*
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
 * @global {gentransformContext_t} Application context
 */
gentransformContext_t app(ctx);

/**
 * @date 2020-03-11 23:06:35
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int __attribute__ ((unused)) sig) {
	if (app.arg_outputDatabase) {
		remove(app.arg_outputDatabase);
	}
	exit(1);
}

/**
 * @date 2020-03-11 23:06:35
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
 * @date 2020-03-11 22:30:36
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.db>  -- Create initial database containing transforms\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force           Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help            This list\n");
		fprintf(stderr, "\t-q --quiet           Say less\n");
		fprintf(stderr, "\t   --text            Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds> Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose         Say more\n");

	}
}

/**
 * @date 2020-03-06 20:22:23
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

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_DEBUG   = 1,
			LO_FORCE,
			LO_TEXT,
			LO_TIMER,
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",   1, 0, LO_DEBUG},
			{"force",   0, 0, LO_FORCE},
			{"help",    0, 0, LO_HELP},
			{"quiet",   2, 0, LO_QUIET},
			{"text",    0, 0, LO_TEXT},
			{"timer",   1, 0, LO_TIMER},
			{"verbose", 2, 0, LO_VERBOSE},
			//
			{NULL,      0, 0, 0}
		};

		char optstring[64];
		char *cp          = optstring;
		int  option_index = 0;

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
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_DEBUG:
			ctx.opt_debug = ::strtoul(optarg, NULL, 0);
			break;
		case LO_FORCE:
			app.opt_force++;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_QUIET:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
			break;
		case LO_TEXT:
			app.opt_text++;
			break;
		case LO_TIMER:
			ctx.opt_timer = ::strtoul(optarg, NULL, 0);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
			break;

		case '?':
			fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
			exit(1);
		default:
			fprintf(stderr, "getopt_long() returned character code %d\n", c);
			exit(1);
		}
	}

	/*
	 * Program arguments
	 */
	if (argc - optind >= 1) {
		app.arg_outputDatabase = argv[optind++];
	}

	if (!app.arg_outputDatabase) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (app.arg_outputDatabase && !app.opt_force) {
		struct stat sbuf;

		if (!stat(app.arg_outputDatabase, &sbuf)) {
			fprintf(stderr, "%s already exists. Use --force to overwrite\n", app.arg_outputDatabase);
			exit(1);
		}
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Create database
	 */

	database_t store(ctx);

	// set section sizes to be created
	store.maxTransform       = MAXTRANSFORM;
	store.transformIndexSize = MAXTRANSFORMINDEX;
	store.maxEvaluator       = tinyTree_t::TINYTREE_NEND * MAXTRANSFORM;

	/*
	 * Allocate sections
	 */
	store.allocateSections(database_t::ALLOCMASK_TRANSFORM | database_t::ALLOCMASK_EVALUATOR);

	app.connect(store);

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %.3fG memory\n", ctx.timeAsString(), ctx.totalAllocated / 1e9);

	/*
	 * Invoke main entrypoint of application context
	 */
	app.main();

	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		json_t *jResult = json_object();
		json_object_set_new_nocheck(jResult, "done", json_string_nocheck(argv[0]));
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.jsonInfo(jResult);
		fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}

	return 0;
}
