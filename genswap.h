/*
 * @date 2020-05-02 23:02:57
 *
 * `genswap` analyses endpoint symmetry.
 * Swapping is relative symmetry in contrast to transformId which references an absolute layout.
 *
 * It is used for level-5 normalisation where sid's are known early and tid's known late.
 *
 * `swaps[]` is a list of transform masks that are applied to a structure that requires level-5 normalisation.
 * - Populate `MAXSLOT` slots with the contents of the endpoints.
 * - Slots are usually assigned in tree walking order. Order is not really important as long as it is consistent.
 * - Apply transform to slots
 * - Compare and reject the worde of th two
 * - Repeat for all remaining transforms.
 *
 * NOTE: it's not about applying a transform on the endpoints, like "abc/cab" -> "cab"
 *       It's applying transforms to the slots. "{slots[2],slots[0],slots[1]}"
 *
 * Example:
 *   `"acb++"`, sid=9 with swaps `"[bac,bca,acb]"`.
 *
 * Starting slots:
 *   `"slots[] = {'c','a','b'}`"
 *
 * Apply first transform `"bac"`:
 *      test[transform[0]-'a'] = test[1] = slots[0]
 *      test[transform[1]-'a'] = test[0] = slots[1]
 *      test[transform[2]-'a'] = test[2] = slots[2]
 *
 *      slots="cab", test="acb".
 *      Choose for "test[]" in favour of "slots"
 *
 * Apply second transform `"bca"`:
 *      test[transform[0]-'a'] = test[1] = slots[0];
 *      test[transform[1]-'a'] = test[2] = slots[1];
 *      test[transform[2]-'a'] = test[0] = slots[2];
 *
 *      slots="acb", test="bac".
 *      Keep "slots[]"
 *
 * Apply third and final transform `"acb"`:
 *      test[transform[0]-'a'] = test[0] = slots[0];
 *      test[transform[1]-'a'] = test[2] = slots[1];
 *      test[transform[2]-'a'] = test[1] = slots[2];
 *
 *      slots="acb", test="abc".
 *      Choose for "test[]" as final result
 *
 * Text modes:
 *
 * `--text[=1]` Display swaps as the generator progresses.
 *              Can be used for the `--load=<file>` option.
 *
 *              <name> <tid> <tid> ...
 *
 * `--test=3`   Display swaps when they are written to the database
 *              NOTE: same format as `--text=1`
 *
 *              <name> <tid> <tid> ...
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
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "dbtool.h"
#include "metrics.h"

/**
 * @date 2020-05-02 23:02:57
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genswapContext_t : dbtool_t {

	enum {
		/// @constant {number} - `--text` modes
		OPTTEXT_WON     = 1,
		OPTTEXT_COMPARE = 2,
		OPTTEXT_BRIEF   = 3,
		OPTTEXT_VERBOSE = 4,

	};

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} force overwriting of database if already exists
	unsigned   opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned   opt_generate;
	/// @var {string} name of file containing swaps
	const char *opt_load;
	/// @var {number} Sid range upper bound
	unsigned   opt_sidHi;
	/// @var {number} Sid range lower bound
	unsigned   opt_sidLo;
	/// @var {number} task Id. First task=1
	unsigned   opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned   opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned   opt_text;

	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	/// @var {number} current version incarnation
	uint32_t iVersion;
	/// @var {number} duplicate swaps in database
	unsigned skipDuplicate;
	/// @var {number[]} - Versioned memory of active swaps/transforms
	uint32_t *swapsActive;
	/// @var {number[]} - List of found swaps/transforms for signature under investigation. IBIT set indicates they are disabled
	uint32_t *swapsFound;
	/// @var {number[]} - Versioned memory of active swaps/transforms
	uint64_t *swapsWeight;
	/// @var {number[]} - Weights to assist choosing in case of draws
	unsigned tidHi[MAXSLOTS + 1];
	/// @var {database_t} - Temporary database to store/group swap alternatived for `allowDeactivate()`
	database_t tmpdb;

	/**
	 * Constructor
	 */
	genswapContext_t(context_t &ctx) : dbtool_t(ctx), tmpdb(ctx) {
		// arguments and options
		opt_force          = 0;
		opt_generate       = 1;
		arg_inputDatabase  = NULL;
		opt_load           = NULL;
		arg_outputDatabase = NULL;
		opt_sidHi          = 0;
		opt_sidLo          = 0;
		opt_taskId         = 0;
		opt_taskLast       = 0;
		opt_text           = 0;

		iVersion = 0;
		pStore   = NULL;

		skipDuplicate = 0;
		swapsActive   = (uint32_t *) ctx.myAlloc("genswapContext_t::swapsActive", MAXTRANSFORM, sizeof(*swapsActive));
		swapsFound    = (uint32_t *) ctx.myAlloc("genswapContext_t::swapsFound", MAXTRANSFORM, sizeof(*swapsFound));
		swapsWeight   = (uint64_t *) ctx.myAlloc("genswapContext_t::swapsWeight", MAXTRANSFORM, sizeof(*swapsWeight));
		for (unsigned j = 0; j <= MAXSLOTS; j++)
			tidHi[j] = 0;
		
		/*
		 * prepare tmpdb
		 */
		tmpdb.maxImprint       = MAXTRANSFORM;
		tmpdb.imprintIndexSize = ctx.nextPrime(tmpdb.maxImprint * 5);
		tmpdb.reallocateSections(database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX);
		// enabled versioned memory
		tmpdb.enableVersioned();
	}

	/**
	 * @date 2020-05-03 21:53:17
	 *
	 * Release system resources
	 */
	~genswapContext_t() {
		ctx.myFree("database_t::swapsActive", swapsActive);
		ctx.myFree("database_t::swapsFound", swapsFound);
		ctx.myFree("database_t::swapsWeight", swapsWeight);
	}

	/*
	 * @date 2021-10-24 10:57:19
	 * 
	 * Connect database and continue initialisation
	 */
	void connect(database_t &db) {
		this->pStore = &db;

		/*
		 * get upper limit for tid's for given number of placeholders
		 */
		assert(MAXSLOTS == 9);
		this->tidHi[0] = 0;
		this->tidHi[1] = pStore->lookupTransform("a", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[2] = pStore->lookupTransform("ba", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[3] = pStore->lookupTransform("cba", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[4] = pStore->lookupTransform("dcba", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[5] = pStore->lookupTransform("edcba", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[6] = pStore->lookupTransform("fedcba", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[7] = pStore->lookupTransform("gfedcba", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[8] = pStore->lookupTransform("hgfedcba", pStore->fwdTransformNameIndex) + 1;
		this->tidHi[9] = MAXTRANSFORM;
		assert(this->tidHi[2] == 2 && this->tidHi[3] == 6 && this->tidHi[4] == 24 && this->tidHi[5] == 120 && this->tidHi[6] == 720 && this->tidHi[7] == 5040 && this->tidHi[8] == 40320);

		/*
		 * Determine weights of transforms. More shorter cyclic loops the better
		 */

		for (unsigned iTid = 0; iTid < pStore->numTransform; iTid++) {
			this->swapsWeight[iTid] = 0;

			// count cycles
			unsigned      found  = 0;
			const char    *pName = pStore->fwdTransformNames[iTid];
			for (unsigned j      = 0; j < MAXSLOTS; j++) {
				if (!(found & (1 << (pName[j] - 'a')))) {
					// new starting point
					unsigned      w = 1;
					// walk cycle
					for (unsigned k = pName[j] - 'a'; ~found & (1 << k); k = pName[k] - 'a') {
						w *= (MAXSLOTS + 1); // weight is power of length
						found |= 1 << k;
					}
					// update wight
					this->swapsWeight[iTid] += w;
				}
			}
		}
	}

	/**
	 * @date 2020-05-04 23:54:12
	 *
	 * Given a list of Transforms, some deactivated by having their IBIT set.
	 * Apply transform to collection to determine if the results are redundant
	 * Mark the result after collecting
	 *
	 * @param {uint32_t} tidPrime - which transform to use
	 * @param {numEntry} numEntry - total number of transforms
	 * @param {uint32_t[]} pEntry - list of transforms
	 * @return {number} - number of entries still active
	 */
	unsigned __attribute__((optimize("O0"))) countNextActive(uint32_t tidPrime, unsigned numSwaps, uint32_t *pSwap) {
		// result
		unsigned numActiveNext = 0;

		// bump version number
		this->iVersion++;

		// get name of request prime transform
		const char *pPrime = pStore->fwdTransformNames[tidPrime];

		for (unsigned j = 0; j < numSwaps; j++) {
			// get original entry
			unsigned   tidOrig = pSwap[j] & ~IBIT;

			// apply transform to slots
			const char *pOrig     = pStore->fwdTransformNames[tidOrig];
			unsigned   tidSwapped = pStore->lookupTransformSlot(pOrig, pPrime, pStore->fwdTransformNameIndex);
			const char *pSwapped  = pStore->fwdTransformNames[tidSwapped];

			/*
			 * The result must already be present as a permutation within the collection, otherwise `tidPrime` is invalid
			 */
			bool found = false;
			for (unsigned k = 0; k < numSwaps; k++) {
				if ((pSwap[k] & ~IBIT) == tidSwapped) {
					found = true;
					break;
				}
			}
			if (!found)
				return 0;

			/*
			 * Compare pOrig/pSwap
			 */
			int           cmp = 0;
			for (unsigned k   = 0; k < MAXSLOTS; k++) {
				cmp = pOrig[k] - pSwapped[k];
				if (cmp)
					break;
			}

			if (cmp < 0) {
				// original is better
				if (this->swapsActive[tidOrig] != iVersion) {
					numActiveNext++;
					this->swapsActive[tidOrig] = iVersion;
				}
			} else if (cmp > 0) {
				// swapped is better
				if (this->swapsActive[tidSwapped] != iVersion) {
					numActiveNext++;
					this->swapsActive[tidSwapped] = iVersion;
				}
			} else {
				assert(0);
			}
		}

		return numActiveNext;
	}

	/*
	 * @date 2021-10-29 11:54:57
	 * 
	 * Test if deactivating a swap-tid will render the remaining set invalid
	 * Based on `selftest.cc:performSelfTestSwaps()`
	 * 
	 * NOTE: only the active elements are used
	 * 
	 * WARNING: CPU intensive
	 */
	bool __attribute__((optimize("O0"))) allowDeactivate(uint32_t sid, unsigned numSwaps, uint32_t *pSwap, uint32_t excludeTid) {
		signature_t *pSignature = pStore->signatures + sid;

		/*
		 * Iterate through all swaps
		 */
		tinyTree_t tree(ctx);

		/*
		 * Reset imprint section
		 */
		tmpdb.InvalidateVersioned();
		tmpdb.numImprint = pStore->IDFIRST;

		/*
		 * Find until upper limit for slot permutations
		 */
		unsigned high = tidHi[pSignature->numPlaceholder];
		for (uint32_t iTid = 0; iTid < high; iTid++) {
			// load tree 
			tree.loadStringFast(pSignature->name, pStore->fwdTransformNames[iTid]);

			// evaluate tree
			tree.eval(pStore->fwdEvaluator);

			// perform swapping
			uint32_t tidSlot = dbtool_t::sidSwapTidList(*pStore, iTid, pSignature->numPlaceholder, numSwaps, pSwap, excludeTid);

			// lookup imprint
			uint32_t ix = tmpdb.lookupImprint(pStore->fwdEvaluator[tree.root]);
			if (tmpdb.imprintVersion[ix] != tmpdb.iVersion) {
				// first time
				uint32_t iImprint = tmpdb.addImprint(pStore->fwdEvaluator[tree.root]);

				// save sid/tidSlot
				imprint_t *pImprint = tmpdb.imprints + iImprint;
				pImprint->sid = sid;
				pImprint->tid = tidSlot;

				tmpdb.imprintIndex[ix] = iImprint;
				tmpdb.imprintVersion[ix] = tmpdb.iVersion;
			} else {
				// followups
				uint32_t iImprint = tmpdb.imprintIndex[ix];

				// verify that all synonyms share the same tidSlot
				imprint_t *pImprint = tmpdb.imprints + iImprint;
				assert(pImprint->sid == sid);
				if (pImprint->tid != tidSlot)
					return false;
			}
		}

		return true;
	}


	/**
	 * @date 2020-05-02 23:06:26
	 *
	 * Build a collection of transforms such that after applying/rewriting all to the dataset, all end-point symmetry has been removed.
	 *
	 * Create a list of transforms that will create a collection of all permutation.
	 * Applying one of the transforms to the collection will divide the collection into pairs, one being better ordered than the other.
	 * Drop all the worse alternatives.
	 * Repeat applying other transforms until the collection consists of a single transparent (normalised) name.
	 *
	 * @param {signature_t} pName - signature requiring swaps
	 * @return {number} swapId
	 */
	unsigned __attribute__((optimize("O0"))) foundSignatureSwap(const char *pName) {

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) numSwap=%u(%.0f%%) | skipDuplicate=%u",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numSwap, pStore->numSwap * 100.0 / pStore->maxSwap,
					skipDuplicate);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d numSwap=%u(%.0f%%) | skipDuplicate=%u",
					ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - this->opt_sidLo) * 100.0 / (ctx.progressHi - this->opt_sidLo), etaH, etaM, etaS,
					pStore->numSwap, pStore->numSwap * 100.0 / pStore->maxSwap,
					skipDuplicate);
			}

			ctx.tick = 0;
		}

		/*
		 * lookup signature
		 */

		unsigned       ix  = pStore->lookupSignature(pName);
		const unsigned sid = pStore->signatureIndex[ix];
		if (sid == 0)
			ctx.fatal("\n{\"error\":\"missing signature\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"progress\":%lu}\n",
				  __FUNCTION__, __FILE__, __LINE__, pName, ctx.progress);

		tinyTree_t tree(ctx);

		signature_t *pSignature = pStore->signatures + sid;

		/*
		 * Create a list of transforms representing all permutations
		 */

		tree.loadStringFast(pSignature->name);

		// put untransformed result in reverse transform
		// NOTE: `fwdEvaluator[0]` is identical to `revEvaluator[0]` 
		tree.eval(pStore->revEvaluator);

		this->iVersion++;
		unsigned      numSwaps = 0;
		for (unsigned tid      = 0; tid < tidHi[pSignature->numPlaceholder]; tid++) {
			// point to evaluator for given transformId
			footprint_t *v = pStore->fwdEvaluator + tid * tinyTree_t::TINYTREE_NEND;

			// evaluate
			tree.eval(v);

			// test if result is unchanged
			if (pStore->revEvaluator[tree.root].equals(v[tree.root])) {
				// remember tid
				assert(numSwaps < MAXTRANSFORM);
				this->swapsFound[numSwaps++] = tid;
				// mark it as in use
				this->swapsActive[tid]       = iVersion;
			}
		}

		// test if swaps are present
		if (numSwaps <= pStore->IDFIRST)
			return 0;

		/*
		 * Result
		 */
		
		swap_t swap;
		unsigned numEntry = 0;
		::memset(&swap, 0, sizeof(swap));

		assert(numSwaps > 0 && this->swapsFound[0] == 0);

		/*
		 * Scan through all transforms.
		 * For those that are capable of condensing the collection,
		 * Disable active entries that do not break the collection
		 */
		for (unsigned iSwap = pStore->IDFIRST; iSwap < numSwaps; iSwap++) {
			uint32_t tidPrime = this->swapsFound[iSwap];

			// ignore disabled entries
			if (tidPrime & IBIT)
				continue;

			// does the entry have the capability to condense the collection
			unsigned activeLeft = this->countNextActive(tidPrime, numSwaps, this->swapsFound);
			if (activeLeft == 0)
				continue; // the resulting collection is invalid

			// disable orphaned entried if possible	
			for (unsigned iOrphan = pStore->IDFIRST; iOrphan < numSwaps; iOrphan++) {
				unsigned tidOrphan = this->swapsFound[iOrphan];

				if (tidOrphan & IBIT)
					continue; // already orphaned
				if (this->swapsActive[tidOrphan] == this->iVersion)
					continue; // not orphaned
				if (tidOrphan == tidPrime)
					continue; // do not disable self

				/*
				 * verify that the runtime `compare()` can reach all permutations  
				 * `allowDeactivate()` is horribly expensive, so delay as long as possible
				 */
				if (allowDeactivate(sid, numSwaps, this->swapsFound, /*excludeTid=*/ tidOrphan))
					this->swapsFound[iOrphan] |= IBIT;
			}
		}

		/*
		 * Add active tids to result
		 */
		for (unsigned iSwap = pStore->IDFIRST; iSwap < numSwaps; iSwap++) {
			if (!(this->swapsFound[iSwap] & IBIT)) {
				assert(numEntry < swap_t::MAXENTRY);
				swap.tids[numEntry++] = this->swapsFound[iSwap];
			}
		}

		if (opt_text == OPTTEXT_WON) {
			printf("%s\n", pSignature->name);
		}


		// add to database
		if (!this->readOnlyMode) {
			// lookup/add swapId
			unsigned ix     = pStore->lookupSwap(&swap);
			unsigned swapId = pStore->swapIndex[ix];
			if (swapId == 0)
				pStore->swapIndex[ix] = swapId = pStore->addSwap(&swap);
			else
				skipDuplicate++;

			// add swapId to signature
			return swapId;
		}

		return 0;
	}

	/**
	 * @date 2020-05-02 23:07:05
	 *
	 * Read and add endpoint swaps from file
	 */
	void /*__attribute__((optimize("O0")))*/ swapsFromFile(void) {

		/*
		 * Load swaps from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading swaps from file\n", ctx.timeAsString());

		FILE *f = ::fopen(this->opt_load, "r");
		if (f == NULL)
			ctx.fatal("\n{\"error\":\"fopen() failed\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"reason\":\"%m\"}\n",
				  __FUNCTION__, __FILE__, __LINE__, this->opt_load);

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;

		char name[64];

		for (;;) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) numSwap=%u(%.0f%%) | skipDuplicate=%u",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numSwap, pStore->numSwap * 100.0 / pStore->maxSwap,
					skipDuplicate);

				ctx.tick = 0;
			}

			static char line[512];
			char        *pLine = line;
			char        *endptr;
			swap_t      swap;

			::memset(&swap, 0, sizeof(swap));

			/*
			 * populate record
			 */

			if (::fgets(line, sizeof(line), f) == 0)
				break; // end-of-input

			/*
			 * load name
			 */

			// extract name
			char *pName = name;

			while (*pLine && !::isspace(*pLine))
				*pName++ = *pLine++;
			*pName           = 0; // terminator

			if (!name[0])
				ctx.fatal("\n{\"error\":\"bad or empty line\",\"where\":\"%s:%s:%d\",\"line\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress);

			/*
			 * load entries
			 */

			unsigned numEntry = 0;
			for (;;) {
				// get next numeric value
				unsigned tid = ::strtoul(pLine, &endptr, 0);

				/*
				 * Test for end-of-line
				 */
				if (pLine == endptr) {
					// let endptr swallow spaces
					while (::isspace(*endptr))
						endptr++;
					// is it end-of-line
					if (!*endptr)
						break;
					// rewind to error position
					endptr = pLine;
				}

				if (pLine == endptr || numEntry >= swap_t::MAXENTRY || tid >= MAXTRANSFORM)
					ctx.fatal("\n{\"error\":\"bad or too many columns\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"line\":%lu}\n",
						  __FUNCTION__, __FILE__, __LINE__, name, ctx.progress);

				swap.tids[numEntry++] = tid;
				pLine = endptr;

				if (!*pLine)
					break; // done
			}

			/*
			 * add to database
			 */

			// lookup signature
			unsigned ix  = pStore->lookupSignature(name);
			unsigned sid = pStore->signatureIndex[ix];
			if (sid == 0)
				ctx.fatal("\n{\"error\":\"missing signature\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"line\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, name, ctx.progress);

			if (!this->readOnlyMode) {
				// lookup/add swapId
				ix = pStore->lookupSwap(&swap);
				unsigned swapId = pStore->swapIndex[ix];
				if (swapId == 0)
					pStore->swapIndex[ix] = swapId = pStore->addSwap(&swap);
				else
					skipDuplicate++;

				// add swapId to signature
				if (pStore->signatures[sid].swapId == 0) {
					pStore->signatures[sid].swapId = swapId;
				} else if (pStore->signatures[sid].swapId != swapId)
					ctx.fatal("\n{\"error\":\"inconsistent swap\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"line\":%lu}\n",
						  __FUNCTION__, __FILE__, __LINE__, name, ctx.progress);
			}

			if (opt_text == OPTTEXT_WON) {
				printf("%s", pStore->signatures[sid].name);

				for (unsigned j = 0; j < swap_t::MAXENTRY && swap.tids[j]; j++)
					printf("\t%u", swap.tids[j]);

				printf("\n");
			}

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "[%s] Read swaps. numSwap=%u(%.0f%%) | skipDuplicate=%u\n",
				ctx.timeAsString(),
				pStore->numSwap, pStore->numSwap * 100.0 / pStore->maxSwap,
				skipDuplicate);
	}

	/**
	 * @date 2020-05-02 23:09:29
	 *
	 */
	void swapsFromSignatures(void) {

		tinyTree_t tree(ctx);

		/*
		 * Apply sid/task setting on generator
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
			if (this->opt_taskId || this->opt_taskLast) {
				if (this->opt_sidHi)
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%u-%u\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_sidLo, this->opt_sidHi);
				else
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%u-last\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_sidLo);
			} else if (this->opt_sidLo || this->opt_sidHi) {
				if (this->opt_sidHi)
					fprintf(stderr, "[%s] INFO: sid=%u-%u\n", ctx.timeAsString(), this->opt_sidLo, this->opt_sidHi);
				else
					fprintf(stderr, "[%s] INFO: sid=%u-last\n", ctx.timeAsString(), this->opt_sidLo);
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating swaps.\n", ctx.timeAsString());

		// reset ticker
		ctx.setupSpeed(this->opt_sidHi ? this->opt_sidHi : pStore->numSignature);
		ctx.tick = 0;

		// create imprints for signature groups
		ctx.progress++; // skip reserved entry;
		for (unsigned iSid = pStore->IDFIRST; iSid < pStore->numSignature; iSid++) {

			if ((opt_sidLo && iSid < opt_sidLo) || (opt_sidHi && iSid >= opt_sidHi)) {
				ctx.progress++;
				continue;
			}

			signature_t *pSignature = pStore->signatures + iSid;
			if (!pSignature->swapId) {
				uint32_t swapId = foundSignatureSwap(pStore->signatures[iSid].name);
				pSignature->swapId = swapId;
			}

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSwap=%u(%.0f%%) | skipDuplicate=%u\n",
				ctx.timeAsString(),
				pStore->numSwap, pStore->numSwap * 100.0 / pStore->maxSwap,
				skipDuplicate);
	}

};
