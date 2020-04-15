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
 * @date 2020-04-07 01:07:34
 *
 * At this moment calculating and collecting:
 * `restartData[]` for `7n9-QnTF`. This is a premier!
 * signature group members for 6n9-QnTF. This is also premier.
 *
 * `QnTF` dataset looks promising:
 * share the same `4n9` address space, which holds 791646 signature groups.
 * `3n9-QnTF` has 790336 empty and 0 unsafe groups
 * `4n9-QnTF` has 695291 ampty and 499 unsafe groups
 * `5n9-QnTF` has .. ampty and .. unsafe groups
 * now scanning `6n9-QnTF` for the last 46844.
 * thats needs to get as low as possible, searching `7n9` is far above my resources.
 * Speed is about 1590999 candidates/s
 *
 * The `QnTF` dataset achieves the same using only `"Q?!T:F"` / `"abc!"` nodes/operators.
 * This releases the requirement to store information about the inverted state of `T`.
 * `T` is always inverted.
 * To compensate for loss of variety more nodes are needed.
 *
 * safe members avoid being normalised when their notation is being constructed.
 * From the constructor point of view:
 *   unsafe members have smaller nodeSize but their notation is written un a language not understood
 *   it can be translated with a penality (extra nodes)
 *
 * @date 2020-04-07 20:57:08
 *
 * `genmember` runs in 3 modes:
 * - Merge (default)
 *   = Signatures are copied
 *   = Imprints are inherited or re-built on demand
 *   = Members are copied
 *   = Additional members are loaded/generated
 *   = Member sorting
 *
 * - Prepare
 *   = Signatures are copied
 *   = Imprints are set to select empty=unsafe signature groups
 *   = Members are inherited
 *   = No member-sorting
 *   = Output is intended for `--mode=merge`
 *
 * - Collect (worker)
 *   = Signatures are copied
 *   = Imprints are inherited
 *   = Members are inherited
 *   = Each candidate member that matches is logged, signature updated and not recorded
 *   = No member-sorting
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
struct genmemberContext_t : callable_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {copntext_t} I/O context
	context_t &ctx;

	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} size of signatures to be generated in this invocation
	unsigned arg_numNodes;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned opt_generate;
	/// @var {number} size of imprint index WARNING: must be prime
	uint32_t opt_imprintIndexSize;
	/// @var {number} interleave for associative imprint index
	unsigned opt_interleave;
	/// @var {number} --keep, do not delete output database in case of errors
	unsigned opt_keep;
	/// @var {string} name of file containing members
	const char *opt_load;
	/// @var {number} Maximum number of imprints to be stored database
	uint32_t opt_maxImprint;
	/// @var {number} Maximum number of members to be stored database
	uint32_t opt_maxMember;
	/// @var {number} size of member index WARNING: must be prime
	uint32_t opt_memberIndexSize;
	/// @var {number} index/data ratio
	double opt_ratio;
	/// @var {number} get task settings from SGE environment
	uint32_t opt_sge;
	/// @var {number} Sid range upper bound
	uint32_t opt_sidHi;
	/// @var {number} Sid range lower bound
	uint32_t opt_sidLo;
	/// @var {number} task Id. First task=1
	unsigned opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {number} reindex imprints based on empty/unsafe signature groups
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
	uint32_t numUnsafe;
	uint32_t numEmpty;
	uint32_t freeMemberRoot;

	/**
	 * Constructor
	 */
	genmemberContext_t(context_t &ctx) : ctx(ctx)  {
		// arguments and options
		arg_outputDatabase = NULL;
		arg_numNodes = 0;
		opt_force = 0;
		opt_generate = 1;
		opt_imprintIndexSize = 0;
		opt_interleave = 0;
		opt_taskId = 0;
		opt_taskLast = 0;
		opt_keep = 0;
		opt_load = NULL;
		opt_maxImprint = 0;
		opt_maxMember = 0;
		opt_memberIndexSize = 0;
		opt_ratio = METRICS_DEFAULT_RATIO / 10.0;
		opt_sge = 0;
		opt_sidHi = 0;
		opt_sidLo = 0;
		opt_text = 0;
		opt_unsafe = 0;

		pStore = NULL;
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
	 * @param {tinyTree_t} treeR - candidate tree
	 */
	void findHeadTail(member_t *pMember, const tinyTree_t &treeR) {

		assert(~treeR.root & IBIT);

		// safe until proven otherwise
		pMember->flags &= ~signature_t::SIGMASK_UNSAFE;

		/*
		 * @date 2020-03-29 23:16:43
		 *
		 * Reserved root entries
		 *
		 * `"N[0] = 0?!0:0"` // zero value, zero QnTF operator, zero reference
		 * `"N[a] = 0?!0:a"` // self reference
		 */
		if (treeR.root == 0) {
			assert(::strcmp(pMember->name, "0") == 0); // must be reserved name
			assert(pMember->sid == 1); // must be reserved entry

			pMember->Qmid = pMember->Tmid = pMember->Fmid = pMember - pStore->members;
			pMember->Qsid = pMember->Tsid = pMember->Fsid = pMember->sid;
			return;
		}
		if (treeR.root == 1) {
			assert(::strcmp(pMember->name, "a") == 0); // must be reserved name
			assert(pMember->sid == 2); // must be reserved entry

			pMember->Qmid = pMember->Tmid = pMember->Fmid = pMember - pStore->members;
			pMember->Qsid = pMember->Tsid = pMember->Fsid = pMember->sid;
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
			tinyTree_t tree(ctx);
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
	 * @date 2020-04-08 16:01:14
	 *
	 * Allocate a new member, either by popping free list or assigning new
	 * Member if zero except for name
	 *
	 * @param {string} pName - name/notation of member
	 * @return {member_t}
	 */
	member_t *memberAlloc(const char *pName) {
		member_t *pMember;

		uint32_t mid = freeMemberRoot;
		if (mid) {
			pMember = pStore->members + mid;
			freeMemberRoot = pMember->nextMember; // pop from free list
			::strcpy(pMember->name, pName); // populate with name
		} else {
			mid = pStore->addMember(pName); // allocate new member
			pMember = pStore->members + mid;
		}

		return pMember;
	}

	/**
	 * @date 2020-04-08 16:04:15
	 *
	 * Release member by pushing it on the free list
	 *
	 * @param pMember
	 */
	void memberFree(member_t *pMember) {
		// zero orphan so it won't be found by `lookupMember()`
		::memset(pMember, 0, sizeof(*pMember));

		// push member on the freelist
		pMember->nextMember = freeMemberRoot;
		freeMemberRoot = pMember - pStore->members;
	}

	/**
	 * @date 2020-04-08 15:21:08
	 *
	 * Propose a member be added to a signature group.
	 * Either link member into group or push onto free list
	 *
	 * @param {member_t} pMember - proposed member
	 * @return {boolean} - `true` if candidate got accepted, `false` otherwise
	 */
	bool memberPropose(member_t *pMember) {
		signature_t *pSignature = pStore->signatures + pMember->sid;

		if (pSignature->flags & signature_t::SIGMASK_UNSAFE) {
			if (pMember->flags & signature_t::SIGMASK_UNSAFE) {
				/*
				 * group/candidate both unsafe
				 * Add to group if same node size
				 */
				if (pMember->size > pSignature->size) {
					// release
					this->memberFree(pMember);

					skipUnsafe++;
					return false;
				}
				assert(pMember->size == pSignature->size);
			} else {
				/*
				 * group is unsafe, candidate is safe.
				 * If candidate is same size then drop all existing unsafe group members
				 * If candidate is larger then keep all smaller unsafe members for later optimisations
				 */

				if (pSignature->firstMember && pMember->size == pSignature->size) {
					/*
					 * Group contains unsafe members of same size.
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

						// release first of chain
						member_t *p = pStore->members + pSignature->firstMember;

						pSignature->firstMember = p->nextMember;

						this->memberFree(p);

					}

					// group has become empty
					numEmpty++;
				}

				// mark group as safe
				pSignature->flags &= ~signature_t::SIGMASK_UNSAFE;
				pSignature->size = pMember->size;

				/*
				 * Output first safe member of a signature group
				 */

				if (opt_text == 4)
					printf("%u\t%s\t%u\t%u\t%u\t%u\n", pMember->sid, pMember->name, pMember->size, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);

				// group has become safe
				numUnsafe--;
			}
		} else {
			if (pMember->flags & signature_t::SIGMASK_UNSAFE) {
				// group is safe, candidate not. Drop candidate
				this->memberFree(pMember);

				skipUnsafe++;
				return false;
			} else {
				// group/candidate both safe
				assert(pMember->size == pSignature->size);
			}
		}

		assert(pMember->name[0]);

		/*
		 * Output candidate members on-the-fly
		 */
		if (opt_text == 3)
			printf("%u\t%s\t%u\t%u\t%u\t%u\n", pMember->sid, pMember->name, pMember->size, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);

		if (pSignature->firstMember == 0)
			numEmpty--; // group now has first member

		pMember->nextMember = pSignature->firstMember;
		pSignature->firstMember = pMember - pStore->members;

		// proposal accepted
		return true;
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
	 * @date 2020-04-15 11:02:46
	 *
	 * For now, collect members only based on size instead of `compareMember()`.
	 * Member properties still need to be discovered to make strategic decisions.
	 * Collecting members is too expensive to ask questions on missing members later.
	 *
	 * @param {generatorTree_t} treeR - candidate tree
	 * @param {string} pNameR - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 */
	void foundTreeMember(const generatorTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			ctx.tick = 0;
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u | hash=%.3f",
				        ctx.timeAsString(), ctx.progress, perSecond,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe - numEmpty,
				        skipDuplicate, skipSize, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash);
			} else {
				int eta = (int) ((treeR.windowHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u | hash=%.3f",
				        ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - treeR.windowLo) * 100.0 / (treeR.windowHi - treeR.windowLo), etaH, etaM, etaS,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe - numEmpty,
				        skipDuplicate, skipSize, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash);
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

		if (!pStore->lookupImprintAssociative(&treeR, pEvalFwd, pEvalRev, &sid, &tid))
			return;

		signature_t *pSignature = pStore->signatures + sid;

		// only if group is safe reject if structure is too large
		if ((~pSignature->flags & signature_t::SIGMASK_UNSAFE) && treeR.count - tinyTree_t::TINYTREE_NSTART > pSignature->size) {
			skipSize++;
			return;
		}

		/*
		 * test  for duplicates
		 */

		uint32_t ix = pStore->lookupMember(pNameR);
		if (pStore->memberIndex[ix] != 0) {
			// duplicate candidate name
			skipDuplicate++;
			return;
		}

		/*
		 * Allocate and populate member
		 */

		member_t *pMember = this->memberAlloc(pNameR);

		pMember->sid = sid;
		pMember->size = treeR.count - tinyTree_t::TINYTREE_NSTART;
		pMember->numPlaceholder = numPlaceholder;
		pMember->numEndpoint = numEndpoint;
		pMember->numBackRef = numBackRef;

		// lookup signature and member id's
		findHeadTail(pMember, treeR);

		/*
		 * Propose
		 */
		if (memberPropose(pMember)) {
			// if member got accepted, fixate in index
			pStore->memberIndex[ix] = pMember - pStore->members;
		}

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
		cmp = treeL.compare(treeL.root, treeR, treeR.root);
		return cmp;
	}

	/**
	 * @date 2020-04-02 21:52:34
	 */
	void reindexImprints(bool unsafeOnly) {
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Creating imprints for empty/unsafe signatures\n", ctx.timeAsString());

		/*
		 * Create imprints for unsafe signature groups
		 */

		generatorTree_t tree(ctx);

		// show window
		if (opt_sidLo || opt_sidHi) {
			if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] Sid window: %u-%u\n", ctx.timeAsString(), opt_sidLo, opt_sidHi ? opt_sidHi : pStore->numSignature);
		}

		// reset progress
		ctx.setupSpeed(pStore->numSignature);
		ctx.tick = 0;

		// re-calculate
		numEmpty = numUnsafe = 0;

		// create imprints for unsafe signature groups
		ctx.progress++; // skip reserved
		for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				ctx.tick = 0;
				int perSecond = ctx.updateSpeed();

				if (perSecond == 0 || ctx.progress > ctx.progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numImprint=%u(%.0f%%) numEmpty=%u numUnsafe=%u | hash=%.3f",
					        ctx.timeAsString(), ctx.progress, perSecond,
					        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					        numEmpty, numUnsafe - numEmpty, (double) ctx.cntCompare / ctx.cntHash);
				} else {
					int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numImprint=%u(%.0f%%) numEmpty=%u numUnsafe=%u | hash=%.3f",
					        ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
					        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
					        numEmpty, numUnsafe - numEmpty, (double) ctx.cntCompare / ctx.cntHash);
				}
			}

			if ((opt_sidLo && iSid < opt_sidLo) || (opt_sidHi && iSid >= opt_sidHi)) {
				ctx.progress++;
				continue;
			}

			const signature_t *pSignature = pStore->signatures + iSid;

			// add imprint for unsafe signatures
			if (!unsafeOnly || (pSignature->flags & signature_t::SIGMASK_UNSAFE)) {
				uint32_t sid = 0;
				uint32_t tid = 0;

				// avoid `"storage full"`. Give warning later
				if (pStore->maxImprint - pStore->numImprint <= pStore->interleave && opt_sidHi == 0) {
					// break now, display text later/ Leave progress untouched
					assert(iSid == ctx.progress);
					break;
				}

				tree.decodeFast(pSignature->name);

				if (!pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid))
					pStore->addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);
			}

			// stats
			if (pSignature->firstMember == 0)
				numEmpty++;
			if (pSignature->flags & signature_t::SIGMASK_UNSAFE)
				numUnsafe++;

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.progress != ctx.progressHi && opt_sidHi == 0) {
			fprintf(stderr, "[%s] WARNING: Imprint storage full. Truncating at sid=%u \"%s\"\n",
			        ctx.timeAsString(), (unsigned) ctx.progress, pStore->signatures[ctx.progress].name);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Created imprints. numImprint=%u(%.0f%%) numEmpty=%u numUnsafe=%u | hash=%.3f\n",
			        ctx.timeAsString(),
			        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
			        numEmpty, numUnsafe - numEmpty, (double) ctx.cntCompare / ctx.cntHash);

	}

	/**
	 * @date 2020-03-22 01:00:05
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add newly unique signatures to the database
	 */
	void /*__attribute__((optimize("O0")))*/ membersFromFile(void) {

		tinyTree_t tree(ctx);

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading members from file\n", ctx.timeAsString());

		FILE *f = fopen(this->opt_load, "r");
		if (f == NULL)
			ctx.fatal("{\"error\":\"fopen() failed\",\"where\":\"%s\",\"name\":\"%s\",\"reason\":\"%m\"}\n",
			      __FUNCTION__, this->opt_load);

		// reset progress
		ctx.setupSpeed(0);
		ctx.tick = 0;

		char name[64];
		unsigned sid, size, numPlaceholder, numEndpoint, numBackRef;

		skipDuplicate = skipSize = skipUnsafe = 0;

		// <sid> <candidateName> <size> <numPlaceholder> <numEndpoint> <numBackRef>
		while (fscanf(f, "%u %s %u %u %u %u\n", &sid, name, &size, &numPlaceholder, &numEndpoint, &numBackRef) == 6) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				ctx.tick = 0;
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u",
				        ctx.timeAsString(), ctx.progress, perSecond,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe - numEmpty,
				        skipDuplicate, skipSize, skipUnsafe);
			}

			/*
			 * test  for duplicates
			 */

			uint32_t ix = pStore->lookupMember(name);
			if (pStore->memberIndex[ix] != 0) {
				// duplicate candidate name
				skipDuplicate++;
				ctx.progress++;
				continue;
			}

			/*
			 * construct tree
			 */
			tree.decodeFast(name);

			/*
			 * Allocate and populate member
			 */

			member_t *pMember = memberAlloc(name);

			pMember->sid = sid;
			pMember->size = tree.count - tinyTree_t::TINYTREE_NSTART;
			pMember->numPlaceholder = numPlaceholder;
			pMember->numEndpoint = numEndpoint;
			pMember->numBackRef = numBackRef;

			// lookup signature and member id's
			findHeadTail(pMember, tree);

			/*
			 * Propose
			 */
			if (memberPropose(pMember)) {
				// if member got accepted, fixate in index
				pStore->memberIndex[ix] = pMember - pStore->members;
			}

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "[%s] Read members. numImprint=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u\n",
			        ctx.timeAsString(),
			        pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
			        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
			        numEmpty, numUnsafe - numEmpty,
			        skipDuplicate, skipSize, skipUnsafe);
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
	void /*__attribute__((optimize("O0")))*/ membersFromGenerator(void) {

		generatorTree_t generator(ctx);

		/*
		 * Apply window/task setting on generator
		 */

		// get metrics
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.opt_flags & context_t::MAGICMASK_QNTF, arg_numNodes);
		assert(pMetrics);

		// apply settings for `--task`
		if (this->opt_taskLast) {
			// split progress into chunks
			uint64_t taskSize = pMetrics->numProgress / this->opt_taskLast;
			if (taskSize == 0)
				taskSize = 1;
			generator.windowLo = taskSize * (this->opt_taskId - 1);
			generator.windowHi = taskSize * this->opt_taskId;

			// limits
			if (opt_taskId == opt_taskLast || generator.windowHi > pMetrics->numProgress)
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
			ofs = restartIndex[this->arg_numNodes][(ctx.opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0];
		if (ofs)
			generator.pRestartData = restartData + ofs;

		// show window
		if (generator.windowLo || generator.windowHi) {
			if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] Task window: %lu-%lu\n", ctx.timeAsString(), generator.windowLo, generator.windowHi);
		}

		// ticker needs `windowHi`
		if (generator.windowHi == 0)
			generator.windowHi = pMetrics->numProgress;

		/*
		 * create generator and candidate members
		 */

		// clear tree
		generator.clearGenerator();

		// reset progress
		ctx.setupSpeed(pMetrics ? pMetrics->numProgress : 0);
		ctx.tick = 0;
		generator.restartTick = 0;

		/*
		 * Generate candidates
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", ctx.timeAsString(), arg_numNodes, MAXSLOTS, ctx.opt_flags & context_t::MAGICMASK_QNTF ? "-QnTF" : "");

		if (arg_numNodes == 0) {
			generator.root = 0; // "0"
			foundTreeMember(generator, "0", 0, 0, 0);
			generator.root = 1; // "a"
			foundTreeMember(generator, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = arg_numNodes * 2 + 1;
			generator.generateTrees(arg_numNodes, endpointsLeft, 0, 0, static_cast<callable_t *>(this), (generatorTree_t::generateTreeCallback_t) &genmemberContext_t::foundTreeMember);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (generator.windowLo == 0 && generator.windowHi == 0 && ctx.progress != ctx.progressHi) {
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%d}\n",
			       __FUNCTION__, ctx.progress, ctx.progressHi, arg_numNodes);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u\n",
			        ctx.timeAsString(),
			        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
			        numEmpty, numUnsafe - numEmpty,
			        skipDuplicate, skipSize, skipUnsafe);
	}

	/**
	 * @date 2020-04-07 22:53:08
	 *
	 * Compact members.
	 * Remove orphans and sort on display name
	 * This should have no effect pre-existing members (they were already sorted)
	 *
	 * Groups may contain (unsafe) members that got orphaned when accepting a safe member.
	 */
	void reindexMembers(void) {
		tinyTree_t tree(ctx);

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Sorting\n", ctx.timeAsString());

		// sort entries.
		qsort_r(pStore->members + 1, pStore->numMember - 1, sizeof(*pStore->members), comparMember, this);

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Re-indexing\n", ctx.timeAsString());

		uint32_t lastMember = pStore->numMember;

		// clear member index and linked-list
		::memset(pStore->memberIndex, 0, pStore->memberIndexSize * sizeof(*pStore->memberIndex));
		for (uint32_t iSid = 0; iSid < pStore->numSignature; iSid++)
			pStore->signatures[iSid].firstMember = 0;
		pStore->numMember = 1;
		skipDuplicate = skipSize = skipUnsafe = 0;

		// reload everything
		ctx.setupSpeed(lastMember);
		ctx.tick = 0;

		ctx.progress++; // skip reserved
		for (uint32_t iMid = 1; iMid < lastMember; iMid++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				ctx.tick = 0;
				int perSecond = ctx.updateSpeed();

				if (perSecond == 0 || ctx.progress > ctx.progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numMember=%u skipUnsafe=%u | hash=%.3f",
					        ctx.timeAsString(), ctx.progress, perSecond, pStore->numMember, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash);
				} else {
					int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numMember=%u skipUnsafe=%u | hash=%.3f",
					        ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, pStore->numMember, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash);
				}
			}

			member_t *pMember = pStore->members + iMid;
			if (pMember->sid) {
				signature_t *pSignature = pStore->signatures + pMember->sid;

				// calculate head/tail
				tree.decodeFast(pMember->name);
				findHeadTail(pMember, tree);

				if (pSignature->flags & signature_t::SIGMASK_UNSAFE) {
					/*
					 * Adding (unsafe) member to unsafe group
					 */

					// member should be unsafe
					assert(pMember->flags & signature_t::SIGMASK_UNSAFE);
					// nodeSize should match
					assert(tree.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);

				} else if (~pMember->flags & signature_t::SIGMASK_UNSAFE) {
					/*
					 * Adding safe member to safe group
					 */

					// nodeSize should match
					assert(tree.count - tinyTree_t::TINYTREE_NSTART == pSignature->size);

					// add safe members to index
					uint32_t ix = pStore->lookupMember(pMember->name);
					assert(pStore->memberIndex[ix] == 0);
					pStore->memberIndex[ix] = pStore->numMember;

				} else if (tree.count - tinyTree_t::TINYTREE_NSTART < pSignature->size) {
					/*
					 * Adding unsafe member to safe group
					 */

				} else {
					/*
					 * Member got orphaned when group became safe
					 */
					skipUnsafe++;
					ctx.progress++;
					continue;
				}

				// add to group
				pMember->nextMember = pSignature->firstMember;
				pSignature->firstMember = pStore->numMember;

				// copy
				::memcpy(pStore->members + pStore->numMember, pMember, sizeof(*pMember));
				pStore->numMember++;
			}

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Re-indexing. numMember=%u skipUnsafe=%u\n",
			        ctx.timeAsString(), pStore->numMember, skipUnsafe);

		/*
		 * Recalculate empty/unsafe groups
		 */

		numEmpty = numUnsafe = 0;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			if (pStore->signatures[iSid].firstMember == 0)
				numEmpty++;
			if (pStore->signatures[iSid].flags & signature_t::SIGMASK_UNSAFE)
				numUnsafe++;
			}

		if (numEmpty || numUnsafe) {
			if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] WARNING: %u empty and %u unsafe signature groups\n", ctx.timeAsString(), numEmpty, numUnsafe);
		}

		if (opt_text == 1) {
			/*
			 * Display members of complete dataset
			 *
			 * <memberName> <numPlaceholder>
			 */
			for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
				member_t *pMember = pStore->members + iMid;

				tree.decodeFast(pMember->name);
				printf("%u\t%s\t%u\t%u\t%u\t%u\n", pMember->sid, pMember->name, tree.count - tinyTree_t::TINYTREE_NSTART, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);
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
		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] {\"numSlot\":%u,\"qntf\":%u,\"interleave\":%u,\"numNode\":%u,\"numImprint\":%u,\"numSignature\":%u,\"numMember\":%u,\"numEmpty\":%u,\"numUnsafe\":%u}\n",
			        ctx.timeAsString(), MAXSLOTS, (ctx.opt_flags & context_t::MAGICMASK_QNTF) ? 1 : 0, pStore->interleave, arg_numNodes, pStore->numImprint, pStore->numSignature, pStore->numMember, numEmpty, numUnsafe);

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

	/**
	 * Constructor
	 */
	genmemberSelftest_t(context_t &ctx) : genmemberContext_t(ctx) {
		opt_selftest = 0;
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
 * @global {genmemberSelftest_t} Application context
 */
genmemberSelftest_t app(ctx);

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
	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
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
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]\n", argv[0]);
//	fprintf(stderr, "       %s --selftest <input.db>            -- Test prerequisites\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                   Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generator          Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                    This list\n");
		fprintf(stderr, "\t   --imprintindex=<number>   Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>     Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --keep                    Do not delete output database in case of errors\n");
		fprintf(stderr, "\t   --load=<file>             Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --maximprint=<number>     Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxmember=<number>      Maximum number of members [default=%u]\n", app.opt_maxMember);
		fprintf(stderr, "\t   --memberindex=<number>    Size of member index [default=%u]\n", app.opt_memberIndexSize);
		fprintf(stderr, "\t   --[no-]paranoid           Enable expensive assertions [default=%s]\n", (ctx.opt_flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --prepare                 Prepare dataset for empty/unsafe groups\n");
		fprintf(stderr, "\t   --[no-]qntf               Enable QnTF-only mode [default=%s]\n", (ctx.opt_flags & context_t::MAGICMASK_QNTF) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                   Say more\n");
		fprintf(stderr, "\t   --ratio=<number>          Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --selftest                Validate prerequisites\n");
		fprintf(stderr, "\t   --sge                     Get SGE task settings from environment\n");
		fprintf(stderr, "\t   --sidhi=<number>          Sid range upper bound [default=%u]\n", app.opt_sidHi);
		fprintf(stderr, "\t   --sidlo=<number>          Sid range lower bound [default=%u]\n", app.opt_sidLo);
		fprintf(stderr, "\t   --task=<id>,<last>        Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                    Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>         Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --unsafe                  Reindex imprints based onempty/unsafe signature groups\n");
		fprintf(stderr, "\t-v --verbose                 Say less\n");
		fprintf(stderr, "\t   --windowhi=<number>       Upper end restart window [default=%lu]\n", app.opt_windowHi);
		fprintf(stderr, "\t   --windowlo=<number>       Lower end restart window [default=%lu]\n", app.opt_windowLo);
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
			LO_GENERATE,
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_KEEP,
			LO_LOAD,
			LO_MAXIMPRINT,
			LO_MAXMEMBER,
			LO_MEMBERINDEXSIZE,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOQNTF,
			LO_PARANOID,
			LO_QNTF,
			LO_RATIO,
			LO_SELFTEST,
			LO_SGE,
			LO_SIDHI,
			LO_SIDLO,
			LO_TASK,
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
			{"debug",            1, 0, LO_DEBUG},
			{"force",            0, 0, LO_FORCE},
			{"generate",         0, 0, LO_GENERATE},
			{"help",             0, 0, LO_HELP},
			{"imprintindexsize", 1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",       1, 0, LO_INTERLEAVE},
			{"keep",             0, 0, LO_KEEP},
			{"load",             1, 0, LO_LOAD},
			{"maximprint",       1, 0, LO_MAXIMPRINT},
			{"maxmember",        1, 0, LO_MAXMEMBER},
			{"memberindexsize",  1, 0, LO_MEMBERINDEXSIZE},
			{"no-generate",       0, 0, LO_NOGENERATE},
			{"no-paranoid",      0, 0, LO_NOPARANOID},
			{"no-qntf",          0, 0, LO_NOQNTF},
			{"paranoid",         0, 0, LO_PARANOID},
			{"qntf",             0, 0, LO_QNTF},
			{"quiet",            2, 0, LO_QUIET},
			{"ratio",            1, 0, LO_RATIO},
			{"selftest",         0, 0, LO_SELFTEST},
			{"sge",              0, 0, LO_SGE},
			{"sidhi",            1, 0, LO_SIDHI},
			{"sidlo",            1, 0, LO_SIDLO},
			{"task",             1, 0, LO_TASK},
			{"text",             2, 0, LO_TEXT},
			{"timer",            1, 0, LO_TIMER},
			{"unsafe",           0, 0, LO_UNSAFE},
			{"verbose",          2, 0, LO_VERBOSE},
			{"windowhi",         1, 0, LO_WINDOWHI},
			{"windowlo",         1, 0, LO_WINDOWLO},
			//
			{NULL,               0, 0, 0}
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
				ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_FORCE:
				app.opt_force++;
				break;
			case LO_GENERATE:
				app.opt_generate++;
				break;
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_IMPRINTINDEXSIZE:
				app.opt_imprintIndexSize = ctx.nextPrime(strtoull(optarg, NULL, 0));
				break;
			case LO_INTERLEAVE:
				app.opt_interleave = (unsigned) strtoul(optarg, NULL, 0);
				if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
					ctx.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
				break;
			case LO_KEEP:
				app.opt_keep++;
				break;
			case LO_LOAD:
				app.opt_load = optarg;
				break;
			case LO_MAXIMPRINT:
				app.opt_maxImprint = ctx.nextPrime(strtoull(optarg, NULL, 0));
				break;
			case LO_MAXMEMBER:
				app.opt_maxMember = ctx.nextPrime(strtoull(optarg, NULL, 0));
				break;
			case LO_MEMBERINDEXSIZE:
				app.opt_memberIndexSize = ctx.nextPrime(strtoull(optarg, NULL, 0));
				break;
			case LO_NOGENERATE:
				app.opt_generate = 0;
				break;
			case LO_NOPARANOID:
				ctx.opt_flags &= ~context_t::MAGICMASK_PARANOID;
				break;
			case LO_NOQNTF:
				ctx.opt_flags &= ~context_t::MAGICMASK_QNTF;
				break;
			case LO_PARANOID:
				ctx.opt_flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_QNTF:
				ctx.opt_flags |= context_t::MAGICMASK_QNTF;
				break;
			case LO_QUIET:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_RATIO:
				app.opt_ratio = strtof(optarg, NULL);
				break;
			case LO_SELFTEST:
				app.opt_selftest++;
				break;
			case LO_SGE: {
				const char *p;

				p = getenv("SGE_TASK_ID");
				app.opt_taskId = p ? atoi(p) : 0;
				if (app.opt_taskId < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_ID\n");
					exit(0);
				}

				p = getenv("SGE_TASK_LAST");
				app.opt_taskLast = p ? atoi(p) : 0;
				if (app.opt_taskLast < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_LAST\n");
					exit(0);
				}

				if (app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "task id exceeds last\n");
					exit(1);
				}

				break;
			}
			case LO_SIDHI:
				app.opt_sidHi = strtoull(optarg, NULL, 0);
				break;
			case LO_SIDLO:
				app.opt_sidLo = strtoull(optarg, NULL, 0);
				break;
			case LO_TASK:
				if (sscanf(optarg, "%u,%u", &app.opt_taskId, &app.opt_taskLast) != 2) {
					usage(argv, true);
					exit(1);
				}
				if (app.opt_taskId == 0 || app.opt_taskLast == 0) {
					fprintf(stderr, "Task id/last must be non-zero\n");
					exit(1);
				}
				if (app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "Task id exceeds last\n");
					exit(1);
				}
				break;
			case LO_TEXT:
				app.opt_text = optarg ? (unsigned) strtoul(optarg, NULL, 0) : app.opt_text + 1;
				break;
			case LO_TIMER:
				ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 0);
				break;
			case LO_UNSAFE:
				app.opt_unsafe++;
				break;
			case LO_VERBOSE:
				ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
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
		usage(argv, false);
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

	if (app.opt_load) {
		struct stat sbuf;

		if (stat(app.opt_load, &sbuf)) {
			fprintf(stderr, "%s does not exist\n", app.opt_load);
			exit(1);
		}
	}

	if (app.opt_text && isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Open input and create output database
	 */

	// Open input
	database_t db(ctx);

	db.open(app.arg_inputDatabase, true);

	if (db.flags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		ctx.logFlags(db.flags);
#if defined(ENABLE_JANSSON)
	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	/*
	 * create output
	 */

	database_t store(ctx);

	if (app.opt_selftest) {

	} else {
		/*
		 * Set defaults
		 */

		// Signatures are always copied as they need modifiable `firstMember`
		store.maxSignature = db.maxSignature;
		store.signatureIndexSize = db.signatureIndexSize;

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
			const metricsImprint_t *pMetrics;

			// don't go below 4 nodes because input database has 4n9 signatures
			if (app.arg_numNodes < 4)
				pMetrics = getMetricsImprint(MAXSLOTS, 0, store.interleave, 4);
			else
				pMetrics = getMetricsImprint(MAXSLOTS, ctx.opt_flags & ctx.MAGICMASK_QNTF, store.interleave, app.arg_numNodes);

			store.maxImprint = pMetrics ? pMetrics->numImprint : 0;
		} else {
			store.maxImprint = app.opt_maxImprint;
		}

		if (app.opt_imprintIndexSize == 0)
			store.imprintIndexSize = ctx.nextPrime(store.maxImprint * app.opt_ratio);
		else
			store.imprintIndexSize = app.opt_imprintIndexSize;

		if (app.opt_maxMember == 0) {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.opt_flags & ctx.MAGICMASK_QNTF, app.arg_numNodes);
			store.maxMember = pMetrics ? pMetrics->numMember : 0;
		} else {
			store.maxMember = app.opt_maxMember;
		}

		if (app.opt_memberIndexSize == 0)
			store.memberIndexSize = ctx.nextPrime(store.maxMember * app.opt_ratio);
		else
			store.memberIndexSize = app.opt_memberIndexSize;

		/*
		 * section inheriting
		 */

		// imprints need regeneration if `--unsafe` or settings change
		if (!app.opt_unsafe && (app.opt_ratio == METRICS_DEFAULT_RATIO / 10.0) && !app.opt_interleave && !app.opt_maxImprint && !app.opt_imprintIndexSize) {
			// inherit section
			store.maxImprint = 0;
		} else {
			// recreate section

			// section needs minimal size or input data might not fit
//			if (store.maxImprint < db.numImprint)
//				store.maxImprint = db.numImprint;
			if (store.imprintIndexSize < db.imprintIndexSize)
				store.imprintIndexSize = db.imprintIndexSize;

			// test if preset was present
			if (store.interleave == 0 || store.interleaveStep == 0)
				ctx.fatal("no preset for --interleave\n");
			if (store.maxImprint == 0 || store.imprintIndexSize == 0)
				ctx.fatal("no preset for --maximprint\n");
		}

		if (!app.arg_outputDatabase && app.opt_text != 1 && app.opt_text != 2) {
			// inherit section if not outputting anything (collecting)
			store.maxMember = 0;
		} else if (app.opt_maxMember == 0) {
			// section needs minimal size or input data might not fit
			if (store.maxMember < db.numMember)
				store.maxMember = db.numMember;
			if (store.memberIndexSize < db.memberIndexSize)
				store.memberIndexSize = db.memberIndexSize;

			// test if preset was present
			if (store.maxMember == 0 || store.memberIndexSize == 0)
				ctx.fatal("no preset for --maxmember\n");
		}
	}

	// create new sections
	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] Store create: interleave=%u maxImprint=%u maxSignature=%u maxMember=%u\n", ctx.timeAsString(), store.interleave, store.maxImprint, store.maxSignature, store.maxMember);

	store.create();

	/*
	 * Copy/inherit sections
	 */

	// templates are always inherited
	store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_TRANSFORM);

	// inherit sections
	if (store.maxImprint == 0)
		store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_IMPRINT);
	if (store.maxMember == 0)
		store.inheritSections(&db, app.arg_inputDatabase, database_t::ALLOCMASK_MEMBER);

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	app.pStore = &store;

	/*
	 * Statistics
	 */

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
		fprintf(stderr, "[%s] Allocated %lu memory\n", ctx.timeAsString(), ctx.totalAllocated);
	if (ctx.totalAllocated >= 30000000000)
		fprintf(stderr, "warning: allocated %lu memory\n", ctx.totalAllocated);

	// initialise evaluators
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, store.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, store.revTransformData);

	/*
	 * Copy sections
	 */

	if (store.allocFlags & database_t::ALLOCMASK_SIGNATURE) {
		assert(store.maxSignature >= db.numSignature);
		::memcpy(store.signatures, db.signatures, db.numSignature * sizeof(*store.signatures));
		store.numSignature = db.numSignature;
	}

	if (store.allocFlags & database_t::ALLOCMASK_MEMBER) {
		assert(store.maxMember >= db.numMember);
		::memcpy(store.members, db.members, db.numMember * sizeof(*store.members));
		if (store.memberIndexSize == db.memberIndexSize) {
			::memcpy(store.memberIndex, db.memberIndex, db.memberIndexSize * sizeof(*store.memberIndex));
		} else {
			for (uint32_t iMid = 1; iMid < db.numMember; iMid++) {
				uint32_t ix = store.lookupMember(store.members[iMid].name);
				assert(store.memberIndex[ix] == 0);
				store.memberIndex[ix] = iMid;
			}
		}
		store.numMember = db.numMember;
	}

	// skip reserved first entry
	if (store.numImprint == 0)
		store.numImprint = 1;
	if (store.numMember == 0)
		store.numMember = 1;

	// count empty/members
	app.numEmpty = app.numUnsafe = 0;
	for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
		if (store.signatures[iSid].firstMember == 0)
			app.numEmpty++;
		if (store.signatures[iSid].flags & signature_t::SIGMASK_UNSAFE)
			app.numUnsafe++;
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] numImprint=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u\n",
		        ctx.timeAsString(),
		        store.numImprint, store.numImprint * 100.0 / store.maxImprint,
		        store.numMember, store.numMember * 100.0 / store.maxMember,
		        app.numEmpty, app.numUnsafe - app.numEmpty);

	/*
	 * Load members from file to increase chance signature groups become safe
	 */
	if (app.opt_load)
		app.membersFromFile();

	/*
	 * Recreate imprints
	 */

	if (store.allocFlags & database_t::ALLOCMASK_IMPRINT)
		app.reindexImprints(app.opt_unsafe != 0);

	/*
	 * Fire up generator for new candidates
	 */

	if (app.opt_generate)
		app.membersFromGenerator();

	/*
	 * re-order and re-index members
	 */

	if (app.arg_outputDatabase || app.opt_text == 1 || app.opt_text == 2) {
			app.reindexMembers();

		/*
		 * Check that all unsafe groups have no safe members (or the group would have been safe)
		 */
		for (uint32_t iSid = 1; iSid < store.numSignature; iSid++) {
			if (store.signatures[iSid].flags & signature_t::SIGMASK_UNSAFE) {
				for (uint32_t iMid = store.signatures[iSid].firstMember; iMid; iMid = store.members[iMid].nextMember) {
					assert(store.members[iMid].flags & signature_t::SIGMASK_UNSAFE);
				}
			}
		}
	}

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
	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY && !app.opt_text) {
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
