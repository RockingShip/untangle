//#pragma GCC optimize ("O0") // optimize on demand

/*
 * kslice.cc
 *      Slice a tree into a collection of smaller trees.
 *      Each node that is referenced multiple times (given by `--thresh`) are considered heads.
 *      Each written tree has extended keys/roots that contain placeholders/references to these head nodes.
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
struct ksliceContext_t {

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxnode;
	/// @var {number} --threshold, Nodes referenced at least this number of times get their own file
	unsigned opt_threshold;

	/// @var {baseTree_t*} input tree
	baseTree_t *pInputTree;

	ksliceContext_t() {
		opt_flags     = 0;
		opt_force     = 0;
		opt_maxnode   = DEFAULT_MAXNODE;
		opt_threshold = 2;
		pInputTree    = NULL;
	}

	/**
	 * @date 2021-05-19 17:00:36
	 *
	 * Main entrypoint
	 */
	int main(const char *outputTemplate, const char *inputFilename) {
		// load oldTree
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

		if (pOldTree->estart != pOldTree->nstart || pOldTree->estart != pOldTree->numRoots) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Tree already has extended keys/roots"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		if (pOldTree->kstart == 1) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("kstart should be at least 2"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		/*
		 * Perform a node reference count
		 */
		uint32_t *pRefCount = pOldTree->allocMap();

		for (uint32_t iNode = 0; iNode < pOldTree->ncount; iNode++)
			pRefCount[iNode] = 0;

		for (uint32_t iNode = pOldTree->nstart; iNode < pOldTree->ncount; iNode++) {
			const baseNode_t *pNode = pOldTree->N + iNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
//			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			pRefCount[Q]++;
			pRefCount[Tu]++;
			if (Tu != F)
				pRefCount[F]++;
		}

		uint32_t numExtended = 0;

		// count the number of nodes that will be saved in a file and need an extended key
		for (uint32_t iNode = pOldTree->nstart; iNode < pOldTree->ncount; iNode++) {
			if (pRefCount[iNode] >= opt_threshold)
				numExtended++;
		}

		// count the number of roots that need to be included
		for (uint32_t iRoot = 0; iRoot < pOldTree->numRoots; iRoot++) {
			uint32_t r = pOldTree->roots[iRoot] & ~IBIT;

			if (r >= pOldTree->nstart && pRefCount[r] < opt_threshold) {
				pRefCount[r] = opt_threshold;
				numExtended++;
			}
		}

		/*
		 * Create newTree
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Splitting into %d parts\n", ctx.timeAsString(), numExtended);

		baseTree_t *pNewTree = new baseTree_t(ctx, pOldTree->kstart, pOldTree->ostart, pOldTree->estart, pOldTree->estart + numExtended/*nstart*/, pOldTree->numRoots + numExtended/*numRoots*/, opt_maxnode, opt_flags);

		/*
		 * Determine keyname length
		 */
		unsigned keyNameLength;
		if (pNewTree->nstart < 10)
			keyNameLength = 1;
		else if (pNewTree->nstart < 100)
			keyNameLength = 2;
		else if (pNewTree->nstart < 1000)
			keyNameLength = 3;
		else if (pNewTree->nstart < 10000)
			keyNameLength = 4;
		else if (pNewTree->nstart < 100000)
			keyNameLength = 5;
		else if (pNewTree->nstart < 1000000)
			keyNameLength = 6;
		else
			keyNameLength = 7;

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] New kstart=%d ostart=%d estart=%d nstart=%d\n", ctx.timeAsString(), pNewTree->kstart, pNewTree->ostart, pNewTree->estart, pNewTree->nstart);

		/*
		 * Setup key/root names
		 */

		for (unsigned iKey = 0; iKey < pNewTree->estart; iKey++)
			pNewTree->keyNames[iKey] = pOldTree->keyNames[iKey];

		// NOTE: memory leak
		for (unsigned iKey = pNewTree->estart; iKey < pNewTree->nstart; iKey++)
			asprintf((char **) &pNewTree->keyNames[iKey], "e%0*d", keyNameLength, iKey);

		// root has same names as keys
		if (pNewTree->rootNames != pNewTree->keyNames)
			ctx.myFree("baseTree_t::rootNames", pNewTree->rootNames);
		pNewTree->rootNames = pNewTree->keyNames;

		// setup keys
		for (unsigned iKey = 0; iKey < pNewTree->nstart; iKey++) {
			pNewTree->N[iKey].Q = 0;
			pNewTree->N[iKey].T = 0;
			pNewTree->N[iKey].F = iKey;
		}

		// setup roots
		for (unsigned iRoot = 0; iRoot < pNewTree->nstart; iRoot++) {
			pNewTree->roots[iRoot] = iRoot;
		}

		/*
		 * All preparations done
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);

		/*
		 * Walk the oldTree and copy nodes to newTree
		 * After encountering a head node:
		 *  - assign it a unique extended key
		 *  - save tree to file
		 *  - empty newTree
		 */

		uint32_t *pMap     = pOldTree->allocMap(); // translation map old->new
		uint32_t *pVersion = pOldTree->allocVersion(); // selector map for sub-trees
		uint32_t thisVersion;

		for (uint32_t iNode = 0; iNode < pOldTree->nstart; iNode++)
			pMap[iNode] = iNode;

		uint32_t nextExtend = pNewTree->estart;
		uint32_t kError     = 1; // error marker 

		// reset ticker
		ctx.setupSpeed(pOldTree->ncount - pOldTree->nstart);
		ctx.tick = 0;
		unsigned numSaves = 0;

		// find node heads
		for (uint32_t iHead = pOldTree->nstart; iHead < pOldTree->ncount; iHead++) {

			ctx.progress++;

			// is node a head
			if (pRefCount[iHead] < opt_threshold)
				continue; // no

			char *filename;
			asprintf(&filename, outputTemplate, nextExtend);

			/*
			 * file may not exist
			 */
			if (!opt_force) {
				struct stat sbuf;
				if (!stat(filename, &sbuf)) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("file already exists. Use --force to overwrite"));
					json_object_set_new_nocheck(jError, "filename", json_string(filename));
					char info[64];
					sprintf(info, "you might need to add '%%0%dd' to the filename", keyNameLength);
					json_object_set_new_nocheck(jError, "info", json_string(info));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}
			}

			if (ctx.tick && ctx.opt_verbose >= ctx.VERBOSE_TICK) {
				int perSecond = ctx.updateSpeed();

				int eta  = (int) ((ctx.progressHi - ctx.progress) / perSecond);
				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d %s ",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, filename);

				ctx.tick = 0;
			}

			/*
			 * select tree
			 */
			thisVersion = ++pOldTree->mapVersionNr;
			assert(thisVersion != 0);

			pVersion[iHead] = thisVersion;

			for (uint32_t iNode = iHead; iNode >= pOldTree->nstart; --iNode) {
				if (pVersion[iNode] == thisVersion) {
					const baseNode_t *pNode = pOldTree->N + iNode;
					const uint32_t   Q      = pNode->Q;
					const uint32_t   Tu     = pNode->T & ~IBIT;
//					const uint32_t   Ti     = pNode->T & IBIT;
					const uint32_t   F      = pNode->F;

					if (Q >= pOldTree->nstart && pRefCount[Q] < opt_threshold)
						pVersion[Q]  = thisVersion;
					if (Tu >= pOldTree->nstart && pRefCount[Tu] < opt_threshold)
						pVersion[Tu] = thisVersion;
					if (F >= pOldTree->nstart && pRefCount[F] < opt_threshold)
						pVersion[F]  = thisVersion;
				}
			}

			/*
			 * Copy nodes to new tree
			 */

			// clear tree
			pNewTree->rootsId = rand(); // this tree roots gets new unique id
			pNewTree->ncount  = pNewTree->nstart;
			// invalidate lookup cache
			++pNewTree->nodeIndexVersionNr;

			// copy nodes
			for (uint32_t iNode = pOldTree->nstart; iNode <= iHead; iNode++) {
				if (pVersion[iNode] == thisVersion) {
					const baseNode_t *pNode = pOldTree->N + iNode;
					const uint32_t   Q      = pNode->Q;
					const uint32_t   Tu     = pNode->T & ~IBIT;
					const uint32_t   Ti     = pNode->T & IBIT;
					const uint32_t   F      = pNode->F;

					assert(pMap[Q] != kError && pMap[Tu] != kError && pMap[F] != kError);

					// create new node
					pMap[iNode] = pNewTree->normaliseNode(pMap[Q], pMap[Tu] ^ Ti, pMap[F]);
				}
			}

			// save head in roots
			pNewTree->roots[nextExtend] = pMap[iHead];

			// export existing roots
			for (uint32_t iRoot = pOldTree->kstart; iRoot < pOldTree->estart; iRoot++) {
				uint32_t r = pOldTree->roots[iRoot];
				if ((r & ~IBIT) == iHead) {
					pNewTree->roots[iRoot] = pMap[r & ~IBIT] ^ (r & IBIT);

					// display in which files the keys are located
					if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
						fprintf(stderr, "\r\e[K%s: %s\n", pNewTree->rootNames[iRoot], filename);
				}
			}

			// next time, node reference will result in extended key
			pMap[iHead] = nextExtend++;

			/*
			 * Save tree
			 */
			pNewTree->saveFile(filename, false);
			numSaves++;

			if (iHead != pOldTree->ncount - 1) {
				// invalidate non-heads of selection
				for (uint32_t iNode = pOldTree->nstart; iNode < iHead; iNode++) {
					if (pVersion[iNode] == thisVersion)
						pMap[iNode] = kError;
				}
				// clear roots
				for (uint32_t iRoot = pNewTree->kstart; iRoot < pNewTree->nstart; iRoot++) {
					pNewTree->roots[iRoot] = iRoot;
				}
			}

			// release filename
			free(filename);
		}
		assert(nextExtend == pNewTree->nstart);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Split into %d files\n", ctx.timeAsString(), numSaves);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			json_t *jResult = json_object();
			pNewTree->headerInfo(jResult);
			pNewTree->extraInfo(jResult);
			printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		pOldTree->freeMap(pMap);
		pOldTree->freeVersion(pVersion);
		delete pNewTree;
		delete pOldTree;

		return 0;
	}


};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {ksliceContext_t} Application context
 */
ksliceContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <outputTemplate.dat> <input.dat> # NOTE: 'outputTemplate' is a sprintf template\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxnode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --threshold=<seconds> [default=%d]\n", app.opt_threshold);
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
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_FORCE, LO_MAXNODE, LO_THRESHOLD,
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
			{"threshold",   1, 0, LO_THRESHOLD},
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

		char optstring[128], *cp;
		cp = optstring;

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

		int option_index = 0;
		int c            = getopt_long(argc, argv, optstring, long_options, &option_index);
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
			app.opt_maxnode = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_THRESHOLD:
			app.opt_threshold = (unsigned) strtoul(optarg, NULL, 10);
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

	char *outputTemplate;
	char *inputFilename;

	if (argc - optind >= 2) {
		outputTemplate = argv[optind++];
		inputFilename  = argv[optind++];
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

	return app.main(outputTemplate, inputFilename);
}
