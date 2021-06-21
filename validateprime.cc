//#pragma GCC optimize ("O0") // optimize on demand

/*
 * validateprime.cc
 *      Validate all prime structures that when broken into smaller components they are all prime.
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
#include "database.h"

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
struct validateprimeContext_t {

	enum {
		/// @constant {number} Size of footprint for `tinyTree_t` in terms of uint64_t
		QUADPERFOOTPRINT = ((1 << MAXSLOTS) / 64)
	};

	/// @var {string} name of database
	const char *arg_databaseName;

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;
	/// @var {number} --normalise, display names as normalised with transforms
	unsigned opt_normalise;

	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for reverse transforms
	footprint_t *pEvalRev;
	/// @var {database_t} - Database store to place results
	database_t *pStore;

	/// @var {uint16_t} - prime structure scores fpr comparison
	uint16_t *pPrimeScores;
	/// @var {tinyTree_t} - prime structure trees for comparison
	tinyTree_t **pPrimeTrees;

	validateprimeContext_t() {
		arg_databaseName = NULL;
		opt_flags = 0;
		opt_force = 0;
		opt_maxNode = DEFAULT_MAXNODE;
		opt_normalise = 0;

		pStore = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;

		pPrimeScores = NULL;
		pPrimeTrees = NULL;
	}

	/**
	 * @date 2021-06-08 23:45:32
	 *
	 * Calculate the hash of a footprint.
	 *
	 * It doesn't really have to be crc,  as long as the result has some linear distribution over index.
	 * crc32 was chosen because it has a single assembler instruction on x86 platforms.
	 *
	 * Inspired by Mark Adler's software implementation of "crc32c.c -- compute CRC-32C using the Intel crc32 instruction"
	 *
	 * @return {number} - calculate crc
	 */
	uint32_t calccrc32(uint64_t *pData, unsigned numData) const {

		static uint32_t crc32c_table[8][256];

		if (crc32c_table[0][0] == 0) {
			/*
			 * Initialize table
			 */
			uint32_t n, crc, k;
			uint32_t poly = 0x82f63b78;

			for (n = 0; n < 256; n++) {
				crc = n;

				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);

				crc32c_table[0][n] = crc;
			}
			for (n = 0; n < 256; n++) {
				crc = crc32c_table[0][n];

				for (k = 1; k < 8; k++) {
					crc = crc32c_table[0][crc & 0xff] ^ (crc >> 8);
					crc32c_table[k][n] = crc;
				}
			}

		}

		/*
		 * Calculate crc
		 */
		uint64_t crc = 0;

		while (numData > 0) {

			crc ^= *pData++;

			crc = crc32c_table[7][crc & 0xff] ^
			      crc32c_table[6][(crc >> 8) & 0xff] ^
			      crc32c_table[5][(crc >> 16) & 0xff] ^
			      crc32c_table[4][(crc >> 24) & 0xff] ^
			      crc32c_table[3][(crc >> 32) & 0xff] ^
			      crc32c_table[2][(crc >> 40) & 0xff] ^
			      crc32c_table[1][(crc >> 48) & 0xff] ^
			      crc32c_table[0][crc >> 56];

			--numData;
		}

		return crc;
	}

	/**
	 * @date 2021-06-08 21:00:46
	 *
	 * Create/load tree based on arguments
	 */
	baseTree_t *loadTree(unsigned numArgs, char *inputArgs[]) {

		/*
		 * Determine number of keys
		 */
		unsigned numKeys = 0;
		for (unsigned iArg = 0; iArg < numArgs; iArg++) {
			unsigned highest = baseTree_t::highestEndpoint(ctx, inputArgs[iArg]);

			if (highest + 1 > numKeys)
				numKeys = highest + 1;
		}

		// number of keys must be at least that of `tinyTree_t` so that CRC's are compatible
		if (numKeys < MAXSLOTS)
			numKeys = MAXSLOTS;

		/*
		 * Create tree
		 */
		uint32_t kstart = 2;
		uint32_t ostart = kstart + numKeys;
		uint32_t estart = ostart + numArgs;
		uint32_t nstart = estart;

		baseTree_t *pTree = new baseTree_t(ctx, kstart, ostart, estart, nstart, nstart/*numRoots*/, opt_maxNode, opt_flags);

		/*
		 * Setup key/root names
		 */
		pTree->keyNames[0] = "ZERO";
		pTree->keyNames[1] = "ERROR";

		/*
		 * keys
		 */
		for (uint32_t iKey = kstart; iKey < ostart; iKey++) {
			// creating is right-to-left. Storage to reverse
			char stack[10], *pStack = stack;
			// value to be encoded
			uint32_t value = iKey - pTree->kstart;

			// push terminator
			*pStack++ = 0;

			*pStack++ = 'a' + (value % 26);
			value /= 26;

			// process the value
			while (value) {
				*pStack++ = 'A' + (value % 26);
				value /= 26;
			}

			// append, including trailing zero
			while (*--pStack) {
				pTree->keyNames[iKey] += *pStack;
			}
		}

		/*
		 * Outputs
		 */
		for (unsigned iKey = ostart; iKey < estart; iKey++) {
			char str[16];

			sprintf(str, "o%d", iKey - ostart);
			pTree->keyNames[iKey] = str;
		}

		pTree->rootNames = pTree->keyNames;

		/*
		 * Load arguments
		 */
		for (unsigned iArg = 0; iArg < numArgs; iArg++) {
			// find transform delimiter
			const char *pTransform = strchr(inputArgs[iArg], '/');

			if (pTransform)
				pTree->roots[ostart + iArg] = pTree->loadNormaliseString(inputArgs[iArg], pTransform + 1);
			else
				pTree->roots[ostart + iArg] = pTree->loadNormaliseString(inputArgs[iArg]);
		}

		return pTree;
	}

	/**
	 * @date 2021-06-18 00:09:37
	 *
	 * Break tree into smaller components and test they are all prime
	 */
	bool testHeadTail(uint32_t iSid, const tinyTree_t &treeR, const char *pNameR) {

		assert(!(treeR.root & IBIT));

		/*
		 * @date 2020-03-29 23:16:43
		 *
		 * Reserved root entries
		 *
		 * `"N[0] = 0?!0:0"` // zero value, zero QnTF operator, zero reference
		 * `"N[a] = 0?!0:a"` // self reference
		 */
		if (treeR.root == 0 || treeR.root == tinyTree_t::TINYTREE_KSTART) {
			return true;
		}

		/*
		 * Single node trees are always prime
		 */
		if (treeR.count - tinyTree_t::TINYTREE_NSTART == 1)
			return true;

		assert(treeR.root >= tinyTree_t::TINYTREE_NSTART);

		tinyTree_t tree(ctx);
		tinyTree_t tree2(ctx);

		/*
		 * @date 2021-06-18 00:11:37
		 * Check all nodes/tails, except root because that is candidate
		 */
		for (unsigned iTail = tinyTree_t::TINYTREE_NSTART; iTail < treeR.root; iTail++) {
			// prepare tree
			tree.N[iTail] = treeR.N[iTail];
			tree.root = iTail;
			tree.count = iTail + 1;

			// lookup head
			unsigned sid = 0;
			unsigned tid = 0;
			pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);

			if (sid == 0) {
				fprintf(stderr, "tail not found. name=%u:%s, iTail=%u tail=%s\n", iSid, pNameR, iTail, tree.encode(tree.root));
				exit(1);
				return false;
			}

			signature_t *pSig = pStore->signatures + sid;
			if (pSig->prime[0] == 0) {
				fprintf(stderr, "tail has missing prime. name=%u:%s, iTail=%u tail=%u:%s\n", iSid, pNameR, iTail, sid, tree.encode(tree.root));
				exit(1);
				return false;
			}

			// remove skin of tree
			char skin[MAXSLOTS + 1];
			char name[tinyTree_t::TINYTREE_NAMELEN + 1];
			tree.encode(tree.root, name, skin);

			/*
			 * @date 2021-06-18 21:29:50
			 *
			 * NOTE/WARNING the extracted component may have non-normalised dyadic ordering
			 * because in the context of the original trees, the endpoints were locked by the now removed node
			 */
			tree2.decodeSafe(name);
			// structure is now okay
			tree2.encode(tree2.root, name, skin);
			// endpoints are now okay

			// fast
			if (strcmp(name, pSig->prime) == 0)
				continue; // match

			fprintf(stderr, "tail not similar to prime. name=%u:%s, iTail=%u tail=%u:%s,%s prime=%u:%s\n", iSid, pNameR, iTail, sid, name, tree.encode(tree2.root), sid, pSig->prime);
			exit(1);
			return false;
		}

		/*
		 * @date 2020-04-01 22:30:09
		 *
		 * check all heads
		 */
		{
			// replace `hot` node with placeholder
			for (unsigned iHead = tinyTree_t::TINYTREE_NSTART; iHead < treeR.root; iHead++) {
				unsigned select = 1 << treeR.root | 1 << 0; // selected nodes to extract nodes
				unsigned nextPlaceholderPlaceholder = tinyTree_t::TINYTREE_KSTART;
				uint32_t what[tinyTree_t::TINYTREE_NEND];
				what[0] = 0; // replacement for zero

				// scan tree for needed nodes, ignoring `hot` node
				for (unsigned k = treeR.root; k >= tinyTree_t::TINYTREE_NSTART; k--) {
					if (k != iHead && (select & (1 << k))) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q = pNode->Q;
						const unsigned To = pNode->T & ~IBIT;
						const unsigned F = pNode->F;

						if (Q >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << Q;
						if (To >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << To;
						if (F >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << F;
					}
				}

				// prepare for extraction
				tree.clearTree();
				// remove `hot` node from selection
				select &= ~(1 << iHead);

				/*
				 * Extract head.
				 * Replacing references by placeholders changes dyadic ordering.
				 * `what[hot]` is not a reference but a placeholder
				 */
				for (unsigned k = tinyTree_t::TINYTREE_NSTART; k <= treeR.root; k++) {
					if (k != iHead && select & (1 << k)) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q = pNode->Q;
						const unsigned To = pNode->T & ~IBIT;
						const unsigned Ti = pNode->T & IBIT;
						const unsigned F = pNode->F;

						// assign placeholder to endpoint or `hot`
						if (~select & (1 << Q)) {
							what[Q] = nextPlaceholderPlaceholder++;
							select |= 1 << Q;
						}
						if (~select & (1 << To)) {
							what[To] = nextPlaceholderPlaceholder++;
							select |= 1 << To;
						}
						if (~select & (1 << F)) {
							what[F] = nextPlaceholderPlaceholder++;
							select |= 1 << F;
						}

						// mark replacement of old node
						what[k] = tree.count;
						select |= 1 << k;

						/*
						 * Reminder:
						 *  [ 2] a ? ~0 : b                  "+" OR
						 *  [ 6] a ? ~b : 0                  ">" GT
						 *  [ 8] a ? ~b : b                  "^" XOR
						 *  [ 9] a ? ~b : c                  "!" QnTF
						 *  [16] a ?  b : 0                  "&" AND
						 *  [19] a ?  b : c                  "?" QTF
						 */

						// perform dyadic ordering
						if (To == 0 && Ti && tree.compare(what[Q], tree, what[F]) > 0) {
							// reorder OR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (To == F && tree.compare(what[Q], tree, what[F]) > 0) {
							// reorder XOR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = what[Q] ^ IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (F == 0 && !Ti && tree.compare(what[Q], tree, what[To]) > 0) {
							// reorder AND
							tree.N[tree.count].Q = what[To];
							tree.N[tree.count].T = what[Q];
							tree.N[tree.count].F = 0;
						} else {
							// default
							tree.N[tree.count].Q = what[Q];
							tree.N[tree.count].T = what[To] ^ Ti;
							tree.N[tree.count].F = what[F];
						}

						tree.count++;
					}
				}

				// set root
				tree.root = tree.count - 1;

				/*
				 * Extrcted tree
				 */

				// lookup head
				unsigned sid = 0;
				unsigned tid = 0;
				pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);

				if (sid == 0) {
					/*
					 * this happens in 6n9 space where the current head is in 5n9 space, and outside the collection of sids
					 */
					fprintf(stderr, "head not found. name=%u:%s, iHead=%u head=%s\n", iSid, pNameR, iHead, tree.encode(tree.root));
					exit(1);
					return false;
				}

				signature_t *pSig = pStore->signatures + sid;
				if (pSig->prime[0] == 0) {
					fprintf(stderr, "head has missing prime. name=%u:%s, iHead=%u head=%u:%s\n", iSid, pNameR, iHead, sid, tree.encode(tree.root));
					exit(1);
					return false;
				}

				// remove skin of tree
				char skin[MAXSLOTS + 1];
				char name[tinyTree_t::TINYTREE_NAMELEN + 1];
				tree.encode(tree.root, name, skin);

				// NOTE/WARNING the extracted component may have non-normalised dyadic ordering, see note above
				tree2.decodeSafe(name);
				// structure is now okay
				tree2.encode(tree2.root, name, skin);
				// endpoints are now okay

				// fast
				if (strcmp(name, pSig->prime) == 0)
					continue; // match

				/*
				 * Even with operands swapped there is not match
				 */
				fprintf(stderr, "head not similar to prime. name=%u:%s, iHead=%u head=%u:%s,%s prime=%u:%s\n", iSid, pNameR, iHead, sid, name, tree2.encode(tree2.root), sid, pSig->prime);
				exit(1);
				return false;
			}
		}

		return true;
	}

	/**
	 * @date 2021-06-18 00:08:18
	 */
	int main(void) {
		unsigned numPrime = 0;

		// allocate a tree
		tinyTree_t tree(ctx);

		// reset ticker
		ctx.setupSpeed(pStore->numSignature);
		ctx.tick = 0;

		/*
		 * Test all primes
		 */
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			const signature_t *pSignature = pStore->signatures + iSid;

			ctx.progress++;
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numPrime=%u %s",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
					numPrime, pSignature->name);

				ctx.tick = 0;
			}

			if (pSignature->prime[0]) {
				// load tree
				tree.decodeFast(pSignature->prime);
				testHeadTail(iSid, tree, pSignature->name);
				numPrime++;
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		fprintf(stderr, "validated %u primes\n", numPrime);
		return 0;
	}

};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {validateprimeContext_t} Application context
 */
validateprimeContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <database.db>\n", argv[0]);
	if (verbose) {
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
			LO_HELP = 1, LO_DEBUG, LO_FORCE, LO_MAXNODE, LO_TIMER,
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
		char *cp = optstring;
		int option_index = 0;

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

	/*
	 * Program arguments
	 */
	if (argc - optind >= 1) {
		app.arg_databaseName = argv[optind++];
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

	// Open database
	app.pStore = new database_t(ctx);

	app.pStore->open(app.arg_databaseName, 0);

	// display system flags when database was created
	if (app.pStore->creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] DB FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(app.pStore->creationFlags));

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	// initialize evaluator early using input database
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, app.pStore->fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, app.pStore->revTransformData);

	// allocate primes
	app.pPrimeScores = (uint16_t *) ctx.myAlloc("genprimeContext_t::pPrimeScores", app.pStore->numSignature, sizeof(*app.pPrimeScores));
	app.pPrimeTrees = (tinyTree_t **) ctx.myAlloc("genprimeContext_t::pPrimeScores", app.pStore->numSignature, sizeof(*app.pPrimeTrees));

	// load primes
	for (unsigned iSid = 1; iSid < app.pStore->numSignature; iSid++) {
		app.pPrimeScores[iSid] = 0;
		app.pPrimeTrees[iSid] = new tinyTree_t(ctx);
		assert(app.pPrimeTrees[iSid]->root == 0);

		// load prime if present
		if (app.pStore->signatures[iSid].prime[0]) {
			// load prime if present
			app.pPrimeTrees[iSid]->decodeFast(app.pStore->signatures[iSid].prime);
			app.pPrimeScores[iSid] = tinyTree_t::calcScoreName(app.pStore->signatures[iSid].prime);
		}
	}

	return app.main();
}
