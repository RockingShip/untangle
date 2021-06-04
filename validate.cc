//#pragma GCC optimize ("O0") // optimize on demand

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

	// json data
	uint32_t                 kstart;
	uint32_t                 ostart;
	uint32_t                 estart;
	// NOTE: Trees may have additional extended keys which will effect the following 
	uint32_t                 nstart;
	uint32_t                 numRoots;
	std::vector<std::string> keyNames;
	std::vector<std::string> rootNames;


	// test data
	unsigned gNumTests;
	uint8_t  *gTestKeys;
	uint8_t  *gTestRoots;

	validateContext_t() {
		opt_onlyIfSet = 0;
		pInputTree    = NULL;

		kstart   = 0;
		ostart   = 0;
		estart   = 0;
		nstart   = 0;
		numRoots = 0;

		// test data
		gNumTests  = 0;
		gTestKeys  = NULL;
		gTestRoots = NULL;
	}


	/*
	 * @date 2021-05-13 16:40:18
	 *
	 * Load dimensions and metrics from json
	 */
	void loadJson(const char *jsonFilename) {
		/*
		 * Load json
		 */

		// load json
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
		 * import dimensions
		 */
		kstart    = jsonTree.kstart;
		ostart    = jsonTree.ostart;
		estart    = jsonTree.estart;
		nstart    = jsonTree.nstart;
		numRoots  = jsonTree.numRoots;
		keyNames  = jsonTree.keyNames;
		rootNames = jsonTree.rootNames;

		/*
		 * Import tests
		 */

		json_t *jTests = json_object_get(jInput, "tests");
		gNumTests = json_array_size(jTests);
		if (!gNumTests) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag 'tests'"));
			json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		// allocate buffers for keys/roots
		gTestKeys = (uint8_t *) calloc(gNumTests, ostart - kstart);
		assert(gTestKeys);
		gTestRoots = (uint8_t *) calloc(gNumTests, estart - ostart);
		assert(gTestRoots);

		// convert ascii to hex and inject at the appropriate location
		for (unsigned iTest = 0; iTest < gNumTests; iTest++) {
			// extract test element
			json_t     *jTest    = json_array_get(jTests, iTest);
			const char *strKeys  = json_string_value(json_array_get(jTest, 0));
			const char *strRoots = json_string_value(json_array_get(jTest, 1));

			// simple validation
			if (!strKeys || !strRoots) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incomplete test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			/*
			 * decode key data
			 */
			uint8_t  *pData = gTestKeys + iTest * (ostart - kstart);
			unsigned iBit   = 0;

			// convert
			for (unsigned iPos = 0; iPos < strlen(strKeys); /* increment in loop */ ) {
				// skip spaces
				if (isspace(strKeys[iPos])) {
					iPos++;
					continue;
				}

				unsigned byte = 0;

				for (int iNibble = 0; iNibble < 2; iNibble++) {
					char ch = strKeys[iPos++]; // loop decrement happens here

					byte *= 16;

					if (ch >= '0' && ch <= '9')
						byte += ch - '0';
					else if (ch >= 'A' && ch <= 'F')
						byte += ch - 'A' + 10;
					else if (ch >= 'a' && ch <= 'f')
						byte += ch - 'a' + 10;
					else {
						json_t *jError = json_object();
						json_object_set_new_nocheck(jError, "error", json_string_nocheck("bad key data in test entry"));
						json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
						json_object_set_new_nocheck(jError, "test", json_integer(iTest));
						json_object_set_new_nocheck(jError, "key-data", json_string_nocheck(strKeys));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
				}

				for (unsigned k = 0; k < 8; k++) {
					if (iBit < nstart - kstart)
						pData[iBit++] = byte & (1 << k);
				}
			}
			if (iBit < ostart - kstart) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("key data too short in test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				json_object_set_new_nocheck(jError, "expected", json_integer(ostart - kstart));
				json_object_set_new_nocheck(jError, "encountered", json_integer(iBit));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			/*
			 * decode root data
			 */
			pData = gTestRoots + iTest * (estart - ostart);
			iBit  = 0;

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
					if (iBit < estart - ostart)
						pData[iBit++] = byte & (1 << k);
				}
			}

			if (iBit < estart - ostart) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("root data too short in test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(jsonFilename));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				json_object_set_new_nocheck(jError, "expected", json_integer(estart - ostart));
				json_object_set_new_nocheck(jError, "numroots", json_integer(numRoots));
				json_object_set_new_nocheck(jError, "encountered", json_integer(iBit));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}
		}

		fprintf(stderr, "Loaded %d tests\n", gNumTests);

		json_delete(jInput);
	}

	/*
	 * @date
	 *
	 * Load baseTree file and compare against imported json
	 */
	void validateData(const char *fname) {
		// load tree
		baseTree_t tree(ctx);

		if (tree.loadFile(fname)) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to load"));
			json_object_set_new_nocheck(jError, "filename", json_string(fname));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(fname));
			tree.headerInfo(jResult);
			tree.extraInfo(jResult);
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
			json_delete(jResult);
		}

		// check dimensions
		if (tree.kstart != kstart || tree.ostart != ostart || tree.estart != estart || tree.numRoots < estart) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("meta mismatch"));
			json_object_set_new_nocheck(jError, "filename", json_string(fname));
			json_t *jMeta = json_object();
			json_object_set_new_nocheck(jMeta, "kstart", json_integer(kstart));
			json_object_set_new_nocheck(jMeta, "ostart", json_integer(ostart));
			json_object_set_new_nocheck(jMeta, "estart", json_integer(estart));
			json_object_set_new_nocheck(jMeta, "nstart", json_integer(nstart));
			json_object_set_new_nocheck(jMeta, "numroots", json_integer(numRoots));
			json_object_set_new_nocheck(jError, "meta", jMeta);
			json_t *jData = json_object();
			json_object_set_new_nocheck(jData, "kstart", json_integer(tree.kstart));
			json_object_set_new_nocheck(jData, "ostart", json_integer(tree.ostart));
			json_object_set_new_nocheck(jData, "estart", json_integer(tree.estart));
			json_object_set_new_nocheck(jData, "nstart", json_integer(tree.nstart));
			json_object_set_new_nocheck(jData, "numroots", json_integer(tree.numRoots));
			json_object_set_new_nocheck(jError, "data", jData);
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		// check names
		for (uint32_t iName = kstart; iName < estart; iName++) {
			if (keyNames[iName].compare(tree.keyNames[iName]) != 0) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("key name mismatch"));
				json_object_set_new_nocheck(jError, "filename", json_string(fname));
				json_object_set_new_nocheck(jError, "kid", json_integer(iName));
				json_object_set_new_nocheck(jError, "expected", json_string_nocheck(keyNames[iName].c_str()));
				json_object_set_new_nocheck(jError, "encountered", json_string_nocheck(tree.keyNames[iName].c_str()));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}
		}
		for (unsigned iName = kstart; iName < estart; iName++) {
			if (rootNames[iName].compare(tree.rootNames[iName]) != 0) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("root name mismatch"));
				json_object_set_new_nocheck(jError, "filename", json_string(fname));
				json_object_set_new_nocheck(jError, "rid", json_integer(iName));
				json_object_set_new_nocheck(jError, "expected", json_string_nocheck(rootNames[iName].c_str()));
				json_object_set_new_nocheck(jError, "encountered", json_string_nocheck(tree.rootNames[iName].c_str()));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}
		}

		if (tree.flags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			ctx.logFlags(tree.flags);

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
			json_t *jList = json_array();

			/*
			 * Which roots are not keys
			 */
			for (uint32_t iRoot = ostart; iRoot < estart; iRoot++) {
				if ((tree.roots[iRoot] & ~IBIT) >= tree.nstart)
					json_array_append_new(jList, json_string_nocheck(rootNames[iRoot].c_str()));
			}

			if (json_array_size(jList) > 0)
				fprintf(stderr, "Validating: %s\n", json_dumps(jList, JSON_PRESERVE_ORDER | JSON_COMPACT));
			json_delete(jList);
		}

		ctx.setupSpeed(gNumTests);
		ctx.tick = 0;

		uint32_t *pEval = tree.allocMap();

		for (uint32_t iTest = 0; iTest < gNumTests; iTest++) {
			ctx.progress++;

			if (ctx.tick && ctx.opt_verbose >= ctx.VERBOSE_TICK) {
				int perSecond = ctx.updateSpeed();

				int eta  = (int) ((ctx.progressHi - ctx.progress) / perSecond);
				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS);

				ctx.tick = 0;
			}

			/*
			 * prepare the data vector.
			 * For validation, either all bits are set or all bits are clear
			 */

			for (uint32_t iKey = 0; iKey < tree.ncount; iKey++)
				pEval[iKey] = 0x5a5a5a5a; // set to invalid value

			pEval[0] = 0; // only zero is defined

			// load the test data into K region
			uint8_t *pData = gTestKeys + iTest * (ostart - kstart);

			for (uint32_t iKey = kstart; iKey < ostart; iKey++)
				pEval[iKey] = pData[iKey - kstart] ? ~0U : 0;

			/*
			 * Run the test
			 */
			for (uint32_t iNode = tree.nstart; iNode < tree.ncount; iNode++) {
				const baseNode_t *pNode = tree.N + iNode;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				// test range
				if (Q >= tree.ncount || Tu >= tree.ncount || F >= tree.ncount) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node references out-of-range"));
					json_object_set_new_nocheck(jError, "filename", json_string(fname));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "node", json_integer(iNode));
					json_object_set_new_nocheck(jError, "q", json_integer(Q));
					json_object_set_new_nocheck(jError, "tu", json_integer(Tu));
					json_object_set_new_nocheck(jError, "f", json_integer(F));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				// test for undefined
				if ((pEval[Q] != 0 && pEval[Q] != ~0U) ||
				    (pEval[Tu] != 0 && pEval[Tu] != ~0U) ||
				    (pEval[F] != 0 && pEval[F] != ~0U)) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node values out-of-range"));
					json_object_set_new_nocheck(jError, "filename", json_string(fname));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "node", json_integer(iNode));
					json_object_set_new_nocheck(jError, "q", json_integer(Q));
					json_object_set_new_nocheck(jError, "tu", json_integer(Tu));
					json_object_set_new_nocheck(jError, "f", json_integer(F));
					json_object_set_new_nocheck(jError, "q-val", json_integer(pEval[Q]));
					json_object_set_new_nocheck(jError, "tu-val", json_integer(pEval[Tu]));
					json_object_set_new_nocheck(jError, "f-val", json_integer(pEval[F]));
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

			// load the result data
			pData = gTestRoots + iTest * (estart - ostart);

			/*
			 * Compare the results for the provides
			 */
			for (uint32_t iRoot = ostart; iRoot < estart; iRoot++) {

				uint32_t R = tree.roots[iRoot];

				// test for undefined
				if (pEval[R & ~IBIT] != 0 && pEval[R & ~IBIT] != ~0U) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Root loads undefined"));
					json_object_set_new_nocheck(jError, "filename", json_string(fname));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "root", json_string(tree.rootNames[iRoot].c_str()));
					json_object_set_new_nocheck(jError, "value", json_integer(pEval[R & ~IBIT]));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				/*
				 * @date 2021-05-30 12:57:24
				 * The expression has been applied to all bits the register.
				 * The final result is either all bits clear (0) or all bits set (~0)
				 */

				uint32_t expected    = pData[iRoot - ostart] ? ~0U : 0;
				uint32_t encountered = (R & IBIT) ? pEval[R & ~IBIT] ^ ~0U : pEval[R & ~IBIT];


				if ((!opt_onlyIfSet || encountered) && expected != encountered) {
					// convert outputs to hex string
					char     strExpected[estart / 4 + 2];
					unsigned strExpectedLen    = 0;
					char     strEncountered[estart / 4 + 2];
					unsigned strEncounteredLen = 0;

					for (unsigned i = tree.ostart; i <= (estart - 1) / 8 * 8; i += 8) {
						unsigned byte;

						byte = 0;
						for (unsigned j = 0; j < 8; j++)
							byte |= (i + j < estart && pData[i + j - ostart]) ? 1 << j : 0;

						strExpected[strExpectedLen++] = "0123456789abcdef"[byte >> 4];
						strExpected[strExpectedLen++] = "0123456789abcdef"[byte & 15];

						byte = 0;
						for (unsigned j = 0; j < 8; j++) {
							if (i + j < estart) {
								uint32_t r2 = tree.roots[i + j];
								if (r2 & IBIT)
									byte |= pEval[r2 & ~IBIT] ? 0 : 1 << j;
								else
									byte |= pEval[r2] ? 1 << j : 0;
							}
						}

						strEncountered[strEncounteredLen++] = "0123456789abcdef"[byte >> 4];
						strEncountered[strEncounteredLen++] = "0123456789abcdef"[byte & 15];
					}

					strExpected[strExpectedLen++]       = 0;
					strEncountered[strEncounteredLen++] = 0;

					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("validation failed"));
					json_object_set_new_nocheck(jError, "filename", json_string(fname));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "bit", json_string(rootNames[iRoot].c_str()));
					json_object_set_new_nocheck(jError, "expected", json_string(strExpected));
					json_object_set_new_nocheck(jError, "encountered", json_string(strEncountered));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\n");
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

	/*
	 * Load json into context
	 */
	app.loadJson(jsonFilename);

	/*
	 * Validate files
	 */
	app.validateData(dataFilename);


	json_t *jError = json_object();
	json_object_set_new_nocheck(jError, "passed", json_string_nocheck("true"));
	json_object_set_new_nocheck(jError, "filename", json_string(dataFilename));
	printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));

	return 0;
}
