//#pragma GCC optimize ("O0") // optimize on demand

/*
 * kjoin.cc
 *      Join a collection of smaller trees into a larger
 *      All trees should have identical key/root allocations
 *      Intermediate extended keys are substituted
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
struct kjoinContext_t {

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;

	kjoinContext_t() {
		opt_flags   = 0;
		opt_force   = 0;
		opt_maxNode = DEFAULT_MAXNODE;
	}

	/**
	 * @date 2021-05-20 23:15:36
	 *
	 * Main entrypoint
	 */
	int main(const char *outputFilename, unsigned numInputs, char **inputFilenames) {

		const char *inputFilename = inputFilenames[0];

		/*
		 * output file may not exist
		 */
		if (!opt_force) {
			struct stat sbuf;
			if (!stat(outputFilename, &sbuf)) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("file already exists. Use --force to overwrite"));
				json_object_set_new_nocheck(jError, "filename", json_string(outputFilename));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}
		}

		/*
		 * Open the first file to extract template data
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

		if (pOldTree->kstart == 1 /*KERROR*/ ) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("kstart should be at least 2"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		/*
		 * Create newTree
		 */

		baseTree_t *pNewTree = new baseTree_t(ctx, pOldTree->kstart, pOldTree->ostart, pOldTree->estart, pOldTree->nstart, pOldTree->numRoots, opt_maxNode, opt_flags);

		// Setup key/root names
		for (unsigned iKey = 0; iKey < pNewTree->nstart; iKey++)
			pNewTree->keyNames[iKey] = pOldTree->keyNames[iKey];

		for (unsigned iRoot = 0; iRoot < pNewTree->numRoots; iRoot++)
			pNewTree->rootNames[iRoot] = pOldTree->rootNames[iRoot];

		// allocate counter map to detect 'write-after-read'
		uint32_t *pKeyRefCount = pNewTree->allocMap();

		for (uint32_t iKey = 0; iKey < pNewTree->nstart; iKey++) {
			pKeyRefCount[iKey]    = 0; // init refcount
			pNewTree->roots[iKey] = iKey; // init roots/eKey substitution
		}

		// reset ticker
		ctx.setupSpeed(numInputs);
		ctx.tick = 0;

		/*
		 * Include input trees
		 */
		for (unsigned iFile = 0; iFile < numInputs; iFile++) {

			/*
			 * Load input (except first)
			 */
			if (iFile > 0) {
				inputFilename = inputFilenames[iFile];
				pOldTree      = new baseTree_t(ctx);

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

				if (pOldTree->kstart == 1 /*KERROR*/ ) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("kstart should be at least 2"));
					json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				// check dimensions
				if (pOldTree->kstart != pNewTree->kstart ||
				    pOldTree->ostart != pNewTree->ostart ||
				    pOldTree->estart != pNewTree->estart ||
				    pOldTree->nstart != pNewTree->nstart ||
				    pOldTree->numRoots != pNewTree->numRoots) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("meta mismatch"));
					json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
					json_t *jMeta = json_object();
					json_object_set_new_nocheck(jMeta, "kstart", json_integer(pOldTree->kstart));
					json_object_set_new_nocheck(jMeta, "ostart", json_integer(pOldTree->ostart));
					json_object_set_new_nocheck(jMeta, "estart", json_integer(pOldTree->estart));
					json_object_set_new_nocheck(jMeta, "nstart", json_integer(pOldTree->nstart));
					json_object_set_new_nocheck(jMeta, "numroots", json_integer(pOldTree->numRoots));
					json_object_set_new_nocheck(jError, "input", jMeta);
					json_t *jData = json_object();
					json_object_set_new_nocheck(jData, "kstart", json_integer(pNewTree->kstart));
					json_object_set_new_nocheck(jData, "ostart", json_integer(pNewTree->ostart));
					json_object_set_new_nocheck(jData, "estart", json_integer(pNewTree->estart));
					json_object_set_new_nocheck(jData, "nstart", json_integer(pNewTree->nstart));
					json_object_set_new_nocheck(jData, "numroots", json_integer(pNewTree->numRoots));
					json_object_set_new_nocheck(jError, "output", jData);
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				// check names
				for (uint32_t iName = 0; iName < pOldTree->nstart; iName++) {
					if (pOldTree->keyNames[iName].compare(pNewTree->keyNames[iName]) != 0) {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("key name mismatch"));
						json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
						json_object_set_new_nocheck(jError, "key", json_integer(iName));
						json_object_set_new_nocheck(jError, "input", json_string_nocheck(pOldTree->keyNames[iName].c_str()));
						json_object_set_new_nocheck(jError, "output", json_string_nocheck(pNewTree->keyNames[iName].c_str()));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
				}
				for (unsigned iName = 0; iName < pOldTree->numRoots; iName++) {
					if (pOldTree->rootNames[iName].compare(pNewTree->rootNames[iName]) != 0) {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("root name mismatch"));
						json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
						json_object_set_new_nocheck(jError, "key", json_integer(iName));
						json_object_set_new_nocheck(jError, "input", json_string_nocheck(pOldTree->rootNames[iName].c_str()));
						json_object_set_new_nocheck(jError, "output", json_string_nocheck(pNewTree->rootNames[iName].c_str()));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
				}
			}

			ctx.progress++;
			if (ctx.tick && ctx.opt_verbose >= ctx.VERBOSE_TICK) {
				int perSecond = ctx.updateSpeed();

				int eta  = (int) ((ctx.progressHi - ctx.progress) / perSecond);
				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d %s ncount=%d",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, inputFilename, pNewTree->ncount);

				ctx.tick = 0;
			}

			// allocate a node-id remapper
			uint32_t *pMap = pOldTree->allocMap();

			//  setup initial substitutions
			for (uint32_t iKey = 0; iKey < pNewTree->nstart; iKey++)
				pMap[iKey] = pNewTree->roots[iKey];

			uint32_t kError = 1; // error marker

			/*
			 * Walk tree
			 */
			for (uint32_t iNode = pOldTree->nstart; iNode < pOldTree->ncount; iNode++) {
				const baseNode_t *pNode = pOldTree->N + iNode;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				assert(pMap[Q] != kError && pMap[Tu] != kError && pMap[F] != kError);

				// update refcounters
				if (Q < pNewTree->nstart)
					pKeyRefCount[Q]++;
				if (Tu < pNewTree->nstart)
					pKeyRefCount[Tu]++;
				if (F < pNewTree->nstart)
					pKeyRefCount[F]++;

				// create new node
				pMap[iNode] = pNewTree->normaliseNode(pMap[Q], pMap[Tu] ^ Ti, pMap[F]);
			}

			/*
			 * Process roots
			 */
			for (unsigned iRoot = 0; iRoot < pOldTree->numRoots; iRoot++) {
				uint32_t r = pOldTree->roots[iRoot];

				if (r != iRoot) {
					/*
					 * Root declared
					 */

					if (pKeyRefCount[iRoot] > 0) {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("key defined after being used"));
						json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
						json_object_set_new_nocheck(jError, "key", json_string(pOldTree->rootNames[iRoot].c_str()));
						json_object_set_new_nocheck(jError, "refcount", json_integer(pKeyRefCount[iRoot]));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
					if (pNewTree->roots[iRoot] != iRoot) {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("key multiply defined"));
						json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
						json_object_set_new_nocheck(jError, "key", json_string(pOldTree->rootNames[iRoot].c_str()));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}

					/*
					 * Update master root with location of extended key
					 */
					pNewTree->roots[iRoot] = pMap[r & ~IBIT] ^ (r & IBIT);
				}
			}

			/*
			 * Release input
			 */
			pOldTree->freeMap(pMap);
			delete pOldTree;
		}
		// remove ticker
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * Save tree
		 */
		pNewTree->saveFile(outputFilename);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			json_t *jResult = json_object();
			pNewTree->headerInfo(jResult);
			pNewTree->extraInfo(jResult);
			printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		/*
		 * release output
		 */
		pNewTree->freeMap(pKeyRefCount);
		delete pNewTree;

		return 0;
	}


};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {kjoinContext_t} Application context
 */
kjoinContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.dat> <input.dat> ...\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
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
	char **inputFilenames;

	if (argc - optind >= 2) {
		outputFilename = argv[optind++];
		inputFilenames = &argv[optind++];
	} else {
		usage(argv, false);
		exit(1);
	}

	/*
	 * Main
	 */

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	return app.main(outputFilename, argc - optind + 1, inputFilenames);
}
