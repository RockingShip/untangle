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
 * - Compare and reject the worst of the two
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

#include "config.h"
#include "genswap.h"
#include "metrics.h"

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

	db.open(app.arg_inputDatabase);

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

	if (db.numTransform == 0)
		ctx.fatal("Missing transform section: %s\n", app.arg_inputDatabase);
	if (db.numEvaluator == 0)
		ctx.fatal("Missing evaluator section: %s\n", app.arg_inputDatabase);
	if (db.numSignature == 0)
		ctx.fatal("Missing signature section: %s\n", app.arg_inputDatabase);

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, 0, !app.readOnlyMode); // numNodes is only needed for defaults that should not occur

	/*
	 * Finalise allocations and create database
	 */

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

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS && !(app.rebuildSections & ~app.inheritSections)) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Allocated %.3fG memory. freeMemory=%.3fG.\n", ctx.timeAsString(), ctx.totalAllocated / 1e9, info.freeram / 1e9);
	}

	/*
	 * Inherit/copy sections
	 */

	app.populateDatabaseSections(store, db);

	/*
	 * Rebuild sections
	 */

	// todo: move this to `populateDatabaseSections()`
	// data sections cannot be automatically rebuilt
	assert((app.rebuildSections & (database_t::ALLOCMASK_SWAP | database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_MEMBER)) == 0);

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
			if (!(found & (1 << (pName[j] - 'a')))) {
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
			store.imprintIndexSize   = 0;
			store.numImprint         = 0;
			store.interleave         = 0;
			store.interleaveStep     = 0;
			store.pairIndexSize      = 0;
			store.memberIndexSize    = 0;
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
