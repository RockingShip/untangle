//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-18 18:04:50
 *
 * `genrestartdata` fires up the generator and extracts some metrics.
 * It generates fully normalised and naturally ordered trees for further processing.
 * With this version, all calls to `foundTree()` are notation unique.
 *
 * Usage:
 *   `"./genrestart"`
 *      to generate `restartdata.h`
 *   `"./genrestart --text [--pure] <numnode>"`
 *      to generate a textual list of candidates
 *
 * Selfcheck consists of brute-force checking windowing and restarting of a `3n9-pure` generator.
 * Or a simple query if `a `numNode` argument is supplied with optional `--pure`.
 * For the latter `--text` can also be supplied to display all trees caught by `foundTree()`
 *
 * @date 2020-03-27 00:18:13
 *
 * Textual output:
 *   <candidateId> <name> <numNode> <numPlaceholder>
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <getopt.h>
#include "tinytree.h"
#include "generator.h"
#include "database.h"
#include "metrics.h"

/**
 * @date 2020-03-19 20:20:53
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genrestartdataContext_t : callable_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {copntext_t} I/O context
	context_t &ctx;

	/// @var {number} size of structures used in this invocation
	unsigned arg_numNodes;

	/// @var {number} task Id. First task=1
	unsigned opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned opt_taskLast;

	/// @var {number} Number of restart entries found
	uint32_t numRestart;

	/// @var {number} - THE generator
	generatorTree_t generator;

	/**
	 * Constructor
	 */
	genrestartdataContext_t(context_t &ctx) : ctx(ctx), generator(ctx) {
		// arguments and options
		arg_numNodes = 0;

		opt_taskId = 0;
		opt_taskLast = 0;

		numRestart = 0;
	}

	/**
	 * @date 2020-03-24 13:20:42
	 *
	 * Found restart tab, Simple count how often
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeRestartTab(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		/*
		 * Simply count how often called
		 */
		numRestart++;

		// counting tabs, no recursion
		return false;
	}

	/**
	 * @date 2020-04-16 00:02:10
	 *
	 * Determine mow many restart tabs current settings has.
	 */
	unsigned countRestartTabs(void) {

		// put generator in `genrestartdata` mode
		ctx.opt_debug |= context_t::DEBUGMASK_GENERATOR_TABS;
		numRestart = 0;

		/*
		 * Run generator
		 */

		unsigned numNode = 1 + generator.restartTabDepth - tinyTree_t::TINYTREE_NSTART; // one level deeper than `restartTabDepth`
		unsigned endpointsLeft = numNode * 2 + 1;

		generator.clearGenerator();
		generator.generateTrees(numNode, endpointsLeft, 0, 0, this, static_cast<generatorTree_t::generateTreeCallback_t>(&genrestartdataContext_t::foundTreeRestartTab));

		return numRestart;
	}

	/**
	 * @date 2020-03-24 13:20:42
	 *
	 * Decide which restart tab to process or not
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeFilterTab(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {


		/*
		 * numRestart start at zero, opt_taskId at 1
		 */
		numRestart++;

		if (numRestart == this->opt_taskId)
			return true; // active tab, recurse
		else
			return false; // inactive tab
	}

	/**
	 * @date 2020-04-16 11:59:53
	 *
	 * Count and output number of raw trees per restart tab.
	 */
	void mainTask(void) {

		/*
		 * Check that `taskLast` is correct
		 */
		unsigned numTabs = this->countRestartTabs();

		if (numRestart != this->opt_taskLast) {
			printf("{\"error\":\"taskLast does not match number of restart tabs\",\"where\":\"%s\",\"encountered\":\"%u\",\"expected\":\"%u\"\n",
			       __FUNCTION__, this->opt_taskLast, numTabs);
			exit(1);
		}

		/*
		 * Setup generator
		 */

		// put generator in `genrestartdata` mode
		ctx.opt_debug |= context_t::DEBUGMASK_GENERATOR_TABS;

		/*
		 * Run generator to count number of restart tabs
		 */

		numRestart = 0;
		unsigned endpointsLeft = this->arg_numNodes * 2 + 1;

		generator.clearGenerator();
		generator.generateTrees(this->arg_numNodes, endpointsLeft, 0, 0, this, static_cast<generatorTree_t::generateTreeCallback_t>(&genrestartdataContext_t::foundTreeRestartTab));


		printf("called %u times\n", numRestart);
	}

	/**
	 * @date 2020-04-16 10:10:46
	 *
	 * Found restart tab, Output restart entry
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreePrintTab(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		/*
		 * Simply count how often called
		 */
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

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d",
				        ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS);
			}

			ctx.tick = 0;
		}

		// tree is incomplete and requires a slightly different notation
		printf("%12ldLL/*", ctx.progress);
		for (uint32_t iNode = tinyTree_t::TINYTREE_NSTART; iNode < tree.count; iNode++) {
			uint32_t qtf = tree.packedN[iNode];
			uint32_t Q = (qtf >> generatorTree_t::PACKED_QPOS) & generatorTree_t::PACKED_MASK;
			uint32_t To = (qtf >> generatorTree_t::PACKED_TPOS) & generatorTree_t::PACKED_MASK;
			uint32_t F = (qtf >> generatorTree_t::PACKED_FPOS) & generatorTree_t::PACKED_MASK;
			uint32_t Ti = (qtf & generatorTree_t::PACKED_TIMASK) ? 1 : 0;

			if (Q >= tinyTree_t::TINYTREE_NSTART)
				putchar("123456789"[Q - tinyTree_t::TINYTREE_NSTART]);
			else
				putchar("0abcdefghi"[Q]);
			if (To >= tinyTree_t::TINYTREE_NSTART)
				putchar("123456789"[To - tinyTree_t::TINYTREE_NSTART]);
			else
				putchar("0abcdefghi"[To]);
			if (F >= tinyTree_t::TINYTREE_NSTART)
				putchar("123456789"[F - tinyTree_t::TINYTREE_NSTART]);
			else
				putchar("0abcdefghi"[F]);
			putchar(Ti ? '!' : '?');
		}
		printf("*/,");

		// `genprogress` needs to know how many restart points are generated.
		this->numRestart++;

		if (this->numRestart % 8 == 1)
			printf("\n");

		// collecting restartdata, so continue with recursion
		return true;
	}

	/**
	 * @date 2020-03-19 20:58:57
	 *
	 * Main entrypoint
	 */
	void main(void) {

		// put generator in `genrestartdata` mode
		ctx.opt_debug |= context_t::DEBUGMASK_GENERATOR_TABS;

		printf("#ifndef _RESTARTDATA_H\n");
		printf("#define _RESTARTDATA_H\n");
		printf("\n");
		printf("#include <stdint.h>\n");
		printf("\n");

		uint32_t buildProgressIndex[tinyTree_t::TINYTREE_MAXNODES][2];

		printf("const uint64_t restartData[] = { 0,\n\n");
		this->numRestart = 1; // skip first zero

		// @formatter:off
		for (unsigned numArgs = 0; numArgs < tinyTree_t::TINYTREE_MAXNODES; numArgs++)
		for (int iPure = 1; iPure >= 0; iPure--) {
		// @formatter:on

			// mark section not in use
			buildProgressIndex[numArgs][iPure] = 0;

			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, iPure, numArgs);
			if (pMetrics) {
				if (pMetrics->noauto)
					continue; // skip automated handling

				buildProgressIndex[numArgs][iPure] = this->numRestart;

				// output section header
				printf("// %d: numNode=%d pure=%d \n", this->numRestart, numArgs, iPure);

				// apply settings
				ctx.flags = iPure ? ctx.flags | context_t::MAGICMASK_PURE : ctx.flags & ~context_t::MAGICMASK_PURE;
				generator.initialiseGenerator();

				ctx.setupSpeed(pMetrics->numProgress);
				ctx.tick = 0;

				// do not supply a callback so `generateTrees` is aware restart data is being created
				unsigned endpointsLeft = numArgs * 2 + 1;

				generator.clearGenerator();
				generator.generateTrees(numArgs, endpointsLeft, 0, 0, this, static_cast<generatorTree_t::generateTreeCallback_t>(&genrestartdataContext_t::foundTreePrintTab));

				// was there any output
				if (buildProgressIndex[numArgs][iPure] != this->numRestart) {
					// yes, output section delimiter
					printf(" 0xffffffffffffffffLL,");
					this->numRestart++;

					// align
					while (this->numRestart % 8 != 1) {
						printf("0,");
						this->numRestart++;
					}

					printf("\n");
				} else {
					// no, erase index entry
					buildProgressIndex[numArgs][iPure] = 0;
				}

				if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
					fprintf(stderr, "\r\e[K");

				if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
					fprintf(stderr, "[%s] numSlot=%d pure=%d numNode=%d numProgress=%ld\n",
					        ctx.timeAsString(), MAXSLOTS, iPure, numArgs, ctx.progress);
			}
		}

		printf("};\n\n");

		/*
		 * Output index
		 */

		printf("const uint32_t restartIndex[%d][2] = {\n", tinyTree_t::TINYTREE_MAXNODES);

		for (unsigned numNode = 0; numNode < tinyTree_t::TINYTREE_MAXNODES; numNode++) {
			printf("\t{ %8d, %8d },\n", buildProgressIndex[numNode][0], buildProgressIndex[numNode][1]);
		}

		printf("};\n\n");
		printf("#endif\n");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Done\n", ctx.timeAsString());

	}

};

/**
 * @date 2020-03-19 20:20:53
 *
 * Selftest wrapper
 *
 * @typedef {object}
 */
struct genrestartdataSelftest_t : genrestartdataContext_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {number} --selftest, perform a selftest
	unsigned opt_selftest;
	/// @var {number} --text, text mode, list candidates
	unsigned opt_text;

	/// @var {database_t} - Database store to place results
	database_t *pStore;

	/**
	 * Constructor
	 */
	genrestartdataSelftest_t(context_t &ctx) : genrestartdataContext_t(ctx) {
		// arguments and options
		opt_selftest = 0;
		opt_text = 0;
		pStore = NULL;
	}

	/**
	 * Destructor
	 */
	~genrestartdataSelftest_t() {
	}

	/**
	 * @date 2020-03-24 13:20:42
	 *
	 * Found candidate, count uniques
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeCandidate(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		/*
		 * Ticker
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) |  numCandidate=%d",
				        ctx.timeAsString(), ctx.progress, perSecond, pStore->numSignature);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numCandidate=%d",
				        ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, pStore->numSignature);
			}

			ctx.tick = 0;
		}

		/*
		 * Test that the tree and name match
		 */
		const char *pTestName = tree.encode(tree.root);
		if (strcmp(pName, pTestName) != 0) {
			printf("{\"error\":\"tree doesn't match name\",\"where\":\"%s\",\"tree\":\"%s\",\"name\":\"%s\"}\n",
			       __FUNCTION__, pTestName, pName);
			exit(1);
		}

		/*
		 * Got candidate
		 */

		// lookup..
		uint32_t ix = pStore->lookupSignature(pName);

		// ...and add if not found
		if (pStore->signatureIndex[ix] == 0) {

			printf("%ld\t%s\t%d\t%d\n", ctx.progress, pName, tree.count - tinyTree_t::TINYTREE_NSTART, numPlaceholder);

			pStore->signatureIndex[ix] = pStore->addSignature(pName);
		}

		return true;
	}

	/**
	 * @date 2020-03-24 13:22:57
 	 *
 	 * Display all candidates as passed to `foundTree()`
 	 *
 	 * Candidates with back-references might be found more than once.
 	 *
 	 * param {database_t} pStore - database fo store candidate and reject duplicates
 	 * param {number} numNode - Tree size
 	 */
	void performListCandidates(database_t *pStore, unsigned numNode) {

		this->pStore = pStore;
		pStore->numSignature = 1; // skip reserved entry

		// reset progress
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, numNode);
		ctx.setupSpeed(pMetrics ? pMetrics->numProgress : 0);
		ctx.tick = 0;

		/*
		 * Run generator
		 */

		if (numNode == 0) {
			generator.root = 0;
			foundTreeCandidate(generator, "0", 0, 0, 0);
			generator.root = 1;
			foundTreeCandidate(generator, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = numNode * 2 + 1;

			generator.clearGenerator();
			generator.generateTrees(numNode, endpointsLeft, 0, 0, this, static_cast<generatorTree_t::generateTreeCallback_t>(&genrestartdataSelftest_t::foundTreeCandidate));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%d pure=%d numNode=%d numProgress=%ld numCandidate=%d\n",
			        ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, numNode, ctx.progress, pStore->numSignature);
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
 * @global {genrestartdataSelftest_t} Application context
 */
genrestartdataSelftest_t app(ctx);

/**
 * @date 2020-03-18 18:08:48
 *
 * Construct a time themed prefix string for console logging
 */
const char *timeAsString(void) {
	static char tstr[256];

	time_t t = time(0);
	struct tm *tm = localtime(&t);
	strftime(tstr, sizeof(tstr), "%F %T", tm);

	return tstr;
}

/**
 * @date 2020-03-18 18:09:31
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int sig) {
	(void) sig; // trick compiler t see parameter is used

	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

/**
 * @date 2020-03-19 20:02:40
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s                  -- generate contents for \"restartdata.h\"\n", argv[0]);
	fprintf(stderr, "       %s --text <numnode> -- display all unique candidates with given node size\n", argv[0]);
	fprintf(stderr, "       %s --task=n,m <numnode> -- display single line for requested task/tab\n", argv[0]);
//	fprintf(stderr, "       %s --selftest       -- Test prerequisites\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t-h --help                  This list\n");
		fprintf(stderr, "\t-q --[no-]paranoid         Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure             Enable QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                 Say more\n");
		fprintf(stderr, "\t   --selftest              Validate prerequisites\n");
		fprintf(stderr, "\t   --sge                   Get SGE task settings from environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>      Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                  Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>       Interval timer for verbose updates [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose               Say less\n");
	}
}

/**
 * @date 2020-03-18 18:13:24
 *
 * Program main entry point
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
			LO_ANCIENT = 1,
			LO_DEBUG,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_PARANOID,
			LO_PURE,
			LO_SELFTEST,
			LO_SGE,
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
			{"debug",       1, 0, LO_DEBUG},
			{"help",        0, 0, LO_HELP},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"no-pure",     0, 0, LO_NOPURE},
			{"paranoid",    0, 0, LO_PARANOID},
			{"pure",        0, 0, LO_PURE},
			{"quiet",       2, 0, LO_QUIET},
			{"selftest",    0, 0, LO_SELFTEST},
			{"sge",         0, 0, LO_SGE},
			{"task",        1, 0, LO_TASK},
			{"text",        0, 0, LO_TEXT},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",     2, 0, LO_VERBOSE},
			//
			{NULL,          0, 0, 0}
		};

		char optstring[128], *cp;
		cp = optstring;

		/* construct optarg */
		for (int i = 0; long_options[i].name; i++) {
			if (::isalpha(long_options[i].val)) {
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
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
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
			case LO_TASK:
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
				break;
			case LO_TEXT:
				app.opt_text++;
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
			app.arg_numNodes = (uint32_t) strtoul(argv[optind++], NULL, 0);

	if (app.opt_text != 0 || app.opt_taskLast != 0) {
		if (app.arg_numNodes == 0) {
			usage(argv, false);
			exit(1);
		}
	}

	if (app.opt_text && isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

	/*
	 * register timer handler
	 */
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(1);
	}


	/*
	 * create optional storage for `--text`
	 */
	database_t *pStore = NULL;

	if (app.opt_text) {
		// create database to detect duplicates
		pStore = new database_t(ctx);

		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, app.arg_numNodes);
		if (!pMetrics) {
			fprintf(stderr, "preset for numNode not found\n");
			exit(1);
		}

		pStore->maxSignature = pMetrics->numCandidate;
		pStore->signatureIndexSize = ctx.nextPrime(pStore->maxSignature * (METRICS_DEFAULT_RATIO / 10.0));

		pStore->create(0);
	}

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);

	/*
	 * Invoke
	 */

	if (app.opt_selftest) {
		/*
		 * self tests
		 */

		fprintf(stderr, "no selftest available\n");
		exit(1);

	} else if (app.opt_text) {
		/*
		 * list candidates
		 */

		app.performListCandidates(pStore, app.arg_numNodes);
	} else {
		/*
		 * regular mode
		 */

		// output comment here because of `argv[]`
		printf("// generated by %s on \"%s\"\n\n", argv[0], timeAsString());

		app.main();
	}

	return 0;
}