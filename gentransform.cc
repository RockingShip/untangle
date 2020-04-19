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
 * transforms is not trivial and therefore pre-determined seperately.
 *
 * Skins are stored as LSB hexadecimal words where each nibble represents an endpoint
 * and a textual string.
 *
 * Basically, `gentransform` provides answers to 3 types of questions:
 * - Given a structure and skin, how would the result look like?
 * - How would a structure look like before a given skin was applied?
 * - Which skin should be put around a structure so that the structure looks ordered?
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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/stat.h>
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

	/// @var {copntext_t} I/O context
	context_t &ctx;

	/// @var {string} name of output database
	const char *arg_outputDatabase;

	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} --keep, do not delete output database in case of errors
	unsigned opt_keep;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {number} --selftest, perform a selftest
	unsigned opt_selftest;

	/**
	 * Constructor
	 */
	gentransformContext_t(context_t &ctx) : ctx(ctx) {
		// arguments and options
		arg_outputDatabase = NULL;
		opt_force = 0;
		opt_keep = 0;
		opt_selftest = 0;
		opt_text = 0;
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

		uint32_t iTransform = 0; // enumeration of skin
		uint32_t IM = 0; // bitmask of endpoints in use, initially empty

		for (uint32_t I = MAXSLOTS - 1; I != (uint32_t) -1; I--) {

			if (IM & (1 << I)) continue; // if current endpoint already in use, skip
			uint32_t HM = IM | (1 << I); // create new bitmap with current endpoint added

			for (uint32_t H = MAXSLOTS - 1; H != (uint32_t) -1; H--) {

				if (HM & (1 << H)) continue;
				uint32_t GM = HM | (1 << H);

				for (uint32_t G = MAXSLOTS - 1; G != (uint32_t) -1; G--) {

					if (GM & (1 << G)) continue;
					uint32_t FM = GM | (1 << G);

					for (uint32_t F = MAXSLOTS - 1; F != (uint32_t) -1; F--) {

						if (FM & (1 << F)) continue;
						uint32_t EM = FM | (1 << F);

						for (uint32_t E = MAXSLOTS - 1; E != (uint32_t) -1; E--) {

							if (EM & (1 << E)) continue;
							uint32_t DM = EM | (1 << E);

							for (uint32_t D = MAXSLOTS - 1; D != (uint32_t) -1; D--) {

								if (DM & (1 << D)) continue;
								uint32_t CM = DM | (1 << D);

								for (uint32_t C = MAXSLOTS - 1; C != (uint32_t) -1; C--) {

									if (CM & (1 << C)) continue;
									uint32_t BM = CM | (1 << C);

									for (uint32_t B = MAXSLOTS - 1; B != (uint32_t) -1; B--) {

										if (BM & (1 << B)) continue;
										uint32_t AM = BM | (1 << B);

										for (uint32_t A = MAXSLOTS - 1; A != (uint32_t) -1; A--) {

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
		assert (iTransform == MAXTRANSFORM);

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
		uint32_t nextFree = (MAXSLOTS + 1) * 2;

		/*
		 * For each transform name
		 */
		for (iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {

			// transform name
			const char *pStr = pNames[iTransform];
			uint32_t ix = 0; // active entry

			// point to entrypoint
			uint32_t pos = MAXSLOTS + 1;

			// process transform name upto but not including the last endpoint
			while (pStr[1]) {
				// which entry
				ix = pos + *pStr - 'a';

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
			assert (pIndex[ix] == 0);

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
			for (uint32_t ix = 0; ix < MAXSLOTS; ix++) {
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
	 * @return {uint32_t} - Transform enumeration id or IBIT if "not-found"
	 */
	inline uint32_t lookupTransform(const char *pName, uint32_t *pIndex) {
		assert(pIndex);

		// starting position in index
		uint32_t pos = MAXSLOTS + 1;

		// walk through states
		while (*pName) {
			pos = pIndex[pos + *pName - 'a'];
			pName++;
		}

		// what to return
		if (pos == 0)
			return IBIT; // "not-found"
		else if (~pos & IBIT)
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
	void main(database_t *pStore) {
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
		for (uint32_t t = 0; t < MAXTRANSFORM; t++)
			pStore->revTransformIds[t] = lookupTransform(pStore->revTransformNames[t], pStore->fwdTransformNameIndex);

		/*
		 * dump contents on request
		 */
		if (opt_text) {
			for (uint32_t t = 0; t < pStore->numTransform; t++)
				printf("%d\t%s\t%s\t%d\n", t, pStore->fwdTransformNames[t], pStore->revTransformNames[t], pStore->revTransformIds[t]);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Generated %d transforms\n", ctx.timeAsString(), pStore->numTransform);
	}

};

/**
 * @date 2020-04-15 13:50:53
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct gentransformSelftest_t : gentransformContext_t {

	/**
	 * Constructor
	 */
	gentransformSelftest_t(context_t &ctx) : gentransformContext_t(ctx) {
	}

	/**
	 * @date 2020-03-12 00:26:06
	 *
	 * Perform a selftest.
	 *
	 * - Test the index by performing lookups on all `MAXSLOT==9` transforms
	 * - Lookup of `""` should return the transparent transform
	 * - Verify that forward/reverse substitution counter each other
	 *
	 * @param {gentransformContext_t} app - program context
	 * @param {database_t} pStore - database just before `main()`
	 */
	void performSelfTestMatch(database_t *pStore) {

		unsigned numPassed = 0;

		// generate datasets
		this->createTransforms(pStore->fwdTransformData, pStore->fwdTransformNames, pStore->fwdTransformNameIndex, true); // forward transform
		this->createTransforms(pStore->revTransformData, pStore->revTransformNames, pStore->revTransformNameIndex, false); // reverse transform

		/*
		 * Test empty name
		 */
		{
			uint32_t tid = this->lookupTransform("", pStore->fwdTransformNameIndex);

			// test empty name is transparent skin
			if (tid != 0) {
				printf("{\"error\":\"failed empty name lookup\",\"where\":\"%s\",\"tid\":%d}\n",
				       __FUNCTION__, tid);
				exit(1);
			}

			// test transparent transform ([0]) is transparent
			for (unsigned k = 0; k < MAXSLOTS; k++) {
				if (pStore->fwdTransformNames[0][k] != (char) ('a' + k)) {
					printf("{\"error\":\"failed transparent forward\",\"where\":\"%s\",\"name\":\"%s\"}\n",
					       __FUNCTION__, pStore->fwdTransformNames[0]);
					exit(1);
				}
				if (pStore->revTransformNames[0][k] != (char) ('a' + k)) {
					printf("{\"error\":\"failed transparent reverse\",\"where\":\"%s\",\"name\":\"%s\"}\n",
					       __FUNCTION__, pStore->revTransformNames[0]);
					exit(1);
				}
			}
		}

		/*
		 * Perform two rounds, first with forward transform, then with reverse transform
		 */
		for (unsigned round = 0; round < 2; round++) {
			// setup data for this round
			transformName_t *pNames;
			uint32_t *pIndex;

			if (round == 0) {
				pNames = pStore->fwdTransformNames;
				pIndex = pStore->fwdTransformNameIndex;
			} else {
				pNames = pStore->revTransformNames;
				pIndex = pStore->revTransformNameIndex;
			}

			/*
			 * Lookup all names with different lengths
			 */
			for (uint32_t iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {
				for (unsigned iLen = 0; iLen < MAXSLOTS; iLen++) {
					/*
					 * Test if substring is a short name
					 */
					bool isShort = true;
					for (unsigned k = iLen; k < MAXSLOTS; k++) {
						if (pNames[iTransform][k] != (char) ('a' + k)) {
							isShort = false;
							break;
						}
					}

					// test if name can be truncated
					if (!isShort)
						continue;

					// truncate name
					pNames[iTransform][iLen] = 0;

					// lookup name
					uint32_t encountered = this->lookupTransform(pNames[iTransform], pIndex);

					// undo truncation
					pNames[iTransform][iLen] = 'a' + iLen;

					if (iTransform != encountered) {
						printf("{\"error\":\"failed lookup\",\"where\":\"%s\",\"encountered\":%d,\"round\":%d,\"iTransform\":%d,\"iLen\":%d,\"name\":\"%s\"}\n",
						       __FUNCTION__, encountered, round, iTransform, iLen, pNames[iTransform]);
						exit(1);
					}

					numPassed++;
				}
			}
		}

		/*
		 * Test that forward/reverse counter each other
		 */
		for (uint32_t iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {
			// setup test skin

			char skin[MAXSLOTS + 1];

			for (unsigned k = 0; k < MAXSLOTS; k++)
				skin[k] = 'a' + k;
			skin[MAXSLOTS] = 0;

			// apply forward skin

			char forward[MAXSLOTS + 1];

			for (unsigned k = 0; k < MAXSLOTS; k++)
				forward[k] = pStore->fwdTransformNames[iTransform][skin[k] - 'a'];
			forward[MAXSLOTS] = 0;

			// apply reserse skin

			char reverse[MAXSLOTS + 1];

			for (unsigned k = 0; k < MAXSLOTS; k++)
				reverse[k] = pStore->revTransformNames[iTransform][forward[k] - 'a'];
			reverse[MAXSLOTS] = 0;

			// test both are identical
			for (unsigned k = 0; k < MAXSLOTS; k++) {
				if (skin[k] != reverse[k]) {
					printf("{\"error\":\"failed forward/reverse\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"iTransform\":%d,\"forward\":\"%s\",\"reverse\":\"%s\"}\n",
					       __FUNCTION__, reverse, skin, iTransform, pStore->fwdTransformNames[iTransform], pStore->revTransformNames[iTransform]);
					exit(1);
				}
			}

			numPassed++;
		}

		fprintf(stderr, "[%s] %s() passed %d tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-03-15 12:13:13
	 *
	 * The list of transform names has repetitive properties which give the enumerated id's modulo properties.
	 *
	 * A modulo property is that the enumeration can be written as `"(row * numCols) + col"`.
	 * `"row"` and `"col"` are dimensions of a rectangle large enough to fit the entire collection of id'd.
	 *
	 * This can be illustrated by the following example with 5-letter transform names with 5! (=120) permutations.
	 * The example rectangle has 1*2*3 (=6) columns and 4*5 (=20) rows.
	 * The names are placed in a left-to-right, top-to-bottom sequence.
	 *
	 *     +<--------------COLUMNS------------->
	 *     ^ abcde bacde acbde cabde bcade cbade
	 *     | abdce badce adbce dabce bdace dbace
	 *     | acdbe cadbe adcbe dacbe cdabe dcabe
	 *     | bcdae cbdae bdcae dbcae cdbae dcbae
	 *     | abced baced acbed cabed bcaed cbaed
	 *     | abecd baecd aebcd eabcd beacd ebacd
	 *     | acebd caebd aecbd eacbd ceabd ecabd
	 *     | bcead cbead becad ebcad cebad ecbad
	 *     R abdec badec adbec dabec bdaec dbaec
	 *     O abedc baedc aebdc eabdc beadc ebadc
	 *     W adebc daebc aedbc eadbc deabc edabc
	 *     S bdeac dbeac bedac ebdac debac edbac
	 *     | acdeb cadeb adceb daceb cdaeb dcaeb
	 *     | acedb caedb aecdb eacdb ceadb ecadb
	 *     | adecb daecb aedcb eadcb deacb edacb
	 *     | cdeab dceab cedab ecdab decab edcab
	 *     | bcdea cbdea bdcea dbcea cdbea dcbea
	 *     | bceda cbeda becda ebcda cebda ecbda
	 *     | bdeca dbeca bedca ebdca debca edbca
	 *     v cdeba dceba cedba ecdba decba edcba
	 *
	 * Examining the lower-right cell the following `"placeholder/skin"` property applies:
	 *
	 *      `"cbade/cdeba -> edcba"`
	 *          ^     ^        ^
	 *          |     |        +- cell
	 *          |     +- first cell of grid row
	 *          +- first cell of grid column
	 *
	 * Rewriting all the names in `"placeholder/skin"` notation
	 *
	 *     +<--------------------------------COLUMNS------------------------------->
	 *     ^ abcde/abcde bacde/abcde acbde/abcde cabde/abcde bcade/abcde cbade/abcde
	 *     | abcde/abdce bacde/abdce acbde/abdce cabde/abdce bcade/abdce cbade/abdce
	 *     | abcde/acdbe bacde/acdbe acbde/acdbe cabde/acdbe bcade/acdbe cbade/acdbe
	 *     | abcde/bcdae bacde/bcdae acbde/bcdae cabde/bcdae bcade/bcdae cbade/bcdae
	 *     | abcde/abced bacde/abced acbde/abced cabde/abced bcade/abced cbade/abced
	 *     | abcde/abecd bacde/abecd acbde/abecd cabde/abecd bcade/abecd cbade/abecd
	 *     | abcde/acebd bacde/acebd acbde/acebd cabde/acebd bcade/acebd cbade/acebd
	 *     | abcde/bcead bacde/bcead acbde/bcead cabde/bcead bcade/bcead cbade/bcead
	 *     R abcde/abdec bacde/abdec acbde/abdec cabde/abdec bcade/abdec cbade/abdec
	 *     O abcde/abedc bacde/abedc acbde/abedc cabde/abedc bcade/abedc cbade/abedc
	 *     W abcde/adebc bacde/adebc acbde/adebc cabde/adebc bcade/adebc cbade/adebc
	 *     S abcde/bdeac bacde/bdeac acbde/bdeac cabde/bdeac bcade/bdeac cbade/bdeac
	 *     | abcde/acdeb bacde/acdeb acbde/acdeb cabde/acdeb bcade/acdeb cbade/acdeb
	 *     | abcde/acedb bacde/acedb acbde/acedb cabde/acedb bcade/acedb cbade/acedb
	 *     | abcde/adecb bacde/adecb acbde/adecb cabde/adecb bcade/adecb cbade/adecb
	 *     | abcde/cdeab bacde/cdeab acbde/cdeab cabde/cdeab bcade/cdeab cbade/cdeab
	 *     | abcde/bcdea bacde/bcdea acbde/bcdea cabde/bcdea bcade/bcdea cbade/bcdea
	 *     | abcde/bceda bacde/bceda acbde/bceda cabde/bceda bcade/bceda cbade/bceda
	 *     | abcde/bdeca bacde/bdeca acbde/bdeca cabde/bdeca bcade/bdeca cbade/bdeca
	 *     v abcde/cdeba bacde/cdeba acbde/cdeba cabde/cdeba bcade/cdeba cbade/cdeba
	 *
	 * To make the patterns more obvious, replace names by their enumerated id's
	 *
	 *     +<-----------------------COLUMNS---------------------->
	 *     | 0/(6* 0) 1/(6* 0) 2/(6* 0) 3/(6* 0) 4/(6* 0) 5/(6* 0)
	 *     | 0/(6* 1) 1/(6* 1) 2/(6* 1) 3/(6* 1) 4/(6* 1) 5/(6* 1)
	 *     | 0/(6* 2) 1/(6* 2) 2/(6* 2) 3/(6* 2) 4/(6* 2) 5/(6* 2)
	 *     | 0/(6* 3) 1/(6* 3) 2/(6* 3) 3/(6* 3) 4/(6* 3) 5/(6* 3)
	 *     | 0/(6* 4) 1/(6* 4) 2/(6* 4) 3/(6* 4) 4/(6* 4) 5/(6* 4)
	 *     | 0/(6* 5) 1/(6* 5) 2/(6* 5) 3/(6* 5) 4/(6* 5) 5/(6* 5)
	 *     | 0/(6* 6) 1/(6* 6) 2/(6* 6) 3/(6* 6) 4/(6* 6) 5/(6* 6)
	 *     | 0/(6* 7) 1/(6* 7) 2/(6* 7) 3/(6* 7) 4/(6* 7) 5/(6* 7)
	 *     R 0/(6* 8) 1/(6* 8) 2/(6* 8) 3/(6* 8) 4/(6* 8) 5/(6* 8)
	 *     O 0/(6* 9) 1/(6* 9) 2/(6* 9) 3/(6* 9) 4/(6* 9) 5/(6* 9)
	 *     W 0/(6*10) 1/(6*10) 2/(6*10) 3/(6*10) 4/(6*10) 5/(6*10)
	 *     S 0/(6*11) 1/(6*11) 2/(6*11) 3/(6*11) 4/(6*11) 5/(6*11)
	 *     | 0/(6*12) 1/(6*12) 2/(6*12) 3/(6*12) 4/(6*12) 5/(6*12)
	 *     | 0/(6*13) 1/(6*13) 2/(6*13) 3/(6*13) 4/(6*13) 5/(6*13)
	 *     | 0/(6*14) 1/(6*14) 2/(6*14) 3/(6*14) 4/(6*14) 5/(6*14)
	 *     | 0/(6*15) 1/(6*15) 2/(6*15) 3/(6*15) 4/(6*15) 5/(6*15)
	 *     | 0/(6*16) 1/(6*16) 2/(6*16) 3/(6*16) 4/(6*16) 5/(6*16)
	 *     | 0/(6*17) 1/(6*17) 2/(6*17) 3/(6*17) 4/(6*17) 5/(6*17)
	 *     | 0/(6*18) 1/(6*18) 2/(6*18) 3/(6*18) 4/(6*18) 5/(6*18)
	 *     v 0/(6*19) 1/(6*19) 2/(6*19) 3/(6*19) 4/(6*19) 5/(6*19)
	 *
	 * This implies that only knowledge of the first cell of each row and column are needed to reconstruct any cell name.
	 * The mechanism is called `"interleaving"`.
	 *
	 * @param {gentransformContext_t} pApp - program context
	 * @param {database_t} pStore - database just before `main()`
	 */
	void performSelfTestInterleave(database_t *pStore) {

		// shortcuts
		transformName_t *pFwdNames = pStore->fwdTransformNames;
		transformName_t *pRevNames = pStore->revTransformNames;

		unsigned numPassed = 0;

		// generate datasets
		this->createTransforms(pStore->fwdTransformData, pFwdNames, pStore->fwdTransformNameIndex, true); // forward transform
		this->createTransforms(pStore->revTransformData, pRevNames, pStore->revTransformNameIndex, false); // reverse transform

		unsigned numRows, numCols = 1;

		/*
		 * Apply test to eash possible grid layout
		 */
		for (unsigned iInterleave = 1; iInterleave <= MAXSLOTS; iInterleave++) {
			// number of columns must be iInterleave!
			numCols *= iInterleave;
			numRows = MAXTRANSFORM / numCols;
			assert(numCols * numRows == MAXTRANSFORM);

			/*
			 * walk through cells.
			 */
			for (uint32_t row = 0; row < numRows; row++) {
				for (uint32_t col = 0; col < numCols; col++) {

					/*
					 * Validate "<first cell of grid column>/<first cell of grid row>" == "<cell>"
					 */

					char cell[10];

					// construct cell name
					cell[0] = pFwdNames[row * numCols][pFwdNames[col][0] - 'a'];
					cell[1] = pFwdNames[row * numCols][pFwdNames[col][1] - 'a'];
					cell[2] = pFwdNames[row * numCols][pFwdNames[col][2] - 'a'];
					cell[3] = pFwdNames[row * numCols][pFwdNames[col][3] - 'a'];
					cell[4] = pFwdNames[row * numCols][pFwdNames[col][4] - 'a'];
					cell[5] = pFwdNames[row * numCols][pFwdNames[col][5] - 'a'];
					cell[6] = pFwdNames[row * numCols][pFwdNames[col][6] - 'a'];
					cell[7] = pFwdNames[row * numCols][pFwdNames[col][7] - 'a'];
					cell[8] = pFwdNames[row * numCols][pFwdNames[col][8] - 'a'];
					cell[9] = 0;

					// check
					if (strcmp(cell, pFwdNames[row * numCols + col]) != 0) {
						printf("{\"error\":\"failed merge\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"numCols\":%d,\"numRows\":%d,\"col\":%d,\"colName\":\"%s\",\"row\":%d,\"rowName\":\"%s\"}\n",
						       __FUNCTION__, cell, pFwdNames[row * numCols + col], numCols, numRows, col, pFwdNames[col], row * numCols, pFwdNames[row * numCols]);
						exit(1);
					}

					numPassed++;

					/*
					 * If the above applies, then the following should be valid
					 */

					assert(pFwdNames[(row * numCols) + col][0] == pFwdNames[(row * numCols)][pFwdNames[col][0] - 'a']);
					assert(pFwdNames[(row * numCols) + col][1] == pFwdNames[(row * numCols)][pFwdNames[col][1] - 'a']);
					assert(pFwdNames[(row * numCols) + col][2] == pFwdNames[(row * numCols)][pFwdNames[col][2] - 'a']);
					assert(pFwdNames[(row * numCols) + col][3] == pFwdNames[(row * numCols)][pFwdNames[col][3] - 'a']);
					assert(pFwdNames[(row * numCols) + col][4] == pFwdNames[(row * numCols)][pFwdNames[col][4] - 'a']);
					assert(pFwdNames[(row * numCols) + col][5] == pFwdNames[(row * numCols)][pFwdNames[col][5] - 'a']);
					assert(pFwdNames[(row * numCols) + col][6] == pFwdNames[(row * numCols)][pFwdNames[col][6] - 'a']);
					assert(pFwdNames[(row * numCols) + col][7] == pFwdNames[(row * numCols)][pFwdNames[col][7] - 'a']);
					assert(pFwdNames[(row * numCols) + col][8] == pFwdNames[(row * numCols)][pFwdNames[col][8] - 'a']);

					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][0] - 'a'] == pFwdNames[col][0]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][1] - 'a'] == pFwdNames[col][1]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][2] - 'a'] == pFwdNames[col][2]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][3] - 'a'] == pFwdNames[col][3]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][4] - 'a'] == pFwdNames[col][4]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][5] - 'a'] == pFwdNames[col][5]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][6] - 'a'] == pFwdNames[col][6]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][7] - 'a'] == pFwdNames[col][7]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][8] - 'a'] == pFwdNames[col][8]);
				}
			}
		}

		fprintf(stderr, "[%s] %s() passed %d tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
	}

};

/*
 * I/O context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} I/O context
 */
context_t ctx;

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {gentransformContext_t} Application context
 */
gentransformSelftest_t app(ctx);

/**
 * @date 2020-03-11 23:06:35
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int sig) {
	if (!app.opt_keep && app.arg_outputDatabase) {
		remove(app.arg_outputDatabase);
	}
	exit(1);
}

/**
 * @date 2020-03-11 23:06:35
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int sig) {
	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

/**
 * @date 2020-03-11 22:30:36
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s [<output.db>]  -- Create initial database containing transforms\n", argv[0]);
	fprintf(stderr, "       %s --selftest   -- Test prerequisites\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force           Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help            This list\n");
		fprintf(stderr, "\t   --keep            Do not delete output database in case of errors\n");
		fprintf(stderr, "\t-q --quiet           Say more\n");
		fprintf(stderr, "\t   --selftest        Validate prerequisites\n");
		fprintf(stderr, "\t   --text            Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds> Interval timer for verbose updates [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose         Say less\n");

	}
}

/**
 * @date 2020-03-06 20:22:23
 *
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context.
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
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
			LO_DEBUG = 1,
			LO_FORCE,
			LO_KEEP,
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
			{"debug",    1, 0, LO_DEBUG},
			{"force",    0, 0, LO_FORCE},
			{"help",     0, 0, LO_HELP},
			{"keep",     0, 0, LO_KEEP},
			{"quiet",    2, 0, LO_QUIET},
			{"selftest", 0, 0, LO_SELFTEST},
			{"text",     0, 0, LO_TEXT},
			{"timer",    1, 0, LO_TIMER},
			{"verbose",  2, 0, LO_VERBOSE},
			//
			{NULL,       0, 0, 0}
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
			case LO_DEBUG:
				ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_FORCE:
				app.opt_force++;
				break;
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_KEEP:
				app.opt_keep++;
				break;
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
				break;
			case LO_TEXT:
				app.opt_text++;
				break;
			case LO_TIMER:
				ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_VERBOSE:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
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
	if (argc - optind >= 1) {
		app.arg_outputDatabase = argv[optind++];
	}

	if (0) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (app.arg_outputDatabase && !app.opt_force) {
		struct stat sbuf;

		if (!stat(app.arg_outputDatabase, &sbuf)) {
			fprintf(stderr, "%s already exists. Use --force to overwrite\n", app.arg_outputDatabase);
			exit(1);
		}
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Create database
	 */

	database_t store(ctx);

	// set section sizes to be created
	store.maxTransform = MAXTRANSFORM;
	store.transformIndexSize = MAXTRANSFORMINDEX;

	// create memory-based store
	store.create();

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);
	if (ctx.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", ctx.totalAllocated);

	/*
	 * Test prerequisite
	 */
	if (app.opt_selftest) {
		// perform selfcheck
		app.performSelfTestMatch(&store);
		app.performSelfTestInterleave(&store);
		exit(0);
	}

	/*
	 * Invoke main entrypoint of application context
	 */
	app.main(&store);

	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

#if defined(ENABLE_JANSSON)
	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY && !app.opt_text) {
		json_t *jResult = json_object();
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.jsonInfo(jResult);
		printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		if (!isatty(1))
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}
#endif

	return 0;
}
