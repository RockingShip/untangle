//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-04-07 16:06:28
 *
 * `slookup` queries the database with supplied arguments and displays signature information.
 *
 * If an argument is numeric (decimal, hexadecimal or octal) it will show the database entry indexed by that id.
 * Otherwise it will perform a display-name lookup.
 * If requested it can also perform an associative lookup on its footprint.
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
#include <errno.h>
#include "tinytree.h"
#include "database.h"

/**
 * @date 2020-04-07 16:29:24
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct slookupContext_t : context_t {

	/// @var {string} name of database
	const char *opt_database;
	/// @var {number} search by imprints
	unsigned opt_imprint;

	/// @var {database_t} - Database store to place results
	database_t *pStore;
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;

	slookupContext_t() {
		opt_database = "untangle.db";
		opt_imprint = 0;
		pStore = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;
	}

	/**
	 * @date 2020-04-07 16:53:40
	 *
	 * Main entrypoint
	 *
	 * Lookup signature in database, either by name (fast) or imprint (slow)
	 *
	 * @param {database_t} pStore - memory based database
	 * @param {string} pName - name/notation of signature
	 */
	void main(const char *pName) {
		// Create worker tree

		signature_t *pSignature = NULL;
		uint32_t sid = 0, tid = 0;

		/*
		 * Test to see if numeric id
		 */

		char *endptr;
		errno = 0;
		sid = strtoul(pName, &endptr, 0);
		if (*pName != 0 && *endptr == 0) {
			/*
			 * numeric id
			 */

		} else {

			if (this->opt_imprint) {
				/*
				 * Find signature using imprint index (slow, required evaluator)
				 */
				tinyTree_t tree(*this);

				tree.decodeSafe(pName);

				if (tree.root & IBIT) {
					// inverted root
					tree.root ^= IBIT;
					pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);
					sid ^= IBIT;
				} else {
					// non-inverted root
					pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);
				}

			} else {
				/*
				 * Find the signature index (fast)
				 */

				uint32_t ix = pStore->lookupSignature(pName);
				sid = pStore->signatureIndex[ix];
			}
		}

		pSignature = pStore->signatures + (sid & ~IBIT);

		/*
		 * Display signature
		 */
		if (sid == 0) {
			printf("%s: not found\n", pName);
			return;
		}

		printf("%d%s:%s%s/%d:%s: size=%u numPlaceholder=%u numEndpoint=%u numBackRef=%u flags=[%x%s%s%s] %s\n",
		       sid & ~IBIT, (sid & IBIT) ? "~" : "",
		       pSignature->name, (sid & IBIT) ? "~" : "",
		       tid, pStore->fwdTransformNames[tid],
		       pSignature->size, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef,
		       pSignature->flags,
		       (pSignature->flags & signature_t::SIGMASK_UNSAFE) ? " UNSAFE" : "",
		       (pSignature->flags & signature_t::SIGMASK_PROVIDES) ? " PROVIDES" : "",
		       (pSignature->flags & signature_t::SIGMASK_REQUIRED) ? " REQUIRED" : "",
		       pName);
	}

};

/*
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {slookupContext_t} Application
 */
slookupContext_t app;

/**
 * @date 2020-04-07 16:46:11
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int sig) {
	if (app.opt_timer) {
		app.tick++;
		alarm(app.opt_timer);
	}
}

/**
 * @date  2020-04-07 16:28:23
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {slookupContext_t} args - argument context
 */
void usage(char *const *argv, bool verbose, const slookupContext_t *args) {
	fprintf(stderr, "usage: %s name [...]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", app.opt_timer);
		fprintf(stderr, "\t-D --database=<filename> [default=%s]\n", app.opt_database);
		fprintf(stderr, "\t-e --evaluate\n");
		fprintf(stderr, "\t-i --imprint\n");
	}
}

/**
 * @date 2020-04-07 16:33:12
 *
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context.
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
			LO_NOPARANOID,
			LO_NOQNTF,
			LO_PARANOID,
			LO_QNTF,
			LO_TIMER,
			// short opts
			LO_DATABASE = 'D',
			LO_HELP = 'h',
			LO_IMPRINT = 'i',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"debug",       1, 0, LO_DEBUG},
			{"help",        0, 0, LO_HELP},
			{"imprint",     0, 0, LO_IMPRINT},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"no-qntf",     0, 0, LO_NOQNTF},
			{"paranoid",    0, 0, LO_PARANOID},
			{"qntf",        0, 0, LO_QNTF},
			{"quiet",       2, 0, LO_QUIET},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",     2, 0, LO_VERBOSE},
			//
			{NULL,          0, 0, 0}
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
				app.opt_database = optarg;
				break;
			case LO_DEBUG:
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_HELP:
				usage(argv, true, &app);
				exit(0);
			case LO_IMPRINT:
				app.opt_imprint++;
				break;
			case LO_NOPARANOID:
				app.opt_flags &= ~context_t::MAGICMASK_PARANOID;
				break;
			case LO_NOQNTF:
				app.opt_flags &= ~context_t::MAGICMASK_QNTF;
				break;
			case LO_PARANOID:
				app.opt_flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_QNTF:
				app.opt_flags |= context_t::MAGICMASK_QNTF;
				break;
			case LO_QUIET:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_verbose - 1;
				break;
			case LO_TIMER:
				app.opt_timer = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_VERBOSE:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_verbose + 1;
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
	 * Program arguments
	 */

	if (argc - optind < 0) {
		usage(argv, false, &app);
		exit(1);
	}

	// register timer handler
	if (app.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(app.opt_timer);
	}

	/*
	 * Open input and create output database
	 */

	// Open input
	database_t db(app);

	db.open(app.opt_database, true);

	if (db.flags && app.opt_verbose >= app.VERBOSE_SUMMARY)
		app.logFlags(db.flags);
#if defined(ENABLE_JANSSON)
	if (app.opt_verbose >= app.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", app.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	if (db.numTransform == 0)
		app.fatal("Missing transform section: %s\n", app.opt_database);
	if (db.numSignature == 0)
		app.fatal("Missing signature section: %s\n", app.opt_database);
	if (db.signatureIndexSize == 0)
		app.fatal("Incomplete signature section: %s (try with --evaluate)\n", app.opt_database);
	if (db.imprintIndexSize == 0 && app.opt_imprint)
		app.fatal("Incomplete imprint section: %s\n", app.opt_database);

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) app.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) app.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	/*
	 * Statistics
	 */

#if 0
	if (app.opt_verbose >= app.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", app.timeAsString(), app.totalAllocated);
	if (app.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", app.totalAllocated);
#endif

	if (app.opt_imprint) {
		// initialise evaluators
		tinyTree_t tree(app);
		tree.initialiseVector(app, app.pEvalFwd, MAXTRANSFORM, db.fwdTransformData);
		tree.initialiseVector(app, app.pEvalRev, MAXTRANSFORM, db.revTransformData);
	} else {
		// evaluators not present
		app.pEvalFwd = app.pEvalRev = NULL;
	}

	/*
	 * Call main for every argument
	 */
	app.pStore = &db;

	while (argc - optind > 0) {
		const char *pName = argv[optind++];

		app.main(pName);
	}

	return 0;
}