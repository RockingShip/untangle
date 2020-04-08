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
 *
 * @date 2020-04-06 22:55:07
 *
 * `genmember` collects raw members.
 * Invocations are made with increasing nodeSize to find new members or safe replacements.
 * Once a group is safe (after invocation) new members will be rejected, this makes that only unsafe groups need detection.
 * Multi-pass is possible by focusing on a a smaller number of signature groups. This allows for extreme high speeds (interleave) at a cost of storage.
 * `genmember` actually needs two modes: preperation of an imprint index (done by master) and collecting (done by workers).
 * Workers can take advantage of the read-only imprint index in shared memory (`mmap`)
 *
 * Basically, `genmember` collects constructing components.
 * Only after all groups are safe can selecting occur.
 *
 * - All single member groups lock components (tails) and providers (heads)
 * - Groups with locked heads and tails become locked themselves.
 * Speculating:
 * - unsafe members can be grouped by component sid (resulting in a single "best `compare()`" member
 * - safe members can be grouped by component mid (resulting in a single "best `compare()`" member
 * - unsafe groups with locked members but unsafe providers can promote the providers (what to do when multiple)
 * - safe groups with unsafe members can release heads/tails allowing their refcount to drop to zero and be removed.
 *
 * Intended usage:
 *
 * - prepare new database by creating imprints for safe members.
 *   It is safe to use extreme high interleave (5040, 15120, 40320 and 60480)
 *   The higher the faster but less groups to detect.
 *
 * - After prepare let workers collect members using `--text=3` which collects on the fly.
 *
 * - After all workers complete, join all worker results and create dataset, use `--text=1`
 *
 * - repeat preparing and collecting until collecting has depleted
 *
 * - increase nodeSize by one and repeat.
 *
 * NOTE: don't be smart in rejecting members until final data-analysis is complete.
 *       This is a new feature for v2 and uncharted territory
 *
 * At this moment calculating and collecting:
 * `restartData[]` for `7n9-QnTF`. This is a premier!
 * signature group members for 6n9-QnTF. This is also premier.
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
	/// @var {database_t} - Input database
	database_t *pInputDb;
	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for referse transforms
	footprint_t *pEvalRev;

	uint32_t skipDuplicate;
	uint32_t skipSize;
	uint32_t skipUnsafe;
	uint32_t numUnsafe;
	uint32_t numEmpty;
	uint32_t freeMemberRoot;

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
		opt_interleave = 0;
		opt_jobId = 0;
		opt_jobLast = 0;
		opt_keep = 0;
		opt_maxImprint = 0;
		opt_maxMember = 0;
		opt_maxSignature = 0;
		opt_memberIndexSize = 0;
		opt_ratio = METRICS_DEFAULT_RATIO / 10.0;
		opt_signatureIndexSize = 0;
		opt_test = 0;
		opt_text = 0;
		opt_unsafe = 0;

		pStore = NULL;
		pInputDb = NULL;
		pEvalFwd = NULL;
		pEvalRev = NULL;

		skipDuplicate = 0;
		skipSize = 0;
		skipUnsafe = 0;
		numUnsafe = 0;
		freeMemberRoot = 0;
	}

	/**
	 * @date 2020-04-04 22:00:59
	 *
	 * Determine heads and tails and lookup their `memberID` and `signatureId`
	 *
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
	 *
	 * @param {member_t} pMember - Member to process
	 * @param {generatorTree_t} treeR - candidate tree
	 */
	void findHeadTail(member_t *pMember, const generatorTree_t &treeR) {

		// safe until proven otherwise
		pMember->flags &= ~signature_t::SIGMASK_UNSAFE;

		// special cases
		if (treeR.root == 0) {
			assert(::strcmp(pMember->name, "0") == 0); // must be reserved name
			assert(pMember - pStore->members == 1); // must be reserved entry

			pMember->Qmid = pMember->Tmid = pMember->Fmid = 1;
			pMember->Qsid = pMember->Tsid = pMember->Fsid = 1;
			return;
		}
		if (treeR.root == 1) {
			assert(::strcmp(pMember->name, "a") == 0); // must be reserved name
			assert(pMember - pStore->members == 2); // must be reserved entry

			pMember->Qmid = pMember->Tmid = pMember->Fmid = 2;
			pMember->Qsid = pMember->Tsid = pMember->Fsid = 2;
			return;
		}

		assert(treeR.root >= tinyTree_t::TINYTREE_NSTART);

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
		{
			char skin[MAXSLOTS + 1];

			uint32_t Q = treeR.N[treeR.root].Q;
			{
				const char *pComponentName = treeR.encode(Q, skin);
				uint32_t ix = pStore->lookupMember(pComponentName);

				pMember->Qmid = pStore->memberIndex[ix];
				pMember->Qsid = pStore->members[pMember->Qmid].sid;

				// member is unsafe if component not found or unsafe
				if (pMember->Qmid == 0 || pMember->Qsid == 0 || (pStore->members[pMember->Qmid].flags & signature_t::SIGMASK_UNSAFE))
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
			}

			uint32_t To = treeR.N[treeR.root].T & ~IBIT;
			{
				const char *pComponentName = treeR.encode(To, skin);
				uint32_t ix = pStore->lookupMember(pComponentName);

				pMember->Tmid = pStore->memberIndex[ix];
				pMember->Tsid = pStore->members[pMember->Tmid].sid ^ (treeR.N[treeR.root].T & IBIT);

				// member is unsafe if component not found or unsafe
				if (pMember->Tmid == 0 || (pMember->Tsid & ~IBIT) == 0 || (pStore->members[pMember->Tmid].flags & signature_t::SIGMASK_UNSAFE))
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
			}

			uint32_t F = treeR.N[treeR.root].F;
			{
				const char *pComponentName = treeR.encode(F, skin);
				uint32_t ix = pStore->lookupMember(pComponentName);

				pMember->Fmid = pStore->memberIndex[ix];
				pMember->Fsid = pStore->members[pMember->Fmid].sid;

				// member is unsafe if component not found or unsafe
				if (pMember->Fmid == 0 || pMember->Fsid == 0 || (pStore->members[pMember->Fmid].flags & signature_t::SIGMASK_UNSAFE))
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
				uint32_t ix = pStore->lookupMember(name);
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
	 * @param {generatorTree_t} treeR - candidate tree
	 * @param {string} pNameR - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints in tree
	 */
	void foundTreeMember(const generatorTree_t &treeR, const char *pNameR, unsigned numPlaceholder) {
		if (opt_verbose >= VERBOSE_TICK && tick) {
			tick = 0;
			int perSecond = this->updateSpeed();

			if (perSecond == 0 || progress > progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numImprint=%u(%.0f%%) numSignature=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u",
				        timeAsString(), progress, perSecond,
				        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe,
				        skipDuplicate, skipSize, skipUnsafe);
			} else {
				int eta = (int) ((treeR.windowHi - progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numImprint=%u(%.0f%%) numSignature=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u",
				        timeAsString(), progress, perSecond, (progress - treeR.windowLo) * 100.0 / (treeR.windowHi - treeR.windowLo), etaH, etaM, etaS,
				        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe,
				        skipDuplicate, skipSize, skipUnsafe);
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

		/*
		 * @date 2020-04-07 23:43:19
		 *
		 * Lookup against input imprints as `pStore` may be undefined
		 */
		if (!pInputDb->lookupImprintAssociative(&treeR, pEvalFwd, pEvalRev, &sid, &tid))
			return;

		signature_t *pSignature = pStore->signatures + sid;

		// only if group is safe reject is structure is too large
		if ((~pSignature->flags & signature_t::SIGMASK_UNSAFE) && treeR.count - tinyTree_t::TINYTREE_NSTART > pSignature->size) {
			skipSize++;
			return;
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

		/*
		 * Allocate member
		 */

		member_t *pMember;

		uint32_t mid = freeMemberRoot;
		if (mid) {
			pMember = pStore->members + mid;
			freeMemberRoot = pMember->nextMember; // pop from free list
			::strcpy(pMember->name, pNameR); // populate with name
		} else {
			mid = pStore->addMember(pNameR); // allocate new member
			pMember = pStore->members + mid;
		}
		pStore->memberIndex[ix] = mid;

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
		 * handle heads/tails
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
			findHeadTail(pMember, treeR);
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
					// zero orphan so it won't be found by `lookupMember()`
					::memset(pMember, 0, sizeof(*pMember));
					// push member on the freelist
					pMember->nextMember = freeMemberRoot;
					freeMemberRoot = pMember - pStore->members;

					skipUnsafe++;
					return;
				}
				assert(treeR.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);
			} else {
				/*
				 * group is unsafe, candidate is safe.
				 * If candidate is same size then drop all existing unsafe group members
				 * If candidate is larger then keep all smaller unsafe members for later optimisations
				 */

				if (pSignature->firstMember && treeR.count - tinyTree_t::TINYTREE_NSTART == pSignature->size) {
					/*
					 * empty group
					 *
					 * @date 2020-04-05 02:21:42
					 *
					 * For `5n9-QnTF` it turns out that the chance of finding safe replacements is rare.
					 * And you need to collect all non-safe members if the group is unsafe.
					 * Orphaning them depletes resources too fast.
					 *
					 * Reuse `members[]`.
					 * Field `nextMember` is perfect for that.
					 */
					while (pSignature->firstMember) {
						// remove all references to
						for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
							member_t *p = pStore->members + iMid;

							if (p->Qmid == pSignature->firstMember) {
								assert(p->flags & signature_t::SIGMASK_UNSAFE);
								p->Qmid = 0;
							}
							if (p->Tmid == pSignature->firstMember) {
								assert(p->flags & signature_t::SIGMASK_UNSAFE);
								p->Tmid = 0;
							}
							if (p->Fmid == pSignature->firstMember) {
								assert(p->flags & signature_t::SIGMASK_UNSAFE);
								p->Fmid = 0;
							}
						}

						// get member
						member_t *p = pStore->members + pSignature->firstMember;
						// remove from list
						pSignature->firstMember = p->nextMember;
						// zero orphan so it won't be found by `lookupMember()`
						::memset(p, 0, sizeof(*p));
						// add to free list
						p->nextMember = freeMemberRoot;
						freeMemberRoot = p - pStore->members;
					}

					numEmpty++; // group has become empty
				}

				// mark group as safe
				pSignature->flags &= ~signature_t::SIGMASK_UNSAFE;
				pSignature->size = treeR.count - tinyTree_t::TINYTREE_NSTART;

				/*
				 * Output first safe member of a signature group
				 */

				if (opt_text == 4)
					printf("%u\t%s\t%u\t%u\t%u\t%u\n", pMember->sid, pMember->name, treeR.count - tinyTree_t::TINYTREE_NSTART, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);

				// one unsafe group less
				numUnsafe--;
			}
		} else {
			if (pMember->flags & signature_t::SIGMASK_UNSAFE) {
				// group is safe, candidate not. Drop candidate

				// zero orphan so it won't be found by `lookupMember()`
				::memset(pMember, 0, sizeof(*pMember));
				// push member on the freelist
				pMember->nextMember = freeMemberRoot;
				freeMemberRoot = pMember - pStore->members;

				skipUnsafe++;
				return;
			} else {
				// group/candidate both safe
				assert(treeR.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);
			}
		}

		assert(pMember->name[0]);

		/*
		 * Output candidate members on-the-fly
		 */
		if (opt_text == 3)
			printf("%u\t%s\t%u\t%u\t%u\t%u\n", pMember->sid, pMember->name, treeR.count - tinyTree_t::TINYTREE_NSTART, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);

		if (pSignature->firstMember == 0)
			numEmpty--; // group now has first member

		pMember->nextMember = pSignature->firstMember;
		pSignature->firstMember = mid;
	}

	/**
	 * @date 2020-04-05 21:07:14
	 *
	 * Compare function for `qsort_r`
	 *
	 * @param {member_t} lhs - left hand side member
	 * @param {member_t} rhs - right hand side member
	 * @param {context_t} state - I/O contect needed to create trees
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int /*__attribute__((optimize("O0")))*/ comparMember(const void *lhs, const void *rhs, void *state) {
		if (lhs == rhs)
			return 0;

		const member_t *pMemberL = (const member_t *) lhs;
		const member_t *pMemberR = (const member_t *) rhs;
		context_t *pApp = (context_t *) state;

		// test for empties (they should gather towards the end of `members[]`)
		if (pMemberL->sid == 0 && pMemberR->sid == 0)
			return 0;
		if (pMemberL->sid == 0)
			return +1;
		if (pMemberR->sid == 0)
			return -1;

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.decodeFast(pMemberL->name);
		treeR.decodeFast(pMemberR->name);

		/*
		 * Compare
		 */

		int cmp = 0;

		// Test for prime goal: reducing number of nodes
		cmp = treeL.count - treeR.count;
		if (cmp)
			return cmp;

		// Test for secondary goal: reduce number of unique endpoints, thus connections
		cmp = pMemberL->numPlaceholder - pMemberR->numPlaceholder;
		if (cmp)
			return cmp;

		// Test for preferred display selection: least number of endpoints
		cmp = pMemberL->numEndpoint - pMemberR->numEndpoint;
		if (cmp)
			return cmp;

		// Test for preferred display selection: least number of back-references
		cmp = pMemberL->numBackRef - pMemberR->numBackRef;
		if (cmp)
			return cmp;

		// Compare layouts, expensive
		cmp = treeL.compare(treeL.root, treeR, treeR.root, true);
		return cmp;
	}

	/**
	 * @date 2020-04-02 21:52:34
	 */
	void loadData(database_t &store, const database_t &db) {
		if (opt_verbose >= VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Creating imprints for unsafe/empty signatures\n", timeAsString());

		assert (store.maxSignature >= db.numSignature);
		assert (store.maxMember >= db.numMember);

		/*
		 * Copy signatures+members to writable memory
		 */

		store.numSignature = db.numSignature;
		::memcpy(store.signatures, db.signatures, sizeof(*store.signatures) * db.numSignature);

		store.numMember = db.numMember;
		::memcpy(store.members, db.members, sizeof(*store.members) * db.numMember);

		// `numMember` may not be zero
		if (store.numMember == 0)
			store.numMember = 1;

		// re-create member index
		for (uint32_t iMid = 1; iMid < store.numMember; iMid++) {
			uint32_t ix = store.lookupMember(db.members[iMid].name);
			assert(store.memberIndex[ix] == 0);
			store.memberIndex[ix] = iMid;
		}

		/*
		 * Create imprints for unsafe signature groups
		 */

		generatorTree_t tree(*this);

		// reset progress
		this->setupSpeed(db.numSignature);
		this->tick = 0;

		numEmpty = 0;
		numUnsafe = 0;

		// create imprints for unsafe signature groups
		progress++; // skip reserved
		for (uint32_t iSid = 1; iSid < db.numSignature; iSid++) {
			if (opt_verbose >= VERBOSE_TICK && tick) {
				tick = 0;
				int perSecond = this->updateSpeed();

				if (perSecond == 0 || progress > progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numImprint=%u(%.0f%%) numSignature=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u",
					        timeAsString(), progress, perSecond,
					        store.numImprint, store.numImprint * 100.0 / store.maxImprint,
					        store.numSignature, store.numSignature * 100.0 / store.maxSignature,
					        store.numMember, store.numMember * 100.0 / store.maxMember,
					        numEmpty, numUnsafe);
				} else {
					int eta = (int) ((progressHi - progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numImprint=%u(%.0f%%) numSignature=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u",
					        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS,
					        store.numImprint, store.numImprint * 100.0 / store.maxImprint,
					        store.numSignature, store.numSignature * 100.0 / store.maxSignature,
					        store.numMember, store.numMember * 100.0 / store.maxMember,
					        numEmpty, numUnsafe);
				}
			}

			const signature_t *pSignature = db.signatures + iSid;

			// add imprint for unsafe signatures
			if (pSignature->flags & signature_t::SIGMASK_UNSAFE) {
				uint32_t sid = 0;
				uint32_t tid = 0;

				// avoid `"storage full"`. Give warning later
				if (store.maxImprint - store.numImprint <= store.interleave)
					break;

				tree.decodeFast(pSignature->name);

				if (!store.lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid))
					store.addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);
			}

			// stats
			if (pSignature->firstMember == 0)
				numEmpty++;
			else if (pSignature->flags & signature_t::SIGMASK_UNSAFE)
				numUnsafe++;

			this->progress++;
		}

		if (this->opt_verbose >= this->VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (progress != progressHi) {
			fprintf(stderr, "[%s] WARNING: Imprint storage almost full. Truncating at sid=%u \"%s\"\n", timeAsString(), (unsigned) (this->progress + 1), store.signatures[this->progress + 1].name);
		}

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Created imprints. numImprint=%u(%.0f%%) numSignature=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u\n",
			        timeAsString(),
			        store.numImprint, store.numImprint * 100.0 / store.maxImprint,
			        store.numSignature, store.numSignature * 100.0 / store.maxSignature,
			        store.numMember, store.numMember * 100.0 / store.maxMember,
			        numEmpty, numUnsafe);

		/*
		 * Check that all unsafe groups have no safe members (of the group would have been safe)
		 */
		for (uint32_t iSid = 1; iSid < store.numSignature; iSid++) {
			if (store.signatures[iSid].flags & signature_t::SIGMASK_UNSAFE) {
				for (uint32_t iMid = store.signatures[iSid].firstMember; iMid; iMid = store.members[iMid].nextMember) {
					assert(store.members[iMid].flags & signature_t::SIGMASK_UNSAFE);
				}
			}
		}

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
	void /*__attribute__((optimize("O0")))*/ main(database_t *pStore) {
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

			// apply restart data for > `4n9`
			unsigned ofs = 0;
			if (this->arg_numNodes > 4 && this->arg_numNodes < tinyTree_t::TINYTREE_MAXNODES)
				ofs = restartIndex[this->arg_numNodes][(this->opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0];
			if (ofs)
				generator.pRestartData = restartData + ofs;

			// show window
			if (generator.windowLo || generator.windowHi) {
				if (opt_verbose >= VERBOSE_SUMMARY)
					fprintf(stderr, "[%s] Job window: %lu-%lu\n", context_t::timeAsString(), generator.windowLo, generator.windowHi);
			}

			// ticker needs `windowHi`
			if (generator.windowHi == 0)
				generator.windowHi = pMetrics->numProgress;
		}

		if (this->opt_append) {
			/*
			 * Load candidates from file.
			 */

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
			unsigned sid, numNode, numPlaceholder, numEndpoint, numBackRef ;

			// <sid> <candidateName> <numNode> <numPlaceholder> <numEndpoint> <numBackRef>
			while (fscanf(f, "%u %s %u %u %u %u\n", &sid, name, &numNode, &numPlaceholder, &numEndpoint, &numBackRef) == 6) {
				generator.decodeFast(name);
				foundTreeMember(generator, name, numPlaceholder);
				progress++;
			}

			fclose(f);

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

		} else {

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
				 * Generate candidates
				 */
				if (this->opt_verbose >= this->VERBOSE_ACTIONS)
					fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", timeAsString(), numNode, MAXSLOTS, this->opt_flags & context_t::MAGICMASK_QNTF ? "-QnTF" : "");

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

				if (generator.windowLo == 0 && generator.windowHi == 0 && this->progress != this->progressHi) {
					printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%d}\n",
					       __FUNCTION__, this->progress, this->progressHi, numNode);
				}
			}
		}

		if (this->opt_verbose >= this->VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numImprint=%u(%.0f%%) numSignature=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u\n",
			        timeAsString(),
			        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
			        pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
			        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
			        numEmpty, numUnsafe,
			        skipDuplicate, skipSize, skipUnsafe);

		/*
		 * Compacting
		 *
		 * Members may contain (unsafe) members that got orphaned when their group accepted a safe member.
		 */
		{
			if (this->opt_verbose >= this->VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Sorting\n", timeAsString());

			// sort entries. Leave "0" and "a" untouched
			assert(pStore->numMember >= 3);
			qsort_r(pStore->members + 3, pStore->numMember - 3, sizeof(*pStore->members), comparMember, this);

			if (this->opt_verbose >= this->VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Re-indexing\n", timeAsString());

			uint32_t lastMember = pStore->numMember;

			// reset member index and friends
			::memset(pStore->memberIndex, 0, pStore->memberIndexSize * sizeof(*pStore->memberIndex));
			for (uint32_t iSid = 0; iSid < pStore->numSignature; iSid++)
				pStore->signatures[iSid].firstMember = 0;
			pStore->numMember = 1;
			skipDuplicate = skipSize = skipUnsafe = 0;

			// reload everything
			this->setupSpeed(lastMember);
			this->tick = 0;

			progress++; // skip reserved
			for (uint32_t iMid = 1; iMid < lastMember; iMid++) {
				if (opt_verbose >= VERBOSE_TICK && tick) {
					tick = 0;
					int perSecond = this->updateSpeed();

					if (perSecond == 0 || progress > progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numMember=%u skipUnsafe=%u",
						        timeAsString(), progress, perSecond, pStore->numMember, skipUnsafe);
					} else {
						int eta = (int) ((progressHi - progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numMember=%u skipUnsafe=%u",
						        timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, pStore->numMember, skipUnsafe);
					}
				}

				member_t *pMember = pStore->members + iMid;
				if (pMember->sid) {
					signature_t *pSignature = pStore->signatures + pMember->sid;

					// calculate head/tail
					generator.decodeFast(pMember->name);
					findHeadTail(pMember, generator);

					if (pSignature->flags & signature_t::SIGMASK_UNSAFE) {
						/*
						 * Adding (unsafe) member to unsafe group
						 */

						// member should be unsafe
						assert(pMember->flags & signature_t::SIGMASK_UNSAFE);
						// nodeSize should match
						assert(generator.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);

					} else if (~pMember->flags & signature_t::SIGMASK_UNSAFE) {
						/*
						 * Adding safe member to safe group
						 */

						// nodeSize should match
						assert(generator.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);

						// add safe members to index
						uint32_t ix = pStore->lookupMember(pMember->name);
						assert(pStore->memberIndex[ix] == 0);
						pStore->memberIndex[ix] = pStore->numMember;

					} else if (generator.count - tinyTree_t::TINYTREE_NSTART < pSignature->size) {
						/*
						 * Adding unsafe member to safe group
						 */

					} else {
						/*
						 * Member got orphaned when group became safe
						 */
						skipUnsafe++;
						this->progress++;
						continue;
					}

					// add to group
					pMember->nextMember = pSignature->firstMember;
					pSignature->firstMember = pStore->numMember;

					// copy
					::memcpy(pStore->members + pStore->numMember, pMember, sizeof(*pMember));
					pStore->numMember++;
				}

				this->progress++;
			}

			if (this->opt_verbose >= this->VERBOSE_TICK)
				fprintf(stderr, "\r\e[K");

			if (this->opt_verbose >= this->VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] Re-indexing. numMember=%u skipUnsafe=%u\n",
				        timeAsString(), pStore->numMember, skipUnsafe);
		}

		/*
		 * Recalculate unsafe/empty groups
		 */

		numEmpty = 0;
		numUnsafe = 0;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			if (pStore->signatures[iSid].firstMember == 0) {
				numEmpty++;
			} else if (pStore->signatures[iSid].flags & signature_t::SIGMASK_UNSAFE) {
				numUnsafe++;
			}
		}

		if (numEmpty || numUnsafe) {
			if (this->opt_verbose >= this->VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] WARNING: %u empty and %u unsafe signature groups\n", timeAsString(), numEmpty, numUnsafe);
		}

		if (opt_text == 1) {
			/*
			 * Display members of complete dataset
			 *
			 * <memberName> <numPlaceholder>
			 */
			for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
				member_t *pMember = pStore->members + iMid;

				generator.decodeFast(pMember->name);
				printf("%u\t%s\t%u\t%u\t%u\t%u\n", pMember->sid, pMember->name, generator.count - tinyTree_t::TINYTREE_NSTART, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);
			}
		}

		if (opt_text == 2) {
			/*
			 * Display full members, grouped by signature
			 */
			for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
				for (uint32_t iMid = pStore->signatures[iSid].firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
					member_t *pMember = pStore->members + iMid;

					printf("%u:%s\t", iMid, pMember->name);
					printf("%u\t", pMember->sid);

					printf("%u:%s\t%u\t", pMember->Qmid, pStore->members[pMember->Qmid].name, pMember->Qsid);
					if (pMember->Tsid & IBIT)
						printf("%u:%s\t-%u\t", pMember->Tmid, pStore->members[pMember->Tmid].name, pMember->Tsid & ~IBIT);
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

		/*
		 * Done
		 */
		if (opt_verbose >= VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] {\"numSlot\":%u,\"qntf\":%u,\"interleave\":%u,\"numNode\":%u,\"numImprint\":%u,\"numSignature\":%u,\"numMember\":%u,\"numEmpty\":%u,\"numUnsafe\":%u}\n",
			        this->timeAsString(), MAXSLOTS, (this->opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0, pStore->interleave, arg_numNodes, pStore->numImprint, pStore->numSignature, pStore->numMember, numEmpty, numUnsafe);

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
//	fprintf(stderr, "       %s --selftest <input.db>            -- Test prerequisites\n", argv[0]);

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

	if (app.opt_selftest) {
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

	if (app.opt_selftest) {

	} else {

		if (app.opt_unsafe) {
			if (app.arg_numNodes != 5)
				fprintf(stderr, "WARNING: --unsafe is intended for 5n9\n");

			// 5n9 has incomplete metrics. Hardcoded settings.
			if (app.opt_interleave == 0)
				app.opt_interleave = 3024;
			if (app.opt_maxImprint == 0)
				app.opt_maxImprint = 220000000; // for `--qntf --interleave=720`. For 64G memory, set to 600000000
			if (app.opt_maxSignature == 0)
				app.opt_maxSignature = 800000;
			if (app.opt_maxMember == 0)
				app.opt_maxMember = 16500000;

			if (app.opt_verbose >= app.VERBOSE_ACTIONS)
				fprintf(stderr, "[%s] Set limits to interleave=%u maxImprint=%u maxSignature=%u maxMember=%u\n", app.timeAsString(), app.opt_interleave, app.opt_maxImprint, app.opt_maxSignature, app.opt_maxMember);
		}

		if (app.opt_interleave == 0) {
			store.interleave = db.interleave;
			store.interleaveStep = db.interleaveStep;
		} else {
			const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, app.opt_interleave);
			assert(pMetrics); // was already checked

			store.interleave = pMetrics->numStored;
			store.interleaveStep = pMetrics->interleaveStep;
		}

		if (app.opt_maxImprint == 0) {
			// get worse-case `numNode` because settings is most likely to be different from `db` and arg_numNodes can be >= 0
			/*
			 * @date 2020-04-05 00:15:27
			 *
			 * When generating based on `4n9-QTF` dataset with 791646 signatures, use metrics `gensignature` used
			 */
			const metricsImprint_t *pMetrics = getMetricsImprint(MAXSLOTS, 0 /*!!!*/, store.interleave, 4 /*!!!*/);
			store.maxImprint = pMetrics ? pMetrics->numImprint : 0;
		} else {
			store.maxImprint = app.opt_maxImprint;
		}

		if (app.opt_imprintIndexSize == 0)
			store.imprintIndexSize = app.nextPrime(store.maxImprint * app.opt_ratio);
		else
			store.imprintIndexSize = app.opt_imprintIndexSize;

		if (app.opt_maxSignature == 0) {
			store.maxSignature = db.maxSignature;
		} else {
			store.maxSignature = app.opt_maxSignature;
		}

		if (app.opt_signatureIndexSize == 0)
			store.signatureIndexSize = app.nextPrime(store.maxSignature * app.opt_ratio);
		else
			store.signatureIndexSize = app.opt_signatureIndexSize;

		if (app.opt_maxMember == 0) {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, app.opt_flags & app.MAGICMASK_QNTF, app.arg_numNodes);
			store.maxMember = pMetrics ? pMetrics->numMember : 0;
		} else {
			store.maxMember = app.opt_maxMember;
		}

		if (app.opt_memberIndexSize == 0)
			store.memberIndexSize = app.nextPrime(store.maxMember * app.opt_ratio);
		else
			store.memberIndexSize = app.opt_memberIndexSize;

		if (store.interleave == 0 || store.interleaveStep == 0)
			app.fatal("no preset for --interleave\n");
		if (store.maxImprint == 0 || store.imprintIndexSize == 0)
			app.fatal("no preset for --maximprint\n");
		if (store.maxSignature == 0 || store.signatureIndexSize == 0)
			app.fatal("no preset for --maxsignature\n");
		if (store.maxMember == 0 || store.memberIndexSize == 0)
			app.fatal("no preset for --maxmember\n");

		if (store.maxSignature < db.numSignature)
			app.fatal("--maxsignature too low. Expected at least %u\n", db.numSignature);
		if (store.maxMember < db.numMember)
			app.fatal("--maxmember too low. Expected at least %u\n", db.numMember);
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
	app.loadData(store, db);

	/*
	 * Invoke
	 */

	if (app.opt_selftest) {
		/*
		 * self tests
		 */

		exit(0);
	}

	/*
	 * Invoke main entrypoint of application context
	 */

	app.pStore = &store;
	app.pInputDb = &db;

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
