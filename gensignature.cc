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

#include "config.h"

#if defined(ENABLE_JANSSON)

#include "jansson.h"

#endif

/**
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 * @date 2020-03-14 11:10:15
 */
struct gensignatureContext_t : context_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {string} name of input database
	const char *arg_inputDatabase;

	/// @var {number} database compatibility and settings
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} --keep, do not delete output database in case of errors
	unsigned opt_keep;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {number} --selftest, perform a selftest
	unsigned opt_selftest;

	/**
	 * Constructor
	 */
	gensignatureContext_t() {
		// arguments and options
		arg_outputDatabase = NULL;
		opt_flags = 0;
		opt_force = 0;
		opt_keep = 0;
		opt_text = 0;
	}

};

/**
 * Perform a selftest.
 *
 * For every single-node tree there a 8 possible operands: Zero, three variables and their inverts.
 * This totals to a collection of (8*8*8) 512 trees.
 *
 * For every tree:
 *  - normalised q,t,f triplet
 *  - Save tree as string
 *  - Load tree as string
 *  - Evaluate
 *  - Compare with independent generated result
 *
 * @param {tree_t} pTree - worker tree
 * @param {footprint_t} pEval - evaluation vector
 * @date 2020-03-10 21:46:10
 */
void performSelfTestTree(tinyTree_t *pTree, footprint_t *pEval) {

	unsigned testNr = 0;
	unsigned numPassed = 0;

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
			pTree->count = tinyTree_t::TINYTREE_NSTART;
			pTree->root = pTree->normaliseQTF(Qo ^ (Qi ? IBIT : 0), To ^ (Ti ? IBIT : 0), Fo ^ (Fi ? IBIT : 0));

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
						fprintf(stderr, "selfTestTree() failed: testNr=%u iFast=%d iQnTF=%d iSkin=%d name:%s/%s ret:%d\n",
						        testNr, iFast, iQnTF, iSkin, treeName, skin, ret);
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
						fprintf(stderr, "selfTestTree() failed: testNr=%u iFast=%d iQnTF=%d iSkin=%d name:%s ret:%d\n",
						        testNr, iFast, iQnTF, iSkin, treeName, ret);
						exit(1);
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
					fprintf(stderr, "selfTestTree() failed: testNr=%u raw=%d qntf=%d skin=%d expected=%08x encountered:%08x Q=%c%x T=%c%x F=%c%x q=%x t=%x f=%x c=%x b=%x a=%x tree=%s\n",
					        testNr, iFast, iQnTF, iSkin, expected, encountered, Qi ? '~' : ' ', Qo, Ti ? '~' : ' ', To, Fi ? '~' : ' ', Fo, q, t, f, c, b, a, treeName);
					exit(1);
				}
				numPassed++;
			}
		}
	}

	printf("selfTestTree() passed %d tests\n", numPassed);
}

/**
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
 * @param {tree_t} pTree - worker tree
 * @param {footprint_t} pEval - evaluation vector
 * @date 2020-03-15 16:35:43
 */
void performSelfTestInterleave(context_t &ctx, database_t *pStore, footprint_t *pEvalFwd, footprint_t *pEvalRev) {

	tinyTree_t tree(0);

	// test name. NOTE: this is deliberately "not ordered"
	const char *pBasename = "abc!defg!!hi!";

	/*
	 * Create evaluators
	 */

	tree.initialiseVector(ctx, pEvalFwd, MAXTRANSFORM, pStore->fwdTransformData);
	tree.initialiseVector(ctx, pEvalRev, MAXTRANSFORM, pStore->revTransformData);

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
		tree.decodeFast("abc!defg!!hi!");
		footprint_t *pEncountered = pEvalFwd + tinyTree_t::TINYTREE_NEND * 3;
		tree.eval(pEncountered);

		// calculate `"cab!defg!!hi!"` (manually applying forward transform)
		tree.decodeFast("cab!defg!!hi!");
		footprint_t *pExpect = pEvalFwd;
		tree.eval(pExpect);

		// compare
		if (!pExpect[tree.root].equals(pEncountered[tree.root])) {
			fprintf(stderr, "performSelfTestInterleave() failed: \"abc!defg!!hi!/cabdefghi\" != \"cab!defg!!hi!\"\n");
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
	for (uint32_t iInterleave = 0; iInterleave < 4; iInterleave++) {

		// interleave. Order it for fastest first
		static uint32_t interleaveFactor[] = {120, 720, 720, 120};
		static uint32_t interleaveMode[] = {0, 1, 0, 1};
		static uint32_t interleaveImprint[] = {72, 120, 504, 720};

		//  mode=1 interleave=5040 numImprint=72 seconds=80 speed=4536         memory=4.160G
		//  mode=0 interleave=120 numImprint=120 seconds=41 speed=8850         memory=6.896G
		//  mode=1 interleave=720 numImprint=504 seconds=7 speed=51840        memory=28.784G
		//  mode=0 interleave=720 numImprint=720 seconds=4 speed=90720        memory=41.095G
		//  mode=1 interleave=120 numImprint=3024 seconds=1 speed=362880     memory=172.420G
		//  mode=0 interleave=5040 numImprint=5040 seconds=1 speed=362880    memory=287.329G

		/*
		 * Setup database and erase indices
		 */

		// mode
		if (interleaveMode[iInterleave])
			pStore->flags |= context_t::MAGICMASK_ROWINTERLEAVE;
		else
			pStore->flags &= ~context_t::MAGICMASK_ROWINTERLEAVE;

		pStore->interleaveFactor = interleaveFactor[iInterleave];

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
				printf("{\"error\":\"tid lookup missmatch\",\"where\":\"%s\",\"expected\":\"%d\",\"encountered\":\"%d\"}\n",
				       __FUNCTION__, iTransform, tid);
				exit(0);
			}

		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");


		seconds = ::time(NULL) - seconds;
		if (seconds == 0)
			seconds = 1;

		fprintf(stderr, "\r[%s] mode=%d interleave=%d numImprint=%d seconds=%ld speed=%ld memory=%ld\n",
		        ctx.timeAsString(), interleaveMode[iInterleave], pStore->interleaveFactor, pStore->numImprint - 1, seconds, MAXTRANSFORM / seconds, sizeof(imprint_t) * 791647 * pStore->numImprint);


		// test that number of imprints match
		if (interleaveImprint[iInterleave] != pStore->numImprint - 1) {
			printf("{\"error\":\"numImprint missmatch\",\"where\":\"%s\",\"expected\":\"%d\",\"encountered\":\"%d\"}\n",
			       __FUNCTION__, interleaveImprint[iInterleave], pStore->numImprint - 1);
			exit(0);
		}
	}
}

/*
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {gensignatureContext_t} Application
 */
gensignatureContext_t app;

/**
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 * @date 2020-03-11 23:06:35
 */
void sigintHandler(int sig) {
	if (!app.opt_keep) {
		remove(app.arg_outputDatabase);
	}
	exit(1);
}

/**
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 * @date 2020-03-11 23:06:35
 */
void sigalrmHandler(int sig) {
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
 * @date  2020-03-14 11:17:04
 */
void usage(char *const *argv, bool verbose, const gensignatureContext_t *args) {
	fprintf(stderr, "usage: %s <input.db> <output.db>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force           Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help            This list\n");
		fprintf(stderr, "\t   --keep            Do not delete output database in case of errors\n");
		fprintf(stderr, "\t-q --quiet           Say more\n");
		fprintf(stderr, "\t   --selftest        Validate proper operation\n");
		fprintf(stderr, "\t   --text            Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds> Interval timer for verbose updates [default=%d]\n", args->opt_timer);
		fprintf(stderr, "\t-v --verbose         Say less\n");

	}
}

/**
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context.
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 * @date   2020-03-14 11:19:40
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
			LO_KEEP,
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
			{"debug",    1, 0, LO_DEBUG},
			{"force",    0, 0, LO_FORCE},
			{"help",     0, 0, LO_HELP},
			{"keep",     0, 0, LO_KEEP},
			{"quiet",    2, 0, LO_QUIET},
			{"selftest", 0, 0, LO_SELFTEST},
			{"text",     0, 0, LO_TEXT},
			{"timer",    1, 0, LO_TIMER},
			{"verbose",  2, 0, LO_VERBOSE},
			//
			{NULL,       0, 0, 0}
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
			case LO_KEEP:
				app.opt_keep++;
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
	 * Program has two argument, the output database
	 */
	if (argc - optind >= 2) {
		app.arg_outputDatabase = argv[optind++];
		app.arg_inputDatabase = argv[optind++];
	} else {
		usage(argv, false, &app);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!app.opt_force) {
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
	if (db.maxTransform == 0)
		app.fatal("Missing transform section: %s\n", app.arg_inputDatabase);

	// create output
	database_t store(app);

	// create new sections
	store.maxImprint = MAXTRANSFORM + 10;
	store.imprintIndexSize = store.maxImprint * 4;

	store.imprintIndexSize = 14000029;
	store.create();
	store.imprintIndexSize = 14000029;

	// inherit to existing
	store.inheritSections(&db, database_t::ALLOCMASK_TRANSFORM | database_t::ALLOCMASK_TRANSFORMINDEX);

	/*
	 * Statistics
	 */

	if (app.opt_verbose >= app.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", app.timeAsString(), app.totalAllocated);
	if (app.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", app.totalAllocated);

	/*
	 * Invoke main entrypoint of application context
	 */

	footprint_t *pEvalCol = new footprint_t[tinyTree_t::TINYTREE_NEND * MAXTRANSFORM];
	footprint_t *pEvalRow = new footprint_t[tinyTree_t::TINYTREE_NEND * MAXTRANSFORM];
	assert(pEvalCol);
	assert(pEvalRow);
	performSelfTestInterleave(app, &store, pEvalCol, pEvalRow);

//	app.main(&store);

	/*
	 * Save the database
	 */

	// unexpected termination should unlink the outputs
	signal(SIGINT, sigintHandler);
	signal(SIGHUP, sigintHandler);

//	store.save(app.arg_outputDatabase);

#if defined(ENABLE_JANSSON)
//	if (app.opt_verbose >= app.VERBOSE_SUMMARY) {
//		json_t *jResult = json_object();
//		json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
//		store.headerInfo(jResult, store.dbHeader);
//		printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
//	}
#endif

	return 0;
}
