//#pragma GCC optimize ("O0") // optimize on demand

/*
 * bexplain
 *      Explain (in json) what the effects of normalisation are.
 *      This should be an alternative validating implementation.
 *
 *      Level 1: basic input Q/T/F "constant" handling.
 *      Level 2: function grouping
 *      Level 3: rewriteData for detector "abc!def!ghi!!" and runtime values for endpoints (slots)
 *      Level 4: signature based alternative orderings of variables.
 *      Level 5: dry-run and apply build instructions.
 *
   {"Q":38,"T":19,"F":20,          	# inputs
    "level1":"F",		   	# level-1 constant folding
    "level1":{"Q":2,"T":3,"F":0},  	# level-1 Q/T/F re-order
    "level2":"F",		   	# level-2 constant folding
    "level2":{"Q":3,"T":~2,"F":0},	# level-2 Q/T/F re-order
    "level3":{
    	"rwslots":[37,3,14],	       	# runtime nodeId values (for left-to-right notation)
    	"name":"abc?bdc!efd!?/abcdef",	# level-3 member name for database lookup
    					# skin for transform mapping left-to-right to depth-first notation
    	"MID":"34:abc?bdc!efd!?/0:abcdef", # detector direct hit: memberId:memberName/transform:skin.
    	"sid":"357705:abc?bd&efd!!",	# signature group. Always with transformId=0
    	"sidslots":[37,3,7,2,17,14]	# runtime endpoint values (for depth-first notation)
    	"probe":[			# runtime dry-runs
    	{
    	 "name":"dabc?^/0:abcd",	# pattern/skin added
    	 "miss":0			# number of missing nodes
    	}
    	]
    },
    "level4":["2:acb", "5:cba"],	# signature `slot[]` re-order
    "level5":{
	"member":"3208934:abc?bdc!efd!?/0:abcdef" # member being created using `sidSlots[]`
    },
    "N":39				# return nodeId. Can be existing or new.
   }
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
struct bevalContext_t {

	enum {
		/// @constant {number} Size of footprint for `tinyTree_t` in terms of uint64_t
		QUADPERFOOTPRINT = ((1 << MAXSLOTS) / 64)
	};

	/// @var {string} name of database
	const char *opt_databaseName;
	/// @var {number} --datasize, Data vector size containing test patterns for CRC (units in uint64_t)
	unsigned opt_dataSize;
	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;
	/// @var {number} --normalise, display names as normalised with transforms
	unsigned opt_normalise;
	/// @global {number} --seed=n, Random seed to generate evaluator test pattern
	unsigned opt_seed;

	/// @global {footprint_t[]} Evulation footprint for `explainNode()`
	footprint_t *gExplainEval;
	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	bevalContext_t() {
		opt_databaseName = "untangle.db";
		opt_dataSize     = QUADPERFOOTPRINT; // compatible with `footprint_t::QUADPERFOOTPRINT`
		opt_flags        = 0;
		opt_force        = 0;
		opt_maxNode      = DEFAULT_MAXNODE;
		opt_normalise    = 0;
		opt_seed         = 0x20210609;

		gExplainEval = NULL;
		pStore       = NULL;

		/*
		 * Create evaluator vector for 4n9 specifically for `explainNode()`
		 */

		gExplainEval = (footprint_t *) ctx.myAlloc("pEval", tinyTree_t::TINYTREE_NEND, sizeof(*gExplainEval));

		// set 64bit slice to zero
		for (unsigned i = 0; i < tinyTree_t::TINYTREE_NEND; i++)
			for (unsigned j = 0; j<footprint_t::QUADPERFOOTPRINT; j++)
				gExplainEval[i].bits[j] = 0;

			// set footprint for 64bit slice
			assert(MAXSLOTS == 9);
			assert(tinyTree_t::TINYTREE_KSTART == 1);
			for (unsigned i = 0; i < (1 << MAXSLOTS); i++) {
				// v[0+(i/64)] should be 0
				if (i & (1 << 0)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 0].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 1)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 1].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 2)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 2].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 3)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 3].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 4)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 4].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 5)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 5].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 6)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 6].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 7)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 7].bits[i / 64] |= 1LL << (i % 64);
				if (i & (1 << 8)) gExplainEval[tinyTree_t::TINYTREE_KSTART + 8].bits[i / 64] |= 1LL << (i % 64);
			}

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
	 * @date 2021-08-13 13:33:13
	 *
	 * Add a node to tree
	 *
	 * If the node already exists then use that.
	 * Otherwise, add a node to tree if it has the expected node ID.
	 * Otherwise, Something changed since the recursion was invoked, re-analyse
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainBasicNode(unsigned depth, uint32_t expectId, baseTree_t *pTree, uint32_t Q, uint32_t T, uint32_t F, unsigned *pFailCount) {
		ctx.cntHash++;

		// lookup
		uint32_t ix = pTree->lookupNode(Q, T, F);
		if (pTree->nodeIndex[ix] != 0) {
			// node already exists
			printf(",   \"old\":{\"qtf\":[%u,%s%u,%u],N:%u}", Q, T & IBIT ? "~" : "", T & ~IBIT, F, pTree->nodeIndex[ix]);
			return pTree->nodeIndex[ix];
		} else if (pTree->ncount != expectId) {
			/*
			 * @date 2021-08-11 21:59:48
			 * if the node id is not what is expected, then something changed and needs to be re-evaluated again
			 */
			printf("\n");
			return explainNormaliseNode(depth + 1, pTree->ncount, pTree, Q, T, F, pFailCount);
		} else if (pFailCount != NULL) {
			/*
			 * Simulate the creation of a new node.
			 * The returned node id must be unique and must not be an end condition `ncount`.
			 */
			(*pFailCount)++;
			return pTree->ncount + *pFailCount;
		} else {
			// situation is stable, create node
			uint32_t ret = pTree->basicNode(Q, T, F);
			printf(",   \"new\":{\"qtf\":[%u,%s%u,%u],N:%u}", Q, T & IBIT ? "~" : "", T & ~IBIT, F, ret);
			return ret;
		}
	}

	/**
	 * @date 2021-08-13 13:44:17
	 *
	 * Apply dyadic ordering and add node(s) to tree.
	 *
	 * The added structures are one of: "ab^cd^^", "cab^^", "ab^" or "a".
	 *
	 * Important NOTE:
	 * The structure "dcab^^^" will cause oscillations.
	 * Say that this is the top of a longer cascading chain, then `b` is also a "^".
	 * Within the current detect span ("dcab^^^"), it is likely that `b` and `d` will swap positions.
	 * The expanded resulting structure will look like "xy^cad^^^", who's head is "xy^cz^^". (`b`="xy^",`z`="ad^")
	 * This new head would trigger a rewrite to "zcxy^^^" making the cycle complete.
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainOrderedNode(unsigned depth, uint32_t expectId, baseTree_t *pTree, uint32_t Q, uint32_t T, uint32_t F, unsigned *pFailCount) {

		// OR (L?~0:R)
		if (pTree->isOR(Q, T, F)) {

			if (pTree->isOR(Q) && pTree->isOR(F)) {
				/*
				 * cascade split
				 * already valid: QQ < QF, FQ < FF
				 */
				uint32_t AB = Q;
				uint32_t CD = F;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].F;
				uint32_t C  = pTree->N[CD].Q;
				uint32_t D  = pTree->N[CD].F;
				int      cmp;

				if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
						// A<B<C<D, no change
						Q = AB;
						T = IBIT;
						F = CD;
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"abcd\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<B<D
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"abd\",\"ab+\":\n", A, B, C, D);
						uint32_t AB = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, B, pFailCount);
						Q = AB;
						T = IBIT;
						F = D;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
						// A<C<B<D
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acbd\",\"ac+\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, C, pFailCount);
						printf(",\"bd+\":\n");
						uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, IBIT, D, pFailCount);
						Q = AC;
						T = IBIT;
						F = BD;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<C<B
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acb\",\"ac+\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, C, pFailCount);
						Q = AC;
						T = IBIT;
						F = B;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else {
						// A<C<D<B
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acdb\",\"ac+\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, C, pFailCount);
						printf(",\"bd+\":\n");
						uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, IBIT, D, pFailCount);
						Q = AC;
						T = IBIT;
						F = BD;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					}
				} else if (cmp == 0) {
					// A==C collapse
					if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
						// A<B<D
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"abd\",\"ab+\":\n", A, B, C, D);
						uint32_t AB = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, B, pFailCount);
						Q = AB;
						T = IBIT;
						F = D;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<B
						Q = A;
						T = IBIT;
						F = B;
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"ab\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else {
						// A<D<B
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"adb\",\"ad+\":\n", A, B, C, D);
						uint32_t AD = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, D, pFailCount);
						Q = AD;
						T = IBIT;
						F = B;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					}
				} else if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
					// C<A<B<D
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cabd\",\"ca+\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, IBIT, A, pFailCount);
					printf(",\"bd+\":\n");
					uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, IBIT, D, pFailCount);
					Q = CA;
					T = IBIT;
					F = BD;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cab\",\"ab+\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, IBIT, A, pFailCount);
					Q = CA;
					T = IBIT;
					F = B;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if ((cmp = pTree->compare(pTree, A, pTree, D)) < 0) {
					// C<A<D<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cadb\",\"ca+\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, IBIT, A, pFailCount);
					printf(",\"db+\":\n");
					uint32_t DB = explainNormaliseNode(depth + 1, expectId, pTree, D, IBIT, B, pFailCount);
					Q = CA;
					T = IBIT;
					F = DB;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cab\",\"ab+\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, IBIT, A, pFailCount);
					Q = CA;
					T = IBIT;
					F = B;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else {
					// C<A<D<B
					Q = CD;
					T = IBIT;
					F = AB;
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cdab\",\"qtf\":[%u,%s%u,%u]}", C, D, A, B, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->isOR(Q)) {
				/*
				 * cascade left
				 */
				uint32_t AB = Q;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].F;
				uint32_t C  = F;
				int      cmp;

				/*
				 * where does c fit relative to ab
				 */

				if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
					// A<B<C, no change
				} else if (cmp == 0) {
					// A
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"a\",\"N\":%u}}", A, B, C, A);
					return A;
				} else if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					// A<C<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"acb\",\"ac+\":\n", A, B, C);
					uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, C, pFailCount);
					Q = B;
					T = IBIT;
					F = AC;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"b\",\"N\":%u}}", A, B, C, B);
					return B;
				} else {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"cab\",\"ca+\":\n", A, B, C);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, IBIT, A, pFailCount);
					Q = B;
					T = IBIT;
					F = CA;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->isOR(F)) {
				/*
				 * cascade right
				 */
				uint32_t AB = F;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].F;
				uint32_t C  = Q;
				int      cmp;

				/*
				 * where does c fit relative to ab
				 */

				if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
					// A<B<C, no change
				} else if (cmp == 0) {
					// A
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"a\",\"N\":%u}}", A, B, C, A);
					return A;
				} else if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					// A<C<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"acb\",\"ac+\":\n", A, B, C);
					uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, IBIT, C, pFailCount);
					Q = B;
					T = IBIT;
					F = AC;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"b\",\"N\":%u}}", A, B, C, B);
					return B;
				} else {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"cab\",\"ca+\":\n", A, B, C);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, IBIT, A, pFailCount);
					Q = B;
					T = IBIT;
					F = CA;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->compare(pTree, Q, pTree, F) > 0) {
				/*
				 * Single node
				 */
				uint32_t tmp = Q;
				Q = F;
				T = IBIT;
				F = tmp;
				printf(",   \"or\":{\"slot\":[%u,%u],\"order\":\"ba\",\"qtf\":[%u,%s%u,%u]}", Q, F, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			}
		}

		// XOR/NE (L?~R:R)
		if (pTree->isNE(Q, T, F)) {

			if (pTree->isNE(Q) && pTree->isNE(F)) {
				/*
				 * cascade split
				 * already valid: QQ < QF, FQ < FF
				 */
				uint32_t AB = Q;
				uint32_t CD = F;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].F;
				uint32_t C  = pTree->N[CD].Q;
				uint32_t D  = pTree->N[CD].F;
				int      cmp;

				if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
						// A<B<C<D, no change
						printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"abcd\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<D
						Q = A;
						T = D ^ IBIT;
						F = D;
						printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"ad\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
						// A<C<B<D
						printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acbd\",\"ac^\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C ^ IBIT, C, pFailCount);
						printf(",\"bd^\":\n");
						uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, D ^ IBIT, D, pFailCount);
						Q = AC;
						T = BD ^ IBIT;
						F = BD;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<C
						Q = A;
						T = C ^ IBIT;
						F = C;
						printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"ac\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else {
						// A<C<D<B
						printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acdb\",\"ac^\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C ^ IBIT, C, pFailCount);
						printf(",\"bd^\":\n");
						uint32_t DB = explainNormaliseNode(depth + 1, expectId, pTree, D, B ^ IBIT, B, pFailCount);
						Q = AC;
						T = DB ^ IBIT;
						F = DB;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					}
				} else if (cmp == 0) {
					if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
						// B<D
						Q = B;
						T = D ^ IBIT;
						F = D;
						printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"bd\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// 0
						assert(0); // this would imply that A==C and B==D
					} else {
						Q = D;
						T = B ^ IBIT;
						F = B;
						printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"db\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					}
				} else if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
					// C<A<B<D
					printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cabd\",\"ca^\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A ^ IBIT, A, pFailCount);
					printf(",\"bd^\":\n");
					uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, D ^ IBIT, D, pFailCount);
					Q = CA;
					T = BD ^ IBIT;
					F = BD;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// C<A
					Q = C;
					T = A ^ IBIT;
					F = A;
					printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"ca\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if ((cmp = pTree->compare(pTree, A, pTree, D)) < 0) {
					// C<A<D<B
					printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cadb\",\"ca^\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A ^ IBIT, A, pFailCount);
					printf(",\"db^\":\n");
					uint32_t DB = explainNormaliseNode(depth + 1, expectId, pTree, D, B ^ IBIT, B, pFailCount);
					Q = CA;
					T = DB ^ IBIT;
					F = DB;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// C<B
					Q = C;
					T = B ^ IBIT;
					F = B;
					printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cb\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else {
					// C<D<A<B
					Q = CD;
					T = AB ^ IBIT;
					F = AB;
					printf(",   \"xor\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cdab\",\"qtf\":%u,%s%u,%u]}", C, D, A, B, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->isNE(Q)) {
				/*
				 * cascade left
				 */
				uint32_t AB = Q;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].F;
				uint32_t C  = F;
				int      cmp;

				/*
				 * where does c fit relative to ab
				 */

				if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
					// A<B<C, no change
				} else if (cmp == 0) {
					// A
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"a\",\"N\":%u}}", A, B, C, A);
					return A;
				} else if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					// A<C<B
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"acb\",\"ac\":\n", A, B, C);
					uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C ^ IBIT, C, pFailCount);
					Q = B;
					T = AC ^ IBIT;
					F = AC;
					printf(",\"qtf\":[%u,:%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// B
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"b\",\"N\":%u}}", A, B, C, B);
					return B;
				} else {
					// C<A<B
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"cab\",\"ca\":\n", A, B, C);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A ^ IBIT, A, pFailCount);
					Q = B;
					T = CA ^ IBIT;
					F = CA;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->isNE(F)) {
				/*
				 * cascade right
				 */
				uint32_t AB = F;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].F;
				uint32_t C  = Q;
				int      cmp;

				/*
				 * where does c fit relative to ab
				 */

				if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
					// A<B<C, no change
				} else if (cmp == 0) {
					// A
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"a\",\"N\":%u}}", A, B, C, A);
					return A;
				} else if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					// A<C<B
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"acb\",\"ac\":\n", A, B, C);
					uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C ^ IBIT, C, pFailCount);
					Q = B;
					T = AC ^ IBIT;
					F = AC;
					printf(",\"qtf\":[%u,:%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// B
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"b\",\"N\":%u}}", A, B, C, B);
					return B;
				} else {
					// C<A<B
					printf(",   \"xor\":{\"slot\":[%u,%u,%u],\"order\":\"cab\",\"ca\":\n", A, B, C);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A ^ IBIT, A, pFailCount);
					Q = B;
					T = CA ^ IBIT;
					F = CA;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->compare(pTree, Q, pTree, F) > 0) {
				/*
				 * Single node
				 */
				uint32_t tmp = Q;
				Q = F;
				T = tmp ^ IBIT;
				F = tmp;
				printf(",   \"xor\":{\"slot\":[%u,%u],\"order\":\"ba\",\"qtf\":[%u,%s%u,%u]}", Q, F, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			}
		}

		// AND (L?T:0)
		if (pTree->isAND(Q, T, F)) {

			if (pTree->isAND(Q) && pTree->isAND(T)) {
				/*
				 * cascade split
				 * already valid: QQ < QF, FQ < FF
				 */
				uint32_t AB = Q;
				uint32_t CD = F;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].F;
				uint32_t C  = pTree->N[CD].Q;
				uint32_t D  = pTree->N[CD].F;
				int      cmp;

				if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
						// A<B<C<D, no change
						Q = AB;
						T = CD;
						F = 0;
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"abcd\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<B<D
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"abd\",\"ab&\":\n", A, B, C, D);
						uint32_t AB = explainNormaliseNode(depth + 1, expectId, pTree, A, B, 0, pFailCount);
						Q = AB;
						T = D;
						F = 0;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
						// A<C<B<D
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acbd\",\"ac&\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C, 0, pFailCount);
						printf(",\"bd&\":\n");
						uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, D, 0, pFailCount);
						Q = AC;
						T = BD;
						F = 0;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<C<B
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acb\",\"ac&\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C, 0, pFailCount);
						Q = AC;
						T = B;
						F = 0;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else {
						// A<C<D<B
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"acdb\",\"ac&\":\n", A, B, C, D);
						uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C, 0, pFailCount);
						printf(",\"bd&\":\n");
						uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, D, 0, pFailCount);
						Q = AC;
						T = BD;
						F = 0;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					}
				} else if (cmp == 0) {
					// A==C collapse
					if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
						// A<B<D
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"abd\",\"ab&\":\n", A, B, C, D);
						uint32_t AB = explainNormaliseNode(depth + 1, expectId, pTree, A, B, 0, pFailCount);
						Q = AB;
						T = D;
						F = 0;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else if (cmp == 0) {
						// A<B
						Q = A;
						T = B;
						F = 0;
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"ab\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					} else {
						// A<D<B
						printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"adb\",\"ad&\":\n", A, B, C, D);
						uint32_t AD = explainNormaliseNode(depth + 1, expectId, pTree, A, D, 0, pFailCount);
						Q = AD;
						T = B;
						F = 0;
						printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
					}
				} else if ((cmp = pTree->compare(pTree, B, pTree, D)) < 0) {
					// C<A<B<D
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cabd\",\"ca&\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A, 0, pFailCount);
					printf(",\"bd&\":\n");
					uint32_t BD = explainNormaliseNode(depth + 1, expectId, pTree, B, D, 0, pFailCount);
					Q = CA;
					T = BD;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cab\",\"ab&\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A, 0, pFailCount);
					Q = CA;
					T = B;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if ((cmp = pTree->compare(pTree, A, pTree, D)) < 0) {
					// C<A<D<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cadb\",\"ca&\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A, 0, pFailCount);
					printf(",\"db&\":\n");
					uint32_t DB = explainNormaliseNode(depth + 1, expectId, pTree, D, B, 0, pFailCount);
					Q = CA;
					T = DB;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cab\",\"ab&\":\n", A, B, C, D);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A, 0, pFailCount);
					Q = CA;
					T = B;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else {
					// C<A<D<B
					Q = CD;
					T = AB;
					F = 0;
					printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"cdab\",\"qtf\":[%u,%s%u,%u]}", C, D, A, B, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->isAND(Q)) {
				/*
				 * cascade left
				 */
				uint32_t AB = Q;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].T;
				uint32_t C  = T;
				int      cmp;

				/*
				 * where does c fit relative to ab
				 */

				if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
					// A<B<C, no change
				} else if (cmp == 0) {
					// A
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"a\",\"N\":%u}}", A, B, C, A);
					return A;
				} else if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					// A<C<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"acb\",\"ac&\":\n", A, B, C);
					uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C, 0, pFailCount);
					Q = B;
					T = AC;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"b\",\"N\":%u}}", A, B, C, B);
					return B;
				} else {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"cab\",\"ca&\":\n", A, B, C);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A, 0, pFailCount);
					Q = B;
					T = CA;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->isAND(T)) {
				/*
				 * cascade right
				 */
				uint32_t AB = T;
				uint32_t A  = pTree->N[AB].Q;
				uint32_t B  = pTree->N[AB].T;
				uint32_t C  = Q;
				int      cmp;

				/*
				 * where does c fit relative to ab
				 */

				if ((cmp = pTree->compare(pTree, B, pTree, C)) < 0) {
					// A<B<C, no change
				} else if (cmp == 0) {
					// A
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"a\",\"N\":%u}}", A, B, C, A);
					return A;
				} else if ((cmp = pTree->compare(pTree, A, pTree, C)) < 0) {
					// A<C<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"acb\",\"ac&\":\n", A, B, C);
					uint32_t AC = explainNormaliseNode(depth + 1, expectId, pTree, A, C, 0, pFailCount);
					Q = B;
					T = AC;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else if (cmp == 0) {
					// B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"b\",\"N\":%u}}", A, B, C, B);
					return B;
				} else {
					// C<A<B
					printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"cab\",\"ca&\":\n", A, B, C);
					uint32_t CA = explainNormaliseNode(depth + 1, expectId, pTree, C, A, 0, pFailCount);
					Q = B;
					T = CA;
					F = 0;
					printf(",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}

			} else if (pTree->compare(pTree, Q, pTree, T) > 0) {
				/*
				 * Single node
				 */
				uint32_t tmp = Q;
				Q = T;
				T = tmp;
				F = 0;
				printf(",   \"or\":{\"slot\":[%u,%u],\"order\":\"ba\",\"qtf\":[%u,%s%u,%u]}", Q, T, Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			}
		}

		return explainBasicNode(depth, expectId, pTree, Q, T, F, pFailCount);
	}

	/*
	 * @date 2021-07-08 04:49:14
	 *
	 * Expand and create a structure name with transform.
	 * Fast version specifically for `tinyTree_t` structures.
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {string} name - structure name
	 * @param {string} skin - structure skin
	 * @param {number[]} slots - structure run-time slots
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainStringFast(unsigned depth, uint32_t expectId, baseTree_t *pTree, const char name[], const char skin[], const uint32_t slot[], unsigned *pFailCount) {

		// state storage for postfix notation
		uint32_t stack[tinyTree_t::TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      stackPos = 0;
		uint32_t beenThere[tinyTree_t::TINYTREE_NEND]; // track id's of display operators.
		unsigned nextNode = tinyTree_t::TINYTREE_NSTART; // next visual node

		// walk through the notation until end or until placeholder/skin separator
		for (const char *pCh = name; *pCh; pCh++) {

			if (isalnum(*pCh) && stackPos >= tinyTree_t::TINYTREE_MAXSTACK)
				assert(!"DERR_OVERFLOW");
			if (islower(*pCh) && !islower(skin[*pCh - 'a']))
				assert(!"DERR_PLACEHOLDER");

			switch (*pCh) {
			case '0':
				stack[stackPos++] = 0;
				break;
			case 'a':
				stack[stackPos++] = slot[skin[0] - 'a'];
				break;
			case 'b':
				stack[stackPos++] = slot[skin[1] - 'a'];
				break;
			case 'c':
				stack[stackPos++] = slot[skin[2] - 'a'];
				break;
			case 'd':
				stack[stackPos++] = slot[skin[3] - 'a'];
				break;
			case 'e':
				stack[stackPos++] = slot[skin[4] - 'a'];
				break;
			case 'f':
				stack[stackPos++] = slot[skin[5] - 'a'];
				break;
			case 'g':
				stack[stackPos++] = slot[skin[6] - 'a'];
				break;
			case 'h':
				stack[stackPos++] = slot[skin[7] - 'a'];
				break;
			case 'i':
				stack[stackPos++] = slot[skin[8] - 'a'];
				break;
			case '1':
				stack[stackPos++] = beenThere[nextNode - ('1' - '0')];
				break;
			case '2':
				stack[stackPos++] = beenThere[nextNode - ('2' - '0')];
				break;
			case '3':
				stack[stackPos++] = beenThere[nextNode - ('3' - '0')];
				break;
			case '4':
				stack[stackPos++] = beenThere[nextNode - ('4' - '0')];
				break;
			case '5':
				stack[stackPos++] = beenThere[nextNode - ('5' - '0')];
				break;
			case '6':
				stack[stackPos++] = beenThere[nextNode - ('6' - '0')];
				break;
			case '7':
				stack[stackPos++] = beenThere[nextNode - ('7' - '0')];
				break;
			case '8':
				stack[stackPos++] = beenThere[nextNode - ('8' - '0')];
				break;
			case '9':
				stack[stackPos++] = beenThere[nextNode - ('9' - '0')];
				break;

			case '>': {
				// GT
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = explainBasicNode(depth, expectId, pTree, L, R ^ IBIT, 0, pFailCount);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '+': {
				// OR
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid;
				if (pFailCount != NULL && (L >= pTree->ncount || R >= pTree->ncount))
					nid = L; // L/R no longer valid
				else
					nid = explainOrderedNode(depth, expectId, pTree, L, 0 ^ IBIT, R, pFailCount);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '^': {
				// XOR/NE
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid;
				if (pFailCount != NULL && (L >= pTree->ncount || R >= pTree->ncount))
					nid = L; // L/R no longer valid
				else
					nid = explainOrderedNode(depth, expectId, pTree, L, R ^ IBIT, R, pFailCount);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '!': {
				// QnTF
				if (stackPos < 3)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid = explainBasicNode(depth, expectId, pTree, Q, T ^ IBIT, F, pFailCount);

				// push
				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '&': {
				// AND
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid;
				if (pFailCount != NULL && (L >= pTree->ncount || R >= pTree->ncount))
					nid = L; // L/R no longer valid
				else
					nid = explainOrderedNode(depth, expectId, pTree, L, R, 0, pFailCount);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '?': {
				// QTF
				if (stackPos < 3)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid = explainBasicNode(depth, expectId, pTree, Q, T, F, pFailCount);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '~': {
				// NOT
				if (stackPos < 1)
					assert(!"DERR_UNDERFLOW");

				// invert top-of-stack
				stack[stackPos - 1] ^= IBIT;
				break;
			}

			case '/':
				// separator between placeholder/skin
				while (pCh[1])
					pCh++;
				break;
			case ' ':
				// skip spaces
				break;
			default:
				assert(!"DERR_UNDERFLOW");
			}
		}

		if (stackPos != 1)
			assert(!"DERR_UNDERFLOW");

		// store result into root
		return stack[stackPos - 1];
	}

	/*
	 * @date 2021-07-05 19:23:46
	 *
	 * Normalise Q/T/F and add to tree
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainNormaliseNode(unsigned depth, uint32_t expectId, baseTree_t *pTree, uint32_t Q, uint32_t T, uint32_t F, unsigned *pFailCount) {

		printf("%*s{\"Q\":%s%u,\"T\":%s%u,\"F\":%s%u",
		       depth, "",
		       (Q & IBIT) ? "~" : "", (Q & ~IBIT),
		       (T & IBIT) ? "~" : "", (T & ~IBIT),
		       (F & IBIT) ? "~" : "", (F & ~IBIT));

		assert(++depth < 40);

		assert ((Q & ~IBIT) < pTree->ncount);
		assert ((T & ~IBIT) < pTree->ncount);
		assert ((F & ~IBIT) < pTree->ncount);

		/*
		 * Level-1 normalisation: invert propagation
		 *
		 * !a ?  b :  c  ->  a ? c : b
		 *  0 ?  b :  c  ->  c
		 *  a ?  b : !c  ->  !(a ? !b : c)
		 */
		uint32_t ibit = 0; // needed for return values
		{
			bool changed = false;

			if (Q & IBIT) {
				// "!Q?T:F" -> "Q?F:T"
				uint32_t savT = T;
				T = F;
				F = savT;
				Q ^= IBIT;
				changed = true;
			}
			if (Q == 0) {
				// "0?T:F" -> "F"
				printf(",   \"level1\":\"F\",\"N\":%s%u}", (F & IBIT) ? "~" : "", (F & ~IBIT));
				return F;
			}

			if (F & IBIT) {
				// "Q?T:!F" -> "!(Q?!T:F)"
				F ^= IBIT;
				T ^= IBIT;
				ibit ^= IBIT;
				changed = true;
			}

			if (changed) {
				printf(",   \"level1\":{\"Q\":%u,\"T\":%s%u,\"F\":%u}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			}
		}

		/*
		 * Level-2 normalisation: function grouping
		 * NOTE: this is also embedded in level-3, it is included for visual completeness
		 */
		{
			bool changed = false;

			if (T & IBIT) {

				if (T == IBIT) {
					if (F == Q || F == 0) {
						// SELF
						// "Q?!0:Q" [1] -> "Q?!0:0" [0] -> Q
						printf(",   \"level2\":\"Q\",\"N\":%s%u}", ibit ? "~" : "", Q);
						return Q ^ ibit;
					} else {
						// OR
						// "Q?!0:F" [2]
					}
				} else if ((T & ~IBIT) == Q) {
					if (F == Q || F == 0) {
						// ZERO
						// "Q?!Q:Q" [4] -> "Q?!Q:0" [3] -> "0"
						printf(",   \"level2\":\"0\",\"N\":%s%u}", ibit ? "~" : "", 0);
						return 0 ^ ibit;
					} else {
						// LESS-THAN
						// "Q?!Q:F" [5] -> "F?!Q:F" -> "F?!Q:0"
						Q = F;
						F = 0;
						changed = true;
					}
				} else {
					if (F == 0) {
						// GREATER-THAN
						// "Q?!T:Q" [7] -> "Q?!T:0" [6]
					} else if (F == Q) {
						// GREATER-THAN
						// "Q?!T:Q" [7] -> "Q?!T:0" [6]
						F = 0;
						changed = true;
					} else if ((T & ~IBIT) == F) {
						// NOT-EQUAL
						// "Q?!F:F" [8]
					} else {
						// QnTF (new unified operator)
						// "Q?!T:F" [9]
					}
				}

			} else {

				if (T == 0) {
					if (F == Q || F == 0) {
						// ZERO
						// "Q?0:Q" [11] -> "Q?0:0" [10] -> "0"
						printf(",   \"level2\":\"0\",\"N\":%s%u}", ibit ? "~" : "", 0);
						return 0 ^ ibit;
					} else {
						// LESS-THAN
						// "Q?0:F" [12] -> "F?!Q:0" [6]
						T = Q ^ IBIT;
						Q = F;
						F = 0;
						changed = true;
					}

				} else if (T == Q) {
					if (F == Q || F == 0) {
						// SELF
						// "Q?Q:Q" [14] -> Q?Q:0" [13] -> "Q"
						printf(",   \"level2\":\"Q\",\"N\":%s%u}", ibit ? "~" : "", Q);
						return Q ^ ibit;
					} else {
						// OR
						// "Q?Q:F" [15] -> "Q?!0:F" [2]
						T = 0 ^ IBIT;
						changed = true;
					}
				} else {
					if (F == 0) {
						// AND
						// "Q?T:Q" [17] -> "Q?T:0" [16]
					} else if (F == Q) {
						// AND
						// "Q?T:Q" [17] -> "Q?T:0" [16]
						F = 0;
						changed = true;
					} else if (T == F) {
						// SELF
						// "Q?F:F" [18] -> "F"
						printf(",   \"level2\":\"F\",\"N\":%s%u}", ibit ? "~" : "", F);
						return F ^ ibit;
					} else {
						// QTF (old unified operator)
						// "Q?T:F" [19]
					}
				}
			}

			if (changed) {
				printf(",   \"level2\":{\"Q\":%u,\"T\":%s%u,\"F\":%u}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			}
		}

		/*
		 * test if node is available
		 *
		 * @date 2021-08-11 22:49:54
		 * This is intended as a fast-path, only it might be that the found node has been depreciated from a different context
		 * `testBasicNode()` now handles the end-condition
		 *
		 * @date 2021-08-13 00:22:55
		 * Now with ordered nodes, the written database should be fully ordered, making this test valid again
		 */
		{
			uint32_t ix = pTree->lookupNode(Q, T, F);
			if (pTree->nodeIndex[ix] != 0) {
				printf(",   \"lookup\":%s%u}}", ibit ? "~" : "", pTree->nodeIndex[ix]);
				return pTree->nodeIndex[ix] ^ ibit;
			}
		}

		/*
		 * Level-3 normalisation: single node rewrites
		 * simulate what `genrewritedata()` does:
		 *   Populate slots, perform member lookup, if not found/depreciated perform signature lookup
		 */
		char level3name[signature_t::SIGNATURENAMELENGTH + 1]; // name used to index `rewritedata[]`
		uint32_t level3mid = 0; // exact member match if non-zero
		uint32_t level3sid = 0; // signature match if non-zero (sid/mid are mutual exclusive)
		uint32_t sidSlots[tinyTree_t::TINYTREE_NEND];

		{
			static unsigned iVersion;
			uint32_t *buildVersion = pTree->allocMap();
			uint32_t *buildSlot = pTree->allocMap();

			tinyTree_t tree(ctx);
			unsigned nextNodeId = tinyTree_t::TINYTREE_NSTART;
			unsigned nextSlotId = tinyTree_t::TINYTREE_KSTART;

			++iVersion;
			assert(iVersion != 0);

			// setup zero
			buildVersion[0] = iVersion;
			buildSlot[0] = 0;

			// raw slots as loaded to index `rewriteData[]`
			uint32_t rwSlots[tinyTree_t::TINYTREE_NEND]; // reverse endpoint index (effectively, KSTART-NSTART being `slots[]`)

			/*
			 * Construct Q component
			 */

			unsigned tlQ;

			if (Q < pTree->nstart) {
				if (buildVersion[Q] != iVersion) {
					buildVersion[Q]       = iVersion;
					buildSlot[Q]          = nextSlotId;
					rwSlots[nextSlotId++] = Q;
				}
				tlQ = buildSlot[Q];
			} else {
				rwSlots[nextNodeId] = Q;
				tlQ = nextNodeId++;
				baseNode_t *pQ = pTree->N + Q;

				if (buildVersion[pQ->Q] != iVersion) {
					buildVersion[pQ->Q] = iVersion;
					buildSlot[pQ->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pQ->Q;
				}
				tree.N[tlQ].Q = buildSlot[pQ->Q];

				if (buildVersion[pQ->T & ~IBIT] != iVersion) {
					buildVersion[pQ->T & ~IBIT] = iVersion;
					buildSlot[pQ->T & ~IBIT] = nextSlotId;
					rwSlots[nextSlotId++]    = pQ->T & ~IBIT;
				}
				tree.N[tlQ].T = buildSlot[pQ->T & ~IBIT] ^ (pQ->T & IBIT);

				if (buildVersion[pQ->F] != iVersion) {
					buildVersion[pQ->F] = iVersion;
					buildSlot[pQ->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pQ->F;
				}
				tree.N[tlQ].F = buildSlot[pQ->F];

				// add node for back link
				buildVersion[Q] = iVersion;
				buildSlot[Q] = tlQ;
			}

			/*
			 * Construct T component
			 */

			uint32_t Ti = T & IBIT;
			uint32_t Tu = T & ~IBIT;
			unsigned tlT;

			if (Tu < pTree->nstart) {
				if (buildVersion[Tu] != iVersion) {
					buildVersion[Tu]  = iVersion;
					buildSlot[Tu]         = nextSlotId;
					rwSlots[nextSlotId++] = Tu;
				}
				tlT = buildSlot[Tu];
			} else {
				/*
				 * @date 2021-07-06 00:38:52
				 * Tu is a reference (10)
				 * It sets stored in `slots[10]`
				 * so that later, when revering `testTree`, `N[10]` will refer to Tu
				 * making the return `T` correct.
				 */
				rwSlots[nextNodeId] = Tu;
				tlT = nextNodeId++;
				baseNode_t *pT = pTree->N + Tu;

				if (buildVersion[pT->Q] != iVersion) {
					buildVersion[pT->Q] = iVersion;
					buildSlot[pT->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pT->Q;
				}
				tree.N[tlT].Q = buildSlot[pT->Q];

				if (buildVersion[pT->T & ~IBIT] != iVersion) {
					buildVersion[pT->T & ~IBIT] = iVersion;
					buildSlot[pT->T & ~IBIT] = nextSlotId;
					rwSlots[nextSlotId++]    = pT->T & ~IBIT;
				}
				tree.N[tlT].T = buildSlot[pT->T & ~IBIT] ^ (pT->T & IBIT);

				if (buildVersion[pT->F] != iVersion) {
					buildVersion[pT->F] = iVersion;
					buildSlot[pT->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pT->F;
				}
				tree.N[tlT].F = buildSlot[pT->F];

				// add node for back link
				buildVersion[Tu] = iVersion;
				buildSlot[Tu] = tlT;
			}

			/*
			 * Construct F component
			 */

			unsigned tlF;

			if (F < pTree->nstart) {
				if (buildVersion[F] != iVersion) {
					buildVersion[F] = iVersion;
					buildSlot[F]          = nextSlotId;
					rwSlots[nextSlotId++] = F;
				}
				tlF = buildSlot[F];
			} else {
				rwSlots[nextNodeId] = F;
				tlF = nextNodeId++;
				baseNode_t *pF = pTree->N + F;

				if (buildVersion[pF->Q] != iVersion) {
					buildVersion[pF->Q] = iVersion;
					buildSlot[pF->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pF->Q;
				}
				tree.N[tlF].Q = buildSlot[pF->Q];

				if (buildVersion[pF->T & ~IBIT] != iVersion) {
					buildVersion[pF->T & ~IBIT] = iVersion;
					buildSlot[pF->T & ~IBIT] = nextSlotId;
					rwSlots[nextSlotId++]    = pF->T & ~IBIT;
				}
				tree.N[tlF].T = buildSlot[pF->T & ~IBIT] ^ (pF->T & IBIT);

				if (buildVersion[pF->F] != iVersion) {
					buildVersion[pF->F] = iVersion;
					buildSlot[pF->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pF->F & ~IBIT;
				}
				tree.N[tlF].F = buildSlot[pF->F];
			}

			/*
			 * Construct top-level
			 */
			tree.root = nextNodeId;
			tree.count = nextNodeId+1;
			tree.N[tree.root].Q = tlQ;
			tree.N[tree.root].T = tlT ^ Ti;
			tree.N[tree.root].F = tlF;

			/*
			 * @date 2021-07-14 22:38:41
			 * Normalize to sanitize the name for lookups
			 */
			tree.saveString(tree.root, level3name, NULL);
			tree.loadStringSafe(level3name);

			/*
			 * The tree has a different endpoint allocation.
			 * The `rewriteData[]` index scans from left-to-right, otherwise it's (the default) depth-first
			 * Convert to depth-first, because that is how members are indexed,
			 * then apply the reverse transform of the skin to update the slots.
			 */

			printf(",   \"level3\":{\"rwslots\"");
			for (unsigned i=tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++) {
				if (i == tinyTree_t::TINYTREE_KSTART)
					printf(":[%u", rwSlots[i]);
				else
					printf(",%u", rwSlots[i]);
			}
			printf("]");

			/*
			 * Determine difference between left-to-right and depth-first
			 * and convert `rawSlots[]` to `slots[]` accordingly
			 */
			char skin[MAXSLOTS + 1];
			tree.saveString(tree.root, level3name, skin);

			printf(",\"name\":\"%s/%s\"", level3name, skin);


			/*
			 * Lookup signature
			 */
			uint32_t tid;

			// lookup the tree used by the detector
			pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &level3sid, &tid);
			assert(level3sid);

			printf(",\"sid\":\"%u:%s\"",
			       level3sid, pStore->signatures[level3sid].name);

			/*
			 * Lookup member
			 */

			uint32_t ix = pStore->lookupMember(level3name);
			level3mid = pStore->memberIndex[ix];
			member_t *pMember = pStore->members + level3mid;

			if (level3mid == 0 || (pMember->flags & member_t::MEMMASK_DEPR)) {
				level3mid = 0;
			} else {
				// use capitals to visually accentuate presence
				printf(",\"MID\":\"%u:%s/%u:%.*s\"",
					level3mid, pMember->name,
					pMember->tid, pStore->signatures[pMember->sid].numPlaceholder, pStore->revTransformNames[pMember->tid]);
			}

			/*
			 * Translate slots relative to `rwSlots[]`
			 */
			for (unsigned i = tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++)
				sidSlots[i] = rwSlots[tinyTree_t::TINYTREE_KSTART + pStore->fwdTransformNames[tid][i - tinyTree_t::TINYTREE_KSTART] - 'a'];

			printf(",\"sidslots\"");
			for (unsigned i=tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++) {
				if (i == tinyTree_t::TINYTREE_KSTART)
					printf(":[%u", sidSlots[i]);
				else
					printf(",%u", sidSlots[i]);
			}
			printf("]");
			printf("}");

			pTree->freeMap(buildVersion);
			pTree->freeMap(buildSlot);
		}

		/*
		 * Level-4 signature operand swapping
		 */
		{
			bool displayed = false;

			signature_t *pSignature = pStore->signatures + level3sid;
			if (pSignature->swapId) {
				swap_t *pSwap = pStore->swaps + pSignature->swapId;

				bool changed;
				do {
					changed = false;

					for (unsigned iSwap = 0; iSwap < swap_t::MAXENTRY && pSwap->tids[iSwap]; iSwap++) {
						unsigned tid = pSwap->tids[iSwap];

						// get the transform string
						const char *pTransformStr = pStore->fwdTransformNames[tid];

						// test if swap needed
						bool needSwap = false;

						for (unsigned i = 0; i < pSignature->numPlaceholder; i++) {
							if (sidSlots[tinyTree_t::TINYTREE_KSTART + i] > sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a']) {
								needSwap = true;
								break;
							}
							if (sidSlots[tinyTree_t::TINYTREE_KSTART + i] < sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a']) {
								needSwap = false;
								break;
							}
						}

						if (needSwap) {
							if (!displayed)
								printf(",   \"level4\":[");
							else
								printf(",");
							printf("%.*s", pSignature->numPlaceholder, pStore->fwdTransformNames[tid]);
							displayed = true;

							uint32_t newSlots[MAXSLOTS];

							for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
								newSlots[i] = sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a'];

							for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
								sidSlots[tinyTree_t::TINYTREE_KSTART + i] = newSlots[i];

							changed = true;
						}
					}
				} while(changed);
			}

			if (displayed)
				printf("]");
		}

		/*
		 * Level-5 normalisation: single node rewrites
		 */
		uint32_t level5mid = 0;
		{
			uint32_t bestCount = 0;

			if (level3mid != 0) {
				level5mid = level3mid;
			} else {
				/*
				 * @date 2021-07-12 23:15:58
				 * The best scoring members are the first on the list.
				 * Test how many nodes need to be created to store the runtime components
				 * This includes the top-level node that she current call is creating.
				 */
				printf(",\"probe\":[");
				for (unsigned iMid = pStore->signatures[level3sid].firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
					member_t *pMember = pStore->members + iMid;

					// depreciated are at the end of the list
					if (pMember->flags & member_t::MEMMASK_DEPR)
						break;

					uint32_t failCount = 0;
					explainStringFast(depth + 1, expectId, pTree, pMember->name, pStore->revTransformNames[pMember->tid], sidSlots + tinyTree_t::TINYTREE_KSTART, &failCount);
					if (level5mid != 0)
						printf(",");
					printf("{\"name\":\"%u:%s/%u:%.*s\",\"miss\":%u}", iMid, pMember->name,
					       pMember->tid, pStore->signatures[pMember->sid].numPlaceholder, pStore->revTransformNames[pMember->tid],
					       failCount);

					if (level5mid == 0 || failCount < bestCount) {
						level5mid = iMid;
						bestCount = failCount;

						if (bestCount == 0) {
							break; // its already there
						} else if (bestCount == 1)
							break; // everything present except top-level
					}
				}
				printf("]");
			}
			assert(level5mid);

			// apply best
			member_t *pMember = pStore->members + level5mid;

			printf(",   \"level5\":{\"member\":\"%u:%s/%u:%.*s\"}",
			       level5mid, pMember->name,
			       pMember->tid, pStore->signatures[pMember->sid].numPlaceholder, pStore->revTransformNames[pMember->tid]);
		}

		/*
		 * apply found member
		 */
		uint32_t ret = explainStringFast(depth + 1, expectId, pTree, pStore->members[level5mid].name, pStore->revTransformNames[pStore->members[level5mid].tid], sidSlots + tinyTree_t::TINYTREE_KSTART, NULL);

		printf(",   \"N\":%s%u}", ibit ? "~" : "", ret);

		/*
		 * @date 2021-08-11 22:38:55
		 * Sometimes a rerun may result in a different tree.
		 * This is because normalisation adapts to what is already found in the tree.
		 *
		 * Some used samples
		 * ./bexplain 'cd^agd1!eh^!a2gdgcd!^c!!!' 'cd^agd1!eh^!a2gdgcd!^c!!!'
		 * ./bexplain 'ef^eg!gg2^^eg!ab^c1dacab!^!^^1aabccd^a7>!+2!^B2ac!ccdB3!^ac!>!^^2!C6C1B5^1g>C8!^1C5c>C6d1!^ggef+^eD5>!5caB1C6!C6!!^93^4gB0^^9B0!>!^^'
		 */

		return ret ^ ibit;
	}

	/*
	 * @date 2021-07-05 19:19:25
	 *
	 * Expand and create a structure name with transform.
	 * Fast version specifically for user input
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {string} name - structure name
	 * @param {string} skin - structure skin
	 * @param {number[]} slots - structure run-time slots
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainStringSafe(unsigned depth, baseTree_t *pTree, const char *pPattern, const char *pTransform) {

		// modify if transform is present
		uint32_t *transformList = NULL;
		if (pTransform && *pTransform)
			transformList = pTree->decodeTransform(ctx, pTree->kstart, pTree->nstart, pTransform);

		/*
		 * init
		 */

		uint32_t stackPos = 0;
		uint32_t nextNode = pTree->nstart;
		uint32_t *pStack  = pTree->allocMap();
		uint32_t *pMap    = pTree->allocMap();
		uint32_t nid;

		/*
		 * Load string
		 */
		for (const char *pattern = pPattern; *pattern; pattern++) {

			switch (*pattern) {
			case '0': //
				pStack[stackPos++] = 0;
				break;

				// @formatter:off
			case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8':
			case '9':
			// @formatter:on
			{
				/*
				 * Push back-reference
				 */
				uint32_t v = nextNode - (*pattern - '0');

				if (v < pTree->nstart || v >= nextNode)
					ctx.fatal("[node out of range: %d]\n", v);
				if (stackPos >= pTree->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[stackPos++] = pMap[v];

				break;
			}

				// @formatter:off
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			// @formatter:on
			{
				/*
				 * Push endpoint
				 */
				uint32_t v = pTree->kstart + (*pattern - 'a');

				if (v < pTree->kstart || v >= pTree->nstart)
					ctx.fatal("[endpoint out of range: %d]\n", v);
				if (stackPos >= pTree->ncount)
					ctx.fatal("[stack overflow]\n");

				if (transformList)
					pStack[stackPos++] = transformList[v];
				else
					pStack[stackPos++] = v;
				break;

			}

				// @formatter:off
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
			// @formatter:on
			{
				/*
				 * Prefix
				 */
				uint32_t v = 0;
				while (isupper(*pattern))
					v = v * 26 + *pattern++ - 'A';

				if (isdigit(*pattern)) {
					/*
					 * prefixed back-reference
					 */
					v = nextNode - (v * 10 + *pattern - '0');

					if (v < pTree->nstart || v >= nextNode)
						ctx.fatal("[node out of range: %d]\n", v);
					if (stackPos >= pTree->ncount)
						ctx.fatal("[stack overflow]\n");

					pStack[stackPos++] = pMap[v];
				} else if (islower(*pattern)) {
					/*
					 * prefixed endpoint
					 */
					v = pTree->kstart + (v * 26 + *pattern - 'a');

					if (v < pTree->kstart || v >= pTree->nstart)
						ctx.fatal("[endpoint out of range: %d]\n", v);
					if (stackPos >= pTree->ncount)
						ctx.fatal("[stack overflow]\n");

					if (transformList)
						pStack[stackPos++] = transformList[v];
					else
						pStack[stackPos++] = v;
				} else {
					ctx.fatal("[bad token '%c']\n", *pattern);
				}
				break;
			}

			case '>': {
				// GT
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--stackPos];
				uint32_t L = pStack[--stackPos];

				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, R ^ IBIT, 0, NULL);
				printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '+': {
				// OR
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t R = pStack[--stackPos]; // right hand side
				uint32_t L = pStack[--stackPos]; // left hand side

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, IBIT, R, NULL);
				printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '^': {
				// XOR/NE
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t R = pStack[--stackPos]; // right hand side
				uint32_t L = pStack[--stackPos]; // left hand side

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, R ^ IBIT, R, NULL);
				printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '!': {
				// QnTF
				if (stackPos < 3)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t F = pStack[--stackPos];
				uint32_t T = pStack[--stackPos];
				uint32_t Q = pStack[--stackPos];

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, Q, T ^ IBIT, F, NULL);
				printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t R = pStack[--stackPos]; // right hand side
				uint32_t L = pStack[--stackPos]; // left hand side

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, R, 0, NULL);
				printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF
				if (stackPos < 3)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t F = pStack[--stackPos];
				uint32_t T = pStack[--stackPos];
				uint32_t Q = pStack[--stackPos];

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, Q, T, F, NULL);
				printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT
				if (stackPos < 1)
					ctx.fatal("[stack underflow]\n");

				pStack[stackPos - 1] ^= IBIT;
				break;
			}
			case '/':
				// separator between pattern/transform
				while (pattern[1])
					pattern++;
				break;
			case ' ':
				// skip spaces
				break;
			default:
				ctx.fatal("[bad token '%c']\n", *pattern);
			}

			if (stackPos > pTree->maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (stackPos != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[stackPos - 1];

		pTree->freeMap(pStack);
		pTree->freeMap(pMap);
		if (transformList)
			pTree->freeMap(transformList);

		return ret;
	}

	/**
	 * @date 2021-06-08 21:00:46
	 *
	 * Create/load tree based on arguments
	 */
	baseTree_t *main(unsigned numArgs, char *inputArgs[]) {

		/*
		 * Determine number of keys
		 */
		unsigned      numKeys = 0;
		for (unsigned iArg    = 0; iArg < numArgs; iArg++) {
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
			char     stack[10], *pStack = stack;
			// value to be encoded
			uint32_t value              = iKey - pTree->kstart;

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
			unsigned iRoot = ostart + iArg;

			// find transform delimiter
			const char *pTransform = strchr(inputArgs[iArg], '/');

			if (pTransform)
				pTree->roots[iRoot] = explainStringSafe(0, pTree, inputArgs[iArg], pTransform + 1);
			else
				pTree->roots[iRoot] = explainStringSafe(0, pTree, inputArgs[iArg], NULL);

			/*
			 * Display expression
			 */

			std::string name;
			std::string transform;

			// display root name
			printf("%s: ", pTree->rootNames[iRoot].c_str());

			// display expression
			if (opt_normalise) {
				name = pTree->saveString(pTree->roots[iRoot], &transform);
				printf(": %s/%s", name.c_str(), transform.c_str());
			} else {
				name = pTree->saveString(pTree->roots[iRoot]);
				printf(": %s", name.c_str());
			}

			printf("\n");

		}

		return pTree;
	}
};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {bevalContext_t} Application context
 */
bevalContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <pattern> ...\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename>   Database to query [default=%s]\n", app.opt_databaseName);
		fprintf(stderr, "\t   --extend\n");
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t-n --normalise  Display pattern as: normalised/transform\n");
		fprintf(stderr, "\t-t --numtests=<seconds> [default=%d]\n", app.opt_dataSize);
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t   --seed=n     Random seed to generate evaluator test pattern. [Default=%u]\n", app.opt_seed);
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
			LO_HELP     = 1, LO_DEBUG, LO_FORCE, LO_MAXNODE, LO_SEED, LO_TIMER,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_DATABASE = 'D', LO_DATASIZE = 't', LO_NORMALISE = 'n', LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"datasize",    1, 0, LO_DATASIZE},
			{"debug",       1, 0, LO_DEBUG},
			{"force",       0, 0, LO_FORCE},
			{"help",        0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"normalise",   0, 0, LO_NORMALISE},
			{"quiet",       2, 0, LO_QUIET},
			{"seed",        1, 0, LO_SEED},
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
		case LO_DATABASE:
			app.opt_databaseName = optarg;
			break;
		case LO_DATASIZE:
			app.opt_dataSize = (unsigned) strtoul(optarg, NULL, 10);
			break;
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
		case LO_NORMALISE:
			app.opt_normalise++;
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_SEED:
			app.opt_seed = ::strtoul(optarg, NULL, 0);
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

	if (argc - optind < 1) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * Main
	 */

	// set random seed
	if (app.opt_seed)
		srand(app.opt_seed);
	else
		srand(clock());

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

		// Open database
	database_t db(ctx);

	db.open(app.opt_databaseName);

	// display system flags when database was created
	if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] DB FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(db.creationFlags));

	app.pStore   = &db;

	/*
	 * Construct the tree
	 */

	app.main(argc - optind, argv + optind);

	/*
	 * Analyse the result
	 */
	return 0;
}
