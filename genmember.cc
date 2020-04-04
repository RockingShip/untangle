//# pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-30 17:20:21
 *
 * Collect signature group members.
 *
 * Basic group members share the same node size, which is the smallest a signature group can have.
 * A member is considered safe if the three components and heads all reference safe members.
 * Some groups are unsafe. Replacements are found by selecting larger structures.
 *
 * Keep smaller unsafe nodes for later normalisations.
 *
 * normalisation:
 * 1) Algebraic (function grouping)
 * 2) Dyadic ordering (layout ordering)
 * 3) Imprints (layout orientation "skins")
 * 4) Signature groups (restructuring)
 * Basically, `genmember` collects structures that do not trigger normalisation or orphans when used for creation/construction.
 *
 * @date 2020-04-01 23:48:02
 *
 * I always thought that the goal motivation was to replace structures with with smallest nodesize but that might not be the case.
 * 3040 signature groups in 4n9 space fail to have safe members. However, they do exist in 5n9 space.
 *
 * @date 2020-04-02 10:44:05
 *
 * Structures have heads and tails.
 * Tails are components and sub-components, heads are the structures minus one node.
 * Safe members have safe heads and tails.
 * Size of signature group is size of smallest safe member.
 *
 * @date 2020-04-02 23:43:18
 *
 * Unsafe members start to occur in 4n9 space, just like back-references.
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
struct genmemberContext_t : context_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} size of signatures to be generated in this invocation
	unsigned arg_numNodes;
	/// @var {string} name of file containing extra input
	const char *opt_append;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} size of imprint index WARNING: must be prime
	uint32_t opt_imprintIndexSize;
	/// @var {number} interleave for associative imprint index
	unsigned opt_interleave;
	/// @var {number} job Id. First job=1
	unsigned opt_jobId;
	/// @var {number} Number of jobs
	unsigned opt_jobLast;
	/// @var {number} --keep, do not delete output database in case of errors
	unsigned opt_keep;
	/// @var {number} Maximum number of imprints to be stored database
	uint32_t opt_maxImprint;
	/// @var {number} Maximum number of members to be stored database
	uint32_t opt_maxMember;
	/// @var {number} Maximum number of signatures to be stored database
	uint32_t opt_maxSignature;
	/// @var {number} size of member index WARNING: must be prime
	uint32_t opt_memberIndexSize;
	/// @var {number} --metrics, Collect metrics intended for "metrics.h"
	unsigned opt_metrics;
	/// @var {number} index/data ratio
	double opt_ratio;
	/// @var {number} size of signature index WARNING: must be prime
	uint32_t opt_signatureIndexSize;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {number} --test, run without output
	unsigned opt_test;
	/// @var {number} Collect unsafe replacements
	unsigned opt_unsafe;
	/// @var {number} generator upper bound
	uint64_t opt_windowHi;
	/// @var {number} generator lower bound
	uint64_t opt_windowLo;

	/// @var {database_t} - Database store to place results
	database_t *pStore;
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;

	uint32_t skipDuplicate;
	uint32_t skipSize;
	uint32_t skipUnsafe;
	uint32_t skipIdentical;
	uint32_t numUnsafe;

	/**
	 * Constructor
	 */
	genmemberContext_t() {
		// arguments and options
		arg_outputDatabase = NULL;
		arg_numNodes = 0;
		opt_append = NULL;
		opt_force = 0;
		opt_imprintIndexSize = 0;
		opt_interleave = 504; // NOTE: Lots of imprint creation and memory. 504 is faster than METRICS_DEFAULT_INTERLEAVE (120)
		opt_jobId = 0;
		opt_jobLast = 0;
		opt_keep = 0;
		opt_maxImprint = 0;
		opt_maxMember = 0;
		opt_maxSignature = 0;
		opt_memberIndexSize = 0;
		opt_metrics = 0;
		opt_ratio = METRICS_DEFAULT_RATIO / 10.0;
		opt_signatureIndexSize = 0;
		opt_test = 0;
		opt_text = 0;
		opt_unsafe = 0;

		pStore = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;

		skipDuplicate = 0;
		skipSize = 0;
		skipUnsafe = 0;
		skipIdentical = 0;
		numUnsafe = 0;
	}

	/**
	 * @date 2020-03-28 18:29:25
	 *
	 * Test if candidate can be a signature group member and add when possible
	 *
	 * @date 2020-04-02 11:41:44
	 *
	 * for `signature_t`, only use `flags`, `size` and `firstMember`.
	 *
	 * @param {generatorTree_t} tree - candidate tree
	 * @param {string} pName - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints in tree
	 */
	void foundTreeMember(const generatorTree_t &treeR, const char *pNameR, unsigned numPlaceholder) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			int perSecond = this->updateSpeed();

			if (perSecond == 0 || progress > progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numImprint=%u numSignature=%u numMember=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipIdentical=%u",
				        timeAsString(), progress, perSecond, pStore->numImprint, pStore->numSignature, pStore->numMember, numUnsafe,
				        skipDuplicate, skipSize, skipUnsafe, skipIdentical);
			} else {
				int eta = (int) ((progressHi - progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numImprint=%u numSignature=%u numMember=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipIdentical=%u",
				        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, pStore->numImprint, pStore->numSignature, pStore->numMember, numUnsafe,
				        skipDuplicate, skipSize, skipUnsafe, skipIdentical);
			}

			if (treeR.restartTick) {
				// passed a restart point
				fprintf(stderr, "\n");
				// todo: writable `restartTick`. Like make it a pointer to static
				(*(generatorTree_t *) &treeR).restartTick = 0;
			}
		}

		/*
		 * Find the matching signature group. It's layout only so ignore transformId.
		 */

		uint32_t sid = 0;
		uint32_t tid = 0;
		signature_t *pSignature = NULL;

		if (!pStore->lookupImprintAssociative(&treeR, pEvalFwd, pEvalRev, &sid, &tid)) {
			// in `--unsafe` mode, only accept candidates that have existing imprints
			if (this->opt_unsafe)
				return;

			// create new signature group with temporary name
			sid = pStore->addSignature(pNameR);

			pSignature = pStore->signatures + sid;

			// mark signature group unsafe until first safe member found
			pSignature->flags = signature_t::SIGMASK_UNSAFE;
			// initial node-size of group members
			pSignature->size = treeR.count - tinyTree_t::TINYTREE_NSTART;
			numUnsafe++;

			// populate index for imprints
			pStore->addImprintAssociative(&treeR, this->pEvalFwd, this->pEvalRev, sid);

		} else {
			pSignature = pStore->signatures + sid;

			// only if group is safe reject is structure is too large
			if ((~pSignature->flags & signature_t::SIGMASK_UNSAFE) && treeR.count - tinyTree_t::TINYTREE_NSTART > pSignature->size) {
				skipSize++;
				return;
			}
		}

		/*
		 * Create a new member or reject as duplicate
		 */

		uint32_t ix = pStore->lookupMember(pNameR);
		if (pStore->memberIndex[ix] != 0) {
			// duplicate candidate name
			skipDuplicate++;
			return;
		}

		// allocate member
		uint32_t mid = pStore->addMember(pNameR);
		pStore->memberIndex[ix] = mid;

		member_t *pMember = pStore->members + mid;

		/*
		 * Name/notation analysis
		 */

		pMember->sid = sid;
		pMember->numPlaceholder = numPlaceholder;

		// name/notation analysis
		for (const char *p = pNameR; *p; p++) {
			if (islower(*p)) {
				pMember->numEndpoint++;
			} else if (isdigit(*p)) {
				pMember->numBackRef++;
			}
		}

		/*
		 * @date 2020-03-29 15:34:32
		 *
		 * Analyse and lookup components (tails)
		 *
		 * Components might have (from a component point of view) a different ordering
		 * like the `F` component in `"ab+bc+a12!!"` which is `"ab+bc+a12!!"`, giving a problem as `"cab+ca+!/bca"`
		 *
		 * Filter them out (by utilizing that `encode()` does not order)
		 *
		 * example of unsafe components: `"ebcabc?!ad1!!"`
		 *   components are `"a"`, `"bcabc?"` and `"adbcabc?!!"`
		 *   `"adbcabc?!!"` is unsafe because it can be rewritten as `"cdab^!/bcad"`

		 */

		if (sid < 3) {
			/*
			 * @date 2020-03-29 23:16:43
			 *
			 * Reserved root sids
			 *
			 * `"N[0] = 0?!0:0"` // zero value, zero QnTF operator, zero reference
			 * `"N[a] = 0?!0:a"` // self reference
			 *
			 */

			// reserved root sids
			assert(sid != 1 || strcmp(pNameR, "0") == 0);
			assert(sid != 2 || strcmp(pNameR, "a") == 0);

			pMember->Qmid = pMember->Tmid = pMember->Fmid = mid;
			pMember->Qsid = pMember->Tsid = pMember->Fsid = sid;
		} else {
			/*
			 * @date 2020-03-29 23:36:18
			 *
			 * Extract components and lookup if they exist.
			 * Components need to be validated signature group members.
			 * If no member is found then this candidate will never appear during run-time.
			 *
			 * Don't reject, just flag as unsafe.
			 *
			 * This is because there are single member groups that use unnormalised components.
			 * Example "faedabc?^?2!".
			 *
			 * The 'T' component is "aedabc?^?" which would/should normalise to "aecd^?"
			 * However, this component cannot be rewritten because `F` has a reference lock on the "^".
			 *
			 * Trying to create the tree using the display name will have the effect that `T` will be substituted by "aecd^?" and `F` expanded to "dabc?^"
			 * resulting in "faecd^?dabc?^!" which is one node larger.
			 *
			 * There is a reasonable chance that the result will create a loop during reconstruction.
			 * For that reason the candidate is flagged unsafe.
			 *
			 * For lower-level normalisation these entries could be dropped
			 * but on higher levels ignoring these might cause duplicate/similars to occur resulting in uncontrolled growth of expression trees.
			 *
			 * for 4n9, 2976 of the 791646 signatures are unsafe.
			 */
			char skin[MAXSLOTS + 1];

			uint32_t Q = treeR.N[treeR.root].Q;
			{
				const char *pComponentName = treeR.encode(Q, skin);
				uint32_t ix2 = pStore->lookupMember(pComponentName);

				pMember->Qmid = pStore->memberIndex[ix2];
				pMember->Qsid = pStore->members[pMember->Qmid].sid;

				// member is unsafe if component not found or unsafe
				if (pMember->Qmid == 0 || pStore->members[pMember->Qmid].sid == 0 || (pStore->members[pMember->Qmid].flags & signature_t::SIGMASK_UNSAFE))
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
			}

			uint32_t To = treeR.N[treeR.root].T & ~IBIT;
			{
				const char *pComponentName = treeR.encode(To, skin);
				uint32_t ix2 = pStore->lookupMember(pComponentName);

				pMember->Tmid = pStore->memberIndex[ix2];
				pMember->Tsid = pStore->members[pMember->Tmid].sid ^ (treeR.N[treeR.root].T & IBIT);

				// member is unsafe if component not found or unsafe
				if (pMember->Tmid == 0 || pStore->members[pMember->Tmid].sid == 0 || (pStore->members[pMember->Tmid].flags & signature_t::SIGMASK_UNSAFE))
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
			}

			uint32_t F = treeR.N[treeR.root].F;
			{
				const char *pComponentName = treeR.encode(F, skin);
				uint32_t ix2 = pStore->lookupMember(pComponentName);

				pMember->Fmid = pStore->memberIndex[ix2];
				pMember->Fsid = pStore->members[pMember->Fmid].sid;

				// member is unsafe if component not found or unsafe
				if (pMember->Fmid == 0 || pStore->members[pMember->Fmid].sid == 0 || (pStore->members[pMember->Fmid].flags & signature_t::SIGMASK_UNSAFE))
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
			}
		}

		/*
		 * @date 2020-04-01 22:30:09
		 *
		 * Analyse and lookup providers (heads)
		 *
		 * example of unsafe head: `"cbdabc!!e21!!"`
		 *   Heads are `"eabc!dc1!!"`, `"cedabc!e!!"` and `"cbdabc!!e!"`
		 *   `"cbdabc!!e!"` is unsafe because that can be rewritten to `"cab&d?/bdce"`
		 */
		{
			tinyTree_t tree(*this);
			unsigned numHead = 0; // number of found heads

			// replace `hot` node with placeholder
			for (unsigned hot = tinyTree_t::TINYTREE_NSTART; hot < treeR.root; hot++) {
				uint32_t select = 1 << treeR.root | 1 << 0; // selected nodes to extract nodes
				uint32_t nextPlaceholderPlaceholder = tinyTree_t::TINYTREE_KSTART;
				uint32_t what[tinyTree_t::TINYTREE_NEND];
				what[0] = 0; // replacement for zero

				// scan tree for needed nodes, ignoring `hot` node
				for (uint32_t k = treeR.root; k >= tinyTree_t::TINYTREE_NSTART; k--) {
					if (k != hot && (select & (1 << k))) {
						const tinyNode_t *pNode = treeR.N + k;
						const uint32_t Q = pNode->Q;
						const uint32_t To = pNode->T & ~IBIT;
						const uint32_t F = pNode->F;

						if (Q >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << Q;
						if (To >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << To;
						if (F >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << F;
					}
				}

				// prepare for extraction
				tree.clearTree();
				// remove `hot` node from selection
				select &= ~(1 << hot);

				/*
				 * Extract head.
				 * Replacing references by placeholders changes dyadic ordering.
				 * `what[hot]` is not a reference but a placeholder
				 */
				for (uint32_t k = tinyTree_t::TINYTREE_NSTART; k <= treeR.root; k++) {
					if (k != hot && select & (1 << k)) {
						const tinyNode_t *pNode = treeR.N + k;
						const uint32_t Q = pNode->Q;
						const uint32_t To = pNode->T & ~IBIT;
						const uint32_t Ti = pNode->T & IBIT;
						const uint32_t F = pNode->F;

						// assign placeholder to endpoint or `hot`
						if (~select & (1 << Q)) {
							what[Q] = nextPlaceholderPlaceholder++;
							select |= 1 << Q;
						}
						if (~select & (1 << To)) {
							what[To] = nextPlaceholderPlaceholder++;
							select |= 1 << To;
						}
						if (~select & (1 << F)) {
							what[F] = nextPlaceholderPlaceholder++;
							select |= 1 << F;
						}

						// mark replacement of old node
						what[k] = tree.count;
						select |= 1 << k;

						/*
						 * Reminder:
						 *  [ 2] a ? ~0 : b                  "+" OR
						 *  [ 6] a ? ~b : 0                  ">" GT
						 *  [ 8] a ? ~b : b                  "^" XOR
						 *  [ 9] a ? ~b : c                  "!" QnTF
						 *  [16] a ?  b : 0                  "&" AND
						 *  [19] a ?  b : c                  "?" QTF
						 */

						// perform dyadic ordering
						if (To == 0 && Ti && tree.compare(what[Q], tree, what[F]) > 0) {
							// reorder OR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (To == F && tree.compare(what[Q], tree, what[F]) > 0) {
							// reorder XOR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = what[Q] ^ IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (F == 0 && !Ti && tree.compare(what[Q], tree, what[To]) > 0) {
							// reorder AND
							tree.N[tree.count].Q = what[To];
							tree.N[tree.count].T = what[Q];
							tree.N[tree.count].F = 0;
						} else {
							// default
							tree.N[tree.count].Q = what[Q];
							tree.N[tree.count].T = what[To] ^ Ti;
							tree.N[tree.count].F = what[F];
						}

						tree.count++;
					}
				}

				// set root
				tree.root = tree.count - 1;

				// get head name/notation
				char skin[MAXSLOTS + 1];
				char name[tinyTree_t::TINYTREE_NAMELEN + 1];
				tree.encode(tree.root, name, skin);

				// perform member lookup
				ix = pStore->lookupMember(name);
				uint32_t midHead = pStore->memberIndex[ix];
				if (midHead == 0) {
					// unsafe
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
				} else {
					// test if head already present
					for (unsigned k = 0; k < member_t::MAXHEAD && pMember->heads[k]; k++) {
						if (pMember->heads[k] == midHead) {
							// found
							midHead = 0;
							break;
						}
					}

					// add to list
					if (midHead) {
						assert(numHead < member_t::MAXHEAD);
						pMember->heads[numHead++] = midHead;
					}
				}
			}
		}

		/*
		 * To reject, or not to reject...
		 */

		if (pSignature->flags & signature_t::SIGMASK_UNSAFE) {
			if (pMember->flags & signature_t::SIGMASK_UNSAFE) {
				/*
				 * group/candidate both unsafe
				 * Add to group if same node size
				 */
				if (treeR.count - tinyTree_t::TINYTREE_NSTART > pSignature->size) {
					skipUnsafe++;
					pStore->numMember--; // undo candidate member
					return;
				}
				assert(treeR.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);
			} else {
				/*
				 * group is unsafe, candidate is safe.
				 * If candidate is same size then drop all existing unsafe group members
				 * If candidate is larger then keep all smaller unsafe members for later optimisations
				 */

				if (treeR.count - tinyTree_t::TINYTREE_NSTART == pSignature->size) {
					// empty group
					while (pSignature->firstMember) {
						// get member
						member_t *p = pStore->members + pSignature->firstMember;
						// remove from list
						pSignature->firstMember = pMember->nextMember;
						// this creates orphan candidates, zero them
						::memset(p, 0, sizeof(*p));
					}
				}

				// finalise group
				pSignature->flags &= ~signature_t::SIGMASK_UNSAFE;
				pSignature->size = treeR.count - tinyTree_t::TINYTREE_NSTART;

				pSignature->firstMember = mid;

				if (opt_text == 5)
					printf("%s\t%u\n", pMember->name, pMember->numPlaceholder);
				if (opt_text == 6)
					printf("%s\t%u\n", pMember->name, pMember->numPlaceholder);

				// one unsafe group less
				numUnsafe--;
				return;
			}
		} else {
			if (pMember->flags & signature_t::SIGMASK_UNSAFE) {
				// group is safe, candidate not. Drop candidate
				skipUnsafe++;
				pStore->numMember--; // undo candidate member
				return;
			} else {
				// group/candidate both safe
				assert(treeR.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);
			}
		}

		/*
		 * Add member to signature group
		 */
		if (opt_text == 5)
			printf("%s\t%u\n", pMember->name, pMember->numPlaceholder);

		pMember->nextMember = pSignature->firstMember;
		pSignature->firstMember = mid;
	}

	/**
	 * @date 2020-03-27 17:05:07
	 *
	 * Compare function for `qsort_r`
	 *
	 * @param {signature_t} lhs - left hand side signature
	 * @param {signature_t} rhs - right hand side signature
	 * @param {context_t} state - I/O contect needed to create trees
	 * @return
	 */
	static int compar(const void *lhs, const void *rhs, void *state) {
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

		// compare
		return treeL.compare(treeL.root, treeR, treeR.root);
	}

	static int comparMember(const void *lhs, const void *rhs, void *state) {
		if (lhs == rhs)
			return 0;

		const member_t *pMemberL = (const member_t *) lhs;
		const member_t *pMemberR = (const member_t *) rhs;
		context_t *pApp = (context_t *) state;

		if (pMemberL->sid != pMemberR->sid)
			return pMemberL->sid - pMemberR->sid;

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.decodeFast(pMemberL->name);
		treeR.decodeFast(pMemberR->name);

		// compare
		return treeL.compare(treeL.root, treeR, treeR.root);

	}

	/**
	 * @date 2020-04-02 21:52:34
	 */
	void loadSafe(database_t &store, const database_t &db, bool unsafeOnly) {
		if (opt_verbose >= VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Loading members+signatures\n", timeAsString());

		generatorTree_t tree(*this);

		// reset progress
		this->setupSpeed(db.numMember + db.numSignature);
		this->tick = 0;

		// load signatures
		for (uint32_t iSid = 1; iSid < db.numSignature; iSid++) {
			const signature_t *pOldSignature = db.signatures + iSid;

			// add signature
			uint32_t sid = store.addSignature(pOldSignature->name);
			signature_t *pNewSignature = store.signatures + sid;
			assert(sid == iSid);

			// with full record
			::memcpy(pNewSignature, pOldSignature, sizeof(*pNewSignature));

			// add imprint for all signatures or (requested) unsafe only
			if (!unsafeOnly || (pNewSignature->flags & signature_t::SIGMASK_UNSAFE)) {
				uint32_t sid = 0;
				uint32_t tid = 0;

				tree.decodeFast(pNewSignature->name);

				if (!store.lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid))
					store.addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);
			}

			// count
			if (pNewSignature->flags & signature_t::SIGMASK_UNSAFE)
				numUnsafe++;

			this->progress++;
		}

		// load members
		for (uint32_t iMid = 1; iMid < db.numMember; iMid++) {
			const member_t *pOldmember = db.members + iMid;

			// add member
			uint32_t mid = store.addMember(pOldmember->name);
			member_t *pNewMember = store.members + mid;
			assert(mid == iMid);

			// with full record
			::memcpy(pNewMember, pOldmember, sizeof(*pNewMember));

			// index
			uint32_t ix = store.lookupMember(pNewMember->name);
			assert(store.memberIndex[ix] == 0);
			store.memberIndex[ix] = mid;

			this->progress++;
		}

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numImprint=%u numSignature=%u numMember=%u  numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipIdentical=%u\n",
			        timeAsString(), store.numImprint, store.numSignature, store.numMember, numUnsafe,
			        skipDuplicate, skipSize, skipUnsafe, skipIdentical);
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

		generatorTree_t generator(*this);

		// apply settings
		{
			// get metrics
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF, arg_numNodes);
			assert(pMetrics);

			// apply settings for `--job`
			if (this->opt_jobLast) {
				// split progress into chunks
				uint64_t jobSize = pMetrics->numProgress / this->opt_jobLast;
				if (jobSize == 0)
					jobSize = 1;
				generator.windowLo = jobSize * (this->opt_jobId - 1);
				generator.windowHi = jobSize * this->opt_jobId;

				// limits
				if (opt_jobId == opt_jobLast || generator.windowHi > pMetrics->numProgress)
					generator.windowHi = pMetrics->numProgress;
			}

			// apply settings for `--window`
			if (this->opt_windowLo)
				generator.windowLo = this->opt_windowLo;
			if (this->opt_windowHi)
				generator.windowHi = this->opt_windowHi;

			// limit window
			if (this->opt_windowLo != 0 && this->opt_windowHi == 0)
				generator.windowHi = pMetrics->numProgress;
			if (this->opt_windowHi > pMetrics->numProgress)
				generator.windowHi = pMetrics->numProgress;

			// apply restart data
			unsigned ofs = 0;
			if (this->arg_numNodes < tinyTree_t::TINYTREE_MAXNODES)
				ofs = restartIndex[this->arg_numNodes][(this->opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0];
			if (ofs)
				generator.pRestartData = restartData + ofs;
		}

		if (generator.windowLo || generator.windowHi)
			fprintf(stderr, "[%s] Job window: %lu-%lu\n", context_t::timeAsString(), generator.windowLo, generator.windowHi);

		/*
		 * Load additional candidates from file. Intended for loading unsafe replacements from `genunsafe`
		 */

		if (this->opt_append) {
			if (opt_verbose >= VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Appending additional candidates\n", timeAsString());

			FILE *f = fopen(this->opt_append, "r");
			if (f == NULL)
				fatal("{\"error\":\"fopen() failed\",\"where\":\"%s\",\"name\":\"%s\",\"reason\":\"%m\"}\n",
				      __FUNCTION__, this->opt_append);

			// reset progress
			this->setupSpeed(0);
			this->tick = 0;
			generator.restartTick = 0;

			char name[64];
			uint32_t numPlaceholder;

			// <candidateName> <numPlaceholder>
			while (fscanf(f, "%s %u\n", name, &numPlaceholder) == 2) {
				generator.decodeFast(name);

				numPlaceholder = 0;
				unsigned seen = 0;
				for (char *p = name; *p; p++) {
					if (islower(*p))
						if (~seen & (1 << (*p - 'a'))) {
							seen |= 1 << (*p - 'a');
							numPlaceholder++;
						}
				}
				foundTreeMember(generator, name, numPlaceholder);
				progress++;
			}

			fclose(f);

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			if (this->opt_verbose >= this->VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] numImprint=%u numSignature=%u numMember=%u  numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipIdentical=%u\n",
				        timeAsString(), pStore->numImprint, pStore->numSignature, pStore->numMember, numUnsafe,
				        skipDuplicate, skipSize, skipUnsafe, skipIdentical);
		}

		/*
		 * create generator and candidate members
		 */

		for (unsigned numNode = arg_numNodes; numNode <= arg_numNodes; numNode++) {
			// find metrics for setting
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF, numNode);
			unsigned endpointsLeft = numNode * 2 + 1;

			// clear tree
			generator.clearGenerator();

			// reset progress
			this->setupSpeed(pMetrics ? pMetrics->numProgress : 0);
			this->tick = 0;
			generator.restartTick = 0;

			/*
			 * Generate members
			 */
			if (this->opt_verbose >= this->VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Generating candidates for %dn%d%s\n", timeAsString(), numNode, MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF ? "-QnTF" : "");

			if (numNode == 0) {
				generator.root = 0; // "0"
				foundTreeMember(generator, "0", 0);
				generator.root = 1; // "a"
				foundTreeMember(generator, "a", 1);
			} else {
				generator.generateTrees(endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &genmemberContext_t::foundTreeMember);
			}

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			if (this->progress != this->progressHi) {
				printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%d}\n",
				       __FUNCTION__, this->progress, this->progressHi, numNode);
			}
		}

		fprintf(stderr, "[%s] numImprint=%u numSignature=%u numMember=%u  numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipIdentical=%u\n",
		        timeAsString(), pStore->numImprint, pStore->numSignature, pStore->numMember, numUnsafe,
		        skipDuplicate, skipSize, skipUnsafe, skipIdentical);

		if (this->progress != this->progressHi) {
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%d}\n",
			       __FUNCTION__, this->progress, this->progressHi, arg_numNodes);
		}

		/*
		 * Load additional candidates from file. Intended for loading unsafe replacements from `genunsafe`
		 */

		if (this->opt_append) {
			if (opt_verbose >= VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Appending additional candidates\n", timeAsString());

			FILE *f = fopen(this->opt_append, "r");
			if (f == NULL)
				fatal("{\"error\":\"fopen() failed\",\"where\":\"%s\",\"name\":\"%s\",\"reason\":\"%m\"}\n",
				      __FUNCTION__, this->opt_append);

			// reset progress
			this->setupSpeed(0);
			this->tick = 0;
			generator.restartTick = 0;

			char name[64];
			uint32_t numPlaceholder;

			// <candidateName> <numPlaceholder>
			while (fscanf(f, "%s %u\n", name, &numPlaceholder) == 2) {
				generator.decodeFast(name);

				numPlaceholder = 0;
				unsigned seen = 0;
				for (char *p = name; *p; p++) {
					if (islower(*p))
						if (~seen & (1 << (*p - 'a'))) {
							seen |= 1 << (*p - 'a');
							numPlaceholder++;
						}
				}
				foundTreeMember(generator, name, numPlaceholder);
				progress++;
			}

			fclose(f);

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			if (this->opt_verbose >= this->VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] numImprint=%u numSignature=%u numMember=%u  numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipIdentical=%u\n",
				        timeAsString(), pStore->numImprint, pStore->numSignature, pStore->numMember, numUnsafe,
				        skipDuplicate, skipSize, skipUnsafe, skipIdentical);
		}

		/*
		 * Compacting
		 *
		 * Members may contain (unsafe) members that got orphaned when their group accepted a safe member.
		 */
		{
			if (this->opt_verbose >= this->VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Compacting\n", timeAsString());

			uint32_t oldNumMember = pStore->numMember;

			// reset member index and friends
			::memset(pStore->memberIndex, 0, pStore->memberIndexSize * sizeof(*pStore->memberIndex));
			for (uint32_t iSid = 0; iSid < pStore->numSignature; iSid++)
				pStore->signatures[iSid].firstMember = 0;
			pStore->numMember = 1;
			skipDuplicate = skipSize = skipUnsafe = skipIdentical = 0;

			// reload everything
			this->setupSpeed(oldNumMember);
			this->tick = 0;
			generator.restartTick = 0;

			for (uint32_t iMid = 1; iMid < oldNumMember; iMid++) {
				member_t *pMember = pStore->members + iMid;
				if (pMember->sid) {
					// copy member as `foundTreeMember()` will erase its contents
					member_t old = *pMember;

					generator.decodeFast(old.name);
					foundTreeMember(generator, old.name, old.numPlaceholder);
				}
				this->progress++;
			}

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			numUnsafe = 0;
			for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
				if (pStore->signatures[iSid].flags & signature_t::SIGMASK_UNSAFE)
					numUnsafe++;
			}

			if (this->opt_verbose >= this->VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] numImprint=%u numSignature=%u numMember=%u  numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipIdentical=%u\n",
				        timeAsString(), pStore->numImprint, pStore->numSignature, pStore->numMember, numUnsafe,
				        skipDuplicate, skipSize, skipUnsafe, skipIdentical);
		}

		{
			unsigned numEmpty = 0;
			numUnsafe = 0;
			for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++)
				if (pStore->signatures[iSid].firstMember == 0) {
					numEmpty++;
				} else if (pStore->signatures[iSid].flags & signature_t::SIGMASK_UNSAFE) {
					numUnsafe++;
				}

			if (numEmpty || numUnsafe)
				fprintf(stderr, "[%s] WARNING: %u empty and %u unsafe signature groups\n", timeAsString(), numEmpty, numUnsafe);
		}

		if (opt_text == 3) {
			for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
				for (uint32_t iMid = pStore->signatures[iSid].firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
					member_t *pMember = pStore->members + iMid;

					printf("%u:%s\t", iMid, pMember->name);
					printf("%u\t", pMember->sid);

					printf("%u:%s\t%u\t", pMember->Qmid, pStore->members[pMember->Qmid].name, pMember->Qsid);
					if (pMember->Tsid & IBIT)
						printf("%u:%s\t%u\t", pMember->Tmid, pStore->members[pMember->Tmid].name, pMember->Tsid & ~IBIT);
					else
						printf("%u:%s\t%u\t", pMember->Tmid, pStore->members[pMember->Tmid].name, pMember->Tsid);
					printf("%u:%s\t%u\t", pMember->Fmid, pStore->members[pMember->Fmid].name, pMember->Fsid);

					for (unsigned i = 0; i < member_t::MAXHEAD; i++)
						printf("%u:%s\t", pMember->heads[i], pStore->members[pMember->heads[i]].name);

					if (pMember->flags & signature_t::SIGMASK_UNSAFE)
						printf("U");
					printf("\n");
				}
			}
		}
		if (opt_text == 4) {
			/*
			 * Display non-zero members
			 *
			 * <name> <numPlaceholder>
			 *
			 * During debugging the output can be fastloaded :
			 *      `./genmember output.db transform.db 0  --maxsignature=999999 --maximprint=200000000 --append=append.txt`
			 */
			for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
				member_t *pMember = pStore->members + iMid;

				if (pMember->sid)
					printf("%s\t%u\n", pMember->name, pMember->numPlaceholder);
			}
		}

		/*
		 * List result
		 */
		if (opt_text == 1) {
			for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
				const signature_t *pSignature = pStore->signatures + iSid;
				printf("%u\t%s\t%u\t%u\t%u\t%u\n", iSid, pSignature->name, pSignature->size, pSignature->numEndpoint, pSignature->numPlaceholder, pSignature->numBackRef);
			}
		}

		/*
		 * Done
		 */
		if (opt_verbose >= VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] {\"numSlot\":%u,\"qntf\":%u,\"interleave\":%u,\"numNode\":%u,\"numMember\":%u,\"numSignature\":%u,\"numImprint\":%u,\"numUnsafe\":%u}\n",
			        this->timeAsString(), MAXSLOTS, (this->opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0, pStore->interleave, arg_numNodes, pStore->numImprint, pStore->numSignature, pStore->numMember, numUnsafe);

	}

};

/**
 * @date 2020-03-22 00:29:34
 *
 * Selftest wrapper
 *
 * @typedef {object}
 */
struct genmemberSelftest_t : genmemberContext_t {

	/// @var {number} --selftest, perform a selftest
	unsigned opt_selftest;
	/// @var {string[]} tree notation for `progress` points
	char **selftestWindowResults;

	/**
	 * Constructor
	 */
	genmemberSelftest_t() {
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
	void foundTreeWindowCreate(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			if (progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), tree.windowLo * 100.0 / progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %lu", timeAsString(), tree.windowLo);
		}

		assert(this->progress < 2000000);

		// assert entry is unique
		if (selftestWindowResults[this->progress] != NULL) {
			printf("{\"error\":\"entry not unique\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%lu}\n",
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
	void foundTreeWindowVerify(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			if (progressHi)
				fprintf(stderr, "\r\e[K[%s] %.5f%%", timeAsString(), tree.windowLo * 100.0 / progressHi);
			else
				fprintf(stderr, "\r\e[K[%s] %lu", timeAsString(), tree.windowLo);
		}

		assert(this->progress < 2000000);

		// assert entry is present
		if (selftestWindowResults[this->progress] == NULL) {
			printf("{\"error\":\"missing\",\"where\":\"%s\",\"expected\":\"%s\",\"progress\":%lu}\n",
			       __FUNCTION__, pName, this->progress);
			exit(1);
		}

		// compare
		if (::strcmp(pName, selftestWindowResults[this->progress]) != 0) {
			printf("{\"error\":\"entry missmatch\",\"where\":\"%s\",\"encountered\":\"%s\",\"expected\":\"%s\",\"progress\":%lu}\n",
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

			generator.generateTrees(endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &genmemberSelftest_t::foundTreeWindowCreate);
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

			generator.generateTrees(endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &genmemberSelftest_t::foundTreeWindowVerify);
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
	void foundTreeMetrics(const generatorTree_t &tree, const char *pName, unsigned numPlaceholder) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			int perSecond = this->updateSpeed();

			if (perSecond == 0 || progress > progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numSignature=%u numImprint=%u",
				        timeAsString(), progress, perSecond, pStore->numSignature, pStore->numImprint);
			} else {
				int eta = (int) ((progressHi - progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numSignature=%u numImprint=%u",
				        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, pStore->numSignature, pStore->numImprint);
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
			foundTreeMetrics(generator, "0", 0);
			generator.root = 1; // "a"
			foundTreeMetrics(generator, "a", 1);

			// regulars
			unsigned endpointsLeft = pRound->numNode * 2 + 1;
			generator.generateTrees(endpointsLeft, 0, 0, this, (generatorTree_t::generateTreeCallback_t) &genmemberSelftest_t::foundTreeMetrics);

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

			fprintf(stderr, "[%s] numSlot=%u qntf=%u interleave=%-4u numNode=%u numSignature=%u numImprint=%u speed=%.3fM/s storage=%.3fGb\n",
			        this->timeAsString(), MAXSLOTS, pRound->qntf, pRound->interleave, pRound->numNode, pStore->numSignature, pStore->numImprint, speed, storage);

			if (this->progress != this->progressHi) {
				printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%d}\n",
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

				fprintf(stderr, "[%d %d %.1f %lu %lu %.5f]", pStore->numImprint, pStore->imprintIndexSize, iRatio / 10.0, this->cntHash, this->cntCompare, (double) this->cntCompare / this->cntHash);

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
 * @global {genmemberContext_t} Application
 */
genmemberSelftest_t app;

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
void usage(char *const *argv, bool verbose, const genmemberContext_t *args) {
	fprintf(stderr, "usage: %s <output.db> <input.db> <numnode> -- Add signatures of given node size\n", argv[0]);
	fprintf(stderr, "       %s --metrics <input.db>             -- Collect metrics\n", argv[0]);
	fprintf(stderr, "       %s --selftest <input.db>            -- Test prerequisites\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --append=<file>           Append extra members from file [default=%s]\n", app.opt_append ? app.opt_append : "");
		fprintf(stderr, "\t   --force                   Force overwriting of database if already exists\n");
		fprintf(stderr, "\t-h --help                    This list\n");
		fprintf(stderr, "\t   --imprintindex=<number>   Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>     Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --job=<id>,<last>         Job id of batch [default=%u,%u]\n", app.opt_jobId, app.opt_jobLast);
		fprintf(stderr, "\t   --keep                    Do not delete output database in case of errors\n");
		fprintf(stderr, "\t   --maximprint=<number>     Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxmember=<number>      Maximum number of members [default=%u]\n", app.opt_maxMember);
		fprintf(stderr, "\t   --maxsignature=<number>   Maximum number of signatures [default=%u]\n", app.opt_maxSignature);
		fprintf(stderr, "\t   --memberindex=<number>    Size of member index [default=%u]\n", app.opt_memberIndexSize);
		fprintf(stderr, "\t   --metrics                 Collect metrics\n");
		fprintf(stderr, "\t   --[no-]qntf               Enable QnTF-only mode [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_QNTF) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]paranoid           Enable expensive assertions [default=%s]\n", (app.opt_flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                   Say more\n");
		fprintf(stderr, "\t   --ratio=<number>          Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --selftest                Validate prerequisites\n");
		fprintf(stderr, "\t   --signatureindex=<number> Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --test                    Run without output\n");
		fprintf(stderr, "\t   --text                    Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>         Interval timer for verbose updates [default=%u]\n", args->opt_timer);
		fprintf(stderr, "\t   --unsafe                  Collect unsafe replacements.\n");
		fprintf(stderr, "\t-v --verbose                 Say less\n");
		fprintf(stderr, "\t   --windowhi=<number>       Upper end restart window [default=%lu]\n", args->opt_windowHi);
		fprintf(stderr, "\t   --windowlo=<number>       Lower end restart window [default=%lu]\n", args->opt_windowLo);
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
			LO_APPEND = 1,
			LO_DEBUG,
			LO_FORCE,
			LO_IMPRINTINDEX,
			LO_INTERLEAVE,
			LO_JOB,
			LO_KEEP,
			LO_MAXIMPRINT,
			LO_MAXMEMBER,
			LO_MAXSIGNATURE,
			LO_MEMBERINDEX,
			LO_METRICS,
			LO_NOPARANOID,
			LO_NOQNTF,
			LO_PARANOID,
			LO_QNTF,
			LO_RATIO,
			LO_SELFTEST,
			LO_SIGNATUREINDEX,
			LO_TEST,
			LO_TEXT,
			LO_TIMER,
			LO_UNSAFE,
			LO_WINDOWHI,
			LO_WINDOWLO,
			// short opts
			LO_HELP = 'h',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"append",         1, 0, LO_APPEND},
			{"debug",          1, 0, LO_DEBUG},
			{"force",          0, 0, LO_FORCE},
			{"help",           0, 0, LO_HELP},
			{"imprintindex",   1, 0, LO_IMPRINTINDEX},
			{"interleave",     1, 0, LO_INTERLEAVE},
			{"job",            1, 0, LO_JOB},
			{"keep",           0, 0, LO_KEEP},
			{"maximprint",     1, 0, LO_MAXIMPRINT},
			{"maxmember",      1, 0, LO_MAXMEMBER},
			{"maxsignature",   1, 0, LO_MAXSIGNATURE},
			{"memberindex",    0, 0, LO_MEMBERINDEX},
			{"metrics",        0, 0, LO_METRICS},
			{"no-paranoid",    0, 0, LO_NOPARANOID},
			{"no-qntf",        0, 0, LO_NOQNTF},
			{"paranoid",       0, 0, LO_PARANOID},
			{"qntf",           0, 0, LO_QNTF},
			{"quiet",          2, 0, LO_QUIET},
			{"ratio",          1, 0, LO_RATIO},
			{"selftest",       0, 0, LO_SELFTEST},
			{"signatureindex", 1, 0, LO_SIGNATUREINDEX},
			{"test",           0, 0, LO_TEST},
			{"text",           2, 0, LO_TEXT},
			{"timer",          1, 0, LO_TIMER},
			{"unsafe",         0, 0, LO_UNSAFE},
			{"verbose",        2, 0, LO_VERBOSE},
			{"windowhi",       1, 0, LO_WINDOWHI},
			{"windowlo",       1, 0, LO_WINDOWLO},
			//
			{NULL,             0, 0, 0}
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
			case LO_APPEND:
				app.opt_append = optarg;
				break;
			case LO_DEBUG:
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_FORCE:
				app.opt_force++;
				break;
			case LO_HELP:
				usage(argv, true, &app);
				exit(0);
			case LO_IMPRINTINDEX:
				app.opt_imprintIndexSize = app.nextPrime((uint32_t) strtoul(optarg, NULL, 0));
				break;
			case LO_INTERLEAVE:
				app.opt_interleave = (unsigned) strtoul(optarg, NULL, 0);
				if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
					app.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
				break;
			case LO_JOB:
				if (sscanf(optarg, "%u,%u", &app.opt_jobId, &app.opt_jobLast) != 2) {
					usage(argv, true, &app);
					exit(1);
				}
				if (app.opt_jobId == 0 || app.opt_jobLast == 0) {
					fprintf(stderr, "Job id/last must be non-zero\n");
					exit(1);
				}
				if (app.opt_jobId > app.opt_jobLast) {
					fprintf(stderr, "Job id exceeds last\n");
					exit(1);
				}
				break;
			case LO_KEEP:
				app.opt_keep++;
				break;
			case LO_MAXIMPRINT:
				app.opt_maxImprint = (uint32_t) strtoul(optarg, NULL, 0);
				break;
			case LO_MAXMEMBER:
				app.opt_maxMember = (uint32_t) strtoul(optarg, NULL, 0);
				break;
			case LO_MAXSIGNATURE:
				app.opt_maxSignature = (uint32_t) strtoul(optarg, NULL, 0);
				break;
			case LO_MEMBERINDEX:
				app.opt_memberIndexSize = app.nextPrime((uint32_t) strtoul(optarg, NULL, 0));
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
				app.opt_test++;
				break;
			case LO_SIGNATUREINDEX:
				app.opt_signatureIndexSize = app.nextPrime((uint32_t) strtoul(optarg, NULL, 0));
				break;
			case LO_TEST:
				app.opt_test++;
				break;
			case LO_TEXT:
				app.opt_text = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_text + 1;
				break;
			case LO_TIMER:
				app.opt_timer = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_UNSAFE:
				app.opt_unsafe++;
				break;
			case LO_VERBOSE:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_verbose + 1;
				break;
			case LO_WINDOWHI:
				app.opt_windowHi = strtoull(optarg, NULL, 0);
				break;
			case LO_WINDOWLO:
				app.opt_windowLo = strtoull(optarg, NULL, 0);
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

	if (app.opt_selftest || app.opt_metrics) {
		// selftest or metrics mode
		if (argc - optind >= 1) {
			app.arg_inputDatabase = argv[optind++];
		} else {
			usage(argv, false, &app);
			exit(1);
		}
	} else {
		// regular mode
		if (argc - optind >= 3) {
			app.arg_outputDatabase = argv[optind++];
			app.arg_inputDatabase = argv[optind++];
			app.arg_numNodes = (uint32_t) strtoul(argv[optind++], NULL, 0);
		} else {
			usage(argv, false, &app);
			exit(1);
		}
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

	if (app.opt_append) {
		struct stat sbuf;

		if (stat(app.opt_append, &sbuf)) {
			fprintf(stderr, "%s does not exist\n", app.opt_append);
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
				fprintf(stderr, "[%s] Set limits to ratio=%.1f maxImprint=%u maxSignature=%u\n", app.timeAsString(), app.opt_ratio, app.opt_maxImprint, app.opt_maxSignature);
		}

		if (app.opt_unsafe) {
			if (app.arg_numNodes != 5)
				fprintf(stderr, "WARNING: --unsafe is intended for 5n9\n");

			// 5n9 has incomplete metrics. Hardcoded settings.
			app.opt_interleave = 3024;
			app.opt_maxMember = 6500000;
			app.opt_maxImprint = 3000000;
			app.opt_maxSignature = 800000;

			if (app.opt_verbose >= app.VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Set limits to interleave=%u maxImprint=%u maxSignature=%u maxMember=%u\n", app.timeAsString(), app.opt_interleave, app.opt_maxImprint, app.opt_maxSignature, app.opt_maxMember);
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

		{
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, app.opt_flags & app.MAGICMASK_QNTF, app.arg_numNodes);
			store.maxMember = pMetrics ? pMetrics->numCandidate : 0;
			store.maxMember = 9999999;
			store.memberIndexSize = app.nextPrime(store.maxMember * app.opt_ratio);
		}

		if (store.interleave == 0 || store.interleaveStep == 0)
			app.fatal("no preset for --interleave\n");
		if (store.maxImprint == 0 || store.imprintIndexSize == 0)
			app.fatal("no preset for --maximprint\n");
		if (store.maxSignature == 0 || store.signatureIndexSize == 0)
			app.fatal("no preset for --maxsignature\n");
		if (store.maxMember == 0 || store.memberIndexSize == 0)
			app.fatal("no preset for --maxmember\n");
	}

	// create new sections
	if (app.opt_verbose >= app.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] Store create: maxImprint=%u maxSignature=%u maxMember=%u\n", app.timeAsString(), store.maxImprint, store.maxSignature, store.maxMember);

	store.create();

	// inherit from existing
	store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_TRANSFORM);

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) app.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) app.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

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
	 * Load original members
	 */
	app.loadSafe(store, db, app.opt_unsafe);

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
	 * Invoke main entrypoint of application context
	 */
	app.main(&store);

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
		store.jsonInfo(jResult);
		printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		if (!isatty(1))
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}
#endif

	return 0;
}
