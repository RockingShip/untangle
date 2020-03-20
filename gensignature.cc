//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-14 11:09:15
 *
 * `gensignature` scans `Xn9` space using `generator_t` and adds associative unique footprints to a given dataset.
 * Associative unique is when all other permutation of endpoints are excluded.
 *
 * `gensignature` can also generate SQL used for signature group analysis.
 *
 * Each footprint can consist of a collection of unique structures called signature group.
 * One member of each signature group, the structure with the most concise notation, is called the representative.
 * The name of the representative is the display name of the signature.
 *
 * For each signature group additional properties are determined.
 * - Scoring to filter which structures should be part of the group.
 * - Scoring to select the representitive.
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
 * - Of every expression it permutates all possible 9!=363600 inputs
 * - Of every permutation it tries all 2^8=512 different input values
 * - In that vastness of information matches are searched
 *
 * All those 512403385356 expressions can be rewritten in terms of 791647 unique expressions fitted with 363600 different skins.
 *
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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/stat.h>
#include "tinytree.h"
#include "database.h"
#include "metrics.h"

#include "config.h"

#if defined(ENABLE_JANSSON)

#include "jansson.h"

#endif

/**
 * @date 2020-03-14 11:10:15
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct gensignatureContext_t : context_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} size of signatures to be generated in this invocation
	unsigned arg_numNodes;
	/// @var {number} database compatibility and settings
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} size of imprint index WARNING: must be prime
	uint32_t opt_imprintIndexSize;
	/// @var {number} interleave for associative imprint index
	unsigned opt_interleave;
	/// @var {number} --keep, do not delete output database in case of errors
	unsigned opt_keep;
	/// @var {number} Maximum number of imprints to be stored database
	uint32_t opt_maxImprint;
	/// @var {number} non-zero to indicate `QnTF`-only mode
	unsigned opt_qntf;
	/// @var {number} index/data ratio
	double opt_ratio;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {number} --test, run without output
	unsigned opt_test;
	/// @var {number} --selftest, perform a selftest
	unsigned opt_selftest;

	/**
	 * Constructor
	 */
	gensignatureContext_t() {
		// arguments and options
		arg_outputDatabase = NULL;
		arg_numNodes = 0;
		opt_flags = 0;
		opt_force = 0;
		opt_imprintIndexSize = 0;
		opt_interleave = 720;
		opt_keep = 0;
		opt_maxImprint = 0;
		opt_qntf = 0;
		opt_ratio = 3.0;
		opt_selftest = 0;
		opt_test = 0;
		opt_text = 0;
	}

};

/**
 * @date 2020-03-10 21:46:10
 *
 * Perform a selftest.
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
 *
 * @param {gentransformContext_t} ctx - I/O context
 * @param {tree_t} pTree - worker tree
 */
void performSelfTestTree(context_t &ctx, tinyTree_t *pTree) {

	unsigned testNr = 0;
	unsigned numPassed = 0;
	footprint_t *pEval = new footprint_t[tinyTree_t::TINYTREE_NEND];

	/*
	 * self-test with different program settings
	 */
	// @formatter:off
	for (unsigned iFast=0; iFast<2; iFast++) // decode notation in fast mode
	for (unsigned iSkin=0; iSkin<2; iSkin++) // use placeholder/skin notation
	for (unsigned iQnTF=0; iQnTF<2; iQnTF++) { // force `QnTF` rewrites
	// @formatter:on

		/*
		 * Test all 512 operand combinations. Zero, 3 endpoints and their 4 inverts (8*8*8=512)
		 */

		// @formatter:off
		for (uint32_t Fo = 0; Fo < tinyTree_t::TINYTREE_KSTART + 3; Fo++) // operand of F: 0, a, b, c
		for (uint32_t Fi = 0; Fi < 2; Fi++)                               // inverting of F
		for (uint32_t To = 0; To < tinyTree_t::TINYTREE_KSTART + 3; To++)
		for (uint32_t Ti = 0; Ti < 2; Ti++)
		for (uint32_t Qo = 0; Qo < tinyTree_t::TINYTREE_KSTART + 3; Qo++)
		for (uint32_t Qi = 0; Qi < 2; Qi++) {
		// @formatter:on

			// additional rangecheck
			if (Qo && Qo < tinyTree_t::TINYTREE_KSTART) continue;
			if (To && To < tinyTree_t::TINYTREE_KSTART) continue;
			if (Fo && Fo < tinyTree_t::TINYTREE_KSTART) continue;

			// bump test number
			testNr++;

			/*
			 * Load the tree with a single operator
			 */

			pTree->flags = context_t::MAGICMASK_PARANOID | (iQnTF ? context_t::MAGICMASK_QNTF : 0);
			pTree->clear();
			pTree->root = pTree->addNode(Qo ^ (Qi ? IBIT : 0), To ^ (Ti ? IBIT : 0), Fo ^ (Fi ? IBIT : 0));

			/*
			 * save with placeholders and reload
			 */
			const char *treeName;

			if (iSkin) {
				char skin[MAXSLOTS + 1];

				treeName = pTree->encode(pTree->root, skin);
				if (iFast) {
					pTree->decodeFast(treeName, skin);
				} else {
					int ret = pTree->decodeSafe(treeName, skin);
					if (ret != 0) {
						printf("{\"error\":\"decodeSafe() failed\",\"where\":\"%s\",\"testNr\":%d,\"iFast\":%d,\"iQnTF\":%d,\"iSkin\":%d,\"name\":\"%s/%s\",\"ret\":%d}\n",
						       __FUNCTION__, testNr, iFast, iQnTF, iSkin, treeName, skin, ret);
						exit(1);
					}
				}
			} else {
				treeName = pTree->encode(pTree->root, NULL);
				if (iFast) {
					pTree->decodeFast(treeName);
				} else {
					int ret = pTree->decodeSafe(treeName);
					if (ret != 0) {
						printf("{\"error\":\"decodeSafe() failed\",\"where\":\"%s\",\"testNr\":%d,\"iFast\":%d,\"iQnTF\":%d,\"iSkin\":%d,\"name\":\"%s\",\"ret\":%d}\n",
						       __FUNCTION__, testNr, iFast, iQnTF, iSkin, treeName, ret);
					}
				}
			}

			/*
			 * Evaluate tree
			 */

			// load test vector
			pEval[0].bits[0] = 0b00000000; // v[0]
			pEval[tinyTree_t::TINYTREE_KSTART + 0].bits[0] = 0b10101010; // v[1]
			pEval[tinyTree_t::TINYTREE_KSTART + 1].bits[0] = 0b11001100; // v[2]
			pEval[tinyTree_t::TINYTREE_KSTART + 2].bits[0] = 0b11110000; // v[3]

			// evaluate
			pTree->eval(pEval);

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

				uint32_t q, t, f;

				/*
				 * Substitute endpoints `a-c` with their actual values.
				 */
				// @formatter:off
				switch (Qo) {
					case 0:            q = 0; break;
					case (tinyTree_t::TINYTREE_KSTART + 0): q = a; break;
					case (tinyTree_t::TINYTREE_KSTART + 1): q = b; break;
					case (tinyTree_t::TINYTREE_KSTART + 2): q = c; break;
				}
				if (Qi) q ^= 1;

				switch (To) {
					case 0:            t = 0; break;
					case (tinyTree_t::TINYTREE_KSTART + 0): t = a; break;
					case (tinyTree_t::TINYTREE_KSTART + 1): t = b; break;
					case (tinyTree_t::TINYTREE_KSTART + 2): t = c; break;
				}
				if (Ti) t ^= 1;

				switch (Fo) {
					case 0:            f = 0; break;
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
				uint32_t ix = c << 2 | b << 1 | a;
				uint32_t encountered = pEval[pTree->root & ~IBIT].bits[0] & (1 << ix) ? 1 : 0;
				if (pTree->root & IBIT)
					encountered ^= 1; // invert result

				if (expected != encountered) {
					printf("{\"error\":\"compare failed\",\"where\":\"%s\",\"testNr\":%d,\"iFast\":%d,\"iQnTF\":%d,\"iSkin\":%d,\"expected\":\"%08x\",\"encountered\":\"%08x\",\"Q\":\"%c%x\",\"T\":\"%c%x\",\"F\":\"%c%x\",\"q\":\"%x\",\"t\":\"%x\",\"f\":\"%x\",\"c\":\"%x\",\"b\":\"%x\",\"a\":\"%x\",\"tree\":\"%s\"}\n",
					       __FUNCTION__, testNr, iFast, iQnTF, iSkin, expected, encountered, Qi ? '~' : ' ', Qo, Ti ? '~' : ' ', To, Fi ? '~' : ' ', Fo, q, t, f, c, b, a, treeName);
					exit(1);
				}
				numPassed++;
			}
		}
	}

	fprintf(stderr,"[%s] %s() passed %d tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
}

/**
 * @date 2020-03-15 16:35:43
 *
 * Perform a selftest.
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
 * The chosen implentation is to take advantage of interleaving properties as described for `performSelfTestInterleave()`
 * It describes that any transform permutatuion can be achieved by only knowing key column and row entries.
 *
 * Demonstrate that for any given footprint it will re-orientate
 * @param {gentransformContext_t} ctx - I/O context
 * @param {database_t} pStore - memory based database
 * @param {footprint_t} pEvalFwd - evaluation vector with forward transform
 * @param {footprint_t} pEvalRev - evaluation vector with reverse transform
 */
void performSelfTestInterleave(context_t &ctx, database_t *pStore, footprint_t *pEvalFwd, footprint_t *pEvalRev) {

	unsigned numPassed = 0;

	tinyTree_t tree(ctx, 0);

	// test name. NOTE: this is deliberately "not ordered"
	const char *pBasename = "abc!defg!!hi!";

	/*
	 * Basic test tree
	 */

	// test is test name can be decoded
	tree.decodeFast(pBasename);

	// test that tree is what was requested
	assert(~tree.root & IBIT);
	assert(::strcmp(pBasename, tree.encode(tree.root, NULL)) == 0);

	/*
	 * Basic test evaluator
	 */
	{
		// `fwdTransform[3]` equals `"cabdefghi"` which is different than `revTransform[3]`
		assert(strcmp(pStore->fwdTransformNames[3], "cabdefghi") == 0);
		assert(strcmp(pStore->revTransformNames[3], "bcadefghi") == 0);

		// calculate `"abc!defg!!hi!"/cabdefghi"`
		tree.decodeSafe("abc!defg!!hi!");
		footprint_t *pEncountered = pEvalFwd + tinyTree_t::TINYTREE_NEND * 3;
		tree.eval(pEncountered);

		// calculate `"cab!defg!!hi!"` (manually applying forward transform)
		tree.decodeSafe("cab!defg!!hi!");
		footprint_t *pExpect = pEvalFwd;
		tree.eval(pExpect);

		// compare
		if (!pExpect[tree.root].equals(pEncountered[tree.root])) {
			printf("{\"error\":\"decode with skin failed\",\"where\":\"%s\"}\n",
			       __FUNCTION__);
			exit(1);
		}

		// test that cache lookups work
		// calculate `"abc!de!fabc!!"`
		tree.decodeSafe("abc!de!fabc!!");
		tree.eval(pEvalFwd);

		const char *pExpectedName = tree.encode(tree.root);

		// compare
		if (strcmp(pExpectedName, "abc!de!f2!") != 0) {
			printf("{\"error\":\"decode with cache failed\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\"}\n",
			       __FUNCTION__, pExpectedName, "abc!de!f2!");
			exit(1);
		}
	}

	/*
	 * @date 2020-03-17 00:34:54
	 *
	 * Generate all possible situations
	 *
	 * With regard to storage/speed trade-offs, only 4 row/column combos are viable.
	 * Storage is based on worst-case scenario.
	 * Actual storage needs to be tested/runtime decided.
	 */
	for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->maxSlots; pInterleave++) {
		if (pInterleave->maxSlots != MAXSLOTS)
			continue; // only process settings that match `MAXSLOTS`

		/*
		 * Setup database and erase indices
		 */

		// mode
		pStore->interleave = pInterleave->numStored;
		pStore->interleaveFactor = pInterleave->interleaveFactor;

		// clear
		memset(pStore->imprints, 0, sizeof(*pStore->imprints) * pStore->maxImprint);
		memset(pStore->imprintIndex, 0, sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize);

		/*
		 * Create a test 4n9 tree with unique endpoints so each permutation is unique.
		 */

		tree.decodeFast(pBasename);

		// add to database
		pStore->numImprint = 1;
		pStore->addImprintAssociative(&tree, pEvalFwd, pEvalRev, 0);

		/*
		 * Lookup all possible permutations
		 */

		time_t seconds = ::time(NULL);
		for (uint32_t iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				fprintf(stderr, "\r[%s] %.5f%%", ctx.timeAsString(), iTransform * 100.0 / MAXTRANSFORM);
				ctx.tick = 0;
			}

			// Load base name with skin
			tree.decodeFast(pBasename, pStore->fwdTransformNames[iTransform]);

			uint32_t sid, tid;

			// lookup
			if (!pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid)) {
				printf("{\"error\":\"tree not found\",\"where\":\"%s\",\"tid\":\"%s\"}\n",
				       __FUNCTION__, pStore->fwdTransformNames[iTransform]);
				exit(1);
			}

			// test that transform id's match
			if (iTransform != tid) {
				printf("{\"error\":\"tid lookup missmatch\",\"where\":\"%s\",\"encountered\":%d,\"expected\":%d}\n",
				       __FUNCTION__, tid, iTransform);
				exit(1);
			}

			numPassed++;

		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");


		seconds = ::time(NULL) - seconds;
		if (seconds == 0)
			seconds = 1;

		// base estimated size on 791647 signatures
		fprintf(stderr, "[%s] metricsInterleave_t { /*maxSlots=*/%d, /*interleaveFactor*/=%d, /*numStored=*/%d, /*numRuntime=*/%d, /*speed=*/%d, /*storage=*/%.3f}\n",
		        ctx.timeAsString(), MAXSLOTS, pStore->interleaveFactor, pStore->numImprint - 1, MAXTRANSFORM / (pStore->numImprint - 1),
		        (int)(MAXTRANSFORM / seconds), (sizeof(imprint_t) * 791647 * pStore->numImprint) / 1.0e9);

		// test that number of imprints match
		if (pInterleave->numStored != pStore->numImprint - 1) {
			printf("{\"error\":\"numImprint missmatch\",\"where\":\"%s\",\"encountered\":%d,\"expected\":%d}\n",
			       __FUNCTION__, pStore->numImprint - 1, pInterleave->numStored);
			exit(1);
		}
	}

	fprintf(stderr,"[%s] %s() passed %d tests\n", ctx.timeAsString(), __FUNCTION__, numPassed);
}

/*
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {gensignatureContext_t} Application
 */
gensignatureContext_t app;

/**
 * @date 2020-03-11 23:06:35
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int sig) {
	if (!app.opt_keep) {
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
void sigalrmHandler(int sig) {
	if (app.opt_timer) {
		app.tick++;
		alarm(app.opt_timer);
	}
}

/**
 * @date  2020-03-14 11:17:04
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *const *argv, bool verbose, const gensignatureContext_t *args) {
	fprintf(stderr, "usage: %s <input.db> <output.db> <numnode>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force                 Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help                  This list\n");
		fprintf(stderr, "\t   --imprintindex=<number> Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>   Imprint index interleave [default=%d]\n", app.opt_interleave);
		fprintf(stderr, "\t   --keep                  Do not delete output database in case of errors\n");
		fprintf(stderr, "\t   --maximprint=<number>   Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --[no-]qntf             Enable QnTF-only mode [default=%s]\n", app.opt_qntf ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                 Say more\n");
		fprintf(stderr, "\t   --ratio=<number>        Index/data ratio [default=%f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --selftest              Validate prerequisites\n");
		fprintf(stderr, "\t   --test                  Run without output\n");
		fprintf(stderr, "\t   --text                  Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>       Interval timer for verbose updates [default=%d]\n", args->opt_timer);
		fprintf(stderr, "\t-v --verbose               Say less\n");
	}
}

/**
 * @date   2020-03-14 11:19:40
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

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_DEBUG = 1,
			LO_FORCE,
			LO_IMPRINTINDEX,
			LO_INTERLEAVE,
			LO_KEEP,
			LO_MAXIMPRINT,
			LO_NOQNTF,
			LO_QNTF,
			LO_RATIO,
			LO_SELFTEST,
			LO_TEST,
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
			{"debug",        1, 0, LO_DEBUG},
			{"force",        0, 0, LO_FORCE},
			{"help",         0, 0, LO_HELP},
			{"imprintindex", 1, 0, LO_IMPRINTINDEX},
			{"interleave",   1, 0, LO_INTERLEAVE},
			{"keep",         0, 0, LO_KEEP},
			{"maximprint",   1, 0, LO_MAXIMPRINT},
			{"no-qntf",      0, 0, LO_NOQNTF},
			{"qntf",         0, 0, LO_QNTF},
			{"quiet",        2, 0, LO_QUIET},
			{"ratio",        1, 0, LO_RATIO},
			{"selftest",     0, 0, LO_SELFTEST},
			{"test",         0, 0, LO_TEST},
			{"text",         0, 0, LO_TEXT},
			{"timer",        1, 0, LO_TIMER},
			{"verbose",      2, 0, LO_VERBOSE},
			//
			{NULL,           0, 0, 0}
		};

		char optstring[128], *cp;
		cp = optstring;

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
		int option_index = 0;
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
			case LO_DEBUG:
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
				break;
			case LO_FORCE:
				app.opt_force++;
				break;
			case LO_HELP:
				usage(argv, true, &app);
				exit(0);
			case LO_IMPRINTINDEX:
				app.opt_imprintIndexSize = (uint32_t) strtoul(optarg, NULL, 10);
				break;
			case LO_INTERLEAVE:
				app.opt_interleave = (unsigned) strtoul(optarg, NULL, 10);
				if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
					app.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
				break;
			case LO_KEEP:
				app.opt_keep++;
				break;
			case LO_MAXIMPRINT:
				app.opt_maxImprint = (uint32_t) strtoul(optarg, NULL, 10);
				break;
			case LO_NOQNTF:
				app.opt_qntf = 0;
				break;
			case LO_QNTF:
				app.opt_qntf = 1;
				break;
			case LO_QUIET:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose - 1;
				break;
			case LO_RATIO:
				app.opt_ratio = strtof(optarg, NULL);
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
				app.opt_test++;
				break;
			case LO_TEST:
				app.opt_test++;
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
	 * Program has two argument, the output database
	 */
	if (argc - optind >= 3) {
		app.arg_outputDatabase = argv[optind++];
		app.arg_inputDatabase = argv[optind++];
		app.arg_numNodes = (uint32_t) strtoul(argv[optind++], NULL, 10);
	} else {
		usage(argv, false, &app);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!app.opt_test && !app.opt_force) {
		struct stat sbuf;

		if (!stat(app.arg_outputDatabase, &sbuf)) {
			fprintf(stderr, "%s already exists. Use --force to overwrite\n", app.arg_outputDatabase);
			exit(1);
		}
	}

	// register timer handler
	if (app.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(app.opt_timer);
	}

	/*
	 * Open input and create output database
	 */

	// Open input
	database_t db(app);

	db.open(app.arg_inputDatabase, true);

	if (db.flags && app.opt_verbose >= app.VERBOSE_SUMMARY)
		app.logFlags(db.flags);
#if defined(ENABLE_JANSSON)
	if (app.opt_verbose >= app.VERBOSE_INITIALIZE)
		fprintf(stderr, "[%s] %s\n", app.timeAsString(), json_dumps(db.headerInfo(NULL, db.dbHeader), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	/*
	 * create output
	 */

	database_t store(app);

	/*
	 * @date 2020-03-17 13:57:25
	 *
	 * Database indices are hashlookup tables with overflow.
	 * The art is to have a hash function that distributes evenly over the hashtable.
	 * If index entries are in use, then jump to overflow entries.
	 * The larger the index in comparison to the number of data entries the lower the chance an overflow will occur.
	 * The ratio between index and data size is called `ratio`.
	 */

	if (app.opt_selftest) {
		// force dimensions when self testing. Need to store a single footprint
		store.maxImprint = MAXTRANSFORM + 10; // = 362880+10
		store.imprintIndexSize = 362897; // =362880+17 force extreme index overflowing

		/*
		 * @date 2020-03-17 16:11:36
		 * constraint: index needs to be larger than number of data entries
		 */
		assert(store.imprintIndexSize > store.maxImprint);
	} else {
		// settings for interleave
		const metricsInterleave_t *pInterleave = getMetricsInterleave(MAXSLOTS, app.opt_interleave);
		assert(pInterleave);

		store.interleave = pInterleave->numStored;
		store.interleaveFactor = pInterleave->interleaveFactor;

		if (app.opt_maxImprint == 0)
			store.maxImprint = getMaxImprints(MAXSLOTS, app.opt_interleave, app.arg_numNodes);
		else
			store.maxImprint = app.opt_maxImprint;

		if (app.opt_imprintIndexSize == 0)
			store.imprintIndexSize = app.double2u32(store.maxImprint * app.opt_ratio);
		else
			store.imprintIndexSize = app.opt_imprintIndexSize;

		if (app.opt_interleave == 0)
			app.fatal("no preset for --interleave\n");
		if (app.opt_maxImprint == 0)
			app.fatal("no preset for --maximprint\n");
	}

	// create new sections
	store.create();

	// dont let `create()` round dimensions
	if (app.opt_selftest) {
		store.maxImprint = MAXTRANSFORM + 10; // = 362880+10
		store.imprintIndexSize = 362897; // =362880+17 force extreme index overflowing
	}

	// inherit from existing
	store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_TRANSFORM);

	/*
	 * Statistics
	 */

	if (app.opt_verbose >= app.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", app.timeAsString(), app.totalAllocated);
	if (app.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", app.totalAllocated);

	/*
	 * Test prerequisite
	 */
	if (app.opt_selftest) {
		// perform selfchecks

		tinyTree_t tree(app, 0);

		// allocate evaluators
		footprint_t *pEvalCol = new footprint_t[tinyTree_t::TINYTREE_NEND * MAXTRANSFORM];
		footprint_t *pEvalRow = new footprint_t[tinyTree_t::TINYTREE_NEND * MAXTRANSFORM];
		assert(pEvalCol);
		assert(pEvalRow);

		// initialise evaluators
		tree.initialiseVector(app, pEvalCol, MAXTRANSFORM, store.fwdTransformData);
		tree.initialiseVector(app, pEvalRow, MAXTRANSFORM, store.revTransformData);

		/*
		 * @date 2020-03-17 16:31:08
		 * I usually avoid `&` in function declarations because it does not allow visual hints that it is pass by value.
		 * Here are some observations:
		 * - it is guaranteed to be non-NULL
		 * - in function declarations it can be used as replacement/placeholder for global names
		 *   that is, if `app` were global before `performSelfTestInterleave`,
		 *   then removing `app` as argument would not require additional changing of code.
		 */
		performSelfTestTree(app, &tree);
		performSelfTestInterleave(app, &store, pEvalCol, pEvalRow);

		exit(0);
	}

	/*
	 * Invoke main entrypoint of application context
	 */

//	app.main(&store);

	/*
	 * Save the database
	 */

	if (!app.opt_test) {
		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

#if defined(ENABLE_JANSSON)
	if (app.opt_verbose >= app.VERBOSE_SUMMARY && !app.opt_text) {
		json_t *jResult = json_object();
		json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.headerInfo(jResult, store.dbHeader);
		printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}
#endif

	return 0;
}
