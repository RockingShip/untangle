#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-18 18:04:50
 *
 * `genprogress` fires up the generator and extracts some metrics.
 * It generates fully normalised and naturally ordered trees for further processing.
 * With this version, all calls to `foundTree()` are notation unique.
 *
 * Selfcheck consists of checking runtime metrics with previous heuristics.
 * An ancient implementation of the generator is included for regression testing.
 * It now seems that the current implementation has out-performed and over achieved the ancient version.
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>

#include "generator.h"
#include "metrics.h"

/**
 * @date 2020-03-19 00:15:03
 *
 * Previous major version of generator. taken from `"untangle-1.48.0"`
 * NOTE: that version was in the mindset of `QTnF` which has been updated
 *
 * For version 1.48, it has been recorded that `foundTree()` was called these many times:
 *
 * `1n9`=6, `2n9`=484, `3n9`=111392, `4n9`=48295088, `5n9`=33212086528
 *
 * @typedef {object}
 */
struct ancientTree_t : tinyTree_t {

	/**
	 * @date 2020-03-19 00:27:45
	 *
	 * Constructor
	 *
	 * @param {context_t} ctx - I/O context
	 * @param {number} flags - Tree/node functionality
	 */
	ancientTree_t(context_t &ctx) : tinyTree_t(ctx) {
	}

	// @date 2020-03-19 00:46:36
	inline void foundTree() {
		ctx.progress++;

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			fprintf(stderr, "\r\e[K[%s] %.5f%%", ctx.timeAsString(), ctx.progress * 100.0 / ctx.progressHi);
			ctx.tick = 0;
		}

		if (ctx.opt_text) {
			printf("%s\n", this->encode(this->root));

#if 0
			// simply dump tree
			for (unsigned i = TINYTREE_NSTART; i < this->count; i++) {
				printf("%d%x%x%x ", N[i].T & IBIT ? 1 : 0, N[i].Q, N[i].T & ~IBIT, N[i].F);
			}
			printf("\n");
#endif
		}
	}

	/**
	 * @date 2020-03-19 15:45:25
	 *
	 * undo last push, releasing nodes that were created
	 */
	inline void pop(void) {
		// pop node
		--this->count;
	}

	/**
	 * @date 2020-03-19 00:15:03
	 *
	 * NOTE: all arguments must *ALWAYS* stay arguments, pass-by-value.
	 * NOTE: `numNodes` may be an extra argument to limit `slotValues[]`. Maybe eliminate the whole thing by replacing it with a single value.
	 * NOTE: zero (for dyadics) count as a point
	 *
	 * @param {number} endpointsLeft -  number of endpoints still to fill
	 * @param {number} numPlaceholder - number of placeholders already assigned
	 * @param {uint64_t} stack - stack for temporary results
	 */
	void /*__attribute__((optimize("O0")))*/ generateTrees(unsigned endpointsLeft, unsigned numPlaceholder, uint64_t stack) {

		/*
		 * @date 2020-03-19 14:59:29
		 *
		 * Some slight adaptations that keeps existing functionality and moves to later insights
		 *
		 * - popping stack values
		 * - stack is `uint64_t` and can hold 12 nodes
		 * - use `pop()` to undo last node addition
		 * - stack only contains non-zero entries. Stack is depleted if it is zero
		 * - numPlaceholder zero based
		 */

		assert(MAXSLOTS == 9);
		assert (numPlaceholder <= MAXSLOTS);
		assert(tinyTree_t::TINYTREE_MAXNODES <= 12);

		// shortcuts
		enum {
			KSTART = TINYTREE_KSTART,
			NSTART = TINYTREE_NSTART,
			WIDTH = generatorTree_t::PACKED_WIDTH,
			MASK = generatorTree_t::PACKED_MASK,
		};

		static const uint32_t slotValues[1 + MAXSLOTS + 3][MAXSLOTS + /*MAXNODEPATTERN*/6 + 1] = {
			{KSTART + 0, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, KSTART + 5, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, KSTART + 5, KSTART + 6, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, KSTART + 5, KSTART + 6, KSTART + 7, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, KSTART + 5, KSTART + 6, KSTART + 7, KSTART + 8, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			// after last slot assigned, no more variables left
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, KSTART + 5, KSTART + 6, KSTART + 7, KSTART + 8, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, KSTART + 5, KSTART + 6, KSTART + 7, KSTART + 8, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
			{KSTART + 0, KSTART + 1, KSTART + 2, KSTART + 3, KSTART + 4, KSTART + 5, KSTART + 6, KSTART + 7, KSTART + 8, NSTART + 0, NSTART + 1, NSTART + 2, NSTART + 3, NSTART + 4, NSTART + 5, NSTART + 6},
		};

		// allocate next node of tree
		uint32_t R = this->count;
		// and declare it root
		this->root = R;

		uint64_t originalStack = stack;

		// upper limit Q
		const unsigned numSlotQ = numPlaceholder;

		/*
		 * first pass, dyadics
		 */

		if (endpointsLeft >= 3) {
			/*
			 * <Q> <T>
			 */
			for (uint32_t iQ = 0, Q; (Q = slotValues[numSlotQ][iQ]) < R; iQ++) {
				// upper limit T
				const unsigned numSlotT = (iQ == numSlotQ) ? numSlotQ + 1 : numSlotQ; // bump number of slots

				for (uint32_t iT = 0, T; (T = slotValues[numSlotT][iT]) < R; iT++) {
					// test allowed
					if (Q == T) continue;
					// upper limit next node
					const unsigned newNumSlot = (iT == numSlotT) ? numSlotT + 1 : numSlotT; // bump number of slots

					// NE
					if (this->addNormalised(Q, T ^ IBIT, T) == R) {
						if (endpointsLeft == 3 && stack == 0)
							foundTree();
						else
							this->generateTrees(endpointsLeft - 3, newNumSlot, stack << WIDTH | R);
						pop();
					}

					// OR
					if (this->addNormalised(Q, 0 ^ IBIT, T) == R) {
						if (endpointsLeft == 3 && stack == 0)
							foundTree();
						else
							this->generateTrees(endpointsLeft - 3, newNumSlot, stack << WIDTH | R);
						pop();
					}

					// GT
					if (this->addNormalised(Q, T ^ IBIT, 0) == R) {
						if (endpointsLeft == 3 && stack == 0)
							foundTree();
						else
							this->generateTrees(endpointsLeft - 3, newNumSlot, stack << WIDTH | R);
						pop();
					}

					// AND
					if (this->addNormalised(Q, T, 0) == R) {
						if (endpointsLeft == 3 && stack == 0)
							foundTree();
						else
							this->generateTrees(endpointsLeft - 3, newNumSlot, stack << WIDTH | R);
						pop();
					}
				}
			}
		}

		uint32_t pop0 = stack & MASK;
		stack >>= WIDTH;

		if (endpointsLeft >= 2 && pop0 != 0) {
			/*
			 * <Q> <pop>
			 */
			{
				// pop T
				const uint32_t T = pop0;

				for (uint32_t iQ = 0, Q; (Q = slotValues[numSlotQ][iQ]) < R; iQ++) {
					// upper limit T
					const unsigned numSlotT = (iQ == numSlotQ) ? numSlotQ + 1 : numSlotQ; // bump number of slots

					{
						// test allowed
						if (Q == T) continue;
						// upper limit next node
						const unsigned newNumSlot = numSlotT; // bump number of slots

						// NE
						if (this->addNormalised(Q, T ^ IBIT, T) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();

						}

						// OR
						if (this->addNormalised(Q, 0 ^ IBIT, T) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// GT
						if (this->addNormalised(Q, T ^ IBIT, 0) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// AND
						if (this->addNormalised(Q, T, 0) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}
					}
				}
			}

			/*
			 * <pop> <T>
			 */
			{
				// pop Q
				uint32_t Q = pop0;

				{
					// upper limit T
					const unsigned numSlotT = numSlotQ; // bump number of slots

					for (unsigned iT = 0, T; (T = slotValues[numSlotT][iT]) < R; iT++) {
						// test allowed
						if (Q == T) continue;
						// upper limit next node
						const unsigned newNumSlot = (iT == numSlotT) ? numSlotT + 1 : numSlotT; // bump number of slots

						// NE
						if (this->addNormalised(Q, T ^ IBIT, T) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// OR
						if (this->addNormalised(Q, 0 ^ IBIT, T) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// GT
						if (this->addNormalised(Q, T ^ IBIT, 0) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// AND
						if (this->addNormalised(Q, T, 0) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}
					}
				}
			}
		}

		uint32_t pop1 = stack & MASK;
		stack >>= WIDTH;

		if (endpointsLeft >= 1 && pop1 != 0) {
			/*
			 * <pop> <pop>
			 */
			{
				// pop Q,T
				const uint32_t T = pop0;
				const uint32_t Q = pop1;

				assert (Q != T);

				{
					// upper limit T
					const unsigned numSlotT = numSlotQ; // bump number of slots

					{
						// upper limit next node
						const unsigned newNumSlot = numSlotT; // bump number of slots

						// NE
						if (this->addNormalised(Q, T ^ IBIT, T) == R) {
							if (endpointsLeft == 1 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// OR
						if (this->addNormalised(Q, 0 ^ IBIT, T) == R) {
							if (endpointsLeft == 1 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// GT
						if (this->addNormalised(Q, T ^ IBIT, 0) == R) {
							if (endpointsLeft == 1 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// AND
						if (this->addNormalised(Q, T, 0) == R) {
							if (endpointsLeft == 1 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
							pop();
						}
					}
				}
			}
		}

		/*
		 * second pass, triadics
		 */

		stack = originalStack;

		if (endpointsLeft >= 3) {
			/*
			 * <Q> <T> <F>
			 */
			for (uint32_t iQ = 0, Q; (Q = slotValues[numSlotQ][iQ]) < R; iQ++) {
				// upper limit T
				const unsigned numSlotT = (iQ == numSlotQ) ? numSlotQ + 1 : numSlotQ; // bump number of slots

				for (uint32_t iT = 0, T; (T = slotValues[numSlotT][iT]) < R; iT++) {
					// test allowed
					if (Q == T) continue;
					// upper limit F
					const unsigned numSlotF = (iT == numSlotT) ? numSlotT + 1 : numSlotT; // bump number of slots

					for (uint32_t iF = 0, F; (F = slotValues[numSlotF][iF]) < R; iF++) {
						// test allowed
						if (Q == F) continue;
						if (T == F) continue;
						// upper limit next node
						const unsigned newNumSlot = (iF == numSlotF) ? numSlotF + 1 : numSlotF; // bump number of slots

						// QnTF
						if (this->addNormalised(Q, T ^ IBIT, F) == R) {
							if (endpointsLeft == 3 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 3, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// QTF
						if (this->addNormalised(Q, T, F) == R) {
							if (endpointsLeft == 3 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 3, newNumSlot, stack << WIDTH | R);
							pop();
						}
					}
				}
			}
		}

		pop0 = stack & MASK;
		stack >>= WIDTH;

		if (endpointsLeft >= 2 && pop0 != 0) {
			/*
			 * <Q> <T> <pop>
			 */
			{
				// pop F
				const uint32_t F = pop0;

				for (uint32_t iQ = 0, Q; (Q = slotValues[numSlotQ][iQ]) < R; iQ++) {
					// upper limit T
					const unsigned numSlotT = (iQ == numSlotQ) ? numSlotQ + 1 : numSlotQ; // bump number of slots

					for (uint32_t iT = 0, T; (T = slotValues[numSlotT][iT]) < R; iT++) {
						// test allowed
						if (Q == T) continue;
						// upper limit F
						const unsigned numSlotF = (iT == numSlotT) ? numSlotT + 1 : numSlotT; // bump number of slots

						{
							// test allowed
							if (Q == F) continue;
							if (T == F) continue;
							// upper limit next node
							const unsigned newNumSlot = numSlotF; // bump number of slots

							// QnTF
							if (this->addNormalised(Q, T ^ IBIT, F) == R) {
								if (endpointsLeft == 2 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
								pop();
							}

							// QTF
							if (this->addNormalised(Q, T, F) == R) {
								if (endpointsLeft == 2 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
								pop();
							}
						}
					}
				}
			}

			/*
			 * <Q> <pop> <F>
			 */
			{
				// pop T
				const uint32_t T = pop0;

				for (uint32_t iQ = 0, Q; (Q = slotValues[numSlotQ][iQ]) < R; iQ++) {
					// upper limit T
					const unsigned numSlotT = (iQ == numSlotQ) ? numSlotQ + 1 : numSlotQ; // bump number of slots

					{
						// test allowed
						if (Q == T) continue;
						// upper limit F
						const unsigned numSlotF = numSlotT; // bump number of slots

						for (uint32_t iF = 0, F; (F = slotValues[numSlotF][iF]) < R; iF++) {
							// test allowed
							if (Q == F) continue;
							if (T == F) continue;
							// upper limit next node
							const unsigned newNumSlot = (iF == numSlotF) ? numSlotF + 1 : numSlotF; // bump number of slots

							// QnTF
							if (this->addNormalised(Q, T ^ IBIT, F) == R) {
								if (endpointsLeft == 2 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
								pop();
							}

							// QTF
							if (this->addNormalised(Q, T, F) == R) {
								if (endpointsLeft == 2 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
								pop();
							}
						}
					}
				}
			}

			/*
			 * <pop> <T> <F>
			 */
			{
				// pop Q
				const uint32_t Q = pop0;

				// upper limit T
				const unsigned numSlotT = numSlotQ; // bump number of slots

				for (uint32_t iT = 0, T; (T = slotValues[numSlotQ][iT]) < R; iT++) {
					// test allowed
					if (Q == T) continue;
					// upper limit F
					const unsigned numSlotF = (iT == numSlotT) ? numSlotT + 1 : numSlotT; // bump number of slots

					for (uint32_t iF = 0, F; (F = slotValues[numSlotF][iF]) < R; iF++) {
						// test allowed
						if (Q == F) continue;
						if (T == F) continue;
						// upper limit next node
						const unsigned newNumSlot = (iF == numSlotF) ? numSlotF + 1 : numSlotF; // bump number of slots

						// QnTF
						if (this->addNormalised(Q, T ^ IBIT, F) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}

						// QTF
						if (this->addNormalised(Q, T, F) == R) {
							if (endpointsLeft == 2 && stack == 0)
								foundTree();
							else
								this->generateTrees(endpointsLeft - 2, newNumSlot, stack << WIDTH | R);
							pop();
						}
					}
				}
			}
		}

		pop1 = stack & MASK;
		stack >>= WIDTH;

		if (endpointsLeft >= 1 && pop1 != 0) {
			/*
			 * <Q> <pop> <pop>
			 */
			{
				// pop T
				const uint32_t F = pop0;
				const uint32_t T = pop1;

				assert(T != F);

				for (uint32_t iQ = 0, Q; (Q = slotValues[numSlotQ][iQ]) < R; iQ++) {
					// upper limit T
					const unsigned numSlotT = (iQ == numSlotQ) ? numSlotQ + 1 : numSlotQ; // bump number of slots

					{
						// test allowed
						if (Q == T) continue;
						// upper limit F
						const unsigned numSlotF = numSlotT; // bump number of slots

						{
							// test allowed
							if (Q == F) continue;
							// upper limit next node
							const unsigned newNumSlot = numSlotF; // bump number of slots

							// QnTF
							if (this->addNormalised(Q, T ^ IBIT, F) == R) {
								if (endpointsLeft == 1 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
								pop();
							}

							// QTF
							if (this->addNormalised(Q, T, F) == R) {
								if (endpointsLeft == 1 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
								pop();
							}
						}
					}
				}
			}

			/*
			 * <pop> <T> <pop>
			 */
			{
				// pop Q,F
				const uint32_t F = pop0;
				const uint32_t Q = pop1;

				assert (Q != F);

				{
					// upper limit T
					const unsigned numSlotT = numSlotQ; // bump number of slots

					for (uint32_t iT = 0, T; (T = slotValues[numSlotT][iT]) < R; iT++) {
						// test allowed
						if (Q == T) continue;
						// upper limit F
						const unsigned numSlotF = (iT == numSlotT) ? numSlotT + 1 : numSlotT; // bump number of slots

						{
							// test allowed
							if (T == F) continue;
							// upper limit next node
							const unsigned newNumSlot = numSlotF; // bump number of slots

							// QnTF
							if (this->addNormalised(Q, T ^ IBIT, F) == R) {
								if (endpointsLeft == 1 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
								pop();
							}

							// QTF
							if (this->addNormalised(Q, T, F) == R) {
								if (endpointsLeft == 1 && stack == 0)
									foundTree();
								else
									this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
								pop();
							}
						}
					}
				}
			}

			/*
			 * <pop> <pop> <F>
			 */
			{
				// pop Q,T
				const uint32_t T = pop0;
				const uint32_t Q = pop1;

				assert (Q != T);

				// upper limit F
				const unsigned numSlotT = numSlotQ; // bump number of slots
				const unsigned numSlotF = numSlotT; // bump number of slots

				for (uint32_t iF = 0, F; (F = slotValues[numSlotF][iF]) < R; iF++) {
					// test allowed
					if (Q == F) continue;
					if (T == F) continue;
					// upper limit next node
					const unsigned newNumSlot = (iF == numSlotF) ? numSlotF + 1 : numSlotF; // bump number of slots

					// QnTF
					if (this->addNormalised(Q, T ^ IBIT, F) == R) {
						if (endpointsLeft == 1 && stack == 0)
							foundTree();
						else
							this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
						pop();
					}

					// QTF
					if (this->addNormalised(Q, T, F) == R) {
						if (endpointsLeft == 1 && stack == 0)
							foundTree();
						else
							this->generateTrees(endpointsLeft - 1, newNumSlot, stack << WIDTH | R);
						pop();
					}
				}
			}
		}

		uint32_t pop2 = stack & MASK;
		stack >>= WIDTH;

		if (pop2 != 0) {
			/*
			 * <pop> <pop> <pop>
			 */
			// pop Q,T
			const uint32_t F = pop0;
			const uint32_t T = pop1;
			const uint32_t Q = pop2;

			assert (Q != T);
			assert (Q != F);
			assert (T != F);

			// QnTF
			if (this->addNormalised(Q, T ^ IBIT, F) == R) {
				if (endpointsLeft == 0 && stack == 0)
					foundTree();
				else
					this->generateTrees(endpointsLeft, numPlaceholder, stack << WIDTH | R);
				pop();
			}

			// QTF
			if (this->addNormalised(Q, T, F) == R) {
				if (endpointsLeft == 0 && stack == 0)
					foundTree();
				else
					this->generateTrees(endpointsLeft, numPlaceholder, stack << WIDTH | R);
				pop();
			}
		}
	}

};

/**
 * @date 2020-03-19 20:20:53
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genprogressContext_t : context_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {number} size of structures used in this invocation
	unsigned arg_numNodes;
	/// @var {number} --ancient, use the ancient implementation
	unsigned opt_ancient;
	/// @var {number} --selftest, perform a selftest
	unsigned opt_selftest;

	/**
	 * Constructor
	 */
	genprogressContext_t() {
		// arguments and options
		arg_numNodes = 0;
		opt_ancient = 0;
		opt_selftest = 0;
	}

	/**
	 * @date 2020-03-18 22:17:26
	 *
	 * found candidate.
	 *
	 * @param {number} r - Root of tree
	 */
	void foundTree(generatorTree_t &tree) {
		this->progress++;

		if (opt_verbose >= VERBOSE_TICK && tick) {
			fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), progress * 100.0 / progressHi);
			tick = 0;
		}

		/*
		 * Debug mode used to create progress metrics and dump generated trees
		 */
		if (opt_text) {

			// display candidate
			printf("%s\n", tree.encode(tree.root));

#if 0
			// simple tree dump for the very paranoia
			for (unsigned i = TINYTREE_NSTART; i < this->count; i++) {
				uint32_t qtf = packedN[i];
				printf("%d%x%x%x ", (qtf & PACKED_TIBIT) ? 1 : 0, (qtf >> PACKED_QPOS) & PACKED_MASK, (qtf >> PACKED_TPOS) & PACKED_MASK, (qtf >> PACKED_FPOS) & PACKED_MASK);
			}
			printf("\n");
#endif
		}

	}

	/**
	 * @date 2020-03-19 20:58:57
	 *
	 * Main entrypoint
	 *
	 * @param {database_t} pStore - data store
	 */
	void main(generatorTree_t &generator) {
		// set generator into debug mode to disable default handling of `foundTree()`

		if (this->opt_ancient) {
			/*
			 * Ancient code
			 */

			// create generator
			ancientTree_t ancient(*this);

			// number of expected calls to `foundTree()`
			static uint64_t numProgress[] = {1, 6, 484, 111392, 45434680, 33212086528LL};
			// number of unique notations passed to `foundTree()`
			static uint64_t numUnique[] = {0, 6, 484, 97696, 35780488, 0};

			(void) numUnique; // suppress unused warning

			// clear tree
			ancient.clearTree();

			// reset progress
			this->progressHi = (this->arg_numNodes < 6) ? numProgress[this->arg_numNodes] : 1;
			this->progress = 0;
			this->tick = 0;

			unsigned endpointsLeft = this->arg_numNodes * 2 + 1;
			ancient.generateTrees(endpointsLeft, 0, 0);

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			if (this->progress != this->progressHi) {
				printf("{\"error\":\"ancientTree_t::progressHi failed\",\"where\":\"%s\",\"encountered\":%ld,\"expected\":%ld,\"numNode\":%d}\n",
				       __FUNCTION__, this->progress, this->progressHi, this->arg_numNodes);
				exit(1);
			}
		} else {
			/*
			 * Current code
			 */

			// clear tree
			generator.clearGenerator();

			// find metrics for setting
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF, this->arg_numNodes);

			// reset progress
			this->progressHi = pMetrics ? pMetrics->numProgress : 1;
			this->progress = 0;
			this->tick = 0;

			unsigned endpointsLeft = this->arg_numNodes * 2 + 1;

			generator.addCallback(this, (void (context_t::*)(generatorTree_t &)) &genprogressContext_t::foundTree);
			generator.generateTrees(endpointsLeft, 0, 0);

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			if (this->progress != this->progressHi) {
				printf("{\"error\":\"generatorTree_t::progressHi failed\",\"where\":\"%s\",\"encountered\":%ld,\"expected\":%ld,\"numNode\":%d}\n",
				       __FUNCTION__, this->progress, this->progressHi, this->arg_numNodes);

				fprintf(stderr, "[%s] metricsGenerator_t { /*numSlots=*/%d, /*qntf=*/%d, /*numNodes=*/%d, /*numProgress=*/%ldLL}\n",
				        this->timeAsString(), MAXSLOTS, (this->opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0, this->arg_numNodes, this->progress);

				exit(1);
			}
		}

		fprintf(stderr, "[%s] generatorTree_t::foundTree() for numNode=%d called %ld times\n", this->timeAsString(), this->arg_numNodes, this->progress);
	}

};

/**
 * @date 2020-03-19 20:06:36
 *
 * Perform a selftest.
 *
 * @param {gentransformContext_t} app - I/O context
 */
void performSelfTest(genprogressContext_t &app) {

	generatorTree_t generator(app);

	{
		// quickly test that `tinyTree_t` does level-2 normalisation
		generator.decodeSafe("ab>ba+&");
		const char *pName = generator.encode(generator.root);
		assert(strcmp(pName, "ab+ab>&") == 0);
	}

	for (unsigned numNodes = 1; numNodes <= 5; numNodes++) {
		// clear tree
		generator.clearGenerator();

		// find metrics for setting
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, 0, numNodes);

		// reset progress
		app.progressHi = pMetrics ? pMetrics->numProgress : 1;
		app.progress = 0;
		app.tick = 0;

		unsigned endpointsLeft = numNodes * 2 + 1;

		generator.addCallback(&app, (void (context_t::*)(generatorTree_t &)) &genprogressContext_t::foundTree);
		generator.generateTrees(endpointsLeft, 0, 0);

		if (app.opt_verbose >= app.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (app.progress != app.progressHi) {
			printf("{\"error\":\"ancientTree_t::progressHi failed\",\"where\":\"%s\",\"encountered\":%ld,\"expected\":%ld,\"numNode\":%d}\n",
			       __FUNCTION__, app.progress, app.progressHi, numNodes);
			exit(1);
		}

		fprintf(stderr, "[%s] generatorTree_t::foundTree() for numNode=%d called %ld times\n", app.timeAsString(), numNodes, app.progress);
	}

	/*
	 * Invoke ancient code
	 */
	ancientTree_t ancient(app);

	for (unsigned numNodes = 1; numNodes <= 5; numNodes++) {
		// number of expected calls to `foundTree()`
		static uint64_t numProgress[] = {0, 6, 484, 111392, 48295088, 33212086528LL};
		// number of unique notations passed to `foundTree()`
		static uint64_t numUnique[] = {0, 6, 484, 97696, 37144912, 0};

		(void) numUnique; // suppress unused warning

		// clear tree
		ancient.clearTree();

		// reset progress
		app.progressHi = numProgress[numNodes];
		app.progress = 0;
		app.tick = 0;

		unsigned endpointsLeft = numNodes * 2 + 1;
		ancient.generateTrees(endpointsLeft, 0, 0);

		if (app.opt_verbose >= app.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (app.progress != app.progressHi) {
			printf("{\"error\":\"ancientTree_t::progressHi failed\",\"where\":\"%s\",\"encountered\":%ld,\"expected\":%ld,\"numNode\":%d}\n",
			       __FUNCTION__, app.progress, app.progressHi, numNodes);
			exit(1);
		}

		fprintf(stderr, "[%s] ancientTree_t::foundTree() for numNode=%d called %ld times\n", app.timeAsString(), numNodes, app.progress);
	}
}

/*
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {genprogressContext_t} Application
 */
genprogressContext_t app;

/**
 * Construct a time themed prefix string for console logging
 *
 * @date 2020-03-18 18:08:48
 */
const char *timeAsString(void) {
	static char tstr[256];

	time_t t = time(0);
	struct tm *tm = localtime(&t);
	strftime(tstr, sizeof(tstr), "%F %T", tm);

	return tstr;
}

/**
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 * @date 2020-03-18 18:09:31
 */
void sigalrmHandler(int sig) {
	(void) sig; // trick compiler t see parameter is used

	if (app.opt_timer) {
		app.tick++;
		alarm(app.opt_timer);
	}
}

/**
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 * @date  2020-03-19 20:02:40
 */
void usage(char *const *argv, bool verbose, const genprogressContext_t *args) {
	fprintf(stderr, "usage:\t%s <numnode>\n\t%s --selftest\n", argv[0], argv[0]);
	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --ancient               Use ancient implementation for regression testing\n");
		fprintf(stderr, "\t-h --help                  This list\n");
		fprintf(stderr, "\t   --[no-]qntf             Enable QnTF-only mode [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_QNTF) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --[no-]paranoid         Enable expensive assertions [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                 Say more\n");
		fprintf(stderr, "\t   --selftest              Validate prerequisites\n");
		fprintf(stderr, "\t   --text                  Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>       Interval timer for verbose updates [default=%d]\n", args->opt_timer);
		fprintf(stderr, "\t-v --verbose               Say less\n");
	}
}

/**
 * Program main entry point
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 * @date   2020-03-18 18:13:24
 */
int main(int argc, char *const *argv) {
	setlinebuf(stdout);

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_ANCIENT = 1,
			LO_DEBUG,
			LO_NOPARANOID,
			LO_NOQNTF,
			LO_PARANOID,
			LO_QNTF,
			LO_SELFTEST,
			LO_TEXT,
			LO_TIMER,
			// short opts
			LO_HELP = 'h',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"ancient",     0, 0, LO_ANCIENT},
			{"debug",       1, 0, LO_DEBUG},
			{"help",        0, 0, LO_HELP},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"no-qntf",     0, 0, LO_NOQNTF},
			{"paranoid",    0, 0, LO_PARANOID},
			{"qntf",        0, 0, LO_QNTF},
			{"quiet",       2, 0, LO_QUIET},
			{"selftest",    0, 0, LO_SELFTEST},
			{"text",        0, 0, LO_TEXT},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",     2, 0, LO_VERBOSE},
			//
			{NULL,          0, 0, 0}
		};

		char optstring[128], *cp;
		cp = optstring;

		/* construct optarg */
		for (int i = 0; long_options[i].name; i++) {
			if (isalpha(long_options[i].val)) {
				*cp++ = (char) long_options[i].val;

				if (long_options[i].has_arg != 0)
					*cp++ = ':';
				if (long_options[i].has_arg == 2)
					*cp++ = ':';
			}
		}
		*cp = '\0';

		// parse long options
		int option_index = 0;
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case LO_ANCIENT:
				app.opt_ancient++;
				break;
			case LO_DEBUG:
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
				break;
			case LO_HELP:
				usage(argv, true, &app);
				exit(0);
			case LO_NOPARANOID:
				app.opt_flags &= ~context_t::MAGICMASK_PARANOID;
				break;
			case LO_NOQNTF:
				app.opt_flags &= ~context_t::MAGICMASK_QNTF;
				break;
			case LO_PARANOID:
				app.opt_flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_QNTF:
				app.opt_flags |= context_t::MAGICMASK_QNTF;
				break;
			case LO_QUIET:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose - 1;
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
				break;
			case LO_TEXT:
				app.opt_text++;
				break;
			case LO_TIMER:
				app.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
				break;
			case LO_VERBOSE:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose + 1;
				break;

			case '?':
				fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
				exit(1);
			default:
				fprintf(stderr, "getopt returned character code %d\n", c);
				exit(1);
		}
	}

	/*
	 * Program arguments
	 */
	if (app.opt_selftest) {
		// selftest mode
		if (argc - optind >= 0) {
		} else {
			usage(argv, false, &app);
			exit(1);
		}
	} else {
		// regular mode
		if (argc - optind >= 1) {
			app.arg_numNodes = (uint32_t) strtoul(argv[optind++], NULL, 10);
		} else {
			usage(argv, false, &app);
			exit(1);
		}
	}

	/*
	 * Expecting a lot of output, redirect to a file or kill the screen
	 */
	if (app.opt_text && isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

	/*
	 * register timer handler
	 */
	if (app.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(1);
	}

	/*
	 * Test
	 */
	if (app.opt_selftest) {
		performSelfTest(app);

		exit(0);
	}

	/*
	 * Invoke current code
	 */

	/*
	 * Invoke main entrypoint of application context
	 */
	generatorTree_t generator(app);
	app.main(generator);

	return 0;
}