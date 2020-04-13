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
 * - Scoring to select the representative.
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
 * @date 2020-03-23 03:39:32
 *
 *   `--text` displays resulting signature collection
 *   `--text=2` displays all candidates
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
#include <errno.h>
#include "tinytree.h"
#include "database.h"
#include "generator.h"
#include "restartdata.h"
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
	/// @var {number} Maximum number of signatures to be stored database
	uint32_t opt_maxSignature;
	/// @var {number} --metrics, Collect metrics intended for "metrics.h"
	unsigned opt_metrics;
	/// @var {number} index/data ratio
	double opt_ratio;
	/// @var {number} size of signature index WARNING: must be prime
	uint32_t opt_signatureIndexSize;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;

	/// @var {database_t} - Database store to place results
	database_t *pStore;
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;

	/**
	 * Constructor
	 */
	gensignatureContext_t() {
		// arguments and options
		arg_outputDatabase = NULL;
		arg_numNodes = 0;
		opt_force = 0;
		opt_imprintIndexSize = 0;
		opt_interleave = METRICS_DEFAULT_INTERLEAVE;
		opt_keep = 0;
		opt_maxImprint = 0;
		opt_maxSignature = 0;
		opt_metrics = 0;
		opt_ratio = METRICS_DEFAULT_RATIO / 10.0;
		opt_signatureIndexSize = 0;
		opt_text = 0;

		pStore = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;
	}

	/**
	 * @date 2020-03-27 13:59:20
	 *
	 * Load signature records and re-index to current settings
	 *
	 * @param {database_t} pStore - Database to write to
	 * @param {database_t} pDB - database to read from
	 */
	void loadSignatures(database_t *pStore, const database_t *pDB) {

		tinyTree_t tree(*this);

		if (this->opt_verbose >= VERBOSE_ACTIONS)
			fprintf(stderr, "\r\e[K[%s] Loading signatures\n", timeAsString());

		this->setupSpeed(pDB->numSignature);
		this->tick = 0;

		for (uint32_t iSid = 1; iSid < pDB->numSignature; iSid++) {
			if (opt_verbose >= VERBOSE_TICK && tick) {
				tick = 0;
				int perSecond = this->updateSpeed();

				if (perSecond == 0 || progress > progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%d numImprint=%d | hash=%.3f",
					        timeAsString(), progress, perSecond, pStore->numSignature, pStore->numImprint, (double) cntCompare / cntHash);
				} else {
					int eta = (int) ((progressHi - progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numSignature=%d numImprint=%d | hash=%.3f",
					        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, pStore->numSignature, pStore->numImprint, (double) cntCompare / cntHash);
				}
			}
			this->progress++;

			const signature_t *pDbSignature = pDB->signatures + iSid;

			/*
			 * Lookup to verify it is unique
			 */

			// load signature name
			tree.decodeFast(pDbSignature->name);

			// perform lookup
			uint32_t sid = 0, tid = 0;
			pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &sid, &tid);

			// It should not exist
			if (sid != 0) {
				printf("{\"error\":\"duplicate signature  \",\"where\":\"%s\",\"encountered\":%d,\"expected\":%d,\"name\":\"%s\"}\n",
				       __FUNCTION__, sid, iSid, pDbSignature->name);
				exit(1);
			}

			// add imprints
			pStore->addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);

			/*
			 * Add signature to database
			 */

			assert(pStore->numSignature == iSid);
			::memcpy(pStore->signatures + iSid, pDbSignature, sizeof(*pStore->signatures));
			pStore->numSignature++;

		}
		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "\r\e[K[%s] Loaded signatures. numSignature=%d numImprint=%d\n", timeAsString(), pStore->numSignature, pStore->numImprint);

	}

	/**
	 * @date 2020-03-22 00:57:15
	 *
	 * found candidate.
	 *
	 * @date 2020-03-23 04:46:53
	 *
	 * Perform an associative lookup to determine signature footprint (sid) and orientation (tid)
	 * expand collection of unique structures.
	 *
	 * If signature has swapping hints then apply the hint and deep-compare.
	 * accept the
	 *
	 * @date 2020-03-27 02:47:42
	 *
	 * At this point in time the generator seems to be functional.
	 * It agrees with metrics from the previous major version.
	 * Unexpected surprise is the two mode associative index.
	 * The previous version had (slower) interleave 504.
	 * All the trees passed to this function are natural ordered trees.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints in tree
	 */
	void foundTreeCandidate(const generatorTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			int perSecond = this->updateSpeed();

			if (perSecond == 0 || progress > progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%d numImprint=%d | hash=%.3f",
				        timeAsString(), progress, perSecond, pStore->numSignature, pStore->numImprint, (double) cntCompare / cntHash);
			} else {
				int eta = (int) ((progressHi - progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numSignature=%d numImprint=%d | hash=%.3f",
				        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, pStore->numSignature, pStore->numImprint, (double) cntCompare / cntHash);
			}
		}

		/*
		 * Lookup/add to data store.
		 * Consider signature groups `unsafe` (no members yet)
		 */

		uint32_t sid = 0;
		uint32_t tid = 0;

		pStore->lookupImprintAssociative(&treeR, pEvalFwd, pEvalRev, &sid, &tid);

		// add to datastore if not found
		if (sid == 0) {
			// add signature to database
			sid = pStore->addSignature(pNameR);
			// add to imprints to index
			pStore->addImprintAssociative(&treeR, pEvalFwd, pEvalRev, sid);

			signature_t *pSignature = pStore->signatures + sid;
			pSignature->flags = signature_t::SIGMASK_UNSAFE;
			pSignature->size = treeR.count - tinyTree_t::TINYTREE_NSTART;

			pSignature->numPlaceholder = numPlaceholder;
			pSignature->numEndpoint = numEndpoint;
			pSignature->numBackRef = numBackRef;

			if (opt_text == 2) {
				printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", progress, sid, '*', pNameR, pSignature->size, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef);
			}

			return;
		}

		/*
		 * !! NOTE: The following selection is just for the display name.
		 *          Better choices will be analysed later.
		 */

		signature_t *pSignature = pStore->signatures + sid;

		int cmp = 0; // "<0" if "best < candidate", ">0" if "best > candidate"

		// Test for prime goal: reducing number of nodes
		if (cmp == 0)
			cmp = pSignature->size - (treeR.count - tinyTree_t::TINYTREE_NSTART);

		// Test for secondary goal: reduce number of unique endpoints, thus connections
		if (cmp == 0)
			cmp = pSignature->numPlaceholder - numPlaceholder;

		// Test for preferred display selection: least number of endpoints
		if (cmp == 0)
			cmp = pSignature->numEndpoint - numEndpoint;

		// Test for preferred display selection: least number of back-references
		if (cmp == 0)
			cmp = pSignature->numBackRef - numBackRef;

		// distinguish between shallow compare (`"-+"`) or deep compare (`"<>"`)
		if (cmp < 0) {
			cmp = '-';
		} else if (cmp > 0) {
			cmp = '+';
		} else {
			/*
			 * Compare layouts, expensive
			 */
			tinyTree_t treeL(*this);
			treeL.decodeFast(pSignature->name);

			cmp = treeL.compare(treeL.root, treeR, treeR.root);

			if (cmp < 0)
				cmp = '<';
			else if (cmp > 0)
				cmp = '>';
			else
				cmp = '=';
		}

		/*
		 * Update record if candidate is better
		 */
		if (cmp == '>' || cmp == '+') {
			::strcpy(pSignature->name, pNameR);
			pSignature->size = treeR.count - tinyTree_t::TINYTREE_NSTART;
			pSignature->numPlaceholder = numPlaceholder;
			pSignature->numEndpoint = numEndpoint;
			pSignature->numBackRef = numBackRef;
		}

		if (opt_text == 2)
			printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", progress, sid, cmp, pNameR, pSignature->size, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef);
	}

	/**
	 * @date 2020-03-27 17:05:07
	 *
	 * Compare function for `qsort_r`
	 *
	 * @param {signature_t} lhs - left hand side signature
	 * @param {signature_t} rhs - right hand side signature
	 * @param {context_t} state - I/O contect needed to create trees
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int comparSignature(const void *lhs, const void *rhs, void *state) {
		if (lhs == rhs)
			return 0;

		const signature_t *pSignatureL = (const signature_t *) lhs;
		const signature_t *pSignatureR = (const signature_t *) rhs;
		context_t *pApp = (context_t *) state;

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.decodeFast(pSignatureL->name);
		treeR.decodeFast(pSignatureR->name);

		/*
		 * Compare
		 */

		int cmp = 0;

		// Test for prime goal: reducing number of nodes
		cmp = treeL.count - treeR.count;
		if (cmp)
			return cmp;

		// Test for secondary goal: reduce number of unique endpoints, thus connections
		cmp = pSignatureL->numPlaceholder - pSignatureR->numPlaceholder;
		if (cmp)
			return cmp;

		// Test for preferred display selection: least number of endpoints
		cmp = pSignatureL->numEndpoint - pSignatureR->numEndpoint;
		if (cmp)
			return cmp;

		// Test for preferred display selection: least number of back-references
		cmp = pSignatureL->numBackRef - pSignatureR->numBackRef;
		if (cmp)
			return cmp;

		// Compare layouts, expensive
		cmp = treeL.compare(treeL.root, treeR, treeR.root);
		return cmp;
	}

	/**
	 * @date 2020-03-22 01:00:05
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add newly unique signatures to the database
	 *
	 * @param {database_t} pStore - memory based database
	 */
	void main(database_t *pStore) {
		this->pStore = pStore;

		/*
		 * create generator
		 */
		generatorTree_t generator(*this);

		for (unsigned numNode = arg_numNodes; numNode <= arg_numNodes; numNode++) {
			// reset progress
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF, numNode);
			this->setupSpeed(pMetrics ? pMetrics->numProgress : 0);
			this->tick = 0;

			// clear tree
			generator.clearGenerator();

			/*
			 * Generate candidates
			 */
			if (this->opt_verbose >= this->VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", timeAsString(), numNode, MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF ? "-QnTF" : "");

			if (numNode == 0) {
				generator.root = 0; // "0"
				foundTreeCandidate(generator, "0", 0, 0, 0);
				generator.root = 1; // "a"
				foundTreeCandidate(generator, "a", 1, 1, 0);
			} else {
				unsigned endpointsLeft = numNode * 2 + 1;
				generator.generateTrees(numNode, endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &gensignatureContext_t::foundTreeCandidate);
			}

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			if (this->progress != this->progressHi) {
				printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%d}\n",
				       __FUNCTION__, this->progress, this->progressHi, numNode);
			}
		}

		/*
		 * Sort signatures. skip first reserved entry
		 */

		if (this->opt_verbose >= this->VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Sorting signatures\n", timeAsString());

		assert(pStore->numSignature >= 3);
		qsort_r(pStore->signatures + 3, pStore->numSignature - 3, sizeof(*pStore->signatures), comparSignature, this);

		/*
		 * List result
		 */
		if (opt_text == 1) {
			for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
				const signature_t *pSignature = pStore->signatures + iSid;
				printf("%u\t%s\t%u\t%u\t%u\t%u\n", iSid, pSignature->name, pSignature->size, pSignature->numPlaceholder, pSignature->numEndpoint, pSignature->numBackRef);
			}
		}

		/*
		 * Done
		 */
		fprintf(stderr, "[%s] numSlot=%d qntf=%d interleave=%d numNode=%d numCandidate=%ld numSignature=%d numImprint=%d\n",
		        this->timeAsString(), MAXSLOTS, (this->opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0, pStore->interleave, arg_numNodes, progress, pStore->numSignature, pStore->numImprint);

	}

};

/**
 * @date 2020-03-22 00:29:34
 *
 * Selftest wrapper
 *
 * @typedef {object}
 */
struct gensignatureSelftest_t : gensignatureContext_t {

	/// @var {number} --selftest, perform a selftest
	unsigned opt_selftest;
	/// @var {string[]} tree notation for `progress` points
	char **selftestWindowResults;

	/**
	 * Constructor
	 */
	gensignatureSelftest_t() {
		opt_selftest = 0;
		selftestWindowResults = NULL;
	}

	/**
	 * @date 2020-03-10 21:46:10
	 *
	 * Test that `tinyTree_t` is working as expected
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
	void performSelfTestTree(void) {

		unsigned testNr = 0;
		unsigned numPassed = 0;
		footprint_t *pEval = new footprint_t[tinyTree_t::TINYTREE_NEND];

		tinyTree_t tree(*this);

		/*
		 * quickly test that `tinyTree_t` does level-2 normalisation
		 */
		{
			tree.decodeSafe("ab>ba+^");
			const char *pName = tree.encode(tree.root);
			if (::strcmp(pName, "ab+ab>^") != 0) {
				printf("{\"error\":\"tree not level-2 normalised\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\"}\n",
				       __FUNCTION__, pName, "ab+ab>^");
				exit(1);
			}
		}

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

				tree.flags = context_t::MAGICMASK_PARANOID | (iQnTF ? context_t::MAGICMASK_QNTF : 0);
				tree.clearTree();
				tree.root = tree.addNode(Qo ^ (Qi ? IBIT : 0), To ^ (Ti ? IBIT : 0), Fo ^ (Fi ? IBIT : 0));

				/*
				 * save with placeholders and reload
				 */
				const char *treeName;

				if (iSkin) {
					char skin[MAXSLOTS + 1];

					treeName = tree.encode(tree.root, skin);
					if (iFast) {
						tree.decodeFast(treeName, skin);
					} else {
						int ret = tree.decodeSafe(treeName, skin);
						if (ret != 0) {
							printf("{\"error\":\"decodeSafe() failed\",\"where\":\"%s\",\"testNr\":%d,\"iFast\":%d,\"iQnTF\":%d,\"iSkin\":%d,\"name\":\"%s/%s\",\"ret\":%d}\n",
							       __FUNCTION__, testNr, iFast, iQnTF, iSkin, treeName, skin, ret);
							exit(1);
						}
					}
				} else {
					treeName = tree.encode(tree.root, NULL);
					if (iFast) {
						tree.decodeFast(treeName);
					} else {
						int ret = tree.decodeSafe(treeName);
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
					uint32_t encountered = pEval[tree.root & ~IBIT].bits[0] & (1 << ix) ? 1 : 0;
					if (tree.root & IBIT)
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

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed %d tests\n", this->timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-03-15 16:35:43
	 *
	 * Test that associative imprint lookups are working as expected
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
	 * @param {database_t} pStore - memory based database
	 * @param {footprint_t} pEvalFwd - evaluation vector with forward transform
	 * @param {footprint_t} pEvalRev - evaluation vector with reverse transform
	 */
	void performSelfTestInterleave(database_t *pStore) {

		unsigned numPassed = 0;

		tinyTree_t tree(*this);

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
		for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {
			if (pInterleave->numSlot != MAXSLOTS)
				continue; // only process settings that match `MAXSLOTS`

			/*
			 * Setup database and erase indices
			 */

			// mode
			pStore->interleave = pInterleave->numStored;
			pStore->interleaveStep = pInterleave->interleaveStep;

			// clear database imprint and index
			memset(pStore->imprints, 0, sizeof(*pStore->imprints) * pStore->maxImprint);
			memset(pStore->imprintIndex, 0, sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize);

			/*
			 * Create a test 4n9 tree with unique endpoints so each permutation is unique.
			 */

			tree.decodeFast(pBasename);

			// add to database
			pStore->numImprint = 1; // skip mandatory zero entry
			pStore->addImprintAssociative(&tree, pEvalFwd, pEvalRev, 0);

			/*
			 * Lookup all possible permutations
			 */

			time_t seconds = ::time(NULL);
			for (uint32_t iTransform = 0; iTransform < MAXTRANSFORM; iTransform++) {

				if (this->opt_verbose >= VERBOSE_TICK && this->tick) {
					fprintf(stderr, "\r[%s] %.5f%%", this->timeAsString(), iTransform * 100.0 / MAXTRANSFORM);
					this->tick = 0;
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

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");


			seconds = ::time(NULL) - seconds;
			if (seconds == 0)
				seconds = 1;

			// base estimated size on 791647 signatures
			fprintf(stderr, "[%s] metricsInterleave_t { /*numSlot=*/%d, /*interleave=*/%d, /*numStored=*/%d, /*numRuntime=*/%d, /*speed=*/%d, /*storage=*/%.3f},\n",
			        this->timeAsString(), MAXSLOTS, pStore->interleave, pStore->numImprint - 1, MAXTRANSFORM / (pStore->numImprint - 1),
			        (int) (MAXTRANSFORM / seconds), (sizeof(imprint_t) * 791647 * pStore->numImprint) / 1.0e9);

			// test that number of imprints match
			if (pInterleave->numStored != pStore->numImprint - 1) {
				printf("{\"error\":\"numImprint missmatch\",\"where\":\"%s\",\"encountered\":%d,\"expected\":%d}\n",
				       __FUNCTION__, pStore->numImprint - 1, pInterleave->numStored);
				exit(1);
			}
		}

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed %d tests\n", this->timeAsString(), __FUNCTION__, numPassed);
	}

	/**
	 * @date 2020-03-21 17:25:47
	 *
	 * Selftest windowing by calling the generator with windowLo/Hi for each possible tree
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints in tree
	 */
	void foundTreeWindowCreate(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			if (progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), tree.windowLo * 100.0 / progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %ld", timeAsString(), tree.windowLo);
		}

		assert(this->progress < 2000000);

		// assert entry is unique
		if (selftestWindowResults[this->progress] != NULL) {
			printf("{\"error\":\"entry not unique\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%ld}\n",
			       __FUNCTION__, selftestWindowResults[this->progress], pName, this->progress);
			exit(1);
		}

		// populate entry
		selftestWindowResults[this->progress] = ::strdup(pName);
	}

	/**
	 * @date 2020-03-21 17:31:46
	 *
	 * Selftest windowing by calling generator without a window and test if results match.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints in tree
	 */
	void foundTreeWindowVerify(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			if (progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), tree.windowLo * 100.0 / progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %ld", timeAsString(), tree.windowLo);
		}

		assert(this->progress < 2000000);

		// assert entry is present
		if (selftestWindowResults[this->progress] == NULL) {
			printf("{\"error\":\"missing\",\"where\":\"%s\",\"expected\":\"%s\",\"progress\":%ld}\n",
			       __FUNCTION__, pName, this->progress);
			exit(1);
		}

		// compare
		if (::strcmp(pName, selftestWindowResults[this->progress]) != 0) {
			printf("{\"error\":\"entry missmatch\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%ld}\n",
			       __FUNCTION__, selftestWindowResults[this->progress], pName, this->progress);
			exit(1);
		}

		// release resources
		::free(selftestWindowResults[this->progress]);
		selftestWindowResults[this->progress] = NULL;
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
		selftestWindowResults = (char **) myAlloc("genrestartdataContext_t::selftestResults", 2000000, sizeof(*selftestWindowResults));

		// set generator into `3n9 QnTF-only` mode
		this->opt_flags &= ~context_t::MAGICMASK_QNTF;
		arg_numNodes = 3;

		generatorTree_t generator(*this);

		// find metrics for setting
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF, arg_numNodes);
		assert(pMetrics);

		unsigned endpointsLeft = pMetrics->numNode * 2 + 1;

		/*
		 * Pass 1, slice dataset into single entries
		 */

		for (uint64_t windowLo = 0; windowLo < pMetrics->numProgress; windowLo++) {
			// clear tree
			generator.clearGenerator();

			// apply settings
			generator.flags = pMetrics->qntf ? generator.flags | context_t::MAGICMASK_QNTF : generator.flags & ~context_t::MAGICMASK_QNTF;
			generator.windowLo = windowLo;
			generator.windowHi = windowLo + 1;
			generator.pRestartData = restartData + restartIndex[pMetrics->numNode][pMetrics->qntf];
			this->progressHi = pMetrics->numProgress;
			this->progress = 0;
			this->tick = 0;

			generator.generateTrees(pMetrics->numNode, endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &gensignatureSelftest_t::foundTreeWindowCreate);
		}

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * Pass 2, validate entries
		 */

		{
			// clear tree
			generator.clearGenerator();

			// apply settings
			generator.flags = pMetrics->qntf ? generator.flags | context_t::MAGICMASK_QNTF : generator.flags & ~context_t::MAGICMASK_QNTF;
			generator.windowLo = 0;
			generator.windowHi = 0;
			generator.pRestartData = restartData + restartIndex[pMetrics->numNode][pMetrics->qntf];
			this->progressHi = pMetrics->numProgress;
			this->progress = 0;
			this->tick = 0;

			generator.generateTrees(pMetrics->numNode, endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &gensignatureSelftest_t::foundTreeWindowVerify);
		}

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		// release resources
		myFree("genrestartdataContext_t::selftestResults", selftestWindowResults);

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] %s() passed\n", this->timeAsString(), __FUNCTION__);
	}

	/**
	 * @date 2020-03-24 23:57:51
	 *
	 * Perform an associative lookup to determine signature footprint (sid) and orientation (tid)
	 * expand collection of unique structures.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - Tree name/notation
	 * @param {number} numUnique - number of unique endpoints in tree
	 */
	void foundTreeMetrics(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			int perSecond = this->updateSpeed();

			if (perSecond == 0 || progress > progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%d numImprint=%d | hash=%.3f",
				        timeAsString(), progress, perSecond, pStore->numSignature, pStore->numImprint, (double) cntCompare / cntHash);
			} else {
				int eta = (int) ((progressHi - progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numSignature=%d numImprint=%d | hash=%.3f",
				        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, pStore->numSignature, pStore->numImprint, (double) cntCompare / cntHash);
			}
		}

		// lookup
		uint32_t sid = 0;
		uint32_t tid = 0;

		pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);

		if (sid == 0) {
			const char *pName = tree.encode(tree.root);

			// add to database
			sid = pStore->addSignature(pName);
			pStore->addImprintAssociative(&tree, pEvalFwd, pEvalRev, sid);
		}
	}

	void performMetrics(database_t *pStore) {
		this->pStore = pStore;

		// create generator
		generatorTree_t generator(*this);

		/*
		 * Scan metrics for setting that require metrics to be collected
		 */
		for (const metricsImprint_t *pRound = metricsImprint; pRound->numSlot; pRound++) {

			if (pRound->noauto)
				continue; // skip automated handling

			// set index to default ratio
			pStore->imprintIndexSize = this->nextPrime(pRound->numImprint * (METRICS_DEFAULT_RATIO / 10.0));

			// find metrics for setting
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, pRound->qntf, pRound->numNode);
			assert(pMetrics);
			const metricsInterleave_t *pInterleave = getMetricsInterleave(MAXSLOTS, pRound->interleave);
			assert(pInterleave);

			// prepare database
			memset(pStore->imprintIndex, 0, sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize);
			memset(pStore->signatureIndex, 0, sizeof(*pStore->signatureIndex) * pStore->signatureIndexSize);
			pStore->numImprint = 1; // skip reserved first entry
			pStore->numSignature = 1; // skip reserved first entry
			pStore->interleave = pInterleave->numStored;
			pStore->interleaveStep = pInterleave->interleaveStep;

			// prepare generator
			generator.flags = pRound->qntf ? generator.flags | context_t::MAGICMASK_QNTF : generator.flags & ~context_t::MAGICMASK_QNTF;
			generator.initialiseGenerator(); // let flags take effect
			generator.clearGenerator();

			// prepare I/O context
			this->setupSpeed(pMetrics ? pMetrics->numProgress : 0);
			this->tick = 0;

			// special case (root only)
			generator.root = 0; // "0"
			foundTreeMetrics(generator, "0", 0, 0, 0);
			generator.root = 1; // "a"
			foundTreeMetrics(generator, "a", 1, 1, 0);

			// regulars
			unsigned endpointsLeft = pRound->numNode * 2 + 1;
			generator.generateTrees(pRound->numNode, endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &gensignatureSelftest_t::foundTreeMetrics);

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			// estimate speed and storage for default ratio
			double speed = 0; // in M/s
			double storage = 0; // in Gb

			{

				this->cntHash = 0;
				this->cntCompare = 0;

				// wait for a tick
				for (this->tick = 0; this->tick == 0;) {
					generator.decodeFast("ab+"); // waste some time
				}

				// do random lookups for 10 seconds
				for (this->tick = 0; this->tick < 5;) {
					// load random signature with random tree
					uint32_t sid = (rand() % (pStore->numSignature - 1)) + 1;
					uint32_t tid = rand() % pStore->numTransform;

					// load tree
					generator.decodeFast(pStore->signatures[sid].name, pStore->fwdTransformNames[tid]);

					// perform a lookup
					uint32_t s = 0, t = 0;
					pStore->lookupImprintAssociative(&generator, this->pEvalFwd, this->pEvalRev, &s, &t);
					assert(sid == s);
				}

				speed = this->cntHash / 5.0 / 1e6;
				storage = ((sizeof(*pStore->imprints) * pStore->numImprint) + (sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize)) / 1e9;
			}

			fprintf(stderr, "[%s] numSlot=%d qntf=%d interleave=%-4d numNode=%d numSignature=%d numImprint=%d speed=%.3fM/s storage=%.3fGb\n",
			        this->timeAsString(), MAXSLOTS, pRound->qntf, pRound->interleave, pRound->numNode, pStore->numSignature, pStore->numImprint, speed, storage);

			if (this->progress != this->progressHi) {
				printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%ld,\"expected\":%ld,\"numNode\":%d}\n",
				       __FUNCTION__, this->progress, this->progressHi, pRound->numNode);
			}

			/*
			 * re-index data to find ratio effects
			 */

			// what you wish...
			if (~this->opt_debug & DEBUGMASK_GEN_RATIO)
				continue;
			if (pRound->numNode < 4)
				continue; // no point for smaller trees

			for (unsigned iRatio = 20; iRatio <= 60; iRatio += 2) {
				pStore->imprintIndexSize = this->nextPrime(pRound->numImprint * (iRatio / 10.0));

				// clear imprint index
				memset(pStore->imprintIndex, 0, sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize);
				pStore->numImprint = 1; // skip mandatory zero entry
				this->cntHash = 0;
				this->cntCompare = 0;

				fprintf(stderr, "[%d %d %.1f]", pStore->numImprint, pStore->imprintIndexSize, iRatio / 10.0);

				// reindex
				for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
					const signature_t *pSignature = pStore->signatures + iSid;

					generator.decodeFast(pSignature->name);
					pStore->addImprintAssociative(&generator, this->pEvalFwd, this->pEvalRev, iSid);
				}

				fprintf(stderr, "[%d %d %.1f %ld %ld %.5f]", pStore->numImprint, pStore->imprintIndexSize, iRatio / 10.0, this->cntHash, this->cntCompare, (double) this->cntCompare / this->cntHash);

				/*
				 * perform a speedtest
				 */

				this->cntHash = 0;
				this->cntCompare = 0;

				// wait for a tick
				for (this->tick = 0; this->tick == 0;) {
					generator.decodeFast("ab+"); // waste some time
				}

				// do random lookups for 10 seconds
				for (this->tick = 0; this->tick < 5;) {
					// load random signature with random tree
					uint32_t sid = (rand() % (pStore->numSignature - 1)) + 1;
					uint32_t tid = rand() % pStore->numTransform;

					// load tree
					generator.decodeFast(pStore->signatures[sid].name, pStore->fwdTransformNames[tid]);

					// perform a lookup
					uint32_t s = 0, t = 0;
					pStore->lookupImprintAssociative(&generator, this->pEvalFwd, this->pEvalRev, &s, &t);
					assert(sid == s);
				}

				fprintf(stderr, "[speed=%7.3fM/s storage=%7.3fG hits=%.5f]\n",
				        this->cntHash / 5.0 / 1e6,
				        ((sizeof(*pStore->imprints) * pStore->numImprint) + (sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize)) / 1e9,
				        (double) this->cntCompare / this->cntHash);
			}
		}
	}

};


/*
 * I/O and Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {gensignatureContext_t} Application
 */
gensignatureSelftest_t app;

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
	if (!app.opt_keep && app.arg_outputDatabase) {
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
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]  -- Add signatures of given node size\n", argv[0]);
	fprintf(stderr, "       %s --metrics <input.db>                -- Collect metrics\n", argv[0]);
	fprintf(stderr, "       %s --selftest <input.db>               -- Test prerequisites\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                   Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help                    This list\n");
		fprintf(stderr, "\t   --imprintindex=<number>   Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>     Imprint index interleave [default=%d]\n", app.opt_interleave);
		fprintf(stderr, "\t   --keep                    Do not delete output database in case of errors\n");
		fprintf(stderr, "\t   --maximprint=<number>     Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxsignature=<number>   Maximum number of signatures [default=%u]\n", app.opt_maxSignature);
		fprintf(stderr, "\t   --metrics                 Collect metrics\n");
		fprintf(stderr, "\t   --[no-]qntf               Enable QnTF-only mode [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_QNTF) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --[no-]paranoid           Enable expensive assertions [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                   Say more\n");
		fprintf(stderr, "\t   --ratio=<number>          Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --selftest                Validate prerequisites\n");
		fprintf(stderr, "\t   --signatureindex=<number> Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --text                    Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>         Interval timer for verbose updates [default=%d]\n", args->opt_timer);
		fprintf(stderr, "\t-v --verbose                 Say less\n");
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
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_KEEP,
			LO_MAXIMPRINT,
			LO_MAXSIGNATURE,
			LO_METRICS,
			LO_NOPARANOID,
			LO_NOQNTF,
			LO_PARANOID,
			LO_QNTF,
			LO_RATIO,
			LO_SELFTEST,
			LO_SIGNATUREINDEXSIZE,
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
			{"debug",              1, 0, LO_DEBUG},
			{"force",              0, 0, LO_FORCE},
			{"help",               0, 0, LO_HELP},
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",         1, 0, LO_INTERLEAVE},
			{"keep",               0, 0, LO_KEEP},
			{"maximprint",         1, 0, LO_MAXIMPRINT},
			{"maxsignature",       1, 0, LO_MAXSIGNATURE},
			{"metrics",            0, 0, LO_METRICS},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-qntf",            0, 0, LO_NOQNTF},
			{"paranoid",           0, 0, LO_PARANOID},
			{"qntf",               0, 0, LO_QNTF},
			{"quiet",              2, 0, LO_QUIET},
			{"ratio",              1, 0, LO_RATIO},
			{"selftest",           0, 0, LO_SELFTEST},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"text",               2, 0, LO_TEXT},
			{"timer",              1, 0, LO_TIMER},
			{"verbose",            2, 0, LO_VERBOSE},
			//
			{NULL,                 0, 0, 0}
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
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_FORCE:
				app.opt_force++;
				break;
			case LO_HELP:
				usage(argv, true, &app);
				exit(0);
			case LO_IMPRINTINDEXSIZE:
				app.opt_imprintIndexSize = app.nextPrime((uint32_t) strtoul(optarg, NULL, 0));
				break;
			case LO_INTERLEAVE:
				app.opt_interleave = (unsigned) strtoul(optarg, NULL, 0);
				if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
					app.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
				break;
			case LO_KEEP:
				app.opt_keep++;
				break;
			case LO_MAXIMPRINT:
				app.opt_maxImprint = (uint32_t) strtoul(optarg, NULL, 0);
				break;
			case LO_MAXSIGNATURE:
				app.opt_maxSignature = (uint32_t) strtoul(optarg, NULL, 0);
				break;
			case LO_METRICS:
				app.opt_metrics++;
				break;
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
			case LO_RATIO:
				app.opt_ratio = strtof(optarg, NULL);
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
				break;
			case LO_SIGNATUREINDEXSIZE:
				app.opt_signatureIndexSize = app.nextPrime((uint32_t) strtoul(optarg, NULL, 0));
				break;
			case LO_TEXT:
				app.opt_text = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_text + 1;
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
	if (argc - optind >= 1)
		app.arg_inputDatabase = argv[optind++];

	if (argc - optind >= 1) {
		char *endptr;

		errno = 0; // To distinguish success/failure after call
		app.arg_numNodes = (uint32_t) strtoul(argv[optind++], &endptr, 0);

		// strip trailing spaces
		while (*endptr && isspace(*endptr))
			endptr++;

		// test for error
		if (errno != 0 || *endptr != '\0')
			app.arg_inputDatabase = NULL;
	}

	if (argc - optind >= 1)
		app.arg_outputDatabase = argv[optind++];

	if (app.arg_inputDatabase == NULL) {
		usage(argv, false, &app);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (app.arg_outputDatabase && !app.opt_force) {
		struct stat sbuf;

		if (!stat(app.arg_outputDatabase, &sbuf)) {
			fprintf(stderr, "%s already exists. Use --force to overwrite\n", app.arg_outputDatabase);
			exit(1);
		}
	}

	if (app.opt_text && isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
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
	if (app.opt_verbose >= app.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", app.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
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

	// settings for interleave
	{
		const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, app.opt_interleave);
		assert(pMetrics); // was already checked

		store.interleave = pMetrics->numStored;
		store.interleaveStep = pMetrics->interleaveStep;
	}

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
		// for metrics: set ratio to maximum because all ratio settings will be probed
		if (app.opt_metrics) {
			app.opt_ratio = 6.0;

			// get worse-case values
			if (app.opt_maxImprint == 0) {
				for (const metricsImprint_t *pMetrics = metricsImprint; pMetrics->numSlot; pMetrics++) {
					if (pMetrics->noauto)
						continue;

					if (app.opt_maxImprint < pMetrics->numImprint)
						app.opt_maxImprint = pMetrics->numImprint;
				}

				// Give extra 5% expansion space
				if (app.opt_maxImprint > UINT32_MAX - app.opt_maxImprint / 20)
					app.opt_maxImprint = UINT32_MAX;
				else
					app.opt_maxImprint += app.opt_maxImprint / 20;
			}
			if (app.opt_maxSignature == 0) {
				for (const metricsGenerator_t *pMetrics = metricsGenerator; pMetrics->numSlot; pMetrics++) {
					if (pMetrics->noauto)
						continue;

					if (app.opt_maxSignature < pMetrics->numSignature)
						app.opt_maxSignature = pMetrics->numSignature;
				}

				// Give extra 5% expansion space
				if (app.opt_maxSignature > UINT32_MAX - app.opt_maxSignature / 20)
					app.opt_maxSignature = UINT32_MAX;
				else
					app.opt_maxSignature += app.opt_maxSignature / 20;
			}

			if (app.opt_verbose >= app.VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Set limits to ratio=%.1f maxImprint=%d maxSignature=%d\n", app.timeAsString(), app.opt_ratio, app.opt_maxImprint, app.opt_maxSignature);
		}

		if (app.opt_maxImprint == 0) {
			const metricsImprint_t *pMetrics = getMetricsImprint(MAXSLOTS, app.opt_flags & app.MAGICMASK_QNTF, app.opt_interleave, app.arg_numNodes);
			store.maxImprint = pMetrics ? pMetrics->numImprint : 0;
		} else {
			store.maxImprint = app.opt_maxImprint;
		}

		if (app.opt_imprintIndexSize == 0)
			store.imprintIndexSize = app.nextPrime(store.maxImprint * app.opt_ratio);
		else
			store.imprintIndexSize = app.opt_imprintIndexSize;

		if (app.opt_maxSignature == 0) {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, app.opt_flags & app.MAGICMASK_QNTF, app.arg_numNodes);
			store.maxSignature = pMetrics ? pMetrics->numSignature : 0;
		} else {
			store.maxSignature = app.opt_maxSignature;
		}

		if (app.opt_signatureIndexSize == 0)
			store.signatureIndexSize = app.nextPrime(store.maxSignature * app.opt_ratio);
		else
			store.signatureIndexSize = app.opt_signatureIndexSize;

		if (store.interleave == 0 || store.interleaveStep == 0)
			app.fatal("no preset for --interleave\n");
		if (store.maxImprint == 0 || store.imprintIndexSize == 0)
			app.fatal("no preset for --maximprint\n");
		if (store.maxSignature == 0 || store.signatureIndexSize == 0)
			app.fatal("no preset for --maxsignature\n");
	}

	// create new sections
	if (app.opt_verbose >= app.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] Store create: maxImprint=%d maxSignature=%d\n", app.timeAsString(), store.maxSignature, store.maxImprint);

	store.create();

	// inherit from existing
	store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_TRANSFORM);

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) app.myAlloc("gensignatureContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) app.myAlloc("gensignatureContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	/*
	 * Statistics
	 */

	if (app.opt_verbose >= app.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", app.timeAsString(), app.totalAllocated);
	if (app.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", app.totalAllocated);

	// initialise evaluators
	tinyTree_t tree(app);
	tree.initialiseVector(app, app.pEvalFwd, MAXTRANSFORM, store.fwdTransformData);
	tree.initialiseVector(app, app.pEvalRev, MAXTRANSFORM, store.revTransformData);

	/*
	 * Invoke
	 */

	if (app.opt_selftest) {
		/*
		 * self tests
		 */
		// dont let `create()` round dimensions
		store.maxImprint = MAXTRANSFORM + 10; // = 362880+10
		store.imprintIndexSize = 362897; // =362880+17 force extreme index overflowing

		app.performSelfTestTree();
		app.performSelfTestInterleave(&store);
		app.performSelfTestWindow();

		exit(0);

	} else if (app.opt_metrics) {
		/*
		 * Collect metrics
		 */
		app.performMetrics(&store);

		exit(0);

	}

	/*
	 * Inject signatures from old database
	 */
	if (db.numSignature > 1)
		app.loadSignatures(&store, &db);

	/*
	 * Invoke main entrypoint of application context
	 */
	app.main(&store);

	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

#if defined(ENABLE_JANSSON)
	if (app.opt_verbose >= app.VERBOSE_SUMMARY && !app.opt_text) {
		json_t *jResult = json_object();
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.jsonInfo(jResult);
		printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		if (!isatty(1))
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}
#endif

	return 0;
}
