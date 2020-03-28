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
#include "database.h" // use signatures as candidates
#include "metrics.h"

/**
 * @date 2020-03-19 20:20:53
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genrestartdataContext_t : context_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {number} size of structures used in this invocation
	unsigned arg_numNodes;

	/**
	 * Constructor
	 */
	genrestartdataContext_t() {
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
		generatorTree_t generator(*this);

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

			unsigned endpointsLeft = numArgs * 2 + 1;

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

				setupSpeed(pMetrics->numProgress);
				this->tick = 0;

				// do not supply a callback so `generateTrees` is aware restart data is being created
				generator.generateTrees(endpointsLeft, 0, 0, NULL, NULL);

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

				if (this->opt_verbose >= this->VERBOSE_TICK)
					fprintf(stderr, "\r\e[K");

				if (this->opt_verbose >= this->VERBOSE_SUMMARY) {
					fprintf(stderr, "[%s] numSlot=%d qntf=%d numNode=%d numProgress=%ld\n",
					        this->timeAsString(), MAXSLOTS, iQnTF, numArgs, this->progress);
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
	genrestartdataSelftest_t() : genrestartdataContext_t() {
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
	 * Found candidate. Treat as signature and count uniques
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {number} numPlaceholder - number of unique endpoints in tree
	 */
	void foundTreeCandidate(generatorTree_t &tree, const char *pName, unsigned numPlaceholder) {
		/*
		 * Ticker
		 */
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			if (progressHi) {
				int perSecond = this->updateSpeed();
				int eta = (int) ((progressHi - progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numCandidate=%d",
				        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, pStore->numSignature);
			} else {
				fprintf(stderr, "\r\e[K[%s] %lu |  numCandidate=%d",
				        timeAsString(), progress, pStore->numSignature);
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

			printf("%ld\t%s\t%d\t%d\n", progress, pName, tree.count - tinyTree_t::TINYTREE_NSTART, numPlaceholder);

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
		generatorTree_t generator(*this);

		// clear tree
		generator.clearGenerator();

		// reset progress
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF, numNode);
		this->setupSpeed(pMetrics ? pMetrics->numProgress : 0);
		this->tick = 0;

		/*
		 * Run generator
		 */

		if (numNode == 0) {
			generator.root = 0;
			foundTreeCandidate(generator, "0", 0);
			generator.root = 1;
			foundTreeCandidate(generator, "a", 1);
		} else {
			unsigned endpointsLeft = numNode * 2 + 1;
			generator.generateTrees(endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &genrestartdataSelftest_t::foundTreeCandidate);
		}

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%d qntf=%d numNode=%d numProgress=%ld numCandidate=%d\n",
			        this->timeAsString(), MAXSLOTS, (this->opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0, numNode, this->progress, pStore->numSignature);
	}

};

/*
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {genrestartdataContext_t} Application
 */
genrestartdataSelftest_t app;

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

	if (app.opt_timer) {
		app.tick++;
		alarm(app.opt_timer);
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
void usage(char *const *argv, bool verbose, const genrestartdataContext_t *args) {
	fprintf(stderr, "usage: %s                  -- generate contents for \"restartdata.h\"\n", argv[0]);
	fprintf(stderr, "       %s --text <numnode> -- display all unique candidates with given node size\n", argv[0]);
//	fprintf(stderr, "       %s --selftest       -- Test prerequisites\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t-h --help                  This list\n");
		fprintf(stderr, "\t   --[no-]qntf             Enable QnTF-only mode [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_QNTF) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --[no-]paranoid         Enable expensive assertions [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                 Say more\n");
		fprintf(stderr, "\t   --selftest              Validate prerequisites\n");
		fprintf(stderr, "\t   --text                  Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>       Interval timer for verbose updates [default=%d]\n", args->opt_timer);
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
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_HELP:
				usage(argv, true, &app);
				exit(0);
			case LO_NOPARANOID:
				app.opt_flags &= ~context_t::MAGICMASK_PARANOID;
				break;
			case LO_NOQNTF:
				app.opt_flags &= ~context_t::MAGICMASK_QNTF;
				break;
			case LO_PARANOID:
				app.opt_flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_QNTF:
				app.opt_flags |= context_t::MAGICMASK_QNTF;
				break;
			case LO_QUIET:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_verbose - 1;
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
				break;
			case LO_TEXT:
				app.opt_text++;
				break;
			case LO_TIMER:
				app.opt_timer = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_VERBOSE:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_verbose + 1;
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
			usage(argv, false, &app);
			exit(1);
		}
	} else {
		// regular mode
		if (argc - optind >= 0) {
		} else {
			usage(argv, false, &app);
			exit(1);
		}
	}

	/*
	 * register timer handler
	 */
	if (app.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(1);
	}


	/*
	 * create optional storage for `--text`
	 */
	database_t *pStore = NULL;

	if (app.opt_text) {
		// create database to detect duplicates
		pStore = new database_t(app);

		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, app.opt_flags & context_t::MAGICMASK_QNTF, app.arg_numNodes);
		if (!pMetrics) {
			fprintf(stderr, "preset for numNode not found\n");
			exit(1);
		}

		pStore->maxSignature = pMetrics->numCandidate;
		pStore->signatureIndexSize = app.nextPrime(pStore->maxSignature * (METRICS_DEFAULT_RATIO / 10.0));

		pStore->create();
	}

	/*
	 * Statistics
	 */

	if (app.opt_verbose >= app.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", app.timeAsString(), app.totalAllocated);
	if (app.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", app.totalAllocated);

	/*
	 * Invoke
	 */

	if (app.opt_text && isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

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