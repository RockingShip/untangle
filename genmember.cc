//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-30 17:20:21
 *
 * Collect signature group members.
 *
 * Basic group members share the same node size, which is the smallest a signature group can have.
 * A member is considered safe if the three components and heads all reference safe members.
 * Some groups are unsafe. Replacements are found by selecting larger structures.
 *
 * Keep smaller unsafe nodes for later normalisations.
 *
 * normalisation:
 * 1) Algebraic (function grouping)
 * 2) Dyadic ordering (layout ordering)
 * 3) Imprints (layout orientation "skins")
 * 4) Signature groups (restructuring)
 * Basically, `genmember` collects structures that do not trigger normalisation or orphans when used for creation/construction.
 *
 * @date 2020-04-01 23:48:02
 *
 * I always thought that the goal motivation was to replace structures with smallest nodesize but that might not be the case.
 * 3040 signature groups in 4n9 space fail to have safe members. However, they do exist in 5n9 space.
 *
 * @date 2020-04-02 10:44:05
 *
 * Structures have heads and tails.
 * Tails are components and sub-components, heads are the structures minus one node.
 * Safe members have safe heads and tails.
 * Size of signature group is size of smallest safe member.
 *
 * @date 2020-04-02 23:43:18
 *
 * Unsafe members start to occur in 4n9 space, just like back-references.
 *
 * @date 2020-04-06 22:55:07
 *
 * `genmember` collects raw members.
 * Invocations are made with increasing nodeSize to find new members or safe replacements.
 * Once a group is safe (after invocation) new members will be rejected, this makes that only unsafe groups need detection.
 * Multi-pass is possible by focusing on a a smaller number of signature groups. This allows for extreme high speeds (interleave) at a cost of storage.
 * `genmember` actually needs two modes: preparation of an imprint index (done by master) and collecting (done by workers).
 * Workers can take advantage of the read-only imprint index in shared memory (`mmap`)
 *
 * Basically, `genmember` collects constructing components.
 * Only after all groups are safe can selecting occur.
 *
 * - All single member groups lock components (tails) and providers (heads)
 * - Groups with locked heads and tails become locked themselves.
 * Speculating:
 * - unsafe members can be grouped by component sid (resulting in a single "best `compare()`" member
 * - safe members can be grouped by component mid (resulting in a single "best `compare()`" member
 * - unsafe groups with locked members but unsafe providers can promote the providers (what to do when multiple)
 * - safe groups with unsafe members can release heads/tails allowing their refcount to drop to zero and be removed.
 *
 * Intended usage:
 *
 * - prepare new database by creating imprints for safe members.
 *   It is safe to use extreme high interleave (5040, 15120, 40320 and 60480)
 *   The higher the faster but less groups to detect.
 *
 * - After prepare let workers collect members using `--text=3` which collects on the fly.
 *
 * - After all workers complete, join all worker results and create dataset, use `--text=1`
 *
 * - repeat preparing and collecting until collecting has depleted
 *
 * - increase nodeSize by one and repeat.
 *
 * NOTE: don't be smart in rejecting members until final data-analysis is complete.
 *       This is a new feature for v2 and uncharted territory
 *
 * @date 2020-04-07 01:07:34
 *
 * At this moment calculating and collecting:
 * `restartData[]` for `7n9-pure`. This is a premier!
 * signature group members for 6n9-pure. This is also premier.
 *
 * pure dataset looks promising:
 * share the same `4n9` address space, which holds 791646 signature groups.
 * `3n9-pure` has 790336 empty and 0 unsafe groups
 * `4n9-pure` has 695291 empty and 499 unsafe groups
 * `5n9-pure` has .. empty and .. unsafe groups
 * now scanning `6n9-pure` for the last 46844.
 * that is needs to get as low as possible, searching `7n9` is far above my resources.
 * Speed is about 1590999 candidates/s
 *
 * The pure dataset achieves the same using only `"Q?!T:F"` / `"abc!"` nodes/operators.
 * This releases the requirement to store information about the inverted state of `T`.
 * `T` is always inverted.
 * To compensate for loss of variety more nodes are needed.
 *
 * safe members avoid being normalised when their notation is being constructed.
 * From the constructor point of view:
 *   unsafe members have smaller nodeSize but their notation is written un a language not understood
 *   it can be translated with penalty (extra nodes)
 *
 * @date 2020-04-07 20:57:08
 *
 * `genmember` runs in 3 modes:
 * - Merge (default)
 *   = Signatures are copied
 *   = Imprints are inherited or re-built on demand
 *   = Members are copied
 *   = Additional members are loaded/generated
 *   = Member sorting
 *
 * - Prepare
 *   = Signatures are copied
 *   = Imprints are set to select empty=unsafe signature groups
 *   = Members are inherited
 *   = No member-sorting
 *   = Output is intended for `--mode=merge`
 *
 * - Collect (worker)
 *   = Signatures are copied
 *   = Imprints are inherited
 *   = Members are inherited
 *   = Each candidate member that matches is logged, signature updated and not recorded
 *   = No member-sorting
 *
 * @date 2020-04-22 21:20:56
 *
 * `genmember` selects candidates already present in the imprint index.
 * Selected candidates are added to `members`.
 *
 * @date 2020-04-22 21:37:03
 *
 * Text modes:
 *
 * `--text[=1]` Brief mode that show selected candidates passed to `foundTreeMember()`.
 *              Selected candidates are those that challenge and win the current display name.
 *              Also intended for transport and merging when broken into multiple tasks.
 *              Can be used for the `--load=<file>` option.
 *              Output can be converted to other text modes by `"./gensignature <input.db> <numNode> [<output.db>]--load=<inputList> --no-generate --text=<newMode> '>' <outputList>
 *
 *              <name>
 *
 * `--text=2`   Full mode of all candidates passed to `foundTreeMember()` including what needed to compare against the display name.
 *
 *              <cid> <sid> <cmp> <name> <size> <numPlaceholder> <numEndpoint> <numBackRef>

 *              where:
 *                  <cid> is the candidate id assigned by the generator.
 *                  <sid> is the signature id assigned by the associative lookup.
 *                  <cmp> is the result of `comparSignature()` between the candidate and the current display name.
 *
 *              <cmp> can be:
 *                  cmp = '<'; // worse, group safe, candidate unsafe
 *                  cmp = '-'; // worse, candidate too large for group
 *                  cmp = '='; // equal, group unsafe, candidate unsafe
 *                  cmp = '+'; // equal, group safe, candidate safe
 *                  cmp = '>'; // better, group unsafe, candidate safe
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
 *              <mid> <sid> <tid> <name> <Qmid> <Tmid> <Fmid> <HeadMids> <Safe/Nonsafe-member> <Safe/Nonsafe-signature>
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

#include "config.h"
#include "genmember.h"
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
 * @global {genmemberContext_t} Application context
 */
genmemberContext_t app(ctx);

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
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --listlookup                    List failed member lookups of `findheadtail()`\n");
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
		fprintf(stderr, "\t   --safe                          Consider safe members only\n");
		fprintf(stderr, "\t   --text[=1]                      Brief accepted `foundTree()` candidates\n");
		fprintf(stderr, "\t   --text=2                        Verbose accepted `foundTree()` candidates\n");
		fprintf(stderr, "\t   --text=3                        Brief database dump\n");
		fprintf(stderr, "\t   --text=4                        Verbose database dump\n");
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --truncate                      Truncate on database overflow\n");
		fprintf(stderr, "\t-v --verbose                       Say more\n");
		fprintf(stderr, "\t-V --version                       Show versions\n");
		fprintf(stderr, "\nSystem options:\n");
		fprintf(stderr, "\t   --[no-]cascade                  Cascading dyadic normalisation [default=%s]\n", (ctx.flags & context_t::MAGICMASK_CASCADE) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]paranoid                 Expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF (single-node) rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]rewrite                  Structure (multi-node)  rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_REWRITE) ? "enabled" : "disabled");
		fprintf(stderr, "\nGenerator options:\n");
		fprintf(stderr, "\t   --mixed                         Only top-level node may be mixed QnTF/QTF, others are QnTF only\n");
		fprintf(stderr, "\t   --sid=[<low>,]<high>            Sid range upper bound  [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --task=sge                      Get task settings from SGE environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>              Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --window=[<low>,]<high>         Upper end restart window [default=%lu,%lu]\n", app.opt_windowLo, app.opt_windowHi);
		fprintf(stderr, "\nDatabase options:\n");
		fprintf(stderr, "\t   --firstindexsize=<number>       Size of patternFirst index [default=%u]\n", app.opt_patternFirstIndexSize);
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>           Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --maxfirst=<number>             Maximum of (first step) patterns [default=%u]\n", app.opt_maxPatternFirst);
		fprintf(stderr, "\t   --maximprint=<number>           Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxmember=<number>            Maximum number of members [default=%u]\n", app.opt_maxMember);
		fprintf(stderr, "\t   --maxpair=<number>              Maximum number of sid/tid pairs [default=%u]\n", app.opt_maxPair);
		fprintf(stderr, "\t   --maxsecond=<number>            Maximum of (second step) patterns [default=%u]\n", app.opt_maxPatternSecond);
		fprintf(stderr, "\t   --maxsignature=<number>         Maximum number of signatures [default=%u]\n", app.opt_maxSignature);
		fprintf(stderr, "\t   --maxswap=<number>              Maximum number of swaps [default=%u]\n", app.opt_maxSwap);
		fprintf(stderr, "\t   --memberindexsize=<number>      Size of member index [default=%u]\n", app.opt_memberIndexSize);
		fprintf(stderr, "\t   --pairindexsize=<number>        Size of sid/tid pair index [default=%u]\n", app.opt_pairIndexSize);
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --[no-]saveindex                Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --secondindexsize=<number>      Size of patternSecond index [default=%u]\n", app.opt_patternSecondIndexSize);
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --swapindexsize=<number>        Size of swap index [default=%u]\n", app.opt_swapIndexSize);
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
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
			LO_VERSION = 'V',
			// long opts
			LO_ALTGEN = 1,
			LO_DEBUG,
			LO_FORCE,
			LO_GENERATE,
			LO_LISTLOOKUP,
			LO_LOAD,
			LO_NOGENERATE,
			LO_SAFE,
			LO_TEXT,
			LO_TIMER,
			LO_TRUNCATE,
			// system options
			LO_AINF,
			LO_CASCADE,
			LO_NOAINF,
			LO_NOCASCADE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_PARANOID,
			LO_PURE,
			// generator options
			LO_MIXED,
			LO_SID,
			LO_TASK,
			LO_WINDOW,
			// database options
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_MAXIMPRINT,
			LO_MAXMEMBER,
			LO_MAXPAIR,
			LO_MAXPATTERNFIRST,
			LO_MAXPATTERNSECOND,
			LO_MAXSIGNATURE,
			LO_MAXSWAP,
			LO_MEMBERINDEXSIZE,
			LO_NOSAVEINDEX,
			LO_PAIRINDEXSIZE,
			LO_PATTERNFIRSTINDEXSIZE,
			LO_PATTERNSECONDINDEXSIZE,
			LO_RATIO,
			LO_SAVEINDEX,
			LO_SIGNATUREINDEXSIZE,
			LO_SWAPINDEXSIZE,
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			// short options
			{"debug",              1, 0, LO_DEBUG},
			{"force",              0, 0, LO_FORCE},
			{"help",               0, 0, LO_HELP},
			{"quiet",              2, 0, LO_QUIET},
			{"timer",              1, 0, LO_TIMER},
			{"verbose",            2, 0, LO_VERBOSE},
			{"version",            0, 0, LO_VERSION},
			// long options
			{"altgen",             0, 0, LO_ALTGEN},
			{"generate",           0, 0, LO_GENERATE},
			{"load",               1, 0, LO_LOAD},
			{"listlookup",         0, 0, LO_LISTLOOKUP},
			{"no-generate",        0, 0, LO_NOGENERATE},
			{"safe",               0, 0, LO_SAFE},
			{"text",               2, 0, LO_TEXT},
			{"truncate",           0, 0, LO_TRUNCATE},
			// system options
			{"ainf",               0, 0, LO_AINF},
			{"cascade",            0, 0, LO_CASCADE},
			{"no-ainf",            0, 0, LO_NOAINF},
			{"no-cascade",         0, 0, LO_NOCASCADE},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-pure",            0, 0, LO_NOPURE},
			{"paranoid",           0, 0, LO_PARANOID},
			{"pure",               0, 0, LO_PURE},
			// generator options
			{"mixed",              0, 0, LO_MIXED},
			{"sid",                1, 0, LO_SID},
			{"task",               1, 0, LO_TASK},
			{"window",             1, 0, LO_WINDOW},
			// database options
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",         1, 0, LO_INTERLEAVE},
			{"maximprint",         1, 0, LO_MAXIMPRINT},
			{"maxfirst",           1, 0, LO_MAXPATTERNFIRST},
			{"maxmember",          1, 0, LO_MAXMEMBER},
			{"maxpair",            1, 0, LO_MAXPAIR},
			{"maxsecond",          1, 0, LO_MAXPATTERNSECOND},
			{"maxsignature",       1, 0, LO_MAXSIGNATURE},
			{"maxswap",            1, 0, LO_MAXSWAP},
			{"memberindexsize",    1, 0, LO_MEMBERINDEXSIZE},
			{"no-saveindex",       0, 0, LO_NOSAVEINDEX},
			{"pairindexsize",      1, 0, LO_PAIRINDEXSIZE},
			{"firstindexsize",     1, 0, LO_PATTERNFIRSTINDEXSIZE},
			{"secondindexsize",    1, 0, LO_PATTERNSECONDINDEXSIZE},
			{"ratio",              1, 0, LO_RATIO},
			{"saveindex",          0, 0, LO_SAVEINDEX},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"swapindexsize",      1, 0, LO_SWAPINDEXSIZE},
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
			/*
			 * Short options
			 */
		case LO_DEBUG:
			ctx.opt_debug = ::strtoul(optarg, NULL, 0);
			break;
		case LO_FORCE:
			app.opt_force++;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_QUIET:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = ::strtoul(optarg, NULL, 0);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
			break;
		case LO_VERSION:
			printf("Program=%s Database=%x\n", PACKAGE_VERSION, FILE_MAGIC);
			exit(0);

			/*
			 * Long options
			 */
		case LO_ALTGEN:
			app.opt_altgen++; // EXPERIMENTAL!
			break;
		case LO_GENERATE:
			app.opt_generate++;
			break;
		case LO_LISTLOOKUP:
			app.opt_listLookup++; // EXPERIMENTAL!
			break;
		case LO_LOAD:
			app.opt_load = optarg;
			break;
		case LO_NOGENERATE:
			app.opt_generate = 0;
			break;
		case LO_SAFE:
			app.opt_safe++;
			break;
		case LO_TEXT:
			app.opt_text = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_text + 1;
			break;
		case LO_TRUNCATE:
			app.opt_truncate = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_truncate + 1;
			break;

			/*
			 * System options
			 */
		case LO_AINF:
			ctx.flags |= context_t::MAGICMASK_AINF;
			break;
		case LO_CASCADE:
			ctx.flags |= context_t::MAGICMASK_CASCADE;
			break;
		case LO_NOAINF:
			ctx.flags &= ~context_t::MAGICMASK_AINF;
			break;
		case LO_NOCASCADE:
			ctx.flags &= ~context_t::MAGICMASK_CASCADE;
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

			/*
			 * Generator options
			 */
		case LO_MIXED:
			app.opt_mixed++;
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
		case LO_TASK: {
			if (::strcmp(optarg, "sge") == 0) {
				const char *p;

				p = getenv("SGE_TASK_ID");
				app.opt_taskId = p ? atoi(p) : 0;
				if (app.opt_taskId < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_ID\n");
					exit(1);
				}

				p = getenv("SGE_TASK_LAST");
				app.opt_taskLast = p ? atoi(p) : 0;
				if (app.opt_taskLast < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_LAST\n");
					exit(1);
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

			/*
			 * Database options
			 */
		case LO_IMPRINTINDEXSIZE:
			app.opt_imprintIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_INTERLEAVE:
			app.opt_interleave = ::strtoul(optarg, NULL, 0);
			if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
				ctx.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
			break;
		case LO_MAXIMPRINT:
			app.opt_maxImprint = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXMEMBER:
			app.opt_maxMember = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXPAIR:
			app.opt_maxPair = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXPATTERNFIRST:
			app.opt_maxPatternFirst = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXPATTERNSECOND:
			app.opt_maxPatternSecond = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXSIGNATURE:
			app.opt_maxSignature = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXSWAP:
			app.opt_maxSwap = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_MEMBERINDEXSIZE:
			app.opt_memberIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_NOSAVEINDEX:
			app.opt_saveIndex = 0;
			break;
		case LO_PAIRINDEXSIZE:
			app.opt_pairIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_PATTERNFIRSTINDEXSIZE:
			app.opt_patternFirstIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_PATTERNSECONDINDEXSIZE:
			app.opt_patternSecondIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
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
	 * Open database for update
	 */

	// Open input
	database_t db(ctx);

	// test readOnly mode
	app.readOnlyMode = (app.arg_outputDatabase == NULL);

	db.open(app.arg_inputDatabase);

	// display system flags when database was created
	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		std::string dbText = ctx.flagsToText(db.creationFlags);
		std::string ctxText = ctx.flagsToText(ctx.flags);

		if (db.creationFlags != ctx.flags)
			fprintf(stderr, "[%s] WARNING: Database/system flags differ: database=[%s] current=[%s]\n", ctx.timeAsString(), dbText.c_str(), ctxText.c_str());
		else if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), dbText.c_str());
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

	if (db.numSignature <= db.IDFIRST)
		ctx.fatal("\n{\"error\":\"Missing signature section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
			  __FUNCTION__, __FILE__, __LINE__, app.arg_inputDatabase);

	// prepare sections (that need writing) and indices for use
	uint32_t sections = database_t::ALLOCMASK_SIGNATUREINDEX |
			    database_t::ALLOCMASK_IMPRINTINDEX |
			    database_t::ALLOCMASK_MEMBER | database_t::ALLOCMASK_MEMBERINDEX |
			    database_t::ALLOCMASK_PAIR | database_t::ALLOCMASK_PAIRINDEX;

	unsigned rebuildIndices = app.prepareSections(db, app.arg_numNodes, sections);

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

	/*
	 * Reconstruct indices
	 */

	// imprints are auto-generated from signatures
	if (rebuildIndices & database_t::ALLOCMASK_IMPRINT) {
		// reconstruct imprints based on signatures
		db.rebuildImprint();
		rebuildIndices &= ~(database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX);
	}

	if (rebuildIndices) {
		db.rebuildIndices(rebuildIndices);
	}

	/*
	 * Main 
	 */

	// attach database
	app.connect(db);

	/*
	 * count empty/unsafe
	 */

	app.numEmpty = app.numUnsafe = 0;
	for (unsigned iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
		if (db.signatures[iSid].firstMember == 0)
			app.numEmpty++;
		else if (!(db.signatures[iSid].flags & signature_t::SIGMASK_SAFE))
			app.numUnsafe++;
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] numImprint=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u\n",
			ctx.timeAsString(),
			db.numImprint, db.numImprint * 100.0 / db.maxImprint,
			db.numMember, db.numMember * 100.0 / db.maxMember,
			app.numEmpty, app.numUnsafe);

	/*
	 * Determine tree size for safe groups
	 */

	for (unsigned iSid = 0; iSid < db.maxSignature; iSid++)
		app.pSafeSize[iSid] = 0;

	// calc initial signature group scores (may differ from signature)
	for (unsigned iSid = 0; iSid < db.numSignature; iSid++) {
		const signature_t *pSignature = db.signatures + iSid;

		if (pSignature->flags & signature_t::SIGMASK_SAFE) {
			assert(pSignature->firstMember);

			const member_t *pMember = db.members + pSignature->firstMember;

			tinyTree_t tree(ctx);
			tree.loadStringFast(pMember->name);

			app.pSafeSize[iSid] = tree.count - tinyTree_t::TINYTREE_NSTART;
		}
	}

	/*
	 * Where to look for new candidates
	 */

	// if input is empty, skip reserved entries
	if (!app.readOnlyMode) {
		assert(db.numMember > 0);
	}

	if (app.opt_load)
		app.membersFromFile();
	if (app.opt_generate) {
		if (app.arg_numNodes == 1) {
			// also include "0" and "a"
			app.arg_numNodes = 0;
			app.membersFromGenerator();
			app.arg_numNodes = 1;
		}

		if (app.opt_altgen)
			app.membersFromAltGenerator(); // alternative generator for 7n9
		else
			app.membersFromGenerator();
	}

	/*
	 * re-order and re-index members
	 */

	if (!app.readOnlyMode) {
		// compact, sort and reindex members
		app.finaliseMembers();
	}

	if (app.opt_text == app.OPTTEXT_BRIEF) {
		/*
		 * Display members of complete dataset
		 *
		 * <memberName> <numPlaceholder>
		 */
		for (unsigned iMid = db.IDFIRST; iMid < db.numMember; iMid++)
			printf("%s\n", db.members[iMid].name);
	}

	if (app.opt_text == app.OPTTEXT_VERBOSE) {
		/*
		 * Display full members, grouped by signature
		 */
		for (unsigned iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			const signature_t *pSignature = db.signatures + iSid;

			for (unsigned iMid = pSignature->firstMember; iMid; iMid = db.members[iMid].nextMember) {
				member_t *pMember = db.members + iMid;

				printf("%u\t%u\t%u\t%s\t", iMid, iSid, pMember->tid, pMember->name);
				printf("%03x\t", tinyTree_t::calcScoreName(pMember->name));

				uint32_t Qmid = db.pairs[pMember->Qmt].id, Qtid = db.pairs[pMember->Qmt].tid;
				printf("%u:%s/%u:%.*s\t",
				       Qmid, db.members[Qmid].name,
				       Qtid, db.signatures[db.members[Qmid].sid].numPlaceholder, db.fwdTransformNames[Qtid]);

				uint32_t Tmid = db.pairs[pMember->Tmt].id, Ttid = db.pairs[pMember->Tmt].tid;
				printf("%u:%s/%u:%.*s\t",
				       Tmid, db.members[Tmid].name,
				       Ttid, db.signatures[db.members[Tmid].sid].numPlaceholder, db.fwdTransformNames[Ttid]);

				uint32_t Fmid = db.pairs[pMember->Fmt].id, Ftid = db.pairs[pMember->Fmt].tid;
				printf("%u:%s/%u:%.*s\t",
				       Fmid, db.members[Fmid].name,
				       Ftid, db.signatures[db.members[Fmid].sid].numPlaceholder, db.fwdTransformNames[Ftid]);

				for (unsigned i = 0; i < member_t::MAXHEAD; i++)
					printf("%u:%s\t", pMember->heads[i], db.members[pMember->heads[i]].name);

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
			// drop indices
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
		} else {
			// rebuild indices based on actual counts so that loading the database does not cause a rebuild
			uint32_t size = ctx.nextPrime(db.numPair * app.opt_ratio);
			if (db.pairIndexSize > size)
				db.pairIndexSize = size;
			size = ctx.nextPrime(db.numMember * app.opt_ratio);
			if (db.memberIndexSize > size)
				db.memberIndexSize = size;

			db.rebuildIndices(database_t::ALLOCMASK_PAIRINDEX | database_t::ALLOCMASK_MEMBERINDEX);
		}

		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		db.creationFlags = ctx.flags;
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
