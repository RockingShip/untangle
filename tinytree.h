#ifndef _TINYTREE_H
#define _TINYTREE_H

/*
 * @date 2020-03-13 19:18:57
 *
 * `tinyTree_t` is a tree specifically designed for database generation.
 * It is speed optimised for `Xn9` datasets.
 *
 * Optimisations are:
 *   - Hardcoded `kstart` and `nstart`
 *   - Maximum of `X` nodes in `QTF` mode and `X*2` nodes in `QnTF` mode
 *   - Versioned memory
 *   - No endpoint/back-reference prefixing
 *   - Decoding assumes correct notations
 *   - Separate placeholder/skin
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
#include <ctype.h>
#include <string.h>
#include <immintrin.h>
#include "context.h"
#include "datadef.h"

/**
 * @date 2020-03-13 19:30:38
 *
 * Single unified operator node
 *
 * @date 2020-04-23 09:22:37
 *
 * The `T` component can have its `IBIT` set to differentiate between the `QTF` and `QnTF` operator. (current model)
 * This model normalises to order because it eliminates the need for inverting as it can be rewritten with level-1 normalisation.
 *
 * The `F` component can have its `IBIT` set to differentiate between the `QTF` and `QTnF` operator. (previous and depreciated model)
 * This model normalises to chaos because it allows trees to have both inverted and non-inverted counterparts that trigger a cascade of tree rewrites when they collide.
 *
 * The `Q` component never has its `IBIT` set because of level-1 normalisation.
 * It is reserved to flag that the node is immune for normalisation when constructing trees.
 * This is needed when the node is the root of an unsafe signature that will guarantee to level-4 normalise which it can't.
 *
 * @typedef {object}
 */
struct tinyNode_t {
	/// @var {number} - reference to `"question"`
	uint32_t Q;
	/// @var {number} - reference to `"when-true"`.
	uint32_t T;
	/// @var {number} - reference to `"when-false"`
	uint32_t F;
};

/**
 * @date 2020-03-13 19:31:48
 *
 * High speed node tree
 *
 * @typedef {object}
 */
struct tinyTree_t {

	enum {
		/// @constant {number} - Number of nodes. Twice MAXSLOTS because of `QnTF` expansion
		TINYTREE_MAXNODES = 7,

		/// @constant {number} - Starting index in tree of first variable/endpoint
		TINYTREE_KSTART = 1,

		/// @constant {number} - Starting index in tree of first operator node
		TINYTREE_NSTART = (TINYTREE_KSTART + MAXSLOTS),

		/// @constant {number} - Total number of entries in tree
		TINYTREE_NEND = (TINYTREE_NSTART + TINYTREE_MAXNODES),

		/// @constant {number} - Maximum stack depth for tree walk. (3 operands + 1 opcode) per node
		TINYTREE_MAXSTACK = ((3 + 1) * TINYTREE_MAXNODES),

		/// @constant {number} - Maximum length of tree name. leaf + (3 operands + 1 opcode) per node + root-invert + terminator
		TINYTREE_NAMELEN = (1 + (3 + 1) * TINYTREE_MAXNODES + 1 + 1),
	};

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {number} index of first free node
	uint32_t count;

	/// @var {node_t[]} array of unified operators
	tinyNode_t N[TINYTREE_NEND];

	// @var {number} single entrypoint/index where the result can be found
	uint32_t root;


	/**
 	 * @date 2020-03-14 00:27:38
 	 *
	 * Constructor
	 *
	 * @param {context_t} ctx - I/O context
	 * @param {number} flags - Tree/node functionality
	 */
	inline tinyTree_t(context_t &ctx) : ctx(ctx) {
		/*
		 * Assert that the highest available node fits into a 5 bit value. `2^5` = 32.
		 *  - for `beenThere[]` using it as index in uint32_t
		 *  - for `pCacheQTF[]` using packed `QTnF` storage of 5 bits per field
		 */
		assert(TINYTREE_NEND < 32);

		this->clearTree();
	}

	/**
	 * @date 2020-03-18 19:30:09
	 *
	 * Release system resources
	 */
	~tinyTree_t() {
	}

	/*
	 * @date 2020-03-14 00:31:04
	 *
	 * Copy constructor not supported, so using them will trigger "unresolved externals"
	 */
	tinyTree_t(const tinyTree_t &rhs);

	tinyTree_t &operator=(const tinyTree_t &rhs);

	/**
	 * @date 2020-03-06 22:27:36
	 *
	 * Erase the contents
	 */
	inline void clearTree(void) {
		this->count = TINYTREE_NSTART; // rewind first free node
		this->root  = 0; // set result to zero-reference
	}

	/**
	 * @date2020-04-09 19:57:28
	 *
	 * Compare trees by content without looking at (internal) references.
	 *
	 * Comparing follows tree walking path.
	 * First layout, when components are satisfied then endpoints.
	 *
	 *
	 * @date 2021-07-19 23:48:11
	 *
	 * With latest insights: compare structure first, then endpoints.
	 * Make compatible with implementation in `eval.c`
	 * ordering of dyadics should work now `./slookup --member 'abc!de^^f^'`
	 *
	 * @param {number} lhs - entrypoint to right side
	 * @param {number} rhs - entrypoint to right side
	 * @param {boolean} layoutOnly - ignore endpoint values when `true`
	 * @return {number} `<0` if `lhs<rhs`, `0` if `lhs==rhs` and `>0` if `lhs>rhs`
	 */
	int compare(unsigned lhs, const tinyTree_t &treeR, unsigned rhs) const {

		uint32_t stackL[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		uint32_t stackR[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      stackPos = 0;
		int      secondary = 0;  // compare structure first, then endpoints

		assert(!(lhs & IBIT));
		assert(!(rhs & IBIT));

		// nodes already processed
		uint32_t beenThereL;
		uint32_t beenThereR;
		uint32_t beenWhatL[TINYTREE_NEND];
		uint32_t beenWhatR[TINYTREE_NEND];

		// mark `zero` processed
		beenThereL = (1 << 0);
		beenThereR = (1 << 0);
		beenWhatL[0] = 0;
		beenWhatR[0] = 0;

		// push root to start
		stackL[stackPos] = lhs;
		stackR[stackPos] = rhs;
		stackPos++;

		do {
			// pop stack
			--stackPos;
			unsigned L = stackL[stackPos];
			unsigned R = stackR[stackPos];

			// shortcut
			if (L == R && this == &treeR)
				continue;

			/*
			 * compare if either is an endpoint
			 */
			if (L < TINYTREE_NSTART && R >= TINYTREE_NSTART)
				return -1; // `end` < `ref`
			if (L >= TINYTREE_NSTART && R < TINYTREE_NSTART)
				return +1; // `ref` > `end`

			/*
			 * compare contents
			 */
			if (L < TINYTREE_NSTART) {
				if (secondary == 0) {
					if (L < R)
						secondary = -1; // `lhs` < `rhs`
					else if (L > R)
						secondary = +1; // `lhs` < `rhs`
				}

				// continue with next stack entry
				continue;
			}

			/*
			 * Been here before
			 */
			if ((beenThereL & (1 << L)) && (beenThereR & (1 << R))) {
				if (beenWhatL[L] == R && beenWhatR[R] == L)
					continue; // yes
			}
			beenThereL |= 1 << L;
			beenThereR |= 1 << R;
			beenWhatL[L] = R;
			beenWhatR[R] = L;

			// decode L and R
			const tinyNode_t *pNodeL = this->N + L;
			const tinyNode_t *pNodeR = treeR.N + R;

			/*
			 * Reminder:
			 *  [ 2] a ? ~0 : b                  "+" OR
			 *  [ 6] a ? ~b : 0                  ">" GT
			 *  [ 8] a ? ~b : b                  "^" XOR
			 *  [ 9] a ? ~b : c                  "!" QnTF
			 *  [16] a ?  b : 0                  "&" AND
			 *  [19] a ?  b : c                  "?" QTF
			 */

			/*
			 * compare structure
			 */
			if ((pNodeL->T & IBIT) && !(pNodeR->T & IBIT))
				return -1; // `QnTF` < `QTF`
			if (!(pNodeL->T & IBIT) && (pNodeR->T & IBIT))
				return +1; // `QTF` > `QnTF`

			if (pNodeL->T == IBIT && pNodeR->T != IBIT)
				return -1; // `OR` < !`OR`
			if (pNodeL->T != IBIT && pNodeR->T == IBIT)
				return +1; // !`OR` > `OR`

			if (pNodeL->F == 0 && pNodeR->F != 0)
				return -1; // `GT` < !`GT` or `AND` < !`AND`
			if (pNodeL->F != 0 && pNodeR->F == 0)
				return +1; // !`GT` > `GT` or !`AND` > `AND`

			if (pNodeL->F == (pNodeL->T ^ IBIT) && pNodeR->F != (pNodeR->T ^ IBIT))
				return -1; // `XOR` < !`XOR`
			if (pNodeL->F != (pNodeL->T ^ IBIT) && pNodeR->F == (pNodeR->T ^ IBIT))
				return +1; // !`XOR` > `XOR`

			/*
			 * Push natural walking order
			 * deep Q, deep T, deep F, endpoint Q, endpoint T, endpoint F
			 *
			 */
			if (pNodeL->F) {
				if (pNodeL->F < TINYTREE_NSTART && pNodeR->F < TINYTREE_NSTART) {
					stackL[stackPos] = pNodeL->F;
					stackR[stackPos] = pNodeR->F;
					stackPos++;
				}
			}
			if (pNodeL->T & ~IBIT) {
				if ((pNodeL->T & ~IBIT) < TINYTREE_NSTART && (pNodeR->T & ~IBIT) < TINYTREE_NSTART) {
					stackL[stackPos] = pNodeL->T & ~IBIT;
					stackR[stackPos] = pNodeR->T & ~IBIT;
					stackPos++;
				}
			}
			if (pNodeL->Q) {
				if (pNodeL->Q < TINYTREE_NSTART && pNodeR->Q < TINYTREE_NSTART) {
					stackL[stackPos] = pNodeL->Q;
					stackR[stackPos] = pNodeR->Q;
					stackPos++;
				}
			}
			if (pNodeL->F) {
				if (pNodeL->F >= TINYTREE_NSTART || pNodeR->F >= TINYTREE_NSTART) {
					stackL[stackPos] = pNodeL->F;
					stackR[stackPos] = pNodeR->F;
					stackPos++;
				}
			}
			if (pNodeL->T & ~IBIT) {
				if ((pNodeL->T & ~IBIT) >= TINYTREE_NSTART || (pNodeR->T & ~IBIT) >= TINYTREE_NSTART) {
					stackL[stackPos] = pNodeL->T & ~IBIT;
					stackR[stackPos] = pNodeR->T & ~IBIT;
					stackPos++;
				}
			}
			if (pNodeL->Q) {
				if (pNodeL->Q >= TINYTREE_NSTART || pNodeR->Q >= TINYTREE_NSTART) {
					stackL[stackPos] = pNodeL->Q;
					stackR[stackPos] = pNodeR->Q;
					stackPos++;
				}
			}

		} while (stackPos > 0);

		assert(secondary || lhs == rhs); // secondary==0 implies lhs==rhs

		// identical
		return secondary;
	}

	/**
	 * @date 2020-03-13 19:34:52
	 *
	 * Perform level 1 normalisation on a `"Q,T,F"` triplet and add to the tree only when unique.
	 *
	 * Level 1: Normalisations include: inverting, function grouping
	 * Level 2: dyadic ordering
	 * Level 3: QnTF expanding.
	 *
	 * @param {number} Q
	 * @param {number} T
	 * @param {number} F
	 * @return {number} index into the tree pointing to a node with identical functionality. May have `IBIT` set to indicate that the result is inverted.
	 */
	unsigned addNode(unsigned Q, unsigned T, unsigned F) {

		if (ctx.flags & context_t::MAGICMASK_PARANOID) {
			assert((Q & ~IBIT) < this->count);
			assert((T & ~IBIT) < this->count);
			assert((F & ~IBIT) < this->count);
		}

		/*
		 * Level 1a - Inverts
		 *
		 * ~q ?  t :  f  ->  q ? f : t
		 *  0 ?  t :  f  ->  f
		 *  q ?  t : ~f  ->  ~(q ? ~t : f)
		 */

		if (Q & IBIT) {
			// "~Q?T:F" -> "Q?F:T"
			unsigned savT = T;
			T = F;
			F = savT;
			Q ^= IBIT;
		}
		if (Q == 0) {
			// "0?T:F" -> "F"
			return F;
		}

		// ibit indicates the result should be inverted
		unsigned ibit = 0;

		if (F & IBIT) {
			// "Q?T:~F" -> "~(Q?~T:F)"
			F ^= IBIT;
			T ^= IBIT;
			ibit ^= IBIT;
		}

		/*
		 * Level 1b: Function grouping
		 *
		 * appreciated:
		 *
		 *  [ 0] a ? ~0 : 0  ->  a
		 *  [ 1] a ? ~0 : a  ->  a ? ~0 : 0
		 *  [ 2] a ? ~0 : b                  "+" or
		 *  [ 3] a ? ~a : 0  ->  0
		 *  [ 4] a ? ~a : a  ->  a ? ~a : 0
		 *  [ 5] a ? ~a : b  ->  b ? ~a : b
		 *  [ 6] a ? ~b : 0                  ">" greater-than
		 *  [ 7] a ? ~b : a  ->  a ? ~b : 0
		 *  [ 8] a ? ~b : b                  "^" xor/not-equal
		 *  [ 9] a ? ~b : c                  "!" QnTF
		 *
		 * depreciated:
		 *  [10] a ?  0 : 0 -> 0
		 *  [11] a ?  0 : a -> 0
		 *  [12] a ?  0 : b -> b ? ~a : 0    "<" less-than
		 *  [13] a ?  a : 0 -> a
		 *  [14] a ?  a : a -> a ?  a : 0
		 *  [15] a ?  a : b -> a ? ~0 : b
		 *  [16] a ?  b : 0                  "&" and
		 *  [17] a ?  b : a -> a ?  b : 0
		 *  [18] a ?  b : b -> b
		 *  [19] a ?  b : c                  "?" QTF
		 *
		 * ./eval --raw 'a00!' 'a0a!' 'a0b!' 'aa0!' 'aaa!' 'aab!' 'ab0!' 'aba!' 'abb!' 'abc!' 'a00?' 'a0a?' 'a0b?' 'aa0?' 'aaa?' 'aab?' 'ab0?' 'aba?' 'abb?' 'abc?'
		 */

		if (T & IBIT) {

			if (T == IBIT) {
				if (F == Q || F == 0) {
					// SELF
					// "Q?~0:Q" [1] -> "Q?~0:0" [0] -> Q
					return Q ^ ibit;
				} else {
					// OR
					// "Q?~0:F" [2]
				}
			} else if ((T & ~IBIT) == Q) {
				if (F == Q || F == 0) {
					// ZERO
					// "Q?~Q:Q" [4] -> "Q?~Q:0" [3] -> "0"
					return 0 ^ ibit;
				} else {
					// LESS-THAN
					// "Q?~Q:F" [5] -> "F?~Q:F" -> "F?~Q:0"
					Q = F;
					F = 0;
				}
			} else {
				if (F == Q || F == 0) {
					// GREATER-THAN
					// "Q?~T:Q" [7] -> "Q?~T:0" [6]
					F = 0;
				} else if ((T & ~IBIT) == F) {
					// XOR/NOT-EQUAL
					// "Q?~F:F" [8]
				} else {
					// QnTF
					// "Q?~T:F" [9]
				}
			}

		} else {

			if (T == 0) {
				if (F == Q || F == 0) {
					// ZERO
					// "Q?0:Q" [11] -> "Q?0:0" [10] -> "0"
					return 0 ^ ibit;
				} else {
					// LESS-THAN
					// "Q?0:F" [12] -> "F?~Q:0" [6]
					T = Q ^ IBIT;
					Q = F;
					F = 0;
				}

			} else if (T == Q) {
				if (F == Q || F == 0) {
					// SELF
					// "Q?Q:Q" [14] -> Q?Q:0" [13] -> "Q"
					return Q ^ ibit;
				} else {
					// OR
					// "Q?Q:F" [15] -> "Q?~0:F" [2]
					T = 0 ^ IBIT;
				}
			} else {
				if (F == Q || F == 0) {
					// AND
					// "Q?T:Q" [17] -> "Q?T:0" [16]
					F = 0;
				} else if (T == F) {
					// SELF
					// "Q?F:F" [18] -> "F"
					return F ^ ibit;
				} else {
					// QTF
					// "Q?T:F" [19]
				}
			}
		}

		/*
		 * Level-2 Normalisation, dyadic ordering
		 */

		/*
		 * Reminder:
		 *  [ 2] a ? ~0 : b                  "+" OR
		 *  [ 8] a ? ~b : b                  "^" XOR
		 *  [16] a ?  b : 0                  "&" AND
		 */

		if (T == IBIT) {
			// `OR` ordering
			if (this->compare(Q, *this, F) > 0) {
				// swap
				unsigned savQ = Q;
				Q = F;
				F = savQ;
			}
		}
		if (F == (T ^ IBIT)) {
			// `XOR` ordering
			if (this->compare(Q, *this, F) > 0) {
				// swap
				unsigned savQ = Q;
				Q             = F;
				F             = savQ;
				T             = savQ ^ IBIT;
			}
		}
		if (F == 0 && !(T & IBIT)) {
			// `AND` ordering
			if (this->compare(Q, *this, T) > 0) {
				// swap
				unsigned savQ = Q;
				Q             = T;
				T             = savQ;
			}
		}

		/*
		 * Directly before caching, rewrite `QTF` to `QnTF`
		 *
		 * a ?  b : c -> a?~(a?~b:c):c  "?" QTF
		 *
		 * ./eval --qntf 'ab&' 'abc?'
		 */

		if ((ctx.flags & context_t::MAGICMASK_PURE) && !(T & IBIT)) {
			// QTF
			// Q?T:F -> Q?~(Q?~T:F):F)
			T = addNode(Q, T ^ IBIT, F) ^ IBIT;
		}

		return this->addNormalised(Q, T, F) ^ ibit;
	}

	/*
	 * @date 2020-03-18 18:20:45
	 *
	 * Versioned memory.
	 *
	 * `tinyTree_t` is tuned for speed.
	 * A performance hit is `addNormalised()` which has to search the tree for existing `QTF` combinations.
	 *
	 * `NEND` if <32 and will fit in 5 bits.
	 * A packed `QnTF` (as used by `genpushdata`) is in total (1+5*3=) 16 bits large
	 *
	 * An ultrafast lookup would be a table indexed by the packed `QTF` and containing the matching node id.
	 *
	 * There is an additional problem, it requires this lookup table to be cleared every time the tree is cleared.
	 * A solution is to use versioned memory.
	 *
	 * Versioned memory has an accompanying shadow array containing the version of the current tree incarnation.
	 * Writing memory also updates the matching memory version.
	 * If the memory version matched the incarnation version then the contents is considered.
	 * Otherwise, the contents should be considered the default value, in this case being zero.
	 */

	/**
	 * @date 2020-03-13 19:57:04
	 *
	 * Simple(fast) hash table lookup for nodes
	 *
	 * @param {number} Q
	 * @param {number} T
	 * @param {number} F
	 * @return {number} index into the tree pointing to a node with identical functionality. May have `IBIT` set to indicate that the result is inverted.
	 */
	inline unsigned addNormalised(unsigned Q, unsigned T, unsigned F) {

		// sanity checking
		if (ctx.flags & context_t::MAGICMASK_PARANOID) {
			assert(!(Q & IBIT));                   // Q not inverted
			assert((T & IBIT) || !(ctx.flags & context_t::MAGICMASK_PURE));
			assert(!(F & IBIT));                   // F not inverted
			assert(Q != 0);                        // Q not zero
			assert(T != 0);                        // Q?0:F -> F?!Q:0
			assert(T != IBIT || F != 0);           // Q?!0:0 -> Q
			assert(Q != (T & ~IBIT));              // Q/T collapse
			assert(Q != F);                        // Q/F collapse
			assert(T != F);                        // T/F collapse
			assert((T & ~IBIT) != F || Q < F);     // XOR/NE ordering
			assert(F != 0 || (T & IBIT) || Q < T); // AND ordering
			assert(T != IBIT || Q < F);            // OR ordering
		}

		/*
		 * Perform a lookup to determine if node was already created
		 */
		// test if component already exists
		for (unsigned nid = TINYTREE_NSTART; nid < this->count; nid++) {
			const tinyNode_t *pNode = this->N + nid;
			if (pNode->Q == Q && pNode->T == T && pNode->F == F)
				return nid;
		}

		unsigned nid = this->count++;
		assert(nid < TINYTREE_NEND);

		tinyNode_t *pNode = this->N + nid;

		pNode->Q = Q;
		pNode->T = T;
		pNode->F = F;

		return nid;
	}

	/**
	 * @date 2020-03-14 12:15:22
	 *
	 * decode error codes
	 */
	enum {
		DERR_OK,                // success
		DERR_SYNTAX,            // unknown character in notation
		DERR_PLACEHOLDER,       // placeholder not a lowercase endpoint
		DERR_OVERFLOW,          // stack overflow, might imply too big
		DERR_UNDERFLOW,         // stack underflow, notation not balanced
		DERR_INCOMPLETE,        // notation too short
		DERR_SIZE,              // notation too large for tree
	};

	/**
	 * @date 2020-03-13 21:30:32
	 *
	 * Parse notation and construct tree accordingly.
	 * Notation is assumed to be normalised.
	 *
	 * Do not spend too much effort on detailing errors
	 *
	 * @param {string} pName - The notation describing the tree
	 * @param {string} pSkin - Skin
	 * @return non-zero when parsing failed
	 */
	int loadStringSafe(const char *pName, const char *pSkin = "abcdefghi") {

		// initialise tree
		this->clearTree();

		// state storage for postfix notation
		uint32_t stack[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      stackPos = 0;
		uint32_t beenThere[TINYTREE_NEND]; // track id's of display operators.
		unsigned nextNode = TINYTREE_NSTART; // next visual node

		// walk through the notation until end or until placeholder/skin separator
		for (const char *pCh = pName; *pCh; pCh++) {

			if (isalnum(*pCh) && stackPos >= TINYTREE_MAXSTACK)
				return DERR_OVERFLOW;
			if (!isalnum(*pCh) && count >= TINYTREE_NEND - 1)
				return DERR_SIZE;
			if (islower(*pCh) && !islower(pSkin[*pCh - 'a']))
				return DERR_PLACEHOLDER;

			switch (*pCh) {
			case '0':
				stack[stackPos++] = 0;
				break;
			case 'a':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[0] - 'a');
				break;
			case 'b':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[1] - 'a');
				break;
			case 'c':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[2] - 'a');
				break;
			case 'd':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[3] - 'a');
				break;
			case 'e':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[4] - 'a');
				break;
			case 'f':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[5] - 'a');
				break;
			case 'g':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[6] - 'a');
				break;
			case 'h':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[7] - 'a');
				break;
			case 'i':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[8] - 'a');
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
				// GT (appreciated)
				if (stackPos < 2)
					return DERR_UNDERFLOW;

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = addNode(L, R ^ IBIT, 0);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '+': {
				// OR (appreciated)
				if (stackPos < 2)
					return DERR_UNDERFLOW;

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = addNode(L, 0 ^ IBIT, R);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '^': {
				// XOR/NE (appreciated)
				if (stackPos < 2)
					return DERR_UNDERFLOW;

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = addNode(L, R ^ IBIT, R);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '!': {
				// QnTF (appreciated)
				if (stackPos < 3)
					return DERR_UNDERFLOW;

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid = addNode(Q, T ^ IBIT, F);

				// push
				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '&': {
				// AND (depreciated)
				if (stackPos < 2)
					return DERR_UNDERFLOW;

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = addNode(L, R, 0);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '<': {
				// LT (obsolete)
				if (stackPos < 2)
					return DERR_UNDERFLOW;

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = addNode(L, 0, R);

				stack[stackPos++]     = nid; // push
				beenThere[stackPos++] = nid; // save actual index for back references
				break;
			}
			case '?': {
				// QTF (depreciated)
				if (stackPos < 3)
					return DERR_UNDERFLOW;

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid = addNode(Q, T, F);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '~': {
				// NOT (support)
				if (stackPos < 1)
					return DERR_UNDERFLOW;

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
				return DERR_SYNTAX;
			}
		}

		if (stackPos != 1)
			return DERR_INCOMPLETE;

		// store result into root
		this->root = stack[stackPos - 1];
		return DERR_OK;
	}

	/**
	 * @date 2020-03-13 21:11:04
	 *
	 * Parse notation and construct tree accordingly.
	 * Notation is taken literally and not normalised
	 *
	 * WARNING: Does not check anything
	 *
	 * @param {string} pName - The notation describing the tree
	 * @param {string} pSkin - Skin
	 */
	void loadStringFast(const char *pName, const char *pSkin = "abcdefghi") {

		assert(pName[0]); // disallow empty name

		// initialise tree
		this->clearTree();

		// temporary stack storage for postfix notation
		uint32_t stack[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      stackPos = 0;

		// walk through the notation
		for (const char *pCh = pName; *pCh; pCh++) {

			switch (*pCh) {
			case '0':
				stack[stackPos++] = 0;
				break;
			case 'a':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[0] - 'a');
				break;
			case 'b':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[1] - 'a');
				break;
			case 'c':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[2] - 'a');
				break;
			case 'd':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[3] - 'a');
				break;
			case 'e':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[4] - 'a');
				break;
			case 'f':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[5] - 'a');
				break;
			case 'g':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[6] - 'a');
				break;
			case 'h':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[7] - 'a');
				break;
			case 'i':
				stack[stackPos++] = (unsigned) (TINYTREE_KSTART + pSkin[8] - 'a');
				break;
			case '1':
				stack[stackPos++] = this->count - ('1' - '0');
				break;
			case '2':
				stack[stackPos++] = this->count - ('2' - '0');
				break;
			case '3':
				stack[stackPos++] = this->count - ('3' - '0');
				break;
			case '4':
				stack[stackPos++] = this->count - ('4' - '0');
				break;
			case '5':
				stack[stackPos++] = this->count - ('5' - '0');
				break;
			case '6':
				stack[stackPos++] = this->count - ('6' - '0');
				break;
			case '7':
				stack[stackPos++] = this->count - ('7' - '0');
				break;
			case '8':
				stack[stackPos++] = this->count - ('8' - '0');
				break;
			case '9':
				stack[stackPos++] = this->count - ('9' - '0');
				break;
			case '>': {
				// GT (appreciated)

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = R ^ IBIT;
				this->N[nid].F = 0;

				// push
				stack[stackPos++] = nid;
				break;
			}
			case '+': {
				// OR (appreciated)

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = 0 ^ IBIT;
				this->N[nid].F = R;

				// push
				stack[stackPos++] = nid;
				break;
			}
			case '^': {
				// XOR/NE (appreciated)

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = R ^ IBIT;
				this->N[nid].F = R;

				// push
				stack[stackPos++] = nid;
				break;
			}
			case '!': {
				// QnTF (appreciated)

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid = this->count++;
				this->N[nid].Q = Q;
				this->N[nid].T = T ^ IBIT;
				this->N[nid].F = F;

				// push
				stack[stackPos++] = nid;
				break;
			}
			case '&': {
				// AND (depreciated)

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = R;
				this->N[nid].F = 0;

				// push
				stack[stackPos++] = nid;
				break;
			}
			case '<': {
				// LT (obsolete)

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = 0;
				this->N[nid].F = R;

				// push
				stack[stackPos++] = nid;
				break;
			}
			case '?': {
				// QTF (depreciated)

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid = this->count++;
				this->N[nid].Q = Q;
				this->N[nid].T = T;
				this->N[nid].F = F;

				// push
				stack[stackPos++] = nid;
				break;
			}
			case '~': {
				// NOT (support)

				// invert top-of-stack
				stack[stackPos - 1] ^= IBIT;
				break;
			}
			case '/':
				// skip delimiter

				// store result into root
				this->root = stack[stackPos - 1];
				return;
			}
		}

		// store result into root
		this->root = stack[stackPos - 1];
	}

	/**
	 * @date 2020-03-13 22:12:24
	 *
	 * Encode a notation describing the tree in "placeholder/skin" notation.
	 * Within the placeholders, endpoints are assigned in order of natural path which can be used as index for the skin to determine the actual endpoint.
	 *
	 * @param {number} id - entrypoint
	 * @param {string} pName - The notation describing the tree
	 * @param {string} pSkin - Skin
	 */
	void saveString(unsigned id, char *pName, char *pSkin) const {

		unsigned nameLen  = 0;

		// temporary stack storage for postfix notation
		uint32_t stack[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      stackPos = 0;

		unsigned nextNode;

		uint32_t beenThere = 0;
		uint32_t beenWhat[TINYTREE_NEND];

		if ((id & ~IBIT) < TINYTREE_NSTART) {
			if (pSkin) {
				if ((id & ~IBIT) == 0) {
					pName[nameLen++] = '0';
					pSkin[0]         = 0;
				} else {
					pSkin[0]         = 'a' + (id & ~IBIT) - TINYTREE_KSTART;
					pSkin[1]         = 0;
					pName[nameLen++] = 'a';
				}

			} else {
				if ((id & ~IBIT) == 0) {
					pName[nameLen++] = '0';
				} else {
					pName[nameLen++] = 'a' + (id & ~IBIT) - TINYTREE_KSTART;
				}
			}

			// test for root invert
			if (id & IBIT)
				pName[nameLen++] = '~';

			// terminator
			pName[nameLen] = 0;

			return;
		}

		/*
		 * For skins, walk the tree depth-first to enumerate the placeholders
		 */
		if (pSkin) {
			unsigned numPlaceholder = 0;

			nextNode = TINYTREE_NSTART;

			// mark `zero` processed
			beenThere = (1 << 0);
			beenWhat[0] = 0;

			stackPos = 0;
			stack[stackPos++] = id & ~IBIT;

			do {
				// pop stack
				unsigned curr = stack[--stackPos];

				const tinyNode_t *pNode = this->N + curr;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   To     = pNode->T & ~IBIT;
//				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				// determine if node already handled
				if (!(beenThere & (1 << curr))) {
					/// first time

					// push id so it visits again a second time
					stack[stackPos++] = curr;

					// push unvisited references
					if (F >= TINYTREE_NSTART && !(beenThere & (1 << F)))
						stack[stackPos++] = F;
					if (To != F && To >= TINYTREE_NSTART && !(beenThere & (1 << To)))
						stack[stackPos++] = To;
					if (Q >= TINYTREE_NSTART && !(beenThere & (1 << Q)))
						stack[stackPos++] = Q;

					// done, flag no endpoint assignment done
					beenThere |= (1 << curr);
					beenWhat[curr] = 0;

				} else if (beenWhat[curr] == 0) {
					// node complete, assign placeholders

					if (Q < TINYTREE_NSTART && !(beenThere & (1 << Q))) {
						beenThere |= (1 << Q);
						beenWhat[Q]             = TINYTREE_KSTART + numPlaceholder;
						pSkin[numPlaceholder++] = (char) ('a' + Q - TINYTREE_KSTART);
					}

					if (To < TINYTREE_NSTART && !(beenThere & (1 << To))) {
						beenThere |= (1 << To);
						beenWhat[To]            = TINYTREE_KSTART + numPlaceholder;
						pSkin[numPlaceholder++] = (char) ('a' + To - TINYTREE_KSTART);
					}

					if (F < TINYTREE_NSTART && !(beenThere & (1 << F))) {
						beenThere |= (1 << F);
						beenWhat[F]             = TINYTREE_KSTART + numPlaceholder;
						pSkin[numPlaceholder++] = (char) ('a' + F - TINYTREE_KSTART);
					}

					// flaq endpoints assigned
					beenWhat[curr] = nextNode++;
				}

			} while (stackPos > 0);

			assert(numPlaceholder <= MAXSLOTS);
			pSkin[numPlaceholder] = 0;
		}

		stackPos = 0;
		stack[stackPos++] = id & ~IBIT;

		// re-walk the tree
		nextNode  = TINYTREE_NSTART;
		beenThere = (1 << 0);

		do {
			// pop stack
			unsigned curr = stack[--stackPos];

			// if endpoint then emit
			if (curr < TINYTREE_NSTART) {
				if (curr == 0)
					pName[nameLen++] = '0'; // zero
				else if (!pSkin)
					pName[nameLen++] = 'a' + curr - TINYTREE_KSTART; // endpoint
				else
					pName[nameLen++] = 'a' + beenWhat[curr] - TINYTREE_KSTART; // placeholder

				continue;
			}

			const tinyNode_t *pNode = this->N + curr;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   To     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			// determine if node already handled
			if (!(beenThere & (1 << curr))) {
				/// first time

				// push id so it visits again a second time after expanding
				stack[stackPos++] = curr;

				// push non-zero endpoints
				if (F >= TINYTREE_KSTART)
					stack[stackPos++] = F;
				if (To != F && To >= TINYTREE_KSTART)
					stack[stackPos++] = To;
				if (Q >= TINYTREE_KSTART)
					stack[stackPos++] = Q;

				// done, flag no operator done
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

				// flag operator done
				beenWhat[curr] = nextNode++;

			} else {
				// back-reference to previous opcode

				unsigned backref = nextNode - beenWhat[curr];
				assert(backref <= 9);
				pName[nameLen++] = '0' + backref;

			}

		} while (stackPos > 0);

		// test for inverted-root
		if (id & IBIT)
			pName[nameLen++] = '~';

		// terminator
		assert(nameLen <= TINYTREE_NAMELEN);
		pName[nameLen] = 0;
	}

	/**
	 * @date 2020-03-26 15:02:28
	 *
	 * Simple wrapper with static storage for `saveString()`
	 *
	 * @param {number} id - entrypoint
	 * @param {string} pSkin - optional Skin
	 * @return {string} Constructed notation. static storage so no multiple calls like with `printf()`.
	 */
	const char *saveString(unsigned id, char *pSkin = NULL) const {

		static char staticName[TINYTREE_NAMELEN + 1];

		saveString(id, staticName, pSkin);

		return staticName;
	}

	/*
	 * @date 2021-06-17 20:39:54
	 *
	 * Determine display score (less is better)
	 *  numNodes << 8  | numEndpoint << 4 | numQTF
	 */
	static uint16_t calcScoreName(const char *pName) {
		// fast score calculation
		unsigned score    = 0;

		while (*pName) {
			if (islower(*pName))
				score += 0x010; // numEndpoint
			else if (*pName == '&' || *pName == '?')
				score += 0x101; // numQTF,numNode
			else if (*pName == '^' || *pName == '+' || *pName == '>' || *pName == '!')
				score += 0x100; // numNode

			pName++;
		}

		return score;
	}

	/**
	 * @date 2020-03-09 19:36:17
	 *
         * Evaluate the tree and store the result in v[]
         *
         * `this->N` contains the unified operators.
         * The parameter of this function `v` is the runtime data to which the operators should be applied.
         *
         * For each entry of `this->N[i]` and `v[i]`:
         * - the unified operator loads the operand data from `v` indicated by `Q`, `T` and `F`
         * - functionality is applied
         * - the result is stored int `v` indexed by the location of the operator
         *
         * Each data entry is a 512-bit wide vector, split into uint64_t chunks.
         *
         * @date 2020-04-17 09:19:47
         *
         * The array `v[]` is a list of `MAXSLOT`! trees with pre-determined footprints for all the inputs.
         * The initial footprints are setup as described in `initialiseEvaluator()`
         * Evaluation is performed by first selecting the tree matching the required transform/skin
         * and then starting from the first real node `nstart` executing the unified operator as described in the tree.
         *
         * Example for `MAXSLOTS`=3, `MAXNODES`=2 and tree `"abc+^"`.
         * Derived from these setttings:
         *  `KSTART`=1
         *  `NSTART`=`KSTART`+`MAXSLOTS` = 4
         *  `NEND`=`NSTART`+MAXNODES` = 6
         *
         * To evaluate the above:
         *  - convert the tree to placeholder/skin notation, which is `"cab+^/bca"`
         *  - select the evaluator tree (column) beloning to transform `"bca"`.
         *  - Solve `"ab+"` being `"[KSTART+0] OR [KSTART+1]"` which is stored in `"[NSTART+0]"`
         *  - Solve `"cab+^"` being `"[KSTART+2] XOR [NSTART+0]"` which is stored in `"[NSTART+1]"`
         *
         * Table below shows how the evaluator `v[]` would look like after evaluating the tree above:
         *
         *                         base   <-------------------- ---transform/skin--------------------------->
         *        v[]           |  expr   |    abc   |    bac   |    acb   |    bca   |    cab   |    cba   |
         * ---------------------+---------+----------+----------+----------+----------+----------+----------+
         * N[tid*NEND+0]        |         | 00000000 | 00000000 | 00000000 | 00000000 | 00000000 | 00000000 |
         * N[tid*NEND+KSTART+0] | (a)     | 10101010 | 11001100 | 10101010 | 11001100 | 11110000 | 11110000 |
         * N[tid*NEND+KSTART+1] | (b)     | 11001100 | 10101010 | 11110000 | 11110000 | 10101010 | 11001100 |
         * N[tid*NEND+KSTART+2] | (c)     | 11110000 | 11110000 | 11001100 | 10101010 | 11001100 | 10101010 |
         * N[tid*NEND+NSTART+0] | (ab+)=x |          |          |          | 11111100 |          |          |
         * N[tid*NEND+NSTART+1] | (cx^)=r |          |          |          | 01010110 |          |          |
         *
         * The resulting footprint is "01010110" which can be interpreted as:
         *
         *  c | b | a | answer "abc+^"  |
         * ---+---+---+-----------------+
         *  0 | 0 | 0 |        0        |
         *  0 | 0 | 1 |        1        |
         *  0 | 1 | 0 |        1        |
         *  0 | 1 | 1 |        0        |
         *  1 | 0 | 0 |        1        |
         *  1 | 0 | 1 |        0        |
         *  1 | 1 | 0 |        1        |
         *  1 | 1 | 1 |        0        |
         *
	 * @param {vector[]} v - the evaluated result of the unified operators
	 */
	inline void eval(footprint_t *v) const {

		/*
		 * @date 2020-04-13 18:42:52
		 * Update to SIMD
		 */

#if 0 && defined(__AVX2__)
		/*
		 * 0x263 bytes of code when compiled with -O3 -mavx2
		 */

		// for all operators eligible for evaluation...
		for (unsigned i = TINYTREE_NSTART; i < count; i++) {
			// point to the first chunk of the `"question"`
			const __m256i *Q = (const __m256i *) v[N[i].Q].bits;
			// point to the first chunk of the `"when-true"`
			// NOTE: this can be marked as "value needs be runtime inverted"
			const __m256i *T = (const __m256i *) v[N[i].T & ~IBIT].bits;
			// point to the first chunk of the `"when-false"`
			const __m256i *F = (const __m256i *) v[N[i].F].bits;
			// point to the first chunk of the `"result"`
			__m256i *R = (__m256i *) v[i].bits;

			// determine if the operator is `QTF` or `QnTF`
			if (N[i].T & IBIT) {
				// `QnTF` for each bit in the chunk, apply the operator `"Q ? !T : F"`
				// R[j] = (Q[j] & ~T[j]) ^ (~Q[j] & F[j])
				R[0] = _mm256_xor_si256(_mm256_andnot_si256(T[0], Q[0]), _mm256_andnot_si256(Q[0], F[0]));
				R[1] = _mm256_xor_si256(_mm256_andnot_si256(T[1], Q[1]), _mm256_andnot_si256(Q[1], F[1]));
			} else {
				// `QTF` for each bit in the chunk, apply the operator `"Q ? T : F"`
				// R[j] = (Q[j] & T[j]) ^ (~Q[j] & F[j]);
				R[0] = _mm256_xor_si256(_mm256_and_si256(T[0], Q[0]), _mm256_andnot_si256(Q[0], F[0]));
				R[1] = _mm256_xor_si256(_mm256_and_si256(T[1], Q[1]), _mm256_andnot_si256(Q[1], F[1]));
			}
		}

#elif defined(__SSE2__)
		/*
		 * 0x118 bytes of code when compiled with -O3 -msse2. This is default on x86-64
		 */

		// for all operators eligible for evaluation...
		for (unsigned i = TINYTREE_NSTART; i < count; i++) {
			// point to the first chunk of the `"question"`
			const __m128i *Q = (const __m128i *) v[N[i].Q].bits;
			// point to the first chunk of the `"when-true"`
			// NOTE: this can be marked as "value needs be runtime inverted"
			const __m128i *T = (const __m128i *) v[N[i].T & ~IBIT].bits;
			// point to the first chunk of the `"when-false"`
			const __m128i *F = (const __m128i *) v[N[i].F].bits;
			// point to the first chunk of the `"result"`
			__m128i       *R = (__m128i *) v[i].bits;

			// determine if the operator is `QTF` or `QnTF`
			if (N[i].T & IBIT) {
				// `QnTF` for each bit in the chunk, apply the operator `"Q ? !T : F"`
				// R[j] = (Q[j] & ~T[j]) ^ (~Q[j] & F[j])
				R[0] = _mm_xor_si128(_mm_andnot_si128(T[0], Q[0]), _mm_andnot_si128(Q[0], F[0]));
				R[1] = _mm_xor_si128(_mm_andnot_si128(T[1], Q[1]), _mm_andnot_si128(Q[1], F[1]));
				R[2] = _mm_xor_si128(_mm_andnot_si128(T[2], Q[2]), _mm_andnot_si128(Q[2], F[2]));
				R[3] = _mm_xor_si128(_mm_andnot_si128(T[3], Q[3]), _mm_andnot_si128(Q[3], F[3]));
			} else {
				// `QTF` for each bit in the chunk, apply the operator `"Q ? T : F"`
				// R[j] = (Q[j] & T[j]) ^ (~Q[j] & F[j]);
				R[0] = _mm_xor_si128(_mm_and_si128(T[0], Q[0]), _mm_andnot_si128(Q[0], F[0]));
				R[1] = _mm_xor_si128(_mm_and_si128(T[1], Q[1]), _mm_andnot_si128(Q[1], F[1]));
				R[2] = _mm_xor_si128(_mm_and_si128(T[2], Q[2]), _mm_andnot_si128(Q[2], F[2]));
				R[3] = _mm_xor_si128(_mm_and_si128(T[3], Q[3]), _mm_andnot_si128(Q[3], F[3]));
			}
		}
#elif 0
#warning gcc vectors are inefficient
		/*
		 * 0xa94 bytes of code when compiled with -O3
		 */
		typedef int v512_t __attribute__ ((vector_size (64)));

		// for all operators eligible for evaluation...
		for (unsigned i = TINYTREE_NSTART; i < count; i++) {
			// point to the first chunk of the `"question"`
			const v512_t *Q = (const v512_t *) v[N[i].Q].bits;
			// point to the first chunk of the `"when-true"`
			// NOTE: this can be marked as "value needs be runtime inverted"
			const v512_t *T = (const v512_t *) v[N[i].T & ~IBIT].bits;
			// point to the first chunk of the `"when-false"`
			const v512_t *F = (const v512_t *) v[N[i].F].bits;
			// point to the first chunk of the `"result"`
			v512_t *R = (v512_t *) v[i].bits;

			// determine if the operator is `QTF` or `QnTF`
			if (N[i].T & IBIT) {
				// `QnTF` for each bit in the chunk, apply the operator `"Q ? !T : F"`
				// R[j] = (Q[j] & ~T[j]) ^ (~Q[j] & F[j])
				*R = (*Q & ~*T) ^ (~*Q & *F);
			} else {
				// `QTF` for each bit in the chunk, apply the operator `"Q ? T : F"`
				// R[j] = (Q[j] & T[j]) ^ (~Q[j] & F[j]);
				*R = (*Q & *T) ^ (~*Q & *F);
			}
		}
#else
#warning non-assembler implementation
		/*
		 * 0x208 bytes of code when compiled with -O3
		 */

		// for all operators eligible for evaluation...
		for (unsigned i = TINYTREE_NSTART; i < count; i++) {
			// point to the first chunk of the `"question"`
			const uint64_t *Q = v[N[i].Q].bits;
			// point to the first chunk of the `"when-true"`
			// NOTE: this can be marked as "value needs be runtime inverted"
			const uint64_t *T = v[N[i].T & ~IBIT].bits;
			// point to the first chunk of the `"when-false"`
			const uint64_t *F = v[N[i].F].bits;
			// point to the first chunk of the `"result"`
			uint64_t *R = v[i].bits;

			// determine if the operator is `QTF` or `QnTF`
			if (N[i].T & IBIT) {
				// `QnTF` for each bit in the chunk, apply the operator `"Q ? !T : F"`
				for (unsigned j = 0; j < footprint_t::QUADPERFOOTPRINT; j++)
					R[j] = (Q[j] & ~T[j]) ^ (~Q[j] & F[j]);
			} else {
				// `QTF` for each bit in the chunk, apply the operator `"Q ? T : F"`
				for (unsigned j = 0; j < footprint_t::QUADPERFOOTPRINT; j++)
					R[j] = (Q[j] & T[j]) ^ (~Q[j] & F[j]);
			}
		}
#endif
	}

	/**
	 * @date 2020-03-15 15:39:59
	 *
	 * Create an initial data vector for the evaluator
	 *
	 * During evaluation there are number of states values can possible take.
	 * For expressions with 9 input variables there are `2^9=512` possible value states
	 *
	 * Using 512 bit vectors, it is possible to associate each bit position with a value state.
	 * When evaluating the tree, all 512 bits of the vector can be performed in parallel.
	 *
	 * Example expression stored in data-vector (`v[]`) and tree (`N[]`)
	 *
	 *    index     | v[]        |  N[]
	 * -------------+------------+--------
	 * [0]          | 0b00000000 | null/zero/false
	 * [1=KSTART+0] | 0b10101010 | `a`
	 * [2=KSTART+1] | 0b11001100 | `b`
	 * [3=KSTART+2] | 0b11110000 | `c`
	 * [4=NSTART+0] | 0b10001000 | `ab&`
	 * [5=NSTART+1] | 0b01111000 | `ab&c^`
	 *
	 * In most cases the trees to be evaluated have explicit skins.
	 * This has effect on how input variables are preloaded into the vector.
	 *
	 *    index     | v[]        |  N[]
	 * -------------+------------+--------
	 * [0]          | 0b00000000 | null/zero/false
	 * [1=KSTART+0] | 0b11001100 | `b`
	 * [2=KSTART+1] | 0b11110000 | `c`
	 * [3=KSTART+2] | 0b10101010 | `a`
	 * [4=NSTART+0] | 0b11000000 | `ab&`/bca
	 * [5=NSTART+1] | 0b01101010 | `ab&c^`/bca
	 *
	 * In the above table, placeholders in `ab&` directly index into `v[]`, and skin `/bca` is how endpoint `v[]` are preloaded.
	 *
	 * Encoding the initial state for `v[]` effected by skins is CPU-expensive.
	 *
	 * To optimise this, not one but 9! vectors are preloaded, each with appropiate endpoint values for each transform permutation.
	 *
	 * For `tinyTree_t` with `MAXNODE` set to 10, the size of the complete data vector is:
	 * 	`(sizeof(footprint_t)*(1+MAXSLOTS+MAXNODE)*MAXSLOTS!)`=(64*(1+9+10)*9!)=464 Mbyte.
	 * And there are 2 data vectors, one for forward transforms and a second for reverse.
	 *
	 * @param {footprint_t) pFootprint - footprint to initialise
	 * @param {footprint_t) maxTransform - How many transforms
	 * @param {uint64_t[]) pTransformData - forward or reverse transform data
	 */
	static void initialiseEvaluator(context_t &ctx, footprint_t *pFootprint, unsigned numTransform, uint64_t *pTransformData) {

		// hardcoded assumptions
		assert(MAXSLOTS == 9);

		/*
		 * The patterns and generators have a hardcoded conceptual assumption about the following:
		 */
		uint64_t *v = (uint64_t *) pFootprint;

		// zero everything
		::memset(pFootprint, 0, TINYTREE_NEND * numTransform * sizeof(*pFootprint));

		/*
		 * Initialize the data structures
		 */
		ctx.tick = 0;
		for (unsigned iTrans = 0; iTrans < MAXTRANSFORM; iTrans++) {

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				fprintf(stderr, "\r\e[KinitialiseEvaluator %.5f%%", iTrans * 100.0 / MAXTRANSFORM);
				ctx.tick = 0;
			}

			// set 64bit slice to zero
			for (unsigned i = 0; i < footprint_t::QUADPERFOOTPRINT * TINYTREE_NSTART; i++)
				v[i] = 0;

			// set footprint for 64bit slice
			for (unsigned i = 0; i < (1 << MAXSLOTS); i++) {

				// binary transform name. Each nibble is unique
				uint64_t transformMask = *pTransformData;

				// v[(i/64)+0*4] should be 0
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 1 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 2 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 3 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 4 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 5 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 6 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 7 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 8 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
				transformMask >>= 4;
				if (i & (1LL << (transformMask & 15))) v[(i / 64) + 9 * footprint_t::QUADPERFOOTPRINT] |= 1LL << (i % 64);
			}

			v += footprint_t::QUADPERFOOTPRINT * TINYTREE_NEND;
			pTransformData++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");
	}

};

#endif
