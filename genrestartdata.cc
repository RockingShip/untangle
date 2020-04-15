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
 *   `"./genrestart --text [--qntf] <numnode>"`
 *      to generate a textual list of candidates
 *
 * Selfcheck consists of brute-force checking windowing and restarting of a `3n9-QnTF` generator.
 * Or a simple query if `a `numNode` argument is supplied with optional `--qntf`.
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

	/**
	 * Constructor
	 */
	genrestartdataContext_t(context_t &ctx) : ctx(ctx) {
		// arguments and options
		arg_numNodes = 0;
	}

	/**
	 * @date 2020-03-19 20:58:57
	 *
	 * Main entrypoint
	 */
	void main(void) {
		// create generator
		generatorTree_t generator(ctx);

		printf("#ifndef _RESTARTDATA_H\n");
		printf("#define _RESTARTDATA_H\n");
		printf("\n");
		printf("#include <stdint.h>\n");
		printf("\n");

		uint32_t buildProgressIndex[tinyTree_t::TINYTREE_MAXNODES][2];

		printf("const uint64_t restartData[] = { 0,\n\n");
		generator.numFoundRestart = 1; // skip first zero

		// @formatter:off
		for (unsigned numArgs = 0; numArgs < tinyTree_t::TINYTREE_MAXNODES; numArgs++)
		for (int iQnTF = 1; iQnTF >= 0; iQnTF--) {
		// @formatter:on

			// mark section not in use
			buildProgressIndex[numArgs][iQnTF] = 0;

			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, iQnTF, numArgs);
			if (pMetrics) {
				if (pMetrics->noauto)
					continue; // skip automated handling

				buildProgressIndex[numArgs][iQnTF] = generator.numFoundRestart;

				// output section header
				printf("// %ld: numNode=%d qntf=%d \n", generator.numFoundRestart, numArgs, iQnTF);

				// apply settings
				generator.flags = (iQnTF) ? generator.flags | context_t::MAGICMASK_QNTF : generator.flags & ~context_t::MAGICMASK_QNTF;
				generator.initialiseGenerator();

				// clear tree
				generator.clearGenerator();

				ctx.setupSpeed(pMetrics->numProgress);
				ctx.tick = 0;

				// do not supply a callback so `generateTrees` is aware restart data is being created
				unsigned endpointsLeft = numArgs * 2 + 1;
				generator.generateTrees(numArgs, endpointsLeft, 0, 0, NULL, NULL);

				// was there any output
				if (buildProgressIndex[numArgs][iQnTF] != generator.numFoundRestart) {
					// yes, output section delimiter
					printf(" 0xffffffffffffffffLL,");
					generator.numFoundRestart++;

					// align
					while (generator.numFoundRestart % 8 != 1) {
						printf("0,");
						generator.numFoundRestart++;
					}

					printf("\n");
				} else {
					// no, erase index entry
					buildProgressIndex[numArgs][iQnTF] = 0;
				}

				if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
					fprintf(stderr, "\r\e[K");

				if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
					fprintf(stderr, "[%s] numSlot=%d qntf=%d numNode=%d numProgress=%ld\n",
					        ctx.timeAsString(), MAXSLOTS, iQnTF, numArgs, ctx.progress);
				}
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
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 */
	void foundTreeCandidate(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		/*
		 * Ticker
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			ctx.tick = 0;
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
	}

	/**
	 * @date 2020-03-24 13:22:57
 	 *
 	 * Display all candidates as passed to `foundTree()`
 	 *
 	 * Candidates with back-references might be found more than once.
 	 *
 	 * param {number} numNode - Tree size
 	 */
	void performListCandidates(database_t *pStore, unsigned numNode) {

		this->pStore = pStore;
		pStore->numSignature = 1; // skip mandatory zero entry

		/*
		 * Setup generator
		 */
		generatorTree_t generator(ctx);

		// clear tree
		generator.clearGenerator();

		// reset progress
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.opt_flags & context_t::MAGICMASK_QNTF, numNode);
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
			generator.generateTrees(numNode, endpointsLeft, 0, 0, static_cast<callable_t *>(this), (generatorTree_t::generateTreeCallback_t) &genrestartdataSelftest_t::foundTreeCandidate);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%d qntf=%d numNode=%d numProgress=%ld numCandidate=%d\n",
			        ctx.timeAsString(), MAXSLOTS, (ctx.opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0, numNode, ctx.progress, pStore->numSignature);
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
 * Construct a time themed prefix string for console logging
 *
 * @date 2020-03-18 18:08:48
 */
const char *timeAsString(void) {
	static char tstr[256];

	time_t t = time(0);
	struct tm *tm = localtime(&t);
	strftime(tstr, sizeof(tstr), "%F %T", tm);

	return tstr;
}

/**
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 * @date 2020-03-18 18:09:31
 */
void sigalrmHandler(int sig) {
	(void) sig; // trick compiler t see parameter is used

	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

/**
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 * @date  2020-03-19 20:02:40
 */
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s                  -- generate contents for \"restartdata.h\"\n", argv[0]);
	fprintf(stderr, "       %s --text <numnode> -- display all unique candidates with given node size\n", argv[0]);
//	fprintf(stderr, "       %s --selftest       -- Test prerequisites\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t-h --help                  This list\n");
		fprintf(stderr, "\t   --[no-]qntf             Enable QnTF-only mode [default=%s]\n", (ctx.opt_flags & context_t::MAGICMASK_QNTF) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --[no-]paranoid         Enable expensive assertions [default=%s]\n", (ctx.opt_flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                 Say more\n");
		fprintf(stderr, "\t   --selftest              Validate prerequisites\n");
		fprintf(stderr, "\t   --text                  Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>       Interval timer for verbose updates [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose               Say less\n");
	}
}

/**
 * Program main entry point
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 * @date   2020-03-18 18:13:24
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
			LO_NOQNTF,
			LO_PARANOID,
			LO_QNTF,
			LO_SELFTEST,
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
			{"no-qntf",     0, 0, LO_NOQNTF},
			{"paranoid",    0, 0, LO_PARANOID},
			{"qntf",        0, 0, LO_QNTF},
			{"quiet",       2, 0, LO_QUIET},
			{"selftest",    0, 0, LO_SELFTEST},
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
				ctx.opt_flags &= ~context_t::MAGICMASK_PARANOID;
				break;
			case LO_NOQNTF:
				ctx.opt_flags &= ~context_t::MAGICMASK_QNTF;
				break;
			case LO_PARANOID:
				ctx.opt_flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_QNTF:
				ctx.opt_flags |= context_t::MAGICMASK_QNTF;
				break;
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
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
	if (app.opt_text != 0) {
		// text mode
		if (argc - optind >= 1) {
			app.arg_numNodes = (uint32_t) strtoul(argv[optind++], NULL, 0);
		} else {
			usage(argv, false);
			exit(1);
		}
	} else {
		// regular mode
		if (argc - optind >= 0) {
		} else {
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

		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.opt_flags & context_t::MAGICMASK_QNTF, app.arg_numNodes);
		if (!pMetrics) {
			fprintf(stderr, "preset for numNode not found\n");
			exit(1);
		}

		pStore->maxSignature = pMetrics->numCandidate;
		pStore->signatureIndexSize = ctx.nextPrime(pStore->maxSignature * (METRICS_DEFAULT_RATIO / 10.0));

		pStore->create();
	}

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);
	if (ctx.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", ctx.totalAllocated);

	/*
	 * Invoke
	 */

	if (app.opt_selftest) {
		/*
		 * self tests
		 */
	} else if (app.opt_text) {
		/*
		 * list candidates
		 */

		app.performListCandidates(pStore, app.arg_numNodes);
	} else {
		/*
		 * regular mode
		 */

		printf("// generated by %s on \"%s\"\n\n", argv[0], timeAsString());

		app.main();
	}

	return 0;
}