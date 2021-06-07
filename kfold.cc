//#pragma GCC optimize ("O3") // optimize on demand

/*
 * kfold.cc
 *      Fold trees
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
#include "basetree.h"

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
struct kfoldContext_t {

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;

	/// @var {baseTree_t*} input tree
	baseTree_t *pInputTree;

	kfoldContext_t() {
		opt_flags   = 0;
		opt_force   = 0;
		opt_maxNode = DEFAULT_MAXNODE;
	}

	// metrics for folds
	struct fold_t {
		uint32_t key;     // key to fold
		uint32_t version; // version last computation
		unsigned count;   // nodes in tree after folding
	};

	/**
	 * @date 2021-06-06 23:34:57
	 *
	 * Compare function for `qsort_r`
	 *
	 * @param {fold_t} lhs - left hand side member
	 * @param {fold_t} rhs - right hand side member
	 * @param {context_t} arg - I/O context
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int comparFold(const void *lhs, const void *rhs, void *arg) {
		if (lhs == rhs)
			return 0;

		const fold_t *pFoldL = static_cast<const fold_t *>(lhs);
		const fold_t *pFoldR = static_cast<const fold_t *>(rhs);
//		kfoldContext_t *pApp   = static_cast<kfoldContext_t *>(arg);

		return pFoldR->count - pFoldL->count;
	}

	/**
	 * @date 2021-06-05 21:42:11
	 *
	 * Main entrypoint
	 */
	int main(const char *outputFilename, const char *inputFilename) {

		/*
		 * Open input tree
		 */
		baseTree_t *pOldTree = new baseTree_t(ctx);

		if (pOldTree->loadFile(inputFilename)) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to load"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(inputFilename));
			pOldTree->headerInfo(jResult);
			pOldTree->extraInfo(jResult);
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
			json_delete(jResult);
		}

		/*
		 * Extended roots are used to implement a stack for tree-walking.
		 */
		if (pOldTree->nstart > pOldTree->estart) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("extended keys not supported"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		/*
		 * Create new tree
		 */
		baseTree_t *pNewTree = new baseTree_t(ctx, pOldTree->kstart, pOldTree->ostart, pOldTree->estart, pOldTree->estart/*nstart*/, pOldTree->ncount/*numRoots*/, opt_maxNode, opt_flags);
		baseTree_t *pTemp    = new baseTree_t(ctx, pOldTree->kstart, pOldTree->ostart, pOldTree->estart, pOldTree->estart/*nstart*/, pOldTree->ncount/*numRoots*/, opt_maxNode, opt_flags);

		/*
		 * Setup key/root names
		 */

		for (unsigned iKey = 0; iKey < pNewTree->nstart; iKey++)
			pNewTree->keyNames[iKey] = pOldTree->keyNames[iKey];

		// Determine keyname length
		unsigned keyNameLength;
		if (pNewTree->ncount < 10)
			keyNameLength = 1;
		else if (pNewTree->ncount < 100)
			keyNameLength = 2;
		else if (pNewTree->ncount < 1000)
			keyNameLength = 3;
		else if (pNewTree->ncount < 10000)
			keyNameLength = 4;
		else if (pNewTree->ncount < 100000)
			keyNameLength = 5;
		else if (pNewTree->ncount < 1000000)
			keyNameLength = 6;
		else
			keyNameLength = 7;

		for (unsigned iRoot = 0; iRoot < pNewTree->nstart; iRoot++)
			pNewTree->rootNames[iRoot] = pNewTree->keyNames[iRoot];

		for (unsigned iRoot = pNewTree->estart; iRoot < pNewTree->numRoots; iRoot++) {
			char sbuf[32];
			sprintf(sbuf, "n%0*d", keyNameLength, iRoot);
			pNewTree->rootNames[iRoot] = sbuf;
		}

		// same with tmp
		pTemp->keyNames  = pNewTree->keyNames;
		pTemp->rootNames = pNewTree->rootNames;

		// set roots to self-reference
		for (unsigned iRoot = 0; iRoot < pOldTree->nstart; iRoot++)
			pNewTree->roots[iRoot] = iRoot;

		// set node results to zero
		for (unsigned iRoot = pOldTree->nstart; iRoot < pOldTree->ncount; iRoot++)
			pNewTree->roots[iRoot] = 0;

		/*
		 * Count references
		 */
		uint32_t *pNodeRefCount = pOldTree->allocMap();

		for (uint32_t iKey = 0; iKey < pOldTree->nstart; iKey++)
			pNodeRefCount[iKey] = 0;

		for (uint32_t iNode = pOldTree->nstart; iNode < pOldTree->ncount; iNode++) {

			const baseNode_t *pNode = pOldTree->N + iNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
//			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			pNodeRefCount[Q]++;
			if (Tu != F) pNodeRefCount[Tu]++;
			pNodeRefCount[F]++;
		}

		// extended keys store equivalent of pMap
		// makes it possible to add 1 node at a time
		// and have all nodes referenced

		// reset ticker
		ctx.setupSpeed(pOldTree->ncount - pOldTree->nstart);
		ctx.tick     = 0;
		ctx.progress = 0;

		// nodes already tree-walk ordered
		for (uint32_t iOldNode = pOldTree->nstart; iOldNode < pOldTree->ncount; iOldNode++) {

			ctx.progress++;
			if (ctx.tick && ctx.opt_verbose >= ctx.VERBOSE_TICK) {
				int perSecond = ctx.updateSpeed();

				int eta  = (int) ((ctx.progressHi - ctx.progress) / perSecond);
				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d numNodes=%d",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, pNewTree->ncount - pNewTree->nstart);

				ctx.tick = 0;
			}

			const baseNode_t *pNode = pOldTree->N + iOldNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			/*
			 * Add single node and release unused roots.
			 */
			pNewTree->roots[iOldNode] = pNewTree->normaliseNode(pNewTree->roots[Q], pNewTree->roots[Tu] ^ Ti, pNewTree->roots[F]);

			// release root when no longer used
			--pNodeRefCount[Q];
			if (Tu != F) --pNodeRefCount[Tu];
			--pNodeRefCount[F];

			if (pNodeRefCount[Q] == 0)
				pNewTree->roots[Q]  = Q;
			if (pNodeRefCount[Tu] == 0)
				pNewTree->roots[Tu] = Tu;
			if (pNodeRefCount[F] == 0)
				pNewTree->roots[F]  = F;

//			printf("inject node iNode=%d numNodes=%d\n", iOldNode, pNewTree->ncount - pNewTree->nstart);

			//////////////// below is tree rotation.

			/*
			 * Count/collect fold candidates
			 */
			uint32_t *pNewRefCount = pNewTree->allocMap();
			fold_t   lstFolds[pNewTree->nstart];
			unsigned numFolds;

			for (uint32_t iKey = 0; iKey < pNewTree->nstart; iKey++)
				pNewRefCount[iKey] = 0;

			for (uint32_t k = pNewTree->nstart; k < pNewTree->ncount; k++) {
				const baseNode_t *pNode = pNewTree->N + k;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
//					const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				pNewRefCount[Q]++;
				if (Tu != F)
					pNewRefCount[Tu]++;
				pNewRefCount[F]++;
			}

			// populate folds
			numFolds = 0;
			for (uint32_t iKey = pNewTree->kstart; iKey < pNewTree->nstart; iKey++) {
				if (pNewRefCount[iKey] > 0) {
					lstFolds[numFolds].key     = iKey;
					lstFolds[numFolds].version = 0;
					lstFolds[numFolds].count   = 1;
					numFolds++;
				}
			}

			pNewTree->freeMap(pNewRefCount);

			// sort in order of decreasing counts
			qsort_r(lstFolds, numFolds, sizeof *lstFolds, comparFold, this);

			while (numFolds > 0) {
				// update counts
				while (numFolds > 0 && lstFolds[numFolds - 1].version == 0) {
					fold_t *pFold = &lstFolds[numFolds - 1];

					pTemp->rewind();
					pTemp->importFold(pNewTree, pFold->key);
					pFold->count   = pTemp->countActive();
					pFold->version = 1;

//					printf("prefold %s active=%d numnodes=%d numFolds=%d\n", pNewTree->keyNames[pFold->key].c_str(), pFold->count, pTemp->ncount - pTemp->nstart, numFolds);

					qsort_r(lstFolds, numFolds, sizeof *lstFolds, comparFold, this);
				}

//					uint32_t iFold = lstFolds[numFolds - 1].key;
//					printf("%d fold %s %d\n", numFolds, pNewTree->keyNames[iFold].c_str(), lstFolds[numFolds - 1].count);

				pTemp->rewind();
				pTemp->importFold(pNewTree, lstFolds[numFolds - 1].key);
//					printf("count=%u\n", pTemp->countActive());
				pNewTree->rewind();
				pNewTree->importActive(pTemp);
//					printf("%s count=%u\n", pNewTree->rootNames[iFold].c_str(), pNewTree->countActive());

				--numFolds;
				for (unsigned i = 0; i < numFolds; i++)
					lstFolds[i].version = 0;
			}
		}

		// remove ticker
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		// verify all intermediates released
		for (uint32_t iKey = 0; iKey < pOldTree->ncount; iKey++) {
			assert(pNodeRefCount[iKey] == 0);
		}

		// assign roots
		for (uint32_t iRoot = pOldTree->kstart; iRoot < pOldTree->nstart; iRoot++) {
			uint32_t R = pOldTree->roots[iRoot];

			pNewTree->roots[iRoot] = pNewTree->roots[R & ~IBIT] ^ (R & IBIT);
		}

		// and system
		pNewTree->system = pNewTree->roots[pOldTree->system & ~IBIT] ^ (pOldTree->system & IBIT);

		/*
		 * Copy result to new tree without extended roots
		 */
		delete pTemp;
		pTemp = new baseTree_t(ctx, pOldTree->kstart, pOldTree->ostart, pOldTree->estart, pOldTree->nstart, pOldTree->numRoots, opt_maxNode, opt_flags);
		pTemp->keyNames  = pOldTree->keyNames;
		pTemp->rootNames = pOldTree->rootNames;
		pTemp->importActive(pNewTree);

		delete pNewTree;
		pNewTree = NULL;

		/*
		 * Save data
		 */
		pTemp->saveFile(outputFilename);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			json_t *jResult = json_object();
			pTemp->headerInfo(jResult);
			pTemp->extraInfo(jResult);
			printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		pOldTree->freeMap(pNodeRefCount);
		delete pOldTree;
		delete pTemp;

		return 0;
	}


};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {kfoldContext_t} Application context
 */
kfoldContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json> <input.dat>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --[no-]paranoid [default=%s]\n", app.opt_flags & ctx.MAGICMASK_PARANOID ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure [default=%s]\n", app.opt_flags & ctx.MAGICMASK_PURE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]rewrite [default=%s]\n", app.opt_flags & ctx.MAGICMASK_REWRITE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]cascade [default=%s]\n", app.opt_flags & ctx.MAGICMASK_CASCADE ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]shrink [default=%s]\n", app.opt_flags &  ctx.MAGICMASK_SHRINK ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]pivot3 [default=%s]\n", app.opt_flags &  ctx.MAGICMASK_PIVOT3 ? "enabled" : "disabled");
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
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_FORCE, LO_MAXNODE,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
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
			app.opt_flags |= ctx.MAGICMASK_PARANOID;
			break;
		case LO_NOPARANOID:
			app.opt_flags &= ~ctx.MAGICMASK_PARANOID;
			break;
		case LO_PURE:
			app.opt_flags |= ctx.MAGICMASK_PURE;
			break;
		case LO_NOPURE:
			app.opt_flags &= ~ctx.MAGICMASK_PURE;
			break;
		case LO_REWRITE:
			app.opt_flags |= ctx.MAGICMASK_REWRITE;
			break;
		case LO_NOREWRITE:
			app.opt_flags &= ~ctx.MAGICMASK_REWRITE;
			break;
		case LO_CASCADE:
			app.opt_flags |= ctx.MAGICMASK_CASCADE;
			break;
		case LO_NOCASCADE:
			app.opt_flags &= ~ctx.MAGICMASK_CASCADE;
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

	return app.main(outputFilename, inputFilename);
}
