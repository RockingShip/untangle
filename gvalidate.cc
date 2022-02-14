#pragma GCC optimize ("O0") // optimize on demand

/*
 * validate.cc
 * 	Validate a `baseTree` file against the tests stored in the matching json
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
#include "grouptree.h"

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
struct bvalidateContext_t {

	/// @var {string} name of database
	const char *opt_databaseName;
	/// @var {number} header flags
	uint32_t   opt_flagsSet;
	/// @var {number} header flags
	uint32_t   opt_flagsClr;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned   opt_maxNode;
	/// @var {number} --onlyifset, only validate non-zero root (consider them a cascading of OR intermediates)
	unsigned   opt_onlyIfSet;

	// test data
	unsigned gNumTests;

	/// @var {database_t} - Database store to place results
	database_t    *pStore;

	bvalidateContext_t() {
		opt_databaseName = "untangle.db";
		opt_flagsSet     = 0;
		opt_flagsClr     = 0;
		opt_maxNode      = groupTree_t::DEFAULT_MAXNODE;
		opt_onlyIfSet    = 0;

		// test data
		gNumTests = 0;
		pStore    = NULL;
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
		groupTree_t jsonTree(ctx, *pStore);

		jsonTree.loadFileJson(jInput, jsonFilename);

		/*
		 * load tree
		 */

		groupTree_t tree(ctx, *pStore);

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

		std::vector<std::string>        testNames;   // The names of the entries
		std::map<std::string, unsigned> testLookup;  // Name lookup	
		std::vector<uint32_t>           testData;    // Test values
		std::vector<uint32_t>           entryMap;    // How tree entrypoints map onto test data
		std::vector<uint32_t>           rootMap;     // How tree roots map onto test data

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
		entryMap.resize(tree.nstart - tree.kstart);
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
			entryMap[iEntry - tree.kstart] = it->second;
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
					if (iBit < (jsonTree.nstart - jsonTree.kstart) + jsonTree.numRoots)
						testData[iBit++] = (byte & (1 << k)) ? ~0U : 0U;
				}
			}

			if (iBit < jsonTree.numRoots) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("root data too short in test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				json_object_set_new_nocheck(jError, "expected", json_integer(jsonTree.numRoots));
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

			pEval[0] = 0; // only zero is well-defined

			for (unsigned iEntry = tree.kstart; iEntry < tree.nstart; iEntry++)
				pEval[iEntry] = testData[entryMap[iEntry - tree.kstart]];

			/*
			 * Run the test
			 */
			// find group headers
			for (uint32_t iGroup = tree.nstart; iGroup < tree.ncount; iGroup++) {
				if (tree.N[iGroup].gid != iGroup)
					continue; // not a group header

				bool once = true;

				// walk through group list in search of a `1n9` node
				for (uint32_t iNode = tree.N[iGroup].next; iNode != iGroup; iNode = tree.N[iNode].next) {
					groupNode_t *pNode = tree.N + iNode;

					for (unsigned iSlot = 0; iSlot < pStore->signatures[pNode->sid].numPlaceholder; iSlot++) {
						uint32_t id = pNode->slots[iSlot];
						
						// test range
						if (id == 0 || id >= tree.ncount) {
							json_t *jError = json_object();
							json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node references out-of-range"));
							json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
							json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
							json_object_set_new_nocheck(jError, "gid", json_integer(iGroup));
							json_object_set_new_nocheck(jError, "nid", json_integer(iNode));
							json_object_set_new_nocheck(jError, "slot", json_integer(iSlot));
							json_object_set_new_nocheck(jError, "id", json_integer(id));
							ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
						}

						// test for undefined
						if (pEval[id] != 0 && pEval[id] != ~0U) {
							json_t *jError = json_object();
							json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node values out-of-range"));
							json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
							json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
							json_object_set_new_nocheck(jError, "gid", json_integer(iGroup));
							json_object_set_new_nocheck(jError, "nid", json_integer(iNode));
							json_object_set_new_nocheck(jError, "slot", json_integer(iSlot));
							json_object_set_new_nocheck(jError, "id", json_integer(id));
							json_object_set_new_nocheck(jError, "value", json_integer(pEval[id]));
							ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
						}
					}

					uint32_t ev = tree.evalNode(iNode, pEval);

					if (once) {
						pEval[iGroup] = ev;
						once = false;
					} else if (pEval[iGroup] != ev) {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node values out-of-range"));
						json_object_set_new_nocheck(jError, "filename", json_string(treeFilename));
						json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
						json_object_set_new_nocheck(jError, "gid", json_integer(iGroup));
						json_object_set_new_nocheck(jError, "nid", json_integer(iNode));
						json_object_set_new_nocheck(jError, "expected", json_integer(pEval[iGroup]));
						json_object_set_new_nocheck(jError, "encountered", json_integer(ev));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
				}
			}
			
#if 0
			for (uint32_t iNode = tree.nstart; iNode < tree.ncount; iNode++) {
				const groupNode_t *pNode = tree.N + iNode;
				const uint32_t   Q      = 0;
				const uint32_t   Ti     = 0;
				const uint32_t   Tu     = 0;
				const uint32_t   F      = 0;

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
#endif

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
 * @global {bvalidateContext_t} Application context
 */
bvalidateContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json> <output.dat>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename>   Database to query [default=%s]\n", app.opt_databaseName);
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
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
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_MAXNODE, LO_ONLYIFSET,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_DATABASE = 'D', LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"debug",       1, 0, LO_DEBUG},
			{"help",        0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"onlyifset",   0, 0, LO_ONLYIFSET},
			{"quiet",       2, 0, LO_QUIET},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",     2, 0, LO_VERBOSE},
			//
			{"paranoid",    0, 0, LO_PARANOID},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"pure",        0, 0, LO_PURE},
			{"no-pure",     0, 0, LO_NOPURE},
			{"rewrite",     0, 0, LO_REWRITE},
			{"no-rewrite",  0, 0, LO_NOREWRITE},
			{"cascade",     0, 0, LO_CASCADE},
			{"no-cascade",  0, 0, LO_NOCASCADE},
//			{"shrink",      0, 0, LO_SHRINK},
//			{"no-shrink",   0, 0, LO_NOSHRINK},
//			{"pivot3",      0, 0, LO_PIVOT3},
//			{"no-pivot3",   0, 0, LO_NOPIVOT3},
			//
			{NULL,          0, 0, 0}
		};

		char optstring[64];
		char *cp          = optstring;
		int  option_index = 0;

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
		case LO_DATABASE:
			app.opt_databaseName = optarg;
			break;
		case LO_DEBUG:
			ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_MAXNODE:
			app.opt_maxNode = (unsigned) strtoul(optarg, NULL, 10);
			break;
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

		case LO_PARANOID:
			app.opt_flagsSet |= ctx.MAGICMASK_PARANOID;
			app.opt_flagsClr &= ~ctx.MAGICMASK_PARANOID;
			break;
		case LO_NOPARANOID:
			app.opt_flagsSet &= ~ctx.MAGICMASK_PARANOID;
			app.opt_flagsClr |= ctx.MAGICMASK_PARANOID;
			break;
		case LO_PURE:
			app.opt_flagsSet |= ctx.MAGICMASK_PURE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_PURE;
			break;
		case LO_NOPURE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_PURE;
			app.opt_flagsClr |= ctx.MAGICMASK_PURE;
			break;
		case LO_REWRITE:
			app.opt_flagsSet |= ctx.MAGICMASK_REWRITE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_REWRITE;
			break;
		case LO_NOREWRITE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_REWRITE;
			app.opt_flagsClr |= ctx.MAGICMASK_REWRITE;
			break;
		case LO_CASCADE:
			app.opt_flagsSet |= ctx.MAGICMASK_CASCADE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_CASCADE;
			break;
		case LO_NOCASCADE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_CASCADE;
			app.opt_flagsClr |= ctx.MAGICMASK_CASCADE;
			break;
//			case LO_SHRINK:
//				app.opt_flags |=  ctx.MAGICMASK_SHRINK;
//				break;
//			case LO_NOSHRINK:
//				app.opt_flags &=  ~ctx.MAGICMASK_SHRINK;
//				break;
//			case LO_PIVOT3:
//				app.opt_flags |=  ctx.MAGICMASK_PIVOT3;
//				break;
//			case LO_NOPIVOT3:
//				app.opt_flags &=  ~ctx.MAGICMASK_PIVOT3;
//				break;

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

	// Open database
	database_t db(ctx);

	db.open(app.opt_databaseName);
	app.pStore = &db;

	// set flags
	ctx.flags = db.creationFlags;
	ctx.flags |= app.opt_flagsSet;
	ctx.flags &= ~app.opt_flagsClr;

	// display system flags when database was created
	if ((ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) || (ctx.flags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY))
		fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(ctx.flags).c_str());

	app.main(jsonFilename, dataFilename);

	json_t *jError = json_object();
	json_object_set_new_nocheck(jError, "passed", json_string_nocheck("true"));
	json_object_set_new_nocheck(jError, "filename", json_string(dataFilename));
	json_object_set_new_nocheck(jError, "numtests", json_integer(app.gNumTests));
	printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));

	return 0;
}
