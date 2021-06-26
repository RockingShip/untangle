//#pragma GCC optimize ("O0") // optimize on demand

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
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include "database.h"
#include "dbtool.h"
#include "metrics.h"
#include "tinytree.h"

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
	/// @var {number} save level-1 indices (hintIndex, signatureIndex, ImprintIndex) and level-2 index (imprints)
	unsigned   opt_saveIndex;
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
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for reverse transforms
	footprint_t *pEvalRev;

	/// @var {number} current version incarnation
	uint32_t iVersion;
	/// @var {number} duplicate swaps in database
	unsigned skipDuplicate;
	/// @var {number[]} - Versioned memory of active swaps/transforms
	uint32_t *swapsActive;
	/// @var {number[]} - List of found swaps/transforms for signature under investigation. IBET set indicates they are disabled
	uint32_t *swapsFound;
	/// @var {number[]} - Versioned memory of active swaps/transforms
	uint64_t *swapsWeight;
	/// @var {number[]} - Weights to assist choosing in case of draws
	unsigned tidHi[MAXSLOTS + 1];

	/**
	 * Constructor
	 */
	genswapContext_t(context_t &ctx) : dbtool_t(ctx) {
		// arguments and options
		opt_force          = 0;
		opt_generate       = 1;
		arg_inputDatabase  = NULL;
		opt_load           = NULL;
		arg_outputDatabase = NULL;
		opt_saveIndex      = 1;
		opt_sidHi          = 0;
		opt_sidLo          = 0;
		opt_taskId         = 0;
		opt_taskLast       = 0;
		opt_text           = 0;

		iVersion = 0;
		pStore   = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;

		skipDuplicate = 0;
		swapsActive   = (uint32_t *) ctx.myAlloc("genswapContext_t::swapsActive", MAXTRANSFORM, sizeof(*swapsActive));
		swapsFound    = (uint32_t *) ctx.myAlloc("genswapContext_t::swapsFound", MAXTRANSFORM, sizeof(*swapsFound));
		swapsWeight   = (uint64_t *) ctx.myAlloc("genswapContext_t::swapsWeight", MAXTRANSFORM, sizeof(*swapsWeight));
		for (unsigned j = 0; j <= MAXSLOTS; j++)
			tidHi[j] = 0;
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


	/**
	 * @date 2020-05-04 23:54:12
	 *
	 * Given a list of Transforms, some deactivated by having their IBIT set.
	 * Apply transform to collection to find pairs.
	 * Flag better half of pair to pass to next round.
	 *
	 * @param {signature_t} pSignature - signature for `--text` mode
	 * @param {number} tidFocus - which transform to use
	 * @param {number} numFound - total number of transforms
	 * @param {number[]} pFound - list of transforms
	 * @return {number} - number of transforms still active
	 */
	unsigned countNextActive(const signature_t *pSignature, unsigned tidFocus, unsigned numFound, uint32_t *pFound) {
		// result
		unsigned numActiveNext = 0;

		// bump version number
		this->iVersion++;

		// get name of selected transform
		const char *pFocus = pStore->fwdTransformNames[tidFocus];

		for (unsigned j = 0; j < numFound; j++) {
			// get transform
			unsigned   tidOrig = pFound[j];
			const char *pOrig  = pStore->fwdTransformNames[tidOrig & ~IBIT];

			// apply transform to slots
			unsigned   tidSwapped = pStore->lookupTransformSlot(pOrig, pFocus, pStore->fwdTransformNameIndex);
			const char *pSwapped  = pStore->fwdTransformNames[tidSwapped];

			// skip if disabled
			if (tidOrig & IBIT)
				continue;

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

			if (opt_text == OPTTEXT_COMPARE && pSignature) {
				// test if `pSwap` becomes active again
				if (cmp < 0)
					cmp = '<';
				else if (cmp > 0)
					cmp = '>';
				else
					assert(0);

				printf("%u\t%s\t%s\t%c\t%.*s\t%.*s\n",
				       (unsigned) (pSignature - pStore->signatures),
				       pSignature->name,
				       pFocus,
				       cmp,
				       pSignature->numPlaceholder, pOrig,
				       pSignature->numPlaceholder, pSwapped);
			}

		}

		return numActiveNext;
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
	unsigned foundSignatureSwap(const char *pName) {

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
		tree.eval(this->pEvalRev);

		this->iVersion++;
		unsigned      numSwaps = 0;
		for (unsigned tid      = 0; tid < tidHi[pSignature->numPlaceholder]; tid++) {
			// point to evaluator for given transformId
			footprint_t *v = this->pEvalFwd + tid * tinyTree_t::TINYTREE_NEND;

			// evaluate
			tree.eval(v);

			// test if result is unchanged
			if (this->pEvalRev[tree.root].equals(v[tree.root])) {
				// remember tid
				assert(numSwaps < MAXTRANSFORM);
				this->swapsFound[numSwaps++] = tid;
				// mark it as in use
				this->swapsActive[tid]       = iVersion;
			}
		}

		// test if swaps are present
		if (numSwaps <= 1)
			return 0;

		/*
		 * Record to populate
		 */
		swap_t swap;
		::memset(&swap, 0, sizeof(swap));
		unsigned numEntry = 0;

		/*
		 * Validate if condensing is possible:
		 *
		 * For each transform in collection:
		 * - apply selected transform to every other collection item including self.
		 * - all transformed should be present in collection.
		 */

		for (unsigned iSelect = 0; iSelect < numSwaps; iSelect++) {
			unsigned tidSelect = this->swapsFound[iSelect];

			// selected may not be transparent (tid=0) because it has no effect
			if (tidSelect == 0)
				continue;

			const char *pSelect = pStore->fwdTransformNames[tidSelect];
			bool       okay     = true;

			/*
			 * apply selected transform to collection and locate pair
			 */
			for (unsigned j = 0; j < numSwaps; j++) {
				unsigned   tidOrig    = this->swapsFound[j] & ~IBIT;
				const char *pOrig     = pStore->fwdTransformNames[tidOrig];
				unsigned   tidSwapped = pStore->lookupTransformSlot(pOrig, pSelect, pStore->fwdTransformNameIndex);

				// test if other half pair present
				if (this->swapsActive[tidSwapped] != iVersion)
					okay = false;
			}

			if (!okay) {
				/*
					 * @date 2020-05-04 11:30:06
					 * Turns out this never happens so coding assumes this to be true.
				 */
				assert(0);
			}
		}

		/*
		 * Find a transform that maximise the disabling of other permutations
		 */

		for (unsigned iRound = 0;; iRound++) {

			/*
				 * NOTE: consider all swaps including those disabled because this allows the possibility that some transforms can be applied multiple times
			 */
			unsigned      bestTid   = 0;
			unsigned      bestCount = 0;
			for (unsigned iFocus    = 0; iFocus < numSwaps; iFocus++) {
				unsigned tidFocus = this->swapsFound[iFocus] & ~IBIT;
				if (tidFocus == 0)
					continue; // skip transparent or disabled transform

				// calculate number active left after applying selected transform
				unsigned activeLeft = this->countNextActive(pSignature, tidFocus, numSwaps, this->swapsFound);

				// remember which is best
				if (bestTid == 0 || activeLeft < bestCount || (activeLeft == bestCount && swapsWeight[tidFocus] < swapsWeight[bestTid])) {
					bestTid   = tidFocus;
					bestCount = activeLeft;
				}
			}
			assert(bestCount);

			// apply best transform
			this->countNextActive(pSignature, bestTid, numSwaps, this->swapsFound);

			// apply result
			for (unsigned iFocus = 0; iFocus < numSwaps; iFocus++) {
				unsigned tidFocus = this->swapsFound[iFocus];

				if (this->swapsActive[tidFocus & ~IBIT] == this->iVersion) {
					// focus remains active
					this->swapsFound[iFocus] &= ~IBIT;
				} else {
					// focus becomes inactive
					this->swapsFound[iFocus] |= IBIT;
				}
			}

			// add to record
			assert(numEntry < swap_t::MAXENTRY);
			swap.tids[numEntry++] = bestTid;

			if (bestCount == 1)
				break;
		}

		if (opt_text == OPTTEXT_WON) {
			printf("%s\t", pSignature->name);

			for (unsigned j = 0; j < swap_t::MAXENTRY && swap.tids[j]; j++)
				printf("\t%u", swap.tids[j]);

			printf("\n");
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
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {

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
 * @global {genswapContext_t} Application context
 */
genswapContext_t app(ctx);

/**
 * @date 2020-05-02 23:11:46
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int __attribute__ ((unused)) sig) {
	if (app.arg_outputDatabase) {
		remove(app.arg_outputDatabase);
	}
	exit(1);
}

/**
 * 2020-05-02 23:11:54
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int __attribute__ ((unused)) sig) {
	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

/**
 * @date 2020-05-02 23:12:01
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <input.db> [<output.db>]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                    Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate            Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                     This list\n");
		fprintf(stderr, "\t   --maxswap=<number>         Maximum number of swaps [default=%u]\n", app.opt_maxSwap);
		fprintf(stderr, "\t   --[no-]paranoid            Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                    Say less\n");
		fprintf(stderr, "\t   --[no-]saveindex           Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --sid=[<low>],<high>       Sid range upper bound [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --swapindexsize=<number>   Size of swap index [default=%u]\n", app.opt_swapIndexSize);
		fprintf(stderr, "\t   --task=sge                 Get sid task settings from SGE environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>         Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                     Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>          Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose                  Say more\n");
	}
}

/**
 * @date 2020-05-02 23:15:58
 *
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context.
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 */
int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_DEBUG   = 1,
			LO_FORCE,
			LO_GENERATE,
			LO_LOAD,
			LO_MAXSWAP,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_NOUNSAFE,
			LO_PARANOID,
			LO_PURE,
			LO_SAVEINDEX,
			LO_SID,
			LO_SWAPINDEXSIZE,
			LO_TASK,
			LO_TEXT,
			LO_TIMER,
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",         1, 0, LO_DEBUG},
			{"force",         0, 0, LO_FORCE},
			{"generate",      0, 0, LO_GENERATE},
			{"help",          0, 0, LO_HELP},
			{"load",          1, 0, LO_LOAD},
			{"maxswap",       1, 0, LO_MAXSWAP},
			{"paranoid",      0, 0, LO_PARANOID},
			{"pure",          0, 0, LO_PURE},
			{"no-generate",   0, 0, LO_NOGENERATE},
			{"no-paranoid",   0, 0, LO_NOPARANOID},
			{"no-pure",       0, 0, LO_NOPURE},
			{"no-saveindex",  0, 0, LO_NOSAVEINDEX},
			{"no-unsafe",     0, 0, LO_NOUNSAFE},
			{"quiet",         2, 0, LO_QUIET},
			{"saveindex",     0, 0, LO_SAVEINDEX},
			{"sid",           1, 0, LO_SID},
			{"swapindexsize", 1, 0, LO_SWAPINDEXSIZE},
			{"task",          1, 0, LO_TASK},
			{"text",          2, 0, LO_TEXT},
			{"timer",         1, 0, LO_TIMER},
			{"verbose",       2, 0, LO_VERBOSE},
			//
			{NULL,            0, 0, 0}
		};

		char optstring[64];
		char *cp          = optstring;
		int  option_index = 0;

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
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_DEBUG:
			ctx.opt_debug = ::strtoul(optarg, NULL, 0);
			break;
		case LO_FORCE:
			app.opt_force++;
			break;
		case LO_GENERATE:
			app.opt_generate++;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_LOAD:
			app.opt_load = optarg;
			break;
		case LO_MAXSWAP:
			app.opt_maxSwap = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_NOGENERATE:
			app.opt_generate = 0;
			break;
		case LO_NOPARANOID:
			ctx.flags &= ~context_t::MAGICMASK_PARANOID;
			break;
		case LO_NOPURE:
			ctx.flags &= ~context_t::MAGICMASK_PURE;
			break;
		case LO_NOSAVEINDEX:
			app.opt_saveIndex = 0;
			break;
		case LO_NOUNSAFE:
			ctx.flags &= ~context_t::MAGICMASK_UNSAFE;
			break;
		case LO_PARANOID:
			ctx.flags |= context_t::MAGICMASK_PARANOID;
			break;
		case LO_PURE:
			ctx.flags |= context_t::MAGICMASK_PURE;
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
			break;
		case LO_SAVEINDEX:
			app.opt_saveIndex     = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_saveIndex + 1;
			break;
		case LO_SID: {
			unsigned m, n;

			int ret = sscanf(optarg, "%u,%u", &m, &n);
			if (ret == 2) {
				app.opt_sidLo = m;
				app.opt_sidHi = n;
			} else if (ret == 1) {
				app.opt_sidHi = m;
			} else {
				usage(argv, true);
				exit(1);
			}
			if (app.opt_sidHi && app.opt_sidLo >= app.opt_sidHi) {
				fprintf(stderr, "--sid low exceeds high\n");
				exit(1);
			}
			break;
		}
		case LO_SWAPINDEXSIZE:
			app.opt_swapIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_TASK: {
			if (::strcmp(optarg, "sge") == 0) {
				const char *p;

				p = getenv("SGE_TASK_ID");
				app.opt_taskId = p ? atoi(p) : 0;
				if (app.opt_taskId < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_ID\n");
					exit(0);
				}

				p = getenv("SGE_TASK_LAST");
				app.opt_taskLast = p ? atoi(p) : 0;
				if (app.opt_taskLast < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_LAST\n");
					exit(0);
				}

				if (app.opt_taskId < 1 || app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "sge id/last out of bounds: %u,%u\n", app.opt_taskId, app.opt_taskLast);
					exit(1);
				}

				// set ticker interval to 60 seconds
				ctx.opt_timer = 60;
			} else {
				if (sscanf(optarg, "%u,%u", &app.opt_taskId, &app.opt_taskLast) != 2) {
					usage(argv, true);
					exit(1);
				}
				if (app.opt_taskId == 0 || app.opt_taskLast == 0) {
					fprintf(stderr, "Task id/last must be non-zero\n");
					exit(1);
				}
				if (app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "Task id exceeds last\n");
					exit(1);
				}
			}

			break;
		}
		case LO_TEXT:
			app.opt_text = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_text + 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = ::strtoul(optarg, NULL, 0);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
			break;

		case '?':
			fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
			exit(1);
		default:
			fprintf(stderr, "getopt_long() returned character code %d\n", c);
			exit(1);
		}
	}

	/*
	 * Program arguments
	 */
	if (argc - optind >= 1)
		app.arg_inputDatabase = argv[optind++];

	if (argc - optind >= 1)
		app.arg_outputDatabase = argv[optind++];

	if (app.arg_inputDatabase == NULL) {
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

	if (app.opt_load) {
		struct stat sbuf;

		if (stat(app.opt_load, &sbuf)) {
			fprintf(stderr, "%s does not exist\n", app.opt_load);
			exit(1);
		}
	}

	if (app.opt_text && isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Open input database
	 */

	// Open input
	database_t db(ctx);

	// test for readOnly mode
	app.readOnlyMode = (app.arg_outputDatabase == NULL);

	db.open(app.arg_inputDatabase, !app.readOnlyMode);

	// display system flags when database was created
	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		char dbText[128], ctxText[128];

		ctx.flagsToText(db.creationFlags, dbText);
		ctx.flagsToText(ctx.flags, ctxText);

		if (db.creationFlags != ctx.flags)
			fprintf(stderr, "[%s] WARNING: Database/system flags differ: database=[%s] current=[%s]\n", ctx.timeAsString(), dbText, ctxText);
		else if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), dbText);
	}

	/*
	 * apply settings for `--task`
	 */
	if (app.opt_taskId || app.opt_taskLast) {
		// split progress into chunks
		uint64_t taskSize = db.numSignature / app.opt_taskLast;
		if (taskSize == 0)
			taskSize = 1;

		app.opt_sidLo = taskSize * (app.opt_taskId - 1);
		app.opt_sidHi = taskSize * app.opt_taskId;

		if (app.opt_taskId == app.opt_taskLast)
			app.opt_sidHi = 0;
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));

	/*
	 * Create output database
	 */

	database_t store(ctx);

	app.inheritSections &= ~(database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_SWAP | database_t::ALLOCMASK_SWAPINDEX);
	// will require local copy of signatures
	app.rebuildSections |= database_t::ALLOCMASK_SIGNATURE;

	// sync signatures to input
	app.opt_maxSignature = db.numSignature;

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, 0); // numNodes is only needed for defaults that should not occur

	/*
	 * Finalise allocations and create database
	 */

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		// Assuming with database allocations included
		size_t allocated = ctx.totalAllocated + store.estimateMemoryUsage(app.inheritSections);

		struct sysinfo info;
		if (sysinfo(&info) == 0) {
			double percent = 100.0 * allocated / info.freeram;
			if (percent > 80)
				fprintf(stderr, "WARNING: using %.1f%% of free memory minus cache\n", percent);
		}
	}

	// actual create
	store.create(app.inheritSections);
	app.pStore = &store;

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS && (~app.rebuildSections & ~app.inheritSections)) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Allocated %.3fG memory. freeMemory=%.3fG.\n", ctx.timeAsString(), ctx.totalAllocated / 1e9, info.freeram / 1e9);
	}

	// initialize evaluator early using input database
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, db.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, db.revTransformData);

	/*
	 * Inherit/copy sections
	 */

	app.populateDatabaseSections(store, db);

	/*
	 * Rebuild sections
	 */

	// todo: move this to `populateDatabaseSections()`
	// data sections cannot be automatically rebuilt
	assert((app.rebuildSections & (database_t::ALLOCMASK_SWAP | database_t::ALLOCMASK_HINT | database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_MEMBER)) == 0);

	if (app.rebuildSections & database_t::ALLOCMASK_SIGNATURE) {
		store.numSignature = db.numSignature;
		::memcpy(store.signatures, db.signatures, store.numSignature * sizeof(*store.signatures));
	}
	if (app.rebuildSections)
		store.rebuildIndices(app.rebuildSections);

	/*
	 * get upper limit for tid's for given number of placeholders
	 */
	assert(MAXSLOTS == 9);
	app.tidHi[0] = 0;
	app.tidHi[1] = store.lookupTransform("a", store.fwdTransformNameIndex) + 1;
	app.tidHi[2] = store.lookupTransform("ba", store.fwdTransformNameIndex) + 1;
	app.tidHi[3] = store.lookupTransform("cba", store.fwdTransformNameIndex) + 1;
	app.tidHi[4] = store.lookupTransform("dcba", store.fwdTransformNameIndex) + 1;
	app.tidHi[5] = store.lookupTransform("edcba", store.fwdTransformNameIndex) + 1;
	app.tidHi[6] = store.lookupTransform("fedcba", store.fwdTransformNameIndex) + 1;
	app.tidHi[7] = store.lookupTransform("gfedcba", store.fwdTransformNameIndex) + 1;
	app.tidHi[8] = store.lookupTransform("hgfedcba", store.fwdTransformNameIndex) + 1;
	app.tidHi[9] = MAXTRANSFORM;
	assert(app.tidHi[2] == 2 && app.tidHi[3] == 6 && app.tidHi[4] == 24 && app.tidHi[5] == 120 && app.tidHi[6] == 720 && app.tidHi[7] == 5040 && app.tidHi[8] == 40320);

	/*
	 * Determine weights of transforms. More shorter cyclic loops the better
	 */

	for (unsigned iTid = 0; iTid < store.numTransform; iTid++) {
		app.swapsWeight[iTid] = 0;

		// count cycles
		unsigned      found  = 0;
		const char    *pName = store.fwdTransformNames[iTid];
		for (unsigned j      = 0; j < MAXSLOTS; j++) {
			if (~found & (1 << (pName[j] - 'a'))) {
				// new starting point
				unsigned      w = 1;
				// walk cycle
				for (unsigned k = pName[j] - 'a'; ~found & (1 << k); k = pName[k] - 'a') {
					w *= (MAXSLOTS + 1); // weight is power of length
					found |= 1 << k;
				}
				// update wight
				app.swapsWeight[iTid] += w;
			}
		}
	}

	/*
	 * Where to look for new candidates
	 */

	// if input is empty, skip reserved entries
	if (!app.readOnlyMode) {
	}

	if (app.opt_load)
		app.swapsFromFile();
	if (app.opt_generate) {
		/*
		 * Create worker database to count imprints.
		 * Use separate db as to not to interfere with real imprints
		 */
		app.swapsFromSignatures();
	}

	/*
	 * List result
	 */

	if (app.opt_text == app.OPTTEXT_BRIEF) {
		// Many swaps are empty, only output those found

		for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
			const signature_t *pSignature = store.signatures + iSid;
			const swap_t      *pSwap      = store.swaps + pSignature->swapId;

			if (pSwap->tids[0]) {
				printf("%s\t", pSignature->name);

				for (unsigned j = 0; j < swap_t::MAXENTRY && pSwap->tids[j]; j++)
					printf("\t%u", pSwap->tids[j]);

				printf("\n");
			}
		}
	}

	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		if (!app.opt_saveIndex) {
			store.signatureIndexSize = 0;
			store.hintIndexSize      = 0;
			store.imprintIndexSize   = 0;
			store.numImprint         = 0;
			store.interleave         = 0;
			store.interleaveStep     = 0;
		}

		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}


	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		json_t *jResult = json_object();
		json_object_set_new_nocheck(jResult, "done", json_string_nocheck(argv[0]));
		if (app.opt_taskLast) {
			json_object_set_new_nocheck(jResult, "taskId", json_integer(app.opt_taskId));
			json_object_set_new_nocheck(jResult, "taskLast", json_integer(app.opt_taskLast));
		}
		if (app.opt_sidLo || app.opt_sidHi) {
			json_object_set_new_nocheck(jResult, "sidLo", json_integer(app.opt_sidLo));
			json_object_set_new_nocheck(jResult, "sidHi", json_integer(app.opt_sidHi));
		}
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.jsonInfo(jResult);
		fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}

	return 0;
}
