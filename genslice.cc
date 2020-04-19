//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-04-18 20:46:40
 *
 * `genslice` creates imprint metrics to hint slice information for job parallelism.
 *
 * This is done by selecting signatures that have imprints with high collision rates.
 * Collisions reduce the index storage.
 * High speed imprint index settings require a lot of storage.
 *
 * Nothing is more frustrating than during a multi hour tuning run having the imprint table overflow.
 *
 * Imprint metrics are non-linear and difficult to predict.
 * `genslice` counts how many imprints a signature actually creates for different interleave settings.
 * This is a very slow process and can take 17 hours with a single job.
 *
 * To create the slices for `imprints.lst`
 * ./genslice --sidhi= --sidlo= --task=m,n <intput.db> -- read section from input and tally `--interleave`
 * ./genslice next..
 * Throughput is around 150k/h (about 6 hours).
 *
 * To give advice ./genslice <input,db> --load=imprints.lst --maxmem=30G
 * ./genmember --stdin | add to members | create full index
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
#include <errno.h>
#include "tinytree.h"
#include "database.h"
#include "generator.h"
#include "restartdata.h"
#include "metrics.h"

/**
 * @date 2020-04-18 21:21:22
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct gensliceContext_t : callable_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {copntext_t} I/O context
	context_t &ctx;

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} Tree size in nodes to be generated for this invocation;
	unsigned arg_numNodes;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} size of imprint index WARNING: must be prime
	uint32_t opt_imprintIndexSize;
	/// @var {number} Maximum number of imprints to be stored database
	uint32_t opt_maxImprint;
	/// @var {number} index/data ratio
	double opt_ratio;
	/// @var {number} Sid range upper bound
	uint32_t opt_sidHi;
	/// @var {number} Sid range lower bound
	uint32_t opt_sidLo;
	/// @var {number} task Id. First task=1
	unsigned opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;

	/// @var {database_t} - Database store to place results
	database_t *pStore;
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;

	/**
	 * Constructor
	 */
	gensliceContext_t(context_t &ctx) : ctx(ctx) {
		// arguments and options
		opt_imprintIndexSize = 0;
		arg_inputDatabase = NULL;
		opt_maxImprint = 0;
		arg_numNodes = 0;
		arg_outputDatabase = NULL;
		opt_ratio = METRICS_DEFAULT_RATIO / 10.0;
		opt_sidHi = 0;
		opt_sidLo = 0;
		opt_taskId = 0;
		opt_taskLast = 0;
		opt_text = 0;

		pStore = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;
	}

	/**
	 * @date 2020-04-17 23:30:39
	 *
	 * Nothing is more frustrating than during a multi hour tuning run having the imprint table overflow.
	 *
	 * Imprint metrics are non-linear and difficult to predict.
	 * The only practical solution is to actually count them and store them in a separate table.
	 * This allows precise memory usage calculations when using windows or high-usage settings.
	 */
	void main(void) {

		tinyTree_t tree(ctx);

		// enable versioned memory or imprint index
		pStore->enabledVersioned();

		// set bounds
		assert(this->opt_sidHi == 0 || this->opt_sidHi <= pStore->numSignature);

		if (this->opt_sidLo < 1)
			this->opt_sidLo = 1;
		if (this->opt_sidHi == 0)
			this->opt_sidHi = pStore->numSignature;

		// reset progress
		ctx.setupSpeed(opt_sidHi - opt_sidLo);
		ctx.tick = 0;

		// create imprints for signature groups
		for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {

			if ((opt_sidLo && iSid < opt_sidLo) || (opt_sidHi && iSid >= opt_sidHi))
				continue;

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				ctx.tick = 0;
				int perSecond = ctx.updateSpeed();

				if (perSecond == 0 || ctx.progress > ctx.progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s)",
					        ctx.timeAsString(), ctx.progress, perSecond);
				} else {
					int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %u(%7d/s) %.5f%% eta=%d:%02d:%02d",
					        ctx.timeAsString(), iSid, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS);
				}
			}

			const signature_t *pSignature = pStore->signatures + iSid;

			printf("%6u %30s ", iSid, pSignature->name);

			for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {

				if (pInterleave->noauto)
					continue;

				// prepare database
				pStore->InvalidateVersioned();
				pStore->numImprint = 1; // skip reserved first entry
				pStore->interleave = pInterleave->numStored;
				pStore->interleaveStep = pInterleave->interleaveStep;

				/*
				 * Add imprint
				 */

				tree.decodeFast(pSignature->name);
				pStore->addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);

				/*
				 * Count
				 */

				printf("%6u ", pStore->numImprint - 1);
			}
			printf("\n");

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (this->opt_taskLast)
			fprintf(stderr, "{\"done\":\"genslice\",\"taskId\":%u,\"taskHi\":%u,\"sidLo\":%u,\"sidHi\":%u}\n", this->opt_taskId, this->opt_taskLast, this->opt_sidLo, this->opt_sidHi);
		else
			fprintf(stderr, "{\"done\":\"genslice\",\"sidLo\":%u,\"sidHi\":%u}\n", this->opt_sidLo, this->opt_sidHi);
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
gensliceContext_t app(ctx);

/**
 * @date 2020-04-18 21:25:33
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
 * @date 2020-04-18 21:30:30
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
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                   Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help                    This list\n");
		fprintf(stderr, "\t-q --quiet                   Say more\n");
		fprintf(stderr, "\t   --sge                     Get SGE task settings from environment\n");
		fprintf(stderr, "\t   --sid=[<lo>],<hi>         Sid range upper bound [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --task=<id>,<last>        Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                    Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>         Interval timer for verbose updates [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose                 Say less\n");
	}
}

/**
 * pure2020-03-14 11:19:40
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
			LO_SGE,
			LO_SID,
			LO_TASK,
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
			{"debug",   1, 0, LO_DEBUG},
			{"force",   0, 0, LO_FORCE},
			{"help",    0, 0, LO_HELP},
			{"quiet",   2, 0, LO_QUIET},
			{"sge",     0, 0, LO_SGE},
			{"sid",     1, 0, LO_SID},
			{"task",    1, 0, LO_TASK},
			{"text",    2, 0, LO_TEXT},
			{"timer",   1, 0, LO_TIMER},
			{"verbose", 2, 0, LO_VERBOSE},
			//
			{NULL,      0, 0, 0}
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
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_SGE: {
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

				if (app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "task id exceeds last\n");
					exit(1);
				}

				break;
			}
			case LO_SID: {
				unsigned m, n;

				int ret = sscanf(optarg, "%u,%u", &m, &n);
				printf("ret=%d\n", ret);
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
			case LO_TASK:
				if (sscanf(optarg, "%u,%u", &app.opt_taskId, &app.opt_taskLast) != 2) {
					usage(argv, true);
					exit(1);
				}
				if (app.opt_taskId == 0 || app.opt_taskLast == 0) {
					fprintf(stderr, "--task id/last must be non-zero\n");
					exit(1);
				}
				if (app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "--task id exceeds last\n");
					exit(1);
				}
				break;
			case LO_TEXT:
				app.opt_text = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_text + 1;
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
	if (argc - optind >= 1)
		app.arg_inputDatabase = argv[optind++];

	if (argc - optind >= 1) {
		char *endptr;

		errno = 0; // To distinguish success/failure after call
		app.arg_numNodes = (uint32_t) strtoul(argv[optind++], &endptr, 0);

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

	db.open(app.arg_inputDatabase, true);

	// display system flags when database was created
	if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		ctx.logFlags(db.creationFlags);
#if defined(ENABLE_JANSSON)
	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	/*
	 * Create output database
	 */

	database_t store(ctx);

	/*
	 * Set defaults
	 */

	// Signatures are always copied as they need modifiable `firstMember`
	store.maxSignature = db.maxSignature;
	store.signatureIndexSize = db.signatureIndexSize;

	if (app.opt_maxImprint == 0) {
		store.maxImprint = MAXTRANSFORM;
	} else {
		store.maxImprint = app.opt_maxImprint;
	}

	if (app.opt_imprintIndexSize == 0)
		store.imprintIndexSize = ctx.nextPrime(store.maxImprint * app.opt_ratio);
	else
		store.imprintIndexSize = app.opt_imprintIndexSize;

	// create new sections
	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] Store create: maxImprint=%u maxSignature=%u\n", ctx.timeAsString(), store.maxImprint, store.maxSignature);

	// actual create
	store.create();
	app.pStore = &store;

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);
	if (ctx.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", ctx.totalAllocated);

	// apply settings for `--task`
	if (app.opt_taskLast) {
		// split progress into chunks
		uint64_t taskSize = db.numSignature / app.opt_taskLast;
		if (taskSize == 0)
			taskSize = 1;

		app.opt_sidLo = taskSize * (app.opt_taskId - 1);
		app.opt_sidHi = taskSize * app.opt_taskId;

		if (app.opt_taskId == app.opt_taskLast)
			app.opt_sidHi = db.numSignature;
	}

	if (app.opt_sidLo || app.opt_sidHi) {
		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Sid window: %u-%u\n", ctx.timeAsString(), app.opt_sidLo, app.opt_sidHi ? app.opt_sidHi : db.numSignature);
	}

	/*
	 * Copy/inherit sections
	 */

	// templates are always inherited
	store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_TRANSFORM);

	// signatures are always modifyable
	if (store.allocFlags & database_t::ALLOCMASK_SIGNATURE) {
		// data
		assert(store.maxSignature >= db.numSignature);
		::memcpy(store.signatures, db.signatures, db.numSignature * sizeof(*store.signatures));
		store.numSignature = db.numSignature;
		// index
		assert (store.signatureIndexSize == db.signatureIndexSize);
		::memcpy(store.signatureIndex, db.signatureIndex, db.signatureIndexSize * sizeof(*store.signatureIndex));
	}

	/*
	 * initialise evaluators
	 */

	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, store.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, store.revTransformData);

	/*
	 * Invoke main entrypoint of application context
	 */
	app.main();

	return 0;
}
