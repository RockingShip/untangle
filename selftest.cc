//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-04-21 23:30:30
 *
 * `selftest` is a collection of test, validations and metrics.
 *
 * Initially collected from `genrestartdata`, `gentransform` and `gensignature`
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
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include "config.h"
#include "database.h"
#include "dbtool.h"
#include "generator.h"
#include "metrics.h"
#include "restartdata.h"
#include "tinytree.h"

/**
 * @date 2020-04-21 23:33:37
 *
 * Selftest wrapper
 *
 * @typedef {object}
 */
struct selftestContext_t : dbtool_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {number} - THE generator
	generator_t generator;

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} Collect metrics intended for "metrics.h"
	unsigned   opt_metrics;
	/// @var {number} index/data ratio
	double     opt_ratio;
	/// @var {number} textual output instead of binary database
	unsigned   opt_text;

	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;
	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	/// @var {string[]} tree notation for `progress` points
	char **selftestWindowResults;

	/**
	 * Constructor
	 */
	selftestContext_t(context_t &ctx) : dbtool_t(ctx), generator(ctx) {
		arg_inputDatabase     = NULL;
		opt_metrics           = 0;
		opt_ratio             = METRICS_DEFAULT_RATIO / 10.0;
		opt_text              = 0;
		selftestWindowResults = NULL;
		pEvalFwd              = NULL;
		pEvalRev              = NULL;
		pStore                = NULL;
	}

	/**
	 * @date 2020-03-15 16:35:43
	 *
	 * Test that tree name encodng/decoding works as expected
	 */
	void performSelfTestTreeName(void) {

		// test name. NOTE: this is deliberately "not ordered"
		const char name[] = "ab+cd>efg&?hi^!";

		/*
		 * Basic test tree
		 */

		tinyTree_t tree(ctx);

		// test is test name can be decoded
		tree.loadStringFast(name);

		// test that tree is what was requested
		assert(!(tree.root & IBIT));
		assert(::strcmp(name, tree.saveString(tree.root)) == 0);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed\n", ctx.timeAsString(), __FUNCTION__);
	}

	/**
	 * @date 2020-04-22 15:03:12
	 *
	 * Test that `tinyTree_t` does level-2 normalisation
	 */
	void performSelfTestTreeNormaliseLevel2(void) {

		tinyTree_t tree(ctx);

		// decode name
		tree.loadStringSafe("ab>ba+^");
		// encode name
		const char *pName = tree.saveString(tree.root);

		if (::strcmp(pName, "ab+ab>^") != 0) {
			printf("{\"error\":\"tree not level-2 normalised\",\"where\":\"%s:%s:%d\",\"encountered\":\"%s\",\"expected\":\"%s\"}\n",
			       __FUNCTION__, __FILE__, __LINE__, pName, "ab+ab>^");
			exit(1);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed\n", ctx.timeAsString(), __FUNCTION__);
	}

	/**
	 * @date 2020-03-10 21:46:10
	 *
	 * Test that evaluating `tinyTree_t` is working as expected
	 *
	 * For every single-node tree there a 8 possible operands: Zero, three variables and their inverts.
	 * This totals to a collection of (8*8*8) 512 trees.
	 *
	 * For every tree:
	 *  - normalise q,t,f triplet
	 *  - Save tree as string
	 *  - Load tree as string
	 *  - Evaluate
	 *  - Compare with independent generated result
	 */
	void performSelfTestTreeEval(void) {

		unsigned testNr    = 0;
		unsigned numPassed = 0;

		// needs 32 byte alignment for AVX2
		footprint_t *pEval = (footprint_t *) ::aligned_alloc(32, pStore->align32(sizeof(*pEval) * tinyTree_t::TINYTREE_NEND));

		tinyTree_t tree(ctx);

		/*
		 * self-test with different program settings
		 */
		// @formatter:off
		for (unsigned iFast=0; iFast<2; iFast++) // decode notation in fast mode
		for (unsigned iSkin=0; iSkin<2; iSkin++) // use placeholder/skin notation
		for (unsigned iPure=0; iPure<2; iPure++) { // force `QnTF` rewrites
		// @formatter:on

			/*
			 * Test all 512 operand combinations. Zero, 3 endpoints and their 4 inverts (8*8*8=512)
			 */

			// @formatter:off
			for (unsigned Fu = 0; Fu < tinyTree_t::TINYTREE_KSTART + 3; Fu++) // operand of F: 0, a, b, c
			for (unsigned Fi = 0; Fi < 2; Fi++)                               // inverting of F
			for (unsigned Tu = 0; Tu < tinyTree_t::TINYTREE_KSTART + 3; Tu++)
			for (unsigned Ti = 0; Ti < 2; Ti++)
			for (unsigned Qu = 0; Qu < tinyTree_t::TINYTREE_KSTART + 3; Qu++)
			for (unsigned Qi = 0; Qi < 2; Qi++) {
			// @formatter:on

				// additional rangecheck
				if (Qu && Qu < tinyTree_t::TINYTREE_KSTART) continue;
				if (Tu && Tu < tinyTree_t::TINYTREE_KSTART) continue;
				if (Fu && Fu < tinyTree_t::TINYTREE_KSTART) continue;

				// bump test number
				testNr++;

				/*
				 * Load the tree with a single operator
				 */

				ctx.flags = context_t::MAGICMASK_PARANOID | (iPure ? context_t::MAGICMASK_PURE : 0);
				tree.clearTree();
				tree.root = tree.addNormaliseNode(Qu ^ (Qi ? IBIT : 0), Tu ^ (Ti ? IBIT : 0), Fu ^ (Fi ? IBIT : 0));

				/*
				 * save with placeholders and reload
				 */
				char treeName[tinyTree_t::TINYTREE_NAMELEN + 1];

				if (iSkin) {
					char skin[MAXSLOTS + 1];

					tree.saveString(tree.root, treeName, skin);
					if (iFast) {
						tree.loadStringFast(treeName, skin);
					} else {
						int ret = tree.loadStringSafe(treeName, skin);
						if (ret != 0) {
							printf("{\"error\":\"loadStringSafe() failed\",\"where\":\"%s:%s:%d\",\"testNr\":%u,\"iFast\":%u,\"iPure\":%u,\"iSkin\":%u,\"name\":\"%s/%s\",\"ret\":%d}\n",
							       __FUNCTION__, __FILE__, __LINE__, testNr, iFast, iPure, iSkin, treeName, skin, ret);
							exit(1);
						}
					}
				} else {
					tree.saveString(tree.root, treeName, NULL);
					if (iFast) {
						tree.loadStringFast(treeName);
					} else {
						int ret = tree.loadStringSafe(treeName);
						if (ret != 0) {
							printf("{\"error\":\"loadStringSafe() failed\",\"where\":\"%s:%s:%d\",\"testNr\":%u,\"iFast\":%u,\"iPure\":%u,\"iSkin\":%u,\"name\":\"%s\",\"ret\":%d}\n",
							       __FUNCTION__, __FILE__, __LINE__, testNr, iFast, iPure, iSkin, treeName, ret);
						}
					}
				}

				/*
				 * Evaluate tree
				 */

				// load test vector
				pEval[0].bits[0]                               = 0b00000000; // v[0]
				pEval[tinyTree_t::TINYTREE_KSTART + 0].bits[0] = 0b10101010; // v[1]
				pEval[tinyTree_t::TINYTREE_KSTART + 1].bits[0] = 0b11001100; // v[2]
				pEval[tinyTree_t::TINYTREE_KSTART + 2].bits[0] = 0b11110000; // v[3]

				// evaluate
				tree.eval(pEval);

				/*
				 * The footprint contains the tree outcome for every possible value combination the endpoints can have
				 * Loop through every state and verify the footprint is correct
				 */
				// @formatter:off
				for (unsigned c = 0; c < 2; c++)
				for (unsigned b = 0; b < 2; b++)
				for (unsigned a = 0; a < 2; a++) {
				// @formatter:on

					// bump test number
					testNr++;

					unsigned q = 0, t = 0, f = 0;

					/*
					 * Substitute endpoints `a-c` with their actual values.
					 */
					// @formatter:off
					switch (Qu) {
					case (tinyTree_t::TINYTREE_KSTART + 0): q = a; break;
					case (tinyTree_t::TINYTREE_KSTART + 1): q = b; break;
					case (tinyTree_t::TINYTREE_KSTART + 2): q = c; break;
					}
					if (Qi) q ^= 1;

					switch (Tu) {
					case (tinyTree_t::TINYTREE_KSTART + 0): t = a; break;
					case (tinyTree_t::TINYTREE_KSTART + 1): t = b; break;
					case (tinyTree_t::TINYTREE_KSTART + 2): t = c; break;
					}
					if (Ti) t ^= 1;

					switch (Fu) {
					case (tinyTree_t::TINYTREE_KSTART + 0): f = a; break;
					case (tinyTree_t::TINYTREE_KSTART + 1): f = b; break;
					case (tinyTree_t::TINYTREE_KSTART + 2): f = c; break;
					}
					if (Fi) f ^= 1;
					// @formatter:on

					/*
					 * `normaliseNode()` creates a tree with the expression `Q?T:F"`
					 * Calculate the outcome without using the tree.
					 */
					unsigned expected = q ? t : f;

					// extract encountered from footprint.
					unsigned ix          = c << 2 | b << 1 | a;
					unsigned encountered = pEval[tree.root & ~IBIT].bits[0] & (1 << ix) ? 1 : 0;
					if (tree.root & IBIT)
						encountered ^= 1; // invert result

					if (expected != encountered) {
						printf("{\"error\":\"compare failed\",\"where\":\"%s:%s:%d\",\"testNr\":%u,\"iFast\":%u,\"iQnTF\":%u,\"iSkin\":%u,\"expected\":\"%08x\",\"encountered\":\"%08x\",\"Q\":\"%c%x\",\"T\":\"%c%x\",\"F\":\"%c%x\",\"q\":\"%x\",\"t\":\"%x\",\"f\":\"%x\",\"c\":\"%x\",\"b\":\"%x\",\"a\":\"%x\",\"tree\":\"%s\"}\n",
						       __FUNCTION__, __FILE__, __LINE__, testNr, iFast, iPure, iSkin, expected, encountered, Qi ? '~' : ' ', Qu, Ti ? '~' : ' ', Tu, Fi ? '~' : ' ', Fu, q, t, f, c, b, a, treeName);
						exit(1);
					}
					numPassed++;
				}
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed %u tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-03-21 17:25:47
	 *
	 * Test restart/windowing by calling the generator with windowLo/Hi for each possible tree
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeWindowCreate(tinyTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			if (ctx.progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", ctx.timeAsString(), generator.windowLo * 100.0 / ctx.progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %lu", ctx.timeAsString(), generator.windowLo);
			ctx.tick = 0;
		}

		assert(ctx.progress < 2000000);

		// assert entry is unique
		if (selftestWindowResults[ctx.progress] != NULL) {
			printf("{\"error\":\"entry not unique\",\"where\":\"%s:%s:%d\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%lu}\n",
			       __FUNCTION__, __FILE__, __LINE__, selftestWindowResults[ctx.progress], pName, ctx.progress);
			exit(1);
		}

		// populate entry
		selftestWindowResults[ctx.progress] = ::strdup(pName);

		return true;
	}

	/**
	 * @date 2020-03-21 17:31:46
	 *
	 * Test restart/windowing by calling generator without a window and test if results match.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeWindowVerify(tinyTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			if (ctx.progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", ctx.timeAsString(), generator.windowLo * 100.0 / ctx.progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %lu", ctx.timeAsString(), generator.windowLo);
			ctx.tick = 0;
		}

		assert(ctx.progress < 2000000);

		// assert entry is present
		if (selftestWindowResults[ctx.progress] == NULL) {
			printf("{\"error\":\"missing\",\"where\":\"%s:%s:%d\",\"expected\":\"%s\",\"progress\":%lu}\n",
			       __FUNCTION__, __FILE__, __LINE__, pName, ctx.progress);
			exit(1);
		}

		// compare
		if (::strcmp(pName, selftestWindowResults[ctx.progress]) != 0) {
			printf("{\"error\":\"entry mismatch\",\"where\":\"%s:%s:%d\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%lu}\n",
			       __FUNCTION__, __FILE__, __LINE__, selftestWindowResults[ctx.progress], pName, ctx.progress);
			exit(1);
		}

		// release resources
		::free(selftestWindowResults[ctx.progress]);
		selftestWindowResults[ctx.progress] = NULL;

		return true;
	}

	/**
	  * @date 2020-03-21 20:09:49
	  *
	  * Test that generator restart/windowing is working as expected
	  *
	  * First call the generator for all `windowLo/windowHi` settings that should select a single tree
	  * Then test gathered collection matches a windowless invocation
	  */
	void performSelfTestWindow(void) {
		// allocate resources
		selftestWindowResults = (char **) ctx.myAlloc("genrestartdataContext_t::selftestResults", 2000000, sizeof(*selftestWindowResults));

		// set generator into `3n9-pure` mode
		ctx.flags &= ~context_t::MAGICMASK_PURE;
		unsigned numNode = 3;

		// find metrics for setting
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, numNode);
		assert(pMetrics);

		unsigned endpointsLeft = pMetrics->numNode * 2 + 1;

		// create templates
		generator.initialiseGenerator(ctx.flags & context_t::MAGICMASK_PURE);

		/*
		 * Pass 1, slice dataset into single entries
		 */

		for (uint64_t windowLo = 0; windowLo < pMetrics->numProgress; windowLo++) {
			// apply settings
			ctx.flags              = pMetrics->pure ? ctx.flags | context_t::MAGICMASK_PURE : ctx.flags & ~context_t::MAGICMASK_PURE;
			generator.windowLo     = windowLo;
			generator.windowHi     = windowLo + 1;
			generator.pRestartData = restartData + restartIndex[pMetrics->numNode][pMetrics->pure];
			ctx.progressHi         = pMetrics->numProgress;
			ctx.progress           = 0;
			ctx.tick               = 0;

			generator.clearGenerator();
			generator.generateTrees(pMetrics->numNode, endpointsLeft, 0, 0, this, static_cast<generator_t::generateTreeCallback_t>(&selftestContext_t::foundTreeWindowCreate));
			generator.pRestartData = NULL;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * Pass 2, validate entries
		 */

		{
			// apply settings
			ctx.flags              = pMetrics->pure ? ctx.flags | context_t::MAGICMASK_PURE : ctx.flags & ~context_t::MAGICMASK_PURE;
			generator.windowLo     = 0;
			generator.windowHi     = 0;
			generator.pRestartData = restartData + restartIndex[pMetrics->numNode][pMetrics->pure];
			ctx.progressHi         = pMetrics->numProgress;
			ctx.progress           = 0;
			ctx.tick               = 0;

			generator.clearGenerator();
			generator.generateTrees(pMetrics->numNode, endpointsLeft, 0, 0, this, static_cast<generator_t::generateTreeCallback_t>(&selftestContext_t::foundTreeWindowVerify));
			generator.pRestartData = NULL;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		// release resources
		ctx.myFree("genrestartdataContext_t::selftestResults", selftestWindowResults);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed\n", ctx.timeAsString(), __FUNCTION__);
	}

	/**
	 * @date 2020-03-15 16:35:43
	 *
	 * Test that skins are properly encodes/decoded
	 */
	void performSelfTestSkin(void) {

		tinyTree_t tree(ctx);

		// `fwdTransform[3]` equals `"cabdefghi"` which is different than `revTransform[3]`
		assert(strcmp(pStore->fwdTransformNames[3], "cabdefghi") == 0);
		assert(strcmp(pStore->revTransformNames[3], "bcadefghi") == 0);

		// calculate `"abc!defg!!hi!"/cabdefghi"`
		tree.loadStringSafe("abc!defg!!hi!");
		footprint_t *pEncountered = pEvalFwd + tinyTree_t::TINYTREE_NEND * 3;
		tree.eval(pEncountered);

		// calculate `"cab!defg!!hi!"` (manually applying forward transform)
		tree.loadStringSafe("cab!defg!!hi!");
		footprint_t *pExpect = pEvalFwd;
		tree.eval(pExpect);

		// compare
		if (!pExpect[tree.root].equals(pEncountered[tree.root])) {
			printf("{\"error\":\"decode with skin failed\",\"where\":\"%s:%s:%d\"}\n",
			       __FUNCTION__, __FILE__, __LINE__);
			exit(1);
		}

		// test that cache lookups work
		// calculate `"abc!de!fabc!!"`
		tree.loadStringSafe("abc!de!fabc!!");
		tree.eval(pEvalFwd);

		const char *pExpectedName = tree.saveString(tree.root);

		// compare
		if (strcmp(pExpectedName, "abc!de!f2!") != 0) {
			printf("{\"error\":\"decode with cache failed\",\"where\":\"%s:%s:%d\",\"encountered\":\"%s\",\"expected\":\"%s\"}\n",
			       __FUNCTION__, __FILE__, __LINE__, pExpectedName, "abc!de!f2!");
			exit(1);
		}
	}

	/**
	 * @date 2020-03-12 00:26:06
	 *
	 * Test that forward/reverse transform complement each other.
	 *
	 * - Lookup of `""` should return the transparent transform
	 * - Verify short/long transform lookup work (it's a state machine).
	 * - Verify that forward/reverse counter each other
	 */
	void performSelfTestTransform(void) {

		unsigned numPassed = 0;

		/*
		 * Test empty name
		 */
		{
			unsigned tid = pStore->lookupTransform("", pStore->fwdTransformNameIndex);

			// test empty name is transparent skin
			if (tid != 0) {
				printf("{\"error\":\"failed empty name lookup\",\"where\":\"%s:%s:%d\",\"tid\":%u}\n",
				       __FUNCTION__, __FILE__, __LINE__, tid);
				exit(1);
			}

			// test transparent transform ([0]) is transparent
			for (unsigned k = 0; k < MAXSLOTS; k++) {
				if (pStore->fwdTransformNames[0][k] != (char) ('a' + k)) {
					printf("{\"error\":\"failed transparent forward\",\"where\":\"%s:%s:%d\",\"name\":\"%s\"}\n",
					       __FUNCTION__, __FILE__, __LINE__, pStore->fwdTransformNames[0]);
					exit(1);
				}
				if (pStore->revTransformNames[0][k] != (char) ('a' + k)) {
					printf("{\"error\":\"failed transparent reverse\",\"where\":\"%s:%s:%d\",\"name\":\"%s\"}\n",
					       __FUNCTION__, __FILE__, __LINE__, pStore->revTransformNames[0]);
					exit(1);
				}
			}
		}

		/*
		 * Perform two rounds, first with forward transform, then with reverse transform
		 */
		for (unsigned round = 0; round < 2; round++) {
			// setup data for this round
			transformName_t *pNames;
			uint32_t        *pIndex;

			if (round == 0) {
				pNames = pStore->fwdTransformNames;
				pIndex = pStore->fwdTransformNameIndex;
			} else {
				pNames = pStore->revTransformNames;
				pIndex = pStore->revTransformNameIndex;
			}

			/*
			 * Lookup all names with different lengths
			 */
			for (unsigned iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {
				for (unsigned iLen = 0; iLen < MAXSLOTS; iLen++) {

					char name[MAXSLOTS + 1];
					::strcpy(name, pNames[iTransform]);
					/*
					 * Test if substring is a short name
					 */
					bool          isShort = true;
					for (unsigned k       = iLen; k < MAXSLOTS; k++) {
						if (name[k] != (char) ('a' + k)) {
							isShort = false;
							break;
						}
					}

					// test if name can be truncated
					if (!isShort)
						continue;

					// truncate name
					name[iLen] = 0;

					// lookup name
					unsigned encountered = pStore->lookupTransform(name, pIndex);

					// undo truncation
					name[iLen] = 'a' + iLen;

					if (iTransform != encountered) {
						printf("{\"error\":\"failed lookup\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"round\":%u,\"iTransform\":%u,\"iLen\":%u,\"name\":\"%s\"}\n",
						       __FUNCTION__, __FILE__, __LINE__, encountered, round, iTransform, iLen, name);
						exit(1);
					}

					numPassed++;
				}
			}
		}

		/*
		 * Verify forward/reverse counter
		 */
		for (unsigned iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {
			// forward lookup
			unsigned forward = pStore->lookupTransform(pStore->fwdTransformNames[iTransform], pStore->fwdTransformNameIndex);

			// reverse lookup
			unsigned reverse = pStore->lookupTransform(pStore->fwdTransformNames[forward], pStore->fwdTransformNameIndex);

			if (reverse != iTransform) {
				printf("{\"error\":\"failed lookup\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n",
				       __FUNCTION__, __FILE__, __LINE__, reverse, iTransform);
				exit(1);
			}

			numPassed++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed %u tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-04-22 12:49:11
	 *
	 * Test that versioned memory for databases works as expected
	 */
	void performSelfTestVersioned(void) {
		// enable versioned memory. Don't change index size
		pStore->enableVersioned();

		// temporarily reduce size of index
		unsigned savSignatureIndexSize = pStore->signatureIndexSize;
		pStore->signatureIndexSize = 99;

		// clear signature index deliberately using memset instead of `InvalidateVersioned()`
		::memset(pStore->signatureIndex, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureIndex));
		::memset(pStore->signatureVersion, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureVersion));
		pStore->numSignature = 1; // skip reserved first entry

		/*
		 * add names to signatures until a collision occurs
		 */

		unsigned      ix1        = 0, collision1 = 0;
		for (unsigned iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {

			// reset counter
			ctx.cntHash = ctx.cntCompare = 0;

			// perform indux lookup
			ix1 = pStore->lookupSignature(pStore->fwdTransformNames[iTransform]);
			if (ctx.cntHash != ctx.cntCompare) {
				// this name caused the collision
				collision1 = iTransform;
				break;
			}
			pStore->signatureIndex[ix1]   = pStore->addSignature(pStore->fwdTransformNames[iTransform]); // mark as used
			pStore->signatureVersion[ix1] = pStore->iVersion;
		}
		if (!collision1) {
			printf("{\"error\":\"failed to find first hash overflow\",\"where\":\"%s:%s:%d\"}\n", __FUNCTION__, __FILE__, __LINE__);
			exit(1);
		}

		/*
		 * Reset index+data and find which name caused the hash overflow
		 */

		// clear signature index deliberately using memset instead of `InvalidateVersioned()`
		::memset(pStore->signatureIndex, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureIndex));
		::memset(pStore->signatureVersion, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureVersion));
		pStore->numSignature = 1; // skip reserved first entry

		// add the collision victim
		ix1 = pStore->lookupSignature(pStore->fwdTransformNames[collision1]);
		pStore->signatureIndex[ix1]   = pStore->addSignature(pStore->fwdTransformNames[collision1]); // mark as used
		pStore->signatureVersion[ix1] = pStore->iVersion;

		unsigned      ix2        = 0, collision2 = 0;
		for (unsigned iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {

			// reset counter
			ctx.cntHash = ctx.cntCompare = 0;

			// perform indux lookup
			ix2 = pStore->lookupSignature(pStore->fwdTransformNames[iTransform]);
			if (ctx.cntHash != ctx.cntCompare) {
				// this name caused the collision
				collision2 = iTransform;
				break;
			}
			pStore->signatureIndex[ix2]   = pStore->addSignature(pStore->fwdTransformNames[iTransform]); // mark as used
			pStore->signatureVersion[ix2] = pStore->iVersion;
		}
		if (!ix1) {
			printf("{\"error\":\"failed to find second hash overflow\",\"where\":\"%s:%s:%d\"}\n", __FUNCTION__, __FILE__, __LINE__);
			exit(1);
		} else if (ix1 == ix2) {
			printf("{\"error\":\"both hash overflow2 identical\",\"where\":\"%s:%s:%d\",\"ix\":%u,\"name\":\"%s\"}\n", __FUNCTION__, __FILE__, __LINE__, ix1, pStore->fwdTransformNames[collision1]);
			exit(1);
		}

		/*
		 * Reset index+data and test if entries can be deleted
		 */

		// clear signature index deliberately using memset instead of `InvalidateVersioned()`
		::memset(pStore->signatureIndex, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureIndex));
		::memset(pStore->signatureVersion, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureVersion));
		pStore->numSignature = 1; // skip reserved first entry

		// add the collisions
		ctx.cntHash = ctx.cntCompare = 0;
		ix1 = pStore->lookupSignature(pStore->fwdTransformNames[collision1]);
		pStore->signatureIndex[ix1]   = pStore->addSignature(pStore->fwdTransformNames[collision1]); // mark as used
		pStore->signatureVersion[ix1] = pStore->iVersion;
		ix2 = pStore->lookupSignature(pStore->fwdTransformNames[collision2]);
		pStore->signatureIndex[ix2]   = pStore->addSignature(pStore->fwdTransformNames[collision2]); // mark as used
		pStore->signatureVersion[ix2] = pStore->iVersion;
		assert(ctx.cntHash == 2 && ctx.cntCompare == 3);

		// delete first entry
		pStore->signatureIndex[ix1] = 0;

		// lookup second again
		unsigned ix = pStore->lookupSignature(pStore->fwdTransformNames[collision2]);

		if (ix == ix2) {
			if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] %s() passed\n", ctx.timeAsString(), __FUNCTION__);
		} else if (ix == ix1) {
			printf("{\"error\":\"delete broke overlow chain\",\"where\":\"%s:%s:%d\",\"ix1\":%u,\"name1\":\"%s\",\"ix2\":%u,\"name2\":\"%s\"}\n",
			       __FUNCTION__, __FILE__, __LINE__, ix1, pStore->fwdTransformNames[collision1], ix2, pStore->fwdTransformNames[collision2]);
			exit(1);
		} else {
			printf("{\"error\":\"delete corrupted index\",\"where\":\"%s:%s:%d\",\"ix\":%u,\"ix1\":%u,\"name1\":\"%s\",\"ix2\":%u,\"name2\":\"%s\"}\n",
			       __FUNCTION__, __FILE__, __LINE__, ix, ix1, pStore->fwdTransformNames[collision1], ix, pStore->fwdTransformNames[collision2]);
			exit(1);
		}

		// restore original index size
		pStore->signatureIndexSize = savSignatureIndexSize;

		// disable versioned memory
		pStore->disableVersioned();

	}

	/**
	 * @date 2020-03-15 12:13:13
	 *
	 * Test Row/Column
	 *
	 * The list of transform names has repetitive properties which give the enumerated id's modulo properties.
	 *
	 * A modulo property is that the enumeration can be written as `"(row * numCols) + col"`.
	 * `"row"` and `"col"` are dimensions of a rectangle large enough to fit the entire collection of id'd.
	 *
	 * This can be illustrated by the following example with 5-letter transform names with 5! (=120) permutations.
	 * The example rectangle has 1*2*3 (=6) columns and 4*5 (=20) rows.
	 * The names are placed in a left-to-right, top-to-bottom sequence.
	 *
	 *     +<--------------COLUMNS------------->
	 *     ^ abcde bacde acbde cabde bcade cbade
	 *     | abdce badce adbce dabce bdace dbace
	 *     | acdbe cadbe adcbe dacbe cdabe dcabe
	 *     | bcdae cbdae bdcae dbcae cdbae dcbae
	 *     | abced baced acbed cabed bcaed cbaed
	 *     | abecd baecd aebcd eabcd beacd ebacd
	 *     | acebd caebd aecbd eacbd ceabd ecabd
	 *     | bcead cbead becad ebcad cebad ecbad
	 *     R abdec badec adbec dabec bdaec dbaec
	 *     O abedc baedc aebdc eabdc beadc ebadc
	 *     W adebc daebc aedbc eadbc deabc edabc
	 *     S bdeac dbeac bedac ebdac debac edbac
	 *     | acdeb cadeb adceb daceb cdaeb dcaeb
	 *     | acedb caedb aecdb eacdb ceadb ecadb
	 *     | adecb daecb aedcb eadcb deacb edacb
	 *     | cdeab dceab cedab ecdab decab edcab
	 *     | bcdea cbdea bdcea dbcea cdbea dcbea
	 *     | bceda cbeda becda ebcda cebda ecbda
	 *     | bdeca dbeca bedca ebdca debca edbca
	 *     v cdeba dceba cedba ecdba decba edcba
	 *
	 * Examining the lower-right cell the following `"placeholder/skin"` property applies:
	 *
	 *      `"cbade/cdeba -> edcba"`
	 *          ^     ^        ^
	 *          |     |        +- cell
	 *          |     +- first cell of grid row
	 *          +- first cell of grid column
	 *
	 * Rewriting all the names in `"placeholder/skin"` notation
	 *
	 *     +<--------------------------------COLUMNS------------------------------->
	 *     ^ abcde/abcde bacde/abcde acbde/abcde cabde/abcde bcade/abcde cbade/abcde
	 *     | abcde/abdce bacde/abdce acbde/abdce cabde/abdce bcade/abdce cbade/abdce
	 *     | abcde/acdbe bacde/acdbe acbde/acdbe cabde/acdbe bcade/acdbe cbade/acdbe
	 *     | abcde/bcdae bacde/bcdae acbde/bcdae cabde/bcdae bcade/bcdae cbade/bcdae
	 *     | abcde/abced bacde/abced acbde/abced cabde/abced bcade/abced cbade/abced
	 *     | abcde/abecd bacde/abecd acbde/abecd cabde/abecd bcade/abecd cbade/abecd
	 *     | abcde/acebd bacde/acebd acbde/acebd cabde/acebd bcade/acebd cbade/acebd
	 *     | abcde/bcead bacde/bcead acbde/bcead cabde/bcead bcade/bcead cbade/bcead
	 *     R abcde/abdec bacde/abdec acbde/abdec cabde/abdec bcade/abdec cbade/abdec
	 *     O abcde/abedc bacde/abedc acbde/abedc cabde/abedc bcade/abedc cbade/abedc
	 *     W abcde/adebc bacde/adebc acbde/adebc cabde/adebc bcade/adebc cbade/adebc
	 *     S abcde/bdeac bacde/bdeac acbde/bdeac cabde/bdeac bcade/bdeac cbade/bdeac
	 *     | abcde/acdeb bacde/acdeb acbde/acdeb cabde/acdeb bcade/acdeb cbade/acdeb
	 *     | abcde/acedb bacde/acedb acbde/acedb cabde/acedb bcade/acedb cbade/acedb
	 *     | abcde/adecb bacde/adecb acbde/adecb cabde/adecb bcade/adecb cbade/adecb
	 *     | abcde/cdeab bacde/cdeab acbde/cdeab cabde/cdeab bcade/cdeab cbade/cdeab
	 *     | abcde/bcdea bacde/bcdea acbde/bcdea cabde/bcdea bcade/bcdea cbade/bcdea
	 *     | abcde/bceda bacde/bceda acbde/bceda cabde/bceda bcade/bceda cbade/bceda
	 *     | abcde/bdeca bacde/bdeca acbde/bdeca cabde/bdeca bcade/bdeca cbade/bdeca
	 *     v abcde/cdeba bacde/cdeba acbde/cdeba cabde/cdeba bcade/cdeba cbade/cdeba
	 *
	 * To make the patterns more obvious, replace names by their enumerated id's
	 *
	 *     +<-----------------------COLUMNS---------------------->
	 *     | 0/(6* 0) 1/(6* 0) 2/(6* 0) 3/(6* 0) 4/(6* 0) 5/(6* 0)
	 *     | 0/(6* 1) 1/(6* 1) 2/(6* 1) 3/(6* 1) 4/(6* 1) 5/(6* 1)
	 *     | 0/(6* 2) 1/(6* 2) 2/(6* 2) 3/(6* 2) 4/(6* 2) 5/(6* 2)
	 *     | 0/(6* 3) 1/(6* 3) 2/(6* 3) 3/(6* 3) 4/(6* 3) 5/(6* 3)
	 *     | 0/(6* 4) 1/(6* 4) 2/(6* 4) 3/(6* 4) 4/(6* 4) 5/(6* 4)
	 *     | 0/(6* 5) 1/(6* 5) 2/(6* 5) 3/(6* 5) 4/(6* 5) 5/(6* 5)
	 *     | 0/(6* 6) 1/(6* 6) 2/(6* 6) 3/(6* 6) 4/(6* 6) 5/(6* 6)
	 *     | 0/(6* 7) 1/(6* 7) 2/(6* 7)store.fwdTransformData 3/(6* 7) 4/(6* 7) 5/(6* 7)
	 *     R 0/(6* 8) 1/(6* 8) 2/(6* 8) 3/(6* 8) 4/(6* 8) 5/(6* 8)
	 *     O 0/(6* 9) 1/(6* 9) 2/(6* 9) 3/(6* 9) 4/(6* 9) 5/(6* 9)
	 *     W 0/(6*10) 1/(6*10) 2/(6*10) 3/(6*10) 4/(6*10) 5/(6*10)
	 *     S 0/(6*11) 1/(6*11) 2/(6*11) 3/(6*11) 4/(6*11) 5/(6*11)
	 *     | 0/(6*12) 1/(6*12) 2/(6*12) 3/(6*12) 4/(6*12) 5/(6*12)
	 *     | 0/(6*13) 1/(6*13) 2/(6*13) 3/(6*13) 4/(6*13) 5/(6*13)
	 *     | 0/(6*14) 1/(6*14) 2/(6*14) 3/(6*14) 4/(6*14) 5/(6*14)
	 *     | 0/(6*15) 1/(6*15) 2/(6*15) 3/(6*15) 4/(6*15) 5/(6*15)
	 *     | 0/(6*16) 1/(6*16) 2/(6*16) 3/(6*16) 4/(6*16) 5/(6*16)
	 *     | 0/(6*17) 1/(6*17) 2/(6*17) 3/(6*17) 4/(6*17) 5/(6*17)
	 *     | 0/(6*18) 1/(6*18) 2/(6*18) 3/(6*18) 4/(6*18) 5/(6*18)
	 *     v 0/(6*19) 1/(6*19) 2/(6*19) 3/(6*19) 4/(6*19) 5/(6*19)
	 *
	 * This implies that only knowledge of the first cell of each row and column are needed to reconstruct any cell name.
	 * The mechanism is called `"interleaving"`.
	 */
	void performSelfTestRowCol(void) {

		// shortcuts
		transformName_t *pFwdNames = pStore->fwdTransformNames;
		transformName_t *pRevNames = pStore->revTransformNames;

		unsigned numPassed        = 0;

		unsigned numRows, numCols = 1;

		/*
		 * Apply test to eash possible grid layout
		 */
		for (unsigned iInterleave = 1; iInterleave <= MAXSLOTS; iInterleave++) {
			// number of columns must be iInterleave!
			numCols *= iInterleave;
			numRows = MAXTRANSFORM / numCols;
			assert(numCols * numRows == MAXTRANSFORM);

			/*
			 * walk through cells.
			 */
			for (unsigned row = 0; row < numRows; row++) {
				for (unsigned col = 0; col < numCols; col++) {

					/*
					 * Validate "<first cell of grid column>/<first cell of grid row>" == "<cell>"
					 */

					char cell[10];

					// construct cell name
					cell[0] = pFwdNames[row * numCols][pFwdNames[col][0] - 'a'];
					cell[1] = pFwdNames[row * numCols][pFwdNames[col][1] - 'a'];
					cell[2] = pFwdNames[row * numCols][pFwdNames[col][2] - 'a'];
					cell[3] = pFwdNames[row * numCols][pFwdNames[col][3] - 'a'];
					cell[4] = pFwdNames[row * numCols][pFwdNames[col][4] - 'a'];
					cell[5] = pFwdNames[row * numCols][pFwdNames[col][5] - 'a'];
					cell[6] = pFwdNames[row * numCols][pFwdNames[col][6] - 'a'];
					cell[7] = pFwdNames[row * numCols][pFwdNames[col][7] - 'a'];
					cell[8] = pFwdNames[row * numCols][pFwdNames[col][8] - 'a'];
					cell[9] = 0;

					// check
					if (strcmp(cell, pFwdNames[row * numCols + col]) != 0) {
						printf("{\"error\":\"failed merge\",\"where\":\"%s:%s:%d\",\"encountered\":\"%s\",\"expected\":\"%s\",\"numCols\":%u,\"numRows\":%u,\"col\":%u,\"colName\":\"%s\",\"row\":%u,\"rowName\":\"%s\"}\n",
						       __FUNCTION__, __FILE__, __LINE__, cell, pFwdNames[row * numCols + col], numCols, numRows, col, pFwdNames[col], row * numCols, pFwdNames[row * numCols]);
						exit(1);
					}

					numPassed++;

					/*
					 * If the above applies, then the following should be valid
					 */

					assert(pFwdNames[(row * numCols) + col][0] == pFwdNames[(row * numCols)][pFwdNames[col][0] - 'a']);
					assert(pFwdNames[(row * numCols) + col][1] == pFwdNames[(row * numCols)][pFwdNames[col][1] - 'a']);
					assert(pFwdNames[(row * numCols) + col][2] == pFwdNames[(row * numCols)][pFwdNames[col][2] - 'a']);
					assert(pFwdNames[(row * numCols) + col][3] == pFwdNames[(row * numCols)][pFwdNames[col][3] - 'a']);
					assert(pFwdNames[(row * numCols) + col][4] == pFwdNames[(row * numCols)][pFwdNames[col][4] - 'a']);
					assert(pFwdNames[(row * numCols) + col][5] == pFwdNames[(row * numCols)][pFwdNames[col][5] - 'a']);
					assert(pFwdNames[(row * numCols) + col][6] == pFwdNames[(row * numCols)][pFwdNames[col][6] - 'a']);
					assert(pFwdNames[(row * numCols) + col][7] == pFwdNames[(row * numCols)][pFwdNames[col][7] - 'a']);
					assert(pFwdNames[(row * numCols) + col][8] == pFwdNames[(row * numCols)][pFwdNames[col][8] - 'a']);

					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][0] - 'a'] == pFwdNames[col][0]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][1] - 'a'] == pFwdNames[col][1]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][2] - 'a'] == pFwdNames[col][2]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][3] - 'a'] == pFwdNames[col][3]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][4] - 'a'] == pFwdNames[col][4]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][5] - 'a'] == pFwdNames[col][5]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][6] - 'a'] == pFwdNames[col][6]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][7] - 'a'] == pFwdNames[col][7]);
					assert(pRevNames[(row * numCols)][pFwdNames[(row * numCols) + col][8] - 'a'] == pFwdNames[col][8]);
				}
			}
		}

		fprintf(stderr, "[%s] %s() passed %u tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-03-15 16:35:43
	 *
	 * Test that associative imprint lookups are working as expected.
	 *
	 * Searching for footprints requires an associative.
	 * A database lookup for a footprint will return an ordered structure and skin.
	 * Evaluating the "structure/skin" will result in the requested footprint.
	 *
	 * Two extreme implementations are:
	 *
	 * - Store and index all 9! possible permutations of the footprint.
	 *   Fastest runtime speed but at an extreme high storage cost.
	 *
	 * - Store the ordered structure.
	 *   During runtime, apply all 9! skin permutations to the footprint
	 *   and perform a database lookup to determine if a matching ordered structure exists.
	 *   Most efficient data storage with an extreme high performance hit.
	 *
	 * The chosen implementation is to take advantage of interleaving properties as described for `performSelfTestInterleave()`
	 * It describes that any transform permutatuion can be achieved by only knowing key column and row entries.
	 *
	 * Demonstrate that for any given footprint it will re-orientate
	 * @param {footprint_t} pEvalFwd - evaluation vector with forward transform
	 * @param {footprint_t} pEvalRev - evaluation vector with reverse transform
	 */
	void performSelfTestInterleave(void) {

		unsigned numPassed = 0;

		// test name. NOTE: this is deliberately "not ordered"
		const char *pBasename = "abc!defg!!hi!";

		tinyTree_t tree(ctx);

		/*
		 * @date 2020-03-17 00:34:54
		 *
		 * Generate all possible situations
		 *
		 * With regard to storage/speed trade-offs, only 4 row/column combos are viable.
		 * Storage is based on worst-case scenario.
		 * Actual storage needs to be tested/runtime decided.
		 */

		for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {
			if (pInterleave->noauto & 2)
				continue; // skip automated handling
			if (pInterleave->numSlot != MAXSLOTS)
				continue; // only process settings that match `MAXSLOTS`

			/*
			 * Setup database and erase indices
			 */

			// mode
			pStore->interleave     = pInterleave->numStored;
			pStore->interleaveStep = pInterleave->interleaveStep;

			// clear database imprint and index
			::memset(pStore->imprintIndex, 0, pStore->imprintIndexSize * sizeof(*pStore->imprintIndex));
			pStore->numImprint = 1; // skip reserved entry

			/*
			 * Create a test 4n9 tree with unique endpoints so each permutation is unique.
			 */

			tree.loadStringFast(pBasename);

			// add to database
			pStore->addImprintAssociative(&tree, pEvalFwd, pEvalRev, 0);

			/*
			 * Lookup all possible permutations
			 */

			time_t        seconds    = ::time(NULL);
			for (unsigned iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {

				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					fprintf(stderr, "\r[%s] %.5f%%", ctx.timeAsString(), iTransform * 100.0 / MAXTRANSFORM);
					ctx.tick = 0;
				}

				// Load base name with skin
				tree.loadStringFast(pBasename, pStore->fwdTransformNames[iTransform]);

				unsigned sid, tid;

				// lookup
				if (!pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &sid, &tid)) {
					printf("{\"error\":\"tree not found\",\"where\":\"%s:%s:%d\",\"interleave\":%u,\"tid\":\"%s\"}\n",
					       __FUNCTION__, __FILE__, __LINE__, pStore->interleave, pStore->fwdTransformNames[iTransform]);
					exit(1);
				}

				// test that transform id's match
				if (iTransform != tid) {
					printf("{\"error\":\"tid lookup mismatch\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n",
					       __FUNCTION__, __FILE__, __LINE__, tid, iTransform);
					exit(1);
				}

				numPassed++;

			}

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");


			seconds         = ::time(NULL) - seconds;
			if (seconds == 0)
				seconds = 1;

			// base estimated size on 791647 signatures
			fprintf(stderr, "[%s] metricsInterleave_t { /*numSlot=*/%u, /*interleave=*/%u, /*numStored=*/%u, /*numRuntime=*/%u, /*speed=*/%u, /*storage=*/%.3f},\n",
				ctx.timeAsString(), MAXSLOTS, pStore->interleave, pStore->numImprint - 1, MAXTRANSFORM / (pStore->numImprint - 1),
				(int) (MAXTRANSFORM / seconds), (sizeof(imprint_t) * 791647 * pStore->numImprint) / 1.0e9);

			// test that number of imprints match
			if (pInterleave->numStored != pStore->numImprint - 1) {
				printf("{\"error\":\"numImprint mismatch\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n",
				       __FUNCTION__, __FILE__, __LINE__, pStore->numImprint - 1, pInterleave->numStored);
				exit(1);
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed %u tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-04-22 17:54:06
	 *
	 * Collect candidates with back-references
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeCompare(tinyTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			if (ctx.progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", ctx.timeAsString(), pStore->numSignature * 100.0 / ctx.progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %u", ctx.timeAsString(), pStore->numSignature);
			ctx.tick = 0;
		}

		if (numBackRef != 0) {
			unsigned ix = pStore->lookupSignature(pName);
			if (pStore->signatureIndex[ix] == 0)
				pStore->signatureIndex[ix] = pStore->addSignature(pName);
		}

		return true;
	}

	/**
	 * @date 2020-03-27 17:05:07
	 *
	 * Compare function for `qsort_r`
	 *
	 * @param {signature_t} lhs - left hand side signature
	 * @param {signature_t} rhs - right hand side signature
	 * @param {context_t} arg - I/O context
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int comparSignature(const void *lhs, const void *rhs, void *arg) {
		if (lhs == rhs)
			return 0;

		const signature_t *pSignatureL = static_cast<const signature_t *>(lhs);
		const signature_t *pSignatureR = static_cast<const signature_t *>(rhs);
		context_t         *pApp        = static_cast<context_t *>(arg);

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.loadStringFast(pSignatureL->name);
		treeR.loadStringFast(pSignatureR->name);

		/*
		 * Compare
		 */

		return treeL.compare(treeL.root, &treeR, treeR.root);
	}

	/*
	 * @date 2020-04-22 17:41:56
	 *
	 * Test the consistency of tree comparing.
	 *
	 * - Collect 4n9 signatures with back-references.
	 * - Shuffle and sort
	 * - Compare every adjacent signature
	 *
	 * Perform multiple rounds to check that initial pre-sort ordering does not effect the compare
	 */
	void performSelfTestCompare(void) {
		unsigned numPassed = 0;

		// reset index
		::memset(pStore->signatureIndex, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureIndex));
		pStore->numSignature = 1; // skip reserved first entry

		// apply settings
		ctx.flags          = 0;
		generator.windowLo = 0;
		generator.windowHi = 0;
		ctx.setupSpeed(16119595);

		generator.initialiseGenerator(ctx.flags & context_t::MAGICMASK_PURE);
		generator.clearGenerator();
		unsigned numNodes     = 4;
		unsigned numEndpoints = numNodes * 2 + 1;
		generator.generateTrees(numNodes, numEndpoints, 0, 0, this, static_cast<generator_t::generateTreeCallback_t>(&selftestContext_t::foundTreeCompare));

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		tinyTree_t treeL(ctx);
		tinyTree_t treeR(ctx);

		unsigned      maxRound = 4;
		for (unsigned iRound   = 0; iRound < maxRound; iRound++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] collecting round=%u/%u\n", ctx.timeAsString(), iRound + 1, maxRound);

			// apply fisher/yates shuffle. Skip first reserved entry
			for (unsigned j = pStore->numSignature - 1; j >= 3; j--) {
				unsigned k = ::rand() % (j - 2) + 1; // range "`0%(j-1)+1=1"` - "`(j-2)%(j-2)+1=j-1"`
				assert(k < j);

				// swap j/k
				if (j != k) {
					signature_t tmp = pStore->signatures[j];
					pStore->signatures[j] = pStore->signatures[k];
					pStore->signatures[k] = tmp;
				}
			}

			if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] sorting %u signatures\n", ctx.timeAsString(), pStore->numSignature);

			/*
			 * Sort signatures.
			 */
			assert(pStore->numSignature >= 1);
			qsort_r(pStore->signatures + 1, pStore->numSignature - 1, sizeof(*pStore->signatures), this->comparSignature, &ctx);

			if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] comparing signatures\n", ctx.timeAsString());

			ctx.setupSpeed(pStore->numSignature);
			ctx.progress = 3;

			/*
			 * two-way compare adjacent signatures
			 */
			for (unsigned j = 3; j < pStore->numSignature; j++) {
				ctx.progress++;

				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();
					int eta       = (int) ((ctx.progressHi - ctx.progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d",
						ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS);

					ctx.tick = 0;
				}

				/*
				 * Compare
				 */
				int cmp;

				treeL.loadStringFast(pStore->signatures[j - 1].name);
				treeR.loadStringFast(pStore->signatures[j].name);
				cmp = treeL.compare(treeL.root, &treeR, treeR.root);
				if (cmp > 0) {
					printf("{\"error\":\"compare fail\",\"where\":\"%s:%s:%d\",\"result\":%d,\",first\":\"%s\",second\":\"%s\"}\n",
					       __FUNCTION__, __FILE__, __LINE__, cmp, pStore->signatures[j - 1].name, pStore->signatures[j].name);
					exit(1);
				}

				treeL.loadStringFast(pStore->signatures[j].name);
				treeR.loadStringFast(pStore->signatures[j - 1].name);
				cmp = treeL.compare(treeL.root, &treeR, treeR.root);
				if (cmp < 0) {
					printf("{\"error\":\"compare fail\",\"where\":\"%s:%s:%d\",\"result\":%d,\",first\":\"%s\",second\":\"%s\"}\n",
					       __FUNCTION__, __FILE__, __LINE__, cmp, pStore->signatures[j].name, pStore->signatures[j - 1].name);
					exit(1);
				}

				numPassed++;
			}

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");
		}


		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed %u tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-03-24 23:57:51
	 *
	 * Perform an associative lookup to determine signature footprint (sid) and orientation (tid)
	 * expand collection of unique structures.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - tree notation/name
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeMetrics(tinyTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | hash=%.3f",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
					pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					(double) ctx.cntCompare / ctx.cntHash);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numSignature=%u(%.0f%%) numImprint=%u(%.0f%%) | hash=%.3f",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
					pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
					pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					(double) ctx.cntCompare / ctx.cntHash);
			}

			ctx.tick = 0;
		}

		// lookup
		unsigned sid = 0;
		unsigned tid = 0;

		pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);

		if (sid == 0) {
			// add to database
			sid = pStore->addSignature(pName);
			pStore->addImprintAssociative(&tree, pEvalFwd, pEvalRev, sid);
		}

		return true;
	}

	/**
	 * Create metrics for `metricsImprint[]`.
	 * It uses `metricsImprint[]`, select all `auto` entries and setups a database accordingly.
	 */
	void createMetrics(void) {

		/*
		 * Scan metrics for setting that require metrics to be collected
		 */
		for (const metricsImprint_t *pRound = metricsImprint; pRound->numSlot; pRound++) {

			if (pRound->noauto & 2)
				continue; // skip automated handling
			if (pRound->numSlot != MAXSLOTS)
				continue; // only process settings that match `MAXSLOTS`

			// set index to default ratio
			pStore->imprintIndexSize = ctx.nextPrime(pRound->numImprint * (METRICS_DEFAULT_RATIO / 10.0));

			// find metrics for setting
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, pRound->pure, pRound->numNode);
			assert(pMetrics);
			const metricsInterleave_t *pInterleave = getMetricsInterleave(MAXSLOTS, pRound->interleave);
			assert(pInterleave);

			// prepare database
			::memset(pStore->imprintIndex, 0, pStore->imprintIndexSize * sizeof(*pStore->imprintIndex));
			::memset(pStore->signatureIndex, 0, pStore->signatureIndexSize * sizeof(*pStore->signatureIndex));
			pStore->numImprint     = 1; // skip reserved first entry
			pStore->numSignature   = 1; // skip reserved first entry
			pStore->interleave     = pInterleave->numStored;
			pStore->interleaveStep = pInterleave->interleaveStep;

			// prepare generator
			ctx.flags = pRound->pure ? ctx.flags | context_t::MAGICMASK_PURE : ctx.flags & ~context_t::MAGICMASK_PURE;
			generator.initialiseGenerator(ctx.flags & context_t::MAGICMASK_PURE); // let flags take effect

			// prepare I/O context
			ctx.setupSpeed(pMetrics ? pMetrics->numProgress : 0);
			ctx.tick = 0;

			// special case (root only)
			tinyTree_t tree(ctx);
			tree.root = 0; // "0"
			foundTreeMetrics(tree, "0", 0, 0, 0);
			tree.root = 1; // "a"
			foundTreeMetrics(tree, "a", 1, 1, 0);

			// regulars
			unsigned endpointsLeft = pRound->numNode * 2 + 1;

			// count signatures and imprints
			generator.clearGenerator();
			generator.generateTrees(pRound->numNode, endpointsLeft, 0, 0, this, static_cast<generator_t::generateTreeCallback_t>(&selftestContext_t::foundTreeMetrics));

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			// estimate speed and storage for default ratio
			double speed   = 0; // in M/s
			double storage = 0; // in Gb

			ctx.cntHash    = 0;
			ctx.cntCompare = 0;

			if (this->opt_metrics) {
				tinyTree_t tree(ctx);

				// wait for a tick
				for (ctx.tick = 0; ctx.tick == 0;)
					tree.loadStringFast("ab+"); // waste some time

				// do random lookups for 10 seconds
				for (ctx.tick = 0; ctx.tick < 5;) {
					// load random signature with random tree
					unsigned sid = (rand() % (pStore->numSignature - 1)) + 1;
					unsigned tid = rand() % pStore->numTransform;

					// load tree
					tree.loadStringFast(pStore->signatures[sid].name, pStore->fwdTransformNames[tid]);

					// perform a lookup
					unsigned s = 0, t = 0;
					pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &s, &t);
					assert(sid == s);
				}
			}

			speed   = ctx.cntHash / 5.0 / 1e6;
			storage = ((sizeof(*pStore->imprints) * pStore->numImprint) + (sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize)) / 1e9;

			fprintf(stderr, "[%s] numSlot=%u pure=%u interleave=%-4d numNode=%u numSignature=%u(%.0f%%) numImprint=%u(%.0f%% speed=%.3fM/s storage=%.3fGb\n",
				ctx.timeAsString(), MAXSLOTS, pRound->pure, pRound->interleave, pRound->numNode,
				pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				speed, storage);

			if (ctx.progress != ctx.progressHi) {
				printf("{\"error\":\"progressHi failed\",\"where\":\"%s:%s:%d\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%u}\n",
				       __FUNCTION__, __FILE__, __LINE__, ctx.progress, ctx.progressHi, pRound->numNode);
			}

			/*
			 * re-index data to find ratio effects
			 */

			// what you wish...
			if (this->opt_metrics != 2)
				continue; // not requested
			if (pRound->numNode != 4)
				continue; // no point for smaller trees

			for (unsigned iRatio = 20; iRatio <= 60; iRatio += 2) {

				assert(iRatio / 10.0 <= this->opt_ratio);
				pStore->imprintIndexSize = ctx.nextPrime(pRound->numImprint * (iRatio / 10.0));

				// clear imprint index
				::memset(pStore->imprintIndex, 0, pStore->imprintIndexSize * sizeof(*pStore->imprintIndex));
				pStore->numImprint = 1; // skip mandatory reserved entry
				ctx.cntHash        = 0;
				ctx.cntCompare     = 0;

				fprintf(stderr, "[numImprint=%u imprintIndexSize=%u ratio=%.1f]", pStore->numImprint, pStore->imprintIndexSize, iRatio / 10.0);

				tinyTree_t tree(ctx);

				// reindex
				for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
					const signature_t *pSignature = pStore->signatures + iSid;

					tree.loadStringFast(pSignature->name);
					pStore->addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);
				}

				fprintf(stderr, "\r\e[K[numImprint=%u imprintIndexSize=%u ratio=%.1f cntHash=%lu cntCompare=%lu hash=%.5f]", pStore->numImprint, pStore->imprintIndexSize, iRatio / 10.0, ctx.cntHash, ctx.cntCompare, (double) ctx.cntCompare / ctx.cntHash);

				/*
				 * perform a speedtest
				 */

				ctx.cntHash    = 0;
				ctx.cntCompare = 0;

				// wait for a tick
				for (ctx.tick = 0; ctx.tick == 0;) {
					tree.loadStringFast("ab+"); // waste some time
				}

				// do random lookups for 10 seconds
				for (ctx.tick = 0; ctx.tick < 5;) {
					// load random signature with random tree
					unsigned sid = (rand() % (pStore->numSignature - 1)) + 1;
					unsigned tid = rand() % pStore->numTransform;

					// load tree
					tree.loadStringFast(pStore->signatures[sid].name, pStore->fwdTransformNames[tid]);

					// perform a lookup
					unsigned s = 0, t = 0;
					pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &s, &t);
					assert(sid == s);
				}

				fprintf(stderr, "[speed=%7.3fM/s storage=%7.3fG hits=%.5f]\n",
					ctx.cntHash / 5.0 / 1e6,
					((sizeof(*pStore->imprints) * pStore->numImprint) + (sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize)) / 1e9,
					(double) ctx.cntCompare / ctx.cntHash);
			}

		}
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
 * @global {selftestContext_t} Application context
 */
selftestContext_t app(ctx);

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
	fprintf(stderr, "usage: %s <input.db> [<numnode> [<output.db>]]   -- Add signatures of given node size\n", argv[0]);
	fprintf(stderr, "       %s --metrics=1 <input.db>                 -- Collect medium metrics for `metricsImprint[]`\n", argv[0]);
	fprintf(stderr, "       %s --metrics=2 <input.db>                 -- Collect slow metrics for `ratioMetrics[]`\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --maximprint=<number>           Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxsignature=<number>         Maximum number of signatures [default=%u]\n", app.opt_maxSignature);
		fprintf(stderr, "\t   --metrics=<number>              Collect metrics\n");
		fprintf(stderr, "\t   --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose                       Say more\n");
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
			// long-only opts
			LO_DEBUG   = 1,
			LO_IMPRINTINDEXSIZE,
			LO_METRICS,
			LO_NOPARANOID,
			LO_PARANOID,
			LO_SIGNATUREINDEXSIZE,
			LO_TEXT,
			LO_TIMER,
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",              1, 0, LO_DEBUG},
			{"help",               0, 0, LO_HELP},
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"metrics",            2, 0, LO_METRICS},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"paranoid",           0, 0, LO_PARANOID},
			{"quiet",              2, 0, LO_QUIET},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"text",               2, 0, LO_TEXT},
			{"timer",              1, 0, LO_TIMER},
			{"verbose",            2, 0, LO_VERBOSE},
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
		*cp        = '\0';

		// parse long options
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_DEBUG:
			ctx.opt_debug = ::strtoul(optarg, NULL, 0);
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_IMPRINTINDEXSIZE:
			app.opt_imprintIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_METRICS:
			app.opt_metrics = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_metrics + 1;
			break;
		case LO_NOPARANOID:
			ctx.flags &= ~context_t::MAGICMASK_PARANOID;
			break;
		case LO_PARANOID:
			ctx.flags |= context_t::MAGICMASK_PARANOID;
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
			break;
		case LO_SIGNATUREINDEXSIZE:
			app.opt_signatureIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_TEXT:
			app.opt_text = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_text + 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = ::strtoul(optarg, NULL, 0);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
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

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Test that tree name encodng/decoding works as expected
	 */
	app.performSelfTestTreeName();

	/*
	 * Test that `tinyTree_t` does level-2 normalisation
	 */
	app.performSelfTestTreeNormaliseLevel2();

	/*
	 * Test that evaluating `tinyTree_t` is working as expected
	 */
	app.performSelfTestTreeEval();

	/*
	 * Test that generator restart/windowing is working as expected
	 */
	app.performSelfTestWindow();

	if (!app.arg_inputDatabase) {
		/*
		 * @date 2020-04-22 00:48:47
		 *
		 * Using backquotes is a nice replacement for using quotes to mark literals in text.
		 * Otherwise you would have a rainfall of backslashes.
		 */
		fprintf(stderr, "[%s] Skipping tests that require a database with a `transform` section.\n", ctx.timeAsString());
		exit(1);
	}

	/*
	 * Open input database
	 */

	// Open database
	database_t db(ctx);

	db.open(app.arg_inputDatabase);

	// display system flags when database was created
	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		char dbText[128], ctxText[128];

		ctx.flagsToText(db.creationFlags, dbText);
		ctx.flagsToText(ctx.flags, ctxText);

		if (db.creationFlags != ctx.flags)
			fprintf(stderr, "[%s] WARNING: Database/system flags differ: database=[%s] current=[%s]\n", ctx.timeAsString(), dbText, ctxText);
		else if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), dbText);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));

	/*
	 * create output
	 */

	database_t store(ctx);

	// for `createMetrics()`
	if (app.opt_metrics) {
		// get worse-case values

		// `createMetrics()` required ratio of at least 6.0
		if (app.opt_metrics == 2)
			app.opt_ratio = 6.0;

		unsigned highestNumNode = 0; // collect highest `numNode` from `metricsGenerator`

		// get highest `numNode` and `numImprint`
		if (app.opt_maxImprint == 0) {
			for (const metricsImprint_t *pMetrics = metricsImprint; pMetrics->numSlot; pMetrics++) {
				if (pMetrics->noauto & 2)
					continue; // skip automated handling
				if (pMetrics->numSlot != MAXSLOTS)
					continue; // only process settings that match `MAXSLOTS`

				if (store.maxImprint < pMetrics->numImprint)
					store.maxImprint = pMetrics->numImprint;
				if (highestNumNode < pMetrics->numNode)
					highestNumNode = pMetrics->numNode;
			}

			// Give extra 5% expansion space
			store.maxImprint = store.maxImprint;
		}

		// get highest `numSignature` but only for the highest `numNode` found above
		if (app.opt_maxSignature == 0) {
			for (const metricsGenerator_t *pMetrics = metricsGenerator; pMetrics->numSlot; pMetrics++) {
				if (pMetrics->noauto & 2)
					continue; // skip automated handling
				if (pMetrics->numSlot != MAXSLOTS)
					continue; // only process settings that match `MAXSLOTS`
				if (pMetrics->numNode > highestNumNode)
					continue; // must match imprints above

				if (store.maxSignature < pMetrics->numSignature)
					store.maxSignature = pMetrics->numSignature;
			}

			// Give extra 5% expansion space
			store.maxSignature = store.maxSignature;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Set limits to maxImprint=%u maxSignature=%u\n", ctx.timeAsString(), store.maxImprint, store.maxSignature);
	}

	// signatures
	if (store.maxSignature < MAXTRANSFORM)
		store.maxSignature = MAXTRANSFORM; // must hold all transforms for `performSelfTestVersioned()`
	if (store.maxSignature < db.numSignature)
		store.maxSignature = db.numSignature; // must hold all input signatures
	if (store.maxSignature < 17000000)
		store.maxSignature = 17000000; // must hold (nearly) candidates for `performSelfTestCompare()`

	store.signatureIndexSize = ctx.nextPrime(store.maxSignature * app.opt_ratio);

	// imprints for `performSelfTestInterleave()`
	if (store.maxImprint < MAXTRANSFORM + 10) // = 362880+10
		store.maxImprint = MAXTRANSFORM + 10;

	store.imprintIndexSize = ctx.nextPrime(store.maxImprint * app.opt_ratio);

	if (store.imprintIndexSize < 362897) // =362880+17 force extreme index overflowing
		store.imprintIndexSize = 362897;

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("gensignatureContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("gensignatureContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	/*
	 * Finalise allocations and create database
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		// Assuming with database allocations included
		size_t allocated = ctx.totalAllocated + store.estimateMemoryUsage(0);

		struct sysinfo info;
		if (sysinfo(&info) == 0) {
			double percent = 100.0 * allocated / info.freeram;
			if (percent > 80)
				fprintf(stderr, "WARNING: using %.1f%% of free memory minus cache\n", percent);
		}
	}

	// actual create
	store.create(0);
	app.pStore = &store;

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %.3fG memory\n", ctx.timeAsString(), ctx.totalAllocated / 1e9);

	/*
	 * Inherit/copy sections
	 */

	store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_TRANSFORM);

	/*
	 * initialise evaluators
	 */

	tinyTree_t::initialiseEvaluator(ctx, app.pEvalFwd, MAXTRANSFORM, store.fwdTransformData);
	tinyTree_t::initialiseEvaluator(ctx, app.pEvalRev, MAXTRANSFORM, store.revTransformData);

	/*
	 * Perform medium/slow metrics
	 */
	if (app.opt_metrics)
		app.createMetrics();

	/*
	 * Perform selftests
	 */

	/**
	 * @date 2020-03-15 16:35:43
	 *
	 * Test that skins are properly encodes/decoded
	 */
	app.performSelfTestSkin();

	/*
	 * Test that forward/reverse transform complement each other.
	 */
	app.performSelfTestTransform();

	/*
	 * Test that versioned memory for databases works as expected
	 */
	app.performSelfTestVersioned();

	/*
	 * Test row/columns
	 */
	app.performSelfTestRowCol();

	/*
	 * Test that associative imprint lookups are working as expected
	 * It also produces metrics.
	 */
	app.performSelfTestInterleave();

	/*
	 * Test the tree comparing.
	 */
	app.performSelfTestCompare();

	return 0;
}
