#ifndef _GENERATOR_H
#define _GENERATOR_H

/*
 * @date 2020-03-17 20:04:16
 *
 * Generate all possible normalised structures for a given structure size
 *
 * Structures will pass level-1 normalisation with exception the of ordering of dyadics.
 * This is because during runtime the placeholder endpoints will be replaced by actual
 * values which might have different values.
 *
 * @date 2020-03-17 23:54:39
 *
 * Building `QnTF` datasets is two-pass:
 *  - First pass is build `QTF` signature base.
 *  - Second pass is to optimize signature names to `QnTF`-only notation.
 *  - `QnTF` databases should be considered `10n9` with full completeness up to `5n9`.
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

/**
 * `generatorTree_t` extends `tinyTree_t` by giving it tree creation capabilities
 *
 * @date 2020-03-17 20:22:08
 */
struct generatorTree_t : tinyTree_t {

	enum {
		STACKGUARD = 0xf,
		PUSH_TIBIT = 0x8000,
		PUSH_FPOS = 0,
		PUSH_TPOS = 4,
		PUSH_QPOS = 8,
	};

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
			const uint32_t *pData = pushData_QTF + pushIndex_QTF[numPlaceholder * tinyTree_t::TINYTREE_MAXNODES + (this->count - tinyTree_t::TINYTREE_NSTART)];

			while (*pData) {
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

		// test for at least 2 pushes and the stack-guard
		if (endpointLeft >= 2 &&  (stack & ~0xffLL)) {

			/*
			 * pop Q, new T and F
			 */
			{
				// pop Q
				uint64_t newStack = stack;
				const uint32_t Q = (uint32_t) newStack & 0xf;
				newStack >>= 4;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData_PTF + pushIndex_PTF[numPlaceholder * tinyTree_t::TINYTREE_MAXNODES + this->count - tinyTree_t::TINYTREE_NSTART];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) || (this->flags & context_t::MAGICMASK_QNTF)) {
						uint32_t R = this->push((*pData & 0xffff) | (Q << PUSH_QPOS)); // merge Q, unpack and push operands
						if (R) {
							if (endpointLeft == 2 && stack == STACKGUARD)
								this->foundTree(); // All placeholders used and stack unwound
							else
								this->generateTrees(endpointLeft - 2, (*pData >> 16), (newStack << 4) | R);
						}
						this->pop(count0);

						pData++;
					}
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

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData_QPF + pushIndex_QPF[numPlaceholder * tinyTree_t::TINYTREE_MAXNODES + this->count - tinyTree_t::TINYTREE_NSTART];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) || (this->flags & context_t::MAGICMASK_QNTF)) {
						// Reject XOR/NE, gets handled in next loop
						if (T == ((*pData >> PUSH_FPOS) & ~0xf))
							continue;

						uint32_t R = this->push((*pData & 0xffff) | (T << PUSH_TPOS)); // merge T, unpack and push operands
						if (R) {
							if (endpointLeft == 2 && stack == STACKGUARD)
								this->foundTree(); // All placeholders used and stack unwound
							else
								this->generateTrees(endpointLeft - 2, (*pData >> 16), (newStack << 4) | R);
						}
						this->pop(count0);
					}

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

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData_QTP + pushIndex_QTP[numPlaceholder * tinyTree_t::TINYTREE_MAXNODES + this->count - tinyTree_t::TINYTREE_NSTART];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) || (this->flags & context_t::MAGICMASK_QNTF)) {
						uint32_t R = this->push((*pData & 0xffff) | (F << PUSH_FPOS)); // merge F, unpack and push operands
						if (R) {
							if (endpointLeft == 2 && stack == STACKGUARD)
								this->foundTree(); // All placeholders used and stack unwound
							else
								this->generateTrees(endpointLeft - 2, (*pData >> 16), (newStack << 4) | R);
						}
						this->pop(count0);
					}

					pData++;
				}
			}
		}

		// test for at least 2 pushes and the stack-guard
		if (endpointLeft >= 1 && (stack & ~0xfffLL)) {
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

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData_PPF + pushIndex_PPF[numPlaceholder * tinyTree_t::TINYTREE_MAXNODES + this->count - tinyTree_t::TINYTREE_NSTART];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) || (this->flags & context_t::MAGICMASK_QNTF)) {
						// Reject XOR/NE, gets handled in next loop
						if (T == ((*pData >> PUSH_FPOS) & ~0xf))
							continue;

						uint32_t R = this->push((*pData & 0xffff) | (Q << PUSH_QPOS) | (T << PUSH_TPOS)); // merge Q+T, unpack and push operands
						if (R) {
							if (endpointLeft == 1 && stack == STACKGUARD)
								this->foundTree(); // All placeholders used and stack unwound
							else
								this->generateTrees(endpointLeft - 1, (*pData >> 16), (newStack << 4) | R);
						}
						this->pop(count0);
					}

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

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData_PTP + pushIndex_PTP[numPlaceholder * tinyTree_t::TINYTREE_MAXNODES + this->count - tinyTree_t::TINYTREE_NSTART];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) || (this->flags & context_t::MAGICMASK_QNTF)) {
						uint32_t R = this->push((*pData & 0xffff) | (Q << PUSH_QPOS) | (F << PUSH_FPOS)); // merge Q+F, unpack and push operands
						if (R) {
							if (endpointLeft == 1 && stack == STACKGUARD)
								this->foundTree(); // All placeholders used and stack unwound
							else
								this->generateTrees(endpointLeft - 1, (*pData >> 16), (newStack << 4) | R);
						}
						this->pop(count0);
					}

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

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData_QPP + pushIndex_QPP[numPlaceholder * tinyTree_t::TINYTREE_MAXNODES + this->count - tinyTree_t::TINYTREE_NSTART];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) || (this->flags & context_t::MAGICMASK_QNTF)) {
						uint32_t R = this->push((*pData & 0xffff) | (T << PUSH_TPOS) | (F << PUSH_FPOS)); // merge Q+F, unpack and push operands
						if (R) {
							if (endpointLeft == 1 && stack == STACKGUARD)
								this->foundTree(); // All placeholders used and stack unwound
							else
								this->generateTrees(endpointLeft - 1, (*pData >> 16), (newStack << 4) | R);
						}
						this->pop(count0);
					}

					pData++;
				}
			}

		}

		// test for at least 3 pushes and the stack-guard
		if (stack & ~0xffffLL) {
			/*
			 * pop Q, T and F
			 */

			uint64_t newStack = stack;
			// The stack looks like `"xxxxQTF"`, which is the contact encoding of `push()`
			const uint32_t QTF = (uint32_t) newStack & 0xfff;
			newStack >>= 12;

			if (~this->flags & context_t::MAGICMASK_QNTF) {
				// QTF
				uint32_t R = this->push(QTF); // push without inverted T
				if (R) {
					if (endpointLeft == 0 && stack == STACKGUARD)
						this->foundTree();
					else
						this->generateTrees(endpointLeft, numPlaceholder, (newStack << 4) | R);
				}
				this->pop(count0);
			}

			{
				// QnTF
				uint32_t R = this->push(PUSH_TIBIT | QTF); // push with inverted T
				if (R) {
					if (endpointLeft == 0 && stack == STACKGUARD)
						this->foundTree(); // All placeholders used and stack unwound
					else
						this->generateTrees(endpointLeft, numPlaceholder, (newStack << 4) | R);
				}
				this->pop(count0);
			}
		}
	}

};

#endif