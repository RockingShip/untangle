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
 * To give advice ./genhint <input,db> --load=imprints.lst --maxmem=30G
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

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned opt_generate;
	/// @var {string} name of file containing interleave hints
	const char *opt_load;
	/// @var {number} Sid range upper bound
	unsigned opt_sidHi;
	/// @var {number} Sid range lower bound
	unsigned opt_sidLo;
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
	genhintContext_t(context_t &ctx) : dbtool_t(ctx) {
		// arguments and options
		opt_force = 0;
		opt_generate = 1;
		arg_inputDatabase = NULL;
		opt_load = NULL;
		arg_outputDatabase = NULL;
		opt_sidHi = 0;
		opt_sidLo = 0;
		opt_taskId = 0;
		opt_taskLast = 0;
		opt_text = 0;

		pStore = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;

		opt_maxHint = 255; // for 4n9 there are 250 hints
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

		FILE *f = fopen(this->opt_load, "r");
		if (f == NULL)
			ctx.fatal("{\"error\":\"fopen() failed\",\"where\":\"%s\",\"name\":\"%s\",\"reason\":\"%m\"}\n",
			          __FUNCTION__, this->opt_load);

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;

		char name[64];
		hint_t hint;

		assert(MAXSLOTS * 2 == 18);

		// <name> <hint0> <hint1> ...
		for (;;) {
			::memset(&hint, 0, sizeof(hint));

			if (fscanf(f, "%s %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u\n",
			           name,
			           &hint.numStored[0], &hint.numStored[1], &hint.numStored[2], &hint.numStored[3],
			           &hint.numStored[4], &hint.numStored[5], &hint.numStored[6], &hint.numStored[7],
			           &hint.numStored[8], &hint.numStored[9], &hint.numStored[10], &hint.numStored[11],
			           &hint.numStored[12], &hint.numStored[13], &hint.numStored[14], &hint.numStored[15]) != 17)
				break;

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numHint=%u(%.0f%%)",
				        ctx.timeAsString(), ctx.progress, perSecond,
				        pStore->numHint, pStore->numHint * 100.0 / pStore->maxHint);

				ctx.tick = 0;
			}

			/*
			 * add to database
			 */

			// lookup signature
			unsigned ix = pStore->lookupSignature(name);
			unsigned sid = pStore->signatureIndex[ix];
			if (sid == 0) {
				printf("{\"error\":\"missing signature\",\"where\":\"%s\",\"expected\":\"%s\",\"progress\":%lu}\n",
				       __FUNCTION__, name, ctx.progress);
				exit(1);
			}

			// lookup/add hintId
			ix = pStore->lookupHint(&hint);
			unsigned hintId = pStore->hintIndex[ix];
			if (hintId == 0)
				pStore->hintIndex[ix] = hintId = pStore->addHint(&hint);

			// add hintId to signature
			pStore->signatures[sid].hintId = hintId;

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "[%s] Read members. numSignature=%u(%.0f%%) numHint=%u(%.0f%%)\n",
			        ctx.timeAsString(),
			        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
			        pStore->numHint, pStore->numHint * 100.0 / pStore->maxHint);
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
	void hintsFromGenerator(void) {

		tinyTree_t tree(ctx);

		// enable versioned memory or imprint index
		pStore->enableVersioned();

		if (this->opt_sidLo < 1)
			this->opt_sidLo = 1;
		if (this->opt_sidHi == 0)
			this->opt_sidHi = pStore->numSignature;

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			if (this->opt_sidLo || this->opt_sidHi) {
				if (this->opt_taskLast)
					fprintf(stderr, "[%s] Window: task=%u,%u sid=%u-%u\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_sidLo, this->opt_sidHi);
				else
					fprintf(stderr, "[%s] Window: Sid window: %u-%u\n", ctx.timeAsString(), this->opt_sidLo, this->opt_sidHi);
			}
		}

		// reset ticker
		ctx.setupSpeed(opt_sidHi - opt_sidLo);
		ctx.tick = 0;

		// create imprints for signature groups
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {

			if ((opt_sidLo && iSid < opt_sidLo) || (opt_sidHi && iSid >= opt_sidHi))
				continue;

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
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

				ctx.tick = 0;
			}

			signature_t *pSignature = pStore->signatures + iSid;
			hint_t hint;

			::memset(&hint, 0, sizeof(hint));

			if (this->opt_text)
				printf("%s", pSignature->name);

			for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {

#if 0
				// Writing imprints with lower interleave settings is fast and makes the data complete
				if (pInterleave->noauto)
					continue;
#endif

				// prepare database
				pStore->InvalidateVersioned();
				pStore->numImprint = 1; // skip reserved first entry
				pStore->interleave = pInterleave->numStored;
				pStore->interleaveStep = pInterleave->interleaveStep;

				// add imprint
				tree.decodeFast(pSignature->name);
				pStore->addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);

				// output counte
				hint.numStored[pInterleave - metricsInterleave] = pStore->numImprint - 1;

				if (this->opt_text)
					printf("\t%u", pStore->numImprint - 1);
			}
			if (this->opt_text)
				printf("\n");

			// add to database
			if (this->arg_outputDatabase) {
				// lookup/add hintId
				unsigned ix = pStore->lookupHint(&hint);
				unsigned hintId = pStore->hintIndex[ix];
				if (hintId == 0)
					pStore->hintIndex[ix] = hintId = pStore->addHint(&hint);

				// add hintId to signature
				pSignature->hintId = hintId;
			}

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Done\n", ctx.timeAsString());
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
	fprintf(stderr, "usage: %s <input.db> [<output.db>]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                    Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate            Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                     This list\n");
		fprintf(stderr, "\t   --hintindexsize=<number>   Size of hint index [default=%u]\n", app.opt_hintIndexSize);
		fprintf(stderr, "\t   --maxhint=<number>         Maximum number of hints [default=%u]\n", app.opt_maxHint);
		fprintf(stderr, "\t   --[no-]paranoid            Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                    Say more\n");
		fprintf(stderr, "\t   --sge                      Get SGE task settings from environment\n");
		fprintf(stderr, "\t   --sid=[<low>],<high>       Sid range upper bound [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --task=<id>,<last>         Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                     Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>          Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --[no-]unsafe                 Reindex imprints based onempty/unsafe signature groups [default=%s]\n", (ctx.flags & context_t::MAGICMASK_UNSAFE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-v --verbose                  Say less\n");
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
			LO_GENERATE,
			LO_HINTINDEXSIZE,
			LO_LOAD,
			LO_MAXHINT,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOUNSAFE,
			LO_PARANOID,
			LO_PURE,
			LO_SGE,
			LO_SID,
			LO_TASK,
			LO_TEXT,
			LO_TIMER,
			LO_UNSAFE,
			// short opts
			LO_HELP = 'h',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
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
			{"no-unsafe",     0, 0, LO_NOUNSAFE},
			{"quiet",         2, 0, LO_QUIET},
			{"sge",           0, 0, LO_SGE},
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
		*cp = '\0';

		// parse long options
		int option_index = 0;
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
				ctx.opt_timer = 60; // set ticker to 1-minute interval.
				break;
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
				fprintf(stderr, "getopt returned character code %d\n", c);
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

#if defined(ENABLE_JANSSON)
	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	/*
	 * Create output database
	 */

	database_t store(ctx);

	// which sections are primary goal and need to be writable to be collect or sort
	if (app.arg_outputDatabase != NULL || app.opt_text == 3 || app.opt_text == 4)
		app.primarySections = database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_HINT;
	else
		app.primarySections = 0;

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, 0); // numNodes is only needed for defaults that do not occur

	// determine if sections are rebuild, inherited or copied (copy-on-write)
	app.modeDatabaseSections(store, db);

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

		fprintf(stderr, "[%s] Allocated %lu memory. freeMemory=%lu.\n", ctx.timeAsString(), ctx.totalAllocated, info.freeram);
	}

	/*
	 * Inherit/copy sections
	 */

	app.populateDatabaseSections(store, db);

	// initialize evaluator
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, store.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, store.revTransformData);

	/*
	 * Rebuild sections
	 */

	assert(app.rebuildSections == 0 || (~app.rebuildSections & database_t::ALLOCMASK_IMPRINT));

	if (app.rebuildSections)
		store.rebuildIndices(app.rebuildSections);

	/*
	 * Where to look for new candidates
	 */

	if (app.opt_load)
		app.hintsFromFile();
	if (app.opt_generate)
		app.hintsFromGenerator();

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
		fprintf(stderr, "{\"done\":\"%s\",\"taskId\":%u,\"taskLast\":%u,\"sidLo\":%u,\"sidHi\":%u}\n", argv[0], app.opt_taskId, app.opt_taskLast, app.opt_sidLo, app.opt_sidHi);
	else if (app.opt_sidLo || app.opt_sidHi)
		fprintf(stderr, "{\"done\":\"%s\",\"sidLo\":%u,\"sidHi\":%u}\n", argv[0], app.opt_sidLo, app.opt_sidHi);
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
