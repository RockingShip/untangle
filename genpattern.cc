//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2021-10-17 17:53:40
 *
 * Generate detector patterns and populate patternFirst/patternSecond tables
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

#include "config.h"
#include "genpattern.h"
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
 * @global {genpatternContext_t} Application context
 */
genpatternContext_t app(ctx);

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
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --cascade                       Apply cascade normalisation\n");
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --maxfirst=<number> [default=%u]\n", app.opt_maxPatternFirst);
		fprintf(stderr, "\t   --firstindex=<number> [default=%u]\n", app.opt_patternFirstIndexSize);
		fprintf(stderr, "\t   --maxsecond=<number> [default=%u]\n", app.opt_maxPatternSecond);
		fprintf(stderr, "\t   --secondindex=<number> [default=%u]\n", app.opt_patternSecondIndexSize);
		fprintf(stderr, "\t   --mixed                         Consider top-level mixed members only\n");
		fprintf(stderr, "\t   --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --safe                          Consider safe members only\n");
		fprintf(stderr, "\t   --[no-]saveindex                Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --sid=[<low>,]<high>            Sid range upper bound  [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --task=sge                      Get task settings from SGE environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>              Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                          Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --truncate                      Truncate on database overflow\n");
		fprintf(stderr, "\t-v --verbose                       Say more\n");
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
int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_CASCADE = 1,
			LO_DEBUG,
			LO_FORCE,
			LO_GENERATE,
			LO_LOAD,
			LO_MAXPATTERNFIRST,
			LO_MAXPATTERNSECOND,
			LO_MIXED,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_PARANOID,
			LO_PATTERNFIRSTINDEXSIZE,
			LO_PATTERNSECONDINDEXSIZE,
			LO_PURE,
			LO_RATIO,
			LO_SAFE,
			LO_SAVEINDEX,
			LO_SID,
			LO_TASK,
			LO_TEXT,
			LO_TIMER,
			LO_TRUNCATE,
			LO_WINDOW,
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"cascade",      0, 0, LO_CASCADE},
			{"debug",        1, 0, LO_DEBUG},
			{"force",        0, 0, LO_FORCE},
			{"generate",     0, 0, LO_GENERATE},
			{"help",         0, 0, LO_HELP},
			{"load",         1, 0, LO_LOAD},
			{"maxfirst",     1, 0, LO_MAXPATTERNFIRST},
			{"maxsecond",    1, 0, LO_MAXPATTERNSECOND},
			{"firstindex",   1, 0, LO_PATTERNFIRSTINDEXSIZE},
			{"secondindex",  1, 0, LO_PATTERNSECONDINDEXSIZE},
			{"mixed",        0, 0, LO_MIXED},
			{"no-generate",  0, 0, LO_NOGENERATE},
			{"no-paranoid",  0, 0, LO_NOPARANOID},
			{"no-pure",      0, 0, LO_NOPURE},
			{"no-saveindex", 0, 0, LO_NOSAVEINDEX},
			{"paranoid",     0, 0, LO_PARANOID},
			{"pure",         0, 0, LO_PURE},
			{"quiet",        2, 0, LO_QUIET},
			{"ratio",        1, 0, LO_RATIO},
			{"safe",         0, 0, LO_SAFE},
			{"saveindex",    0, 0, LO_SAVEINDEX},
			{"sid",          1, 0, LO_SID},
			{"task",         1, 0, LO_TASK},
			{"text",         2, 0, LO_TEXT},
			{"timer",        1, 0, LO_TIMER},
			{"truncate",     0, 0, LO_TRUNCATE},
			{"verbose",      2, 0, LO_VERBOSE},
			{"window",       1, 0, LO_WINDOW},
			//
			{NULL,           0, 0, 0}
		};

		char optstring[64];
		char *cp                            = optstring;
		int  option_index                   = 0;

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
		case LO_CASCADE:
			ctx.flags |= context_t::MAGICMASK_CASCADE;
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
		case LO_LOAD:
			app.opt_load = optarg;
			break;
		case LO_MAXPATTERNFIRST:
			app.opt_maxPatternFirst = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_PATTERNFIRSTINDEXSIZE:
			app.opt_patternFirstIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_MAXPATTERNSECOND:
			app.opt_maxPatternSecond = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_PATTERNSECONDINDEXSIZE:
			app.opt_patternSecondIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
			break;
		case LO_MIXED:
			app.opt_mixed++;
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
		case LO_RATIO:
			app.opt_ratio = strtof(optarg, NULL);
			break;
		case LO_SAFE:
			app.opt_safe++;
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

			break;
		}
		case LO_TASK:
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
		case LO_TEXT:
			app.opt_text = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_text + 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = ::strtoul(optarg, NULL, 0);
			break;
		case LO_TRUNCATE:
			app.opt_truncate = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_truncate + 1;
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
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
			fprintf(stderr, "getopt_long() returned character code %d\n", c);
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
		app.arg_numNodes = ::strtoul(argv[optind++], &endptr, 0);

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
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, app.arg_numNodes, ctx.flags & context_t::MAGICMASK_PURE);
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
		// is restart data present?
		const metricsRestart_t *pRestart = getMetricsRestart(MAXSLOTS, app.arg_numNodes, ctx.flags & context_t::MAGICMASK_PURE);
		if (pRestart == NULL) {
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
	 * Open input database for update
	 */

	// Open input
	database_t db(ctx);

	// test readOnly mode
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

	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));

	// prepare sections and indices for use
	app.prepareSections(db, app.arg_numNodes,
			    database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_SIGNATUREINDEX |
			    database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX |
			    database_t::ALLOCMASK_PATTERNFIRST | database_t::ALLOCMASK_PATTERNFIRSTINDEX |
			    database_t::ALLOCMASK_PATTERNSECOND | database_t::ALLOCMASK_PATTERNSECONDINDEX);

	// attach database
	app.pStore = &db;

	/*
	 * Finalise allocations and create database
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		struct sysinfo info;
		if (sysinfo(&info) == 0) {
			double percent = 100.0 * ctx.totalAllocated / info.freeram;
			if (percent > 80)
				fprintf(stderr, "WARNING: using %.1f%% of free memory minus cache\n", percent);
		}
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Allocated %.3fG memory. freeMemory=%.3fG.\n", ctx.timeAsString(), ctx.totalAllocated / 1e9, info.freeram / 1e9);
	}

	if (app.opt_load)
		app.patternsFromFile();
	if (app.opt_generate)
		app.patternsFromGenerator();

#if 0
	/*
	 * Where to look for new candidates
	 */

	// if input is empty, skip reserved entries
	if (!app.readOnlyMode) {
		assert(store.numMember > 0);
	}

	if (app.opt_text == app.OPTTEXT_BRIEF) {
		/*
		 * Display members of complete dataset
		 *
		 * <memberName> <numPlaceholder>
		 */
		for (unsigned iMid = 1; iMid < store.numMember; iMid++)
			printf("%s\n", store.members[iMid].name);
	}

	if (app.opt_text == app.OPTTEXT_VERBOSE) {
		/*
		 * Display full members, grouped by signature
		 */
		for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
			const signature_t *pSignature = store.signatures + iSid;

			for (unsigned iMid = pSignature->firstMember; iMid; iMid = store.members[iMid].nextMember) {
				member_t *pMember = store.members + iMid;

				printf("%u\t%u\t%u\t%s\t", iMid, iSid, pMember->tid, pMember->name);
				printf("%03x\t", tinyTree_t::calcScoreName(pMember->name));

				uint32_t Qmid = store.pairs[pMember->Qmt].id, Qtid = store.pairs[pMember->Qmt].tid;
				printf("%u:%s/%u:%.*s\t",
				       Qmid, store.members[Qmid].name,
				       Qtid, store.signatures[store.members[Qmid].sid].numPlaceholder, store.fwdTransformNames[Qtid]);

				uint32_t Tmid = store.pairs[pMember->Tmt].id, Ttid = store.pairs[pMember->Tmt].tid;
				printf("%u:%s/%u:%.*s\t",
				       Tmid, store.members[Tmid].name,
				       Ttid, store.signatures[store.members[Tmid].sid].numPlaceholder, store.fwdTransformNames[Ttid]);

				uint32_t Fmid = store.pairs[pMember->Fmt].id, Ftid = store.pairs[pMember->Fmt].tid;
				printf("%u:%s/%u:%.*s\t",
				       Fmid, store.members[Fmid].name,
				       Ftid, store.signatures[store.members[Fmid].sid].numPlaceholder, store.fwdTransformNames[Ftid]);

				for (unsigned i = 0; i < member_t::MAXHEAD; i++)
					printf("%u:%s\t", pMember->heads[i], store.members[pMember->heads[i]].name);

				if (pSignature->flags & signature_t::SIGMASK_SAFE) {
					if (pMember->flags & member_t::MEMMASK_SAFE)
						printf("S");
					else
						printf("s");
				}
				if (store.signatures[pMember->sid].flags & signature_t::SIGMASK_KEY)
					printf("K");
				if (pMember->flags & member_t::MEMMASK_COMP)
					printf("C");
				if (pMember->flags & member_t::MEMMASK_LOCKED)
					printf("L");
				if (pMember->flags & member_t::MEMMASK_DEPR)
					printf("D");
				if (pMember->flags & member_t::MEMMASK_DELETE)
					printf("X");
				printf("\n");
			}
		}
	}
#endif
	
	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		if (!app.opt_saveIndex) {
			db.interleave             = 0;
			db.interleaveStep         = 0;
			db.signatureIndexSize     = 0;
			db.swapIndexSize          = 0;
			db.numImprint             = 0;
			db.imprintIndexSize       = 0;
			db.pairIndexSize          = 0;
			db.memberIndexSize        = 0;
			db.patternFirstIndexSize  = 0;
			db.patternSecondIndexSize = 0;
		}

		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		db.save(app.arg_outputDatabase);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		json_t *jResult = json_object();
		json_object_set_new_nocheck(jResult, "done", json_string_nocheck(argv[0]));
		if (app.opt_taskLast) {
			json_object_set_new_nocheck(jResult, "taskId", json_integer(app.opt_taskId));
			json_object_set_new_nocheck(jResult, "taskLast", json_integer(app.opt_taskLast));
		}
		if (app.opt_windowLo || app.opt_windowHi) {
			json_object_set_new_nocheck(jResult, "windowLo", json_integer(app.opt_windowLo));
			json_object_set_new_nocheck(jResult, "windowHi", json_integer(app.opt_windowHi));
		}
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		db.jsonInfo(jResult);
		fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}

	return 0;
}
