//#pragma GCC optimize ("O0") // optimize on demand

/*
 * validaterewrite.cc
 * 	Brute force test that `baseTree_t::normaliseNode()` performs proper rewriting.
 * 	Specifically, focus on rewriting rules and display related statistics
 *
 * 	Testing is done by manually constructing trees and comparing them with a saved/loaded version that rewrites.
 *
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
#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rewritedata.h" // include before basetree.h
#include "basetree.h"

#define KSTART 1
#define NSTART (KSTART+MAXSLOTS)
#define NEND (NSTART+15)

struct validaterewriteContext_t {

	enum {
		QUADPERFOOTPRINT = ((1 << MAXSLOTS) / 64),
	};

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;
	/// @global {number} --seed=n, Random seed to generate evaluator test pattern
	uint32_t opt_seed;
	/// @var {number} extra verbose
	unsigned opt_text;

	validaterewriteContext_t(context_t &ctx) : ctx(ctx) {
		opt_flags   = 0;
		opt_text    = 0;
		opt_maxNode = DEFAULT_MAXNODE;
		opt_seed    = 0x20190303;
	}

	void treeEval(baseTree_t *pTree, uint64_t *pEval) {
		// use constants as -funroll-loops optimisation
		for (unsigned i = NSTART; i < NEND && i < pTree->ncount; i++) {
			const baseNode_t *pNode = pTree->N + i;
			const uint64_t   *Q     = &pEval[pNode->Q * QUADPERFOOTPRINT];
			const uint64_t   *T     = &pEval[(pNode->T & ~IBIT) * QUADPERFOOTPRINT];
			const uint64_t   *F     = &pEval[pNode->F * QUADPERFOOTPRINT];
			uint64_t         *R     = &pEval[i * QUADPERFOOTPRINT];

			if (pNode->T & IBIT) {
				R[0] = (~Q[0] & F[0]) ^ (Q[0] & ~T[0]);
				R[1] = (~Q[1] & F[1]) ^ (Q[1] & ~T[1]);
				R[2] = (~Q[2] & F[2]) ^ (Q[2] & ~T[2]);
				R[3] = (~Q[3] & F[3]) ^ (Q[3] & ~T[3]);
				R[4] = (~Q[4] & F[4]) ^ (Q[4] & ~T[4]);
				R[5] = (~Q[5] & F[5]) ^ (Q[5] & ~T[5]);
				R[6] = (~Q[6] & F[6]) ^ (Q[6] & ~T[6]);
				R[7] = (~Q[7] & F[7]) ^ (Q[7] & ~T[7]);
			} else {
				R[0] = (~Q[0] & F[0]) ^ (Q[0] & T[0]);
				R[1] = (~Q[1] & F[1]) ^ (Q[1] & T[1]);
				R[2] = (~Q[2] & F[2]) ^ (Q[2] & T[2]);
				R[3] = (~Q[3] & F[3]) ^ (Q[3] & T[3]);
				R[4] = (~Q[4] & F[4]) ^ (Q[4] & T[4]);
				R[5] = (~Q[5] & F[5]) ^ (Q[5] & T[5]);
				R[6] = (~Q[6] & F[6]) ^ (Q[6] & T[6]);
				R[7] = (~Q[7] & F[7]) ^ (Q[7] & T[7]);
			}
		}
	}

	bool isNormalised(baseNode_t *pNode) {

		const uint32_t Q  = pNode->Q;
		const uint32_t Tu = pNode->T & ~IBIT;
		const uint32_t Ti = pNode->T & IBIT;
		const uint32_t F  = pNode->F;

		if (Q & IBIT) {
			// "!Q?T:F" -> "Q?F:T"
			return false;
		}
		if (Q == 0) {
			// "0?T:F" -> "F"
			return false;
		}

		if (F & IBIT) {
			// "Q?T:!F" -> "!(Q?!T:F)"
			return false;
		}

		if (Ti) {

			if (Tu == 0) {
				if (F == Q || F == 0) {
					// SELF
					return false;
				} else {
					// OR
					// "Q?!0:F" [2]
					return true;
				}
			} else if (Tu == Q) {
				if (F == Q || F == 0) {
					// ZERO
					// "Q?!Q:Q" [4] -> "Q?!Q:0" [3] -> "0"
					return false;
				} else {
					// LESS-THAN
					// "Q?!Q:F" [5] -> "F?!Q:F" -> "F?!Q:0"
					return false;
				}
			} else {
				if (F == Q) {
					// GREATER-THAN
					// "Q?!T:Q" [7] -> "Q?!T:0" [6]
					return false;
				} else if (F == 0) {
					// GREATER-THAN
					// "Q?!T:0" [6]
					return true;
				} else if (Tu == F) {
					// NOT-EQUAL
					// "Q?!F:F" [8]
					return true;
				} else {
					// QnTF (new unified operator)
					// "Q?!T:F" [9]
					return true;
				}
			}

		} else {

			if (Tu == 0) {
				if (F == Q || F == 0) {
					// ZERO
					// "Q?0:Q" [11] -> "Q?0:0" [10] -> "0"
					return false;
				} else {
					// LESS-THAN
					// "Q?0:F" [12] -> "F?!Q:0" [6]
					return false;
				}

			} else if (Tu == Q) {
				if (F == Q || F == 0) {
					// SELF
					// "Q?Q:Q" [14] -> Q?Q:0" [13] -> "Q"
					return false;
				} else {
					// OR
					// "Q?Q:F" [15] -> "Q?!0:F" [2]
					return false;
				}
			} else {
				if (F == Q) {
					// AND
					// "Q?T:Q" [17] -> "Q?T:0" [16]
					return false;
				} else if (F == 0) {
					// AND
					// "Q?T:0" [16]
					return true;
				} else if (Tu == F) {
					// SELF
					// "Q?F:F" [18] -> "F"
					return false;
				} else {
					// QTF (old unified operator)
					// "Q?T:F" [19]
					return true;
				}
			}
		}
		assert(0);
	}

	void main(void) {
		/*
		 * Optimisations are default off
		 */
		if (!(opt_flags & ctx.MAGICMASK_REWRITE))
			fprintf(stderr, "WARNING: optimisation `--rewrite` not specified\n");

		baseTree_t origTree(ctx, KSTART, NSTART, NSTART, NSTART, 1/*numRoots*/, opt_maxNode, opt_flags);
		baseTree_t testTree(ctx, KSTART, NSTART, NSTART, NSTART, 1/*numRoots*/, opt_maxNode, opt_flags);
		uint32_t   slots[NEND];

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "\r\e[K[%s] Fixed patterns\n", ctx.timeAsString());

		ctx.setupSpeed(1);
		ctx.tick = 0;

		/*
		 * Quick test with (wide) 4-node dyadic / 5-endpoint trees
		 * Construction direct without use of `normaliseNode()`.
		 * Save/load using `normaliseNode()`
		 * Compare results
		 */

		//@formatter:off
		for (uint32_t Q1 = 0; Q1 < NSTART; Q1++)
		for (uint32_t T1u=0; T1u<NSTART; T1u++)
		for (uint32_t T1i = 0; T1i < 2; T1i++)
		for (uint32_t F1 = 0; F1 < NSTART; F1++) {
		//@formatter:on

			// validate
			if (Q1 == 0) continue;               // Q not zero
			if (Q1 == T1u) continue;             // Q/T fold
			if (Q1 == F1) continue;              // Q/F fold
			if (T1u == F1 && T1i == 0) continue; // T/F fold
			if (T1u == 0 && T1i == 0) continue;  // Q?0:F -> F?!Q:0
			if (T1u == 0 && F1 == 0) continue;   // Q?!0:0 -> Q

			//@formatter:off
			for (uint32_t Q2 = 0; Q2 < NSTART+1; Q2++)
			for (uint32_t T2u=0; T2u<NSTART+1; T2u++)
			for (uint32_t T2i = 0; T2i < 2; T2i++)
			for (uint32_t F2 = 0; F2 < NSTART+1; F2++) {
			//@formatter:on

				++ctx.progress;

				// validate
				if (Q2 == 0) continue;               // Q not zero
				if (Q2 == T2u) continue;             // Q/T fold
				if (Q2 == F2) continue;              // Q/F fold
				if (T2u == F2 && T2i == 0) continue; // T/F fold
				if (T2u == 0 && T2i == 0) continue;  // Q?0:F -> F?!Q:0
				if (T2u == 0 && F2 == 0) continue;   // Q?!0:0 -> Q

				// nodes must be connected
				if (Q2 != NSTART && T2u != NSTART && F2 != NSTART)
					continue;

				// construct
				origTree.N[NSTART + 0].Q = Q1;
				origTree.N[NSTART + 0].T = T1u ^ (T1i ? IBIT : 0);
				origTree.N[NSTART + 0].F = F1;

				origTree.N[NSTART + 1].Q = Q2;
				origTree.N[NSTART + 1].T = T2u ^ (T2i ? IBIT : 0);
				origTree.N[NSTART + 1].F = F2;

				origTree.roots[0] = NSTART + 1;
				origTree.ncount = NSTART + 2;

				/*
				 * Calculate bitmap
				 */
				uint32_t origBitmap = 0; // vector containing results indexed by "abcde"
				uint32_t bix        = 0;

				//@formatter:off
				for (unsigned a=0; a<2; a++)
				for (unsigned b=0; b<2; b++)
				for (unsigned c=0; c<2; c++)
				for (unsigned d=0; d<2; d++)
				for (unsigned e=0; e<2; e++) {
				//@formatter:on

					slots[0] = 0;
					slots[1] = a;
					slots[2] = b;
					slots[3] = c;
					slots[4] = d;
					slots[5] = e;

					for (unsigned i = origTree.nstart; i < origTree.ncount; i++) {
						const baseNode_t *pNode = origTree.N + i;
						const uint32_t   Q      = slots[pNode->Q];
						const uint32_t   T      = slots[pNode->T & ~IBIT];
						const uint32_t   F      = slots[pNode->F];

						if (pNode->T & IBIT) {
							slots[i] = (~Q & F) ^ (Q & ~T);
						} else {
							slots[i] = (~Q & F) ^ (Q & T);
						}
					}

					origBitmap |= slots[origTree.roots[0]] << bix++;
				}
				assert(bix == 32);

				std::string origName = origTree.saveString(origTree.roots[0]);
				if (opt_text)
					printf("%ld: %08x %-8s ", ctx.progress, origBitmap, origName.c_str());

				/*
				 * Reload with rewriting
				 */
				testTree.rewind();
				testTree.roots[0] = testTree.loadStringSafe(origName.c_str());

				/*
				 * Calculate bitmap
				 */
				uint32_t testBitmap = 0;
				bix = 0;

				//@formatter:off
				for (unsigned a=0; a<2; a++)
				for (unsigned b=0; b<2; b++)
				for (unsigned c=0; c<2; c++)
				for (unsigned d=0; d<2; d++)
				for (unsigned e=0; e<2; e++) {
				//@formatter:on

					slots[0] = 0;
					slots[1] = a;
					slots[2] = b;
					slots[3] = c;
					slots[4] = d;
					slots[5] = e;

					for (unsigned i = testTree.nstart; i < testTree.ncount; i++) {
						const baseNode_t *pNode = testTree.N + i;
						const uint32_t   Q      = slots[pNode->Q];
						const uint32_t   T      = slots[pNode->T & ~IBIT];
						const uint32_t   F      = slots[pNode->F];

						if (pNode->T & IBIT) {
							slots[i] = (~Q & F) ^ (Q & ~T);
						} else {
							slots[i] = (~Q & F) ^ (Q & T);
						}
					}

					testBitmap |= slots[testTree.roots[0]] << bix++;
				}
				assert(bix == 32);

				if (opt_text) {
					printf("%08x %-8s ", testBitmap, testTree.saveString(testTree.roots[0]).c_str());

					printf("\n");
				}

				/*
				 * Compare results
				 */
				if (origBitmap != testBitmap) {
					fprintf(stderr, "fail for %ld: {%d %s%d %d}{%d %s%d %d} -> %s -> %s [lastRewriteIndex=%x]\n",
						ctx.progress,
						origTree.N[NSTART + 0].Q,
						origTree.N[NSTART + 0].T & IBIT ? "~" : "",
						origTree.N[NSTART + 0].T & ~IBIT,
						origTree.N[NSTART + 0].F,
						origTree.N[NSTART + 1].Q,
						origTree.N[NSTART + 1].T & IBIT ? "~" : "",
						origTree.N[NSTART + 1].T & ~IBIT,
						origTree.N[NSTART + 1].F,
						origName.c_str(),
						testTree.saveString(testTree.roots[0]).c_str(),
						gLastRewriteIndex);

					assert(0);
				}
			}
		}

		/*
		 * Create evaluator vector for 4n9.
		 */

		uint64_t *pEval = (uint64_t *) ctx.myAlloc("pEval", origTree.maxNodes, sizeof(*pEval) * QUADPERFOOTPRINT);

		// set 64bit slice to zero
		for (unsigned j = 0; j < origTree.maxNodes * QUADPERFOOTPRINT; j++)
			pEval[j] = 0;

		// set footprint for 64bit slice
		assert(MAXSLOTS == 9);
		assert(KSTART == 1);
		for (unsigned i = 0; i < (1 << MAXSLOTS); i++) {
			// v[0+(i/64)] should be 0
			if (i & (1 << 0)) pEval[(KSTART + 0) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 1)) pEval[(KSTART + 1) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 2)) pEval[(KSTART + 2) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 3)) pEval[(KSTART + 3) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 4)) pEval[(KSTART + 4) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 5)) pEval[(KSTART + 5) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 6)) pEval[(KSTART + 6) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 7)) pEval[(KSTART + 7) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
			if (i & (1 << 8)) pEval[(KSTART + 8) * QUADPERFOOTPRINT + (i / 64)] |= 1LL << (i % 64);
		}

		/*
		 * Extended test with random nodes
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "\r\e[K[%s] Random patterns [progress(speed) cntRewriteNo cntRewriteYes cntRewriteCollapse cntRewriteTree [cntRewritePower]]\n", ctx.timeAsString());

		ctx.setupSpeed(1);
		ctx.progress = 0;

		for (;;) {
			++ctx.progress;

			if (ctx.tick && ctx.opt_verbose >= ctx.VERBOSE_TICK) {
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %ld %ld %ld %ld [%ld %ld %ld %ld %ld]", ctx.timeAsString(),
					ctx.progress, perSecond,
					gCountRewriteNo, gCountRewriteYes, gCountRewriteCollapse, gCountRewriteTree,
					gCountRewritePower[0], gCountRewritePower[1], gCountRewritePower[2], gCountRewritePower[3], gCountRewritePower[4]
				);

				ctx.tick = 0;
			}

			/*
			 * Generate random origTree
			 */
			if ((rand() & 7) == 0) {
				origTree.N[NSTART + 0].Q = origTree.N[NSTART + 0].T = origTree.N[NSTART + 0].F = rand() % (NSTART + 0);
			} else {
				origTree.N[NSTART + 0].Q = rand() % (NSTART + 0);
				origTree.N[NSTART + 0].T = rand() % (NSTART + 0);
				origTree.N[NSTART + 0].F = rand() % (NSTART + 0);
				if (rand() & 1)
					origTree.N[NSTART + 0].T ^= IBIT;

				if (!isNormalised(origTree.N + NSTART + 0))
					continue;
			}

			if ((rand() & 7) == 0) {
				origTree.N[NSTART + 1].Q = origTree.N[NSTART + 1].T = origTree.N[NSTART + 1].F = rand() % (NSTART + 1);
			} else {
				origTree.N[NSTART + 1].Q = rand() % (NSTART + 1);
				origTree.N[NSTART + 1].T = rand() % (NSTART + 1);
				origTree.N[NSTART + 1].F = rand() % (NSTART + 1);
				if (rand() & 1)
					origTree.N[NSTART + 1].T ^= IBIT;

				if (!isNormalised(origTree.N + NSTART + 1))
					continue;
			}

			if ((rand() & 7) == 0) {
				origTree.N[NSTART + 2].Q = origTree.N[NSTART + 2].T = origTree.N[NSTART + 2].F = rand() % (NSTART + 2);
			} else {
				origTree.N[NSTART + 2].Q = rand() % (NSTART + 2);
				origTree.N[NSTART + 2].T = rand() % (NSTART + 2);
				origTree.N[NSTART + 2].F = rand() % (NSTART + 2);
				if (rand() & 1)
					origTree.N[NSTART + 2].T ^= IBIT;

				if (!isNormalised(origTree.N + NSTART + 2))
					continue;
			}

			origTree.N[NSTART + 3].Q = NSTART + 0;
			origTree.N[NSTART + 3].T = NSTART + 1;
			origTree.N[NSTART + 3].F = NSTART + 2;
			if (rand() & 1)
				origTree.N[NSTART + 3].T ^= IBIT;

			origTree.roots[0] = NSTART + 3;
			origTree.ncount = NSTART + 4;

			/*
			 * Calculate bitmap
			 */

			// calculate bitmaps
			treeEval(&origTree, pEval);

			uint64_t      origResult[QUADPERFOOTPRINT];
			for (unsigned j = 0; j < QUADPERFOOTPRINT; j++)
				origResult[j] = pEval[origTree.roots[0] * QUADPERFOOTPRINT + j];

			std::string origName = origTree.saveString(origTree.roots[0]);
			if (opt_text)
				printf("%ld: %016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx %-8s ",
				       ctx.progress,
				       origResult[0],
				       origResult[1],
				       origResult[2],
				       origResult[3],
				       origResult[4],
				       origResult[5],
				       origResult[6],
				       origResult[7],
				       origName.c_str());

			/*
			 * Reload with rewriting
			 */
			testTree.rewind();
			testTree.roots[0] = testTree.loadStringSafe(origName.c_str());

			/*
			 * Calculate bitmap
			 */
			treeEval(&testTree, pEval);

			uint64_t testResult[QUADPERFOOTPRINT];

			for (unsigned j = 0; j < QUADPERFOOTPRINT; j++)
				testResult[j] = pEval[testTree.roots[0] * QUADPERFOOTPRINT + j];

			if (opt_text) {
				printf(" %-8s ", testTree.saveString(testTree.roots[0]).c_str());

				printf("\n");
			}

			bool matches = true;

			for (unsigned j = 0; j < QUADPERFOOTPRINT; j++)
				matches &= origResult[j] == testResult[j];

			if (!matches) {
				fprintf(stderr, "fail for %ld: {%d %s%d %d}{%d %s%d %d}{%d %s%d %d}{%d %s%d %d} -> %s -> %s [lastRewriteIndex=%x]\n",
					ctx.progress,
					origTree.N[NSTART + 0].Q,
					origTree.N[NSTART + 0].T & IBIT ? "~" : "",
					origTree.N[NSTART + 0].T & ~IBIT,
					origTree.N[NSTART + 0].F,
					origTree.N[NSTART + 1].Q,
					origTree.N[NSTART + 1].T & IBIT ? "~" : "",
					origTree.N[NSTART + 1].T & ~IBIT,
					origTree.N[NSTART + 1].F,
					origTree.N[NSTART + 2].Q,
					origTree.N[NSTART + 2].T & IBIT ? "~" : "",
					origTree.N[NSTART + 2].T & ~IBIT,
					origTree.N[NSTART + 2].F,
					origTree.N[NSTART + 3].Q,
					origTree.N[NSTART + 3].T & IBIT ? "~" : "",
					origTree.N[NSTART + 3].T & ~IBIT,
					origTree.N[NSTART + 3].F,
					origName.c_str(),
					testTree.saveString(testTree.roots[0]).c_str(),
					gLastRewriteIndex);
				fprintf(stderr, "origResult: %016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx\n",
					origResult[0], origResult[1], origResult[2], origResult[3], origResult[4], origResult[5], origResult[6], origResult[7]);
				fprintf(stderr, "testResult: %016lx %016lx %016lx %016lx %016lx %016lx %016lx %016lx\n",
					testResult[0], testResult[1], testResult[2], testResult[3], testResult[4], testResult[5], testResult[6], testResult[7]);

				assert(0);
			}
		}

	}

};

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {genrestartdataContext_t} Application context
 */
validaterewriteContext_t app(ctx);

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

void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-n --seed=<seed> [default=%d]\n", app.opt_seed);
		fprintf(stderr, "\t   --text\n");
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

int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	/*
	 * scan options
	 */

	for (;;) {
		enum {
			LO_HELP     = 1, LO_DEBUG, LO_MAXNODE, LO_TIMER, LO_TEXT, LO_SEED,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_DATABASE = 'D', LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",       1, 0, LO_DEBUG},
			{"help",        0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"norewrite",   0, 0, LO_NOREWRITE},
			{"quiet",       2, 0, LO_QUIET},
			{"seed",        1, 0, LO_SEED},
			{"text",        0, 0, LO_TEXT},
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
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_MAXNODE:
			app.opt_maxNode = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_SEED:
			app.opt_seed = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_TEXT:
			app.opt_text++;
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
			fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
			exit(1);
		default:
			fprintf(stderr, "getopt returned character code %d\n", c);
			exit(1);
		}
	}

	/*
	 * Setup
	 */

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(1);
	}

	// set random seed
	if (app.opt_seed)
		srand(app.opt_seed);
	else
		srand(clock());

	/*
	 * Run
	 */

	app.main();

	return 0;
}
