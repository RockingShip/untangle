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
		PACKED_TIBIT = 1 << (PACKED_WIDTH * 3),

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_FPOS = (PACKED_WIDTH * 0),

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_TPOS = (PACKED_WIDTH * 1),

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		PACKED_QPOS = (PACKED_WIDTH * 2),

		/// @constant {number} - used for `pIsType[]` to indicate type of node
		PACKED_OR = 0x01,
		PACKED_GT = 0x02,
		PACKED_XOR = 0x04,
		PACKED_QnTF = 0x08,
		PACKED_AND = 0x10,
		PACKED_QTF = 0x20,
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

	/// @var {object} callback object for `foundTree()`
	context_t* cbObject;

	/// @var {object} callback member for `foundTree()`
	void(context_t::* cbMember)(generatorTree_t&);

	/**
	 * @date 2020-03-18 18:45:33
	 *
	 * Constructor
	 *
	 * @param {context_t} ctx - I/O context
	 */
	inline generatorTree_t(context_t &ctx) : tinyTree_t(ctx) {
		// assert `pushdata.h` is usable
		assert ((int) tinyTree_t::TINYTREE_KSTART == (int) PUSH_KSTART);
		assert ((int) tinyTree_t::TINYTREE_NSTART == (int) PUSH_NSTART);
		assert ((int) MAXSLOTS <= (int) PUSH_MAXPLACEHOLDERS);
		assert ((int) tinyTree_t::TINYTREE_MAXNODES <= (int) PUSH_MAXNODES);

		// Assert that the highest available node fits into a 5 bit value. `2^5` = 32.
		assert(TINYTREE_NEND < 32);

		// allocate structures
		pIsType = (uint8_t *) ctx.myAlloc("generatorTree_t::pIsType", 1 << 16, sizeof(*this->pIsType));
		pCacheQTF = (uint32_t *) ctx.myAlloc("generatorTree_t::pCacheQTF", 1 << 16, sizeof(*this->pCacheQTF));
		pCacheVersion = (uint32_t *) ctx.myAlloc("generatorTree_t::pCacheVersion", 1 << 16, sizeof(*this->pCacheVersion));

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
			::memset(pCacheVersion, 0, (sizeof(*pCacheVersion) * (1 << 16)));
		}
		iVersion++; // when overflows, next call will clear

		this->count = TINYTREE_NSTART; // rewind first free node
		this->root = 0; // set result to zero-reference
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
		for (uint32_t F = 0; F < tinyTree_t::TINYTREE_NEND; F++)
		for (uint32_t To = 0; To < tinyTree_t::TINYTREE_NEND; To++)
		for (uint32_t Q = 0; Q < tinyTree_t::TINYTREE_NEND; Q++) {
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
		}
	}

	/**
	 * @date 2020-03-20 18:19:45
	 *
	 * Level-2 normalisation: dyadic ordering.
	 *
	 * Comparing the operand reference id's is not sufficient to determining ordering.
	 *
	 * For example `"ab+cd>&~"` and `"cd>ab+&~"` would be considered 2 different trees.
	 *
	 * To find them identical a deep inspection must occur
	 *
	 * @param {number} lhs - entrypoint to right side
	 * @param {number} rhs - entrypoint to right side
	 * @return {number} `-1` if `lhs<rhs`, `0` if `lhs==rhs` and `+1` if `lhs>rhs`
	 */
	int compare(uint32_t lhs, uint32_t rhs) {

		uint32_t stackL[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		uint32_t stackR[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int stackPos = 0;

		assert(~lhs & IBIT);
		assert(~rhs & IBIT);

		stackL[stackPos] = lhs;
		stackR[stackPos] = rhs;
		stackPos++;

		uint32_t beenThere = 0;
		uint8_t beenWhere[TINYTREE_NEND];

		do {
			// pop stack
			--stackPos;
			uint32_t L = stackL[stackPos];
			uint32_t R = stackR[stackPos];

			/*
			 * compare endpoints/references
			 */
			if (L < TINYTREE_NSTART && R >= TINYTREE_NSTART)
				return -1; // `end` < `ref`
			if (L >= TINYTREE_NSTART && R < TINYTREE_NSTART)
				return +1; // `ref` > `end`

			/*
			 * compare contents
			 */
			if (L < TINYTREE_NSTART) {
				if (L < R)
					return -1; // `lhs` < `rhs`
				if (L > R)
					return +1; // `lhs` < `rhs`

				// continue with next stack entry
				continue;
			}

			/*
			 * Been here before
			 */
			if (beenThere & (1 << L)) {
				if (beenWhere[L] == R)
					continue; // yes
			}
			beenThere |= 1 << L;
			beenWhere[L] = R;

			// decode L and R
			L = packedN[L];
			R = packedN[R];

			/*
			 * Reminder:
			 *  [ 2] a ? ~0 : b                  "+" OR
			 *  [ 6] a ? ~b : 0                  ">" GT
			 *  [ 8] a ? ~b : b                  "^" XOR
			 *  [ 9] a ? ~b : c                  "!" QnTF
			 *  [16] a ?  b : 0                  "&" AND
			 *  [19] a ?  b : c                  "?" QTF
			 */

			assert(pIsType[L]);
			assert(pIsType[R]);

			/*
			 * compare structure
			 */
			if (pIsType[L] < pIsType[R])
				return -1; // `L` < `R`
			if (pIsType[L] > pIsType[R])
				return +1; // `L` > `R`

			/*
			 * Push references
			 */
			stackL[stackPos] = (L >> PACKED_FPOS) & PACKED_MASK;
			stackR[stackPos] = (R >> PACKED_FPOS) & PACKED_MASK;
			stackPos++;
			stackL[stackPos] = (L >> PACKED_TPOS) & PACKED_MASK;
			stackR[stackPos] = (R >> PACKED_TPOS) & PACKED_MASK;
			stackPos++;
			stackL[stackPos] = (L >> PACKED_QPOS) & PACKED_MASK;
			stackR[stackPos] = (R >> PACKED_QPOS) & PACKED_MASK;
			stackPos++;

		} while (stackPos > 0);

		// identical
		return 0;
	}

	/**
	 * @date 2020-03-21 02:01:18
	 *
	 * Register a callback for found trees
	 */
	void addCallback(context_t* object, void(context_t::* member)(generatorTree_t&)) {
		cbObject = object;
		cbMember = member;
	}

	/**
	 * @date 2020-03-18 22:17:26
	 *
	 * found level-1,2 normalised candidate.
	 */
	inline void callFoundTree(void) {
		// invoke the callback
		(*cbObject.*cbMember)(*this);
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
		// save root
		this->root = nid;

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
\	 * @param {number} endpointsLeft -  number of endpoints still to fill
	 * @param {number} numPlaceholder - number of placeholders already assigned
	 * @param {uint64_t} stack - `decode` stack
	 */
	void /*__attribute__((optimize("O0")))*/ generateTrees(unsigned endpointsLeft, unsigned numPlaceholder, uint64_t stack) {

		assert (numPlaceholder <= MAXSLOTS);
		assert(tinyTree_t::TINYTREE_MAXNODES <= 64 / PACKED_WIDTH);

		/*
		 * Nodes with three endpoints
		 */

		if (endpointsLeft >= 3) {
			/*
			 * Because there is no substitution, the templates are normalised and ordered by nature
			 */

			// point to start of state table index by number of already assigned placeholders and nodes
			const uint32_t *pData = pushData + pushIndex[PUSH_QTF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

			while (*pData) {
				if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF)) {
					pData++;
					continue; // bailout when QnTF is exhausted
				}

				// NOTE: template endpoints are always ordered

				uint32_t R = this->push(*pData & 0xffff); // unpack and push operands
				if (R) {
					if (endpointsLeft == 3 && stack == 0)
						this->callFoundTree(); // All placeholders used and stack unwound
					else
						this->generateTrees(endpointsLeft - 3, (*pData >> 16), stack << PACKED_WIDTH | R);
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
			 * If `T` or `F` would be zero and the node dyadic, then ordering would fail because `<pop>` is always higher.
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = pop0 << PACKED_QPOS;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_PTF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					// NOTE: template endpoints are always ordered

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q, unpack and push operands
					if (R) {
						if (endpointsLeft == 2 && stack == 0)
							this->callFoundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 2, (*pData >> 16), (stack << PACKED_WIDTH) | R);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `Q` `<pop>` `F`
			 * If `F` is zero then node can be `GT` if `T` is inverted or `AND` otherwise
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = pop0 << PACKED_TPOS;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_QPF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					// NOTE: `F` is templated, might be 0 which makes it an `GT` (which is never ordered) or `AND` (which is always ordered because `Q` is lower than `<pop>`

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge T, unpack and push operands
					if (R) {
						if (endpointsLeft == 2 && stack == 0)
							this->callFoundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 2, (*pData >> 16), (stack << PACKED_WIDTH) | R);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `Q` `T` `<pop>`
			 * If `T` is zero then node can be `OR` if `T` is inverted or `LT` otherwise which is non-normalised.
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = pop0 << PACKED_FPOS;

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_QTP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					// NOTE: `T` is templated which might be ~0 making it `OR`. `Q` is always lower than `<pop>` making it always ordered

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge F, unpack and push operands
					if (R) {
						if (endpointsLeft == 2 && stack == 0)
							this->callFoundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 2, (*pData >> 16), (stack << PACKED_WIDTH) | R);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 *  `Q` XOR `<pop>`
			 * <pop> is higher than `Q` and therefore always ordered
			 *
			 * NOTE: Node has invisible endpoint
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop0 << PACKED_TPOS) | (pop0 << PACKED_FPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_QPP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if (~*pData & PUSH_TIBIT) {
						pData++;
						continue; // only `QnTF` allowed. stop when exhausted
					}

					// NOTE: `Q` is templated, which is always lower than `<pop>` making it always ordered

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q+F, unpack and push operands
					if (R) {
						if (endpointsLeft == 2 && stack == 0) {
							this->callFoundTree(); // All placeholders used and stack unwound
						} else {
							this->generateTrees(endpointsLeft - 2, (*pData >> 16), (stack << PACKED_WIDTH) | R);
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
			 * If `F` is zero then node can be `GT` if `T` is inverted or `AND` otherwise
			 *
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop1 << PACKED_QPOS) | (pop0 << PACKED_TPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_PPF][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					// NOTE: `F` is templated, might be 0 which makes it an `GT` (which is never ordered) or `AND`
					if (pIsType[(*pData & 0xffff) | qtf] & PACKED_AND) {
						if (this->compare(pop1, pop0) > 0) {
							pData++;
							continue;
						}
					}

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q+T, unpack and push operands
					if (R) {
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, (*pData >> 16), (stack << PACKED_WIDTH) | R);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `<pop>` `T` `<pop>`
			 * If `T` is zero then node can be `OR` if `T` is inverted or `LT` otherwise which is non-normalsed.
			 *
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop1 << PACKED_QPOS) | (pop0 << PACKED_FPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_PTP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					// NOTE: `T` is templated, might be ~0 which makes it an `OR`
					if (pIsType[(*pData & 0xffff) | qtf] & PACKED_OR) {
						if (this->compare(pop1, pop0) > 0) {
							pData++;
							continue;
						}
					}

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q+F, unpack and push operands
					if (R) {
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, (*pData >> 16), (stack << PACKED_WIDTH) | R);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 * `Q` `<pop>` `<pop>`
			 * Never a dyadic or XOR
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = (pop1 << PACKED_TPOS) | (pop0 << PACKED_FPOS);

				// point to start of state table index by number of already assigned placeholders and nodes
				const uint32_t *pData = pushData + pushIndex[PUSH_QPP][this->count - tinyTree_t::TINYTREE_NSTART][numPlaceholder];

				while (*pData) {
					if ((~*pData & PUSH_TIBIT) && (this->flags & context_t::MAGICMASK_QNTF)) {
						pData++;
						continue; // bailout when QnTF is exhausted
					}

					uint32_t R = this->push((*pData & 0xffff) | qtf); // merge Q+F, unpack and push operands
					if (R) {
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, (*pData >> 16), (stack << PACKED_WIDTH) | R);
						this->pop();
					}

					pData++;
				}
			}

			/*
			 *  `<pop>` XOR `<pop>`
			 * XOR needs ordering test
			 *
			 * NOTE: Node has invisible endpoint
			 */
			{
				// runtime values to merge into template
				uint32_t qtf = PACKED_TIBIT | (pop1 << PACKED_QPOS) | (pop0 << PACKED_TPOS) | (pop0 << PACKED_FPOS);

				// NOTE: make sure pop1 < pop0

				if (this->compare(pop1, pop0) < 0) {
					uint32_t R = this->push(qtf); // merge Q+F, unpack and push operands
					if (R) {
						if (endpointsLeft == 1 && stack == 0)
							this->callFoundTree(); // All placeholders used and stack unwound
						else
							this->generateTrees(endpointsLeft - 1, numPlaceholder, (stack << PACKED_WIDTH) | R);
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
				uint32_t R = this->push(PUSH_TIBIT | qtf); // push with inverted T
				if (R) {
					if (endpointsLeft == 0 && stack == 0)
						this->callFoundTree(); // All placeholders used and stack unwound
					else
						this->generateTrees(endpointsLeft, numPlaceholder, (stack << PACKED_WIDTH) | R);
					this->pop();
				}
			}

			if (~this->flags & context_t::MAGICMASK_QNTF) {
				// QTF
				uint32_t R = this->push(qtf); // push without inverted T
				if (R) {
					if (endpointsLeft == 0 && stack == 0)
						this->callFoundTree();
					else
						this->generateTrees(endpointsLeft, numPlaceholder, (stack << PACKED_WIDTH) | R);
					this->pop();
				}
			}
		}
	}

};

#endif