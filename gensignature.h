/*
 * @date 2020-03-14 11:09:15
 *
 * `gensignature` scans `*n9` space using `generator_t` and adds associative unique footprints to a given dataset.
 * Associative unique is when all other permutation of endpoints are excluded.
 *
 * `gensignature` can also generate SQL used for signature group analysis.
 *
 * Each footprint can consist of a collection of unique structures called signature group.
 * One member of each signature group, the structure with the most fitting notation, is called the representative.
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
 * - Of every expression it permutes all possible 9!=363600 inputs
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
 * @date 2020-05-01 17:17:32
 *
 * Text modes:
 *
 * `--text[=1]` Brief mode that show selected candidates passed to `foundTreeSignature()`.
 *              Selected candidates are those that challenge and win the current display name.
 *              Also intended for transport and merging when broken into multiple tasks.
 *              Can be used for the `--load=<file>` option.
 *              Output can be converted to other text modes by `"./gensignature <input.db> <numNode> [<output.db>]--load=<inputList> --no-generate --text=<newMode> '>' <outputList>
 *
 *              <name>
 *
 * `--text=2`   Full mode of all candidates passed to `foundTreeSignature()` including what needed to compare against the display name.
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
 *              NOTE: requires sorting and will copy (not inherit) imprint section
 *
 *              <name>
 *
 * `--text=4`   Selected and sorted signatures that are written to the output database
 *              NOTE: requires sorting and will copy (not inherit) imprint section
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
 *
 *  @date 2020-04-24 11:34:48
 *
 *  workflow:
 *
 *      - set system model with `"--[no-]pure" "--[no-]paranoid"`
 *      - database settings with `"--interleave=" "--maxsignature=" "--maximprint=" "--ratio" "--signatureindexsize=" "--imprintindexsize"`
 *      - rebuild/inherit/copy database sections
 *      - load candidate signatures from file when `"--load"` with `"--task=" "--window="`
 *      - generate candidate signatures when `"--generate"` `"--task=" "--window="`
 *
 * @date 2020-04-24 18:14:26
 *
 * With the new add-if-not-found database can be stored/archived with `"--interleave=1"` and have imprints quickly created on the fly.
 * This massively saves storage.
 *
 * @date 2020-04-25 21:49:21
 *
 * add-if-not-found only works if tid's can be ignored, otherwise it creates false positives.
 * It is perfect for ultra-high speed pre-processing and low storage.
 * Only activate with `--fast` option and issue a warning that it is experimental.
 *
 * @date 2021-08-06 15:35:26
 *
 * `--markmixed` is intended to flag signatures that are used for `mixed` lookups.
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
#include <jansson.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>

#include "database.h"
#include "dbtool.h"
#include "generator.h"
#include "metrics.h"
#include "restartdata.h"

/**
 * @date 2020-03-14 11:10:15
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct gensignatureContext_t : dbtool_t {

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
	/// @var {number} Tree size in nodes to be generated for this invocation;
	unsigned   arg_numNodes;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned   opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned   opt_generate;
	/// @var {number} List incomplete signatures (LOOKUP and !SAFE), for inclusion
	unsigned   opt_listIncomplete;
	/// @var {number} List empty/unsafe signatures, for exclusion
	unsigned   opt_listSafe;
	/// @var {number} List safe signatures, for inclusion
	unsigned   opt_listUnsafe;
	/// @var {number} List used signatures (have members), for inclusion
	unsigned   opt_listUsed;
	/// @var {string} name of file containing members
	const char *opt_load;
	/// @var {number} flag signatures that have pure with top-level mixed members
	unsigned opt_markMixed;
	/// @var {number} --mixed, consider/accept top-level mixed
	unsigned opt_mixed;
	/// @var {number} save imprints with given interleave
	unsigned opt_saveInterleave;
	/// @var {number} task Id. First task=1
	unsigned   opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned   opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned   opt_text;
	/// @var {number} truncate on database overflow
	double     opt_truncate;
	/// @var {number} generator upper bound
	uint64_t   opt_windowHi;
	/// @var {number} generator lower bound
	uint64_t   opt_windowLo;

	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	/// @var {number} - THE generator
	generator_t generator;
	/// @var {number} `foundTree()` duplicate by name
	unsigned    skipDuplicate;
	/// @var {number} Where database overflow was caught
	uint64_t        truncated;
	/// @var {number} Name of signature causing overflow
	char            truncatedName[tinyTree_t::TINYTREE_NAMELEN + 1];

	/**
	 * Constructor
	 */
	gensignatureContext_t(context_t &ctx) : dbtool_t(ctx), generator(ctx) {
		// arguments and options
		arg_inputDatabase  = NULL;
		arg_numNodes       = 0;
		arg_outputDatabase = NULL;
		opt_force          = 0;
		opt_generate       = 1;
		opt_listIncomplete = 0;
		opt_listSafe       = 0;
		opt_listUnsafe     = 0;
		opt_listUsed       = 0;
		opt_load           = NULL;
		opt_markMixed      = 0;
		opt_mixed          = 0;
		opt_saveInterleave = 0;
		opt_taskId         = 0;
		opt_taskLast       = 0;
		opt_text           = 0;
		opt_truncate       = 0;
		opt_windowHi       = 0;
		opt_windowLo       = 0;

		pStore           = NULL;
		skipDuplicate    = 0;
		truncated        = 0;
		truncatedName[0] = 0;
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
	bool foundTreeSignature(tinyTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {

		if (this->truncated)
			return false; // quit as fast as possible

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | skipDuplicate=%u hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
					pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					skipDuplicate, (double) ctx.cntCompare / ctx.cntHash, pNameR);
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
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | skipDuplicate=%u hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - generator.windowLo) * 100.0 / (ctx.progressHi - generator.windowLo), etaH, etaM, etaS,
					pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
					pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					skipDuplicate, (double) ctx.cntCompare / ctx.cntHash, pNameR);
			}

#if 0
			if (ctx.restartTick) {
				// passed a restart point
				fprintf(stderr, "\n");
				ctx.restartTick = 0;
			}
#endif

			ctx.tick = 0;
		}

		/*
		 * test for duplicates
		 */

		unsigned six = pStore->lookupSignature(pNameR);
		if (pStore->signatureIndex[six] != 0) {
			// duplicate candidate name
			skipDuplicate++;
			return true;
		}

		/*
		 * Test for database overflow
		 */
		if (this->opt_truncate) {
			// avoid `"storage full"`. Give warning later
			if (pStore->maxImprint - pStore->numImprint <= pStore->interleave || pStore->maxSignature - pStore->numSignature <= 1) {
				// break now, display text later/ Leave progress untouched
				this->truncated = ctx.progress;
				::strcpy(this->truncatedName, pNameR);

				// quit as fast as possible
				return false;
			}
		}

		/*
		 * @date 2021-07-20 09:51:35
		 *
		 * `--pure` v2 experiment: components must be QnTF, except signature top-level
		 * Generator is still mixed mode so use a different flag and test explicitly here
		 *
		 * @date 2021-08-04 11:58:56
		 *
		 * Record if a signature contains a full/mixed/pure structure
		 * This is to flag the core collection that should be available for lookups.
		 */
		enum { FULL, MIXED, PURE } area;
		area = PURE;

		for (unsigned k = tinyTree_t::TINYTREE_NSTART; k < treeR.root; k++) {
			if (!(treeR.N[k].T & IBIT)) {
				area = FULL;
				break;
			}
		}
		if (area == PURE && !(treeR.N[treeR.root].T & IBIT))
			area = MIXED;

		// with `--mixed`, only accept PURE/MIXED
		if (opt_mixed && area == FULL)
			return true;

		/*
		 * Lookup/add data store.
		 * Consider signature groups `unsafe` (no members yet)
		 */

		/*
		 * @date 2020-04-24 14:04:13
		 *
		 * If imprints are writable perform fast 'add-if-not-found'
		 */

		unsigned sid              = 0;
		unsigned origNumSignature = pStore->numSignature;

		if ((ctx.flags & context_t::MAGICMASK_AINF) && !this->readOnlyMode) {
			/*
			 * @date 2020-04-25 22:00:29
			 *
			 * WARNING: add-if-not-found only checks tid=0 to determine if (not-)found.
			 *          This creates false-positives.
			 *          Great for high-speed loading, but not for perfect duplicate detection.
			 *          To get better results, re-run with next increment interleave.
			 */
			// add to imprints to index
			sid = pStore->addImprintAssociative(&treeR, pStore->fwdEvaluator, pStore->revEvaluator, origNumSignature);
		} else {
			unsigned tid = 0;
			pStore->lookupImprintAssociative(&treeR, pStore->fwdEvaluator, pStore->revEvaluator, &sid, &tid);
		}

		// add to datastore if not found
		if (sid == 0) {
			// won challenge
			if (opt_text == OPTTEXT_WON)
				printf("%s\n", pNameR);

			// only add if signatures are writable
			if (!this->readOnlyMode) {
				// add signature to database
				sid = pStore->addSignature(pNameR);
				assert(sid == origNumSignature);

				// add to name index
				pStore->signatureIndex[six] = sid;

				// add to imprints to index
				if (!(ctx.flags & context_t::MAGICMASK_AINF)) {
					unsigned newSid = pStore->addImprintAssociative(&treeR, pStore->fwdEvaluator, pStore->revEvaluator, sid);
					assert(newSid == 0 || newSid == origNumSignature);
				}

				signature_t *pSignature = pStore->signatures + sid;
				pSignature->flags = 0;
				if (opt_markMixed && area != FULL)
					pSignature->flags |= signature_t::SIGMASK_KEY;
				pSignature->size  = treeR.count - tinyTree_t::TINYTREE_NSTART;

				pSignature->numPlaceholder = numPlaceholder;
				pSignature->numEndpoint    = numEndpoint;
				pSignature->numBackRef     = numBackRef;
			}

			return true;
		}

		signature_t *pSignature = pStore->signatures + sid;

		if (opt_markMixed && area != FULL && !this->readOnlyMode) {
			// update flags
			pSignature->flags |= signature_t::SIGMASK_KEY;
		}

		/*
		 * !! NOTE: The following selection is just for the display name.
		 *          Better choices will be analysed later.
		 */

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
			treeL.loadStringFast(pSignature->name);

			cmp = treeL.compare(treeL.root, &treeR, treeR.root);

			if (cmp < 0)
				cmp = '<'; // worse by compare
			else if (cmp > 0)
				cmp = '>'; // better by compare
			else
				cmp = '='; // equals
		}

		if (opt_text == OPTTEXT_COMPARE)
			printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", ctx.progress, sid, cmp, pNameR, treeR.count - tinyTree_t::TINYTREE_NSTART, numPlaceholder, numEndpoint, numBackRef);

		/*
		 * Update record if candidate is better
		 */
		if (cmp == '>' || cmp == '+') {
			// won challenge
			if (opt_text == OPTTEXT_WON)
				printf("%s\n", pNameR);

			// only add if signatures are writable
			if (!this->readOnlyMode) {
				assert(strlen(pNameR) <= signature_t::SIGNATURENAMELENGTH);
				::strcpy(pSignature->name, pNameR);
				pSignature->size           = treeR.count - tinyTree_t::TINYTREE_NSTART;
				pSignature->numPlaceholder = numPlaceholder;
				pSignature->numEndpoint    = numEndpoint;
				pSignature->numBackRef     = numBackRef;
			}
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
		context_t         *pApp        = static_cast<context_t *>(arg);

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.loadStringFast(pSignatureL->name);
		treeR.loadStringFast(pSignatureR->name);

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
		cmp = treeL.compare(treeL.root, &treeR, treeR.root);
		return cmp;
	}

	/**
	 * @date 2020-04-15 19:07:53
	 *
	 * Recreate imprint index for signature groups
	 */
	void rebuildImprints(void) {
		// clear signature and imprint index
		::memset(pStore->imprintIndex, 0, pStore->imprintIndexSize * sizeof(*pStore->imprintIndex));

		if (pStore->numSignature < 2)
			return; //nothing to do

		// skip reserved entry
		pStore->numImprint = 1;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Rebuilding imprints\n", ctx.timeAsString());

		/*
		 * Create imprints for signature groups
		 */

		tinyTree_t tree(ctx);

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

			tree.loadStringFast(pSignature->name);

			/*
			 * @date 2020-04-27 12:32:06
			 *
			 * Imprints are being rebuild from stored signatures.
			 * These signatures are unique and therefore safe to use add-if-not-found
			 * Keep old code for historics
			 */
#if 1
			unsigned ret = pStore->addImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, iSid);
			assert(ret == 0);
#else
			unsigned sid, tid;

			if (ctx.flags & context_t::MAGICMASK_AINF) {
				// add-if-not-found, but actually it should not have been found
				unsigned ret = pStore->addImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, iSid);
				assert(ret == 0);
			} else {
				if (!pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sid, &tid))
					pStore->addImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, iSid);
			}
#endif

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
	 * @date 2021-08-08 11:04:51
	 *
	 * Output a signaure with flags
	 */
	inline void signatureLine(const signature_t *pSignature) {
		printf("%s", pSignature->name);

		if (pSignature->flags)
			putchar('\t');
		if (pSignature->flags & signature_t::SIGMASK_SAFE)
			putchar('S');
		if (pSignature->flags & signature_t::SIGMASK_PROVIDES)
			putchar('P');
		if (pSignature->flags & signature_t::SIGMASK_REQUIRED)
			putchar('R');
		if (pSignature->flags & signature_t::SIGMASK_KEY)
			putchar('K');
		putchar('\n');
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
			ctx.fatal("\n{\"error\":\"fopen('%s') failed\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n",
				  this->opt_load, __FUNCTION__, __FILE__, __LINE__);

		// apply settings for `--window`
		generator.windowLo = this->opt_windowLo;
		generator.windowHi = this->opt_windowHi;

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;
		skipDuplicate = 0;

		tinyTree_t tree(ctx);

		// <name> [ <numPlaceholder> <numEndpoint> <numBackRef> ]
		for (;;) {
			static char line[512];
			char *pLine = line;

			if (::fgets(line, sizeof(line), f) == 0)
				break; // end-of-input

			/*
			 * load name
			 */

			char *pName = pLine;

			while (*pLine && !::isspace(*pLine))
				pLine++;
			*pLine++ = 0; // terminator

			if (!pName[0])
				ctx.fatal("\n{\"error\":\"bad or empty line\",\"where\":\"%s:%s:%d\",\"line\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress);

			/*
			 * load flags
			 */

			char *pFlags = pLine;

			while (*pLine && !::isspace(*pLine))
				pLine++;
			*pLine++ = 0; // terminator

			/*
			 * construct tree
			 */
			tree.loadStringFast(pName);

			// calculate values
			unsigned        newPlaceholder = 0, newEndpoint = 0, newBackRef = 0;
			unsigned        beenThere      = 0;
			for (const char *p             = pName; *p; p++) {
				if (::islower(*p)) {
					if (!(beenThere & (1 << (*p - 'a')))) {
						newPlaceholder++;
						beenThere |= 1 << (*p - 'a');
					}
					newEndpoint++;
				} else if (::isdigit(*p) && *p != '0') {
					newBackRef++;
				}
			}

			/*
			 * call `foundTreeSignature()`
			 */

			if (!foundTreeSignature(tree, pName, newPlaceholder, newEndpoint, newBackRef))
				break;

			/*
			 * Perform a lookup to update the flags
			 */
			 if (pFlags) {
				unsigned ix  = pStore->lookupSignature(pName);
				unsigned sid = pStore->signatureIndex[ix];

				if (sid) {
					signature_t *pSignature = pStore->signatures + sid;

					/*
					 * Update flags
					 */
					while (*pFlags) {
						if (*pFlags == 'S') {
							// This should be calculated
							// pSignature->flags |= signature_t::SIGMASK_SAFE;
						} else if (*pFlags == 'P') {
							pSignature->flags |= signature_t::SIGMASK_PROVIDES;
						} else if (*pFlags == 'R') {
							pSignature->flags |= signature_t::SIGMASK_REQUIRED;
						} else if (*pFlags == 'K') {
							pSignature->flags |= signature_t::SIGMASK_KEY;
						} else
							ctx.fatal("\n{\"error\":\"unknown flag\",\"where\":\"%s:%s:%d\",\"name\":\"%s\"}\n", __FUNCTION__, __FILE__, __LINE__, pName);

						pFlags++;
					}
				}
			 }

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (truncated) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: Signature/Imprint storage full. Truncating at progress=%lu \"%s\"\n",
					ctx.timeAsString(), this->truncated, this->truncatedName);

			// save position for final status
			this->opt_windowHi = this->truncated;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "[%s] Read %ld candidates. numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | skipDuplicate=%u hash=%.3f\n",
				ctx.timeAsString(),
				ctx.progress,
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
		generator.windowLo = this->opt_windowLo;
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
		skipDuplicate = 0;

		tinyTree_t tree(ctx);

		/*
		 * Generate candidates
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", ctx.timeAsString(), arg_numNodes, MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE ? "-pure" : "");

		if (arg_numNodes == 0) {
			tree.root = 0; // "0"
			foundTreeSignature(tree, "0", 0, 0, 0);
			tree.root = 1; // "a"
			foundTreeSignature(tree, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = arg_numNodes * 2 + 1;

			generator.initialiseGenerator(ctx.flags & context_t::MAGICMASK_PURE);
			generator.clearGenerator();
			generator.generateTrees(arg_numNodes, endpointsLeft, 0, 0, this, static_cast<generator_t::generateTreeCallback_t>(&gensignatureContext_t::foundTreeSignature));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.progress != ctx.progressHi && this->opt_windowLo == 0 && this->opt_windowHi == 0) {
			// can only test if windowing is disabled
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s:%s:%d\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%u}\n",
			       __FUNCTION__, __FILE__, __LINE__, ctx.progress, ctx.progressHi, arg_numNodes);
		}

		if (truncated) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: Signature/Imprint storage full. Truncating at progress=%lu \"%s\"\n",
					ctx.timeAsString(), this->truncated, this->truncatedName);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%u pure=%u numNode=%u interleave=%u numCandidate=%lu numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | skipDuplicate=%u\n",
				ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, arg_numNodes, pStore->interleave, ctx.progress,
				pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				skipDuplicate);

	}

};
