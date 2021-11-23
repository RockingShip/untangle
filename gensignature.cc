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
 *      - sort signatures when `"--sort"`
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
 * @date 2021-07-28 19:56:51
 *
 * The advantage of `--truncate` is when exploring a new space (like 5n9-pure)
 *   and the guessing for `--maximprint` was too optimistic that it overflows.
 * This option will safely stop at the moment of overflow and write a database.
 * The database can be reindexed/redimensioned and pressing can continue.
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
#include "gensignature.h"
#include "genswap.h"
#include "metrics.h"

// Need generator to allow ranges
#include "restartdata.h"

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
 * @global {gensignatureContext_t} Application context
 */
gensignatureContext_t app(ctx);

/*
 * @date 2021-10-24 10:51:25
 * 
 * Application context for generating signature swaps.

 * @global {gensignatureContext_t} Application context
 */
genswapContext_t appSwap(ctx);

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
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]  -- Add signatures of given node size\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --listincomplete                List unsafe core signatures, for inclusion\n");
		fprintf(stderr, "\t   --listsafe                      List safe signatures, for inclusion\n");
		fprintf(stderr, "\t   --listunsafe                    List empty/unsafe signatures, for exclusion\n");
		fprintf(stderr, "\t   --listused                      List used/non-empty signatures, for inclusion\n");
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --markmixed                     Flag signatures that have pure with top-level mixed members\n");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
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
			LO_DEBUG = 1,
			LO_FORCE,
			LO_GENERATE,
			LO_LISTINCOMPLETE,
			LO_LISTSAFE,
			LO_LISTUNSAFE,
			LO_LISTUSED,
			LO_LOAD,
			LO_MARKMIXED,
			LO_NOGENERATE,
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
			{"generate",           0, 0, LO_GENERATE},
			{"listincomplete",     0, 0, LO_LISTINCOMPLETE},
			{"listsafe",           0, 0, LO_LISTSAFE},
			{"listunsafe",         0, 0, LO_LISTUNSAFE},
			{"listused",           0, 0, LO_LISTUSED},
			{"load",               1, 0, LO_LOAD},
			{"markmixed",          0, 0, LO_MARKMIXED},
			{"no-generate",        0, 0, LO_NOGENERATE},
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
		case LO_GENERATE:
			app.opt_generate++;
			break;
		case LO_LISTINCOMPLETE:
			app.opt_listIncomplete++;
			app.opt_text++; // to test for output redirection
			break;
		case LO_LISTSAFE:
			app.opt_listSafe++;
			app.opt_text++; // to test for output redirection
			break;
		case LO_LISTUNSAFE:
			app.opt_listUnsafe++;
			app.opt_text++; // to test for output redirection
			break;
		case LO_LISTUSED:
			app.opt_listUsed++;
			app.opt_text++; // to test for output redirection
			break;
		case LO_LOAD:
			app.opt_load = optarg;
			break;
		case LO_MARKMIXED:
			app.opt_markMixed++;
			break;
		case LO_NOGENERATE:
			app.opt_generate = 0;
			break;
		case LO_TEXT:
			app.opt_text = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_text + 1;
			break;
		case LO_TRUNCATE:
			app.opt_truncate++;
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

	// display system flags when database was created
	if (ctx.flags & context_t::MAGICMASK_AINF && ctx.opt_verbose >= ctx.VERBOSE_WARNING)
		fprintf(stderr, "[%s] WARNING: add-if-not-found leaks false positives and is considered experimental\n", ctx.timeAsString());

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
	 * @date 2020-03-17 13:57:25
	 *
	 * Database indices are hashlookup tables with overflow.
	 * The art is to have a hash function that distributes evenly over the hashtable.
	 * If index entries are in use, then jump to overflow entries.
	 * The larger the index in comparison to the number of data entries the lower the chance an overflow will occur.
	 * The ratio between index and data size is called `ratio`.
	 */

	// prepare sections and indices for use
	uint32_t sections =  database_t::ALLOCMASK_SIGNATURE |  database_t::ALLOCMASK_SIGNATUREINDEX |
			     database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX |
		             database_t::ALLOCMASK_SWAP | database_t::ALLOCMASK_SWAPINDEX;

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
	 * Fast queries before re-building indices 
	 */
	
	/*
	 * @date 2021-07-26 02:14:33
	 *
	 * Create a list of "safe" signatures for `4n9-pure`.
	 * This will exclude signatures that have 7n1 members.
	 * Interesting will be how full-throttle normalising will rewrite using basic "QTF->QnTF" conversion
	 */
	if (app.opt_listSafe) {
		// list all safe signatures
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			signature_t *pSignature = db.signatures + iSid;

			if (pSignature->firstMember != 0 && (pSignature->flags & signature_t::SIGMASK_SAFE))
				app.signatureLine(pSignature);
		}
		exit(0);
	}
	if (app.opt_listUnsafe) {
		// list all signatures that are empty or unsafe
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			signature_t *pSignature = db.signatures + iSid;

			if (pSignature->firstMember == 0 || !(pSignature->flags & signature_t::SIGMASK_SAFE))
				app.signatureLine(pSignature);
		}
		exit(0);
	}

	/*
	 * @date 2021-08-06 11:58:26
	 * List signature still in use after `gendepreciate`
	 */
	if (app.opt_listUsed) {
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			signature_t *pSignature = db.signatures + iSid;

			if (pSignature->firstMember != 0)
				app.signatureLine(pSignature);
		}
		exit(0);
	}

	if (app.opt_listIncomplete) {
		// list sigatures used for lookups but are not SAFE
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			signature_t *pSignature = db.signatures + iSid;

			if ((pSignature->flags & signature_t::SIGMASK_KEY) && !(pSignature->flags & signature_t::SIGMASK_SAFE))
				app.signatureLine(pSignature);
		}
		exit(0);
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
	appSwap.connect(db);

	if (app.opt_load)
		app.signaturesFromFile();
	if (app.opt_generate) {
		if (app.arg_numNodes == 1) {
			// also include "0" and "a"
			app.arg_numNodes = 0;
			app.signaturesFromGenerator();
			app.arg_numNodes = 1;
		}
		app.signaturesFromGenerator();
	}

	/*
	 * sort signatures and ...
	 */

	if (!app.readOnlyMode) {
		// Sort signatures. This will invalidate index and imprints. hints are safe.

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Sorting signatures\n", ctx.timeAsString());

		assert(db.numSignature >= 1);
		qsort_r(db.signatures + 1, db.numSignature - 1, sizeof(*db.signatures), app.comparSignature, &app);

		// clear name index
		::memset(db.signatureIndex, 0, db.signatureIndexSize * sizeof(*db.signatureIndex));

		// reindex
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			// re-index name
			uint32_t ix = db.lookupSignature(db.signatures[iSid].name);
			assert(db.signatureIndex[ix] == 0);
			db.signatureIndex[ix] = iSid;
		}

		app.rebuildImprints();

		/*
		 * @date 2021-07-31 11:40:31
		 *
		 * Existing signatures may not change.
		 * That is, signatures are grouped by tree size, so is this run adds signatures with the same size as existing then...
		 * This happens when extending 4n9 with toplevel mixed.
		 * only sids change, tids remain unchanged
		 */
		bool changed = false;
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			if (strcmp(db.signatures[iSid].name, db.signatures[iSid].name) != 0) {
				changed = true;
				break;
			}
		}

		if (changed) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: Old signatures changed position, rebuilding.\n", ctx.timeAsString());

			/*
			 * Allocate and find initial mapping
			 */
			uint32_t *pNewSid = (uint32_t *) ctx.myAlloc("pNewSid", db.maxSignature, sizeof(*pNewSid));

			for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
				uint32_t ix = db.lookupSignature(db.signatures[iSid].name);
				pNewSid[iSid] = db.signatureIndex[ix];

				if (pNewSid[iSid] == 0) {
					// this run changed an old signature name, perform imprint lookup
					tinyTree_t tree(ctx);
					tree.loadStringFast(db.signatures[iSid].name);
					unsigned tid;
					db.lookupImprintAssociative(&tree, db.fwdEvaluator, db.revEvaluator, &pNewSid[iSid], &tid);
				}
				// this run changed
				assert(pNewSid[iSid] != 0);
			}

			/*
			 * Remap sids, this has no effect on the member index
			 */
			for (uint32_t iMember = db.IDFIRST; iMember < db.numMember; iMember++) {
				member_t *pMember = db.members + iMember;

				assert(pMember->sid < db.numSignature);

				pMember->sid = pNewSid[pMember->sid];
			}

			ctx.myFree("pNewSid", pNewSid);
		}

		/*
		 * @date 2021-10-24 10:54:53
		 * Before saving database, make sure the swap section is up-to-date
		 */
		appSwap.swapsFromSignatures();
	}

	/*
	 * List result
	 */

	if (app.opt_text == app.OPTTEXT_BRIEF) {
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			const signature_t *pSignature = db.signatures + iSid;

			app.signatureLine(pSignature);
		}
	}
	if (app.opt_text == app.OPTTEXT_VERBOSE) {
		for (uint32_t iSid = db.IDFIRST; iSid < db.numSignature; iSid++) {
			const signature_t *pSignature = db.signatures + iSid;

			printf("%u\t%s\t%u\t%u\t%u\t%u", iSid, pSignature->name, pSignature->size, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef);

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
			uint32_t size = ctx.nextPrime(db.numSignature * app.opt_ratio);
			if (db.signatureIndexSize > size)
				db.signatureIndexSize = size;
			size = ctx.nextPrime(db.numSwap * app.opt_ratio);
			if (db.swapIndexSize > size)
				db.swapIndexSize = size;
			size = ctx.nextPrime(db.numImprint * app.opt_ratio);
			if (db.imprintIndexSize > size)
				db.imprintIndexSize = size;

			db.rebuildIndices(database_t::ALLOCMASK_SIGNATUREINDEX | database_t::ALLOCMASK_SWAPINDEX | database_t::ALLOCMASK_IMPRINTINDEX);
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
