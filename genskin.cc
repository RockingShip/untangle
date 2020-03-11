#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-11 21:53:16
 *
 * `genskin` creates the initial database containing skin forward and reverse mappings.
 *
 * Historically, skins were called transforms because they instruct how to connect endpoints
 * to ordered structures, basically transforming them to the structure being requested.
 * In code the variable `tid` represents the enumerated id of skins/transforms.
 *
 * The collection of skins are all the endpoint permutations a structure can have.
 *
 * This version of `untangle` focuses on skins with 9 endpoints (`SKINSIZE`==9).
 * There are 9! different skins (`MAXSKINS==362880)
 *
 * Each skin also has a reverse mapping. This is used to "undo" the effect of an
 * applied skin. For example `"bca?/bca"` would have the effect `"a->b, b->c, c->a"`
 * resulting in "cab?". The reverse skin would be `"cab?/cab"`. Determining a reverse
 * skin is not trivial and therefore pre-determined and stored in a lookup table.
 *
 * `genskin` also validates the proper functioning of "interleaving". A mechanism
 * used by the associative lookup index where skins are split into a row/column.
 *
 * Skins are stored as LSB hexadecimal words where each nibble represents an endpoint
 * and a textual string.
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
#include <sys/stat.h>

/**
 * User specified program options as argument context
 *
 * The main program is namespaced and scoped in its own `struct`.
 * This `struct` contains all user options
 *
 * @date 2020-03-11 22:36:29
 */
typedef struct genskinArguments_t {
	/// @var {string} name of output database
	const char *arg_outputDatabase;

	/// @var {number} database compatibility and settings
	uint32_t opt_flags;
	/// @var {number} intentionally undocumented
	uint32_t opt_debug;
	/// @var {number} --verbose, level of explanations
	unsigned opt_verbose;
	/// @var {number} --timer, interval timer for verbose updates
	unsigned opt_timer;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} --keep, do not delete output database in case of errors
	unsigned opt_keep;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;

	genskinArguments_t() {
		arg_outputDatabase = NULL;
		opt_flags          = 0;
		opt_debug          = 0;
		opt_verbose        = 0;
		opt_timer          = 0;
		opt_force          = 0;
		opt_keep           = 0;
		opt_text           = 0;
	}

} userArguments_t;


/**
 * Program usage. Keep high in source code for easy reference
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 * @date  2020-03-11 22:30:36
 */
void usage(char *const *argv, bool verbose, const userArguments_t *args) {
	fprintf(stderr, "usage: %s <output.db>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-q --quiet           Say more\n");
		fprintf(stderr, "\t-v --verbose         Say less\n");
		fprintf(stderr, "\t   --timer=<seconds> Interval timer for verbose updates [default=%d]\n", args->opt_timer);
		fprintf(stderr, "\t   --force           Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --keep            Do not delete output database in case of errors\n");
		fprintf(stderr, "\t   --text            Textual output instead of binary database\n");
	}
}

/**
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @date 2020-03-11 22:53:39
 */
struct genskinContext_t {

	genskinArguments_t &args;

	/**
	 * Constructor
	 *
	 * @param {genskinArguments_t} userArguments - Settings to activate/deactivate functionality
	 */
	genskinContext_t(genskinArguments_t &userArguments)
	/*
	 * initialize fields using initializer lists
	 */
		:
		args(userArguments)
	/*
	 * test all allocations succeeded
	 */
	{
	}

	/**
	 * Main entrypoint
	 */
	void main(void) {

	}

};

/**
 * Argument context.
 * Needs to be global to be accessable to signal handlers.
 *
 * @global {genskinArguments_t}
 */
genskinArguments_t userArgs;

/**
 * Delete partially created database unless explicitly requested
 *
 * Called in case of error or by signal handler
 *
 * @param {number} sig - signal (ignored)
 * @date 2020-03-11 23:06:35
 */
void unlinkAndErrorExit(int sig) {
	if (!userArgs.opt_keep) {
		remove(userArgs.arg_outputDatabase);
	}
	exit(1);
}

/**
 * Program main entry point
 * Process all user supplied arguments to construct a argument context.
 * Create application context using argument context.
 * Activate application context.
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 * @date   2020-03-06 20:22:23
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
			LO_TIMER,
			LO_FORCE,
			LO_KEEP,
			LO_TEXT,
			// short opts
			LO_HELP = 'h',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",   1, 0, LO_DEBUG},
			{"force",   0, 0, LO_FORCE},
			{"help",    0, 0, LO_HELP},
			{"keep",    0, 0, LO_KEEP},
			{"quiet",   2, 0, LO_QUIET},
			{"text",    0, 0, LO_TEXT},
			{"timer",   1, 0, LO_TIMER},
			{"verbose", 2, 0, LO_VERBOSE},
			//
			{NULL,      0, 0, 0}
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
			case LO_HELP:
				usage(argv, true, &userArgs);
				exit(0);
			case LO_QUIET:
				userArgs.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : userArgs.opt_verbose - 1;
				break;
			case LO_VERBOSE:
				userArgs.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : userArgs.opt_verbose + 1;
				break;
			case LO_DEBUG:
				userArgs.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
				break;
			case LO_TIMER:
				userArgs.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
				break;
			case LO_FORCE:
				userArgs.opt_force++;
				break;
			case LO_KEEP:
				userArgs.opt_keep++;
				break;
			case LO_TEXT:
				userArgs.opt_text++;
				break;

			case '?':
				fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
				exit(1);
			default:
				fprintf(stderr, "getopt returned character code %d\n", c);
				exit(1);
		}
	}

	/*
	 * Program has one argument, the output database
	 */
	if (argc - optind >= 1) {
		userArgs.arg_outputDatabase = argv[optind++];
	} else {
		usage(argv, false, &userArgs);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!userArgs.opt_force) {
		struct stat sbuf;

		if (!stat(userArgs.arg_outputDatabase, &sbuf)) {
			fprintf(stderr, "%s already exists. Use --force to overwrite\n", userArgs.arg_outputDatabase);
			exit(1);
		}
	}

	// create application context
	genskinContext_t ctx(userArgs);

	/*
	 * Invoke main entrypoint of application context
	 */
	ctx.main();

	return 0;
}

