//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-04-18 20:46:40
 *
 * `genhint` creates imprint metrics to hint slice information for job parallelism.
 *
 * This is done by selecting signatures that have imprints with high collision rates.
 * Collisions reduce the index storage.
 * High speed imprint index settings require a lot of storage.
 *
 * Nothing is more frustrating than during a multi hour tuning run having the imprint table overflow.
 *
 * Imprint metrics are non-linear and difficult to predict.
 * `genhint` counts how many imprints a signature actually creates for different interleave settings.
 * This is a very slow process and can take 17 hours with a single job.
 *
 * To create the slices for `imprints.lst`
 * ./genhint --sidhi= --sidlo= --task=m,n <intput.db> -- read section from input and tally `--interleave`
 * ./genhint next..
 * Throughput is around 150k/h (about 6 hours).
 *
 * @date 2020-04-22 21:37:03
 *
 * Text modes:
 *
 * `--text[=1]` Display hints as the generator progresses. There are MAXTRANSFORM*2 hints.
 *              Can be used for the `--load=<file>` option.
 *
 *              <name> <hintForInterleave> <hintForInterleave> ...
 *
 * `--test=3`   Display hints when they are written to the database
 *              NOTE: same format as `--text=1`
 *
 *              <name> <hintForInterleave> <hintForInterleave> ...
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
 * @date 2020-04-18 21:21:22
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genhintContext_t : dbtool_t {

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
	/// @var {number} Analyse input database
	unsigned   opt_analyse;
	/// @var {number} force overwriting of database if already exists
	unsigned   opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned   opt_generate;
	/// @var {string} name of file containing interleave hints
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

	/// @var {unsigned} - active index for `hints[]`
	unsigned activeHintIndex;
	/// @var {number} duplicate swaps in database
	unsigned skipDuplicate;

	/**
	 * Constructor
	 */
	genhintContext_t(context_t &ctx) : dbtool_t(ctx) {
		// arguments and options
		opt_analyse        = 0;
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

		pStore   = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;

		activeHintIndex = 0;
		skipDuplicate   = 0;
	}

	/**
	 * @date 2020-05-02 11:25:24
	 *
	 * Determine hints for signature
	 *
	 * @param {signature_t} pName - signature requiring hits
	 * @param {database_t} tempdb - temporary database to generate imprints with
	 * @return {number} hintId
	 */
	unsigned foundSignatureHints(const char *pName, database_t &tempdb) {

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) numHint=%u(%.0f%%) | skipDuplicate=%u",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numHint, pStore->numHint * 100.0 / pStore->maxHint,
					skipDuplicate);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d numHint=%u(%.0f%%) | skipDuplicate=%u",
					ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - this->opt_sidLo) * 100.0 / (ctx.progressHi - this->opt_sidLo), etaH, etaM, etaS,
					pStore->numHint, pStore->numHint * 100.0 / pStore->maxHint,
					skipDuplicate);
			}

			ctx.tick = 0;
		}

		hint_t     hint;
		tinyTree_t tree(ctx);

		::memset(&hint, 0, sizeof(hint));

		if (this->opt_text == OPTTEXT_WON)
			printf("%s", pName);

		for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {
			// prepare database
			tempdb.InvalidateVersioned();
			tempdb.numImprint     = 1; // skip reserved first entry
			tempdb.interleave     = pInterleave->numStored;
			tempdb.interleaveStep = pInterleave->interleaveStep;

			// add imprint
			tree.decodeFast(pName);
			tempdb.addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, 1 /* dummy sid */);

			// output count
			hint.numStored[pInterleave - metricsInterleave] = tempdb.numImprint - 1;

			if (this->opt_text == OPTTEXT_WON)
				printf("\t%u", tempdb.numImprint - 1);
		}
		if (this->opt_text == OPTTEXT_WON)
			printf("\n");

		// add to database
		if (!this->readOnlyMode) {
			// lookup/add hintId
			unsigned ix     = pStore->lookupHint(&hint);
			unsigned hintId = pStore->hintIndex[ix];
			if (hintId == 0)
				pStore->hintIndex[ix] = hintId = pStore->addHint(&hint);
			else
				skipDuplicate++;
			// add hintId to signature
			return hintId;
		}

		return 0;
	}

	/**
	 * @date 2020-04-19 22:03:49
	 *
	 * Read and add hints from file
	 *
	 * Create generator for given dataset and add newly unique imprint hints to the database
	 */
	void /*__attribute__((optimize("O0")))*/ hintsFromFile(void) {

		/*
		 * Load hints from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading hints from file\n", ctx.timeAsString());

		FILE *f = ::fopen(this->opt_load, "r");
		if (f == NULL)
			ctx.fatal("\n{\"error\":\"fopen('%s') failed\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n",
				  this->opt_load, __FUNCTION__, __FILE__, __LINE__);

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;

		char name[64];

		// <name> <hint0> <hint1> ...
		for (;;) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) numHint=%u(%.0f%%) | skipDuplicate=%u",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numHint, pStore->numHint * 100.0 / pStore->maxHint,
					skipDuplicate);

				ctx.tick = 0;
			}

			static char line[512];
			char        *pLine = line;
			char        *endptr;
			hint_t      hint;

			::memset(&hint, 0, sizeof(hint));

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

				if (pLine == endptr || numEntry >= hint_t::MAXENTRY)
					ctx.fatal("\n{\"error\":\"bad or too many columns\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"line\":%lu}\n",
						  __FUNCTION__, __FILE__, __LINE__, name, ctx.progress);

				hint.numStored[numEntry++] = tid;
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
				// lookup/add hintId
				ix = pStore->lookupHint(&hint);
				unsigned hintId = pStore->hintIndex[ix];
				if (hintId == 0)
					pStore->hintIndex[ix] = hintId = pStore->addHint(&hint);
				else
					skipDuplicate++;

				// add hintId to signature
				if (pStore->signatures[sid].hintId == 0) {
					pStore->signatures[sid].hintId = hintId;
				} else if (pStore->signatures[sid].hintId != hintId)
					ctx.fatal("\n{\"error\":\"inconsistent hint\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"line\":%lu}\n",
						  __FUNCTION__, __FILE__, __LINE__, name, ctx.progress);
			}

			if (opt_text == OPTTEXT_WON) {
				printf("%s", pStore->signatures[sid].name);

				for (unsigned j = 0; j < hint_t::MAXENTRY && hint.numStored[j]; j++)
					printf("\t%u", hint.numStored[j]);

				printf("\n");
			}

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "[%s] Read hints. numHint=%u(%.0f%%) | skipDuplicate=%u\n",
				ctx.timeAsString(),
				pStore->numHint, pStore->numHint * 100.0 / pStore->maxHint,
				skipDuplicate);
	}

	/**
	 * @date 2020-04-17 23:30:39
	 *
	 * Nothing is more frustrating than during a multi hour tuning run having the imprint table overflow.
	 *
	 * Imprint metrics are non-linear and difficult to predict.
	 * The only practical solution is to actually count them and store them in a separate table.
	 * This allows precise memory usage calculations when using windows or high-usage settings.
	 *
	 * @param {database_t} tempdb - worker database to count imprints
	 */
	void hintsFromSignature(database_t &tempdb) {

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
			fprintf(stderr, "[%s] Generating hints.\n", ctx.timeAsString());

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
			if (!pSignature->hintId) {
				uint32_t hintId = foundSignatureHints(pSignature->name, tempdb);
				pSignature->hintId = hintId;
			}

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numHint=%u(%.0f%%) | skipDuplicate=%u\n",
				ctx.timeAsString(),
				pStore->numHint, pStore->numHint * 100.0 / pStore->maxHint,
				skipDuplicate);
	}

	/**
	 * @date 2020-04-20 19:57:08
	 *
	 * Compare function for `qsort_r`
	 *
	 * Compare two hints.
	 * Do not compare them directly, but use the arguments as index to `database_t::hints[]`.
	 *
	 * @param {signature_t} lhs - left hand side hint index
	 * @param {signature_t} rhs - right hand side hint index
	 * @param {genhintContext_t} arg - Application context
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int comparHint(const void *lhs, const void *rhs, void *arg) {
		if (lhs == rhs)
			return 0;

		genhintContext_t  *pApp        = static_cast<genhintContext_t *>(arg);
		// Arguments are signature offsets
		const signature_t *pSignatureL = pApp->pStore->signatures + *(unsigned *) lhs;
		const signature_t *pSignatureR = pApp->pStore->signatures + *(unsigned *) rhs;
		const hint_t      *pHintL      = pApp->pStore->hints + pSignatureL->hintId;
		const hint_t      *pHintR      = pApp->pStore->hints + pSignatureR->hintId;

		int cmp;

		// first compare active index (lowest first)
		cmp = pHintL->numStored[pApp->activeHintIndex] - pHintR->numStored[pApp->activeHintIndex];
		if (cmp)
			return cmp;

		// then compare inactive indices (highest first)
		for (unsigned j = 0; j < hint_t::MAXENTRY; j++) {
			if (j != pApp->activeHintIndex) {
				cmp = pHintR->numStored[j] - pHintL->numStored[j];
				if (cmp)
					return cmp;
			}
		}

		// identical
		return 0;
	}

	/**
	 * @date 2020-04-20 15:53:35
	 *
	 * Collect metrics for optimal `--interleave` usage.
	 * Intended for optimizing the imprint index when running jobs in parallel.
	 *
	 * Basically, Which interleave and imprint setting to use.
	 *
	 * An interleave setting has effect on reading and writing.
	 * Large datasets can be sliced into rows and columns.
	 * Rows are ranges of signature.
	 * Each row can be sliced into columns based on generator progress.
	 *
	 * Imprint index is a speed/storage tradeoff.
	 * Not all signatures have the same number of imprints,
	 * depends on the collisions that occur with the row/column algorithm for the assosiative index lookups.
	 *
	 * Instead of dividing the signatures into equally sized collection of rows for parallelism,
	 * For a given interleave:
	 *   - Re-sort the signatures with high collisions rate
	 *   - Select as many signatures that fit in available memory
	 *   - Process the collection in parallel
	 *   - Rebuild the imprint index based on empty or unsafe signatures
	 *   - Decide which interleave has highest speed advantage
	 *   - Jump to first step until all signatures have been processed.
	 *
	 * Interleave efficiency is expressed in the number of footprint compares.
	 * The overwhelming majority of associative lookups will result in a miss.
	 * The footprint lookups are the main bottleneck and it also kills the CPU cache.
	 *
	 * For imprint index writing/creation: The number of indexed signatures is `"memAvail / sizeof(imprint_t) / numStored"`.
	 * `"memAvail / sizeof(imprint_t)"` is the number of imprints that can fit in memory.
	 * `"numStored"` is the cumulative sum of imprints belonging to the selected and re-ordered signatures.
	 * If an interleave setting can only store 25% of the signatures in memory, then 4 rounds are needed to process all signatures.
	 * The above is not entirely true as the second round has different metric properties.
	 *
	 * For imprint index reading: The total number of compares is `"numRuntime * numRound"`.
	 * `"numRuntime"` is the worst-case number of compares per associative lookup and can be found in `"metricsInterleave[]"`.
	 *
	 * Interleave efficiency is the total number of compares per generator candidate.
	 */

	void analyseHints(void) {
		if (pStore->numHint < 2) {
			fprintf(stderr, "missing `hint` section\n");
			exit(1);
		}

		unsigned *pHintMap = (unsigned *) ctx.myAlloc("pHintMap", pStore->maxSignature, sizeof(*pHintMap));

		for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {

			// set active index for this round
			this->activeHintIndex = pInterleave - metricsInterleave;

			// fill map with offsets to signatures
			unsigned      numHint = 0;
			for (unsigned iSid    = 1; iSid < pStore->numSignature; iSid++) {
				const signature_t *pSignature = pStore->signatures + iSid;

				if (~pSignature->flags & signature_t::SIGMASK_SAFE)
					pHintMap[numHint++] = iSid;

			}

			// sort entries.
			qsort_r(pHintMap, numHint, sizeof(*pHintMap), comparHint, this);

			// count how many signatures fit in memory
			uint64_t      memLeft   = this->opt_analyse;
			unsigned      numSelect = 0;
			for (unsigned iHint     = 0; iHint < numHint; iHint++) {
				const signature_t *pSignature = pStore->signatures + pHintMap[iHint];
				const hint_t      *pHint      = pStore->hints + pSignature->hintId;

				// size imprints will use for signature
				unsigned sz = sizeof(imprint_t) * pHint->numStored[this->activeHintIndex];

				// will signature fit
				if (memLeft < sz)
					break;

				numSelect++;
				memLeft -= sz;
			}

			double   numRound   = (double) numHint / numSelect;
			unsigned numCompare = __builtin_ceil(numRound) * pInterleave->numRuntime;
			printf("ix=%u numHint=%u interleave=%u numSelect=%u numRound=%.1f numCompare=%u\n", this->activeHintIndex, numHint, metricsInterleave[this->activeHintIndex].numStored, numSelect, numRound, numCompare);
		}

		ctx.myFree("pSignatureIndex", pHintMap);
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
 * @global {genhintContext_t} Application context
 */
genhintContext_t app(ctx);

/**
 * @date 2020-04-18 21:25:33
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
 * @date 2020-04-18 21:30:30
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
 * @date 2020-03-14 11:17:04
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
		fprintf(stderr, "\t   --analyse=<number>         Analyise input database for given amount of memory\n");
		fprintf(stderr, "\t   --force                    Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate            Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                     This list\n");
		fprintf(stderr, "\t   --hintindexsize=<number>   Size of hint index [default=%u]\n", app.opt_hintIndexSize);
		fprintf(stderr, "\t   --maxhint=<number>         Maximum number of hints [default=%u]\n", app.opt_maxHint);
		fprintf(stderr, "\t   --[no-]paranoid            Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                    Say less\n");
		fprintf(stderr, "\t   --[no-]saveindex           Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --sid=[<low>],<high>       Sid range upper bound [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --task=sge                 Get sid task settings from SGE environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>         Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                     Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>          Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --[no-]unsafe              Reindex imprints based on empty/unsafe signature groups [default=%s]\n", (ctx.flags & context_t::MAGICMASK_UNSAFE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-v --verbose                  Say more\n");
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
int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_ANALYSE = 1,
			LO_DEBUG,
			LO_FORCE,
			LO_GENERATE,
			LO_HINTINDEXSIZE,
			LO_LOAD,
			LO_MAXHINT,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_NOUNSAFE,
			LO_PARANOID,
			LO_PURE,
			LO_SAVEINDEX,
			LO_SID,
			LO_TASK,
			LO_TEXT,
			LO_TIMER,
			LO_UNSAFE,
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"analyse",       1, 0, LO_ANALYSE},
			{"debug",         1, 0, LO_DEBUG},
			{"force",         0, 0, LO_FORCE},
			{"generate",      0, 0, LO_GENERATE},
			{"help",          0, 0, LO_HELP},
			{"hintindexsize", 1, 0, LO_HINTINDEXSIZE},
			{"load",          1, 0, LO_LOAD},
			{"maxhint",       1, 0, LO_MAXHINT},
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
			{"task",          1, 0, LO_TASK},
			{"text",          2, 0, LO_TEXT},
			{"timer",         1, 0, LO_TIMER},
			{"unsafe",        0, 0, LO_UNSAFE},
			{"verbose",       2, 0, LO_VERBOSE},
			//
			{NULL,            0, 0, 0}
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
		*cp        = '\0';

		// parse long options
		int option_index = 0;
		int c            = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_ANALYSE:
			app.opt_analyse = ctx.dToMax(::strtod(optarg, NULL));
			break;
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
		case LO_HINTINDEXSIZE:
			app.opt_hintIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_LOAD:
			app.opt_load = optarg;
			break;
		case LO_MAXHINT:
			app.opt_maxHint = ctx.nextPrime(::strtod(optarg, NULL));
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
			app.opt_saveIndex = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_saveIndex + 1;
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
		case LO_UNSAFE:
			ctx.flags |= context_t::MAGICMASK_UNSAFE;
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

	app.inheritSections &= ~(database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_HINT | database_t::ALLOCMASK_HINTINDEX);
	// will require local copy of signatures
	app.rebuildSections |= database_t::ALLOCMASK_SIGNATURE;

	// sync signatures to input
	app.opt_maxSignature = db.numSignature;

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, 0); // numNodes is only needed for defaults that do not occur

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
	 * Where to look for new candidates
	 */

	if (app.opt_analyse) {
		app.analyseHints();
		exit(0);
	}

	// if input is empty, skip reserved entries
	if (!app.readOnlyMode) {
		assert(store.numHint > 0);
	}

	if (app.opt_load)
		app.hintsFromFile();
	if (app.opt_generate) {
		/*
		 * Create worker database to count imprints.
		 * Use separate db as to not to interfere with real imprints
		 */
		database_t tempdb(ctx);
		tempdb.maxHint          = 0;
		tempdb.hintIndexSize    = ctx.nextPrime(tempdb.maxHint * app.opt_ratio);
		tempdb.maxImprint       = MAXTRANSFORM + 1;
		tempdb.imprintIndexSize = ctx.nextPrime(tempdb.maxImprint * app.opt_ratio);
		tempdb.create(0);
		tempdb.enableVersioned();

		app.hintsFromSignature(tempdb);
	}

	/*
	 * List result
	 */

	if (app.opt_text == app.OPTTEXT_BRIEF) {
		// also output 'empty' hints to easy track what is missing

		for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
			const signature_t *pSignature = store.signatures + iSid;
			const hint_t      *pHint      = store.hints + pSignature->hintId;

			printf("%s\t", pSignature->name);

			for (unsigned j = 0; j < hint_t::MAXENTRY && pHint->numStored[j]; j++)
				printf("\t%u", pHint->numStored[j]);

			printf("\n");
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
