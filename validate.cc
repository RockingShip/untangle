//#pragma GCC optimize ("O0")

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
 * @global {validateContext_t} Application context
 */
context_t ctx;

/**
 * @date 2021-05-13 15:30:14
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct validateContext_t {

	/// @var {string} input tree name
	const char *arg_data;
	/// @var {string} input metadata filename
	const char *arg_json;
	/// @var {number} --onlyifset, only validate non-zero root (consider them a cascading of OR intermediates)
	unsigned   opt_onlyIfSet;

	/// @var {baseTree_t*} input tree
	baseTree_t *pInputTree;

	// json data
	uint32_t   kstart;
	uint32_t   estart;
	uint32_t   nstart;
	uint32_t   ncount;
	uint32_t   numRoots;
	const char **keyNames;
	const char **rootNames;


	// test data
	unsigned gNumTests;
	uint8_t  *gTestKeys;
	uint8_t  *gTestRoots;

	validateContext_t() {
		arg_data      = NULL;
		arg_json      = NULL;
		opt_onlyIfSet = 0;
		pInputTree    = NULL;

		kstart    = 0;
		estart    = 0;
		nstart    = 0;
		numRoots  = 0;
		keyNames  = NULL;
		rootNames = NULL;


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
	void loadJson() {
		/*
		 * Load json
		 */
		json_error_t jerror;

		// load json
		FILE *f = fopen(arg_json, "r");
		if (!f) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("fopen()"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "errno", json_integer(errno));
			json_object_set_new_nocheck(jError, "errtxt", json_string(strerror(errno)));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		json_t *jInput = json_loadf(f, 0, &jerror);
		if (jInput == 0) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to decode json"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "line", json_integer(jerror.line));
			json_object_set_new_nocheck(jError, "text", json_string(jerror.text));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}
		fclose(f);

		/*
		 * import dimensions
		 */
		kstart   = json_integer_value(json_object_get(jInput, "kstart"));
		estart   = json_integer_value(json_object_get(jInput, "estart"));
		nstart   = json_integer_value(json_object_get(jInput, "nstart"));
		ncount   = json_integer_value(json_object_get(jInput, "ncount"));
		numRoots = json_integer_value(json_object_get(jInput, "numroots"));

		if (kstart == 0 || kstart >= ncount) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("kstart out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "kstart", json_integer(kstart));
			json_object_set_new_nocheck(jError, "ncount", json_integer(ncount));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}
		if (estart < kstart || kstart >= ncount) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("pstart out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "kstart", json_integer(kstart));
			json_object_set_new_nocheck(jError, "estart", json_integer(estart));
			json_object_set_new_nocheck(jError, "ncount", json_integer(ncount));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}
		if (nstart < estart || nstart >= ncount) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("nstart out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "estart", json_integer(estart));
			json_object_set_new_nocheck(jError, "nstart", json_integer(nstart));
			json_object_set_new_nocheck(jError, "ncount", json_integer(ncount));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}
		if (numRoots == 0) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("numroots out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "numroots", json_integer(numRoots));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		/*
		 * import key+root names
		 */

		json_t *jKeyNames = json_object_get(jInput, "keys");
		if (!jKeyNames) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag keys"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		unsigned numKeyNames = json_array_size(jKeyNames);
		if (numKeyNames != nstart) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incorrect number of keys"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "expected", json_integer(nstart));
			json_object_set_new_nocheck(jError, "encountered", json_integer(numKeyNames));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		keyNames = (const char **) malloc(nstart * sizeof *keyNames);
		for (uint32_t iKey = 0; iKey < nstart; iKey++)
			keyNames[iKey] = strdup(json_string_value(json_array_get(jKeyNames, iKey)));

		json_t *jRootNames = json_object_get(jInput, "roots");
		if (!jRootNames) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag roots"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		unsigned numRootNames = json_array_size(jRootNames);
		if (numRootNames != numRoots) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incorrect number of roots"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			json_object_set_new_nocheck(jError, "expected", json_integer(numRoots));
			json_object_set_new_nocheck(jError, "encountered", json_integer(numRootNames));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		rootNames = (const char **) malloc(numRoots * sizeof *rootNames);
		for (unsigned iRoot = 0; iRoot < numRoots; iRoot++)
			rootNames[iRoot] = strdup(json_string_value(json_array_get(jRootNames, iRoot)));

		/*
		 * Import tests
		 */

		json_t *jTests = json_object_get(jInput, "tests");
		gNumTests = json_array_size(jTests);
		if (!gNumTests) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag tests"));
			json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		// allocate buffers for keys/roots
		gTestKeys = (uint8_t *) calloc(gNumTests, nstart - kstart);
		assert(gTestKeys);
		gTestRoots = (uint8_t *) calloc(gNumTests, numRoots);
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
				json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			/*
			 * decode key data
			 */
			uint8_t  *pData = gTestKeys + iTest * (nstart - kstart);
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
						json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
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
			if (iBit < nstart - kstart) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("key data too short in test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				json_object_set_new_nocheck(jError, "expected", json_integer(nstart - kstart));
				json_object_set_new_nocheck(jError, "encountered", json_integer(iBit));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			/*
			 * decode root data
			 */
			pData = gTestRoots + iTest * numRoots;
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
						json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
						json_object_set_new_nocheck(jError, "test", json_integer(iTest));
						json_object_set_new_nocheck(jError, "root-data", json_string_nocheck(strRoots));
						ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
					}
				}

				for (unsigned k = 0; k < 8; k++) {
					if (iBit < numRoots)
						pData[iBit++] = byte & (1 << k);
				}
			}

			if (iBit < numRoots) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("root data too short in test entry"));
				json_object_set_new_nocheck(jError, "filename", json_string(arg_json));
				json_object_set_new_nocheck(jError, "test", json_integer(iTest));
				json_object_set_new_nocheck(jError, "expected", json_integer(numRoots));
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
		baseTree_t tree(ctx, 0, 0, 0, 0, 0); // todo: constructor

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
		if (tree.kstart != kstart || tree.estart != estart || tree.nstart != nstart || tree.numRoots != numRoots) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("meta mismatch"));
			json_object_set_new_nocheck(jError, "filename", json_string(fname));
			json_t *jMeta = json_object();
			json_object_set_new_nocheck(jMeta, "kstart", json_integer(kstart));
			json_object_set_new_nocheck(jMeta, "estart", json_integer(estart));
			json_object_set_new_nocheck(jMeta, "nstart", json_integer(nstart));
			json_object_set_new_nocheck(jMeta, "numroots", json_integer(numRoots));
			json_object_set_new_nocheck(jError, "meta", jMeta);
			json_t *jData = json_object();
			json_object_set_new_nocheck(jData, "kstart", json_integer(tree.kstart));
			json_object_set_new_nocheck(jData, "estart", json_integer(tree.estart));
			json_object_set_new_nocheck(jData, "nstart", json_integer(tree.nstart));
			json_object_set_new_nocheck(jData, "numroots", json_integer(tree.numRoots));
			json_object_set_new_nocheck(jError, "data", jData);
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		// check names
		for (uint32_t iName = 0; iName < nstart; iName++) {
			if (strcmp(keyNames[iName], keyNames[iName]) != 0) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("key name mismatch"));
				json_object_set_new_nocheck(jError, "filename", json_string(fname));
				json_object_set_new_nocheck(jError, "ix", json_integer(iName));
				json_object_set_new_nocheck(jError, "expected", json_string_nocheck(keyNames[iName]));
				json_object_set_new_nocheck(jError, "encountered", json_string_nocheck(keyNames[iName]));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}
		}
		for (unsigned iName = 0; iName < numRoots; iName++) {
			if (strcmp(rootNames[iName], rootNames[iName]) != 0) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("root name mismatch"));
				json_object_set_new_nocheck(jError, "filename", json_string(fname));
				json_object_set_new_nocheck(jError, "ix", json_integer(iName));
				json_object_set_new_nocheck(jError, "expected", json_string_nocheck(rootNames[iName]));
				json_object_set_new_nocheck(jError, "encountered", json_string_nocheck(rootNames[iName]));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}
		}

		if (tree.flags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			ctx.logFlags(tree.flags);

		/*
		 * Count references
		 */
		{
			uint32_t *pRefCount = tree.allocMap();
			json_t   *jList     = json_array();

			for (uint32_t i = 0; i < nstart; i++)
				pRefCount[i] = 0;

			for (uint32_t i = nstart; i < ncount; i++) {
				const baseNode_t *pNode = tree.N + i;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
//				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				pRefCount[Q]++;
				pRefCount[Tu]++;
				if (Tu != F)
					pRefCount[F]++;
			}

			/*
			 * Count externals
			 */
			for (uint32_t iKey = estart; iKey < nstart; iKey++) {
				if (pRefCount[iKey] > 0)
					json_array_append_new(jList, json_string_nocheck(keyNames[iKey]));
			}

			if (json_array_size(jList) > 0) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("unresolved externals"));
				json_object_set_new_nocheck(jError, "filename", json_string(fname));
				json_object_set_new_nocheck(jError, "k", jList);
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			json_delete(jList);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
			json_t *jList = json_array();

			/*
			 * Which roots are not keys
			 */
			for (uint32_t iRoot = 0; iRoot < numRoots; iRoot++) {
				if ((tree.roots[iRoot] & ~IBIT) >= tree.nstart)
					json_array_append_new(jList, json_string_nocheck(rootNames[iRoot]));
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
			}

			/*
			 * prepare the data vector
			 */

			for (uint32_t iKey = 0; iKey < tree.ncount; iKey++)
				pEval[iKey] = iKey; // non-zero is error marker

			// load the test data
			uint8_t *pData = gTestKeys + iTest * (nstart - kstart);

			for (uint32_t iKey = 0; iKey < kstart; iKey++)
				pEval[iKey] = iKey; // all non-zero de-references will trigger an error
			for (uint32_t iKey  = kstart; iKey < nstart; iKey++)
				pEval[iKey] = pData[iKey - kstart] ? IBIT : 0;

			/*
			 * Run the test
			 */
			for (uint32_t iNode = nstart; iNode < ncount; iNode++) {
				const baseNode_t *pNode = tree.N + iNode;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				// test range
				if (Q >= ncount || Tu >= ncount || F >= ncount) {
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
				if ((pEval[Q] & ~IBIT) != 0 ||
				    (pEval[Q] != 0 && (pEval[Tu] & ~IBIT) != 0) ||
				    (pEval[Q] == 0 && (pEval[F] & ~IBIT) != 0)) {

					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Node loads undefined"));
					json_object_set_new_nocheck(jError, "filename", json_string(fname));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "node", json_integer(iNode));
					json_object_set_new_nocheck(jError, "eval-q", json_integer(pEval[Q] & ~IBIT));
					json_object_set_new_nocheck(jError, "eval-tu", json_integer(pEval[Tu] & ~IBIT));
					json_object_set_new_nocheck(jError, "eval-f", json_integer(pEval[F] & ~IBIT));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				unsigned result = pEval[Q] ? (pEval[Tu] ^ Ti) : pEval[F];

				pEval[iNode] = result;
			}

			// load the result data
			pData = gTestRoots + iTest * numRoots;

			/*
			 * Compare the results for the provides
			 */
			for (uint32_t iRoot = 0; iRoot < numRoots; iRoot++) {

				uint32_t r = tree.roots[iRoot];

				// test for undefined
				if ((pEval[r & ~IBIT] & ~IBIT) != 0) {
					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("Root loads undefined"));
					json_object_set_new_nocheck(jError, "filename", json_string(fname));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "root", json_integer(iRoot));
					json_object_set_new_nocheck(jError, "eval-root", json_integer(pEval[r & ~IBIT] & ~IBIT));
					ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				}

				uint32_t expected    = pData[iRoot] ? IBIT : 0;
				uint32_t encountered = pEval[r & ~IBIT] ^(r & IBIT);


				if ((!opt_onlyIfSet || encountered) && expected != encountered) {
					// convert outputs to hex string
					char     strExpected[numRoots / 4 + 2];
					unsigned strExpectedLen    = 0;
					char     strEncountered[numRoots / 4 + 2];
					unsigned strEncounteredLen = 0;

					for (unsigned i = 0; i <= (numRoots - 1) / 8 * 8; i += 8) {
						unsigned byte;

						byte = 0;
						for (unsigned j = 0; j < 8; j++)
							byte |= (i + j < numRoots && pData[i + j]) ? 1 << j : 0;

						strExpected[strExpectedLen++] = "0123456789abcdef"[byte >> 4];
						strExpected[strExpectedLen++] = "0123456789abcdef"[byte & 15];

						byte = 0;
						for (unsigned j = 0; j < 8; j++)
							byte |= (i + j < numRoots && (pEval[tree.roots[i + j] & ~IBIT] ^ (tree.roots[i + j] & IBIT)) == IBIT) ? 1 << j : 0;

						strEncountered[strEncounteredLen++] = "0123456789abcdef"[byte >> 4];
						strEncountered[strEncounteredLen++] = "0123456789abcdef"[byte & 15];
					}

					strExpected[strExpectedLen++]       = 0;
					strEncountered[strEncounteredLen++] = 0;

					json_t *jError = json_object();
					json_object_set_new_nocheck(jError, "error", json_string_nocheck("validation failed"));
					json_object_set_new_nocheck(jError, "filename", json_string(fname));
					json_object_set_new_nocheck(jError, "testnr", json_integer(iTest));
					json_object_set_new_nocheck(jError, "bit", json_integer(iRoot));
					json_object_set_new_nocheck(jError, "root", json_string(rootNames[iRoot]));
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

void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s <json> <data>\n", argv[0]);
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
int main(int argc, char *const *argv) {
	setlinebuf(stdout);

	for (;;) {
		int option_index = 0;
		enum {
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_ONLYIFSET,
			LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"help",      0, 0, LO_HELP},
			{"quiet",     2, 0, LO_QUIET},
			{"verbose",   2, 0, LO_VERBOSE},
			{"debug",     1, 0, LO_DEBUG},
			{"timer",     1, 0, LO_TIMER},
			{"onlyifset", 0, 0, LO_ONLYIFSET},

			{NULL,        0, 0, 0}
		};

		char optstring[128], *cp;
		cp = optstring;

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
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
				break;
			case LO_VERBOSE:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose + 1;
				break;
			case LO_DEBUG:
				ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
				break;
			case LO_TIMER:
				ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
				break;
			case LO_ONLYIFSET:
				app.opt_onlyIfSet++;
				break;


			case '?':
				ctx.fatal("Try `%s --help' for more information.\n", argv[0]);
			default:
				ctx.fatal("getopt returned character code %d\n", c);
		}
	}

	if (argc - optind >= 2) {
		app.arg_json = argv[optind++];
		app.arg_data = argv[optind++];
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

	/*
	 * Load json into context
	 */
	app.loadJson();

	/*
	 * Validate files
	 */
	app.validateData(app.arg_data);


	json_t *jError = json_object();
	json_object_set_new_nocheck(jError, "passed", json_string_nocheck("true"));
	json_object_set_new_nocheck(jError, "filename", json_string(app.arg_data));
	printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));

	return 0;
}
