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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
struct slookupContext_t {

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {string} name of database
	const char *opt_database;
	/// @var {number} search by imprints
	unsigned opt_imprint;
	/// @var {number} show signature members
	unsigned opt_member;
	/// @var {number} show signature swaps
	unsigned opt_swap;

	/// @var {database_t} - Database store to place results
	database_t *pStore;
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;

	slookupContext_t(context_t &ctx) : ctx(ctx) {
		opt_database = "untangle.db";
		opt_imprint = 0;
		opt_member = 0;
		opt_swap = 0;
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
	void lookup(const char *pName) {
		// Create worker tree

		signature_t *pSignature = NULL;
		unsigned sid = 0, tid = 0;

		/*
		 * Test to see if numeric id
		 */

		char *endptr;
		errno = 0;
		sid = ::strtoul(pName, &endptr, 0);
		if (*pName != 0 && *endptr == 0) {
			/*
			 * numeric id
			 */

		} else {

			if (this->opt_imprint) {
				/*
				 * Find signature using imprint index (slow, required evaluator)
				 */
				tinyTree_t tree(ctx);

				const char *slash = ::strchr(pName, '/');
				if (slash)
					tree.decodeSafe(pName, slash + 1);
				else
					tree.decodeSafe(pName);

				if (tree.root & IBIT) {
					// inverted root
					tree.root ^= IBIT;
					pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &sid, &tid);
					sid ^= IBIT;
				} else {
					// non-inverted root
					pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &sid, &tid);
				}

			} else {
				/*
				 * Find the signature index (fast)
				 */

				unsigned ix = pStore->lookupSignature(pName);
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

		printf("%u%s:%s%s/%u:%.*s: size=%u numPlaceholder=%u numEndpoint=%u numBackRef=%u",
		       sid & ~IBIT, (sid & IBIT) ? "~" : "",
		       pSignature->name, (sid & IBIT) ? "~" : "",
		       tid, pSignature->numPlaceholder, pStore->fwdTransformNames[tid],
		       pSignature->size, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef);

		printf(" flags=[%x%s%s%s]",
		       pSignature->flags,
		       (pSignature->flags & signature_t::SIGMASK_SAFE) ? " SAFE" : "",
		       (pSignature->flags & signature_t::SIGMASK_PROVIDES) ? " PROVIDES" : "",
		       (pSignature->flags & signature_t::SIGMASK_REQUIRED) ? " REQUIRED" : "");

		if (opt_swap) {
			printf(" swaps=[");
			const swap_t *pSwap = pStore->swaps + pSignature->swapId;
			for (unsigned j = 0; j < swap_t::MAXENTRY && pSwap->tids[j]; j++) {
				if (j)
					putchar(',');
				printf("%u:%.*s", pSwap->tids[j], pSignature->numPlaceholder, pStore->fwdTransformNames[pSwap->tids[j]]);
			}
			putchar(']');
		}

		printf(" %s\n", pName);

		if (opt_member) {
			unsigned lenName = 0, lenQ = 0, lenT = 0, lenF = 0, lenHead = 0, len;
			static char txt[256];
			tinyTree_t tree(ctx);

			/*
			 * Determine column widths
			 */
			for (unsigned iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
				member_t *pMember = pStore->members + iMid;

				// find tid
				tree.decodeSafe(pMember->name);
				pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &sid, &tid);

				len = sprintf(txt, "%u:%s/%u:%.*s", iMid, pMember->name, tid, pMember->numPlaceholder, pStore->revTransformNames[tid]);
				if (lenName < len)
					lenName = len;

				if (this->opt_member > 1) {
					len = sprintf(txt, "%u:%u:%s", pMember->Qsid, pMember->Qmid, pStore->members[pMember->Qmid].name);
					if (lenQ < len)
						lenQ = len;

					if (pMember->Tsid & IBIT)
						len = sprintf(txt, "-%u:%u:%s", pMember->Tsid & ~IBIT, pMember->Tmid, pStore->members[pMember->Tmid].name);
					else
						len = sprintf(txt, "%u:%u:%s", pMember->Tsid, pMember->Tmid, pStore->members[pMember->Tmid].name);
					if (lenT < len)
						lenT = len;

					len = sprintf(txt, "%u:%u:%s", pMember->Fsid, pMember->Fmid, pStore->members[pMember->Fmid].name);
					if (lenF < len)
						lenF = len;

					len = 0;
					for (unsigned i = 0; i < member_t::MAXHEAD; i++) {
						if (pMember->heads[i]) {
							if (i)
								txt[len++] = ',';
							len += sprintf(txt + len, "%u:%u:%s", pMember->heads[i], pStore->members[pMember->heads[i]].sid, pStore->members[pMember->heads[i]].name);
						}
					}
					if (lenHead < len)
						lenHead = len;
				}
			}

			/*
			 * Show columns
			 */
			for (unsigned iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
				member_t *pMember = pStore->members + iMid;

				// find tid
				tree.decodeSafe(pMember->name);
				pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &sid, &tid);

				sprintf(txt, "%u:%s/%u:%.*s", iMid, pMember->name, tid, pMember->numPlaceholder, pStore->revTransformNames[tid]);
				printf("\t%-*s", lenName, txt);

				printf(" size=%u numPlaceholder=%u numEndpoint=%-2u numBackRef=%u", tree.count - tinyTree_t::TINYTREE_NSTART, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);

				if (this->opt_member > 1) {
					sprintf(txt, "%u:%u:%s", pMember->Qsid, pMember->Qmid, pStore->members[pMember->Qmid].name);
					printf(" Q=%-*s", lenQ, txt);

					if (pMember->Tsid & IBIT)
						sprintf(txt, "-%u:%u:%s", pMember->Tsid & ~IBIT, pMember->Tmid, pStore->members[pMember->Tmid].name);
					else
						sprintf(txt, "%u:%u:%s", pMember->Tsid, pMember->Tmid, pStore->members[pMember->Tmid].name);
					printf(" T=%-*s", lenT, txt);

					sprintf(txt, "%u:%u:%s", pMember->Fsid, pMember->Fmid, pStore->members[pMember->Fmid].name);
					printf(" F=%-*s", lenF, txt);

					len = 0;
					for (unsigned i = 0; i < member_t::MAXHEAD; i++) {
						if (pMember->heads[i]) {
							if (i)
								txt[len++] = ',';
							len += sprintf(txt + len, "%u:%u:%s", pMember->heads[i], pStore->members[pMember->heads[i]].sid, pStore->members[pMember->heads[i]].name);
						}
					}
					printf(" heads=%-*s", lenHead, txt);
				}

				printf(" flags=[%x%s%s%s]",
				       pMember->flags,
				       (pMember->flags & signature_t::SIGMASK_SAFE) ? " SAFE" : "",
				       (pMember->flags & signature_t::SIGMASK_PROVIDES) ? " PROVIDES" : "",
				       (pMember->flags & signature_t::SIGMASK_REQUIRED) ? " REQUIRED" : "");

				printf("\n");
			}

		}
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
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {slookupContext_t} Application
 */
slookupContext_t app(ctx);

/**
 * @date 2020-04-07 16:46:11
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
 * @date 2020-04-07 16:28:23
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {slookupContext_t} args - argument context
 */
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s name [...]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename>   Database to query [default=%s]\n", app.opt_database);
		fprintf(stderr, "\t-i --imprint               Use imprint index\n");
		fprintf(stderr, "\t-m --members[=1]           Show members brief\n");
		fprintf(stderr, "\t-m --members=2             Show members verbose\n");
		fprintf(stderr, "\t-q --quiet                 Say less\n");
		fprintf(stderr, "\t-s --swap                  Show swaps\n");
		fprintf(stderr, "\t-v --verbose               Say more\n");
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
			LO_NOPURE,
			LO_PARANOID,
			LO_PURE,
			LO_TIMER,
			// short opts
			LO_DATABASE = 'D',
			LO_HELP = 'h',
			LO_IMPRINT = 'i',
			LO_MEMBER = 'm',
			LO_QUIET = 'q',
			LO_SWAP = 's',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"debug",       1, 0, LO_DEBUG},
			{"help",        0, 0, LO_HELP},
			{"imprint",     0, 0, LO_IMPRINT},
			{"member",      2, 0, LO_MEMBER},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"no-pure",     0, 0, LO_NOPURE},
			{"paranoid",    0, 0, LO_PARANOID},
			{"pure",        0, 0, LO_PURE},
			{"quiet",       2, 0, LO_QUIET},
			{"swap",        2, 0, LO_SWAP},
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
				ctx.opt_debug = ::strtoul(optarg, NULL, 0);
				break;
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_IMPRINT:
				app.opt_imprint++;
				break;
			case LO_MEMBER:
				app.opt_member = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_member + 1;
				break;
			case LO_NOPARANOID:
				ctx.flags &= ~context_t::MAGICMASK_PARANOID;
				break;
			case LO_NOPURE:
				ctx.flags &= ~context_t::MAGICMASK_PURE;
				break;
			case LO_PARANOID:
				ctx.flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_PURE:
				ctx.flags |= context_t::MAGICMASK_PURE;
				break;
			case LO_QUIET:
				ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_SWAP:
				app.opt_swap = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_swap + 1;
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

	if (argc - optind < 0) {
		usage(argv, false);
		exit(1);
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Open input and create output database
	 */

	// Open database
	database_t db(ctx);

	db.open(app.opt_database, 0);

	// display system flags when database was created
	if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(db.creationFlags));

#if defined(ENABLE_JANSSON)
	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	if (db.numTransform == 0)
		ctx.fatal("Missing transform section: %s\n", app.opt_database);
	if (db.numSignature == 0)
		ctx.fatal("Missing signature section: %s\n", app.opt_database);
	if (db.signatureIndexSize == 0)
		ctx.fatal("Incomplete signature section: %s (try with --evaluate)\n", app.opt_database);
	if (db.imprintIndexSize == 0 && app.opt_imprint)
		ctx.fatal("Incomplete imprint section: %s\n", app.opt_database);

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	/*
	 * Statistics
	 */

#if 0
	if (app.opt_verbose >= app.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %.3fG memory\n", app.timeAsString(), app.totalAllocated / 1e9);
#endif

	if (app.opt_imprint || app.opt_member) {
		// initialise evaluators
		tinyTree_t tree(ctx);
		tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, db.fwdTransformData);
		tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, db.revTransformData);
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

		app.lookup(pName);
	}

	return 0;
}