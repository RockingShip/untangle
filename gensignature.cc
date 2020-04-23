//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-14 11:09:15
 *
 * `gensignature` scans `*n9` space using `generator_t` and adds associative unique footprints to a given dataset.
 * Associative unique is when all other permutation of endpoints are excluded.
 *
 * `gensignature` can also generate SQL used for signature group analysis.
 *
 * Each footprint can consist of a collection of unique structures called signature group.
 * One member of each signature group, the structure with the most concise notation, is called the representative.
 * The name of the representative is the display name of the signature.
 *
 * For each signature group additional properties are determined.
 * - Scoring to filter which structures should be part of the group.
 * - Scoring to select the representative.
 * - Endpoint swapping for associative properties.
 *
 * `gensignature` self-test demonstrates:
 *   - Decoding, encoding and evaluation of `tinyTree_t`
 *   - Database section
 *   - Interleaving
 *   - Associative index
 *
 * Basically, `gensignature` finds uniqueness in a given dataset.
 *
 * - It creates all possible 512403385356 expressions consisting of 5 or less unified operators and 9 or less variables
 * - Of every expression it permutates all possible 9!=363600 inputs
 * - Of every permutation it tries all 2^8=512 different input values
 * - In that vastness of information matches are searched
 *
 * All those 512403385356 expressions can be rewritten in terms of 791647 unique expressions fitted with 363600 different skins.
 *
 * @date 2020-03-23 03:39:32
 *
 *   `--text` displays resulting signature collection
 *   `--text=2` displays all candidates
 *
 * @date 2020-03-29 13:47:04
 *
 * Mystery of the missing candidates.
 * The candidates are now ordered using `compare()`.
 * This has the side effect that components might be (forced) unordered which fails validation and gets rejected.
 * For example: the top-level `F` component of `"ab>ab+21&?"` which is `"ab>ab+&"` and unordered.
 * see: `"./eval 'ab>ab+21&?' --F"` and `"./eval 'ab>ab+21&?' --F --fast"`
 *
 * @date 2020-04-21 19:22:40
 *
 * `gensignature` collects signatures and therefore does not support `--unsafe`.
 *
 * @date 2020-04-22 21:20:56
 *
 * `gensignature` selects candidates not present in the imprint index.
 * Selected candidates are added to `signatures`.
 *
 * @date 2020-04-22 21:37:03
 *
 * Text modes:
 *
 * `--text[=1]` Concise mode that show selected candidates passed to `foundTreeSignature()`.
 *              Selected candidates are those that challenge and win the current display name.
 *              Also intended for transport and merging when broken into multiple tasks.
 *              Can be used for the `--load=<file>` option.
 *              Output can be converted to other text modes by `"./gensignature <input.db> <numNode> [<output.db>]--load=<inputList> --no-generate --text=<newMode> '>' <outputList>
 *
 *              <name> <numPlaceholder> <numEndpoint> <numBackRef>
 *
 * `--test=2`   Full mode of all candidates passed to `foundTreeSignature()` including what needed to compare against the display name.
 *
 *              <cid> <sid> <cmp> <name> <size> <numPlaceholder> <numEndpoint> <numBackRef>
 *
 *              where:
 *                  <cid> is the candidate id assigned by the generator.
 *                  <sid> is the signature id assigned by the associative lookup.
 *                  <cmp> is the result of `comparSignature()` between the candidate and the current display name.
 *
 *              <cmp> can be:
 *                  cmp = '*'; // not compared
 *                  cmp = '-'; // worse by numbers
 *                  cmp = '<'; // worse by name compare
 *                  cmp = '='; // equals
 *                  cmp = '>'; // better by compare
 *                  cmp = '+'; // better by numbers
 *
 * `--text=3`   Selected and sorted signatures that are written to the output database.
 *              NOTE: same format as `--text=1`
 *
 *              <name> <numPlaceholder> <numEndpoint> <numBackRef>
 *
 * `--text=4`   Selected and sorted signatures that are written to the output database
 *
 *              <sid> <name> <size> <numPlaceholder> <numEndpoint> <numBackRef>
 *
 * @date 2020-04-23 10:50:58
 *
 *   Not specifying an output database puts `gensignature` in read-only mode.
 *   This reduces memory usage for `4n9` from 20G to 5G.
 *   Read-only mode is slower (for `4n9` 2x) because it disables updating the imprint index making it operates worst-case.
 *
 *   Low memory profile is intended for workers when dividing database creation in parallel tasks.
 *     - Determine how may workers per machine. Using `"/proc/meminfo"` "(<MemFree> + <Cached> - <sizeofDatabase>) / 5G".
 *     - Higher database interleave gives higher speed at the cost of more storage.
 *     - Change interleave with `./gensignature <input.db> <numNode> <output.db> --no-generate --interleave=<newInterleave>"
 *     - Run:
 *           `"./gensignature <input.db> <numNode> --task=n,m --text '>' <outputList>`"`
 *       or
 *           `"mkdir logs-task"`
 *           `"qsub -cwd -o logs-task -e logs -b y -t 1-<maxTask> -q <queue> ./gensignature <input.db> <numNode> --task=sge --text"`
 *       when finished and checked all jobs have complete with no error (they display "done" in their stderr logs)
 *           `"cat logs-task/? <joinedList>"`
 *           `"./gensignature <input.db> <numNode> <output.db> --load=<joinedList> --no-generate"`
 *
 *  ./gensignature is tuned with metrics upto/including `4n9`, see `"metricsImprint[]"`.
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
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include "tinytree.h"
#include "database.h"
#include "generator.h"
#include "restartdata.h"
#include "metrics.h"

#include "config.h"

#if defined(ENABLE_JANSSON)
#include "jansson.h"
#endif

/**
 * @date 2020-03-14 11:10:15
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct gensignatureContext_t : callable_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} Tree size in nodes to be generated for this invocation;
	unsigned arg_numNodes;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned opt_generate;
	/// @var {number} size of imprint index WARNING: must be prime
	unsigned opt_imprintIndexSize;
	/// @var {number} interleave for associative imprint index
	unsigned opt_interleave;
	/// @var {string} name of file containing members
	const char *opt_load;
	/// @var {number} Maximum number of imprints to be stored database
	unsigned opt_maxImprint;
	/// @var {number} Maximum number of signatures to be stored database
	unsigned opt_maxSignature;
	/// @var {number} index/data ratio
	double opt_ratio;
	/// @var {number} size of signature index WARNING: must be prime
	unsigned opt_signatureIndexSize;
	/// @var {number} task Id. First task=1
	unsigned opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {number} generator upper bound
	uint64_t opt_windowHi;
	/// @var {number} generator lower bound
	uint64_t opt_windowLo;

	/// @var {database_t} - Database store to place results
	database_t *pStore;
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;

	/// @var {number} - THE generator
	generatorTree_t generator;

	/**
	 * Constructor
	 */
	gensignatureContext_t(context_t &ctx) : ctx(ctx), generator(ctx) {
		// arguments and options
		arg_inputDatabase = NULL;
		arg_numNodes = 0;
		arg_outputDatabase = NULL;
		opt_force = 0;
		opt_generate = 1;
		opt_imprintIndexSize = 0;
		opt_interleave = 0;
		opt_taskId = 0;
		opt_taskLast = 0;
		opt_load = NULL;
		opt_maxImprint = 0;
		opt_maxSignature = 0;
		opt_ratio = METRICS_DEFAULT_RATIO / 10.0;
		opt_signatureIndexSize = 0;
		opt_text = 0;
		opt_windowHi = 0;
		opt_windowLo = 0;

		pStore = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;
	}

	/**
	 * @date 2020-03-22 00:57:15
	 *
	 * found candidate.
	 *
	 * @date 2020-03-23 04:46:53
	 *
	 * Perform an associative lookup to determine signature footprint (sid) and orientation (tid)
	 * expand collection of unique structures.
	 *
	 * If signature has swapping hints then apply the hint and deep-compare.
	 * accept the
	 *
	 * @date 2020-03-27 02:47:42
	 *
	 * At this point in time the generator seems to be functional.
	 * It agrees with metrics from the previous major version.
	 * Unexpected surprise is the two mode associative index.
	 * The previous version had (slower) interleave 504.
	 * All the trees passed to this function are natural ordered trees.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeSignature(const generatorTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | hash=%.3f %s",
				        ctx.timeAsString(), ctx.progress,
				        perSecond, pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				        (double) ctx.cntCompare / ctx.cntHash, pNameR);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				/*
				 * @date 2020-04-23 17:26:04
				 *
				 *   ctx.progress is candidateId
				 *   ctx.progressHi is ticker upper limit
				 *   treeR.windowLo/treeR.windowHi is ctx.progress limits. windowHi can be zero
				 */
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | hash=%.3f %s",
				        ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - treeR.windowLo) * 100.0 / (ctx.progressHi - treeR.windowLo), etaH, etaM, etaS,
				        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				        (double) ctx.cntCompare / ctx.cntHash, pNameR);
			}

			ctx.tick = 0;
		}

		/*
		 * Lookup/add to data store.
		 * Consider signature groups `unsafe` (no members yet)
		 */

		unsigned sid = 0;
		unsigned tid = 0;

		pStore->lookupImprintAssociative(&treeR, pEvalFwd, pEvalRev, &sid, &tid);

		// add to datastore if not found
		if (sid == 0) {
			if (opt_text == 1)
				printf("%s\t%u\t%u\t%u\n", pNameR, numPlaceholder, numEndpoint, numBackRef);

			// only add if signatures are writable
			if (~pStore->allocFlags & database_t::ALLOCMASK_SIGNATURE)
				return true;

			// add signature to database
			sid = pStore->addSignature(pNameR);
			// add to imprints to index
			pStore->addImprintAssociative(&treeR, pEvalFwd, pEvalRev, sid);

			signature_t *pSignature = pStore->signatures + sid;
			pSignature->flags = signature_t::SIGMASK_UNSAFE;
			pSignature->size = treeR.count - tinyTree_t::TINYTREE_NSTART;

			pSignature->numPlaceholder = numPlaceholder;
			pSignature->numEndpoint = numEndpoint;
			pSignature->numBackRef = numBackRef;

			return true;
		}

		/*
		 * !! NOTE: The following selection is just for the display name.
		 *          Better choices will be analysed later.
		 */

		signature_t *pSignature = pStore->signatures + sid;

		int cmp = 0; // "<0" if "best < candidate", ">0" if "best > candidate"

		// Test for prime goal: reducing number of nodes
		if (cmp == 0)
			cmp = pSignature->size - (treeR.count - tinyTree_t::TINYTREE_NSTART);

		// Test for secondary goal: reduce number of unique endpoints, thus connections
		if (cmp == 0)
			cmp = pSignature->numPlaceholder - numPlaceholder;

		// Test for preferred display selection: least number of endpoints
		if (cmp == 0)
			cmp = pSignature->numEndpoint - numEndpoint;

		// Test for preferred display selection: least number of back-references
		if (cmp == 0)
			cmp = pSignature->numBackRef - numBackRef;

		// distinguish between shallow compare (`"-+"`) or deep compare (`"<>"`)
		if (cmp < 0) {
			cmp = '-'; // worse by numbers
		} else if (cmp > 0) {
			cmp = '+'; // better by numbers
		} else {
			/*
			 * Compare layouts, expensive
			 */
			tinyTree_t treeL(ctx);
			treeL.decodeFast(pSignature->name);

			cmp = treeL.compare(treeL.root, treeR, treeR.root);

			if (cmp < 0)
				cmp = '<'; // worse by compare
			else if (cmp > 0)
				cmp = '>'; // better by compare
			else
				cmp = '='; // equals
		}

		if (opt_text == 2)
			printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", ctx.progress, sid, cmp, pNameR, treeR.count - tinyTree_t::TINYTREE_NSTART, numPlaceholder, numEndpoint, numBackRef);

		/*
		 * Update record if candidate is better
		 */
		if (cmp == '>' || cmp == '+') {
			if (opt_text == 1)
				printf("%s\t%u\t%u\t%u\n", pNameR, numPlaceholder, numEndpoint, numBackRef);

			// only update if signatures are writable
			if (~pStore->allocFlags & database_t::ALLOCMASK_SIGNATURE)
				return true;

			::strcpy(pSignature->name, pNameR);
			pSignature->size = treeR.count - tinyTree_t::TINYTREE_NSTART;
			pSignature->numPlaceholder = numPlaceholder;
			pSignature->numEndpoint = numEndpoint;
			pSignature->numBackRef = numBackRef;
		}

		return true;
	}

	/**
	 * @date 2020-03-27 17:05:07
	 *
	 * Compare function for `qsort_r`
	 *
	 * @param {signature_t} lhs - left hand side signature
	 * @param {signature_t} rhs - right hand side signature
	 * @param {context_t} arg - I/O context
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int comparSignature(const void *lhs, const void *rhs, void *arg) {
		if (lhs == rhs)
			return 0;

		const signature_t *pSignatureL = static_cast<const signature_t *>(lhs);
		const signature_t *pSignatureR = static_cast<const signature_t *>(rhs);
		context_t *pApp = static_cast<context_t *>(arg);

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.decodeFast(pSignatureL->name);
		treeR.decodeFast(pSignatureR->name);

		/*
		 * Compare
		 */

		int cmp = 0;

		// Test for prime goal: reducing number of nodes
		cmp = treeL.count - treeR.count;
		if (cmp)
			return cmp;

		// Test for secondary goal: reduce number of unique endpoints, thus connections
		cmp = pSignatureL->numPlaceholder - pSignatureR->numPlaceholder;
		if (cmp)
			return cmp;

		// Test for preferred display selection: least number of endpoints
		cmp = pSignatureL->numEndpoint - pSignatureR->numEndpoint;
		if (cmp)
			return cmp;

		// Test for preferred display selection: least number of back-references
		cmp = pSignatureL->numBackRef - pSignatureR->numBackRef;
		if (cmp)
			return cmp;

		// Compare layouts, expensive
		cmp = treeL.compare(treeL.root, treeR, treeR.root);
		return cmp;
	}

	/**
	 * @date 2020-04-15 19:07:53
	 *
	 * Recreate imprint index for signature groups
	 */
	void rebuildImprints(void) {
		// clear signature and imprint index
		::memset(pStore->imprintIndex, 0, sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize);

		if (pStore->numSignature < 2)
			return; //nothing to do

		// skip reserved entry
		pStore->numImprint = 1;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Rebuilding imprints\n", ctx.timeAsString());

		/*
		 * Create imprints for signature groups
		 */

		generatorTree_t tree(ctx);

		// reset ticker
		ctx.setupSpeed(pStore->numSignature);
		ctx.tick = 0;

		// create imprints for signature groups
		ctx.progress++; // skip reserved
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				if (perSecond == 0 || ctx.progress > ctx.progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numImprint=%u(%.0f%%) | hash=%.3f",
					        ctx.timeAsString(), ctx.progress, perSecond,
					        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					        (double) ctx.cntCompare / ctx.cntHash);
				} else {
					int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numImprint=%u(%.0f%%) | hash=%.3f",
					        ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
					        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					        (double) ctx.cntCompare / ctx.cntHash);
				}

				ctx.tick = 0;
			}

			const signature_t *pSignature = pStore->signatures + iSid;

			tree.decodeFast(pSignature->name);

			unsigned sid, tid;

			if (!pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid))
				pStore->addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Imprints built. numImprint=%u(%.0f%%) | hash=%.3f\n",
			        ctx.timeAsString(),
			        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
			        (double) ctx.cntCompare / ctx.cntHash);
	}

	/**
	 * @date 2020-04-21 18:56:28
	 *
	 * Read signatures from file
	 */
	void /*__attribute__((optimize("O0")))*/ signaturesFromFile(void) {

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading signatures from file\n", ctx.timeAsString());

		FILE *f = fopen(this->opt_load, "r");
		if (f == NULL)
			ctx.fatal("{\"error\":\"fopen() failed\",\"where\":\"%s\",\"name\":\"%s\",\"reason\":\"%m\"}\n",
			          __FUNCTION__, this->opt_load);

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;

		char name[64];
		unsigned numPlaceholder, numEndpoint, numBackRef;

		unsigned skipDuplicate = 0;

		// <cid> <sid> <candidateName> <cmp> <size> <numPlaceholder> <numEndpoint> <numBackRef>
		while (fscanf(f, "%s %u %u %u\n", name, &numPlaceholder, &numEndpoint, &numBackRef) == 4) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | skipDuplicate=%u | hash=%.3f",
				        ctx.timeAsString(), ctx.progress, perSecond,
				        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				        skipDuplicate, (double) ctx.cntCompare / ctx.cntHash);

				ctx.tick = 0;
			}

			/*
			 * construct tree
			 */
			generator.decodeFast(name);

			/*
			 * call `foundTreeSignature()`
			 */

			foundTreeSignature(generator, name, numPlaceholder, numEndpoint, numBackRef);

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "[%s] Read %ld candidates. numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | skipDuplicate=%u | hash=%.3f\n",
			        ctx.timeAsString(),
			        ctx.progress++,
			        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
			        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
			        skipDuplicate, (double) ctx.cntCompare / ctx.cntHash);
	}

	/**
	 * @date 2020-03-22 01:00:05
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add newly unique signatures to the database
	 */
	void signaturesFromGenerator(void) {

		/*
		 * Apply window/task setting on generator
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
			if (this->opt_taskId || this->opt_taskLast) {
				if (this->opt_windowHi)
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%lu-%lu\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_windowLo, this->opt_windowHi);
				else
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%lu-last\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_windowLo);
			} else if (this->opt_windowLo || this->opt_windowHi) {
				if (this->opt_windowHi)
					fprintf(stderr, "[%s] INFO: window=%lu-%lu\n", ctx.timeAsString(), this->opt_windowLo, this->opt_windowHi);
				else
					fprintf(stderr, "[%s] INFO: window=%lu-last\n", ctx.timeAsString(), this->opt_windowLo);
			}
		}

		// apply settings for `--window`
		if (this->opt_windowLo)
			generator.windowLo = this->opt_windowLo;
		if (this->opt_windowHi)
			generator.windowHi = this->opt_windowHi;

		// apply restart data for > `4n9`
		unsigned ofs = 0;
		if (this->arg_numNodes > 4 && this->arg_numNodes < tinyTree_t::TINYTREE_MAXNODES)
			ofs = restartIndex[this->arg_numNodes][(ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0];
		if (ofs)
			generator.pRestartData = restartData + ofs;

		// reset progress
		if (generator.windowHi) {
			ctx.setupSpeed(generator.windowHi);
		} else {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, arg_numNodes);
			ctx.setupSpeed(pMetrics ? pMetrics->numProgress : 0);
		}
		ctx.tick = 0;

		/*
		 * Generate candidates
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", ctx.timeAsString(), arg_numNodes, MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE ? "-pure" : "");

		if (arg_numNodes == 0) {
			generator.root = 0; // "0"
			foundTreeSignature(generator, "0", 0, 0, 0);
			generator.root = 1; // "a"
			foundTreeSignature(generator, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = arg_numNodes * 2 + 1;

			generator.initialiseGenerator(ctx.flags & context_t::MAGICMASK_PURE);
			generator.clearGenerator();
			generator.generateTrees(arg_numNodes, endpointsLeft, 0, 0, this, static_cast<generatorTree_t::generateTreeCallback_t>(&gensignatureContext_t::foundTreeSignature));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.progress != ctx.progressHi && this->opt_windowLo == 0 && this->opt_windowHi == 0) {
			// can only test if windowing is disabled
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%u}\n",
			       __FUNCTION__, ctx.progress, ctx.progressHi, arg_numNodes);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%u pure=%u interleave=%u numNode=%u numCandidate=%lu numSignature=%u(%.0f%%) numImprint=%u(%.0f%%)\n",
			        ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, pStore->interleave, arg_numNodes, ctx.progress,
			        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
			        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint);

	}

	/**
	 * @date 2020-04-21 22:04:41
	 *
	 * Finalise signatures by sorting.
	 *
	 * This should have no effect pre-loaded signaturs (they were already sorted)
	 */
	void finaliseSignatures(void) {

		// Sort signatures. This will invalidate index and imprints

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Sorting signatures\n", ctx.timeAsString());

		assert(pStore->numSignature >= 1);
		qsort_r(pStore->signatures + 1, pStore->numSignature - 1, sizeof(*pStore->signatures), this->comparSignature, this);

		/*
		 * List result
		 */
		if (opt_text == 3) {
			for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
				const signature_t *pSignature = pStore->signatures + iSid;
				printf("%s\t%u\t%u\t%u\n", pSignature->name, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef);
			}
		}
		if (opt_text == 4) {
			for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
				const signature_t *pSignature = pStore->signatures + iSid;
				printf("%u\t%s\t%u\t%u\t%u\t%u\n", iSid, pSignature->name, pSignature->size, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef);
			}
		}

		// rebuild imprints
		if (this->arg_outputDatabase)
			this->rebuildImprints();
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
 * @global {gensignatureSelftest_t} Application context
 */
gensignatureContext_t app(ctx);

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
	if (app.arg_outputDatabase) {
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
 * @date 2020-03-14 11:17:04
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]  -- Add signatures of given node size\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>           Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --maximprint=<number>           Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxsignature=<number>         Maximum number of signatures [default=%u]\n", app.opt_maxSignature);
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say more\n");
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --task=sge                      Get window task settings from SGE environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>              Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text[=1]                      Signatures from `foundTree()`\n");
		fprintf(stderr, "\t   --text=2                        Sorted signatures from output database\n");
		fprintf(stderr, "\t   --text=3                        Candidates from `foundTree()`\n");
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose                       Say less\n");
		fprintf(stderr, "\t   --window=[<low>,]<high>         Upper end restart window [default=%lu,%lu]\n", app.opt_windowLo, app.opt_windowHi);
	}
}

/**
 * @date 2020-03-14 11:19:40
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
			LO_GENERATE,
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_LOAD,
			LO_MAXIMPRINT,
			LO_MAXSIGNATURE,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_PARANOID,
			LO_PURE,
			LO_RATIO,
			LO_SIGNATUREINDEXSIZE,
			LO_TASK,
			LO_TEXT,
			LO_TIMER,
			LO_WINDOW,
			// short opts
			LO_HELP = 'h',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",              1, 0, LO_DEBUG},
			{"force",              0, 0, LO_FORCE},
			{"generate",           0, 0, LO_GENERATE},
			{"help",               0, 0, LO_HELP},
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",         1, 0, LO_INTERLEAVE},
			{"load",               1, 0, LO_LOAD},
			{"maximprint",         1, 0, LO_MAXIMPRINT},
			{"maxsignature",       1, 0, LO_MAXSIGNATURE},
			{"no-generate",        0, 0, LO_NOGENERATE},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-pure",            0, 0, LO_NOPURE},
			{"paranoid",           0, 0, LO_PARANOID},
			{"pure",               0, 0, LO_PURE},
			{"quiet",              2, 0, LO_QUIET},
			{"ratio",              1, 0, LO_RATIO},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"task",               1, 0, LO_TASK},
			{"text",               2, 0, LO_TEXT},
			{"timer",              1, 0, LO_TIMER},
			{"verbose",            2, 0, LO_VERBOSE},
			{"window",             1, 0, LO_WINDOW},
			//
			{NULL,                 0, 0, 0}
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
			case LO_GENERATE:
				app.opt_generate++;
				break;
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_IMPRINTINDEXSIZE:
				app.opt_imprintIndexSize = ctx.nextPrime(strtoul(optarg, NULL, 0));
				break;
			case LO_INTERLEAVE:
				app.opt_interleave = (unsigned) strtoul(optarg, NULL, 0);
				if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
					ctx.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
				break;
			case LO_LOAD:
				app.opt_load = optarg;
				break;
			case LO_MAXIMPRINT:
				app.opt_maxImprint = ctx.nextPrime(strtoull(optarg, NULL, 0));
				break;
			case LO_MAXSIGNATURE:
				app.opt_maxSignature = ctx.nextPrime(strtoull(optarg, NULL, 0));
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
			case LO_PARANOID:
				ctx.flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_PURE:
				ctx.flags |= context_t::MAGICMASK_PURE;
				break;
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_RATIO:
				app.opt_ratio = strtof(optarg, NULL);
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
			case LO_SIGNATUREINDEXSIZE:
				app.opt_signatureIndexSize = ctx.nextPrime(strtoul(optarg, NULL, 0));
				break;
			case LO_TEXT:
				app.opt_text = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_text + 1;
				break;
			case LO_TIMER:
				ctx.opt_timer = strtoul(optarg, NULL, 0);
				break;
			case LO_VERBOSE:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
				break;
			case LO_WINDOW: {
				uint64_t m, n;

				int ret = sscanf(optarg, "%lu,%lu", &m, &n);
				if (ret == 2) {
					app.opt_windowLo = m;
					app.opt_windowHi = n;
				} else if (ret == 1) {
					app.opt_windowHi = m;
				} else {
					usage(argv, true);
					exit(1);
				}

				break;
			}
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
	if (argc - optind >= 1)
		app.arg_inputDatabase = argv[optind++];

	if (argc - optind >= 1) {
		char *endptr;

		errno = 0; // To distinguish success/failure after call
		app.arg_numNodes = (unsigned) strtoul(argv[optind++], &endptr, 0);

		// strip trailing spaces
		while (*endptr && isspace(*endptr))
			endptr++;

		// test for error
		if (errno != 0 || *endptr != '\0')
			app.arg_inputDatabase = NULL;
	}

	if (argc - optind >= 1)
		app.arg_outputDatabase = argv[optind++];

	if (app.arg_inputDatabase == NULL) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * `--task` post-processing
	 */
	if (app.opt_taskId || app.opt_taskLast) {
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, app.arg_numNodes);
		if (!pMetrics)
			ctx.fatal("no preset for --task\n");

		// split progress into chunks
		uint64_t taskSize = pMetrics->numProgress / app.opt_taskLast;
		if (taskSize == 0)
			taskSize = 1;
		app.opt_windowLo = taskSize * (app.opt_taskId - 1);
		app.opt_windowHi = taskSize * app.opt_taskId;

		// last task is open ended in case metrics are off
		if (app.opt_taskId == app.opt_taskLast)
			app.opt_windowHi = 0;
	}
	if (app.opt_windowHi && app.opt_windowLo >= app.opt_windowHi) {
		fprintf(stderr, "--window low exceeds high\n");
		exit(1);
	}

	if (app.opt_windowLo || app.opt_windowHi) {
		if (app.arg_numNodes > tinyTree_t::TINYTREE_MAXNODES || restartIndex[app.arg_numNodes][(ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0] == 0) {
			fprintf(stderr, "No restart data for --window\n");
			exit(1);
		}
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
	 * Open input and create output database
	 */

	// Open input
	database_t db(ctx);

	db.open(app.arg_inputDatabase, true);

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

#if defined(ENABLE_JANSSON)
	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	/*
	 * create output
	 */

	database_t store(ctx);

	/*
	 * @date 2020-04-21 19:59:47
	 *
	 * if (rebuildSection)
	 *   rebuild();
	 * else if (inheritSection)
	 *   inherit();
	 * else
	 *   copy();
	 */

	// sections that need rebuilding
	unsigned rebuildSections = 0;
	// sections to inherit from original database. Can also be interpreted as ReadOnly.
	unsigned inheritSections = database_t::ALLOCMASK_TRANSFORM;

	// flag that signatures to be collected or sorted
	unsigned collectSignatures = (app.arg_outputDatabase != NULL || app.opt_text == 3 || app.opt_text == 4);

	/*
	 * @date 2020-03-17 13:57:25
	 *
	 * Database indices are hashlookup tables with overflow.
	 * The art is to have a hash function that distributes evenly over the hashtable.
	 * If index entries are in use, then jump to overflow entries.
	 * The larger the index in comparison to the number of data entries the lower the chance an overflow will occur.
	 * The ratio between index and data size is called `ratio`.
	 */

	// interleave
	{
		if (app.opt_interleave)
			store.interleave = app.opt_interleave; // manual
		else if (db.interleave)
			store.interleave = db.interleave; // inherit
		else
			store.interleave = METRICS_DEFAULT_INTERLEAVE; // default

		// find matching `interleaveStep`
		const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, store.interleave);
		if (!pMetrics)
			ctx.fatal("no preset for --interleave\n");

		store.interleaveStep = pMetrics->interleaveStep;
	}

	// signatures
	if (app.opt_maxSignature != 0) {
		store.maxSignature = app.opt_maxSignature; // user specified
	} else if (!collectSignatures) {
		store.maxSignature = db.maxSignature; // keep section read-only
		store.signatureIndexSize = db.signatureIndexSize;
	} else {
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, app.arg_numNodes);
		if (!pMetrics)
			ctx.fatal("no preset for --maxsignature\n");

		store.maxSignature = pMetrics->numSignature;
	}

	if (store.signatureIndexSize == 0) {
		if (app.opt_signatureIndexSize == 0)
			store.signatureIndexSize = ctx.nextPrime(store.maxSignature * app.opt_ratio);
		else
			store.signatureIndexSize = app.opt_signatureIndexSize;
	}

	// optional hints
	if (db.numHint != 0) {
		store.maxHint = db.maxHint;
		store.hintIndexSize = ctx.nextPrime(store.maxHint * app.opt_ratio);
	}

	// imprints
	if (app.opt_maxImprint != 0) {
		store.maxImprint = app.opt_maxImprint; // user specified
	} else if (!collectSignatures) {
		store.maxImprint = db.maxImprint; // keep section read-only
		store.imprintIndexSize = db.imprintIndexSize;
	} else {
		const metricsImprint_t *pMetrics = getMetricsImprint(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, store.interleave, app.arg_numNodes);
		if (!pMetrics)
			ctx.fatal("no preset for --maximprint\n");

		store.maxImprint = pMetrics->numImprint;
	}

	if (store.imprintIndexSize == 0) {
		if (app.opt_imprintIndexSize != 0)
			store.imprintIndexSize = app.opt_imprintIndexSize;
		else
			store.imprintIndexSize = ctx.nextPrime(store.maxImprint * app.opt_ratio);
	}

	/*
	 * section inheriting
	 */

	// changing interleave needs imprint rebuilding. This also validates imprintIndex
	if (store.interleave != db.interleave)
		rebuildSections |= database_t::ALLOCMASK_IMPRINT;

	// signatures
	if (!collectSignatures)
		inheritSections |= database_t::ALLOCMASK_SIGNATURE; // no output, signatures and imprints are read-only
	if (store.signatureIndexSize != db.signatureIndexSize)
		rebuildSections |= database_t::ALLOCMASK_SIGNATUREINDEX;

	// optional hints
	if (db.numHint > 0) {
		inheritSections |= database_t::ALLOCMASK_HINT;
		if (store.hintIndexSize != db.hintIndexSize)
			rebuildSections |= database_t::ALLOCMASK_HINTINDEX;
	}

	// imprints
	if (!collectSignatures)
		inheritSections |= database_t::ALLOCMASK_IMPRINT; // no output, signatures and imprints are read-only
	if (store.imprintIndexSize != db.imprintIndexSize)
		rebuildSections |= database_t::ALLOCMASK_IMPRINTINDEX;

	// rebuilt (rw) sections may not be inherited (ro)
	inheritSections &= ~rebuildSections;

	// preloading signatures requires writable signatures and imprints
	if (app.opt_load)
		inheritSections &= ~(database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_SIGNATUREINDEX | database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX);

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("gensignatureContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("gensignatureContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	/*
	 * Finalise allocations and create database
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		// Assuming with database allocations included
		size_t allocated = ctx.totalAllocated + store.estimateMemoryUsage(inheritSections);

		struct sysinfo info;
		if (sysinfo(&info) == 0) {
			double percent = 100.0 * allocated / info.freeram;
			if (percent > 80)
				fprintf(stderr, "WARNING: using %.1f%% of free memory\n", percent);
		}
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] Store create: interleave=%u maxSignature=%u signatureIndex=%u maxImprint=%u imprintIndex=%u\n", ctx.timeAsString(), store.interleave, store.maxSignature, store.signatureIndexSize, store.maxImprint, store.imprintIndexSize);

	// actual create
	store.create(inheritSections);
	app.pStore = &store;

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS && (~rebuildSections & ~inheritSections))
		fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);

	/*
	 * Inherit/copy sections
	 */

	store.inheritSections(&db, app.arg_inputDatabase, inheritSections);

	// early initialise or the progress ticker will be misunderstood for section copying
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, store.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, store.revTransformData);

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS && (~rebuildSections & ~inheritSections))
		fprintf(stderr, "[%s] Copying database sections\n", ctx.timeAsString());

	// signatures
	if (~rebuildSections & ~inheritSections & database_t::ALLOCMASK_SIGNATURE) {
		if (db.numSignature == 0) {
			// input section empty
			store.numSignature = 1;
		} else {
			assert(store.maxSignature >= db.numSignature);
			::memcpy(store.signatures, db.signatures, db.numSignature * sizeof(*store.signatures));
			store.numSignature = db.numSignature;
		}
	}

	// optional hints
	if (db.numHint > 0) {
		if (~rebuildSections & ~inheritSections & database_t::ALLOCMASK_HINT) {
			assert(store.maxHint >= db.numHint);
			::memcpy(store.hints, db.hints, db.numHint * sizeof(*store.hints));
			store.numHint = db.numHint;
		}
	}

	// imprints
	if (~rebuildSections & ~inheritSections & database_t::ALLOCMASK_IMPRINT) {
		if (db.numImprint == 0) {
			// input section empty
			store.numImprint = 1;
		} else {
			assert(store.maxImprint >= db.numImprint);
			::memcpy(store.imprints, db.imprints, db.numImprint * sizeof(*store.imprints));
			store.numImprint = db.numImprint;
		}
	}

	// skip reserved first entry
	assert(store.numSignature >= 1);
	assert(store.numImprint >= 1);

	/*
	 * Rebuild sections
	 */

	if (rebuildSections & database_t::ALLOCMASK_IMPRINT) {
		// rebuild imprints
		app.rebuildImprints();
		rebuildSections &= ~(database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX);
	}
	if (rebuildSections)
		store.rebuildIndices(rebuildSections);

	/*
	 * Where to look for new candidates
	 */

	if (app.opt_load)
		app.signaturesFromFile();
	if (app.opt_generate)
		app.signaturesFromGenerator();

	/*
	 * re-order and re-index signatures
	 */

	if (collectSignatures) {
		// sort and reindex signatures
		app.finaliseSignatures();
	}

	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

	if (app.opt_taskLast)
		fprintf(stderr, "{\"done\":\"%s\",\"taskId\":%u,\"taskLast\":%u,\"windowLo\":\"%lu\",\"windowHi\":\"%lu\"}\n", argv[0], app.opt_taskId, app.opt_taskLast, app.opt_windowLo, app.opt_windowHi);
	else if (app.opt_windowLo || app.opt_windowHi)
		fprintf(stderr, "{\"done\":\"%s\",\"windowLo\":\"%lu\",\"windowHi\":\"%lu\"}\n", argv[0], app.opt_windowLo, app.opt_windowHi);
	else
		fprintf(stderr, "{\"done\":\"%s\"}\n", argv[0]);

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
