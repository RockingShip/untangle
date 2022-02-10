#pragma GCC optimize ("O0") // optimize on demand

/*
 * validate.cc
 * 	validate a `baseTree` file against the tests stored in the matching json
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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <stdlib.h>
#include <unistd.h>
#include <map>

#include "context.h"
#include "basetree.h"

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

/**
 * @date 2021-05-17 22:45:37
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
 * @date 2021-05-13 15:30:14
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct validateContext_t {

	/// @var {number} --onlyifset, only validate non-zero root (consider them a cascading of OR intermediates)
	unsigned opt_onlyIfSet;

	/// @var {baseTree_t*} input tree
	baseTree_t *pInputTree;

	std::vector<std::string>        testNames;   // The names of the entries
	std::map<std::string, unsigned> testLookup;  // Name lookup	
	std::vector<uint32_t>           testData;    // Test values
	std::vector<uint32_t>           entryMap;    // How tree entrypoints map onto test data
	std::vector<uint32_t>           rootMap;     // How tree roots map onto test data

	// test data
	unsigned gNumTests;

	validateContext_t() {
		opt_onlyIfSet = 0;
		pInputTree    = NULL;

		// test data
		gNumTests  = 0;
	}


	/*
	 * @date 2021-05-13 16:40:18
	 *
	 * Load dimensions and metrics from json
	 */
	void main(const char *jsonFilename, const char *treeFilename) {
		/*
		 * Load json
		 */

		FILE *f = fopen(jsonFilename, "r");

		if (!f) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("fopen()"));
			json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
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
			json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
			json_object_set_new_nocheck(jError, "line", json_integer(jLoadError.line));
			json_object_set_new_nocheck(jError, "text", json_string(jLoadError.text));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}
		fclose(f);

		/*
		 * Create an incomplete tree based on json
		 */
		baseTree_t jsonTree(ctx);

		jsonTree.loadFileJson(jInput, jsonFilename);

		/*
		 * load tree
		 */

		baseTree_t tree(ctx);

		if (tree.loadFile(treeFilename)) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to load"));
			json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(treeFilename));
			tree.headerInfo(jResult);
			tree.extraInfo(jResult);
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
			json_delete(jResult);
		}

		if (tree.flags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			ctx.logFlags(tree.flags);

		/*
		 * Determine json entry/root names
		 */

		for (unsigned iEntry = jsonTree.kstart; iEntry < jsonTree.nstart; iEntry++) {
			std::string name = jsonTree.entryNames[iEntry - jsonTree.kstart];

			testLookup[name] = testNames.size();
			testNames.push_back(name);
		}
		for (unsigned iRoot = 0; iRoot < jsonTree.numRoots; iRoot++) {
			std::string name = jsonTree.rootNames[iRoot];

			testLookup[name] = testNames.size();
			testNames.push_back(name);
		}

		/*
		 * Now map the tree entry/roots onto the json 
		 */
		entryMap.resize(tree.nstart);
		for (unsigned iEntry = tree.kstart; iEntry < tree.nstart; iEntry++) {
			std::string name = tree.entryNames[iEntry - tree.kstart];
			std::map<std::string, unsigned>::iterator it;

			it = testLookup.find(name);
			if (it == testLookup.end()) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("entryName not found"));
				json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
				json_object_set_new_nocheck(jError, "name", json_string(name.c_str()));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			// make connection
			entryMap[iEntry] = it->second;
		}

		if (!(tree.flags & context_t::MAGICMASK_SYSTEM)) {
			rootMap.resize(tree.numRoots);
			for (unsigned iRoot = 0; iRoot < tree.numRoots; iRoot++) {
				std::string name = tree.rootNames[iRoot];
				std::map<std::string, unsigned>::iterator it;

				it = testLookup.find(name);
				if (it == testLookup.end()) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("rootName not found"));
					json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
					json_object_set_new_nocheck(jError, "name", json_string(name.c_str()));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				// make connection
				rootMap[iRoot] = it->second;
			}
		}

		/*
		 * Load and perform tests
		 */

		json_t *jTests = json_object_get(jInput, "tests");
		gNumTests = json_array_size(jTests);
		if (!gNumTests) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag 'tests'"));
			json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		/*
		 * Perform tests
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
			json_t *jList = json_array();

			/*
			 * Which roots are not keys
			 */
			for (unsigned iRoot = 0; iRoot < tree.numRoots; iRoot++) {
				json_array_append_new(jList, json_string_nocheck(tree.rootNames[iRoot].c_str()));
			}

			fprintf(stderr, "Validating: %s\n", json_dumps(jList, JSON_PRESERVE_ORDER | JSON_COMPACT));
			json_delete(jList);
		}

		uint32_t *pEval = tree.allocMap(); // evaluation stack

		// convert ascii to hex and inject at the appropriate location
		for (unsigned iTest = 0; iTest < gNumTests; iTest++) {
			/*
			 * Load/decode the test 
			 */

			// extract test element
			json_t     *jTest    = json_array_get(jTests, iTest);
			const char *strEntry = json_string_value(json_array_get(jTest, 0));
			const char *strRoots = json_string_value(json_array_get(jTest, 1));

			// simple validation
			if (!strEntry || !strRoots) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incomplete test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			/*
			 * decode entrypoint/root data
			 */
			unsigned iBit   = 0;
			testData.resize(testNames.size());

			// convert
			for (unsigned iPos = 0; iPos < strlen(strEntry); /* increment in loop */ ) {
				// skip spaces
				if (isspace(strEntry[iPos])) {
					iPos++;
					continue;
				}

				unsigned byte = 0;

				for (int iNibble = 0; iNibble < 2; iNibble++) {
					char ch = strEntry[iPos++]; // loop decrement happens here

					byte *= 16;

					if (ch >= '0' && ch <= '9')
						byte += ch - '0';
					else if (ch >= 'A' && ch <= 'F')
						byte += ch - 'A' + 10;
					else if (ch >= 'a' && ch <= 'f')
						byte += ch - 'a' + 10;
					else {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("bad entry data in test entry"));
						json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
						json_object_set_new_nocheck(jError, "test", json_integer(iTest));
						json_object_set_new_nocheck(jError, "key-data", json_string_nocheck(strEntry));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
				}

				for (unsigned k = 0; k < 8; k++) {
					if (iBit < jsonTree.nstart - jsonTree.kstart)
						testData[iBit++] = (byte & (1 << k)) ? ~0U : 0U;
				}
			}

			if (iBit < jsonTree.nstart - jsonTree.kstart) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("entry data too short in test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				json_object_set_new_nocheck(jError, "expected", json_integer(jsonTree.nstart - jsonTree.kstart));
				json_object_set_new_nocheck(jError, "encountered", json_integer(iBit));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			// convert
			for (unsigned iPos = 0; iPos < strlen(strRoots); /* increment in loop */ ) {
				// skip spaces
				if (isspace(strRoots[iPos])) {
					iPos++;
					continue;
				}

				unsigned byte = 0;

				for (int iNibble = 0; iNibble < 2; iNibble++) {
					char ch = strRoots[iPos++]; // loop increment happens here

					byte *= 16;

					if (ch >= '0' && ch <= '9')
						byte += ch - '0';
					else if (ch >= 'A' && ch <= 'F')
						byte += ch - 'A' + 10;
					else if (ch >= 'a' && ch <= 'f')
						byte += ch - 'a' + 10;
					else {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("bad root data in test entry"));
						json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
						json_object_set_new_nocheck(jError, "test", json_integer(iTest));
						json_object_set_new_nocheck(jError, "root-data", json_string_nocheck(strRoots));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
				}

				for (unsigned k = 0; k < 8; k++) {
					if (iBit < tree.nstart - tree.kstart + tree.numRoots)
						testData[iBit++] = (byte & (1 << k)) ? ~0U : 0U;
				}
			}

			if (iBit < tree.numRoots) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("root data too short in test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				json_object_set_new_nocheck(jError, "expected", json_integer(tree.numRoots));
				json_object_set_new_nocheck(jError, "numroots", json_integer(iBit));
				json_object_set_new_nocheck(jError, "encountered", json_integer(iBit));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			/*
			 * Prepare evaluator
			 */

			// set everything to an invalidation marker
			for (unsigned iNode = 0; iNode < tree.ncount; iNode++)
				pEval[iNode] = 0x5a5a5a5a; // set to invalid value

			pEval[0] = 0; // only zero is defined

			for (unsigned iEntry = tree.kstart; iEntry < tree.nstart; iEntry++)
				pEval[iEntry] = testData[entryMap[iEntry]];

			/*
			 * Run the test
			 */
			for (uint32_t iNode = tree.nstart; iNode < tree.ncount; iNode++) {
				const baseNode_t *pNode = tree.N + iNode;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   F      = pNode->F;

				// test range
				if (Q >= tree.ncount || Tu >= tree.ncount || F >= tree.ncount) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node references out-of-range"));
					json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "nid", json_integer(iNode));
					json_t *jNode = json_object();
					json_object_set_new_nocheck(jNode, "q", json_integer(Q));
					json_object_set_new_nocheck(jNode, "ti", json_integer(Ti ? 1 : 0));
					json_object_set_new_nocheck(jNode, "tu", json_integer(Tu));
					json_object_set_new_nocheck(jNode, "f", json_integer(F));
					json_object_set_new_nocheck(jError, "node", jNode);
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				// test for undefined
				if ((pEval[Q] != 0 && pEval[Q] != ~0U) || (pEval[Tu] != 0 && pEval[Tu] != ~0U) || (pEval[F] != 0 && pEval[F] != ~0U)) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node values out-of-range"));
					json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "nid", json_integer(iNode));
					json_t *jNode = json_object();
					json_object_set_new_nocheck(jNode, "q", json_integer(Q));
					json_object_set_new_nocheck(jNode, "ti", json_integer(Ti ? 1 : 0));
					json_object_set_new_nocheck(jNode, "tu", json_integer(Tu));
					json_object_set_new_nocheck(jNode, "f", json_integer(F));
					json_object_set_new_nocheck(jError, "node", jNode);
					json_t *jValue = json_object();
					json_object_set_new_nocheck(jValue, "q", json_integer(pEval[Q]));
					json_object_set_new_nocheck(jValue, "ti", json_integer(pEval[Tu] & IBIT ? 1 : 0));
					json_object_set_new_nocheck(jValue, "tu", json_integer(pEval[Tu] & ~IBIT));
					json_object_set_new_nocheck(jValue, "f", json_integer(pEval[F]));
					json_object_set_new_nocheck(jError, "value", jValue);
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				/*
				 * Apply QTF operator
				 */

				// determine if the operator is `QTF` or `QnTF`
				if (Ti) {
					// `QnTF` apply the operator `"Q ? ~T : F"`
					pEval[iNode] = (pEval[Q] & ~pEval[Tu]) ^ (~pEval[Q] & pEval[F]);
				} else {
					// `QTF` apply the operator `"Q ? T : F"`
					pEval[iNode] = (pEval[Q] & pEval[Tu]) ^ (~pEval[Q] & pEval[F]);
				}
			}

			/*
			 * Compare the results for the provides
			 */
			for (unsigned iRoot = 0; iRoot < tree.numRoots; iRoot++) {
				uint32_t expected = (tree.flags & context_t::MAGICMASK_SYSTEM) ? 0 : testData[rootMap[iRoot]];
				uint32_t R = tree.roots[iRoot];
				uint32_t encountered = pEval[R & ~IBIT];
				if (R & IBIT)
					encountered ^= ~0U;

				// test for undefined. Initial value `pEval[]` is 0x5a5a5a5a.
				if (pEval[R & ~IBIT] != 0 && pEval[R & ~IBIT] != ~0U) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Root loads undefined"));
					json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "root", json_string(tree.rootNames[iRoot].c_str()));
					json_object_set_new_nocheck(jError, "value", json_integer(pEval[R & ~IBIT]));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				if ((!opt_onlyIfSet || encountered) && expected != encountered) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("validation failed"));
					json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					if (tree.flags & context_t::MAGICMASK_SYSTEM)
						json_object_set_new_nocheck(jError, "root", json_string(tree.rootNames[iRoot].c_str()));
					else
						json_object_set_new_nocheck(jError, "root", json_string(testNames[rootMap[iRoot]].c_str()));
					json_object_set_new_nocheck(jError, "expected", json_integer(expected));
					json_object_set_new_nocheck(jError, "encountered", json_integer(encountered));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}
			}
		}

		fprintf(stderr, "Passed %d tests\n", gNumTests);

		tree.freeMap(pEval);
		json_delete(jInput);
	}
};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {validateContext_t} Application context
 */
validateContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json> <output.dat>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --onlyifset\n");
	}
}

/**
 * @date 2021-05-13 15:28:31
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

	for (;;) {
		enum {
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_ONLYIFSET,
			LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",     1, 0, LO_DEBUG},
			{"help",      0, 0, LO_HELP},
			{"onlyifset", 0, 0, LO_ONLYIFSET},
			{"quiet",     2, 0, LO_QUIET},
			{"timer",     1, 0, LO_TIMER},
			{"verbose",   2, 0, LO_VERBOSE},

			{NULL,        0, 0, 0}
		};

		char optstring[64];
		char *cp                            = optstring;
		int  option_index                   = 0;

		for (int i = 0; long_options[i].name; i++) {
			if (isalpha(long_options[i].val)) {
				*cp++ = (char) long_options[i].val;

				if (long_options[i].has_arg)
					*cp++ = ':';
				if (long_options[i].has_arg == 2)
					*cp++ = ':';
			}
		}

		*cp = '\0';

		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_DEBUG:
			ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_ONLYIFSET:
			app.opt_onlyIfSet++;
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose + 1;
			break;

		case '?':
			ctx.fatal("Try `%s --help' for more information.\n", argv[0]);
		default:
			ctx.fatal("getopt returned character code %d\n", c);
		}
	}

	char *jsonFilename;
	char *dataFilename;

	if (argc - optind >= 2) {
		jsonFilename = argv[optind++];
		dataFilename = argv[optind++];
	} else {
		usage(argv, false);
		exit(1);
	}

	/*
	 * Create storage
	 */

	/*
	 * Create components
	 */

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);

	/*
	 * Main
	 */

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	app.main(jsonFilename, dataFilename);


	json_t *jError = json_object();
	json_object_set_new_nocheck(jError, "passed", json_string_nocheck("true"));
	json_object_set_new_nocheck(jError, "filename", json_string(dataFilename));
	json_object_set_new_nocheck(jError, "numtests", json_integer(app.gNumTests));
	printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));

	return 0;
}
