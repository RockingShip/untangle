//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-18 18:04:50
 *
 * `genrestartdata` fires up the generator and extracts some metrics.
 * It generates fully normalised and naturally ordered trees for further processing.
 * With this version, all calls to `foundTree()` are notation unique.
 *
 * Selfcheck consists of brute-force checking windowing and restarting of a `3n9-QnTF` generator.
 * Or a simple query if `a `numNode` argument is supplied with optional `--qntf`.
 * For the latter `--text` can also be supplied to display all trees caught by `foundTree()`
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

#include "generator.h"
#include "metrics.h"

/*
 * NOTE: in trying to avoid a separate program to just test windowing...
 *
 *  `"restartdata.h"` is only needed for testing the windowing functionality
 *
 * If you are not developing:
 *   File timestamps got messed up, probably after a git clone and make has deleted `"restartdata.h"`
 *   Recover `"restartdata.h"` and do a `"touch restartdata.h"`
 *   Continue with building
 *
 * Otherwise:
 *  - Create an empty `"restartdata.h"`
 *  - compile, run and redirect output to `"restartdata.h"`
 *  - compile again
 *
 *  OR: Remove the include and compile. This will disable the windowing selftest
 */
#include "restartdata.h"

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
	/// @var {number} non-zero if in `QnTF`-only mode
	unsigned arg_qntf;

	/**
	 * Constructor
	 */
	genrestartdataContext_t() {
		// arguments and options
		arg_numNodes = 0;
		arg_qntf = 0;
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
		for (arg_numNodes = 0; arg_numNodes < tinyTree_t::TINYTREE_MAXNODES; arg_numNodes++)
		for (arg_qntf = 1; arg_qntf != (unsigned)-1; arg_qntf--) {
		// @formatter:on

			// mark section not in use
			buildProgressIndex[arg_numNodes][arg_qntf] = 0;

			unsigned endpointsLeft = arg_numNodes * 2 + 1;

			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, arg_qntf, arg_numNodes);
			if (pMetrics) {
				buildProgressIndex[arg_numNodes][arg_qntf] = generator.numFoundRestart;

				// output section header
				printf("// %ld: numNode=%d qntf=%d \n", generator.numFoundRestart, arg_numNodes, arg_qntf);

				// clear tree
				generator.clearGenerator();

				// apply settings
				generator.flags = (arg_qntf) ? generator.flags | context_t::MAGICMASK_QNTF : generator.flags & ~context_t::MAGICMASK_QNTF;
				this->progressHi = pMetrics->numProgress;
				this->progress = 0;
				this->tick = 0;

				// do not supply a callback so `generateTrees` is aware restart data is being created
				// NOTE: `foundTree()` is called when `numNode` matches, restart data triggers when trees are exactly 2 nodes in size.
				generator.addCallback(NULL, NULL);
				generator.generateTrees(endpointsLeft, 0, 0);

				// was there any output
				if (buildProgressIndex[arg_numNodes][arg_qntf] != generator.numFoundRestart) {
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
					buildProgressIndex[arg_numNodes][arg_qntf] = 0;
				}

				if (this->opt_verbose >= this->VERBOSE_TICK)
					fprintf(stderr, "\r\e[K");

				if (this->opt_verbose >= this->VERBOSE_SUMMARY) {
					fprintf(stderr, "[%s] metricsGenerator_t { /*numSlots=*/%d, /*qntf=*/%d, /*numNodes=*/%d, /*numProgress=*/%ldLL}\n",
					        this->timeAsString(), MAXSLOTS, arg_qntf, arg_numNodes, this->progress);
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
	/// @var {number} --text, often used switch
	unsigned opt_text;
	/// @var {string[]} tree notation for `progress` points
	char **selftestResults;

	/**
	 * Constructor
	 */
	genrestartdataSelftest_t() : genrestartdataContext_t() {
		// arguments and options
		opt_selftest = 0;
		opt_text = 0;

		selftestResults = (char **) myAlloc("genrestartdataContext_t::selftestResults", 2000000, sizeof(*selftestResults));
	}

	/**
	 * Destructor
	 */
	~genrestartdataSelftest_t() {
		myFree("genrestartdataContext_t::selftestResults", selftestResults);

	}

	/**
	 * @date 2020-03-21 17:25:47
	 *
	 * Selftest windowing by calling the generator with windowLo/Hi for each possible tree
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 */
	void selftestFoundTreeCreate(generatorTree_t &tree) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			if (progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), tree.windowLo * 100.0 / progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %ld", timeAsString(), tree.windowLo);
			tick = 0;
		}

		assert(this->progress < 2000000);

		const char *pName = tree.encode(tree.root);

		// assert entry is unique
		if (selftestResults[this->progress] != NULL) {
			printf("{\"error\":\"entry not unique\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%ld}\n",
			       __FUNCTION__, selftestResults[this->progress], pName, this->progress);
			exit(1);
		}

		// populate entry
		selftestResults[this->progress] = ::strdup(pName);
	}

	/**
	 * @date 2020-03-21 17:31:46
	 *
	 * Selftest windowing by calling generator without a window and test if results match.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 */
	void selftestFoundTreeVerify(generatorTree_t &tree) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			if (progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), tree.windowLo * 100.0 / progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %ld", timeAsString(), tree.windowLo);
			tick = 0;
		}

		assert(this->progress < 2000000);

		const char *pName = tree.encode(tree.root);

		// assert entry is present
		if (selftestResults[this->progress] == NULL) {
			printf("{\"error\":\"missing\",\"where\":\"%s\",\"expected\":\"%s\",\"progress\":%ld}\n",
			       __FUNCTION__, pName, this->progress);
			exit(1);
		}

		// compare
		if (::strcmp(pName, selftestResults[this->progress]) != 0) {
			printf("{\"error\":\"entry missmatch\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%ld}\n",
			       __FUNCTION__, selftestResults[this->progress], pName, this->progress);
			exit(1);
		}

		// release resources
		::free(selftestResults[this->progress]);
		selftestResults[this->progress] = NULL;
	}

	/**
	 * @date 2020-03-21 17:36:59
	 *
	 * Selftest windowing
	 */
	void selftestWindow(void) {
		// set generator into `3n9 QnTF` mode
		arg_qntf = 1;
		arg_numNodes = 3;

		generatorTree_t generator(*this);

		// find metrics for setting
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, arg_qntf, arg_numNodes);
		assert(pMetrics);

		unsigned endpointsLeft = pMetrics->numNodes * 2 + 1;

		/*
		 * Pass 1, slice dataset into single entries
		 */
		if (this->opt_verbose >= this->VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Trying all window settings for %dn9%s\n", timeAsString(), pMetrics->numNodes, pMetrics->qntf ? "-QnTF" : "");

		for (uint64_t windowLo = 0; windowLo < pMetrics->numProgress; windowLo++) {
			// clear tree
			generator.clearGenerator();

			// apply settings
			generator.flags = (pMetrics->qntf) ? generator.flags | context_t::MAGICMASK_QNTF : generator.flags & ~context_t::MAGICMASK_QNTF;
			generator.windowLo = windowLo;
			generator.windowHi = windowLo + 1;
#if defined(_RESTARTDATA_H)
			generator.pRestartData = restartData + restartIndex[pMetrics->numNodes][pMetrics->qntf];
#endif
			this->progressHi = pMetrics->numProgress;
			this->progress = 0;
			this->tick = 0;

			generator.addCallback(this, (void (context_t::*)(generatorTree_t &)) &genrestartdataSelftest_t::selftestFoundTreeCreate);
			generator.generateTrees(endpointsLeft, 0, 0);
		}

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * Pass 2, validate entries
		 */
		if (this->opt_verbose >= this->VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Verifying results against non-windowed\n", timeAsString());

		{
			// clear tree
			generator.clearGenerator();

			// apply settings
			generator.flags = (pMetrics->qntf) ? generator.flags | context_t::MAGICMASK_QNTF : generator.flags & ~context_t::MAGICMASK_QNTF;
			generator.windowLo = 0;
			generator.windowHi = 0;
#if defined(_RESTARTDATA_H)
			generator.pRestartData = restartData + restartIndex[pMetrics->numNodes][pMetrics->qntf];
#endif
			this->progressHi = pMetrics->numProgress;
			this->progress = 0;
			this->tick = 0;

			generator.addCallback(this, (void (context_t::*)(generatorTree_t &)) &genrestartdataSelftest_t::selftestFoundTreeVerify);
			generator.generateTrees(endpointsLeft, 0, 0);
		}

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed\n", this->timeAsString(), __FUNCTION__);
	}

	/**
	 * @date 2020-03-18 22:17:26
	 *
	 * found candidate.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 */
	void selftestFoundTreePrint(generatorTree_t &tree) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			if (progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), progress * 100.0 / progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %ld", timeAsString(), progress);
			tick = 0;
		}

		/*
		 * Debug mode used to create restart metrics and dump generated trees
		 */
		if (opt_text) {
#if 1
			// display candidate
			printf("%s\n", tree.encode(tree.root));
#else
			// simple tree dump for the very paranoia
			for (unsigned i = TINYTREE_NSTART; i < this->count; i++) {
				uint32_t qtf = packedN[i];
				printf("%d%x%x%x ", (qtf & PACKED_TIBIT) ? 1 : 0, (qtf >> PACKED_QPOS) & PACKED_MASK, (qtf >> PACKED_TPOS) & PACKED_MASK, (qtf >> PACKED_FPOS) & PACKED_MASK);
			}
			printf("\n");
#endif
		}
	}

	/**
	 * @date 2020-03-21 20:09:49
 	 *
 	 * Selftest windowing
 	 */
	void selftest(void) {
		/*
		 * Expecting a lot of output, redirect to a file or kill the screen
		 */

		if (this->opt_text && isatty(1)) {
			fprintf(stderr, "stdout not redirected\n");
			exit(1);
		}

		generatorTree_t generator(*this);

		/*
		 * quickly test that `tinyTree_t` does level-2 normalisation
		 */
		{
			generator.decodeSafe("ab>ba+^");
			const char *pName = generator.encode(generator.root);
			assert(::strcmp(pName, "ab+ab>^") == 0);
		}

		// find metrics for setting
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, arg_qntf, arg_numNodes);
		unsigned endpointsLeft = arg_numNodes * 2 + 1;

		/*
		 * Current version
		 */
		// clear tree
		generator.clearGenerator();

		// reset progress
#if defined(_RESTARTDATA_H)
		if (pMetrics)
			generator.pRestartData = restartData + restartIndex[pMetrics->numNodes][pMetrics->qntf];
#endif
		this->progressHi = pMetrics ? pMetrics->numProgress : 0;
		this->progress = 0;
		this->tick = 0;

		generator.addCallback(this, (void (context_t::*)(generatorTree_t &)) &genrestartdataSelftest_t::selftestFoundTreePrint);
		generator.generateTrees(endpointsLeft, 0, 0);

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (this->progress != this->progressHi) {
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%ld,\"expected\":%ld,\"numNode\":%d}\n",
			       __FUNCTION__, this->progress, this->progressHi, this->arg_numNodes);
			exit(1);
		}

		fprintf(stderr, "[%s] foundTree() for numNode=%d called %ld times\n", this->timeAsString(), this->arg_numNodes, this->progress);
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
	fprintf(stderr, "usage:\t%s\n\t%s --selftest [<numnode>]\n", argv[0], argv[0]);
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
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
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
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose - 1;
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
				break;
			case LO_TEXT:
				app.opt_text++;
				break;
			case LO_TIMER:
				app.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
				break;
			case LO_VERBOSE:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose + 1;
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
	if (!app.opt_selftest) {
		// regular mode
		if (argc - optind >= 0) {
		} else {
			usage(argv, false, &app);
			exit(1);
		}
	} else {
		// selftest mode
		if (argc - optind >= 0) {
			// optional argument
			if (argc - optind >= 1)
				app.arg_numNodes = (uint32_t) strtoul(argv[optind++], NULL, 10);
			else
				app.arg_numNodes = 0;
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
	 * Test
	 */
	if (app.opt_selftest) {
#if !defined(_RESTARTDATA_H)
		fprintf(stderr, "[%s] WARNING: \"restartdata.h\" not found or empty. Windowing selftest with restart disabled\n", this->timeAsString());
#endif

		if (app.arg_numNodes == 0) {
			// call default selftest
			app.selftestWindow();
		} else {
			// call selftest for specific setting
			app.selftest();
		}
		exit(0);
	}

	/*
	 * Invoke current code
	 */

	/*
	 * Invoke main entrypoint of application context
	 */

	if (isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

	printf("// generated by %s on \"%s\"\n\n", argv[0], timeAsString());

	app.main();

	return 0;
}