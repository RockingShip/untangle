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
 * Building pure datasets is two-pass:
 *  - First pass is build signature base.
 *  - Second pass is to optimize signature names to `QnTF`-only (pure) notation.
 *  - Pure databases should be considered `10n9` with full completeness up to `5n9`.
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
 *
 * @date 2020-04-10 19:43:42
 *
 * Dyadics can have pre-calculated ordering, EXCEPT if both operands are references.
 * When both operands are references, run-time `compare()` is required.
 * `foundTree()` can be called with non-unique trees because templating might create duplicates.
 * 
 * @date 2020-04-11 17:56:13
 * 
 * New approach based on new insights.
 * The stack contains unique nodes.
 * Values are always ordered from low to high, top of stack being highest.
 * This makes it possible to implement stack as a bitmap.
 * Also, last action of `generateTrees()` section is to push the newly created node id on the stack.
 * This implies that the top-of-stack equals `numNode`.
 * Keep all dyadic ordering simple, let run-time perform `compare()`.
 * Don't reorder `packedN[]` because that is used for backReference detection.
 *
 * NOTE: due to new packed layout, `TINYTREE_MAXNODE` is now set to max 8.
 * However, 8 nodes requires 4930223 template entries. 7 nodes has 2181293.
 * Don't be wasteful, 7n9 spans a massively large space.
 * 
 * @date 2021-09-20 19:05:41
 * 
 * The generator is sensitive to context_t::MAGICMASK_PURE and context_t::MAGICMASK_CASCADE
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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "tinytree.h"

/*
 * @date 2020-04-15 13:27:57
 *
 * Placeholder base type for generator callbacks
 */
struct callable_t {

};

/**
 * @date 2020-03-17 20:22:08
 *
 * `generatorTree_t` extends `tinyTree_t` by giving it tree creation capabilities
 */
struct generator_t {

	/*
	 * @date 2020-03-19 16:12:52
	 *
	 * Packed Notation to make operator fit into 16 bits
	 *   packedQTF =  <invertedT> << (WIDTH*3) | Q << (WIDTH*2) | T << (WIDTH*1) | F << (WIDTH*0)
	 *
	 * This order is chosen as it is the order found on stacks during `decode()`
	 *
	 * @date 2020-04-11 20:38:39
	 *
	 * Not using packed fields because of alignment and endianness
	 */
	enum {
		// +-------+----------+-------------+----+----+----+----+
		// | stack | endpoint | placeholder | Ti | Q  | Tu |  F |
		// +-------+----------+-------------+----+----+----+----+
		// <---8--><----4----><------4-----><-1-><-5-><-5-><-5->
		// <---------------16--------------><--------16-------->

		/// @constant {number} - Q/T/F Field width
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
		PACKED_TIMASK = 1 << PACKED_TIPOS,

		/// @constant {number} - Size of packed `qtf` word in bits
		PACKED_SIZE = 16,

		/// @constant {number} - position/width of `numPlaceholder`
		PACKED_POS_PLACEHOLDER   = 16,
		PACKED_WIDTH_PLACEHOLDER = 4,

		/// @constant {number} - position/width of `numWidth`
		PACKED_POS_ENDPOINT   = 20,
		PACKED_WIDTH_ENDPOINT = 4,

		/// @constant {number} - position/width of `stack`
		PACKED_POS_STACK   = 24,
		PACKED_WIDTH_STACK = 12,

		/// @constant {number} - used for `pIsType[]` to indicate type of node
		PACKED_OR      = 0x01,
		PACKED_GT      = 0x02,
		PACKED_NE      = 0x04,
		PACKED_QnTF    = 0x08,
		PACKED_AND     = 0x10,
		PACKED_QTF     = 0x20,
		PACKED_COMPARE = 0x40, // needs run-time compare

		/// @constant {number} - size of `pTemplateData[]`
		TEMPLATE_MAXDATA      = 5321417,
		TEMPLATE_MAXDATA_PURE = 2855957,

		/*
		 * @date 2021-09-20 18:54:29
		 * Maximum tree size created by generator.
		 * Forked from TINYTREE_MAXNODES as that can now be too large to handle because of cascading
		 */

		/// @constant {number} - convenience
		GENERATOR_KSTART   = tinyTree_t::TINYTREE_KSTART,
		GENERATOR_NSTART   = tinyTree_t::TINYTREE_NSTART,
		GENERATOR_MAXNODES = 7,
		GENERATOR_NEND     = GENERATOR_NSTART + GENERATOR_MAXNODES,
	};

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {number[]} lookup table for `push()` index by packed `QTF`
	uint32_t *pCacheQTF;

	/// @var {number[]} versioned memory for `pCacheQTF`
	uint32_t *pCacheVersion;

	/// @var {number} current version incarnation
	uint32_t iVersion;

	/// @var {uint32_t[]} array of packed unified operators
	uint32_t packedN[GENERATOR_NEND];

	/// @var {uint8_t[]} array indexed by packed `QTnF` to indicate what type of operator
	uint8_t *pIsType;

	/// @var {uint8_t[]} Value of top-of-stack
	uint8_t *pTOS;

	/// @var {uint64_t} lower bound `progress` (if non-zero)
	uint64_t windowLo;

	/// @var {uint64_t} upper bound (not including) `progress` (if non-zero)
	uint64_t windowHi;

	/// @var {uint64_t[]} restart data
	const uint64_t *pRestartData;

	/// @var {number} Node depth at which to handle restart tabs.
	unsigned restartTabDepth;

	/// @var {number[]} template data for generator
	uint32_t *pTemplateData;

	/// @var {number[7][10][10]} starting offset in `templateData[]`. `templateIndex[SECTION][numNode][numPlaceholder]`
	uint32_t templateIndex[1 << GENERATOR_MAXNODES][MAXSLOTS + 1][4];

	/// @var {tintTree_t} Tree under construction`
	tinyTree_t buildTree;

	/// @var {tintTree_t} Tree needed to re-order endpoints before calling `foundTree()`
	tinyTree_t foundTree;


	/**
	 * @date 2020-03-18 18:45:33
	 *
	 * Constructor
	 *
	 * @param {context_t} ctx - I/O context
	 */
	generator_t(context_t &ctx) : ctx(ctx), buildTree(ctx), foundTree(ctx) {
		// Assert that the highest available node fits into a 5 bit value. `2^5` = 32. Last 3 are reserved for template wildcards
		assert(GENERATOR_NEND < 32 - 3);

		windowLo = 0;
		windowHi = 0;
		pRestartData = NULL;
		ctx.tick = 0;
		restartTabDepth = GENERATOR_NSTART + 2; // for `7n9` +3 is a better choice. But `7n9-pure` still has 70177 restart tabs.

		::memset(templateIndex, 0, sizeof(templateIndex));

		// allocate structures
		pIsType = (uint8_t *) ctx.myAlloc("generatorTree_t::pIsType", 1 << PACKED_SIZE, sizeof(*this->pIsType));
		pTOS = (uint8_t *) ctx.myAlloc("generatorTree_t::pTOS", 1 << GENERATOR_MAXNODES, sizeof(*this->pTOS));
		pCacheQTF = (uint32_t *) ctx.myAlloc("generatorTree_t::pCacheQTF", 1 << PACKED_SIZE, sizeof(*this->pCacheQTF));
		pCacheVersion = (uint32_t *) ctx.myAlloc("generatorTree_t::pCacheVersion", 1 << PACKED_SIZE, sizeof(*this->pCacheVersion));
		pTemplateData = (uint32_t *) ctx.myAlloc("generatorTree_t::pTemplateData", TEMPLATE_MAXDATA, sizeof(*pTemplateData));

		// clear versioned memory
		iVersion = 0;
		clearGenerator();

		// call to make compiler actually generator code
		decodePacked(0);
	}

	/**
	 * @date 2020-03-18 19:30:09
	 *
	 * Release system resources
	 */
	~generator_t() {
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
			::memset(pCacheVersion, 0, (1 << PACKED_SIZE) * sizeof(*pCacheVersion));
		}
		iVersion++; // when overflows, next call will clear

		buildTree.clearTree();
	}

	/**
	 * @date 2020-04-11 21:48:14
	 *
	 * Breakdown packed record for debugging aid
	 */
	static const char *decodePacked(unsigned packed) {
		static char name[128];
		sprintf(name, "newStack=%x newEndpoint=%u newPlaceholder=%u Ti=%u Q=%u Tu=%u F=%u",
		        (packed >> PACKED_POS_STACK) & ((1 << PACKED_WIDTH_STACK) - 1),
		        (packed >> PACKED_POS_ENDPOINT) & ((1 << PACKED_WIDTH_ENDPOINT) - 1),
		        (packed >> PACKED_POS_PLACEHOLDER) & ((1 << PACKED_WIDTH_PLACEHOLDER) - 1),
		        (packed >> PACKED_TIPOS) & 1,
			(unsigned) ((packed >> PACKED_QPOS) & PACKED_MASK),
			(unsigned) ((packed >> PACKED_TPOS) & PACKED_MASK),
			(unsigned) ((packed >> PACKED_FPOS) & PACKED_MASK));
		return name;
	}

	/**
	 * @date 2020-03-18 21:05:34
	 *
	 * Initialise lookup tables for generator
	 *
	 * @param {number} pure - zero for any operator, non-zero for `QnTF` only operator
	 */
	void initialiseGenerator(void) {
		/*
		 * Create lookup table indexed by packed notation to determine if `Q,T,F` combo is normalised
		 * Exclude ordered dyadics.
		 */

		/*
		 * filter nodes baded on `--pure` settings.
		 * NOTE: `--cascade` is handled in `tinyTree_t::compare()`.
		 */
		unsigned pure = ctx.flags & context_t::MAGICMASK_PURE;
		
		// @formatter:off
		for (unsigned Ti = 0; Ti < 2; Ti++)
		for (unsigned F = 0; F < (1 << PACKED_WIDTH); F++)
		for (unsigned Tu = 0; Tu < (1 << PACKED_WIDTH); Tu++)
		for (unsigned Q = 0; Q < (1 << PACKED_WIDTH); Q++) {
		// @formatter:on

			// create packed notation
			unsigned ix = (Ti ? 1 << 15 : 0 << 15) | Q << 10 | Tu << 5 | F << 0;

			// default, not normalised
			pIsType[ix] = 0;

			// test if combo is normalised
			if (Q == Tu)
				continue;  // Q?Q:F or Q?~Q:F
			if (Q == F)
				continue; // Q?T:Q or Q?~T:Q
			if (Q == 0)
				continue; // 0?X:Y
			if (Tu == F && F == 0)
				continue; // Q?~0:0
			if (Tu == F && !Ti)
				continue; // "SELF" Q?F:F
			if (Tu == 0 && !Ti)
				continue; // "LT" Q?0:F -> F?~Q:0

			// passed, normalised

			/*
			 * Reminder:
			 *  [ 2] a ? ~0 : b                  "+" OR
			 *  [ 6] a ? ~b : 0                  ">" GT
			 *  [ 8] a ? ~b : b                  "^" NE
			 *  [ 9] a ? ~b : c                  "!" QnTF
			 *  [16] a ?  b : 0                  "&" AND
			 *  [19] a ?  b : c                  "?" QTF
			 */

			if (Ti) {
				if (Tu == 0)
					pIsType[ix] |= PACKED_OR;
				else if (F == 0)
					pIsType[ix] |= PACKED_GT;
				else if (F == Tu)
					pIsType[ix] |= PACKED_NE;
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
			 * Runtime only needs to call `compare()` if both operands are references
			 */
			if (pIsType[ix] & PACKED_OR) {
				if (Q >= GENERATOR_NSTART && F >= GENERATOR_NSTART)
					pIsType[ix] |= PACKED_COMPARE;
			}
			if (pIsType[ix] & PACKED_NE) {
				if (Q >= GENERATOR_NSTART && F >= GENERATOR_NSTART)
					pIsType[ix] |= PACKED_COMPARE;
			}
			if (pIsType[ix] & PACKED_AND) {
				if (Q >= GENERATOR_NSTART && Tu >= GENERATOR_NSTART)
					pIsType[ix] |= PACKED_COMPARE;
			}
		}

		/*
		 * @date 2020-04-11 18:29:09
		 *
		 * top-of-stack lookup.
		 *
		 * NOTE: `stack` is a bitmap of nodeID's.
		 *       However, due to packing the template data chops off the unused first `NSTART` bits.
		 *       The index to `pTOS` is the packed field.
		 */
		for (unsigned iStack = 0; iStack < (1 << GENERATOR_MAXNODES); iStack++) {
			pTOS[iStack] = 0;
			for (int j = GENERATOR_MAXNODES - 1; j >= 0; j--) {
				if (iStack & (1 << j)) {
					pTOS[iStack] = GENERATOR_NSTART + j;
					break;
				}
			}
		}

		/*
		 * @date 2020-03-18 10:52:57
		 *
		 * Wildcard values represent node-references that are popped from the stack during runtime.
		 * Zero means no wildcard, otherwise it must be a value greater than NSTART
		 */

		unsigned numTemplateData = 1; // skip initial zero

		/*
		 * @date 2020-04-11 22:09:22
		 *
		 * NOTE: `stack` is a bitmap of nodeID's.
		 *       However, due to packing the template data chops off the unused first `NSTART` bits.
		 *       
		 * @date 2021-09-06 10:48:22
		 * 
		 * With ordered cascading, nodes many get orphaned.
		 * `TINYTREE_MAXNODES` has been nearly doubled to accommodate the orphans
		 * The generator is orphan free, limit maxnodes or `PACKED_WIDTH_STACK` will overflow 
		 * 
		 * @date 2021-09-11 00:17:15
		 * Cascades 
		 * generator is hardcoded to `TINYTREE_MAXNODES=6` to lower `progressHi` and `TEMPLATE_MAXDATA`.
		 * assuming `--pure --cascade` that all extended structures are within `6n9-pure` space
		 * There should be an assert somewhere guarding this
		 */

		// @formatter:off
		for (unsigned iStack = 0; iStack < (1U << GENERATOR_MAXNODES); iStack++)
		for (unsigned numPlaceholder=0; numPlaceholder < (MAXSLOTS + 1); numPlaceholder++)
		for (unsigned numEndpointLeft=0; numEndpointLeft < 4; numEndpointLeft++) { // no more than 3 endpoints per node
		// @formatter:on

			/*
			 * Get number of nodes already allocated.
			 * Last created nodeId is in top-of-stack.
			 *
			 * @date 2021-08-29 19:19:47
			 * Get the current node ID
			 */
			unsigned nodeId = (pTOS[iStack]) ? pTOS[iStack] + 1 : GENERATOR_NSTART;

			/*
			 * Iterate through all possible `Q,T,F` possibilities
			 */

			templateIndex[iStack][numPlaceholder][numEndpointLeft] = numTemplateData;

			// @formatter:off
			for (int Ti = 1; Ti >= 0; Ti--)
			for (unsigned Q = 0; Q < nodeId; Q++)
			for (unsigned Tu = 0; Tu < nodeId; Tu++)
			for (unsigned F = 0; F < nodeId; F++) {
			// @formatter:on

				if (!Ti && pure) {
					// reject `non-pure` template with `--pure` invocation
					continue;
				}

				// NOTE: endpoints include zero. Placeholders are never zero.
				unsigned newPlaceholder = numPlaceholder; // new `numPlaceholder`
				unsigned newEndpoint = 0; // decrement!! of `numEndpoint`
				unsigned newStack = iStack << GENERATOR_NSTART; // new `stack`

				/*
				 * create packed notation
				 */
				unsigned qtf = (Ti << PACKED_TIPOS) | (Q << PACKED_QPOS) | (Tu << PACKED_TPOS) | (F << PACKED_FPOS);

				if (pIsType[qtf] == 0)
					continue; // must be normalised

				/*
				 * First components if F,T,Q order because the node with highest id is TOS
				 */

				if (pTOS[newStack >> GENERATOR_NSTART] && F == pTOS[newStack >> GENERATOR_NSTART]) {
					// pop stack entry
					newStack &= ~(1 << F);
				} else if (F >= GENERATOR_NSTART) {
					// back-reference counts as endpoint
					newEndpoint++;
				}

				if (pTOS[newStack >> GENERATOR_NSTART] && Tu == pTOS[newStack >> GENERATOR_NSTART]) {
					// pop stack entry
					newStack &= ~(1 << Tu);
				} else if (Tu >= GENERATOR_NSTART) {
					// back-reference counts as endpoint
					newEndpoint++;
				}

				if (pTOS[newStack >> GENERATOR_NSTART] && Q == pTOS[newStack >> GENERATOR_NSTART]) {
					// pop stack entry
					newStack &= ~(1 << Q);
				} else if (Q >= GENERATOR_NSTART) {
					// back-reference counts as endpoint
					newEndpoint++;
				}

				/*
				 * Then endpoints in Q,T,F order because that is walking order
				 */

				if (Q >= GENERATOR_NSTART) {
					// component
				} else if (Q > GENERATOR_KSTART + newPlaceholder && Q < GENERATOR_NSTART) {
					// placeholder not created yet
					continue;
				} else if (Q == GENERATOR_KSTART + newPlaceholder) {
					// bump placeholder if using for the first time
					newPlaceholder++;
					newEndpoint++;
					if (newPlaceholder > MAXSLOTS)
						continue; // skip if exceeds maximum
				} else {
					// regular endpoint
					newEndpoint++;
				}

				if (Tu >= GENERATOR_NSTART) {
					// component
				} else if (Tu > GENERATOR_KSTART + newPlaceholder && Tu < GENERATOR_NSTART) {
					// placeholder not created yet
					continue;
				} else if (Tu == GENERATOR_KSTART + newPlaceholder) {
					// bump placeholder if using for the first time
					newPlaceholder++;
					newEndpoint++;
					if (newPlaceholder > MAXSLOTS)
						continue; // skip if exceeds maximum
				} else {
					// regular endpoint
					newEndpoint++;
				}

				if (F >= GENERATOR_NSTART) {
					// component
				} else if (F > GENERATOR_KSTART + newPlaceholder && F < GENERATOR_NSTART) {
					// placeholder not created yet
					continue;
				} else if (F == GENERATOR_KSTART + newPlaceholder) {
					// bump placeholder if using for the first time
					newPlaceholder++;
					newEndpoint++;
					if (newPlaceholder > MAXSLOTS)
						continue; // skip if exceeds maximum
				} else {
					// regular endpoint
					newEndpoint++;
				}

				/*
				 * Reject if less endpoints were available then required
				 */
				if (newEndpoint > numEndpointLeft)
					continue;

				/*
				 * Push new node-id on stack and prepare for packed format
				 */
				newStack |= 1 << nodeId;
				newStack >>= GENERATOR_NSTART;

				assert(!(Q & ~PACKED_MASK));
				assert(!(Tu & ~PACKED_MASK));
				assert(!(newPlaceholder & ~((1 << PACKED_WIDTH_PLACEHOLDER) - 1)));
				assert(!(newEndpoint & ~((1 << PACKED_WIDTH_ENDPOINT) - 1)));
				assert(!(newStack & ~((1 << PACKED_WIDTH_STACK) - 1)));

				/*
				 * Store
				 */

				// add template
				pTemplateData[numTemplateData] = (Ti << PACKED_TIPOS) | Q << PACKED_QPOS | Tu << PACKED_TPOS | F << PACKED_FPOS;
				pTemplateData[numTemplateData] |= newPlaceholder << PACKED_POS_PLACEHOLDER;
				pTemplateData[numTemplateData] |= newEndpoint << PACKED_POS_ENDPOINT;
				pTemplateData[numTemplateData] |= newStack << PACKED_POS_STACK;
				numTemplateData++;
			}

			// end of section
			pTemplateData[numTemplateData++] = 0;
		}

		if (pure) {
			if (numTemplateData != TEMPLATE_MAXDATA_PURE)
				fprintf(stderr, "numTemplateDataPure=%u\n", numTemplateData);
			assert(numTemplateData <= TEMPLATE_MAXDATA_PURE);
		} else {
			if (numTemplateData != TEMPLATE_MAXDATA)
				fprintf(stderr, "numTemplateData=%u\n", numTemplateData);
			assert(numTemplateData <= TEMPLATE_MAXDATA);
		}
	}

	/**
	 * @date 2020-03-18 18:15:57
	 *
	 * Push/add packed node to tree
	 *
	 * @param {number} qtf - packed notation of `QTF`
	 * @return {number} 0 if not normalised, or node id of already existing or newly created one
	 */
	inline unsigned push(unsigned qtf) {
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
		unsigned nid = buildTree.count;
		assert(nid < GENERATOR_NEND); // overflow
		assert((nid & ~PACKED_MASK) == 0); // may not overflow packed field

		// add to packed nodes
		this->packedN[nid] = qtf;

		// populate node
		tinyNode_t *pNode = buildTree.N + nid;

		pNode->Q = (qtf >> PACKED_QPOS) & PACKED_MASK;
		pNode->T = (qtf >> PACKED_TPOS) & PACKED_MASK;
		pNode->F = (qtf >> PACKED_FPOS) & PACKED_MASK;
		if (qtf & PACKED_TIMASK)
			pNode->T ^= IBIT;

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
		 * `reconstruct()` re-orders on the fly before a `compare()` is called.
		 *
		 * Drop this code and leave for historics.
		 *
		 * @date 2020-04-09 23:06:54
		 *
		 * Re-enabled cause `compare()` redesigned and `push()` should reject un-ordered instead of rewriting
		 * Also, fixating before `return` polluted the cache.
		 *
		 * @date 2020-04-11 23:38:44
		 *
		 * Ordering changed again. Do not reject but swap when copying to `N[]`
		 *
		 * @date 2021-07-25 13:37:27
		 *
		 * Reject instead of swap.
		 * Don't forget, with the structure based compare, node id's have become invisible.
		 * For templates/constructor testing nodeID is possible because the generator creates all alternates,
		 *   one of them is correct, is it isn't this one then just ignore it
		 * For runtime/detector, testing nodeID is invisible because of the structure based compare.
		 *   If alternatives ar eneeded, then most likely its the top-level and let higher levels be smart and swap where needed
		 * Also, `rewriteData[]` is based on similarity/equality instead of compare, so that is immune.
		 *   `patterns` (untngle v1) *might* be influenced, however the structure based compare was introduced in untangle v2.
		 *
		 * @date 2021-08-29 16:58:09
		 * With the renewed cascade,only left-hand-side may cascade
		 * 
		 * @date 2021-09-22 23:50:28
		 * Generator needs to be generic, do not test cascading as this is unfit for optimisation
		 */
		if (pIsType[qtf] & (PACKED_OR|PACKED_NE|PACKED_AND)) {
		}

		// fixate
		// add to cache of fast duplicate lookups
		pCacheQTF[qtf] = buildTree.count++;
		pCacheVersion[qtf] = iVersion;

		return nid;
	}

	/**
	 * @date 2020-03-18 18:00:10
	 *
	 * Unwind pushed nodes from tree, releasing nodes that were created
	 */
	inline void pop(void) {
		// pop node
		unsigned qtf = this->packedN[--buildTree.count];

		// erase index
		pCacheQTF[qtf] = 0;
	}

	/**
	 * @date 2020-03-24 16:01:58
	 *
	 * Typedef of callback function to `"void foundTree(generatorTree_t &tree, unsigned numUnique)"`
	 *
	 * @typedef {callback} generateTreeCallback_t
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	typedef bool(callable_t::* generateTreeCallback_t)(tinyTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef);

	/**
	 * @date 2020-03-18 22:17:26
	 *
	 * found level-1,2 normalised candidate.
	 *
	 *  - Walk through the tree
	 *  - Reject if endpoints not naturally ordered, templates might generate things like "de+ab+c>+".
	 *  - Construct name
	 *  - Collect stats
	 *
	 * @date 2021-08-29 10:43:32
	 *
	 * With the renewed structure base compare and naming algorithm it is no longer possible to test/reject naturally ordered endpoints.
	 * Also, with renewed cascading it is also possible that structure will collapse/fold
	 *
	 * The generator builds trees in that is different than the walking path
	 * Copy/reassign endpoints to `foundTree`.
	 * This has the extra advantage that the tree may be modified
	 *
	 * @param {object} cbObject - callback object
	 * @param {object} cbMember - callback member in object
	 */
	void callFoundTree(callable_t *cbObject, generateTreeCallback_t cbMember) {

		// test if `genrestartdata` mode
		if (ctx.opt_debug & ctx.DEBUGMASK_GENERATOR_TABS)
			return;

		// test that tree is within limits
		assert(buildTree.count >= GENERATOR_NSTART && buildTree.count <= GENERATOR_NEND);

		// test if tree is within progress range
		// NOTE: first tree has `progress==0`
		if (windowLo && ctx.progress < windowLo)
			return;
		if (windowHi && ctx.progress >= windowHi)
			return;

		// stack
		uint32_t stack[tinyTree_t::TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int stackPos = 0;

		// name
		char name[tinyTree_t::TINYTREE_NAMELEN + 1];
		unsigned nameLen = 0;

		// counters
		unsigned nextNode = GENERATOR_NSTART;
		unsigned nextPlaceholder = GENERATOR_KSTART;
		unsigned numEndpoint = 0;
		unsigned numBackRef = 0;

		// nodes already processed
		uint32_t beenThere = (1 << 0); // don't let zero count as endpoint
		uint32_t beenWhat[GENERATOR_NEND];
		beenWhat[0] = 0;

		// push start on stack
		stack[stackPos++] = buildTree.count - 1;

		foundTree.clearTree();

		do {
			// pop stack
			unsigned curr = stack[--stackPos];

			if (curr < GENERATOR_NSTART) {
				/*
				 * Endpoint
				 */
				assert(curr != 0);

				if (!(beenThere & (1 << curr))) {
					beenThere |= (1 << curr);
					beenWhat[curr] = nextPlaceholder++;
				}
				name[nameLen++] = (char) ('a' + beenWhat[curr] - GENERATOR_KSTART);
				numEndpoint++;

			} else {
				/*
				 * Reference
				 */

				const tinyNode_t *pNode = buildTree.N + curr;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				// determine if node already handled
				if (!(beenThere & (1 << curr))) {
					// first time

					/*
					 * @date 2021-08-30 00:18:42
					 *
					 * below is the tree-walking sequence
					 * the signature representative is the structure
					 * that would be the lowest of all `compare(a,b) < 0`.
					 * The root node is the first encountered
					 * Q should be or refer to the first and lowest, endpoint and placeholder.
					 */
					// push id so it visits again a second time for the operator
					stack[stackPos++] = curr;

					// push non-zero endpoints
					if (F >= GENERATOR_KSTART)
						stack[stackPos++] = F;
					if (Tu != F && Tu >= GENERATOR_KSTART)
						stack[stackPos++] = Tu;
					if (Q >= GENERATOR_KSTART)
						stack[stackPos++] = Q;

					// done, flag first-time done
					beenThere |= (1 << curr);
					beenWhat[curr] = 0;

				} else if (beenWhat[curr] == 0) {
					// node complete, output operator
					tinyNode_t *pFoundNode = foundTree.N + foundTree.count++;

					if (Ti) {
						if (F == 0) {
							// GT Q?!T:0
							name[nameLen++] = '>';
							pFoundNode->Q = beenWhat[Q];
							pFoundNode->T = beenWhat[Tu] ^ IBIT;
							pFoundNode->F = 0;
						} else if (Tu == 0) {
							// OR Q?!0:F
							name[nameLen++] = '+';
							pFoundNode->Q = beenWhat[Q];
							pFoundNode->T = IBIT;
							pFoundNode->F = beenWhat[F];
						} else if (F == Tu) {
							// NE Q?!F:F
							name[nameLen++] = '^';
							pFoundNode->Q = beenWhat[Q];
							pFoundNode->T = beenWhat[F] ^ IBIT;
							pFoundNode->F = beenWhat[F];
						} else {
							// QnTF Q?!T:F
							name[nameLen++] = '!';
							pFoundNode->Q = beenWhat[Q];
							pFoundNode->T = beenWhat[Tu] ^ IBIT;
							pFoundNode->F = beenWhat[F];
						}
					} else {
						if (F == 0) {
							// AND Q?T:0
							name[nameLen++] = '&';
							pFoundNode->Q = beenWhat[Q];
							pFoundNode->T = beenWhat[Tu];
							pFoundNode->F = 0;
						} else if (Tu == 0) {
							// LT Q?0:F
							name[nameLen++] = '<';
							pFoundNode->Q = beenWhat[Q];
							pFoundNode->T = 0;
							pFoundNode->F = beenWhat[F];
						} else if (F == Tu) {
							// SELF Q?F:F
							assert(!"Q?F:F");
						} else {
							// QTF Q?T:F
							name[nameLen++] = '?';
							pFoundNode->Q = beenWhat[Q];
							pFoundNode->T = beenWhat[Tu];
							pFoundNode->F = beenWhat[F];
						}
					}

					// flag operator done
					beenWhat[curr] = nextNode++;
				} else {
					// back-reference to earlier node

					unsigned backref = nextNode - beenWhat[curr];
					assert(backref <= 9);
					name[nameLen++] = (char) ('0' + backref);

					numBackRef++;
				}
			}

		} while (stackPos > 0);

		assert(nameLen <= tinyTree_t::TINYTREE_NAMELEN);
		name[nameLen] = 0;

		foundTree.root = foundTree.count - 1;

		/*
		 * invoke the callback
		 */
		(*cbObject.*cbMember)(foundTree, name, nextPlaceholder - GENERATOR_KSTART, numEndpoint, numBackRef);

	}

	/**
	 * @date 2020-03-17 20:24:13
	 *
	 * Generate all possible structures a tree of `n` nodes can have.
	 *
	 * It recursively pushes and pops nodes to the current tree until all available nodes and placeholders are exhausted.
	 *
	 * It also maintains a virtual stack, one that would be in sync with the stack found in `decode()` decoders.
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
	 * @param {number} nodesLeft -  number of nodes still to fill
	 * @param {number} endpointsLeft -  number of endpoints still to fill
	 * @param {number} numPlaceholder - number of placeholders already assigned
	 * @param {object} cbObject - callback object
	 * @param {object} cbMember - callback member in object
	 * @param {uint64_t} stack - `decode` stack
	 */
	void /*__attribute__((optimize("O0")))*/ generateTrees(unsigned nodesLeft, unsigned endpointsLeft, unsigned numPlaceholder, unsigned stack, callable_t *cbObject, generateTreeCallback_t cbMember) {

		assert(numPlaceholder <= MAXSLOTS);
		assert(endpointsLeft <= GENERATOR_MAXNODES * 2 + 1);

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
		if (buildTree.count == this->restartTabDepth) {
			if (this->pRestartData) {
				/*
				 * assert that restart data is in sync with reality
				 */
				if (ctx.progress != *this->pRestartData)
					ctx.fatal("\n{\"error\":\"restartData out of sync\",\"where\":\"%s:%s:%d\",\"encountered\":\"%lu\",\"expected\":\"%lu\"}\n", __FUNCTION__, __FILE__, __LINE__, ctx.progress, *this->pRestartData);

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
					ctx.restartTick++;
				}

			} else if (ctx.opt_debug & ctx.DEBUGMASK_GENERATOR_TABS) {
				/*
				 * Hit a restart tab, intended for `genrestartdata`
				 */
				foundTree.root = 0;
				(*cbObject.*cbMember)(foundTree, "", 0, 0, 0);
			}
		}

		/*
		 * Use templates to construct node
		 */

		// point to start of state table index by number of already assigned placeholders and nodes
		const uint32_t *pData = pTemplateData + templateIndex[stack][numPlaceholder][endpointsLeft > 3 ? 3 : endpointsLeft];

		while (*pData) {
			unsigned R = this->push(*pData & 0xffff);
			if (R) {
				unsigned newPlaceholder = (*pData >> PACKED_POS_PLACEHOLDER) & ((1 << PACKED_WIDTH_PLACEHOLDER) - 1);
				unsigned newEndpoint = (*pData >> PACKED_POS_ENDPOINT) & ((1 << PACKED_WIDTH_ENDPOINT) - 1);
				unsigned newStack = (*pData >> PACKED_POS_STACK) & ((1 << PACKED_WIDTH_STACK) - 1);

				if (nodesLeft > 1) {
					this->generateTrees(nodesLeft - 1, endpointsLeft - newEndpoint, newPlaceholder, newStack, cbObject, cbMember);
				} else if (endpointsLeft == newEndpoint && newStack == (1U << (R - GENERATOR_NSTART))) {
					// all endpoints populated and stack only current pushed node
					this->callFoundTree(cbObject, cbMember);
					// bump counter after processing
					ctx.progress++;
				}
				this->pop();
			}

			pData++;
		}
	}

};

#endif
