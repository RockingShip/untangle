//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-11 21:53:16
 *
 * `gentransform` creates the initial database containing transforms for forward and reverse skin mappings.
 *
 * Historically, skins were called transforms because they instruct how to connect endpoints
 * to ordered structures, basically transforming them to the structure being requested.
 * In code the variable `tid` represents the enumerated id of skins/transforms.
 *
 * The collection of transforms are all the endpoint permutations a 4-node/9-endpoint structure can have.
 *
 * This version of `untangle` focuses on transforms with 9 endpoints (`MAXSLOTS`==9).
 * There are 9! different transforms (`MAXTRANSFORM`==362880)
 *
 * Each transforms also has a reverse mapping. This is used to "undo" the effect of an
 * applied transforms. For example `"bca?/bca"` would have the effect `"a->b, b->c, c->a"`
 * resulting in "cab?". The reverse transforms would be `"cab?/cab"`. Determining a reverse
 * transforms is not trivial and therefore pre-determined separately.
 *
 * Skins are stored as LSB hexadecimal words where each nibble represents an endpoint
 * and a textual string.
 *
 * Basically, `gentransform` provides answers to 3 types of questions:
 * - Given a structure and skin, how would the result look like?
 * - How would a structure look like before a given skin was applied?
 * - Which skin should be put around a structure so that the structure looks ordered?
 *
 * @date 2020-05-01 19:18:15
 *
 * Text modes:
 *
 * `--test`   Verbose
 *
 *              <tid> <forwardName> <reverseName> <reverseId>
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
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"

/*
 * Constants
 */

/// @constant {number} MAXTRANSFORMINDEX - Number of blocks times block size
#define MAXTRANSFORMINDEX ((MAXSLOTS + 1) + (1 + (1 + (1 + (1 + (1 + (1 + (1 + (1 + 2) * 3) * 4) * 5) * 6) * 7) * 8) * 9) * (MAXSLOTS + 1))

/**
 * @date 2020-03-11 22:53:39
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct gentransformContext_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {string} name of output database
	const char *arg_outputDatabase;

	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;

	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	/**
	 * Constructor
	 */
	gentransformContext_t(context_t &ctx) : ctx(ctx) {
		// arguments and options
		arg_outputDatabase = NULL;
		opt_force          = 0;
		opt_text           = 0;
		pStore		   = NULL;
	}
	
	/*
	 * @date 2021-10-24 10:57:19
	 * 
	 * Connect database
	 */
	void connect(database_t &db) {
		this->pStore = &db;
	}

	/**
	 * @date 2020-03-12 00:39:44
	 *
	 * Create all forward transforms.
	 *
	 * if `"bca"` is the forward transform then `"bca?/bca"` would have the
	 * effect `"a->b, b->c, c->a"` resulting in "cab?"
	 *
	 * @param {number[MAXTRANSFORM} pData - output array hexadecimal transforms
	 * @param {string[MAXSLOTS+1][MAXTRANSFORM]} pString - output array if fixed sized transform names
	 * @param {number[MAXTRANSFORMINDEX]} pIndex - output name lookup index
	 * @param {boolean} isForward - `true` for forward mapping and `false` for reverse mapping
	 */
	void createTransforms(uint64_t *pData, transformName_t *pNames, uint32_t *pIndex, bool isForward) {

		/*
		 * Generate all permutations:
		 *
		 * The leftmost endpoint (indexed by placeholder `'a'`) in a skin notation is the fastest changing
		 * The rightmost endpoint (indexed by placeholder `'i'`) is the slowest decrementing
		 *
		 * The leftmost (`'a'`) should be the most inner loop
		 * The rightmost (`'i'`) should be the most outer loop
		 */

		assert(MAXSLOTS == 9);

		unsigned iTransform = 0; // enumeration of skin
		unsigned IM         = 0; // bitmask of endpoints in use, initially empty

		for (unsigned I = MAXSLOTS - 1; I != (unsigned) -1; I--) {

			if (IM & (1 << I)) continue; // if current endpoint already in use, skip
			unsigned HM = IM | (1 << I); // create new bitmap with current endpoint added

			for (unsigned H = MAXSLOTS - 1; H != (unsigned) -1; H--) {

				if (HM & (1 << H)) continue;
				unsigned GM = HM | (1 << H);

				for (unsigned G = MAXSLOTS - 1; G != (unsigned) -1; G--) {

					if (GM & (1 << G)) continue;
					unsigned FM = GM | (1 << G);

					for (unsigned F = MAXSLOTS - 1; F != (unsigned) -1; F--) {

						if (FM & (1 << F)) continue;
						unsigned EM = FM | (1 << F);

						for (unsigned E = MAXSLOTS - 1; E != (unsigned) -1; E--) {

							if (EM & (1 << E)) continue;
							unsigned DM = EM | (1 << E);

							for (unsigned D = MAXSLOTS - 1; D != (unsigned) -1; D--) {

								if (DM & (1 << D)) continue;
								unsigned CM = DM | (1 << D);

								for (unsigned C = MAXSLOTS - 1; C != (unsigned) -1; C--) {

									if (CM & (1 << C)) continue;
									unsigned BM = CM | (1 << C);

									for (unsigned B = MAXSLOTS - 1; B != (unsigned) -1; B--) {

										if (BM & (1 << B)) continue;
										unsigned AM = BM | (1 << B);

										for (unsigned A = MAXSLOTS - 1; A != (unsigned) -1; A--) {

											if (AM & (1 << A)) continue;

											/*
											 * Construct hexadecimal version of transform
											 */

											uint64_t data = 0;

											if (isForward) {
												/*
												 * encode as `endpoint` << (`placeholder` * 4)
												 * this uses `placeholder` as index to be replaced by `endpoint`
												 */
												data |= (uint64_t) A << (0 * 4);
												data |= (uint64_t) B << (1 * 4);
												data |= (uint64_t) C << (2 * 4);
												data |= (uint64_t) D << (3 * 4);
												data |= (uint64_t) E << (4 * 4);
												data |= (uint64_t) F << (5 * 4);
												data |= (uint64_t) G << (6 * 4);
												data |= (uint64_t) H << (7 * 4);
												data |= (uint64_t) I << (8 * 4);
											} else {
												/*
												 * encode as `placeholder` << (`endpoint` * 4)
												 * this uses `endpoint` as index to be replaced by `placeholder`
												 */
												data |= (uint64_t) 0 << (A * 4);
												data |= (uint64_t) 1 << (B * 4);
												data |= (uint64_t) 2 << (C * 4);
												data |= (uint64_t) 3 << (D * 4);
												data |= (uint64_t) 4 << (E * 4);
												data |= (uint64_t) 5 << (F * 4);
												data |= (uint64_t) 6 << (G * 4);
												data |= (uint64_t) 7 << (H * 4);
												data |= (uint64_t) 8 << (I * 4);
											}

											// store result in binary part
											pData[iTransform] = data;

											// decode binary into a string

											for (unsigned k = 0; k < MAXSLOTS; k++) {
												pNames[iTransform][k] = "abcdefghi"[data & 15];
												data >>= 4;
											}
											pNames[iTransform][MAXSLOTS] = 0;

											iTransform++;
										}
									}
								}
							}
						}
					}
				}
			}
		}

		// sanity check
		assert(iTransform == MAXTRANSFORM);

		/*
		 * Create a state based index of the transform names for fast text-to-id lookups
		 * The index is a collection of blocks containing `MAXSLOT+1` entries.
		 * Each entry represents an endpoint and its content points to the block of the next state.
		 * The first block contains all zeros and is used to indicate "not-found".
		 * Once zero ("not-found"), always zero ("not-found") until the name is exhausted.
                 *
		 * If an entry is found with IBIT set then that indicates the end-state and the value is the transform id.
		 * The name should be exhausted. Longer names should be considered "not-found".
		 *
		 * For short names the last entry in a block contains the transform id with IBIT set.
		 *
		 * Below is the lookup of `"cba"`. `"0"` indicates `not-found`
		 *
		 *   +---+
		 *  >| a |-
		 *   | b |-    +---+
		 *   | c |---->| a |-    +---+
		 *   +---+     | b |---->| a |----= id^IBIT
		 *             | c |-0   | b |-0
		 *             +---+     | c |-0
		 *                       +---+
		 *
		 */

		/*
		 * Clear the index
		 */
		for (unsigned i = 0; i < MAXTRANSFORMINDEX; i++)
			pIndex[i] = 0;

		// first block is all zero, second block is entrypoint, third block is first-free
		unsigned nextFree = (MAXSLOTS + 1) * 2;

		/*
		 * For each transform name
		 */
		for (iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {

			// transform name
			const char *pStr = pNames[iTransform];
			unsigned   ix; // active entry

			// point to entrypoint
			unsigned pos = MAXSLOTS + 1;

			// process transform name upto but not including the last endpoint
			while (pStr[1]) {
				// which entry
				ix  = pos + *pStr - 'a';

				// test if slot for endpoint inuse
				if (pIndex[ix] == 0) {
					// no, create
					pIndex[ix] = nextFree;
					nextFree += MAXSLOTS + 1;
				}
				// what is the next block
				pos = pIndex[ix];

				// next position in name
				pStr++;
			}

			// last endpoint, which entry
			ix = pos + *pStr - 'a';

			// the entry containing the transform id should be free
			assert(pIndex[ix] == 0);

			// place transform id into entry with IBIT set to indicate terminated
			pIndex[ix] = iTransform ^ IBIT;
		}

		assert(nextFree == MAXTRANSFORMINDEX);

		/*
		 * Set the defaults for short names.
		 * Work backwards making it possible to deference "next pointers"
		 */

		// work backwards
		while (nextFree >= (MAXSLOTS + 1)) {
			// find first used entry which is also the default entry
			for (unsigned ix = 0; ix < MAXSLOTS; ix++) {
				if (pIndex[nextFree + ix] != 0) {
					// is it a node or leaf
					if (pIndex[nextFree + ix] & IBIT) {
						// leaf, default is node id
						pIndex[nextFree + MAXSLOTS] = pIndex[nextFree + ix];
					} else {
						// node, default is propagated from the next state
						pIndex[nextFree + MAXSLOTS] = pIndex[pIndex[nextFree + ix] + MAXSLOTS];
					}
					// next block
					break;
				}
			}
			// next block
			nextFree -= (MAXSLOTS + 1);
		}
	}

	/**
	 * @date 2020-03-12 10:28:05
	 *
	 * Lookup a transform name and return its matching enumeration id.
	 * Transform names can be short meaning that trailing endpoints which are in sync can be omitted.
	 * Example: For `"bdacefghi"`, `"bdac"` is the minimum transform name and `"efghi"` is the "long" part.
	 *
	 * NOTE: Transform names must be syntactically correct:
	 *  - No longer than `MAXSLOTS` characters
	 *  - Consisting of exclusively the lowercase letters `'a'` to `'i'` (for `MAXSLOTS`==9)
	 *
	 * @param {string} pName - Transform name
  	 * @param {number[MAXTRANSFORMINDEX]} pIndex - output name lookup index
	 * @return {number} - Transform enumeration id or IBIT if "not-found"
	 */
	inline unsigned lookupTransform(const char *pName, uint32_t *pIndex) {
		assert(pIndex);

		// starting position in index
		unsigned pos = MAXSLOTS + 1;

		// walk through states
		while (*pName) {
			pos = pIndex[pos + *pName - 'a'];
			pName++;
		}

		// what to return
		if (pos == 0)
			return IBIT; // "not-found"
		else if (!(pos & IBIT))
			return pIndex[pos + MAXSLOTS] & ~IBIT; // short names
		else
			return pos & ~IBIT; // long name
	}

	/**
	 * @date 2020-03-12 19:58:14
	 *
	 * Main entrypoint
	 *
	 * @param {database_t} pStore - data store
	 */
	void main(void) {
		/*
		 * generate datasets
		 */
		this->createTransforms(pStore->fwdTransformData, pStore->fwdTransformNames, pStore->fwdTransformNameIndex, true); // forward
		this->createTransforms(pStore->revTransformData, pStore->revTransformNames, pStore->revTransformNameIndex, false); // reverse
		pStore->numTransform = MAXTRANSFORM;
		assert(pStore->numTransform == pStore->maxTransform);

		/*
		 * Reverse Id's are the lookups of reverse names
		 */
		for (unsigned t = 0; t < MAXTRANSFORM; t++)
			pStore->revTransformIds[t] = lookupTransform(pStore->revTransformNames[t], pStore->fwdTransformNameIndex);

		/*
		 * dump contents on request
		 */
		if (opt_text) {
			for (unsigned t = 0; t < pStore->numTransform; t++)
				printf("%u\t%s\t%s\t%u\n", t, pStore->fwdTransformNames[t], pStore->revTransformNames[t], pStore->revTransformIds[t]);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Generated %u transforms\n", ctx.timeAsString(), pStore->numTransform);

		/*
		 * allocate evaluators, will be generated on save
		 */
		pStore->numEvaluator = tinyTree_t::TINYTREE_NEND * MAXTRANSFORM;
	}

};
