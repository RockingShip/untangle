#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2021-11-26 23:19:57
 *
 * Inspect `pattern` lookup algorithm
 *
 * To isolate self-awareness, load the argument into a `baseTree`
 * Use the top-level Q/T/F to load `groupTree_t` arguments
 * Graph the Cath product. 
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
#include "dbtool.h"

/**
 * @date 2020-04-07 16:29:24
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct glookupContext_t {

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {string} name of database
	const char *opt_database;

	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	glookupContext_t(context_t &ctx) : ctx(ctx) {
		opt_database = "untangle.db";
		pStore       = NULL;
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

		/*
		 * Load tree 
		 */

		printf("%s:", pName);

		/*
		 * Find signature using imprint index (slow, required evaluator)
		 */
		tinyTree_t tree(ctx);

		const char *slash = ::strchr(pName, '/');
		int        ret;
		if (slash)
			ret = tree.loadStringSafe(pName, slash + 1);
		else
			ret = tree.loadStringSafe(pName);
		if (ret) {
			printf(" [Failed: parse error]\n");
			return;
		}
		if (tree.root & IBIT) {
			printf(" [Failed: tree is inverted]\n");
			return;
		}
		if (tree.root < tinyTree_t::TINYTREE_NSTART) {
			printf(" [Failed: not a structure]\n");
			return;
		}

		/*
		 * Extract components
		 */
		uint32_t sidQ      = 0, tidQ = 0;
		uint32_t sidTi     = 0, sidTu = 0, tidT = 0;
		uint32_t sidF      = 0, tidF = 0;
		{
			std::string nameQ = tree.saveString(tree.N[tree.root].Q);
			tinyTree_t  treeQ(ctx);
			treeQ.loadStringFast(nameQ.c_str());
			pStore->lookupImprintAssociative(&treeQ, pStore->fwdEvaluator, pStore->revEvaluator, &sidQ, &tidQ);

			if (sidQ == 0) {
				printf(" [Failed: Q not found:%s]\n", nameQ.c_str());
				return;
			}

		}
		printf(" Q=%u:%s/%u:%.*s",
		       sidQ, pStore->signatures[sidQ].name,
		       tidQ, pStore->signatures[sidQ].numPlaceholder, pStore->fwdTransformNames[tidQ]);

		{
			sidTi = tree.N[tree.root].T & IBIT;
			std::string nameT = tree.saveString(tree.N[tree.root].T & ~IBIT);
			tinyTree_t  treeT(ctx);
			treeT.loadStringFast(nameT.c_str());
			pStore->lookupImprintAssociative(&treeT, pStore->fwdEvaluator, pStore->revEvaluator, &sidTu, &tidT);

			if (sidTu == 0) {
				printf("%s [Failed: T not found:%s]\n", pName, nameT.c_str());
				return;
			}

		}
		printf(" T=%u:%s%s/%u:%.*s",
		       sidTu, pStore->signatures[sidTu].name, sidTi ? "~" : "",
		       tidT, pStore->signatures[sidTu].numPlaceholder, pStore->fwdTransformNames[tidT]);

		{
			std::string nameF = tree.saveString(tree.N[tree.root].F);
			tinyTree_t  treeF(ctx);
			treeF.loadStringFast(nameF.c_str());
			pStore->lookupImprintAssociative(&treeF, pStore->fwdEvaluator, pStore->revEvaluator, &sidF, &tidF);

			if (sidF == 0) {
				printf("%s [Failed: Q not found:%s]\n", pName, nameF.c_str());
				return;
			}

		}
		printf(" F=%u:%s/%u:%.*s",
		       sidF, pStore->signatures[sidF].name,
		       tidF, pStore->signatures[sidF].numPlaceholder, pStore->fwdTransformNames[tidF]);

		/*
		 * end-of-preparation 
		 */
		
		/*
		 * Construct slots.
		 * Code taken from `genpattern`
		 * 
		 * NOTE: Use tidQ/tidT/tidF names as Q/T/F slot contents
		 */

		printf(" |");

		// reassembly transform
		char     slotsQ[MAXSLOTS + 1];
		char     slotsT[MAXSLOTS + 1];
		char     slotsF[MAXSLOTS + 1];
		char     slotsR[MAXSLOTS + 1];
		// reassembly TF transform relative to Q
		uint32_t tidSlotT  = 0;
		uint32_t tidSlotF  = 0;
		uint32_t tidSlotR  = 0;  // transform from `slotsR[]` to resulting `groupNode_t::slots[]`.
		// nodes already processed
		uint32_t beenThere = 0; // bit set means beenWhat[] is valid/defined
		char     beenWhat[tinyTree_t::TINYTREE_NEND]; // endpoint in resulting `slotsR[]`
		uint32_t nextSlot  = 0;

		/*
		 * Construct slots as `groupTree_t` would do
		 */

		signature_t *pSignature = pStore->signatures + sidQ;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			unsigned endpoint = pStore->fwdTransformNames[tidQ][iSlot] - 'a';
			// was it seen before
			if (!(beenThere & (1 << endpoint))) {
				beenWhat[endpoint] = (char) ('a' + nextSlot); // assign new placeholder
				slotsR[nextSlot]   = (char) ('a' + endpoint); // put endpoint in result
				nextSlot++;
				beenThere |= (1 << endpoint);
			}
			slotsQ[iSlot] = beenWhat[endpoint];
		}
		slotsQ[pSignature->numPlaceholder] = 0; // terminator
		(void) slotsQ; // suppress compiler warning "unused variable"

		pSignature = pStore->signatures + sidTu;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			unsigned endpoint = pStore->fwdTransformNames[tidT][iSlot] - 'a';
			// was it seen before
			if (!(beenThere & (1 << endpoint))) {
				beenWhat[endpoint] = (char) ('a' + nextSlot);
				slotsR[nextSlot]   = (char) ('a' + endpoint);
				nextSlot++;
				beenThere |= (1 << endpoint);
			}
			slotsT[iSlot] = beenWhat[endpoint];
		}
		slotsT[pSignature->numPlaceholder] = 0; // terminator

		pSignature = pStore->signatures + sidF;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			unsigned endpoint = pStore->fwdTransformNames[tidF][iSlot] - 'a';
			// was it seen before
			if (!(beenThere & (1 << endpoint))) {
				beenWhat[endpoint] = (char) ('a' + nextSlot);
				slotsR[nextSlot]   = (char) ('a' + endpoint);
				nextSlot++;
				beenThere |= (1 << endpoint);
			}
			slotsF[iSlot] = beenWhat[endpoint];
		}
		slotsF[pSignature->numPlaceholder] = 0; // terminator

		// slots should not overflow
		assert(nextSlot <= MAXSLOTS);

		slotsR[nextSlot] = 0; // terminator

		/*
		 * Determine transforms
		 */

		tidSlotR = pStore->lookupRevTransform(slotsR);
		tidSlotT = pStore->lookupFwdTransform(slotsT);
		tidSlotF = pStore->lookupFwdTransform(slotsF);
		assert(tidSlotR != IBIT);
		assert(tidSlotT != IBIT);
		assert(tidSlotF != IBIT);

		// "signature-swap" slots
		tidSlotT = dbtool_t::sidSwapTid(*pStore, sidTu, tidSlotT, pStore->fwdTransformNames);
		tidSlotF = dbtool_t::sidSwapTid(*pStore, sidF, tidSlotF, pStore->fwdTransformNames);

		printf(" slotT=%u:%.*s", tidSlotT, pStore->signatures[sidTu].numPlaceholder, pStore->fwdTransformNames[tidSlotT]);
		printf(" slotF=%u:%.*s", tidSlotF, pStore->signatures[sidF].numPlaceholder, pStore->fwdTransformNames[tidSlotF]);
		printf(" slotR=%.*s", nextSlot, slotsR);

		/*
		 * Database lookup
		 */

		printf(" |");

		uint32_t ixFirst = pStore->lookupPatternFirst(sidQ, sidTi ^ sidTu, tidSlotT);
		uint32_t idFirst = pStore->patternFirstIndex[ixFirst];

		printf(" ix/idFirst=%08x/%u", ixFirst, idFirst);

		if (idFirst == 0) {
			printf(" [Failed: idFirst not found]\n");
			return;
		}


		uint32_t ixSecond = pStore->lookupPatternSecond(idFirst, sidF, tidSlotF);
		uint32_t idSecond = pStore->patternSecondIndex[ixSecond];

		printf(" ix/idSecond=%08x/%u", ixSecond, idSecond);

		if (idSecond == 0) {
			printf(" [Failed: idSecond not found]\n");
			return;
		}

		/*
		 * end-of-lookup 
		 */

		/*
		 * Extract slots
		 */

		const patternSecond_t *pSecond           = pStore->patternsSecond + idSecond;
		unsigned              numPlaceholder     = pStore->signatures[pSecond->sidR].numPlaceholder;
		const char            *pExtractTransform = pStore->fwdTransformNames[pSecond->tidExtract];

		char extractSlots[MAXSLOTS + 1];
		for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++)
			extractSlots[iSlot] = slotsR[(unsigned) (pExtractTransform[iSlot] - 'a')];
		extractSlots[numPlaceholder] = 0;

		unsigned tidExtract = pStore->lookupFwdTransform(extractSlots);

		printf(" | sidR=%u tidExtract=%u:%.*s --> %s/%u:%.*s",
		       pSecond->sidR,
		       pSecond->tidExtract, numPlaceholder, pExtractTransform,
		       pStore->signatures[pSecond->sidR].name,
		       tidExtract, pStore->signatures[pSecond->sidR].numPlaceholder, pStore->fwdTransformNames[tidExtract]);

		printf("\n");

		unsigned cnt = 0;
		for (unsigned i = pStore->IDFIRST; i < pStore->numPatternSecond; i++) {
			const patternSecond_t *pSecond = pStore->patternsSecond + i;
			if (pSecond->sidR >= 1 && pSecond->power == 1)
				cnt++;
		}
		printf("%u %u\n", cnt, pStore->numPatternSecond - cnt);

#if 0
		/*
		 * Scratch area
		 */
		
		uint32_t theSid = pSecond->sidR;
		for (unsigned i = pStore->IDFIRST; i < pStore->numPatternSecond; i++) {
			const patternSecond_t *pSecond = pStore->patternsSecond + i;
			const patternFirst_t  *pFirst  = pStore->patternsFirst + pSecond->idFirst;

			if (pSecond->sidR == theSid) {
				// NOTE: invalid as it misses Q/T/F transforms
				printf("%u: %s %s %s %s\n",
				       i,
				       pStore->signatures[pFirst->sidQ].name,
				       pStore->signatures[pFirst->sidTu].name,
				       pStore->signatures[pSecond->sidF].name,
				       pFirst->sidTj ? "!" : "?");
			}
		}

		unsigned cnt[900000];
		for (unsigned i = pStore->IDFIRST; i < pStore->numPatternSecond; i++) {
			const patternSecond_t *pSecond = pStore->patternsSecond + i;
			cnt[pSecond->sidR]++;
		}
		for (unsigned i = pStore->IDFIRST; i < pStore->numSignature; i++)
			printf("%u:%s %u\n", i, pStore->signatures[i].name, cnt[i]);
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
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {glookupContext_t} Application
 */
glookupContext_t app(ctx);

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
 * @param {glookupContext_t} args - argument context
 */
void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s name [...]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename>   Database to query [default=%s]\n", app.opt_database);
		fprintf(stderr, "\t-q --quiet                 Say less\n");
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
int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_DEBUG    = 1,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_PARANOID,
			LO_PURE,
			LO_TIMER,
			// short opts
			LO_DATABASE = 'D',
			LO_HELP     = 'h',
			LO_QUIET    = 'q',
			LO_VERBOSE  = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"debug",       1, 0, LO_DEBUG},
			{"help",        0, 0, LO_HELP},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"no-pure",     0, 0, LO_NOPURE},
			{"paranoid",    0, 0, LO_PARANOID},
			{"pure",        0, 0, LO_PURE},
			{"quiet",       2, 0, LO_QUIET},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",     2, 0, LO_VERBOSE},
			//
			{NULL,          0, 0, 0}
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
		case LO_DATABASE:
			app.opt_database = optarg;
			break;
		case LO_DEBUG:
			ctx.opt_debug = ::strtoul(optarg, NULL, 0);
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
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

	db.open(app.opt_database);

	// display system flags when database was created
	if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(db.creationFlags).c_str());

	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));

	if (db.numSignature == 0 || db.signatureIndexSize == 0)
		ctx.fatal("Missing/incomplete signature section: %s\n", app.opt_database);
	if (db.numImprint == 0 || db.imprintIndexSize == 0)
		ctx.fatal("Missing/ncomplete imprint section: %s\n", app.opt_database);
	if (db.numPatternFirst == 0 || db.numPatternSecond == 0 || db.patternFirstIndexSize == 0 || db.patternSecondIndexSize == 0)
		ctx.fatal("Missing/incomplete pattern section: %s\n", app.opt_database);

	/*
	 * Statistics
	 */

#if 0
	if (ctx.opt_verbose >= app.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %.3fG memory\n", app.timeAsString(), app.totalAllocated / 1e9);
#endif

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
