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
 * @date 2020-03-17 23:54:39
 *
 * Building `QnTF` datasets is two-pass:
 *  - First pass is build `QTF` signature base.
 *  - Second pass is to optimize signature names to `QnTF`-only notation.
 *  - `QnTF` databases should be considered `10n9` with full completeness up to `5n9`.
 *
 *  @date 2020-03-18 21:12:26
 *
 *  All normalisation excludes ordered dyadics.
 *
 *  This is to allow pattern detector generators to use all generated combos to create all
 *  transformation masks without having to resort to run-time ordering.
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

#include "tinytree.h"
#include "pushdata.h"

/**
 * `generatorTree_t` extends `tinyTree_t` by giving it tree creation capabilities
 *
 * @date 2020-03-17 20:22:08
 */
struct generatorTree_t : tinyTree_t {

	/// @var {uint32_t[]} array of packed unified operators
	uint32_t packedN[TINYTREE_NEND];

	/// @var {uint8_t[]} array indexed by packed `QTnF` to indicate if index is normalised
	uint8_t *pIsNormalised;

	enum {
		STACKGUARD = 0xf,
	};

	/**
	 * Constructor
	 *
	 * @param {context_t} ctx - I/O context
	 * @param {number} flags - Tree/node functionality
	 * @date 2020-03-14 2020-03-18 18:45:33
	 */
	inline generatorTree_t(context_t &ctx, uint32_t flags) : tinyTree_t(ctx, flags) {
		// assert `pushdata.h` is usable
		assert ((int) tinyTree_t::TINYTREE_KSTART == (int) PUSH_KSTART);
		assert ((int) tinyTree_t::TINYTREE_NSTART == (int) PUSH_NSTART);
		assert ((int) MAXSLOTS <= (int) PUSH_MAXPLACEHOLDERS);
		assert ((int) tinyTree_t::TINYTREE_MAXNODES <= (int) PUSH_MAXNODES);

		// allocate lookup tables
		pIsNormalised = (uint8_t *) ctx.myAlloc("generatorTree_t::pIsNormalised", 1 << 16, sizeof(*this->pIsNormalised));

		initialiseGenerator();
	}

	/**
	 * Release system resources
	 *
	 * @date 2020-03-18 19:30:09
	 */
	~generatorTree_t() {
		ctx.myFree("generatorTree_t::pIsNormalised", this->pIsNormalised);
	}

	/**
	 * Initialise lookup tables for generator
	 *
	 * @date 2020-03-18 21:05:34
	 */
	void initialiseGenerator(void) {
		/*
		 * Create lookup table indexed by packed QnTF notation to determine if `Q,T,F` combo is normalised
		 * Exclude ordered dyadics.
		 */

		// @formatter:off
		for (uint32_t Ti = 0; Ti < 2; Ti++)
		for (uint32_t F = 0; F < tinyTree_t::TINYTREE_NEND; F++)
		for (uint32_t To = 0; To < tinyTree_t::TINYTREE_NEND; To++)
		for (uint32_t Q = 0; Q < tinyTree_t::TINYTREE_NEND; Q++) {
		// @formatter:on

			// create packed notation
			uint32_t ix = (Ti ? 1 << 15 : 0 << 15) | Q << 10 | To << 5 | F << 0;

			// default, not normalised
			pIsNormalised[ix] = 0;

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
				continue; // Q?F:F
			if (To == 0 && !Ti)
				continue; // Q?0:F -> F?~Q:0

			// passed, normalised
			pIsNormalised[ix] = 1;
		}
	}

	/**
	 * Push/add packed node to tree
	 *
	 * @param {uint32_t} qtf - packed notation of `QTF`
	 * @return {uint32_t} 0 if not normalised, or node id of already existing or newly created one
	 * @date 2020-03-18 18:15:57
	 */
	inline uint32_t push(uint32_t qtf) {
		// is it a valid packed notation
		assert((qtf & ~0xffff) == 0);

		// test of normalised
		if (!pIsNormalised[qtf])
			return 0;

		// test if already in cache
		if (pCacheVersion[qtf] == iVersion && pCacheQTF[qtf] != 0) {
			return pCacheQTF[qtf];
		}

		// add/push packed node
		uint32_t nid = this->count++;
		assert(nid < TINYTREE_NEND);

		// add to packed nodes
		this->packedN[nid] = qtf;

		// add to cache
		pCacheQTF[qtf] = nid;
		pCacheVersion[qtf] = iVersion;

		return nid;
	}

	/**
	 * Unwind pushed nodes from tree, releasing nodes that were created
	 *
	 * @param {number} count0 - position to restore to
	 * @date 2020-03-18 18:00:10
	 */
	inline void pop(uint32_t count0) {
		// pop and remove nodes until restore achieved
		while (this->count > count0) {
			// pop last node
			uint32_t qtf = this->packedN[--this->count];

			// erase index
			pCacheQTF[qtf] = 0;
		}
	}

	/**
	 * Unwind pushed nodes from tree, releasing nodes that were created
	 *
	 * @param {number} count0 - position to restore to
	 * @date 2020-03-18 22:17:26
	 */
	inline void foundTree() {
		// simply dump tree
		for (unsigned i = TINYTREE_NSTART; i < this->count; i++) {
			unsigned qtf = packedN[i];
			printf("%d.%x.%x.%x ", qtf & 0x8000 ? 1 : 0, (qtf >> 10) & 0x1f, (qtf >> 5) & 0x1f, (qtf >> 0) & 0x1f);
		}
		printf("\n");
	}

	/**
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
	 * Each pushed/popped value is max 4 bits in size, allowing for a stack-depth of 16
	 * This allows for easy shifting. "<<=4" for "push", ">>=4" for pops.
	 *
	 * The generator makes use of 6 state driven tables `push_QTF[]`, `push_PTF[]`, `push_QPF[]`, `push_QTP[]`, `push_PPF[]`, `push_PTP[]`, `push_QPP[]`
	 * You index them with the current number of placeholders and nodes already assigned.
	 * It will return the list of all possible nodes the current position could become.
	 * The list is used as templates to create nodes
	 *
	 * Table naming:
	 *  - Q,T,F are operands that are newly assigned placeholders.
	 *  - P should be replaced by popped stack values (pointing to other nodes)
	 *
	 * @param {number} endpointLeft -  number of endpoints still to fill
	 * @param {number} numPlaceholder - number of placeholders already assigned
	 * @param {uint64_t} stack - `decode` stack
	 * @date 2020-03-17 20:24:13
	 */
	void /*__attribute__((optimize("O0")))*/ generateTrees(unsigned endpointLeft, unsigned numPlaceholder, uint64_t stack) {

		const uint32_t count0 = this->count;

		if (endpointLeft >= 3) {
			/*
			 * new Q, T and F
			 */

			// point to start of state table index by number of already assigned placeholders and nodes
			const uint32_t *pData = pushData + pushIndex[PUSH_QTF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

			while (*pData) {
				if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF))
					continue; // bailout when QnTF is exhausted

				uint32_t R = this->push(*pData & 0xffff); // unpack and push operands
				if (R) {
					if (endpointLeft == 3 && stack == STACKGUARD)
						this->foundTree(); // All placeholders used and stack unwound
					else
						this->generateTrees(endpointLeft - 3, (*pData >> 16), stack << 4 | R);
				}
				this->pop(count0);

				pData++;
			}
		}

		// test for at least 1 push and the stack-guard
		if (endpointLeft >= 2 && (stack & ~0xfLL)) {

			/*
			 * pop Q, new T and F
			 */
			{
				// pop Q
				uint64_t newStack = stack;
				const uint32_t Q = (uint32_t) newStack & 0xf;
				newStack >>= 4;

				// runtime values to merge into template
				uint32_t qtf = Q << PUSH_POS_Q;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_PTF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF))
						continue; // bailout when QnTF is exhausted

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q, unpack and push operands
					if (R) {
						if (endpointLeft == 2 && newStack == STACKGUARD)
							this->foundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointLeft - 2, (*pData >> 16), (newStack << 4) | R);
					}
					this->pop(count0);

					pData++;
				}
			}

			/*
			 * pop T, new Q, F
			 */
			{
				// pop T
				uint64_t newStack = stack;
				const uint32_t T = (uint32_t) newStack & 0xf;
				newStack >>= 4;

				// runtime values to merge into template
				uint32_t qtf = T << PUSH_POS_T;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_QPF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF))
						continue; // bailout when QnTF is exhausted

					// Reject XOR/NE, gets handled in next loop
					if (T == ((*pData >> PUSH_POS_F) & ~0xf))
						continue;

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge T, unpack and push operands
					if (R) {
						if (endpointLeft == 2 && newStack == STACKGUARD)
							this->foundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointLeft - 2, (*pData >> 16), (newStack << 4) | R);
					}
					this->pop(count0);

					pData++;
				}
			}

			/*
			 * pop F, new Q, T
			 */
			{
				// pop F
				uint64_t newStack = stack;
				const uint32_t F = (uint32_t) newStack & 0xf;
				newStack >>= 4;

				// runtime values to merge into template
				uint32_t qtf = F << PUSH_POS_F;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_QTP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF))
						continue; // bailout when QnTF is exhausted

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge F, unpack and push operands
					if (R) {
						if (endpointLeft == 2 && newStack == STACKGUARD)
							this->foundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointLeft - 2, (*pData >> 16), (newStack << 4) | R);
					}
					this->pop(count0);

					pData++;
				}
			}
		}

		// test for at least 2 pushes and the stack-guard
		if (endpointLeft >= 1 && (stack & ~0xffLL)) {
			/*
			 * pop Q and T, new F
			 */
			{
				// pop Q, T
				uint64_t newStack = stack;
				const uint32_t T = (uint32_t) newStack & 0xf;
				newStack >>= 4;
				const uint32_t Q = (uint32_t) newStack & 0xf;
				newStack >>= 4;

				// runtime values to merge into template
				uint32_t qtf = (Q << PUSH_POS_Q) | (T << PUSH_POS_T);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_PPF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF))
						continue; // bailout when QnTF is exhausted

					// Reject XOR/NE, gets handled in next loop
					if (T == ((*pData >> PUSH_POS_F) & ~0xf))
						continue;

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q+T, unpack and push operands
					if (R) {
						if (endpointLeft == 1 && newStack == STACKGUARD)
							this->foundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointLeft - 1, (*pData >> 16), (newStack << 4) | R);
					}
					this->pop(count0);

					pData++;
				}
			}

			/*
			 * pop Q and F, new T
			 */
			{
				// pop Q, F
				uint64_t newStack = stack;
				const uint32_t F = (uint32_t) newStack & 0xf;
				newStack >>= 4;
				const uint32_t Q = (uint32_t) newStack & 0xf;
				newStack >>= 4;

				// runtime values to merge into template
				uint32_t qtf = (Q << PUSH_POS_Q) | (F << PUSH_POS_F);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_PTP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF))
						continue; // bailout when QnTF is exhausted

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q+F, unpack and push operands
					if (R) {
						if (endpointLeft == 1 && newStack == STACKGUARD)
							this->foundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointLeft - 1, (*pData >> 16), (newStack << 4) | R);
					}
					this->pop(count0);

					pData++;
				}
			}

			/*
			 * pop T and F, new Q
			 */
			{
				// pop T, F
				uint64_t newStack = stack;
				const uint32_t F = (uint32_t) newStack & 0xf;
				newStack >>= 4;
				const uint32_t T = (uint32_t) newStack & 0xf;
				newStack >>= 4;

				// runtime values to merge into template
				uint32_t qtf = (T << PUSH_POS_T) | (F << PUSH_POS_F);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_QPP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF))
						continue; // bailout when QnTF is exhausted

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q+F, unpack and push operands
					if (R) {
						if (endpointLeft == 1 && newStack == STACKGUARD)
							this->foundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointLeft - 1, (*pData >> 16), (newStack << 4) | R);
					}
					this->pop(count0);

					pData++;
				}
			}
		}

		// test for at least 3 pushes and the stack-guard
		if (stack & ~0xfffLL) {
			/*
			 * pop Q, T and F
			 */

			uint64_t newStack = stack;
			// The stack looks like `"xxxxQTF"`, which is the contact encoding of `push()`
			const uint32_t QTF = (uint32_t) newStack & 0xfff;
			newStack >>= 12;

			{
				// QnTF
				uint32_t R = this->push(PUSH_TIBIT | QTF); // push with inverted T
				if (R) {
					if (endpointLeft == 0 && newStack == STACKGUARD)
						this->foundTree(); // All placeholders used and stack unwound
					else
						this->generateTrees(endpointLeft, numPlaceholder, (newStack << 4) | R);
				}
				this->pop(count0);
			}

			if (~this->flags & context_t::MAGICMASK_QNTF) {
				// QTF
				uint32_t R = this->push(QTF); // push without inverted T
				if (R) {
					if (endpointLeft == 0 && newStack == STACKGUARD)
						this->foundTree();
					else
						this->generateTrees(endpointLeft, numPlaceholder, (newStack << 4) | R);
				}
				this->pop(count0);
			}
		}
	}

};

#endif