/*
 * @date 2021-10-17 17:53:40
 *
 * Generate detector patterns and populate patternFirst/patternSecond tables
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2021, xyzzy@rockingship.org
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
#include <jansson.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "dbtool.h"
#include "generator.h"
#include "metrics.h"
#include "tinytree.h"

// Need generator to allow ranges
#include "restartdata.h"

/**
 * @date 2020-03-14 11:10:15
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genpatternContext_t : dbtool_t {

	enum {
		/// @constant {number} - `--text` modes
		OPTTEXT_WON     = 1,
		OPTTEXT_COMPARE = 2,
		OPTTEXT_BRIEF   = 3,
		OPTTEXT_VERBOSE = 4,
	};

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} Tree size in nodes to be generated for this invocation;
	unsigned   arg_numNodes;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} --fast, fast load patterns by calling `addPatternDatabase()` directly. 
	unsigned   opt_fast;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned   opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned   opt_generate;
	/// @var {string} name of file containing patterns
	const char *opt_load;
	/// @var {string} --mixed, Consider/accept top-level mixed members only
	unsigned   opt_mixed;
	/// @var {number} task Id. First task=1
	unsigned   opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned   opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned   opt_text;
	/// @var {number} truncate on database overflow
	double     opt_truncate;
	/// @var {number} Allow wildcards in structures
	unsigned   opt_wildcard;
	/// @var {number} generator upper bound
	uint64_t   opt_windowHi;
	/// @var {number} generator lower bound
	uint64_t   opt_windowLo;

	/// @var {database_t} - Database store to place results
	database_t *pStore;

	/// @var {number} - THE generator
	generator_t generator;
	/// @var {number} Duplicate entry
	unsigned    skipDuplicate;
	/// @var {number} Structure contained a wildcard node
	unsigned    skipWildcard;
	/// @var {number} sid normalisation collapse
	unsigned    skipCollapse;
	/// @var {number} slot reconstruction placeholder mismatch
	unsigned    skipPlaceholder;
	/// @var {number} Where database overflow was caught
	uint64_t    truncated;
	/// @var {number} Name of signature causing overflow
	char        truncatedName[tinyTree_t::TINYTREE_NAMELEN + 1];
	/// @var {number} Sid lookup table for endpoints and `lookupImprintAssociative()`
	uint32_t    fastLookupSid[tinyTree_t::TINYTREE_NSTART];
	/// @var {number} Tid lookup table for endpoints and `lookupImprintAssociative()`
	uint32_t    fastLookupTid[tinyTree_t::TINYTREE_NSTART];

	/**
	 * Constructor
	 */
	genpatternContext_t(context_t &ctx) : dbtool_t(ctx), generator(ctx) {
		// arguments and options
		arg_inputDatabase  = NULL;
		arg_numNodes       = 0;
		arg_outputDatabase = NULL;
		opt_force          = 0;
		opt_generate       = 1;
		opt_taskId         = 0;
		opt_taskLast       = 0;
		opt_load           = NULL;
		opt_mixed          = 0;
		opt_text           = 0;
		opt_truncate       = 0;
		opt_wildcard       = 0;
		opt_windowHi       = 0;
		opt_windowLo       = 0;

		pStore = NULL;

		skipDuplicate   = 0;
		skipWildcard    = 0;
		skipCollapse    = 0;
		skipPlaceholder = 0;
		truncated       = 0;
		truncatedName[0] = 0;
	}

	/*
	 * @date 2021-10-25 15:19:41
	 * 
	 * Connect database and continue initialisation
	 */
	void connect(database_t &db) {
		this->pStore = &db;

		/*
		 * @date 2021-10-25 11:44:55
		 * sid/tid lookup tables for endpoints, to avoid calling `lookupImprintAssociative()`
		 */

		// Initialise lookup table
		if (fastLookupSid[0] == 0) {
			uint32_t ix = pStore->lookupSignature("0");
			fastLookupSid[0] = pStore->signatureIndex[ix];
			fastLookupTid[0] = 0;
			assert(fastLookupSid[0] != 0);

			for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
				char name[2] = {(char) ('a' + iSlot), 0};

				uint32_t ix = pStore->lookupSignature("a");
				fastLookupSid[tinyTree_t::TINYTREE_KSTART + iSlot] = pStore->signatureIndex[ix];
				fastLookupTid[tinyTree_t::TINYTREE_KSTART + iSlot] = pStore->lookupFwdTransform(name);
				assert(fastLookupSid[tinyTree_t::TINYTREE_KSTART] != 0);
			}
		}
	}

	/*
	 * Add the structure in `treeR` to the sid/tid detector dataset.
	 *
	 * @param {generatorTree_t} treeR - candidate tree
	 * @param {string} pNameR - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool /*__attribute__((optimize("O0")))*/ foundTreePattern(tinyTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {

		if (this->truncated)
			return false; // quit as fast as possible

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numPatternFirst=%u(%.0f%%) numPatternSecond=%u(%.0f%%) | skipDuplicate=%u  skipWildcard=%u  skipCollapse=%u  skipPlaceholder=%u | hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numPatternFirst, pStore->numPatternFirst * 100.0 / pStore->maxPatternFirst,
					pStore->numPatternSecond, pStore->numPatternSecond * 100.0 / pStore->maxPatternSecond,
					skipDuplicate, skipWildcard, skipCollapse, skipPlaceholder, (double) ctx.cntCompare / ctx.cntHash, pNameR);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;


				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numPatternFirst=%u(%.0f%%) numPatternSecond=%u(%.0f%%) |  skipDuplicate=%u  skipWildcard=%u  skipCollapse=%u  skipPlaceholder=%u | hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - generator.windowLo) * 100.0 / (ctx.progressHi - generator.windowLo), etaH, etaM, etaS,
					pStore->numPatternFirst, pStore->numPatternFirst * 100.0 / pStore->maxPatternFirst,
					pStore->numPatternSecond, pStore->numPatternSecond * 100.0 / pStore->maxPatternSecond,
					skipDuplicate, skipWildcard, skipCollapse, skipPlaceholder, (double) ctx.cntCompare / ctx.cntHash, pNameR);
			}

			if (ctx.restartTick) {
				// passed a restart point
				fprintf(stderr, "\n");
				ctx.restartTick = 0;
			}

			ctx.tick = 0;
		}

		/*
		 * Test for database overflow
		 */
		if (this->opt_truncate) {
			// avoid `"storage full"`. Give warning later
			if (pStore->maxPatternFirst - pStore->numPatternFirst <= pStore->IDFIRST || pStore->maxPatternSecond - pStore->numPatternSecond <= pStore->IDFIRST) {
				// break now, display text later/ Leave progress untouched
				this->truncated = ctx.progress;
				::strcpy(this->truncatedName, pNameR);

				// quit as fast as possible
				return false;
			}
		}

		if (opt_mixed) {
			enum {
				FULL, MIXED, PURE
			} area;
			area = PURE;

			for (unsigned k = tinyTree_t::TINYTREE_NSTART; k < treeR.root; k++) {
				if (!(treeR.N[k].T & IBIT)) {
					area = FULL;
					break;
				}
			}
			if (area == PURE && !(treeR.N[treeR.root].T & IBIT))
				area = MIXED;

			// with `--mixed`, only accept PURE/MIXED
			if (area == FULL)
				return true;
		}

#if 0
		// @date <historic>
		// extra lines to test hardened restart by matching progress/name
		// test with `./genshrink --numnode=4 --sge` and comparing results without `--sge`'
		printf("%ju %s\n", progress-1, treeR.saveString(treeR.count-1));
		return true;
#endif

		/*
		 * Skip patterns with 'wildcard' nodes
		 * Wildcards are nodes that can be replaced by a placeholder because they do not share endpoints with other nodes 
		 * 
		 */
		if (!opt_wildcard) {
			uint32_t singleRef = 0;
			uint32_t multiRef  = 0;

			for (unsigned i = tinyTree_t::TINYTREE_NSTART; i < treeR.count; i++) {
				const tinyNode_t *pNode = treeR.N + i;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   F      = pNode->F;

				if (Q && Q < tinyTree_t::TINYTREE_NSTART) {
					multiRef |= singleRef & (1 << Q);
					singleRef |= (1 << Q);
				}
				if (Tu && Tu < tinyTree_t::TINYTREE_NSTART) {
					multiRef |= singleRef & (1 << Tu);
					singleRef |= (1 << Tu);
				}
				if (F && F < tinyTree_t::TINYTREE_NSTART && F != Tu) {
					multiRef |= singleRef & (1 << F);
					singleRef |= (1 << F);
				}
			}

			for (unsigned i = tinyTree_t::TINYTREE_NSTART; i < treeR.count; i++) {
				const tinyNode_t *pNode = treeR.N + i;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   T      = pNode->T;
				const uint32_t   Fu     = pNode->F & ~IBIT;

				if (Q < tinyTree_t::TINYTREE_NSTART && T < tinyTree_t::TINYTREE_NSTART && Fu < tinyTree_t::TINYTREE_NSTART) {
					if (!(multiRef & (1 << Q)) && !(multiRef & (1 << T)) && !(multiRef & (1 << Fu))) {
						skipWildcard++;
						return true;
					}
				}
			}
		}

		/*
		 * Search the QTF components
		 * 
		 * @date 2021-10-25 11:42:52
		 * Instead of calling `lookupImprintAssociative()` four times for R/Q/T/F,
		 * Call it once (by inlining) and perform in parallel
		 */
		enum FIND {
			FIND_F = 1 << 0,
			FIND_T = 1 << 1,
			FIND_Q = 1 << 2,
			FIND_R = 1 << 3,
		};
		unsigned find = FIND_R;

		// Get top-level QTF
		uint32_t R    = treeR.root;
		uint32_t tlQ  = treeR.N[R].Q;
		uint32_t tlTi = treeR.N[R].T & IBIT;
		uint32_t tlTu = treeR.N[R].T & ~IBIT;
		uint32_t tlF  = treeR.N[R].F;

		uint32_t sidR = 0, sidQ = 0, sidT = 0, sidF = 0;
		uint32_t tidR = 0, tidQ = 0, tidT = 0, tidF = 0;

		/*
		 * Which top-level components to lookup.  
		 */
		if (tlQ < tinyTree_t::TINYTREE_NSTART) {
			sidQ = fastLookupSid[tlQ];
			tidQ = fastLookupTid[tlQ];
		} else {
			find |= FIND_Q;
		}
		if (tlTu < tinyTree_t::TINYTREE_NSTART) {
			sidT = fastLookupSid[tlTu];
			tidT = fastLookupTid[tlTu];
		} else {
			find |= FIND_T;
		}
		if (tlTu != tlF) {
			// ignore F double reference when T==F
			if (tlF < tinyTree_t::TINYTREE_NSTART) {
				sidF = fastLookupSid[tlF];
				tidF = fastLookupTid[tlF];
			} else {
				find |= FIND_F;
			}
		}

		/*
		 * @date 2021-10-20 22:21:02
		 * 
		 * Perform an associative lookup for the root and the top-level components
		 * 
		 * Instead of performing 4x `database_t::lookupImprintAssociative()` with basically the same tree,
		 * Unwind the call by evaluating the tree once and examining the Q/T/F/R entry points.
		 * 
		 * The following code is taken directly from `database_t::lookupImprintAssociative()`.
		 * For optimisation, it assumes versioned memory is disabled.
		 * Comments have been trimmed.
		 */
		assert(pStore->imprintVersion == NULL); // not for versioned memory
		if (pStore->interleave == pStore->interleaveStep) {
			/*
			 * index is populated with key cols, runtime scans rows
			 * Because of the jumps, memory cache might be killed
			 */

			// permute all rows
			for (unsigned iRow = 0; iRow < MAXTRANSFORM; iRow += pStore->interleaveStep) {

				// find where the evaluator for the key is located in the evaluator store
				footprint_t *v = pStore->revEvaluator + iRow * tinyTree_t::TINYTREE_NEND;

				// apply the reverse transform
				treeR.eval(v);

				if (find & FIND_R) {
					// debug validation
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[R]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidR = pImprint->sid;
						tidR = pImprint->tid + iRow;

						find &= ~FIND_R;
						if (!find)
							break;
					}
				}

				if (find & FIND_Q) {
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[tlQ]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidQ = pImprint->sid;
						tidQ = pImprint->tid + iRow;

						find &= ~FIND_Q;
						if (!find)
							break;
					}
				}

				if (find & FIND_T) {
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[tlTu]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidT = pImprint->sid;
						tidT = pImprint->tid + iRow;

						find &= ~FIND_T;
						if (!find)
							break;
					}
				}

				if (find & FIND_F) {
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[tlF]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidF = pImprint->sid;
						tidF = pImprint->tid + iRow;

						find &= ~FIND_F;
						if (!find)
							break;
					}
				}

			}
		} else {
			/*
			 * index is populated with key rows, runtime scans cols
			 *
			 * @date 2020-04-18 22:51:05
			 *
			 * This path is cpu cache friendlier because of `iCol++`
			 */
			footprint_t *v = pStore->fwdEvaluator;

			// permutate all columns
			for (unsigned iCol = 0; iCol < pStore->interleaveStep; iCol++) {

				// apply the tree to the store
				treeR.eval(v);

				if (find & FIND_R) {
					// debug validation
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[R]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidR = pImprint->sid;
						tidR = pStore->revTransformIds[pImprint->tid + iCol];

						find &= ~FIND_R;
						if (!find)
							break;
					}
				}

				if (find & FIND_Q) {
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[tlQ]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidQ = pImprint->sid;
						tidQ = pStore->revTransformIds[pImprint->tid + iCol];

						find &= ~FIND_Q;
						if (!find)
							break;
					}
				}

				if (find & FIND_T) {
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[tlTu]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidT = pImprint->sid;
						tidT = pStore->revTransformIds[pImprint->tid + iCol];

						find &= ~FIND_T;
						if (!find)
							break;
					}
				}

				if (find & FIND_F) {
					// de-sign
					uint32_t ix = pStore->lookupImprint(v[tlF]);
					if (pStore->imprintIndex[ix] != 0) {
						imprint_t *pImprint = pStore->imprints + pStore->imprintIndex[ix];
						sidF = pImprint->sid;
						tidF = pStore->revTransformIds[pImprint->tid + iCol];

						find &= ~FIND_F;
						if (!find)
							break;
					}
				}

				v += tinyTree_t::TINYTREE_NEND;
			}
		}

		// test all components found
		if (find != 0) {
			return true;
		}

		/*
		 * Fixup delayed NE
		 */
		if (tlTu == tlF) {
			// root is NE, T-invert is stored in tlTi
			sidF = sidT;
			tidF = tidT;
		}

		assert(sidR && sidQ && sidT && sidF);

		/*
		 * @date 2021-10-25 12:26:23
		 * test for sid based collapse
		 * NOTE: `SID_ZERO=1`
		 */
		if (sidQ == pStore->SID_ZERO ||                               // Q not zero
		    (sidQ == sidT && tidQ == tidT) ||                         // Q/T fold
		    (sidQ == sidF && tidQ == tidF) ||                         // Q/F fold
		    (!tlTi && sidT == sidF && tidT == tidF) ||                // T/F fold
		    (sidT == pStore->SID_ZERO && sidF == pStore->SID_ZERO) || // Q?!0:0 -> Q
		    (!tlTi && sidT == pStore->SID_ZERO)) {                    // Q?0:F -> F?!Q:0
			skipCollapse++;
			return true;
		}

		/*
		 * @date 2021-10-20 02:22:04
		 * 
		 * Point of NO return.
		 * 
		 * The structure in `treeR` has been identified as: sidR/tidR, sidQ/tidQ, sidT/tidT, sidF/tidF.
		 */

		/*
		 * Convert tidR/tidQ/tidT/tidF such that `tidR=0`. 
		 * One of the test patterns used: "bcd?bca?>"
		 */
		{
			char reverse[MAXSLOTS + 1];

			// determine inverse transform to get from result to intermediate
			const char *revRstr = pStore->revTransformNames[tidR];
			const char *fwdQstr = pStore->fwdTransformNames[tidQ];
			const char *fwdTstr = pStore->fwdTransformNames[tidT];
			const char *fwdFstr = pStore->fwdTransformNames[tidF];

			// reverse the transforms
			for (int i = 0; i < MAXSLOTS; i++)
				reverse[i] = revRstr[fwdQstr[i] - 'a'];
			reverse[pStore->signatures[sidQ].numPlaceholder] = 0;
			tidQ = pStore->lookupFwdTransform(reverse);

			for (int i = 0; i < MAXSLOTS; i++)
				reverse[i] = revRstr[fwdTstr[i] - 'a'];
			reverse[pStore->signatures[sidT].numPlaceholder] = 0;
			tidT = pStore->lookupFwdTransform(reverse);

			for (int i = 0; i < MAXSLOTS; i++)
				reverse[i] = revRstr[fwdFstr[i] - 'a'];
			reverse[pStore->signatures[sidF].numPlaceholder] = 0;
			tidF = pStore->lookupFwdTransform(reverse);

			tidR = 0;
		}

		/*
		 * Sid-swap endpoints as the run-time would do
		 */
		tidQ = dbtool_t::sidSwapTid(*pStore, sidQ, tidQ, pStore->fwdTransformNames);
		tidT = dbtool_t::sidSwapTid(*pStore, sidT, tidT, pStore->fwdTransformNames);
		tidF = dbtool_t::sidSwapTid(*pStore, sidF, tidF, pStore->fwdTransformNames);

		/*
		 * Validate
		 */
		if (ctx.flags & ctx.MAGICMASK_PARANOID) {
			char       name[tinyTree_t::TINYTREE_NAMELEN * 3 + 1 + 1]; // for 3 trees and final operator
			unsigned   nameLen = 0;
			tinyTree_t tree(ctx);

			tree.loadStringFast(pStore->signatures[sidQ].name, pStore->fwdTransformNames[tidQ]);
			tree.saveString(tree.root, name + nameLen, NULL);
			nameLen = strlen(name);

			tree.loadStringFast(pStore->signatures[sidT].name, pStore->fwdTransformNames[tidT]);
			tree.saveString(tree.root, name + nameLen, NULL);
			nameLen = strlen(name);

			tree.loadStringFast(pStore->signatures[sidF].name, pStore->fwdTransformNames[tidF]);
			tree.saveString(tree.root, name + nameLen, NULL);
			nameLen = strlen(name);

			name[nameLen++] = (tlTi) ? '!' : '?';
			name[nameLen]   = 0;

			tree.loadStringFast(name);

			uint32_t sid = 0;
			uint32_t tid = 0;
			pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sid, &tid);

			assert(sid == sidR);
			assert(tid == 0);

//			printf("./eval \"%s\" \"%s\"\n", name, pStore->signatures[sidR].name);
		}

		if (tlTi)
			this->addPatternToDatabase(pNameR, sidR, sidQ, tidQ, sidT ^ IBIT, tidT, sidF, tidF);
		else
			this->addPatternToDatabase(pNameR, sidR, sidQ, tidQ, sidT, tidT, sidF, tidF);

		return true;
	}

	/*
	 * @date 2021-10-21 15:26:44
	 * 
	 * Add top level triplet to database
	 * Extract the 3 components and scan them as the runtime (Q/T/F Cartesian product) would do.
	 * Determine the transform needed to re-arrange the resulting slot for the `groupTree_t` node.
	 * `groupTree_t` does not scan trees for pattern matches but is a collection of prime structures that are Cartesian product.
	 * First step is the Cartesian product between Q and T.
	 * Second step are the ound combos cross-multiplied with F.
	 */
	uint32_t /*__attribute__((optimize("O0")))*/ addPatternToDatabase(const char *pNameR, uint32_t sidR, uint32_t sidQ, uint32_t tidQ, uint32_t sidT, uint32_t tidT, uint32_t sidF, uint32_t tidF) {
		assert(!(sidR & IBIT));
		assert(!(sidQ & IBIT));
		assert(!(sidF & IBIT));

		// reassembly transform
		char     slotsQ[MAXSLOTS + 1];
		char     slotsT[MAXSLOTS + 1];
		char     slotsF[MAXSLOTS + 1];
		char     slotsR[MAXSLOTS + 1];
		// reassembly TF transform relative to Q
		uint32_t tidSlotT  = 0;
		uint32_t tidSlotF  = 0;
		uint32_t tidSlotR  = 0;  // transform from `slotsR[]` to resulting `groupNode_t::slots[]`.
		// nodes already processed
		uint32_t beenThere = 0; // bit set means beenWhat[] is valid/defined
		char     beenWhat[tinyTree_t::TINYTREE_NEND]; // endpoint in resulting `slotsR[]`
		uint32_t nextSlot  = 0;
		
		if (opt_text == OPTTEXT_COMPARE) {

			// progress sidQ tidQ sidT tidT sidF tidF sidR treeR

			printf("%lu\t%u:%s\t%u:%.*s\t%u:%s%s\t%u:%.*s\t%u:%s\t%u:%.*s\t%u:%s\t%s\n",
			       ctx.progress,

			       sidQ, pStore->signatures[sidQ].name,
			       tidQ, pStore->signatures[sidQ].numPlaceholder, pStore->fwdTransformNames[tidQ],

			       sidT, pStore->signatures[sidT & ~IBIT].name,
			       (sidT & IBIT) ? "~" : "",
			       tidT, pStore->signatures[sidT & ~IBIT].numPlaceholder, pStore->fwdTransformNames[tidT],

			       sidF, pStore->signatures[sidF].name,
			       tidF, pStore->signatures[sidF].numPlaceholder, pStore->fwdTransformNames[tidF],

			       sidR, pStore->signatures[sidR].name, pNameR);
		}

		/*
		 * Slot population as `fragmentTree_t` would do
		 */

		signature_t *pSignature = pStore->signatures + sidQ;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			unsigned endpoint = pStore->fwdTransformNames[tidQ][iSlot] - 'a';
			// was it seen before
			if (!(beenThere & (1 << endpoint))) {
				beenWhat[endpoint] = (char) ('a' + nextSlot); // assign new placeholder
				slotsR[nextSlot]   = (char) ('a' + endpoint); // put endpoint in result
				nextSlot++;
				beenThere |= (1 << endpoint);
			}
			slotsQ[iSlot] = beenWhat[endpoint];
		}
		slotsQ[pSignature->numPlaceholder] = 0; // terminator
		(void) slotsQ; // suppress compiler warning "unused variable"

		pSignature = pStore->signatures + (sidT & ~IBIT);
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			unsigned endpoint = pStore->fwdTransformNames[tidT][iSlot] - 'a';
			// was it seen before
			if (!(beenThere & (1 << endpoint))) {
				beenWhat[endpoint] = (char) ('a' + nextSlot);
				slotsR[nextSlot]   = (char) ('a' + endpoint);
				nextSlot++;
				beenThere |= (1 << endpoint);
			}
			slotsT[iSlot] = beenWhat[endpoint];
		}
		slotsT[pSignature->numPlaceholder] = 0; // terminator

		pSignature = pStore->signatures + sidF;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			unsigned endpoint = pStore->fwdTransformNames[tidF][iSlot] - 'a';
			// was it seen before
			if (!(beenThere & (1 << endpoint))) {
				beenWhat[endpoint] = (char) ('a' + nextSlot);
				slotsR[nextSlot]   = (char) ('a' + endpoint);
				nextSlot++;
				beenThere |= (1 << endpoint);
			}
			slotsF[iSlot] = beenWhat[endpoint];
		}
		slotsF[pSignature->numPlaceholder] = 0; // terminator

		// slots should not overflow
		assert(nextSlot <= MAXSLOTS);

		slotsR[nextSlot] = 0; // terminator

#if 0
		/*
		 * @date 2021-10-21 19:22:25
		 * Structures that collapse, like "aab+b>" can have more slots than the resulting structure.
		 * In itself this does not pose a problem,
		 * The issue is that pattern duplicates will have mismatched slots and throw a fit.
		 * 
		 * @date 2021-10-25 20:03:41
		 * Problem seems to be solved by the truncating the slots in `sidSwapTid()`.
		 * 
		 * @date 2021-11-06 00:14:38
		 * `groupTree_t` needs collapses or things like "aabc^^^" will not be detected
		 */
		if (nextSlot != pStore->signatures[sidR].numPlaceholder) {
			// collapse occurred, structure is unsuited as a pattern.
			skipPlaceholder++;
			return 0;
		}
#else
		assert(nextSlot >= pStore->signatures[sidR].numPlaceholder);
#endif
		
		/*
		 * Get slot transforms relative to Q
		 */
		tidSlotR = pStore->lookupRevTransform(slotsR);
		tidSlotT = pStore->lookupFwdTransform(slotsT);
		tidSlotF = pStore->lookupFwdTransform(slotsF);
		assert(tidSlotR != IBIT);
		assert(tidSlotT != IBIT);
		assert(tidSlotF != IBIT);
		
		/*
		 * @date 2021-11-07 01:34:52
		 * 
		 * Order slots
		 * This is needed because reverse transforms of generated structures break ordering 
		 */
		
		tidSlotT = dbtool_t::sidSwapTid(*pStore, sidT & ~IBIT, tidSlotT, pStore->fwdTransformNames);
		tidSlotF = dbtool_t::sidSwapTid(*pStore, sidF, tidSlotF, pStore->fwdTransformNames);

		/*
		 * @date 2021-10-21 23:45:02
		 * The result slots can have swapped placeholders
		 * 
		 * 'a baac>! >' 'a caab>! >'
		 * 
		 * "baac>!" and "caab>!" are distinct, however as component they can exchange b/c
		 * The difference between these two are found in `slotsR[]`.
		 * 
		 * @date 2021-10-25 15:36:47
		 * There should be a total of 4 calls to `sidSwapTid()`.
		 */
		tidSlotR = dbtool_t::sidSwapTid(*pStore, sidR, tidSlotR, pStore->revTransformNames);

		/*
		 * Add to database
		 */

		// lookup/create first
		uint32_t ixFirst = pStore->lookupPatternFirst(sidQ, sidT, tidSlotT);
		if (pStore->patternFirstIndex[ixFirst] == 0)
			pStore->patternFirstIndex[ixFirst] = pStore->addPatternFirst(sidQ, sidT, tidSlotT);
		uint32_t idFirst = pStore->patternFirstIndex[ixFirst];

		// lookup/create second
		uint32_t        ixSecond = pStore->lookupPatternSecond(idFirst, sidF, tidSlotF);
		uint32_t        idSecond;
		patternSecond_t *patternSecond;

		if (pStore->patternSecondIndex[ixSecond] == 0) {
			pStore->patternSecondIndex[ixSecond] = pStore->addPatternSecond(idFirst, sidF, tidSlotF);

			// new entry
			idSecond      = pStore->patternSecondIndex[ixSecond];
			patternSecond = pStore->patternsSecond + idSecond;

			assert(sidR < (1 << 20));

			patternSecond->sidR     = sidR;
			patternSecond->tidSlotR = tidSlotR;

			if (opt_text == OPTTEXT_WON) {
				/*
				 * Construct tree containing sid/tid
				 */
				tinyTree_t tree(ctx);
				uint32_t   tlQ = tree.addStringFast(pStore->signatures[sidQ].name, pStore->fwdTransformNames[tidQ]);
				uint32_t   tlT = tree.addStringFast(pStore->signatures[sidT & ~IBIT].name, pStore->fwdTransformNames[tidT]);
				uint32_t   tlF = tree.addStringFast(pStore->signatures[sidF].name, pStore->fwdTransformNames[tidF]);
				tree.root = tree.addBasicNode(tlQ, tlT ^ ((sidT & IBIT) ? IBIT : 0), tlF);

				printf("%s\n", tree.saveString(tree.root));
			}
		} else {
			// verify duplicate
			idSecond      = pStore->patternSecondIndex[ixSecond];
			patternSecond = pStore->patternsSecond + idSecond;

			if (patternSecond->sidR != sidR || patternSecond->tidSlotR != tidSlotR) {
				/*
				 * @date 2021-10-25 19:29:56
				 * Be very verbose.
				 * This is a very nasty situation that may arise hours into the run.
				 */
				fprintf(stderr, "{\"error\":\"addPatternToDatabase\",\"progress\":\"%lu\",\"name\":\"%s\",\"idFirst\":\"%u\",\"idSecond\":\"%u\","
						"\"oldSidR\":\"%u:%s\",\"sidR:%u:%s\","
						"\"oldTidSlotR\":\"%u:%.*s\",\"tidSlotR:%u:%.*s\","
						"\"sidQ\":\"%u:%s\",\"tidQ\":\"%u:%.*s\",\"sidT\":\"%u:%s%s\",\"tidT\":\"%u:%.*s\",\"sidF:%u:%s\",\"tidF\":\"%u:%.*s\","
						"\"tidSlotT\":\"%u:%.*s\",\"tidSlotF\":\"%u:%.*s\","
						"\"slotsQ\":\"%s\",\"slotsT\":\"%s\",\"slotsF\":\"%s\",\"slotsR:%s\"}\n",
					ctx.progress, pNameR, idFirst, idSecond,

					patternSecond->sidR, pStore->signatures[patternSecond->sidR].name,
					sidR, pStore->signatures[sidR].name,
					patternSecond->tidSlotR, pStore->signatures[patternSecond->sidR].numPlaceholder, pStore->fwdTransformNames[patternSecond->tidSlotR],
					tidSlotR, pStore->signatures[sidR].numPlaceholder, pStore->fwdTransformNames[tidSlotR],

					sidQ, pStore->signatures[sidQ].name,
					tidQ, pStore->signatures[sidQ].numPlaceholder, pStore->fwdTransformNames[tidQ],

					sidT & ~IBIT, pStore->signatures[sidT & ~IBIT].name,
					(sidT & IBIT) ? "~" : "",
					tidT, pStore->signatures[sidT & ~IBIT].numPlaceholder, pStore->fwdTransformNames[tidT],

					sidF, pStore->signatures[sidF].name,
					tidF, pStore->signatures[sidF].numPlaceholder, pStore->fwdTransformNames[tidF],

					tidSlotT, pStore->signatures[sidT & ~IBIT].numPlaceholder, pStore->fwdTransformNames[tidSlotT],
					tidSlotF, pStore->signatures[sidF].numPlaceholder, pStore->fwdTransformNames[tidSlotF],

					slotsQ,
					slotsT,
					slotsF,
					slotsR);

				/*
				 * @date 2021-11-08 02:15:53
				 * 
				 * emergency break, total collection corrupt
				 * 
				 * Difference is highly expected to be `tidSlotR`
				 * All alternatives should and must be identical in creating `groupNode_t::slots[]` 
				 */
				assert(patternSecond->sidR == sidR);
				assert(patternSecond->tidSlotR == tidSlotR);
			}

			skipDuplicate++;
		}

		return idSecond;
	}

	/**
	 * @date 2020-04-05 21:07:14
	 *
	 * Compare function for `qsort_r`
	 *
	 * @param {member_t} lhs - left hand side member
	 * @param {member_t} rhs - right hand side member
	 * @param {context_t} arg - I/O context
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int comparMember(const void *lhs, const void *rhs, void *arg) {
		if (lhs == rhs)
			return 0;

		const member_t *pMemberL = static_cast<const member_t *>(lhs);
		const member_t *pMemberR = static_cast<const member_t *>(rhs);
		context_t      *pApp     = static_cast<context_t *>(arg);

		int cmp = 0;

		/*
		 * depreciates go last
		 */
		if ((pMemberL->flags & member_t::MEMMASK_DEPR) && !(pMemberR->flags & member_t::MEMMASK_DEPR))
			return +1;
		if (!(pMemberL->flags & member_t::MEMMASK_DEPR) && (pMemberR->flags & member_t::MEMMASK_DEPR))
			return -1;

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.loadStringFast(pMemberL->name);
		treeR.loadStringFast(pMemberR->name);

		// order by size first because (smaller) components must be located first 
		cmp = (int) treeL.count - (int) treeR.count;
		if (cmp)
			return cmp;

		cmp = treeL.compare(treeL.root, &treeR, treeR.root);
		return cmp;
	}

	/**
	 * @date 2020-03-22 01:00:05
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add newly unique patterns to the database
	 */
	void /*__attribute__((optimize("O0")))*/ patternsFromFile(void) {

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading patterns from file\n", ctx.timeAsString());

		FILE *f;
		if (strcmp(this->opt_load, "-") != 0) {
			f = fopen(this->opt_load, "r");
			if (f == NULL)
				ctx.fatal("\n{\"error\":\"fopen('%s') failed\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n",
					  this->opt_load, __FUNCTION__, __FILE__, __LINE__);
		} else {
			f = stdin;
		}

		// apply settings for `--window`
		generator.windowLo = this->opt_windowLo;
		generator.windowHi = this->opt_windowHi;

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;
		skipDuplicate = skipCollapse = skipPlaceholder = skipWildcard = 0;

		char     name[64];
		unsigned numPlaceholder, numEndpoint, numBackRef;
		this->truncated = 0;

		tinyTree_t tree(ctx);

		// <name> [ <numPlaceholder> <numEndpoint> <numBackRef> ]
		for (;;) {
			static char line[512];

			if (::fgets(line, sizeof(line), f) == 0)
				break; // end-of-input

			name[0] = 0;
			int ret = ::sscanf(line, "%s %u %u %u\n", name, &numPlaceholder, &numEndpoint, &numBackRef);

			// calculate values
			unsigned        newPlaceholder = 0, newEndpoint = 0, newBackRef = 0;
			unsigned        beenThere      = 0;
			for (const char *p             = name; *p; p++) {
				if (::islower(*p)) {
					if (!(beenThere & (1 << (*p - 'a')))) {
						newPlaceholder++;
						beenThere |= 1 << (*p - 'a');
					}
					newEndpoint++;
				} else if (::isdigit(*p) && *p != '0') {
					newBackRef++;
				}
			}

			if (ret != 1 && ret != 4)
				ctx.fatal("\n{\"error\":\"bad/empty line\",\"where\":\"%s:%s:%d\",\"linenr\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress);
			if (ret == 4 && (numPlaceholder != newPlaceholder || numEndpoint != newEndpoint || numBackRef != newBackRef))
				ctx.fatal("\n{\"error\":\"line has incorrect values\",\"where\":\"%s:%s:%d\",\"linenr\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress);

			// test if line is within progress range
			// NOTE: first line has `progress==0`
			if ((generator.windowLo && ctx.progress < generator.windowLo) || (generator.windowHi && ctx.progress >= generator.windowHi)) {
				ctx.progress++;
				continue;
			}

			/*
			 * construct tree
			 */
			tree.loadStringFast(name);

			/*
			 * call `foundTreePattern()`
			 */

			if (!opt_fast) {
				if (!foundTreePattern(tree, name, newPlaceholder, newEndpoint, newBackRef))
					break;
			} else {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numPatternFirst=%u(%.0f%%) numPatternSecond=%u(%.0f%%) | skipDuplicate=%u | hash=%.3f %s",
							ctx.timeAsString(), ctx.progress, perSecond,
							pStore->numPatternFirst, pStore->numPatternFirst * 100.0 / pStore->maxPatternFirst,
							pStore->numPatternSecond, pStore->numPatternSecond * 100.0 / pStore->maxPatternSecond,
							skipDuplicate, (double) ctx.cntCompare / ctx.cntHash, name);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;


						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numPatternFirst=%u(%.0f%%) numPatternSecond=%u(%.0f%%) |  skipDuplicate=%u | hash=%.3f %s",
							ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - generator.windowLo) * 100.0 / (ctx.progressHi - generator.windowLo), etaH, etaM, etaS,
							pStore->numPatternFirst, pStore->numPatternFirst * 100.0 / pStore->maxPatternFirst,
							pStore->numPatternSecond, pStore->numPatternSecond * 100.0 / pStore->maxPatternSecond,
							skipDuplicate, (double) ctx.cntCompare / ctx.cntHash, name);
					}

					if (ctx.restartTick) {
						// passed a restart point
						fprintf(stderr, "\n");
						ctx.restartTick = 0;
					}

					ctx.tick = 0;
				}

				uint32_t R    = tree.root;
				uint32_t tlQ  = tree.N[R].Q;
				uint32_t tlTi = tree.N[R].T & IBIT;
				uint32_t tlTu = tree.N[R].T & ~IBIT;
				uint32_t tlF  = tree.N[R].F;

				uint32_t sidR = 0, sidQ = 0, sidT = 0, sidF = 0;
				uint32_t tidR = 0, tidQ = 0, tidT = 0, tidF = 0;

				pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sidR, &tidR, R);

				if (tlQ < tinyTree_t::TINYTREE_NSTART) {
					sidQ = fastLookupSid[tlQ];
					tidQ = fastLookupTid[tlQ];
				} else {
					pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sidQ, &tidQ, tlQ);
				}
				if (tlTu < tinyTree_t::TINYTREE_NSTART) {
					sidT = fastLookupSid[tlTu];
					tidT = fastLookupTid[tlTu];
				} else {
					pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sidT, &tidT, tlTu);
				}
				if (tlTu != tlF) {
					// ignore F double reference when T==F
					if (tlF < tinyTree_t::TINYTREE_NSTART) {
						sidF = fastLookupSid[tlF];
						tidF = fastLookupTid[tlF];
					} else {
						pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sidF, &tidF, tlF);
					}
				}

				if (tlTu == tlF) {
					assert(tlTi);
					addPatternToDatabase(name, sidR, sidQ, tidQ, sidT ^ IBIT, tidT, sidT, tidT);
				} else if (tlTi) {
					addPatternToDatabase(name, sidR, sidQ, tidQ, sidT ^ IBIT, tidT, sidF, tidF);
				} else {
					addPatternToDatabase(name, sidR, sidQ, tidQ, sidT, tidT, sidF, tidF);
				}
			}

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (truncated) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: Pattern storage full. Truncating at progress=%lu \"%s\"\n",
					ctx.timeAsString(), this->truncated, this->truncatedName);

			// save position for final status
			this->opt_windowHi = this->truncated;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Read %lu patterns. numSignature=%u(%.0f%%) numPatternFirst=%u(%.0f%%) numPatternSecond=%u(%.0f%%) | skipDuplicate=%u  skipWildcard=%u  skipCollapse=%u  skipPlaceholder=%u\n",
				ctx.timeAsString(),
				ctx.progress,
				pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				pStore->numPatternFirst, pStore->numPatternFirst * 100.0 / pStore->maxPatternFirst,
				pStore->numPatternSecond, pStore->numPatternSecond * 100.0 / pStore->maxPatternSecond,
				skipDuplicate, skipWildcard, skipCollapse, skipPlaceholder);

	}

	/**
	 * @date 2021-10-19 23:17:46
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add newly unique signatures to the database
	 *
	 * @param {database_t} pStore - memory based database
	 */
	void /*__attribute__((optimize("O0")))*/ patternsFromGenerator(void) {

		/*
		 * Apply window/task setting on generator
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
			if (this->opt_taskId || this->opt_taskLast) {
				if (this->opt_windowHi)
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%lu-%lu\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_windowLo, this->opt_windowHi);
				else
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%lu-last\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_windowLo);
			} else if (this->opt_windowLo || this->opt_windowHi) {
				if (this->opt_windowHi)
					fprintf(stderr, "[%s] INFO: window=%lu-%lu\n", ctx.timeAsString(), this->opt_windowLo, this->opt_windowHi);
				else
					fprintf(stderr, "[%s] INFO: window=%lu-last\n", ctx.timeAsString(), this->opt_windowLo);
			}
		}

		// apply settings for `--window`
		generator.windowLo = this->opt_windowLo;
		generator.windowHi = this->opt_windowHi;

		// setup restart data, only for 5n9+
		if (arg_numNodes > 4) {
			// walk through list
			const metricsRestart_t *pRestart = getMetricsRestart(MAXSLOTS, arg_numNodes, ctx.flags & context_t::MAGICMASK_PURE);
			// point to first entry if section present
			if (pRestart && pRestart->sectionOffset)
				generator.pRestartData = restartData + pRestart->sectionOffset;
		}

		// reset progress
		if (generator.windowHi) {
			ctx.setupSpeed(generator.windowHi);
		} else {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, arg_numNodes, ctx.flags & context_t::MAGICMASK_PURE);
			ctx.setupSpeed(pMetrics ? pMetrics->numProgress : 0);
		}
		ctx.tick = 0;
		skipDuplicate = skipCollapse = skipPlaceholder = skipWildcard = 0;

		/*
		 * Generate candidates
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", ctx.timeAsString(), arg_numNodes, MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE ? "-pure" : "");

		if (arg_numNodes == 0) {
			tinyTree_t tree(ctx);

			tree.root = 0; // "0"
			foundTreePattern(tree, "0", 0, 0, 0);
			tree.root = 1; // "a"
			foundTreePattern(tree, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = arg_numNodes * 2 + 1;

			generator.initialiseGenerator();
			generator.clearGenerator();
			generator.generateTrees(arg_numNodes, endpointsLeft, 0, 0, this, static_cast<generator_t::generateTreeCallback_t>(&genpatternContext_t::foundTreePattern));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.progress != ctx.progressHi && this->opt_windowLo == 0 && this->opt_windowHi == 0) {
			// can only test if windowing is disabled
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s:%s:%d\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%u}\n",
			       __FUNCTION__, __FILE__, __LINE__, ctx.progress, ctx.progressHi, arg_numNodes);
		}

		if (truncated) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: Pattern storage full. Truncating at progress=%lu \"%s\"\n",
					ctx.timeAsString(), this->truncated, this->truncatedName);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%u pure=%u numNode=%u numCandidate=%lu numPatternFirst=%u(%.0f%%) numPatternSecond=%u(%.0f%%) | skipDuplicate=%u  skipWildcard=%u  skipCollapse=%u  skipPlaceholder=%u\n",
				ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, arg_numNodes, ctx.progress,
				pStore->numPatternFirst, pStore->numPatternFirst * 100.0 / pStore->maxPatternFirst,
				pStore->numPatternSecond, pStore->numPatternSecond * 100.0 / pStore->maxPatternSecond,
				skipDuplicate, skipWildcard, skipCollapse, skipPlaceholder);

	}
};
