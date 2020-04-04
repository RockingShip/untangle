#ifndef _GENERATOR_H
#define _GENERATOR_H

/*
 * @date 2020-03-17 20:04:16
 *
 * Generate all possible normalised structures for a given structure size.
 *
 * Structures will pass level-1 normalisation with exception the of ordering of dyadics.
 * This is because during runtime the placeholder endpoints will be replaced by actual
 * values which might have different values.
 *
 * Generation is based on templates instead of nested `for` loops for generating `Q,T,F` combos.
 *
 * Demonstrated:
 *  - Tree deep-comparison
 *  - Packed `QTF` notation
 *
 *  Basically, `generator_t` is a tree notation decoder (like `decode()`) with a wildcard as notation
 *
 * @date 2020-03-17 23:54:39
 *
 * Building `QnTF` datasets is two-pass:
 *  - First pass is build `QTF` signature base.
 *  - Second pass is to optimize signature names to `QnTF`-only notation.
 *  - `QnTF` databases should be considered `10n9` with full completeness up to `5n9`.
 *
 *  @date 2020-03-18 21:12:26
 *
 *  It generates fully normalised and naturally ordered trees for further processing.
 *  With this version, all calls to `foundTree()` are notation unique.
 *
 *  @date 2020-03-24 02:31:46
 *
 * Duplicates calls to `foundTree()` can happen.
 * It happens when substituted popped values are also present as a template.
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
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "tinytree.h"

/**
 * @date 2020-03-17 20:22:08
 *
 * `generatorTree_t` extends `tinyTree_t` by giving it tree creation capabilities
 */
struct generatorTree_t : tinyTree_t {

	/*
	 * @date 2020-03-19 16:12:52
	 *
	 * Packed Notation to make operator fit into 16 bits
	 *   packedQTF =  <invertedT> << (WIDTH*3) | Q << (WIDTH*2) | T << (WIDTH*1) | F << (WIDTH*0)
	 *
	 * This order is chosen as it is the order found on stacks during `decode()`
	 */
	enum {
		/// @constant {number} - Field width
		PACKED_WIDTH = 5,

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_MASK = (1 << PACKED_WIDTH) - 1,

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_FPOS = (PACKED_WIDTH * 0),

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_TPOS = (PACKED_WIDTH * 1),

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_QPOS = (PACKED_WIDTH * 2),

		/// @constant {number} - Position of `inverted-T` bit
		PACKED_TIPOS = (PACKED_WIDTH * 3),

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_TIBIT = 1 << PACKED_TIPOS,

		/// @constant {number} - Size of packed word in bits
		PACKED_SIZE = 16,

		/// @constant {number} - used for `pIsType[]` to indicate type of node
		PACKED_OR = 0x01,
		PACKED_GT = 0x02,
		PACKED_XOR = 0x04,
		PACKED_QnTF = 0x08,
		PACKED_AND = 0x10,
		PACKED_QTF = 0x20,
		PACKED_UNORDERED = 0x40,

		/// @constant {number} - size of `pTemplateData[]` (166448 for `QnTF-only`, 314772 otherwise)
		TEMPLATE_MAXDATA = 320000,

		/// @constant {number} - Different sections in `pTemplateData[]`
		TEMPLATE_QTF = 0b000,
		TEMPLATE_QTP = 0b001,
		TEMPLATE_QPF = 0b010,
		TEMPLATE_QPP = 0b011,
		TEMPLATE_PTF = 0b100,
		TEMPLATE_PTP = 0b101,
		TEMPLATE_PPF = 0b110,
	};

	/// @var {number[]} lookup table for `push()` index by packed `QTF`
	uint32_t *pCacheQTF;

	/// @var {number[]} versioned memory for `pCacheQTF`
	uint32_t *pCacheVersion;

	/// @var {number} current version incarnation
	uint32_t iVersion;

	/// @var {uint32_t[]} array of packed unified operators
	uint32_t packedN[TINYTREE_NEND];

	/// @var {uint8_t[]} array indexed by packed `QTnF` to indicate what type of operator
	uint8_t *pIsType;

	/// @var {uint64_t} lower bound `progress` (if non-zero)
	uint64_t windowLo;

	/// @var {uint64_t} upper bound (not including) `progress` (if non-zero)
	uint64_t windowHi;

	/// @var {uint64_t[]} restart data
	const uint64_t *pRestartData;

	/// @var {number} Indication that a restart point has passed
	uint64_t restartTick;

	/// @var {number} Number of restart entries found
	uint64_t numFoundRestart;

	/// @var {number[]} template data for generator
	uint32_t *pTemplateData;

	/// @var {number[7][10][10]} starting offset in `templateData[]`. `templateIndex[SECTION][numNode][numPlaceholder]`
	uint32_t templateIndex[7][tinyTree_t::TINYTREE_MAXNODES][MAXSLOTS + 1];

	/// @var {tintTree_t} Tree needed to re-order endpoints before calling `foundTree()`
	tinyTree_t foundTree;


	/**
	 * @date 2020-03-18 18:45:33
	 *
	 * Constructor
	 *
	 * @param {context_t} ctx - I/O context
	 */
	inline generatorTree_t(context_t &ctx) : tinyTree_t(ctx), foundTree(ctx) {
		// Assert that the highest available node fits into a 5 bit value. `2^5` = 32. Last 3 are reserved for template wildcards
		assert(TINYTREE_NEND < 32 - 3);

		windowLo = 0;
		windowHi = 0;
		pRestartData = NULL;
		restartTick = 0;
		numFoundRestart = 0;
		memset(templateIndex, 0, sizeof(templateIndex));

		// allocate structures
		pIsType = (uint8_t *) ctx.myAlloc("generatorTree_t::pIsType", 1 << PACKED_SIZE, sizeof(*this->pIsType));
		pCacheQTF = (uint32_t *) ctx.myAlloc("generatorTree_t::pCacheQTF", 1 << PACKED_SIZE, sizeof(*this->pCacheQTF));
		pCacheVersion = (uint32_t *) ctx.myAlloc("generatorTree_t::pCacheVersion", 1 << PACKED_SIZE, sizeof(*this->pCacheVersion));
		pTemplateData = (uint32_t *) ctx.myAlloc("generatorTree_t::pTemplateData", TEMPLATE_MAXDATA, sizeof(*pTemplateData));

		// clear versioned memory
		iVersion = 0;
		clearGenerator();

		initialiseGenerator();
	}

	/**
	 * @date 2020-03-18 19:30:09
	 *
	 * Release system resources
	 */
	~generatorTree_t() {
		ctx.myFree("generatorTree_t::pIsType", this->pIsType);
		ctx.myFree("generatorTree_t::pCacheQTF", this->pCacheQTF);
		ctx.myFree("generatorTree_t::pCacheVersion", this->pCacheVersion);
		ctx.myFree("generatorTree_t::pTemplateData", this->pTemplateData);
	}

	/**
	 * @date 2020-03-20 18:27:44
	 *
	 * Erase the contents
	 */
	inline void clearGenerator(void) {
		// bump incarnation.
		if (iVersion == 0) {
			// clear versioned memory
			::memset(pCacheVersion, 0, (sizeof(*pCacheVersion) * (1 << PACKED_SIZE)));
		}
		iVersion++; // when overflows, next call will clear

		this->clearTree();
	}

	/**
	 * @date 2020-03-18 21:05:34
	 *
	 * Initialise lookup tables for generator
	 */
	void initialiseGenerator(void) {
		/*
		 * Create lookup table indexed by packed QnTF notation to determine if `Q,T,F` combo is normalised
		 * Exclude ordered dyadics.
		 */

		// @formatter:off
		for (uint32_t Ti = 0; Ti < 2; Ti++)
		for (uint32_t F = 0; F < (1 << PACKED_WIDTH); F++)
		for (uint32_t To = 0; To < (1 << PACKED_WIDTH); To++)
		for (uint32_t Q = 0; Q < (1 << PACKED_WIDTH); Q++) {
		// @formatter:on

			// create packed notation
			uint32_t ix = (Ti ? 1 << 15 : 0 << 15) | Q << 10 | To << 5 | F << 0;

			// default, not normalised
			pIsType[ix] = 0;

			// test if combo is normalised
			if (Q == To)
				continue;  // Q?Q:F or Q?~Q:F
			if (Q == F)
				continue; // Q?T:Q or Q?~T:Q
			if (Q == 0)
				continue; // 0?X:Y
			if (To == F && F == 0)
				continue; // Q?~0:0
			if (To == F && !Ti)
				continue; // "SELF" Q?F:F
			if (To == 0 && !Ti)
				continue; // "LT" Q?0:F -> F?~Q:0

			// passed, normalised

			/*
			 * Reminder:
			 *  [ 2] a ? ~0 : b                  "+" OR
			 *  [ 6] a ? ~b : 0                  ">" GT
			 *  [ 8] a ? ~b : b                  "^" XOR
			 *  [ 9] a ? ~b : c                  "!" QnTF
			 *  [16] a ?  b : 0                  "&" AND
			 *  [19] a ?  b : c                  "?" QTF
			 */

			if (Ti) {
				if (To == 0)
					pIsType[ix] |= PACKED_OR;
				else if (F == 0)
					pIsType[ix] |= PACKED_GT;
				else if (F == To)
					pIsType[ix] |= PACKED_XOR;
				else
					pIsType[ix] |= PACKED_QnTF;
			} else {
				if (F == 0)
					pIsType[ix] |= PACKED_AND;
				else
					pIsType[ix] |= PACKED_QTF;
			}

			/*
			 * @date 2020-03-23 23:09:54
			 *
			 * Test if dyadics are ordered
			 * NOTE: testing against id (as opposed to deep compare) is sufficient because tree is fully normalised
			 */
			if ((pIsType[ix] & PACKED_OR) && Q > F)
				pIsType[ix] |= PACKED_UNORDERED;
			if ((pIsType[ix] & PACKED_XOR) && Q > F)
				pIsType[ix] |= PACKED_UNORDERED;
			if ((pIsType[ix] & PACKED_AND) && Q > To)
				pIsType[ix] |= PACKED_UNORDERED;
		}

		/*
		 * @date 2020-03-18 10:52:57
		 *
		 * Wildcard values represent node-references that are popped from the stack during runtime.
		 * Zero means no wildcard, otherwise it must be a value greater than NSTART
		 */

		uint32_t numTemplateData = 1; // skip initial zero

		/*
		 * Run in multiple rounds, each round is a 3-bit mask, each bit indicating which operands are wildcards
		 * Do not include all bits set because that implies at runtime all operands were popped from stack with optimized handling
		 */
		for (unsigned iWildcard = 0; iWildcard < 0b111; iWildcard++) {

			// @date  2020-03-23 13:27:11 -- range: 0 < numPlaceholder <= MAXSLOTS

			// @formatter:off
			for (unsigned numNode=0; numNode < tinyTree_t::TINYTREE_MAXNODES; numNode++)
			for (unsigned numPlaceholder=0; numPlaceholder < (MAXSLOTS + 1); numPlaceholder++) {
			// @formatter:on

				/*
				 * Iterate through all possible `Q,T,F` possibilities
				 * First all the `QnTF` (Ti=1), then all the `QTF` (Ti=0)
				 *
				 * This to allow early bailout of list handling in `QnTF` mode.
				 */

				templateIndex[iWildcard][numNode][numPlaceholder] = numTemplateData;

				// @formatter:off
				for (int Ti = 1; Ti >= 0; Ti--)
				for (unsigned Q = 0; Q < tinyTree_t::TINYTREE_NSTART + numNode; Q++)
				for (unsigned To = 0; To < tinyTree_t::TINYTREE_NSTART + numNode; To++)
				for (unsigned F = 0; F < tinyTree_t::TINYTREE_NSTART + numNode; F++) {
				// @formatter:on

					if (!Ti && (this->flags & context_t::MAGICMASK_QNTF)) {
						// reject `non-QnTF` template in `QnTF-only` invocation
						continue;
					}

					unsigned nextPlaceholder = numPlaceholder;

					/*
					 * Test if some placeholders are wildcards.
					 * Wildcards get runtime replaced by popped values from the stack
					 * The replacement values must be higher than the end-loop condition
					 *
					 * @date 2020-03-24 19:45:38
					 *   Generator will never exceed 6-7 nodes.
					 *   Wildcard replacements should fit in 5 bits because of `pIsType[]`
					 *   Use top-3 id's that fit in 5 bits
					 */
					if (iWildcard & 0b100) {
						Q = 0x1d; // assign unique value and break loop after finishing code block
					} else {
						// Q must be a previously existing placeholder
						if (Q > tinyTree_t::TINYTREE_KSTART + nextPlaceholder && Q < tinyTree_t::TINYTREE_NSTART)
							continue; // placeholder not created yet
						// bump placeholder if using for the first time
						if (Q == tinyTree_t::TINYTREE_KSTART + nextPlaceholder)
							nextPlaceholder++;

						if (nextPlaceholder > MAXSLOTS)
							continue; // skip if exceeds maximum

						// verify that fielded does not overflow
						assert(!(Q & ~PACKED_MASK));
					}

					if (iWildcard & 0b010) {
						To = 0x1e; // assign unique value and break loop after finishing code block
					} else {
						// T must be a previously existing placeholder
						if (To > tinyTree_t::TINYTREE_KSTART + nextPlaceholder && To < tinyTree_t::TINYTREE_NSTART)
							continue; // placeholder not created yet
						// bump placeholder if using for the first time
						if (To == tinyTree_t::TINYTREE_KSTART + nextPlaceholder)
							nextPlaceholder++;

						if (nextPlaceholder > MAXSLOTS)
							continue; // skip if exceeds maximum

						// verify that fielded does not overflow
						assert(!(To & ~PACKED_MASK));
					}

					if (iWildcard & 0b001) {
						F = 0x1f; // assign unique value and break loop after finishing code block
					} else {
						// F must be a previously existing placeholder
						if (F > tinyTree_t::TINYTREE_KSTART + nextPlaceholder && F < tinyTree_t::TINYTREE_NSTART)
							continue; // placeholder not created yet
						// bump placeholder if using for the first time
						if (F == tinyTree_t::TINYTREE_KSTART + nextPlaceholder)
							nextPlaceholder++;

						if (nextPlaceholder > MAXSLOTS)
							continue; // skip if exceeds maximum

						// verify that fielded does not overflow
						assert(!(F & ~PACKED_MASK));
					}

					// create packed notation
					uint32_t qtf = (Ti << PACKED_TIPOS) | (Q << PACKED_QPOS) | (To << PACKED_TPOS) | (F << PACKED_FPOS);

					if (pIsType[qtf] == 0)
						continue; // must be normalised
					if (pIsType[qtf] & PACKED_UNORDERED)
						continue; // must be ordered

					// zero out wildcards
					uint32_t outQ = (Q == 0x1d) ? 0 : Q;
					uint32_t outT = (To == 0x1e) ? 0 : To;
					uint32_t outF = (F == 0x1f) ? 0 : F;

					// add template
					pTemplateData[numTemplateData++] = (nextPlaceholder << PACKED_SIZE) | (Ti << PACKED_TIPOS) | outQ << PACKED_QPOS | outT << PACKED_TPOS | outF << PACKED_FPOS;
				}

				// end of section
				pTemplateData[numTemplateData++] = 0;
			}

			// bump data index
		}

//		fprintf(stderr, "numTemplateData=%d\n", numTemplateData);
		assert(numTemplateData <= TEMPLATE_MAXDATA);
	}

	/**
	 * @date 2020-03-18 18:15:57
	 *
	 * Push/add packed node to tree
	 *
	 * @param {uint32_t} qtf - packed notation of `QTF`
	 * @return {uint32_t} 0 if not normalised, or node id of already existing or newly created one
	 */
	inline uint32_t push(uint32_t qtf) {
		// is it a valid packed notation
		assert((qtf & ~0xffff) == 0);

		// test if normalised
		if (pIsType[qtf] == 0)
			return 0; // no

		// test if already in cache then fail. 
		// Must use a back-reference and not create a new node 
		if (pCacheVersion[qtf] == iVersion && pCacheQTF[qtf] != 0)
			return 0;

		// add/push packed node
		uint32_t nid = this->count++;
		assert(nid < TINYTREE_NEND); // overflow
		assert((nid & ~PACKED_MASK) == 0); // may not overflow packed field

		// add to packed nodes
		this->packedN[nid] = qtf;

		// add to cache of fast duplicate lookups
		pCacheQTF[qtf] = nid;
		pCacheVersion[qtf] = iVersion;

#if 0
		// save type of node so `qtf` can be modified
		uint32_t typ = pIsType[qtf];
#endif

		// populate node
		tinyNode_t *pNode = this->N + nid;
		pNode->F = qtf & PACKED_MASK;
		qtf >>= PACKED_WIDTH;
		pNode->T = qtf & PACKED_MASK;
		qtf >>= PACKED_WIDTH;
		pNode->Q = qtf & PACKED_MASK;
		qtf >>= PACKED_WIDTH;
		if (qtf)
			pNode->T ^= IBIT;

#if 0
		/*
		 * @date 2020-03-26 12:27:30
		 *
		 * "abc?d1^^" is the same as "dabc?^2^", the difference that the top-level `XOR` is swapped.
		 *
		 * The first is from the generator which (for speed) compares by id,
		 * the second if by `tinyTree_t` which does deep-compare.
		 *
		 * The generator used `packedN[]` which needs to be converted to `N[]`.
		 * During conversion, apply deep-compare.
		 *
		 * However, swapping the operands of symmetric dyadics changes the tree walk path.
		 *
		 * `"ab>cd+^"` would change into `"cd+ab>^"` which is actually `"ab+cd>^/cdab"`
		 *
		 * @date 2020-03-26 23:07:48
		 *
		 * Turn out this doesn't work.
		 * When reconstructing an ordered tree from a non-ordered generated one, all the endpoints get re-assigned.
		 * This re-assignment has the side effect that `compare()` outcomes are different.
		 * `tinyTree_t::reconstruct()` re-orders on the fly before a `compare()` is called.
		 *
		 * Drop this code and leave for historics.
		 */
		assert(!"never enable this code");
		if (typ & (PACKED_OR | PACKED_XOR | PACKED_AND)) {
			if (typ & PACKED_OR) {
				// swap `OR` if unordered
				if (this->compare(pNode->Q, *this, pNode->F, true) > 0) {
					assert(pNode->T == IBIT);
					uint32_t savQ = pNode->Q;
					pNode->Q = pNode->F;
					pNode->F = savQ;
				}
			} else if (typ & PACKED_XOR) {
				// swap `XOR` if unordered
				if (this->compare(pNode->Q, *this, pNode->F, true) > 0) {
					uint32_t savQ = pNode->Q;
					pNode->Q = pNode->F;
					pNode->F = savQ;
					pNode->T = savQ ^ IBIT;
				}
			} else {
				// swap `AND` if unordered
				if (this->compare(pNode->Q, *this, pNode->T, true) > 0) {
					uint32_t savQ = pNode->Q;
					pNode->Q = pNode->T;
					pNode->T = savQ;
				}
			}
		}
#endif

		return nid;
	}

	/**
	 * @date 2020-03-18 18:00:10
	 *
	 * Unwind pushed nodes from tree, releasing nodes that were created
	 */
	inline void pop(void) {
		// pop node
		uint32_t qtf = this->packedN[--this->count];

		// erase index
		pCacheQTF[qtf] = 0;
	}

	/**
	 * @date 2020-03-24 16:01:58
	 *
	 * Typedef of callback function to `"void foundTree(generatorTree_t &tree, unsigned numUnique)"`
	 *
	 * @typedef {callback} generateTreeCallback_t
	 */
	typedef void(context_t::* generateTreeCallback_t)(tinyTree_t &, const char *, unsigned);

	/**
 * @date 2020-03-26 13:43:17
 *
 * Copy a given tree and re-construct in tree walking order and placeholder/skin
 *
 * Also output zero-terminated "placeholder/skin".
 *
 * @param {number} root - entry point
 * @param {string} pName - output, notation of tree
 * @param {string} pSkin - output, accompanying skin for placeholders
 * @param pPlaceholders
 */
	void unpack(uint32_t root, char *pName, char *pSkin) {
		// temporary stack storage for postfix notation
		uint32_t stack[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int stackPos = 0;

		unsigned nameLen = 0; // length of notation
		unsigned numPlaceholder = 0; // first free placeholder

		// nodes already processed
		uint32_t beenThere;
		uint32_t beenWhat[TINYTREE_NEND];

		this->clearTree();

		// mark `zero` processed
		beenThere = (1 << 0);
		beenWhat[0] = 0;

		// push start on stack
		stack[stackPos++] = root & ~IBIT;

		/*
		 * Pass-1, reconstruct path
		 */
		do {
			// pop stack
			uint32_t curr = stack[--stackPos];

			// unpack
			uint32_t qtf = packedN[curr];

			uint32_t F = qtf & PACKED_MASK;
			qtf >>= PACKED_WIDTH;
			uint32_t To = qtf & PACKED_MASK;
			qtf >>= PACKED_WIDTH;
			uint32_t Q = qtf & PACKED_MASK;
			qtf >>= PACKED_WIDTH;
			uint32_t Ti = qtf & 1;

			// determine if node already handled
			if (~beenThere & (1 << curr)) {
				/// first time

				// push id so it visits again a second time
				stack[stackPos++] = curr;

				// push unvisited references
				if (F >= TINYTREE_NSTART && (~beenThere & (1 << F)))
					stack[stackPos++] = F;
				if (To != F && To >= TINYTREE_NSTART && (~beenThere & (1 << To)))
					stack[stackPos++] = To;
				if (Q >= TINYTREE_NSTART && (~beenThere & (1 << Q)))
					stack[stackPos++] = Q;

				// done, flag no endpoint assignment done
				beenThere |= (1 << curr);
				beenWhat[curr] = 0;

			} else if (beenWhat[curr] == 0) {

				/*
				 * now that operands are complete, assign them new placeholders
                                 */

				if (Q < TINYTREE_NSTART && (~beenThere & (1 << Q))) {
					beenThere |= (1 << Q);
					beenWhat[Q] = TINYTREE_KSTART + numPlaceholder;
					pSkin[numPlaceholder++] = (char) ('a' + Q - TINYTREE_KSTART);
				}

				if (To < TINYTREE_NSTART && (~beenThere & (1 << To))) {
					beenThere |= (1 << To);
					beenWhat[To] = TINYTREE_KSTART + numPlaceholder;
					pSkin[numPlaceholder++] = (char) ('a' + To - TINYTREE_KSTART);
				}

				if (F < TINYTREE_NSTART && (~beenThere & (1 << F))) {
					beenThere |= (1 << F);
					beenWhat[F] = TINYTREE_KSTART + numPlaceholder;
					pSkin[numPlaceholder++] = (char) ('a' + F - TINYTREE_KSTART);
				}

				/*
				 * @date 2020-03-29 14:20:38
				 *
				 * The generator might offer weird ordering like the raw tree: `"{ {1,~0,2}, {1,~2,0}, {10,11,0}, {11,10,12} }"`
				 * The top-level dives into `"[11]"` first, marking that as `"beenWhat[10]=11"`
				 * This needs the `compare()` to be fed with `beenWhat[Q,T,F]`
				 * That is why endpoints get assigned after being ordered(compare), some of the `beenWhat` can be uninitialised.
				 * If the two operands are newly assigned endpoints, they get ordered in sequence originally found (and don't swap).
				 *
				 * So: if both operands are references, use `beenWhat[]` for deep compare,
				 *     otherwise use plain `Q,T,F` safely as `compare()` falls back to a `ref/end` comparison
				 *
				 * The result has surprising effects like rejecting `"ab+ac+2&&"` in favour of `"ab+1ac+&&"`.
				 */

				if (To == 0 && Ti) {
					// swap `OR` if unordered
					if (this->compare(beenWhat[Q], *this, beenWhat[F]) > 0) {
						uint32_t savQ = Q;
						Q = F;
						F = savQ;
					}
				} else if (To == F) {
					// swap `XOR` if unordered
					if (this->compare(beenWhat[Q], *this, beenWhat[F]) > 0) {
						uint32_t savQ = Q;
						Q = F;
						F = savQ;
						To = savQ;
					}
				} else if (F == 0 && !Ti) {
					// swap `AND` if unordered
					if (this->compare(beenWhat[Q], *this, beenWhat[To]) > 0) {
						uint32_t savQ = Q;
						Q = To;
						To = savQ;
					}
				}


				/*
				 * populate new node
				 */

				tinyNode_t *pNewNode = this->N + this->count;
				pNewNode->Q = beenWhat[Q];
				pNewNode->T = beenWhat[To] ^ (Ti ? IBIT : 0);
				pNewNode->F = beenWhat[F];

				// flaq endpoints assigned
				beenWhat[curr] = this->count++;
			}

		} while (stackPos > 0);

		assert(numPlaceholder <= MAXSLOTS);
		pSkin[numPlaceholder] = 0;

		// set root
		this->root = beenWhat[root & ~IBIT] ^ (root & IBIT);
		assert(root == this->root);

		/*
		 * Pass-2, Create notation
		 */

		// push start on stack
		stack[stackPos++] = this->root & ~IBIT;

		// reset, but consider endpoints proper placeholders
		beenThere = (1 << 0);

		uint32_t nextNode = TINYTREE_NSTART;

		do {
			// pop stack
			uint32_t curr = stack[--stackPos];

			if (curr < TINYTREE_NSTART) {
				if (curr == 0) {
					// `zero`
					pName[nameLen++] = '0';
				} else {
					// assigned placeholder (not original endpoint)
					pName[nameLen++] = 'a' + curr - TINYTREE_KSTART;
				}

				continue;
			}

			const tinyNode_t *pNode = this->N + curr;
			const uint32_t Q = pNode->Q;
			const uint32_t To = pNode->T & ~IBIT;
			const uint32_t Ti = pNode->T & IBIT;
			const uint32_t F = pNode->F;

			// determine if node already handled
			if (~beenThere & (1 << curr)) {
				/// first time

				// push id so it visits again a second time
				stack[stackPos++] = curr;

				// push non-zero endpoints
				if (F >= TINYTREE_KSTART)
					stack[stackPos++] = F;
				if (To != F && To >= TINYTREE_KSTART)
					stack[stackPos++] = To;
				if (Q >= TINYTREE_KSTART)
					stack[stackPos++] = Q;

				// done, flag no endpoint assignment done
				beenThere |= (1 << curr);
				beenWhat[curr] = 0;

			} else if (beenWhat[curr] == 0) {
				// node complete, output operator

				if (Ti) {
					if (F == 0) {
						// GT Q?!T:0
						pName[nameLen++] = '>';
					} else if (To == 0) {
						// OR Q?!0:F
						pName[nameLen++] = '+';
					} else if (F == To) {
						// XOR Q?!F:F
						pName[nameLen++] = '^';
					} else {
						// QnTF Q?!T:F
						pName[nameLen++] = '!';
					}
				} else {
					if (F == 0) {
						// AND Q?T:0
						pName[nameLen++] = '&';
					} else if (To == 0) {
						// LT Q?0:F
						pName[nameLen++] = '<';
					} else if (F == To) {
						// SELF Q?F:F
						assert(!"Q?F:F");
					} else {
						// QTF Q?T:F
						pName[nameLen++] = '?';
					}
				}

				// flaq endpoints assigned
				beenWhat[curr] = nextNode++;
			} else {
				// back-reference to previous node

				uint32_t backref = nextNode - beenWhat[curr];
				assert(backref <= 9);
				pName[nameLen++] = '0' + backref;
			}

		} while (stackPos > 0);

		// append inverted-root
		if (this->root & IBIT)
			pName[nameLen++] = '~';

		assert(numPlaceholder <= MAXSLOTS);
		pSkin[numPlaceholder] = 0;
		assert(nameLen <= TINYTREE_NAMELEN);
		pName[nameLen] = 0;
	}

	/**
	 * @date 2020-03-18 22:17:26
	 *
	 * found level-1,2 normalised candidate.
	 *
	 * @param {object} cbObject - callback object
	 * @param {object} cbMember - callback member in object
	 * @param {number} numUnique - number of unique placeholders
	 */
	inline void callFoundTree(context_t *cbObject, generateTreeCallback_t cbMember, unsigned numUnique) {
		// test that tree is within limits
		assert(this->count >= TINYTREE_NSTART && this->count <= TINYTREE_NEND);

		// test if tree is within progress range
		// NOTE: first tree has `progress==0`
		if (windowLo && ctx.progress < windowLo) {
			ctx.progress++;
			return;
		}
		if (windowHi && ctx.progress >= windowHi) {
			ctx.progress++;
			return;
		}

		// snapshot
		uint32_t savCount = this->count;

		// re-order endpoints
		char name[TINYTREE_NAMELEN + 1];
		char skin[MAXSLOTS + 1];

		unpack(this->count - 1, name, skin);
		assert(savCount = this->count); // `count`` may not change

		// invoke the callback
		if (cbObject != NULL) {
			(*cbObject.*cbMember)(*this, name, numUnique);
			assert(savCount = this->count); // `count`` may not change
		}

		// bump counter after processing
		ctx.progress++;
	}

	/**
	 * @date 2020-03-17 20:24:13
	 *
	 * Generate all possible structures a tree of `n` nodes can have.
	 *
	 * It recursively pushes and pops nodes to the current tree until all available nodes and placeholders are exhausted.
	 *
	 * It also maintains a virtual stack, one that would be in sync with the stack found in `tinyTree_t::decode()` decoders.
	 * Basically, it works similar as `decode()` except the notation is created on the fly.
	 *
	 * Because the tree is built in the same order as `decode()`, it will always be natural path walking order
	 *
	 * The stack is implemented as a 64 bit word.
	 * Each pushed/popped value is max 5 bits in size, allowing for a stack-depth of 12
	 * This allows for easy shifting. "<<=5" for "push", ">>=5" for pops.
	 *
	 * The generator makes use of 6 state driven tables `push_QTF[]`, `push_PTF[]`, `push_QPF[]`, `push_QTP[]`, `push_PPF[]`, `push_PTP[]`, `push_QPP[]`
	 * You index them with the current number of placeholders and nodes already assigned.
	 * It will return the list of all possible nodes the current position could become.
	 * The list is used as templates to create nodes
	 *
	 * Table naming:
	 *  - Q,T,F are operands that are newly assigned placeholders.
	 *  - P should be replaced by popped stack values (pointing to other nodes)
	 *  - Templates only contain endpoints
	 *  - Stack only contains node'id, and entries are unique
	 *  - Use deep comparison during runtime to compare structures and not id's
	 *  - `endpointsLeft` are the number of open-ends a tree has.
	 *  - `XOR`'s have a hidden endpoints
	 *
	 * @param {number} endpointsLeft -  number of endpoints still to fill
	 * @param {number} numPlaceholder - number of placeholders already assigned
	 * @param {object} cbObject - callback object
	 * @param {object} cbMember - callback member in object
	 * @param {uint64_t} stack - `decode` stack
	 */
	void /*__attribute__((optimize("O0")))*/ generateTrees(unsigned endpointsLeft, unsigned numPlaceholder, uint64_t stack, context_t *cbObject, generateTreeCallback_t cbMember) {

		assert (numPlaceholder <= MAXSLOTS);
		assert(tinyTree_t::TINYTREE_MAXNODES <= 64 / PACKED_WIDTH);

		/*
		 * Test progress end-condition
		 */
		if (windowHi && ctx.progress >= windowHi)
			return; // hit end condition

		/*
		 * @date 2020-03-21 12:30:41
		 *
		 * Windowing and progress.
		 *
		 * With large datasets it is crucial to have generator restart capabilities.
		 * Restarting introduces the concept of windowing making it possible to break larger invocations
		 * into a collection of smaller jobs which can be run in parallel.
		 *
		 * Speeding progression of this call can be done by suppressing ignoring code execution and
		 * increasing `progress` in such a was that it seems like normal operation.
		 *
		 * Recursion on its 3nd level (when trees are 2 nodes in size) is a good moment to check conditions
		 * This makes that restarting sensible for trees >= 4 nodes
		 */
		if (this->count == TINYTREE_NSTART + 2) {
			if (this->pRestartData) {
				/*
				 * assert that restart data is in sync with reality
				 */
				if (ctx.progress != *this->pRestartData) {
					ctx.fatal("restartData out of sync. Encountered:%ld, Expected:%ld", ctx.progress, *this->pRestartData);
					assert(ctx.progress == *this->pRestartData);
				}

				// jump to next restart point
				this->pRestartData++;

				/*
				 * Test if any of this level overlaps the window
				 */
				if (windowLo >= *this->pRestartData) {
					// jump to next restart point
					ctx.progress = *this->pRestartData;
					return;
				} else {
					// passed restart point. Status on new line
					this->restartTick++;
				}

			} else if (cbObject == NULL) {
				/*
				 * Generate restart data
				 */
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					ctx.tick = 0;
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s)",
						        ctx.timeAsString(), ctx.progress, perSecond);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d",
						        ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS);
					}
				}

				// tree is incomplete and requires a slightly different notation
				printf("%12ldLL/*", ctx.progress);
				for (uint32_t iNode = tinyTree_t::TINYTREE_NSTART; iNode < this->count; iNode++) {
					const tinyNode_t *pNode = &this->N[iNode];
					uint32_t Q = pNode->Q;
					uint32_t To = pNode->T & ~IBIT;
					uint32_t Ti = pNode->T & IBIT;
					uint32_t F = pNode->F;

					if (Q >= tinyTree_t::TINYTREE_NSTART)
						putchar("123456789"[Q - tinyTree_t::TINYTREE_NSTART]);
					else
						putchar("0abcdefghi"[Q]);
					if (To >= tinyTree_t::TINYTREE_NSTART)
						putchar("123456789"[To - tinyTree_t::TINYTREE_NSTART]);
					else
						putchar("0abcdefghi"[To]);
					if (F >= tinyTree_t::TINYTREE_NSTART)
						putchar("123456789"[F - tinyTree_t::TINYTREE_NSTART]);
					else
						putchar("0abcdefghi"[F]);
					putchar(Ti ? '!' : '?');
				}
				printf("*/,");

				// `genprogress` needs to know how many restart points are generated.
				numFoundRestart++;

				if (numFoundRestart % 8 == 1)
					printf("\n");

			}
		}

		/*
		 * Nodes with three endpoints
		 */

		if (endpointsLeft >= 3) {
			/*
			 * Because there is no substitution, the templates are normalised and ordered by nature
			 */

			// point to start of state table index by number of already assigned placeholders and nodes
			const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_QTF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

			while (*pData) {
				// NOTE: template endpoints are always ordered

				uint32_t R = this->push(*pData & 0xffff); // unpack and push operands
				if (R) {
					unsigned numUnique = (*pData >> PACKED_SIZE);
					if (endpointsLeft == 3 && stack == 0)
						this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
					else
						this->generateTrees(endpointsLeft - 3, numUnique, stack << PACKED_WIDTH | R, cbObject, cbMember);
					this->pop();
				}

				pData++;
			}
		}

		/*
		 * POP value from stack.
		 * Nodes with two endpoints and one reference.
		 */

		if (stack == 0)
			return; // stack exhausted
		uint32_t pop0 = (uint32_t) (stack & PACKED_MASK);
		stack >>= PACKED_WIDTH;

		if (endpointsLeft >= 2) {
			/*
			 * `<pop>` `T` `F`
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = pop0 << PACKED_QPOS;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_PTF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					uint32_t merged = (*pData & 0xffff) | qtf;

					// keep tree ordered and drop `XOR`
					if (pIsType[merged] & (PACKED_UNORDERED | PACKED_XOR)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push(merged); // merge Q, unpack and push operands
					if (R) {
						unsigned numUnique = (*pData >> PACKED_SIZE);
						if (endpointsLeft == 2 && stack == 0)
							this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 2, numUnique, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `Q` `<pop>` `F`
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = pop0 << PACKED_TPOS;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_QPF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					uint32_t merged = (*pData & 0xffff) | qtf;

					// keep tree ordered and drop `XOR`
					if (pIsType[merged] & (PACKED_UNORDERED | PACKED_XOR)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push(merged); // merge T, unpack and push operands
					if (R) {
						unsigned numUnique = (*pData >> PACKED_SIZE);
						if (endpointsLeft == 2 && stack == 0)
							this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 2, numUnique, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `Q` `T` `<pop>`
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = pop0 << PACKED_FPOS;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_QTP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					uint32_t merged = (*pData & 0xffff) | qtf;

					// keep tree ordered and drop `XOR`
					if (pIsType[merged] & (PACKED_UNORDERED | PACKED_XOR)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push(merged); // merge F, unpack and push operands
					if (R) {
						unsigned numUnique = (*pData >> PACKED_SIZE);
						if (endpointsLeft == 2 && stack == 0)
							this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 2, numUnique, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 *  `Q` XOR `<pop>`
			 *
			 * NOTE: Node has invisible endpoint
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop0 << PACKED_TPOS) | (pop0 << PACKED_FPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_QPP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					uint32_t merged = (*pData & 0xffff) | qtf;

					if (~*pData & PACKED_TIBIT) {
						pData++;
						continue; // only `QnTF` allowed. stop when exhausted
					}

					// keep tree ordered
					if (pIsType[merged] & PACKED_UNORDERED) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push(merged); // merge Q+F, unpack and push operands
					if (R) {
						unsigned numUnique = (*pData >> PACKED_SIZE);
						if (endpointsLeft == 2 && stack == 0) {
							this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
						} else {
							this->generateTrees(endpointsLeft - 2, numUnique, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						}
						this->pop();
					}

					pData++;
				}
			}
		}

		/*
		 * POP second value from stack.
		 * Nodes with one endpoints and two reference.
		 */

		if (stack == 0)
			return; // stack exhausted
		uint32_t pop1 = (uint32_t) (stack & PACKED_MASK);
		stack >>= PACKED_WIDTH;

		if (endpointsLeft >= 1) {
			/*
			 * `<pop>` `<pop>` `F`
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop1 << PACKED_QPOS) | (pop0 << PACKED_TPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_PPF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					uint32_t merged = (*pData & 0xffff) | qtf;

					// keep tree ordered and drop `XOR`
					if (pIsType[merged] & (PACKED_UNORDERED | PACKED_XOR)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push(merged); // merge Q+T, unpack and push operands
					if (R) {
						unsigned numUnique = (*pData >> PACKED_SIZE);
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, numUnique, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `<pop>` `T` `<pop>`
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop1 << PACKED_QPOS) | (pop0 << PACKED_FPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_PTP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					uint32_t merged = (*pData & 0xffff) | qtf;

					// keep tree ordered and drop `XOR`
					if (pIsType[merged] & (PACKED_UNORDERED | PACKED_XOR)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push(merged); // merge Q+F, unpack and push operands
					if (R) {
						unsigned numUnique = (*pData >> PACKED_SIZE);
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, numUnique, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `Q` `<pop>` `<pop>`
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop1 << PACKED_TPOS) | (pop0 << PACKED_FPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pTemplateData + templateIndex[TEMPLATE_QPP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					uint32_t merged = (*pData & 0xffff) | qtf;

					// keep tree ordered and drop `XOR`
					if (pIsType[merged] & (PACKED_UNORDERED | PACKED_XOR)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push(merged); // merge Q+F, unpack and push operands
					if (R) {
						unsigned numUnique = (*pData >> PACKED_SIZE);
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(cbObject, cbMember, numUnique); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, numUnique, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 *  `<pop>` XOR `<pop>`
			 *
			 * NOTE: Node has invisible endpoint
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = PACKED_TIBIT | (pop1 << PACKED_QPOS) | (pop0 << PACKED_TPOS) | (pop0 << PACKED_FPOS);

				// keep tree ordered

				if (~pIsType[qtf] & PACKED_UNORDERED) {
					uint32_t R = this->push(qtf); // merge Q+F, unpack and push operands
					if (R) {
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(cbObject, cbMember, numPlaceholder); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, numPlaceholder, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
						this->pop();
					}
				}
			}
		}

		/*
		 * POP third value from stack.
		 * Nodes with no endpoints and three reference.
		 */

		if (stack == 0)
			return; // stack exhausted
		uint32_t pop2 = (uint32_t) (stack & PACKED_MASK);
		stack >>= PACKED_WIDTH;

		if (endpointsLeft >= 0) {
			// runtime values to merge into template
			uint32_t qtf = (pop2 << PACKED_QPOS) | (pop1 << PACKED_TPOS) | (pop0 << PACKED_FPOS);

			{
				// QnTF
				uint32_t R = this->push(PACKED_TIBIT | qtf); // push with inverted T
				if (R) {
					if (endpointsLeft == 0 && stack == 0)
						this->callFoundTree(cbObject, cbMember, numPlaceholder); // All placeholders used and stack unwound
					else
						this->generateTrees(endpointsLeft, numPlaceholder, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
					this->pop();
				}
			}

			if (~this->flags & context_t::MAGICMASK_QNTF) {
				// QTF
				uint32_t R = this->push(qtf); // push without inverted T
				if (R) {
					if (endpointsLeft == 0 && stack == 0)
						this->callFoundTree(cbObject, cbMember, numPlaceholder);
					else
						this->generateTrees(endpointsLeft, numPlaceholder, (stack << PACKED_WIDTH) | R, cbObject, cbMember);
					this->pop();
				}
			}
		}
	}

};

#endif