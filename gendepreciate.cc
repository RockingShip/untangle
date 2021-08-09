// # pragma GCC optimize ("O3") // optimize on demand

/*
 * @date 2021-06-27 15:56:11
 * 
 * Mark excess members as depreciated.
 * Excess members are those that when removed, the remaining collection spans all signature groups.
 * The goal is to have a collection wih the minimal amount of components,
 *   i.e. members that are used to construct other members, either in part or as a whole.
 * The `rewritedata[]` pre-processor can use this as a first-attempt to reduce the most obvious mirrors and transforms.
 * The mechanics behind this is: if structures are never created (because other structures have the same effect),
 *   they can be excluded from the system and safely ignored.
 *
 * The collection is pruned by removing he component members one at a time.
 * If the remaining collection has at least one member per signature group,
 *   then the component is considered excess and can be safely ignored (depreciated).
 * However, if the collection becomes incomplete then the component is considered critical and locked.
 *
 * Several algorithms have been tried to determine the order of members to examine.
 * Trying members with the greatest effect when removed are considered first.
 * In order or priority:
 *   - Smallest structures first as they are the most versatile building blocks
 *   - Members that have the highest reference count
 *   - Most versatile members first (lowest memberId)
 *
 * The "safe" 5n9 collection consists of 6533489 members, of which 684839 are used as component.
 * Rebuilding a collection where some are excluded, is an extreme time-consuming two-pass operation.
 * The first pass is to determine which members are part of the new set, the second pass to flag those that were left behind.
 * The speed is around 11 operations per second, which would take some 19 hours.
 *
 * However, the number of members to exclude does not effect the speed of the operation.
 * The main optimisation is to exclude members in bursts.
 * If the exclusion should fail because the burst includes an undetected locked member,
 *   then the burst is reduced in size in expectation that the remaining (smaller) burst will succeed.
 * This approach reduces the overall computation to about 8 hours.
 *
 * The second challenge is the speed of updating the reference counts to update the prune ordering.
 * Sorting some 300k-700k elements is also highly time-consuming.
 * An alternative approach is to determine the relative distance in the waiting queue, and using memmove() to shift the intermediate areas
 *
 * Runtimes:
 *   numNode=4, about 15 minutes
 *   numNode=5, about 6 hours
 *
 * Text modes:
 *
 *  <flags> can be:
 *     'D' depreciated (member must be excluded)
 *     'L' Locked (member must be included)

 * `--text[=1]` Brief mode that shows selected members that have their flags adjusted
 *              Also intended for transport and checkpoint/restarting
 *              Can be used for the `--load=<file>` option.
 *              Output can be converted to other text modes by `"./gendepreciate <input.db> <numNode> [<output.db>]--load=<inputList> --no-generate --text=<newMode> '>' <outputList>
 *
 *              <name> <flags>
 *
 * `--text=2`   Full mode of all members as they are being processed
 *
 *              <flags> <numComponents> <mid> <refcnt> <name>
 *
 * `--text=3`   Selected and sorted members, included all implied and cascaded
 *              NOTE: same format as `--text=1`
 *              NOTE: requires sorting and will copy (not inherit) member section
 *
 *              <name> <flags>
 *
 * `--text=4`   Selected and sorted signatures that are written to the output database
 *              NOTE: requires sorting and will copy (not inherit) imprint section
 *
 *              <sid> <mid> <tid> <name> <flags>
 * Ticker:
 *   [TIMESTAMP] position( speed/s) procent% eta=EstimatedTime | numComponent=n numDepr=n | cntDepr=n cntLock=n | refcnt=n mid=n name/expression
 *   numComponents: number of prime components, the lower the better
 *   cntDepr: number of components depreciated.
 *   numDepr: also including all members referencing the component (total)
 *   cntLock: number of component locked.
 *
 * @date 2021-07-02 22:50:02
 *
 * when restarted for 4n9 at position 6397, and restarted and restarted at 4164
 * and concatenating the 3 ists and reloading, the final resut is different.
 * Also 589497 instead of 589536 when created in a single run.
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2017-2021, xyzzy@rockingship.org
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
#include "database.h"
#include "dbtool.h"
#include "gendepreciate.h"
#include "genmember.h"

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
 * @global {gendepreciateContext_t} Application context
 */
gendepreciateContext_t app(ctx);
genmemberContext_t appMember(ctx);

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
		fprintf(stderr, "\t   --burst=<number>                Burst size for excluding members [default=%u, 0=determined by <numnode>]\n", app.opt_burst);
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>           Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --lookupsafe                    Only lookup signatures are safe\n");
		fprintf(stderr, "\t   --maximprint=<number>           Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxmember=<number>            Maximum number of members [default=%u]\n", app.opt_maxMember);
		fprintf(stderr, "\t   --memberindexsize=<number>      Size of member index [default=%u]\n", app.opt_memberIndexSize);
		fprintf(stderr, "\t   --mode=<number>                 Operational mode [default=%u]\n", app.opt_mode);
		fprintf(stderr, "\t   --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --reverse                       Reverse order of signatures\n");
		fprintf(stderr, "\t   --[no-]saveindex                Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --text                          Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --[no-]unsafe                   Reindex imprints based on empty/unsafe signature groups [default=%s]\n", (ctx.flags & context_t::MAGICMASK_UNSAFE) ? "enabled" : "disabled");
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
			LO_BURST = 1,
			LO_DEBUG,
			LO_FORCE,
			LO_GENERATE,
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_LOAD,
			LO_LOOKUPSAFE,
			LO_MAXIMPRINT,
			LO_MAXMEMBER,
			LO_MEMBERINDEXSIZE,
			LO_MODE,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_NOUNSAFE,
			LO_PARANOID,
			LO_PURE,
			LO_RATIO,
			LO_REVERSE,
			LO_SAVEINDEX,
			LO_SIGNATUREINDEXSIZE,
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
			{"burst",              1, 0, LO_BURST},
			{"debug",              1, 0, LO_DEBUG},
			{"force",              0, 0, LO_FORCE},
			{"generate",           0, 0, LO_GENERATE},
			{"help",               0, 0, LO_HELP},
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",         1, 0, LO_INTERLEAVE},
			{"load",               1, 0, LO_LOAD},
			{"lookupsafe",         0, 0, LO_LOOKUPSAFE},
			{"maximprint",         1, 0, LO_MAXIMPRINT},
			{"maxmember",          1, 0, LO_MAXMEMBER},
			{"memberindexsize",    1, 0, LO_MEMBERINDEXSIZE},
			{"mode",               1, 0, LO_MODE},
			{"no-generate",        0, 0, LO_NOGENERATE},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-pure",            0, 0, LO_NOPURE},
			{"no-saveindex",       0, 0, LO_NOSAVEINDEX},
			{"no-unsafe",          0, 0, LO_NOUNSAFE},
			{"paranoid",           0, 0, LO_PARANOID},
			{"pure",               0, 0, LO_PURE},
			{"quiet",              2, 0, LO_QUIET},
			{"ratio",              1, 0, LO_RATIO},
			{"reverse",            0, 0, LO_REVERSE},
			{"saveindex",          0, 0, LO_SAVEINDEX},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"text",               2, 0, LO_TEXT},
			{"timer",              1, 0, LO_TIMER},
			{"unsafe",             0, 0, LO_UNSAFE},
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
		case LO_BURST:
			app.opt_burst = ::strtoul(optarg, NULL, 0);
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
		case LO_IMPRINTINDEXSIZE:
			app.opt_imprintIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_INTERLEAVE:
			app.opt_interleave = ::strtoul(optarg, NULL, 0);
			if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
				ctx.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
			break;
		case LO_LOAD:
			app.opt_load = optarg;
			break;
		case LO_LOOKUPSAFE:
			app.opt_lookupSafe++;
			break;
		case LO_MAXIMPRINT:
			app.opt_maxImprint = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXMEMBER:
			app.opt_maxMember = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MEMBERINDEXSIZE:
			app.opt_memberIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_MODE:
			app.opt_mode = ::strtoul(optarg, NULL, 0);
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
		case LO_RATIO:
			app.opt_ratio = strtof(optarg, NULL);
			break;
		case LO_REVERSE:
			app.opt_reverse++;
			break;
		case LO_NOSAVEINDEX:
			app.opt_saveIndex = 0;
			break;
		case LO_SAVEINDEX:
			app.opt_saveIndex = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_saveIndex + 1;
			break;
		case LO_SIGNATUREINDEXSIZE:
			app.opt_signatureIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
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

	// Default `--burst` depends on the size of the prune collection
	if (app.opt_burst == 0) {
		if (app.arg_numNodes >= 5)
			app.opt_burst = 32;
		else if (app.arg_numNodes == 4)
			app.opt_burst = 8;
		else if (app.arg_numNodes == 3)
			app.opt_burst = 2;
		else
			app.opt_burst = 1;
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

	/*
	 * @date 2020-04-21 00:16:34
	 *
	 * create output
	 *
	 * Transforms, signature, hint and imprint data never change and can be inherited
	 * Members can be inherited when nothing is added (missing output database)
	 *
	 * Sections can be inherited if their data or index settings remain unchanged
	 *
	 * NOTE: Signature data must be writable when `firstMember` changes (output database present)
	 */

	database_t store(ctx);

	// will be using `lookupSignature()` and `lookupMember()`
	app.inheritSections &= ~(database_t::ALLOCMASK_MEMBER | database_t::ALLOCMASK_MEMBERINDEX);
	// signature indices are used read-only, remove from inherit if sections are empty
	if (!db.numSignature)
		app.inheritSections &= ~database_t::ALLOCMASK_SIGNATURE;
	if (!db.signatureIndexSize)
		app.inheritSections &= ~database_t::ALLOCMASK_SIGNATUREINDEX;
	if (!db.numImprint)
		app.inheritSections &= ~database_t::ALLOCMASK_IMPRINT;
	if (!db.imprintIndexSize)
		app.inheritSections &= ~database_t::ALLOCMASK_IMPRINTINDEX;
	// will require local copy of members
	app.rebuildSections |= database_t::ALLOCMASK_MEMBER;

	// inherit signature/member size
	if (!app.readOnlyMode) {
		app.opt_maxSignature = db.numSignature;
		app.opt_maxMember = db.numMember;
	}

	if (db.numTransform == 0)
		ctx.fatal("Missing transform section: %s\n", app.arg_inputDatabase);
	if (db.numEvaluator == 0)
		ctx.fatal("Missing evaluator section: %s\n", app.arg_inputDatabase);
	if (db.numSignature == 0)
		ctx.fatal("Missing signature section: %s\n", app.arg_inputDatabase);
	if (db.numMember == 0)
		ctx.fatal("Missing member section: %s\n", app.arg_inputDatabase);

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, 5, !app.readOnlyMode);

	/*
	 * Finalise allocations and create database
	 */

	// allocate big arrays
	app.pSafeSid      = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSafeSid", store.maxSignature, sizeof(*app.pSafeSid));
	app.pSafeMid      = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSafeMid", store.maxMember, sizeof(*app.pSafeMid));
	app.pSafeMap      = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSafeMap", store.maxMember, sizeof(*app.pSafeMap));
	app.pSelect       = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSelect", store.maxMember, sizeof(*app.pSelect));

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
	appMember.pStore = &store;

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

	if (app.rebuildSections & database_t::ALLOCMASK_MEMBER) {
		store.numMember = db.numMember;
		::memcpy(store.members, db.members, store.numMember * sizeof(*store.members));
	}
	if (app.rebuildSections)
		store.rebuildIndices(app.rebuildSections);

	/*
	 * count empty/unsafe
	 */

	app.numEmpty = app.numUnsafe = 0;
	for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
		if (store.signatures[iSid].firstMember == 0) {
			app.numEmpty++;
		} else if (!(store.signatures[iSid].flags & signature_t::SIGMASK_SAFE)) {
			app.numUnsafe++;
		} else {
			uint32_t iMid = store.signatures[iSid].firstMember;

//			if (!(store.members[iMid].flags & member_t::MEMMASK_SAFE)) fprintf(stderr, "S %u\n", iSid);
//			if ((store.members[iMid].flags & member_t::MEMMASK_DEPR)) fprintf(stderr, "D %u\n", iSid);
			assert(store.members[iMid].flags & member_t::MEMMASK_SAFE);
			assert(!(store.members[iMid].flags & member_t::MEMMASK_DEPR));
		}
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u\n",
			ctx.timeAsString(),
			store.numMember, store.numMember * 100.0 / store.maxMember,
			app.numEmpty, app.numUnsafe);

	/*
	 * Validate, all members should be safe and properly ordered
	 */
	{
		unsigned cntUnsafe = 0;
		unsigned cntSid, cntMid;

		// select appreciated
		++app.iVersionSelect; // select/exclude none
		app.countSafeExcludeSelected(cntSid, cntMid);

		for (unsigned iMid = 3; iMid < store.numMember; iMid++) {
			member_t *pMember = store.members + iMid;

			if (!(pMember->flags & member_t::MEMMASK_SAFE))
				cntUnsafe++;

			if (pMember->flags & member_t::MEMMASK_DEPR) {
				assert(app.pSafeMid[iMid] != app.iVersionSafe); // depreciated should not be in the appreciated set
			} else {
				assert(app.pSafeMid[iMid] == app.iVersionSafe); // depreciated must be in the appreciated set
			}

			assert(pMember->Qmt == 0 || store.pairs[pMember->Qmt].id < iMid);
			assert(pMember->Tmt == 0 || store.pairs[pMember->Tmt].id < iMid);
			assert(pMember->Fmt == 0 || store.pairs[pMember->Fmt].id < iMid);

			assert(pMember->heads[0] == 0 || pMember->heads[0] < iMid);
			assert(pMember->heads[1] == 0 || pMember->heads[1] < iMid);
			assert(pMember->heads[2] == 0 || pMember->heads[2] < iMid);
			assert(pMember->heads[3] == 0 || pMember->heads[3] < iMid);
			assert(pMember->heads[4] == 0 || pMember->heads[4] < iMid);
			assert(member_t::MAXHEAD == 5);
		}
		if (cntUnsafe > 0)
			fprintf(stderr,"WARNING: Found %u unsafe members\n", cntUnsafe);
	}

	/*
	 * Where to look for new candidates
	 */

	// if input is empty, skip reserved entries
	if (!app.readOnlyMode) {
		assert(store.numMember > 0);

		// reset locked bit, they may be outdated
		for (uint32_t iMid = 1; iMid < store.numMember; iMid++)
			store.members[iMid].flags &= ~member_t::MEMMASK_LOCKED;
	}

	// update locking
	app.updateLocked();

	if (app.opt_load) {
		app.showCounts();
		app.depreciateFromFile();

		if (!app.readOnlyMode) {
			// compact, sort and reindex members
			appMember.finaliseMembers();
		}
	}
	if (app.opt_generate) {
		for (;;) {
			app.showCounts();
			bool restart = app.depreciateFromGenerator();

			if (!app.readOnlyMode) {
				// compact, sort and reindex members
				appMember.finaliseMembers();
			}

			if (!restart)
				break;
		}
	}

	app.showCounts();

	/*
	 * Validate, all members should be safe and properly ordered
	 */
	{
		unsigned cntUnsafe = 0;
		unsigned cntSid, cntMid;

		// select appreciated
		++app.iVersionSelect; // select/exclude none
		app.countSafeExcludeSelected(cntSid, cntMid);

		for (uint32_t iMid = 1; iMid < store.numMember; iMid++) {
			member_t *pMember = store.members + iMid;

			if (pMember->flags & member_t::MEMMASK_DEPR)
				continue;

			assert(app.pSafeMid[iMid] == app.iVersionSafe);

			if (!(pMember->flags & member_t::MEMMASK_SAFE))
				cntUnsafe++;

			assert(pMember->Qmt == 0 || store.pairs[pMember->Qmt].id < iMid);
			assert(pMember->Tmt == 0 || store.pairs[pMember->Tmt].id < iMid);
			assert(pMember->Fmt == 0 || store.pairs[pMember->Fmt].id < iMid);

			assert(pMember->heads[0] == 0 || pMember->heads[0] < iMid);
			assert(pMember->heads[1] == 0 || pMember->heads[1] < iMid);
			assert(pMember->heads[2] == 0 || pMember->heads[2] < iMid);
			assert(pMember->heads[3] == 0 || pMember->heads[3] < iMid);
			assert(pMember->heads[4] == 0 || pMember->heads[4] < iMid);
			assert(member_t::MAXHEAD == 5);
		}
		if (cntUnsafe > 0)
			fprintf(stderr,"WARNING: Found %u unsafe members\n", cntUnsafe);
	}

	if (app.opt_text == app.OPTTEXT_BRIEF) {
		/*
		 * Display depreciated components
		 *
		 * <memberName>
		 */
		for (unsigned iMid = 1; iMid < store.numMember; iMid++) {
			member_t *pMember = store.members + iMid;

			if (pMember->flags & member_t::MEMMASK_COMP) {
				if (pMember->flags & member_t::MEMMASK_DEPR)
					printf("%s\tD\n", pMember->name);
				else if (pMember->flags & member_t::MEMMASK_LOCKED)
					printf("%s\tL\n", pMember->name);
			}
		}
	}

	if (app.opt_text == app.OPTTEXT_VERBOSE) {
		/*
		 * Display full members, grouped by signature
		 */
		for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
			const signature_t *pSignature = store.signatures + iSid;

			for (unsigned iMid = pSignature->firstMember; iMid; iMid = store.members[iMid].nextMember) {
				member_t *pMember = store.members + iMid;

				printf("%u\t%u\t%u\t%s\t", iSid, iMid, pMember->tid, pMember->name);

				if (pSignature->flags & signature_t::SIGMASK_SAFE) {
					if (pMember->flags & member_t::MEMMASK_SAFE)
						printf("S");
					else
						printf("s");
				}
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
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.jsonInfo(jResult);
		fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}

	return 0;
}
