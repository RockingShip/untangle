//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2021-07-16 12:50:46
 *
 * `genimport` does a fast load of a database based on the outpt created by `genexport`
 * 
 * It combines the functionality of `gentransform`, `genimport`, `genswap`, `genmember`, `gendepreciate`. 
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2021, xyzzy@rockingship.org
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
#include <unistd.h>

#include "config.h"
#include "genport.h"
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
 * @global {genimportContext_t} Application context
 */
genportContext_t app(ctx);

/**
 * @date 2020-03-11 23:06:35
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int __attribute__ ((unused)) sig) {
	if (app.arg_databaseName) {
		remove(app.arg_databaseName);
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
	fprintf(stderr, "usage: %s <output.db> <input.json>\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>           Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --maxpair=<number>              Maximum number of sid/mid/tid pairs [default=%u]\n", app.opt_maxPair);
		fprintf(stderr, "\t   --pairindexsize=<number>        Size of pair index [default=%u]\n", app.opt_pairIndexSize);
		fprintf(stderr, "\t   --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --[no-]saveindex                Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --swapindexsize=<number>        Size of swap index [default=%u]\n", app.opt_swapIndexSize);
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --truncate                      Truncate on database overflow\n");
		fprintf(stderr, "\t-v --verbose                       Say more\n");
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
			LO_DEBUG = 0,
			LO_FORCE,
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_MAXPAIR,
			LO_MEMBERINDEXSIZE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_NOSORT,
			LO_PAIRINDEXSIZE,
			LO_PARANOID,
			LO_PURE,
			LO_RATIO,
			LO_SAVEINDEX,
			LO_SIGNATUREINDEXSIZE,
			LO_SWAPINDEXSIZE,
			LO_TIMER,
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",              1, 0, LO_DEBUG},
			{"force",              0, 0, LO_FORCE},
			{"help",               0, 0, LO_HELP},
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",         1, 0, LO_INTERLEAVE},
			{"maxpair",            1, 0, LO_MAXPAIR},
			{"memberindexsize",    1, 0, LO_MEMBERINDEXSIZE},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-pure",            0, 0, LO_NOPURE},
			{"no-saveindex",       0, 0, LO_NOSAVEINDEX},
			{"no-sort",            0, 0, LO_NOSORT},
			{"pairindexsize",      1, 0, LO_PAIRINDEXSIZE},
			{"paranoid",           0, 0, LO_PARANOID},
			{"pure",               0, 0, LO_PURE},
			{"quiet",              2, 0, LO_QUIET},
			{"ratio",              1, 0, LO_RATIO},
			{"saveindex",          0, 0, LO_SAVEINDEX},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"swapindexsize",      1, 0, LO_SWAPINDEXSIZE},
			{"timer",              1, 0, LO_TIMER},
			{"verbose",            2, 0, LO_VERBOSE},
			//
			{NULL,                 0, 0, 0}
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
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_IMPRINTINDEXSIZE:
			app.opt_imprintIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_INTERLEAVE:
			app.opt_interleave = ::strtoul(optarg, NULL, 0);
			if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
				ctx.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
			break;
		case LO_MAXPAIR:
			app.opt_maxPair = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MEMBERINDEXSIZE:
			app.opt_memberIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_PAIRINDEXSIZE:
			app.opt_pairIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
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
		case LO_RATIO:
			app.opt_ratio = strtof(optarg, NULL);
			break;
		case LO_SAVEINDEX:
			app.opt_saveIndex = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_saveIndex + 1;
			break;
		case LO_SIGNATUREINDEXSIZE:
			app.opt_signatureIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_SWAPINDEXSIZE:
			app.opt_swapIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
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
		app.arg_databaseName = argv[optind++];

	if (argc - optind >= 1)
		app.arg_jsonName = argv[optind++];

	if (app.arg_jsonName == NULL) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */

	if (app.arg_databaseName && !app.opt_force) {
		struct stat sbuf;

		if (!stat(app.arg_databaseName, &sbuf)) {
			fprintf(stderr, "%s already exists. Use --force to overwrite\n", app.arg_databaseName);
			exit(1);
		}
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Load json
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Loading \"%s\"\n", ctx.timeAsString(), app.arg_jsonName);
	}

	// load json
	FILE *f = fopen(app.arg_jsonName, "r");

	if (!f) {
		json_t *jError = json_object();
		json_object_set_new_nocheck(jError, "error", json_string_nocheck("fopen()"));
		json_object_set_new_nocheck(jError, "filename", json_string(app.arg_jsonName));
		json_object_set_new_nocheck(jError, "errno", json_integer(errno));
		json_object_set_new_nocheck(jError, "errtxt", json_string(strerror(errno)));
		printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		exit(1);
	}

	json_error_t jLoadError;
	json_t       *jInput = json_loadf(f, 0, &jLoadError);
	if (jInput == 0) {
		json_t *jError = json_object();
		json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to decode json"));
		json_object_set_new_nocheck(jError, "filename", json_string(app.arg_jsonName));
		json_object_set_new_nocheck(jError, "line", json_integer(jLoadError.line));
		json_object_set_new_nocheck(jError, "text", json_string(jLoadError.text));
		printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		exit(1);
	}
	fclose(f);

	/*
	 * Open input and create output database
	 */

	database_t store(ctx);

	// set section sizes to be created
	store.maxTransform       = MAXTRANSFORM;
	store.transformIndexSize = MAXTRANSFORMINDEX;
	store.maxEvaluator       = tinyTree_t::TINYTREE_NEND * MAXTRANSFORM;

	app.opt_maxSignature     = json_integer_value(json_object_get(jInput, "maxSignature"));
	app.opt_maxSwap          = json_integer_value(json_object_get(jInput, "maxSwap"));
	app.opt_maxImprint       = json_integer_value(json_object_get(jInput, "maxImprint"));
	app.opt_maxPair          = json_integer_value(json_object_get(jInput, "maxPair"));
	app.opt_maxMember        = json_integer_value(json_object_get(jInput, "maxMember"));
	store.signatureIndexSize = json_integer_value(json_object_get(jInput, "signatureIndexSize"));
	store.swapIndexSize      = json_integer_value(json_object_get(jInput, "swapIndexSize"));
	store.imprintIndexSize   = json_integer_value(json_object_get(jInput, "imprintIndexSize"));
	store.pairIndexSize      = json_integer_value(json_object_get(jInput, "pairIndexSize"));
	store.memberIndexSize    = json_integer_value(json_object_get(jInput, "memberIndexSize"));

	store.interleave         = json_integer_value(json_object_get(jInput, "interleave"));

	ctx.flags		 = ctx.flagsFromJson(json_object_get(jInput, "flags"));

	// find matching `interleaveStep`
	const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, store.interleave);
	assert(pMetrics);
	store.interleaveStep = pMetrics->interleaveStep;

	// either use preset values or override with commandline
	app.inheritSections = 0; // can not inherit from self
	app.sizeDatabaseSections(store, store, 0, false /*autoSize*/);

	// create memory-based store
	store.create(0);

	app.pStore   = &store;

	// display system flags when database was created
	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
		fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(store.creationFlags).c_str());
	}

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(store.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));

	/*
	 * Finalise allocations and create database
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Allocated %.3fG memory. freeMemory=%.3fG.\n", ctx.timeAsString(), ctx.totalAllocated / 1e9, info.freeram / 1e9);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		// Assuming with database allocations included
		size_t allocated = ctx.totalAllocated + store.estimateMemoryUsage(0);

		struct sysinfo info;
		if (sysinfo(&info) == 0) {
			double percent = 100.0 * allocated / info.freeram;
			if (percent > 80)
				fprintf(stderr, "WARNING: using %.1f%% of free memory minus cache\n", percent);
		}
	}

	/*
	 * Create transforms/evaluator
	 */
	{
		gentransformContext_t appTransform(ctx);

		appTransform.pStore = &store;
		appTransform.main();
		app.pStore->initialiseEvaluators();
	}

	/*
	 * Load signatures
	 */

	{
		gensignatureContext_t appSignature(ctx);

		app.signaturesFromJson(jInput, appSignature);

		appSignature.pStore = &store;
		appSignature.rebuildImprints();
	}

	/*
	 * Load swaps
	 */

	{
		genswapContext_t appSwap(ctx);

		appSwap.pStore = &store;
		app.swapsFromJson(jInput, appSwap);
	}

	/*
	 * Load members and create pair intermediates
	 */

	{
		genmemberContext_t appMember(ctx);

		appMember.pStore = &store;
		app.membersFromJson(jInput, appMember);
	}

	/*
	 * Saving
	 */

	if (app.arg_databaseName) {
		if (!app.opt_saveIndex) {
			store.signatureIndexSize = 0;
			store.imprintIndexSize   = 0;
			store.numImprint         = 0;
			store.interleave         = 0;
			store.interleaveStep     = 0;
			store.pairIndexSize      = 0;
			store.memberIndexSize    = 0;
		}

		/*
		 * Save the database
		 */

		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_databaseName);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		json_t *jResult = json_object();
		json_object_set_new_nocheck(jResult, "done", json_string_nocheck(argv[0]));
		if (app.arg_databaseName)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_databaseName));
		store.jsonInfo(jResult);
		fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}

	return 0;
}
