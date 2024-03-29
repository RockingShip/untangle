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

	// OR (L?~0:R)
	inline bool __attribute__((pure)) isOR(void) const {
		return T == IBIT;
	}

	// GT (L?~R:0)
	inline bool __attribute__((pure)) isGT(void) const {
		return (T & IBIT) && F == 0;
	}

	// NE (L?~R:R)
	inline bool __attribute__((pure)) isNE(void) const {
		return (T ^ IBIT) == F;
	}

	// AND (L?R:0)
	inline bool __attribute__((pure)) isAND(void) const {
		return !(T & IBIT) && F == 0;
	}
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
		/*
		 * @date 2021-08-30 15:21:45
		 * With ordered cascading nodes many get orphaned
		 * Raising `TINYTREE_MAXNODES` effects the maximum name length,
		 * All nodes greater than GENERATOR_MAXNODES are needed for cascade expansions.
		 */
#if defined(TINYTREE_MAXNODES_VALUE)		
		/*
		 * @date 2021-09-16 01:05:40
		 * `selftest::performSelfTestCascade()` needs 21 nodes
		 */
		TINYTREE_MAXNODES = TINYTREE_MAXNODES_VALUE,
#else
		/// @constant {number} - Number of nodes. Twice MAXSLOTS because of `QnTF` expansion
		TINYTREE_MAXNODES = 13,
#endif

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

	// OR (L?~0:R)
	inline bool __attribute__((pure)) isOR(uint32_t i) const {
		return i >= TINYTREE_NSTART && N[i].isOR();
	}

	// GT (L?~R:0)
	inline bool __attribute__((pure)) isGT(uint32_t i) const {
		return i >= TINYTREE_NSTART && N[i].isGT();
	}

	// NE (L?~R:R)
	inline bool __attribute__((pure)) isNE(uint32_t i) const {
		return i >= TINYTREE_NSTART && N[i].isNE();
	}

	// AND (L?R:0)
	inline bool __attribute__((pure)) isAND(uint32_t i) const {
		return i >= TINYTREE_NSTART && N[i].isAND();
	}

	// OR (L?~0:R)
	inline bool __attribute__((const)) isOR(uint32_t Q, uint32_t T, uint32_t F) const {
		return T == IBIT;
	}

	// GT (L?~R:0)
	inline bool __attribute__((const)) isGT(uint32_t Q, uint32_t T, uint32_t F) const {
		return (T & IBIT) && F == 0;
	}

	// NE (L?~R:R)
	inline bool __attribute__((const)) isNE(uint32_t Q, uint32_t T, uint32_t F) const {
		return (T ^ IBIT) == F;
	}

	// AND (L?R:0)
	inline bool __attribute__((const)) isAND(uint32_t Q, uint32_t T, uint32_t F) const {
		return !(T & IBIT) && F == 0;
	}

	/*
	 * Types of communicative dyadics/cascades
	 */
	enum { CASCADE_NONE, CASCADE_OR, CASCADE_NE, CASCADE_AND, CASCADE_SYNC };

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
	 * Make compatible with implementation in `evaluate.c`
	 * ordering of dyadics should work now `./slookup --member 'abc!de^^f^'`
	 *
	 * @date 2021-08-30 00:39:38
	 *
	 * To better understand and simplify the code,
	 *   remove everything related to cascade.
	 * Cascades are part of a runtime optimiser.
	 *
	 * @param {number} lhs - entrypoint to right side
	 * @param {number} rhs - entrypoint to right side
	 * @param {boolean} layoutOnly - ignore endpoint values when `true`
	 * @return {number} `<0` if `lhs<rhs`, `0` if `lhs==rhs` and `>0` if `lhs>rhs`
	 */
	int compare(uint32_t lhs, const tinyTree_t *treeR, uint32_t rhs, unsigned topLevelCascade = CASCADE_NONE) {

		/*
		 * @date 2021-09-16 17:41:17
		 * Ignore cascading if disabled
		 */
		if (!(ctx.flags & context_t::MAGICMASK_CASCADE))
			topLevelCascade = CASCADE_NONE;
			
		uint32_t stackL[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		uint32_t stackR[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode

		assert(!(lhs & IBIT));
		assert(!(rhs & IBIT));

		// nodes already processed
		uint32_t beenThereL; // bit set means beenWhatL[] is valid
		uint32_t beenThereR;
		uint32_t beenWhatL[TINYTREE_NEND];
		uint32_t beenWhatR[TINYTREE_NEND];

		// mark `zero` processed
		beenThereL = (1 << 0);
		beenThereR = (1 << 0);
		beenWhatL[0] = 0;
		beenWhatR[0] = 0;

		uint32_t numStackL      = 0; // top of stack
		uint32_t numStackR      = 0; // top of stack
		uint32_t parentCascadeL = CASCADE_NONE; // parent of current cascading node
		uint32_t parentCascadeR = CASCADE_NONE; // parent of current cascading node

		// push arguments on stack
		stackL[numStackL++] = topLevelCascade;
		stackL[numStackL++] = lhs;
		stackR[numStackR++] = topLevelCascade;
		stackR[numStackR++] = rhs;

		do {
			uint32_t         L, R;
			const tinyNode_t *pNodeL, *pNodeR;

			/*
			 * sync left/right to traverse cascade border
			 * unwind node if part of the parent cascade until border reached
			 * This should align cascades. eg: `abc++` and `ab+c+`, `ab+cd++` and `abd++`.
			 */
			for (;;) {
				L              = stackL[--numStackL];
				parentCascadeL = stackL[--numStackL];

				pNodeL = this->N + L;

				if (L < TINYTREE_NSTART) {
					break;
				} else if (parentCascadeL == CASCADE_SYNC) {
					break;
				} else if (parentCascadeL == CASCADE_OR && pNodeL->isOR()) {
					stackL[numStackL++] = parentCascadeL;
					stackL[numStackL++] = pNodeL->F;
					stackL[numStackL++] = parentCascadeL;
					stackL[numStackL++] = pNodeL->Q;
				} else if (parentCascadeL == CASCADE_NE && pNodeL->isNE()) {
					stackL[numStackL++] = parentCascadeL;
					stackL[numStackL++] = pNodeL->F;
					stackL[numStackL++] = parentCascadeL;
					stackL[numStackL++] = pNodeL->Q;
				} else if (parentCascadeL == CASCADE_AND && pNodeL->isAND()) {
					stackL[numStackL++] = parentCascadeL;
					stackL[numStackL++] = pNodeL->T;
					stackL[numStackL++] = parentCascadeL;
					stackL[numStackL++] = pNodeL->Q;
				} else {
					break;
				}
			}
			for (;;) {
				R              = stackR[--numStackR];
				parentCascadeR = stackR[--numStackR];

				pNodeR = treeR->N + R;

				if (R < TINYTREE_NSTART) {
					break;
				} else if (parentCascadeR == CASCADE_SYNC) {
					break;
				} else if (parentCascadeR == CASCADE_OR && pNodeR->isOR()) {
					stackR[numStackR++] = parentCascadeR;
					stackR[numStackR++] = pNodeR->F;
					stackR[numStackR++] = parentCascadeR;
					stackR[numStackR++] = pNodeR->Q;
				} else if (parentCascadeR == CASCADE_NE && pNodeR->isNE()) {
					stackR[numStackR++] = parentCascadeR;
					stackR[numStackR++] = pNodeR->F;
					stackR[numStackR++] = parentCascadeR;
					stackR[numStackR++] = pNodeR->Q;
				} else if (parentCascadeR == CASCADE_AND && pNodeR->isAND()) {
					stackR[numStackR++] = parentCascadeR;
					stackR[numStackR++] = pNodeR->T;
					stackR[numStackR++] = parentCascadeR;
					stackR[numStackR++] = pNodeR->Q;
				} else {
					break;
				}
			}

			/*
			 * Test if cascades are exhausted
			 */
			if (parentCascadeL != parentCascadeR) {
				if (numStackL < numStackR || parentCascadeL == CASCADE_SYNC)
					return -1; // `lhs` exhausted
				if (numStackL > numStackR || parentCascadeR == CASCADE_SYNC)
					return +1; // `rhs` exhausted
				assert(0);
			}

			// for same tree, identical lhs/rhs implies equal
			if (L == R && this == treeR)
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
			if ((beenThereL & (1 << L)) && (beenThereR & (1 << R)) && beenWhatL[L] == R && beenWhatR[R] == L)
				continue; // yes

			beenThereL |= 1 << L;
			beenThereR |= 1 << R;
			beenWhatL[L] = R;
			beenWhatR[R] = L;

			// decode L and R
			pNodeL = this->N + L;
			pNodeR = treeR->N + R;

			/*
			 * Reminder:
			 *  [ 2] a ? ~0 : b                  "+" OR
			 *  [ 6] a ? ~b : 0                  ">" GT
			 *  [ 8] a ? ~b : b                  "^" NE/XOR
			 *  [ 9] a ? ~b : c                  "!" QnTF
			 *  [16] a ?  b : 0                  "&" AND
			 *  [19] a ?  b : c                  "?" QTF
			 */

			/*
			 * compare structure
			 */

			// compare Ti
			if ((pNodeL->T & IBIT) && !(pNodeR->T & IBIT))
				return -1; // `QnTF` < `QTF`
			if (!(pNodeL->T & IBIT) && (pNodeR->T & IBIT))
				return +1; // `QTF` > `QnTF`

			// compare OR
			if (pNodeL->T == IBIT && pNodeR->T != IBIT)
				return -1; // `OR` < !`OR`
			if (pNodeL->T != IBIT && pNodeR->T == IBIT)
				return +1; // !`OR` > `OR`

			// compare GT
			if (pNodeL->F == 0 && pNodeR->F != 0)
				return -1; // `GT` < !`GT` or `AND` < !`AND`
			if (pNodeL->F != 0 && pNodeR->F == 0)
				return +1; // !`GT` > `GT` or !`AND` > `AND`

			// compare NE
			if ((pNodeL->T ^ IBIT) == pNodeL->F && (pNodeR->T ^ IBIT) != pNodeR->F)
				return -1; // `NE` < !`NE`
			if ((pNodeL->T ^ IBIT) != pNodeL->F && (pNodeR->T ^ IBIT) == pNodeR->F)
				return +1; // !`NE` > `NE`

			/*
			 * what is current cascade
			 */
			unsigned thisCascade = CASCADE_NONE;

			if (ctx.flags & context_t::MAGICMASK_CASCADE) {
				if (pNodeL->T & IBIT) {
					if (pNodeL->T == IBIT)
						thisCascade = CASCADE_OR; // OR
					else if ((pNodeL->T ^ IBIT) == pNodeL->F)
						thisCascade = CASCADE_NE; // NE
				} else if (pNodeL->F == 0) {
					thisCascade = CASCADE_AND; // AND
				}
			}

			/*
			 * @date 2021-08-28 19:18:04
			 * Push a sync when starting a new cascade to detect an exausted right-hand-side cascade
			 */
			if (thisCascade != parentCascadeL && thisCascade != CASCADE_NONE) {
				stackL[numStackL++] = CASCADE_SYNC;
				stackL[numStackL++] = 0;
				stackR[numStackR++] = CASCADE_SYNC;
				stackR[numStackR++] = 0;
			}

			/*
			 * Push Q/T/F components for deeper processing
			 * Test if result is cached
			 * 
			 * @date 2021-09-24 13:49:08
			 * Caching will break for "abc!bd^2^a!" and "abc!b1^c1d!!";
			 * caching will swallow the RHS "1",
			 * so when L points to "d", R does not point to "1" but 0 (belonging to CASCADE_SYNC)
			 */
			if (pNodeL->F != 0 && (pNodeL->T & ~IBIT) != pNodeL->F) {
				stackL[numStackL++] = thisCascade;
				stackL[numStackL++] = pNodeL->F;
				stackR[numStackR++] = thisCascade;
				stackR[numStackR++] = pNodeR->F;
			}

			if ((pNodeL->T & ~IBIT) != 0) {
				stackL[numStackL++] = thisCascade;
				stackL[numStackL++] = pNodeL->T & ~IBIT;
				stackR[numStackR++] = thisCascade;
				stackR[numStackR++] = pNodeR->T & ~IBIT;
			}

			{
				stackL[numStackL++] = thisCascade;
				stackL[numStackL++] = pNodeL->Q;
				stackR[numStackR++] = thisCascade;
				stackR[numStackR++] = pNodeR->Q;
			}
		} while (numStackL > 0 && numStackR > 0);

		/*
		 * test if exhausted
		 */
		if (numStackL < numStackR)
			return -1;
		if (numStackL > numStackR)
			return +1;

		return 0;
	}

	/*
	 * @date 2022-01-30 22:18:30
	 * 
	 * This is the improved inverting side-channel normalisation.
	 * All three Q/T/F may be inverted or duplicated.
	 * Dyadics are properly ordered.
	 * The result is normalised with the polarisation (=result is inverted) as return value (0/IBIT)
	 * 
	 * NOTE: this code has been brute-force validated with `selftest.cc::performSelfTestNormaliseQTF()`.
	 */
	static uint32_t normaliseQTF(uint32_t &Q, uint32_t &T, uint32_t &F) {

		const uint32_t I  = IBIT;
		uint32_t       Qu = Q & ~I; // Q with side-channel Invert-bit removed
		uint32_t       Tu = T & ~I; // T with side-channel Invert-bit removed
		uint32_t       Fu = F & ~I; // F with side-channel Invert-bit removed
		uint32_t       Ri; // output polarity

		/*
		 * Friendly reminder:
		 *
		 *   a ? !0 : b   "+" OR            (must be ordered: a<b)
		 *   a ? !b : 0   ">" GREATER-THAN
		 *   a ? !b : b   "^" NOT-EQUAL/XOR (must be ordered: a<b)
		 *   a ? !b : c   "!" QnTF
		 *   a ?  b : 0   "&" AND           (must be ordered: a<b)
		 *   a ?  b : c   "?" QTF
		 */

		/*
		 * "PATH" is the logic that decodes Qu/Qi Tu/Ti Fu/Fi to find the state condition
		 *
		 * "STATE" is the analysis of the imputs, the result after constant folding, and how that maps to the resulting Q/T/F
		 * 
		 * "ACTIONS" is code how to rewrite the arguments.
		 *  Instructions are comma seperated, so it all fits in a single return statement.
		 *  All `QTF` and `QnTF` fall through as their duplicate argument detection is more complicated. 
		 *
		 */

/*                     PATH                          */ /*              STATE                */ /* ACTIONS */
/*---------------------------------------------------*/ /* Q  T  F  -> logical-> Q  T  F  Ri */ /*--------*/

// @formatter:off
if (Q&I) if (Qu) if (T&I) if (Tu) if (F&I) if (Fu)	/* Q~ T~ F~ -> qft?~  -> Q  F  T  I  */         Q&=~I,T=Fu,F=Tu,Ri=I; // fallthrough
else							/* Q~ T~ 0~ -> tq>~   -> T  Q~ 0  I  */  return (Qu==Tu) ? (Q=T=F=0,I) : (T=Q,Q=Tu,F=0,I);
else if (Fu)						/* Q~ T~ F  -> qft!~  -> Q  F~ T  I  */         Q&=~I,T=Fu|I,F=Tu,Ri=I; // fallthrough
else							/* Q~ T~ 0  -> qt+~   -> Q  0~ T  I  */  return (Qu==Tu) ? (Q=T=F=Qu,I) : (Qu>Tu) ? (Q=Tu,T=I,F=Qu,I) : (Q&=~I,T=I,F=Tu,I);
else if (F&I) if (Fu)					/* Q~ 0~ F~ -> fq&~   -> Q  F  0  I  */  return (Qu==Fu) ? (Q=T=F=Qu,I) : (Qu>Fu) ? (Q=Fu,T=Qu,F=0,Ri=I) : (Q&=~I,T=Fu,F=0,Ri=I);
else							/* Q~ 0~ 0~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Fu)						/* Q~ 0~ F  -> qf>~   -> Q  F~ 0  I  */  return (Qu==Fu) ? (Q=T=F=0,I) : (Q&=~I,T=Fu|I,F=0,I);
else							/* Q~ 0~ 0  -> q~     -> Q  Q  Q  I  */  return Q=T=F=Qu,I;
else if (Tu) if (F&I) if (Fu) 				/* Q~ T  F~ -> qft!   -> Q  F~ T  0  */         Q&=~I,T=Fu|I,F=Tu,Ri=0; // fallthrough
else							/* Q~ T  0~ -> qt+    -> Q  0~ T  0  */  return (Qu==Tu) ? (Q=T=F=Qu,0) : (Qu>Tu) ? (Q=Tu,T=I,F=Qu,0) : (Q&=~I,T=I,F=Tu,0);
else if (Fu)						/* Q~ T  F  -> qft?   -> Q  F  T  0  */         Q&=~I,T=Fu,F=Tu,Ri=0; // fallthrough
else							/* Q~ T  0  -> tq>    -> T  Q~ 0  0  */  return (Qu==Tu) ? (Q=T=F=0,0) : (T=Q,Q=Tu,F=0,0);
else if (F&I) if (Fu)					/* Q~ 0  F~ -> qf>    -> Q  F~ 0  0  */  return (Qu==Fu) ? (Q=T=F=0,0) : (Q&=~I,T=Fu|I,F=0,0);
else							/* Q~ 0  0~ -> q      -> Q  Q  Q  0  */  return Q=T=F=Qu,0;
else if (Fu)						/* Q~ 0  F  -> fq&    -> Q  F  0  0  */  return (Qu==Fu) ? (Q=T=F=Qu,0) : (Qu>Fu) ? (Q=Fu,T=Qu,F=0,Ri=0) : (Q&=~I,T=Fu,F=0,Ri=0);
else							/* Q~ 0  0  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else if (T&I) if (Tu) if (F&I) if (Fu)			/* 0~ T~ F~ -> t~     -> T  T  T  I  */  return Q=T=F=Tu,I;
else							/* 0~ T~ 0~ -> t~     -> T  T  T  I  */  return Q=T=F=Tu,I;
else if (Fu)						/* 0~ T~ F  -> t~     -> T  T  T  I  */  return Q=T=F=Tu,I;
else							/* 0~ T~ 0  -> t~     -> T  T  T  I  */  return Q=T=F=Tu,I;
else if (F&I) if (Fu)					/* 0~ 0~ F~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else							/* 0~ 0~ 0~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Fu)						/* 0~ 0~ F  -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else							/* 0~ 0~ 0  -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Tu) if (F&I) if (Fu)				/* 0~ T  F~ -> t      -> T  T  T  0  */  return Q=T=F=Tu,0;
else							/* 0~ T  0~ -> t      -> T  T  T  0  */  return Q=T=F=Tu,0;
else if (Fu)						/* 0~ T  F  -> t      -> T  T  T  0  */  return Q=T=F=Tu,0;
else							/* 0~ T  0  -> t      -> T  T  T  0  */  return Q=T=F=Tu,0;
else if (F&I) if (Fu)					/* 0~ 0  F~ -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else							/* 0~ 0  0~ -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else if (Fu)						/* 0~ 0  F  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else							/* 0~ 0  0  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else if (Qu) if (T&I) if (Tu) if (F&I) if (Fu)		/* Q  T~ F~ -> qtf?~  -> Q  T  F  I  */         T&=~I,F=Fu,Ri=I; // fallthrough
else							/* Q  T~ 0~ -> qt&~   -> Q  T  0  I  */  return (Qu==Tu) ? (Q=T=F=Tu,I) : (Qu>Tu) ? (Q=Tu,T=Qu,F=0,Ri=I) : (T=Tu,F=0,Ri=I);
else if (Fu)						/* Q  T~ F  -> qtf!   -> Q  T~ F  0  */         Ri=0; // fallthrough
else							/* Q  T~ 0  -> qt>    -> Q  T~ 0  0  */  return (Qu==Tu) ? (Q=T=F=0,0) : (Q&=~I,F=0,0);
else if (F&I) if (Fu)					/* Q  0~ F~ -> fq>~   -> F  Q~ 0  I  */  return (Qu==Fu) ? (Q=T=F=0,I) : (Q=Fu,T=Qu|I,F=0,I);
else							/* Q  0~ 0~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Fu)						/* Q  0~ F  -> fq+    -> Q  0~ F  0  */  return (Qu==Fu) ? (Q=T=F=Qu,0) : (Qu>Fu) ? (Q=Fu,T=I,F=Qu,0) : (Q&=~I,T=I,F=Fu,0);
else							/* Q  0~ 0  -> q      -> Q  Q  Q  0  */  return Q=T=F=Qu,0;
else if (Tu) if (F&I) if (Fu)				/* Q  T  F~ -> qtf!~  -> Q  T~ F  I  */         T|=I,F=Fu,Ri=I; // fallthrough
else							/* Q  T  0~ -> qt>~   -> Q  T~ 0  I  */  return (Qu==Tu) ? (Q=T=F=0,I) : (T|=I,F=0,I);
else if (Fu)						/* Q  T  F  -> qtf?   -> Q  T  F  0  */         Ri=0; // fallthrough
else							/* Q  T  0  -> qt&    -> Q  T  0  0  */  return (Qu==Tu) ? (Q=T=F=Tu,0) : (Qu>Tu) ? (Q=Tu,T=Qu,F=0,Ri=0) : (T=Tu,F=0,Ri=0);
else if (F&I) if (Fu)					/* Q  0  F~ -> fq+~   -> Q  0~ F  I  */  return (Qu==Fu) ? (Q=T=F=Qu,I) : (Qu>Fu) ? (Q=Fu,T=I,F=Qu,I) : (Q&=~I,T=I,F=Fu,I);
else							/* Q  0  0~ -> q~     -> Q  Q  Q  I  */  return Q=T=F=Qu,I;
else if (Fu)						/* Q  0  F  -> fq>    -> F  Q~ 0  0  */  return (Qu==Fu) ? (Q=T=F=0,0) : (Q=F,T=Qu|I,F=0,0);
else							/* Q  0  0  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else if (T&I) if (Tu) if (F&I) if (Fu)			/* 0  T~ F~ -> f~     -> F  F  F  I  */  return Q=T=F=Fu,I;
else							/* 0  T~ 0~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Fu)						/* 0  T~ F  -> f      -> F  F  F  0  */  return Q=T=F=Fu,0;
else							/* 0  T~ 0  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else if (F&I) if (Fu)					/* 0  0~ F~ -> f~     -> F  F  F  I  */  return Q=T=F=Fu,I;
else							/* 0  0~ 0~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Fu)						/* 0  0~ F  -> f      -> F  F  F  0  */  return Q=T=F=Fu,0;
else							/* 0  0~ 0  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else if (Tu) if (F&I) if (Fu)				/* 0  T  F~ -> f~     -> F  F  F  I  */  return Q=T=F=Fu,I;
else							/* 0  T  0~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Fu)						/* 0  T  F  -> f      -> F  F  F  0  */  return Q=T=F=Fu,0;
else							/* 0  T  0  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
else if (F&I) if (Fu)					/* 0  0  F~ -> f~     -> F  F  F  I  */  return Q=T=F=Fu,I;
else							/* 0  0  0~ -> 0~     -> 0  0  0  I  */  return Q=T=F=0,I;
else if (Fu)						/* 0  0  F  -> f      -> F  F  F  0  */  return Q=T=F=Fu,0;
else							/* 0  0  0  -> 0      -> 0  0  0  0  */  return Q=T=F=0,0;
// @formatter:on

		/*
		 * Duplicate argument detection
		 */

		// @formatter:off
		if (T & I) {
			const uint32_t Tu = T & ~IBIT; // shadow (now outdated) declaration

			// QnTF

			if (Q == F) {
				if (Q == Tu)
					return Q=T=F=0,Ri; // qqq! -> 0
				else
					return F=0,Ri;   // qtq! -> qt>
			} else if (Q == Tu) {
				return Q=F,F=0,Ri; // qqf! -> fq>
			} else if (Tu == F) {
				if (Q > F)
					return F=Q,Q=Tu,T=F|I,Ri; // qff! -> fq^
				else
					return Ri; // qff! -> qf^
			} else {
				return Ri; // qtf!
			}

		} else {

			// QTF

			if (Q == F) {
				if (Q == T)
					return T=F=Q,Ri; // qqq? -> q
				else if (Q > T)
					return Q=T,T=F,F=0,Ri;   // qtq? -> tq&
				else
					return F=0,Ri;   // qtq? -> qt&
			} else if (Q == T) {
				if (Q > F)
					return Q=F,F=T,T=I,Ri; // qqf? -> fq+
				else
					return T=I,Ri; // qqf? -> qf+
			} else if (T == F) {
				return Q=T=F,Ri; // qff? -> f
			} else {
				return Ri; // qtf?
			}

		}
		// @formatter:on

	}

	/**
	 * @date 2020-03-13 19:57:04
	 *
	 * Simple(fast) hash table lookup for nodes
	 *
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @return {number} newly created nodeId
	 */
	inline uint32_t addBasicNode(uint32_t Q, uint32_t T, uint32_t F) {

		/*
		 * @date 2021-09-15 22:12:26
		 * `cascadeQTF()` can return endpoints and marks them by `Q=F=endpoint` 
		 */
		if (Q == F) {
			if (ctx.flags & context_t::MAGICMASK_PARANOID) {
				assert(Q == T);
			}

			return Q;
		}

		// sanity checking
		if (ctx.flags & context_t::MAGICMASK_PARANOID) {
			assert(!(Q & IBIT));                   // Q not inverted
			assert((T & IBIT) || !(ctx.flags & context_t::MAGICMASK_PURE));
			assert(!(F & IBIT));                   // F not inverted
			assert(Q != 0);                        // Q not zero
			assert(T != 0);                        // Q?0:F -> F?!Q:0
			assert(T != IBIT || F != 0);           // Q?!0:0 -> Q
			assert(Q != (T & ~IBIT));              // Q/T fold
			assert(Q != F);                        // Q/F fold
			assert(T != F);                        // T/F fold

			if (ctx.flags & context_t::MAGICMASK_CASCADE) {
				if (this->isOR(Q, T, F)) {
					assert(!this->isOR(F));
					assert(compare(Q, this, F, CASCADE_OR) < 0);
				}
				if (this->isNE(Q, T, F)) {
					assert(!this->isNE(F));
					assert(compare(Q, this, F, CASCADE_NE) < 0);
				}
				if (this->isAND(Q, T, F)) {
					assert(!this->isAND(T));
					assert(compare(Q, this, T, CASCADE_AND) < 0);
				}
			}
		}

		/*
		 * Perform a lookup to determine if node was already created
		 */
		// test if node already exists
		for (unsigned nid = TINYTREE_NSTART; nid < this->count; nid++) {
			const tinyNode_t *pNode = this->N + nid;
			if (pNode->Q == Q && pNode->T == T && pNode->F == F)
				return nid;
		}

		uint32_t nid = this->count++;
		assert(nid < TINYTREE_NEND);

		tinyNode_t *pNode = this->N + nid;

		pNode->Q = Q;
		pNode->T = T;
		pNode->F = F;

		return nid;
	}

	/**
	 * @date 2021-08-13 13:44:17
	 *
	 * Apply communicative dyadics ordering on a low level.
	 * Cascades are left-hand-size only
	 *   left+right made code highly complex
	 *   right-hand-side was open-range
	 * with LHS, all the cascaded left hand terms are less than the right hand term
	 * drawback, reduced detector range.
	 *
	 * Important NOTE (only relevant for right-hand-side cascading):
	 * The structure "dcab^^^" will cause oscillations.
	 * Say that this is the top of a longer cascading chain, then `b` is also a "^".
	 * Within the current detect span ("dcab^^^"), it is likely that `b` and `d` will swap positions.
	 * The expanded resulting structure will look like "xy^cad^^^", who's head is "xy^cz^^". (`b`="xy^",`z`="ad^")
	 * This new head would trigger a rewrite to "zcxy^^^" making the cycle complete.
	 *
	 * @date 2021-08-19 11:08:05
	 * All structures below top-level are ordered
	 *
	 * @date 2021-08-23 23:21:35
	 * cascades make it complex because endpoints can be placeholders for deeper cascading.
	 * In an attempt to simplify logic, let only the left hand side continue cascading, and the lowest (oldest) values located deeper.
	 * Example `ab+c+d+`
	 * This also makes the names more naturally readable.
	 * This also means that for `ab+c+', if `b` continues the cascade, the terms will all be "less than" `c`.
	 * Would the right hand side continue, then there is no indicator of highest value in th deeper levels.
	 *
	 * @date 2021-09-12 22:46:15
	 * Q/T/F are assumed trivial normalised
	 * return value might fold and return as `Q=T=F`. However,`IBIT` is never set.
	 * return value only sensible when called for OR/NE/AND 
	 * 
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 */
	bool cascadeQTF(uint32_t *Q, uint32_t *T, uint32_t *F) {

		/*
		 * Special cases folding may introduce
		 * 
		 * Reminder:
		 *  [ 2] a ? ~0 : b                  "+" OR
		 *  [ 6] a ? ~b : 0                  ">" GT
		 *  [ 8] a ? ~b : b                  "^" NE
		 *  [ 9] a ? ~b : c                  "!" QnTF
		 *  [16] a ?  b : 0                  "&" AND
		 *  [19] a ?  b : c                  "?" QTF
		 */

		if (*Q == 0) {
			// "0?T:F" -> "F" (fold)
			*Q = *T = *F;
			return true;
		} else if (*Q == *F) {
			// "Q=T=F" -> "F" (endpoint)
			*Q = *T = *F;
			return true;
		} else if (*T == *F) {
			// "Q?F:F" -> "F" (fold)
			*Q = *T = *F;
			return true;
		} else if (*Q == *T) {
			// "Q?Q:F" -> "Q?~0:F" (OR)
			*T = IBIT;
			cascadeQTF(Q, T, F);
			return true;
		} else if (*Q == (*T ^ IBIT)) {
			// "Q?~Q:F" -> "Q?0:F" -> "F?~Q:0" (GT)
			*T = *Q ^ IBIT;
			*Q = *F;
			*F = 0;
			cascadeQTF(Q, T, F);
			return true;
		}

		// OR (L?~0:R)
		if (this->isOR(*Q, *T, *F)) {
			if (this->isOR(*Q) && this->isOR(*F)) {
				// AB+CD++
				uint32_t AB = *Q; // may cascade
				uint32_t CD = *F; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // may cascade
				uint32_t C  = this->N[CD].Q; // does not cascade
				uint32_t D  = this->N[CD].F; // does not cascade

				if (A == CD) {
					*Q = *T = *F = AB;
					return true;
				} else if (B == CD) {
					/*
					 * @date 2021-09-16 15:57:46
					 * CD cascades which implies that B also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = AB;
					return true;
				} else if (C == AB) {
					*Q = *T = *F = CD;
					return true;
				} else if (D == AB) {
					/*
					 * @date 2021-09-16 15:57:46
					 * AB cascades which implies that D also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = CD;
					return true;
				}

				if (A == C) {
					if (B == D) {
						// A=C<B=D
						/*
						 * @date 2021-09-16 00:21:52
						 * This implies that `Q==F` which has already ben tested
						 */
						assert(0);
						*Q = *T = *F = AB;
						return true;
					} else if (compare(B, this, D, CASCADE_OR) < 0) {
						// C=A<B<D
						// already *Q = AB;
						// already *T = IBIT;
						*F = D;
						return false; // remains OR
					} else {
						// A=C<D<B
						*Q = CD;
						// already *T = IBIT;
						*F = B;
						return false; // remains OR
					}
				} else if (A == D) {
					// C<D=A<B
					*Q = CD;
					// already *T = IBIT;
					*F = B;
					return false; // remains OR
				} else if (B == C) {
					// A<B=C<D
					// already *Q = AB;
					// already *T = IBIT;
					*F = D;
					return false; // remains OR
				} else if (B == D) {
					// A<C<D=B or C<A<B=D
					// A and C can cascade, neither will exceed B/D
					if (this->isOR(A) || this->isOR(C)) {
						// cascades, merge AC, B last
						*Q = A;
						// already *T = IBIT;
						*F = C;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							*T = IBIT;
							*F = B;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							// already *T = IBIT;
							*F = B;
							return false; // remains OR
						}
					} else if (compare(A, this, C, CASCADE_OR) < 0) {
						// A<C<B
						*Q = addBasicNode(A, IBIT, C);
						// already *T = IBIT;
						*F = B;
						return false; // remains OR
					} else {
						// C<A<B
						*Q = addBasicNode(C, IBIT, A);
						// already *T = IBIT;
						*F = B;
						return false; // remains OR
					}
				}

				/*
				 * 4! permutations where A<B and C<D has 6 candidates
				 */
				if (compare(B, this, C, CASCADE_OR) < 0) {
					// A<B<C<D
					if (this->isOR(C)) {
						// C cascades and unusable for right-hand-side
						// merge ABC, D last
						*Q = AB;
						// already *T = IBIT;
						*F = C;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, D no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addBasicNode(*Q, *T, *F);
							*T = IBIT;
							*F = D;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							// already *T = IBIT;
							*F = D;
							return false; // remains OR
						}
					} else {
						// simple rewrite
						*Q = addBasicNode(AB, IBIT, C);
						// already *T = IBIT;
						*F = D;
						return false; // remains OR
					}
				} else if (compare(D, this, A, CASCADE_OR) < 0) {
					// C<D<A<B
					if (this->isOR(A)) {
						// A cascades and unusable for right-hand-side
						// merge CDA, B last
						*Q = CD;
						// already *T = IBIT;
						*F = A;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addBasicNode(*Q, *T, *F);
							*T = IBIT;
							*F = B;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							// already *T = IBIT;
							*F = B;
							return false; // remains OR
						}
					} else {
						// simple rewrite
						*Q = addBasicNode(CD, IBIT, A);
						// already *T = IBIT;
						*F = B;
						return false; // remains OR
					}
				} else if (compare(B, this, D, CASCADE_OR) < 0) {
					// A<C<B<D or C<A<B<D
					// merge ABC, D last
					// already *Q = AB;
					// already *T = IBIT;
					*F = C;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, D no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = IBIT;
						*F = D;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						// already *T = IBIT;
						*F = D;
						return false; // remains OR
					}
				} else {
					// A<C<D<B or C<A<D<B
					// merge ACD, B last
					*Q = A;
					// already *T = IBIT;
					// already *F = CD;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = IBIT;
						*F = B;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						// already *T = IBIT;
						*F = B;
						return false; // remains OR
					}
				}

			} else if (this->isOR(*Q)) {
				// AB+C+
				uint32_t AB = *Q; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // does not cascade
				uint32_t C  = *F; // may cascade

				assert(!this->isOR(B));
				assert(!this->isOR(C));

				if (C == A) {
					// C=A<B
					*Q = *T = *F = AB;
					return true;
				} else if (C == B) {
					// A<B=C
					*Q = *T = *F = AB;
					return true;
				}

				/*
				 * 3! permutations where A<B has 3 candidates
				 */
				if (compare(B, this, C, CASCADE_OR) < 0) {
					// A<B<C
					// natural order
					// already *Q = AB;
					// already *T = IBIT;
					// already *F = C;
					return false;
				} else if (this->isOR(A)) {
					// cascade, merge AC, B last
					*Q = A;
					// already *T = IBIT;
					// already *F = C;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = IBIT;
						*F = B;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						// already *T = IBIT;
						*F = B;
						return false; // remains OR
					}
				} else if (compare(A, this, C, CASCADE_OR) < 0) {
					// A<C<B
					*Q = addBasicNode(A, IBIT, C);
					// already *T = IBIT;
					*F = B;
					return false; // remains OR
				} else {
					// C<A<B
					*Q = addBasicNode(C, IBIT, A);
					// already *T = IBIT;
					*F = B;
					return false; // remains OR
				}

			} else if (this->isOR(*F)) {
				// ABC++
				uint32_t BC = *F; // may cascade
				uint32_t A  = *Q; // does not cascade
				uint32_t B  = this->N[BC].Q; // may cascade
				uint32_t C  = this->N[BC].F; // does not cascade

				assert (!this->isOR(A));
				assert (!this->isOR(C));

				if (A == B) {
					// A=B<C
					*Q = *T = *F = BC;
					return true;
				} else if (A == C) {
					// B<C=A
					*Q = *T = *F = BC;
					return true;
				}

				/*
				 * 3! permutations where B<C has 3 candidates
				 */
				if (compare(C, this, A, CASCADE_OR) < 0) {
					// B<C<A
					*Q = BC;
					// already *T = IBIT;
					*F = A;
					return false; // remains OR
				} else if (this->isOR(B)) {
					// cascade, merge AB, C last
					// already *Q = A;
					// already *T = IBIT;
					*F = B;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, C no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = IBIT;
						*F = C;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						// already *T = IBIT;
						*F = C;
						return false; // remains OR
					}
				} else if (compare(A, this, B, CASCADE_OR) < 0) {
					// A<B<C
					*Q = addBasicNode(A, IBIT, B);
					// already *T = IBIT;
					*F = C;
					return false; // remains OR
				} else {
					// B<A<C
					*Q = addBasicNode(B, IBIT, A);
					// already *T = IBIT;
					*F = C;
					return false; // remains OR
				}

			} else if (compare(*F, this, *Q, CASCADE_OR) < 0) {
				// swap
				uint32_t savQ = *Q;
				*Q = *F;
				// already *T = IBIT;
				*F = savQ;
				return false; // remains OR
			} else {
				// no change
				return false;
			}
		}

		// NE (L?~R:R)
		if (this->isNE(*Q, *T, *F)) {
			if (this->isNE(*Q) && this->isNE(*F)) {
				// AB^CD^^
				uint32_t AB = *Q; // may cascade
				uint32_t CD = *F; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // may cascade
				uint32_t C  = this->N[CD].Q; // does not cascade
				uint32_t D  = this->N[CD].F; // does not cascade

				if (A == CD) {
					*Q = *T = *F = B;
					return true;
				} else if (B == CD) {
					/*
					 * @date 2021-09-16 15:57:46
					 * CD cascades which implies that B also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = A;
					return true;
				} else if (C == AB) {
					*Q = *T = *F = D;
					return true;
				} else if (D == AB) {
					/*
					 * @date 2021-09-16 15:57:46
					 * AB cascades which implies that D also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = C;
					return true;
				}

				if (A == C) {
					if (B == D) {
						// A=C<B=D
						/*
						 * @date 2021-09-16 00:21:52
						 * This implies that `Q==F` which has already been tested
						 */
						assert(0);
						*Q = *T = *F = 0;
						return true;
					} else if (compare(B, this, D, CASCADE_NE) < 0) {
						// C=A<B<D
						*Q = B;
						*T = D ^ IBIT;
						*F = D;
						return false; // remains NE
					} else {
						// A=C<D<B
						*Q = D;
						*T = B ^ IBIT;
						*F = B;
						return false; // remains NE
					}
				} else if (A == D) {
					// C<D=A<B
					*Q = C;
					*T = B ^ IBIT;
					*F = B;
					return false; // remains NE
				} else if (B == C) {
					// A<B=C<D
					*Q = A;
					*T = D ^ IBIT;
					*F = D;
					return false; // remains NE
				} else if (B == D) {
					// A<C<D=B or C<A<B=D
					if (this->isNE(A) || this->isNE(C)) {
						*Q = A;
						*T = C ^ IBIT;
						*F = C;
						return cascadeQTF(Q, T, F);
					} else if (compare(A, this, C, CASCADE_NE) < 0) {
						// A<C
						*Q = A;
						*T = C ^ IBIT;
						*F = C;
						return false; // remains NE
					} else {
						// C<A
						*Q = C;
						*T = A ^ IBIT;
						*F = A;
						return false; // remains NE
					}
				}

				/*
				 * 4! permutations where A<B and C<D has 6 candidates
				 */
				if (compare(B, this, C, CASCADE_NE) < 0) {
					// A<B<C<D
					if (this->isNE(C)) {
						// C cascades and unusable for right-hand-side
						// merge ABC, D last
						*Q = AB;
						*T = C ^ IBIT;
						*F = C;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, D no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addBasicNode(*Q, *T, *F);
							*T = D ^ IBIT;
							*F = D;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							*T = D ^ IBIT;
							*F = D;
							return false; // remains NE
						}
					} else {
						// simple rewrite
						*Q = addBasicNode(AB, C ^ IBIT, C);
						*T = D ^ IBIT;
						*F = D;
						return false; // remains NE
					}
				} else if (compare(D, this, A, CASCADE_NE) < 0) {
					// C<D<A<B
					if (this->isNE(A)) {
						// A cascades and unusable for right-hand-side
						// merge CDA, B last
						*Q = CD;
						*T = A ^ IBIT;
						*F = A;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addBasicNode(*Q, *T, *F);
							*T = B ^ IBIT;
							*F = B;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							*T = B ^ IBIT;
							*F = B;
							return false; // remains NE
						}
					} else {
						// simple rewrite
						*Q = addBasicNode(CD, A ^ IBIT, A);
						*T = B ^ IBIT;
						*F = B;
						return false; // remains NE
					}
				} else if (compare(B, this, D, CASCADE_NE) < 0) {
					// A<C<B<D or C<A<B<D
					// merge ABC, D last
					// already *Q = AB;
					*T = C ^ IBIT;
					*F = C;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, D no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = D ^ IBIT;
						*F = D;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = D ^ IBIT;
						*F = D;
						return false; // remains NE
					}
				} else {
					// A<C<D<B or C<A<D<B
					// merge ACD, B last
					*Q = A;
					// already *T = CD ^ IBIT;
					// already *F = CD;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B ^ IBIT;
						*F = B;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B ^ IBIT;
						*F = B;
						return false; // remains NE
					}
				}

			} else if (this->isNE(*Q)) {
				// AB^C^
				uint32_t AB = *Q; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // does not cascade
				uint32_t C  = *F; // may cascade

				assert(!this->isNE(B));
				assert(!this->isNE(C));

				if (C == A) {
					// C=A<B
					*Q = *T = *F = B;
					return true;
				} else if (C == B) {
					// A<B=C
					*Q = *T = *F = A;
					return true;
				}

				/*
				 * 3! permutations where A<B has 3 candidates
				 */
				if (compare(B, this, *F, CASCADE_NE) < 0) {
					// A<B<C
					// natural order
					// already *Q = AB;
					// already *T = C ^ IBIT;
					// already *F = C;
					return false;
				} else if (this->isNE(A)) {
					// cascade, merge AC, B last
					*Q = A;
					// already *T = C ^ IBIT;
					// already *F = C;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B ^ IBIT;
						*F = B;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B ^ IBIT;
						*F = B;
						return false; // remains NE
					}
				} else if (compare(A, this, C, CASCADE_NE) < 0) {
					// A<C<B
					*Q = addBasicNode(A, C ^ IBIT, C);
					*T = B ^ IBIT;
					*F = B;
					return false; // remains NE
				} else {
					// C<A<B
					*Q = addBasicNode(C, A ^ IBIT, A);
					*T = B ^ IBIT;
					*F = B;
					return false; // remains NE
				}

			} else if (this->isNE(*F)) {
				// ABC^^
				uint32_t BC = *F; // may cascade
				uint32_t A  = *Q; // does not cascade
				uint32_t B  = this->N[BC].Q; // may cascade
				uint32_t C  = this->N[BC].F; // does not cascade

				assert (!this->isNE(A));
				assert (!this->isNE(C));

				if (A == B) {
					// A=B<C
					*Q = *T = *F = C;
					return true;
				} else if (A == C) {
					// B<C=A
					*Q = *T = *F = B;
					return true;
				}

				/*
				 * 3! permutations where B<C has 3 candidates
				 */
				if (compare(C, this, A, CASCADE_NE) < 0) {
					// B<C<A
					*Q = BC;
					*T = A ^ IBIT;
					*F = A;
					return false; // remains NE
				} else if (this->isNE(B)) {
					// cascade, merge AB, C last
					// already *Q = A;
					*T = B ^ IBIT;
					*F = B;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, C no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = C ^ IBIT;
						*F = C;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = C ^ IBIT;
						*F = C;
						return false; // remains NE
					}
				} else if (compare(A, this, B, CASCADE_NE) < 0) {
					// A<B<C
					*Q = addBasicNode(A, B ^ IBIT, B);
					*T = C ^ IBIT;
					*F = C;
					return false; // remains NE
				} else {
					// B<A<C
					*Q = addBasicNode(B, A ^ IBIT, A);
					*T = C ^ IBIT;
					*F = C;
					return false; // remains NE
				}

			} else if (compare(*F, this, *Q, CASCADE_NE) < 0) {
				// swap
				uint32_t savQ = *Q;
				*Q = *F;
				*T = savQ ^ IBIT;
				*F = savQ;
				return false; // remains NE
			} else {
				// no change
				return false;
			}
		}

		// AND (L?R:0)
		if (this->isAND(*Q, *T, *F)) {
			if (this->isAND(*Q) && this->isAND(*T)) {
				// AB&CD&&
				uint32_t AB = *Q; // may cascade
				uint32_t CD = *T; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].T; // may cascade
				uint32_t C  = this->N[CD].Q; // does not cascade
				uint32_t D  = this->N[CD].T; // does not cascade

				if (A == CD) {
					*Q = *T = *F = AB;
					return true;
				} else if (B == CD) {
					/*
					 * @date 2021-09-16 15:57:46
					 * CD cascades which implies that B also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = AB;
					return true;
				} else if (C == AB) {
					*Q = *T = *F = CD;
					return true;
				} else if (D == AB) {
					/*
					 * @date 2021-09-16 15:57:46
					 * AB cascades which implies that D also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = CD;
					return true;
				}

				if (A == C) {
					if (B == D) {
						// A=C<B=D
						/*
						 * @date 2021-09-16 00:21:52
						 * This implies that `Q==F` which has already ben tested
						 */
						assert(0);
						*Q = *T = *F = AB;
						return true;
					} else if (compare(B, this, D, CASCADE_AND) < 0) {
						// C=A<B<D
						// already *Q = AB;
						*T = D;
						// already *F = 0;
						return false; // remains AND
					} else {
						// A=C<D<B
						*Q = CD;
						*T = B;
						// already *F = 0;
						return false; // remains AND
					}
				} else if (A == D) {
					// C<D=A<B
					*Q = CD;
					*T = B;
					// already *F = 0;
					return false; // remains AND
				} else if (B == C) {
					// A<B=C<D
					*Q = AB;
					*T = D;
					//already *F = 0;
					return false; // remains AND
				} else if (B == D) {
					// A<C<D=B or C<A<B=D
					// A and C can cascade, neither will exceed B/D
					if (this->isAND(A) || this->isAND(C)) {
						// cascades, merge AC, B last
						*Q = A;
						*T = C;
						// already *F = 0;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							*T = B;
							*F = 0;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							*T = B;
							// already *F = 0;
							return false; // remains AND
						}
					} else if (compare(A, this, C, CASCADE_AND) < 0) {
						// A<C<B
						*Q = addBasicNode(A, C, 0);
						*T = B;
						// already *F = 0;
						return false; // remains AND
					} else {
						// C<A<B
						*Q = addBasicNode(C, A, 0);
						*T = B;
						// already *F = 0;
						return false; // remains AND
					}
				}

				/*
				 * 4! permutations where A<B and C<D has 6 candidates
				 */
				if (compare(B, this, C, CASCADE_AND) < 0) {
					// A<B<C<D
					if (this->isAND(C)) {
						// C cascades and unusable for right-hand-side
						// merge ABC, D last
						*Q = AB;
						*T = C;
						// already *F = 0;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, D no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addBasicNode(*Q, *T, *F);
							*T = D;
							*F = 0;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							*T = D;
							// already *F = 0;
							return false; // remains AND
						}
					} else {
						// simple rewrite
						*Q = addBasicNode(AB, C, 0);
						*T = D;
						// already *F = 0;
						return false; // remains AND
					}
				} else if (compare(D, this, A, CASCADE_AND) < 0) {
					// C<D<A<B
					if (this->isAND(A)) {
						// A cascades and unusable for right-hand-side
						// merge CDA, B last
						*Q = CD;
						*T = A;
						// already *F = 0;
						if (cascadeQTF(Q, T, F)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addBasicNode(*Q, *T, *F);
							*T = B;
							*F = 0;
							return cascadeQTF(Q, T, F);
						} else {
							// append last placeholder
							*Q = addBasicNode(*Q, *T, *F);
							*T = B;
							// already *F = 0;
							return false; // remains AND
						}
					} else {
						// simple rewrite
						*Q = addBasicNode(CD, A, 0);
						*T = B;
						// already *F = 0;
						return false; // remains AND
					}
				} else if (compare(B, this, D, CASCADE_AND) < 0) {
					// A<C<B<D or C<A<B<D
					// merge ABC, D last
					// already *Q = AB;
					*T = C;
					// already *F = 0;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, D no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = D;
						*F = 0;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = D;
						// already *F = 0;
						return false; // remains AND
					}
				} else {
					// A<C<D<B or C<A<D<B
					// merge ACD, B last
					*Q = A;
					// already *T = CD;
					// already *F = 0;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B;
						*F = 0;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B;
						// already *F = 0;
						return false; // remains AND
					}
				}

			} else if (this->isAND(*Q)) {
				// AB&C&
				uint32_t AB = *Q; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].T; // does not cascade
				uint32_t C  = *T; // does not cascade

				assert(!this->isAND(B));
				assert(!this->isAND(C));

				if (C == A) {
					// C=A<B
					*Q = *T = *F = AB;
					return true;
				} else if (C == B) {
					// A<B=C
					*Q = *T = *F = AB;
					return true;
				}

				/*
				 * 3! permutations where A<B has 3 candidates
				 */
				if (compare(B, this, C, CASCADE_AND) < 0) {
					// A<B<C
					// natural order
					// already *Q = AB;
					// already *T = C;
					// already *F = 0;
					return false;
				} else if (isAND(A)) {
					// cascade, merge AC, B last
					*Q = A;
					// already *T = C;
					// already *F = 0;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B;
						*F = 0;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = B;
						// already *F = 0;
						return false; // remains AND
					}
				} else if (compare(A, this, C, CASCADE_AND) < 0) {
					// A<C<B
					*Q = addBasicNode(A, C, 0);
					*T = B;
					// already *F = 0;
					return false; // remains AND
				} else {
					// C<A<B
					*Q = addBasicNode(C, A, 0);
					*T = B;
					// already *F = 0;
					return false; // remains AND
				}

			} else if (this->isAND(*T)) {
				// ABC&&
				uint32_t BC = *T; // may cascade
				uint32_t A  = *Q; // does not cascade
				uint32_t B  = this->N[BC].Q; // may cascade
				uint32_t C  = this->N[BC].T; // does not cascade

				assert(!this->isAND(A));
				assert(!this->isAND(C));

				if (A == B) {
					// A=B<C
					*Q = *T = *F = BC;
					return true;
				} else if (A == C) {
					// B<C=A
					*Q = *T = *F = BC;
					return true;
				}

				/*
				 * 3! permutations where B<C has 3 candidates
				 */
				if (compare(C, this, A, CASCADE_AND) < 0) {
					// B<C<A
					*Q = BC;
					*T = A;
					//already *F = 0;
					return false; // remains AND
				} else if (isAND(B)) {
					// cascade, merge AB, C last
					// already *Q = A;
					*T = B;
					// already *F = 0;
					if (cascadeQTF(Q, T, F)) {
						// *Q,*T,*F changed or folded, C no longer assumed highest
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = C;
						*F = 0;
						return cascadeQTF(Q, T, F);
					} else {
						// append last placeholder
						*Q = addBasicNode(*Q, *T, *F);
						*T = C;
						// already *F = 0;
						return false; // remains AND
					}
				} else if (compare(A, this, B, CASCADE_AND) < 0) {
					// A<B<C
					*Q = addBasicNode(A, B, 0);
					*T = C;
					// already *F = 0;
					return false; // remains AND
				} else {
					// B<A<C
					*Q = addBasicNode(B, A, 0);
					*T = C;
					// already *F = 0;
					return false; // remains AND
				}

			} else if (compare(*T, this, *Q, CASCADE_AND) < 0) {
				// swap
				uint32_t savQ = *Q;
				*Q = *T;
				*T = savQ;
				// already *F = 0;
				return false; // remains AND
			} else {
				// no change
				return false;
			}
		}

		return false;
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
	uint32_t addNormaliseNode(uint32_t Q, uint32_t T, uint32_t F) {

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
			uint32_t savT = T;
			T = F;
			F = savT;
			Q ^= IBIT;
		}
		if (Q == 0) {
			// "0?T:F" -> "F"
			return F;
		}

		// ibit indicates the result should be inverted
		uint32_t ibit = 0;

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
		 *  [ 8] a ? ~b : b                  "^" not-equal/xor
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
					// NOT-EQUAL/XOR
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
		 * Rewrite `QTF` to `QnTF`
		 *
		 * a ?  b : c -> a?~(a?~b:c):c  "?" QTF
		 *
		 * ./eval --qntf 'ab&' 'abc?'
		 */

		if ((ctx.flags & context_t::MAGICMASK_PURE) && !(T & IBIT)) {
			// QTF
			// Q?T:F -> Q?~(Q?~T:F):F)
			T = addBasicNode(Q, T ^ IBIT, F) ^ IBIT;
		}

		if (ctx.flags & context_t::MAGICMASK_CASCADE) {
			// Extended cascading/ordering
			this->cascadeQTF(&Q, &T, &F);
		} else if (this->isOR(Q, T, F)) {
			// Basic ordering, do not supply a cascading option as that is disabled 
			if (this->compare(F, this, Q) < 0) {
				uint32_t savQ = Q;
				Q = F;
				F = savQ;
			}
		} else if (this->isNE(Q, T, F)) {
			// Basic ordering, do not supply a cascading option as that is disabled 
			if (this->compare(F, this, Q) < 0) {
				uint32_t savQ = Q;
				Q = F;
				T = savQ ^ IBIT;
				F = savQ;
			}
		} else if (this->isAND(Q, T, F)) {
			// Basic ordering, do not supply a cascading option as that is disabled 

			if (this->compare(T, this, Q) < 0) {
				uint32_t savQ = Q;
				Q = T;
				T = savQ;
			}
		}

		return this->addBasicNode(Q, T, F) ^ ibit;
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

		assert(pName[0]); // disallow empty name

		// initialise tree
		this->clearTree();

		// state storage for postfix notation
		uint32_t stack[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      numStack = 0;
		uint32_t nextNode = TINYTREE_NSTART; // next visual node
		uint32_t beenWhat[TINYTREE_NEND]; // track id's of display operators.

		// walk through the notation until end or until placeholder/skin separator
		for (const char *pCh = pName; *pCh; pCh++) {

			if (isalnum(*pCh) && numStack >= TINYTREE_MAXSTACK)
				return DERR_OVERFLOW;
			if (!isalnum(*pCh) && count >= TINYTREE_NEND)
				return DERR_SIZE;
			if (islower(*pCh) && !islower(pSkin[*pCh - 'a']))
				return DERR_PLACEHOLDER;

			switch (*pCh) {
			case '0':
				stack[numStack++] = 0;
				break;
			case 'a':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[0] - 'a');
				break;
			case 'b':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[1] - 'a');
				break;
			case 'c':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[2] - 'a');
				break;
			case 'd':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[3] - 'a');
				break;
			case 'e':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[4] - 'a');
				break;
			case 'f':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[5] - 'a');
				break;
			case 'g':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[6] - 'a');
				break;
			case 'h':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[7] - 'a');
				break;
			case 'i':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[8] - 'a');
				break;
			case '1':
				stack[numStack++] = beenWhat[nextNode - ('1' - '0')];
				break;
			case '2':
				stack[numStack++] = beenWhat[nextNode - ('2' - '0')];
				break;
			case '3':
				stack[numStack++] = beenWhat[nextNode - ('3' - '0')];
				break;
			case '4':
				stack[numStack++] = beenWhat[nextNode - ('4' - '0')];
				break;
			case '5':
				stack[numStack++] = beenWhat[nextNode - ('5' - '0')];
				break;
			case '6':
				stack[numStack++] = beenWhat[nextNode - ('6' - '0')];
				break;
			case '7':
				stack[numStack++] = beenWhat[nextNode - ('7' - '0')];
				break;
			case '8':
				stack[numStack++] = beenWhat[nextNode - ('8' - '0')];
				break;
			case '9':
				stack[numStack++] = beenWhat[nextNode - ('9' - '0')];
				break;

			case '+': {
				// OR (appreciated)
				if (numStack < 2)
					return DERR_UNDERFLOW;

				// pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = addNormaliseNode(L, 0 ^ IBIT, R);

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '>': {
				// GT (appreciated)
				if (numStack < 2)
					return DERR_UNDERFLOW;

				//pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = addNormaliseNode(L, R ^ IBIT, 0);

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '^': {
				// NE/XOR (appreciated)
				if (numStack < 2)
					return DERR_UNDERFLOW;

				//pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = addNormaliseNode(L, R ^ IBIT, R);

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '!': {
				// QnTF (appreciated)
				if (numStack < 3)
					return DERR_UNDERFLOW;

				// pop operands
				uint32_t F = stack[--numStack];
				uint32_t T = stack[--numStack];
				uint32_t Q = stack[--numStack];

				// create operator
				uint32_t nid = addNormaliseNode(Q, T ^ IBIT, F);

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND (depreciated)
				if (numStack < 2)
					return DERR_UNDERFLOW;

				// pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = addNormaliseNode(L, R, 0);

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '<': {
				// LT (obsolete)
				if (numStack < 2)
					return DERR_UNDERFLOW;

				//pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = addNormaliseNode(L, 0, R);

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF (depreciated)
				if (numStack < 3)
					return DERR_UNDERFLOW;

				// pop operands
				uint32_t F = stack[--numStack];
				uint32_t T = stack[--numStack];
				uint32_t Q = stack[--numStack];

				// create operator
				uint32_t nid = addNormaliseNode(Q, T, F);

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT (support)
				if (numStack < 1)
					return DERR_UNDERFLOW;

				// invert top-of-stack
				stack[numStack - 1] ^= IBIT;
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

		if (numStack != 1)
			return DERR_INCOMPLETE;

		// store result into root
		this->root = stack[numStack - 1];

		// store result into root
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
	uint32_t addStringFast(const char *pName, const char *pSkin = "abcdefghi") {

		assert(pName[0]); // disallow empty name

		// state storage for postfix notation
		uint32_t stack[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      numStack        = 0;
		uint32_t nextPlaceholder = TINYTREE_KSTART;
		uint32_t nextNode        = TINYTREE_NSTART; // next visual node
		uint32_t beenWhat[TINYTREE_NEND]; // track id's of display operators.

		// walk through the notation until end or until placeholder/skin separator
		for (const char *pCh = pName; *pCh; pCh++) {

			switch (*pCh) {
			case '0':
				stack[numStack++] = 0;
				break;
			case 'a':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[0] - 'a');
				break;
			case 'b':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[1] - 'a');
				break;
			case 'c':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[2] - 'a');
				break;
			case 'd':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[3] - 'a');
				break;
			case 'e':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[4] - 'a');
				break;
			case 'f':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[5] - 'a');
				break;
			case 'g':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[6] - 'a');
				break;
			case 'h':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[7] - 'a');
				break;
			case 'i':
				stack[numStack++] = (uint32_t) (TINYTREE_KSTART + pSkin[8] - 'a');
				break;
			case '1':
				stack[numStack++] = beenWhat[nextNode - ('1' - '0')];
				break;
			case '2':
				stack[numStack++] = beenWhat[nextNode - ('2' - '0')];
				break;
			case '3':
				stack[numStack++] = beenWhat[nextNode - ('3' - '0')];
				break;
			case '4':
				stack[numStack++] = beenWhat[nextNode - ('4' - '0')];
				break;
			case '5':
				stack[numStack++] = beenWhat[nextNode - ('5' - '0')];
				break;
			case '6':
				stack[numStack++] = beenWhat[nextNode - ('6' - '0')];
				break;
			case '7':
				stack[numStack++] = beenWhat[nextNode - ('7' - '0')];
				break;
			case '8':
				stack[numStack++] = beenWhat[nextNode - ('8' - '0')];
				break;
			case '9':
				stack[numStack++] = beenWhat[nextNode - ('9' - '0')];
				break;

			case '+': {
				// OR (appreciated)

				// pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = 0 ^ IBIT;
				this->N[nid].F = R;

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '>': {
				// GT (appreciated)

				//pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = R ^ IBIT;
				this->N[nid].F = 0;

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '^': {
				// NE/XOR (appreciated)

				//pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = R ^ IBIT;
				this->N[nid].F = R;

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '!': {
				// QnTF (appreciated)

				// pop operands
				uint32_t F = stack[--numStack];
				uint32_t T = stack[--numStack];
				uint32_t Q = stack[--numStack];

				// create operator
				uint32_t nid = this->count++;
				this->N[nid].Q = Q;
				this->N[nid].T = T ^ IBIT;
				this->N[nid].F = F;

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND (depreciated)

				// pop operands
				uint32_t R = stack[--numStack]; // right hand side
				uint32_t L = stack[--numStack]; // left hand side

				// create operator
				uint32_t nid = this->count++;
				this->N[nid].Q = L;
				this->N[nid].T = R;
				this->N[nid].F = 0;

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF (depreciated)

				// pop operands
				uint32_t F = stack[--numStack];
				uint32_t T = stack[--numStack];
				uint32_t Q = stack[--numStack];

				// create operator
				uint32_t nid = this->count++;
				this->N[nid].Q = Q;
				this->N[nid].T = T;
				this->N[nid].F = F;

				// push
				stack[numStack++] = beenWhat[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT (support)

				// invert top-of-stack
				stack[numStack - 1] ^= IBIT;
				break;
			}

			case '/':
				// skip delimiter
				while (pCh[1])
					pCh++;
				break;
			}
		}
		assert (numStack == 1);
		assert(this->count <= tinyTree_t::TINYTREE_NEND);
		assert(nextPlaceholder <= TINYTREE_NSTART);

		// return root of name
		return stack[numStack - 1];

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
	inline void loadStringFast(const char *pName, const char *pSkin = "abcdefghi") {

		// initialise tree
		this->clearTree();
		// load root
		this->root = this->addStringFast(pName, pSkin);
	}

	/**
	 * @date 2020-03-13 22:12:24
	 *
	 * Encode a notation describing the tree in "placeholder/skin" notation.
	 * Within the placeholders, endpoints are assigned in order of natural path which can be used as index for the skin to determine the actual endpoint.
	 *
	 * @date 2021-08-29 08:34:38
	 *
	 * Assign placeholders in tree-walk order
	 *
	 * @param {number} id - entrypoint
	 * @param {string} pName - The notation describing the tree
	 * @param {string} pSkin - Skin
	 */
	void saveString(uint32_t id, char *pName, char *pSkin) const {

		unsigned nameLen  = 0;

		if ((id & ~IBIT) < TINYTREE_NSTART) {
			if (pSkin) {
				if ((id & ~IBIT) == 0) {
					pName[nameLen++] = '0';
					pSkin[0]         = 0;
				} else {
					pSkin[0]         = (char) ('a' + (id & ~IBIT) - TINYTREE_KSTART);
					pSkin[1]         = 0;
					pName[nameLen++] = 'a';
				}

			} else {
				if ((id & ~IBIT) == 0) {
					pName[nameLen++] = '0';
				} else {
					pName[nameLen++] = (char) ('a' + (id & ~IBIT) - TINYTREE_KSTART);
				}
			}

			// test for root invert
			if (id & IBIT)
				pName[nameLen++] = '~';

			// terminator
			pName[nameLen] = 0;

			return;
		}

		// temporary stack storage for postfix notation
		uint32_t stack[TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      numStack = 0;

		uint32_t nextNode       = TINYTREE_NSTART;
		unsigned numPlaceholder = 0;

		uint32_t beenThere = (1 << 0);
		uint32_t beenWhat[TINYTREE_NEND];
		beenWhat[0] = 0;

		// starting point
		stack[numStack++] = id & ~IBIT;

		do {
			// pop stack
			uint32_t curr = stack[--numStack];

			assert(curr != 0);

			// if endpoint then emit
			if (curr < TINYTREE_NSTART) {
				if (!pSkin) {
					// endpoint
					pName[nameLen++] = (char) ('a' + curr - TINYTREE_KSTART);
				} else {
					// placeholder
					if (!(beenThere & (1 << curr))) {
						beenThere |= (1 << curr);
						pSkin[numPlaceholder] = (char) ('a' + curr - TINYTREE_KSTART);
						beenWhat[curr] = TINYTREE_KSTART + numPlaceholder++;
					}
					pName[nameLen++] = (char) ('a' + beenWhat[curr] - TINYTREE_KSTART);
				}

				continue;
			}

			const tinyNode_t *pNode = this->N + curr;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			// determine if node already handled
			if (!(beenThere & (1 << curr))) {
				// first time
				beenThere |= (1 << curr);
				beenWhat[curr] = 0;

				// push id so it visits again a second time for the operator
				stack[numStack++] = curr;

				// push non-zero endpoints
				if (F >= TINYTREE_KSTART)
					stack[numStack++] = F;
				if (Tu != F && Tu >= TINYTREE_KSTART)
					stack[numStack++] = Tu;
				if (Q >= TINYTREE_KSTART)
					stack[numStack++] = Q;

				assert(numStack <= TINYTREE_MAXSTACK);

			} else if (beenWhat[curr] == 0) {
				// node complete, output operator
				beenWhat[curr] = nextNode++;

				if (Ti) {
					if (Tu == 0) {
						// OR Q?!0:F
						pName[nameLen++] = '+';
					} else if (F == 0) {
						// GT Q?!T:0
						pName[nameLen++] = '>';
					} else if (F == Tu) {
						// NE Q?!F:F
						pName[nameLen++] = '^';
					} else {
						// QnTF Q?!T:F
						pName[nameLen++] = '!';
					}
				} else {
					if (Tu == 0) {
						// LT Q?0:F
						pName[nameLen++] = '<';
					} else if (F == 0) {
						// AND Q?T:0
						pName[nameLen++] = '&';
					} else if (F == Tu) {
						// SELF Q?F:F
						assert(!"Q?F:F");
					} else {
						// QTF Q?T:F
						pName[nameLen++] = '?';
					}
				}

			} else {
				// back-reference to earlier node

				uint32_t backref = nextNode - beenWhat[curr];
				assert(backref <= 9);
				pName[nameLen++] = (char) ('0' + backref);
			}

		} while (numStack > 0);

		// test for inverted-root
		if (id & IBIT)
			pName[nameLen++] = '~';

		// terminators
		assert(nameLen <= TINYTREE_NAMELEN);
		pName[nameLen] = 0;

		if (pSkin != NULL) {
			assert(numPlaceholder <= MAXSLOTS);
			pSkin[numPlaceholder] = 0;
		}
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
	const char *saveString(uint32_t id) const {

		static char staticName[TINYTREE_NAMELEN + 1];

		saveString(id, staticName, NULL);

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
		for (uint32_t i = TINYTREE_NSTART; i < count; i++) {
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
		for (uint32_t i = TINYTREE_NSTART; i < count; i++) {
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
		for (uint32_t i = TINYTREE_NSTART; i < count; i++) {
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
