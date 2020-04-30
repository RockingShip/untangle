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
 * `restartData[]` for `7n9-pure`. This is a premier!
 * signature group members for 6n9-pure. This is also premier.
 *
 * pure dataset looks promising:
 * share the same `4n9` address space, which holds 791646 signature groups.
 * `3n9-pure` has 790336 empty and 0 unsafe groups
 * `4n9-pure` has 695291 ampty and 499 unsafe groups
 * `5n9-pure` has .. empty and .. unsafe groups
 * now scanning `6n9-pure` for the last 46844.
 * thats needs to get as low as possible, searching `7n9` is far above my resources.
 * Speed is about 1590999 candidates/s
 *
 * The pure dataset achieves the same using only `"Q?!T:F"` / `"abc!"` nodes/operators.
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
 *
 * @date 2020-04-22 21:20:56
 *
 * `genmember` selects candidates already present in the imprint index.
 * Selected candidates are added to `members`.
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
#include <stdio.h>
#include <stdlib.h>
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
struct genmemberContext_t : dbtool_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} Tree size in nodes to be generated for this invocation;
	unsigned arg_numNodes;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned opt_generate;
	/// @var {string} name of file containing members
	const char *opt_load;
	/// @var {number} save level-1 indices (hintIndex, signatureIndex, ImprintIndex) and level-2 index (imprints)
	unsigned opt_saveIndex;
	/// @var {number} Sid range upper bound
	unsigned opt_sidHi;
	/// @var {number} Sid range lower bound
	unsigned opt_sidLo;
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

	/// @var {number} - THE generator
	generatorTree_t generator;

	unsigned skipDuplicate;
	unsigned skipSize;
	unsigned skipUnsafe;
	unsigned numUnsafe;
	unsigned numEmpty;
	unsigned freeMemberRoot;

	/**
	 * Constructor
	 */
	genmemberContext_t(context_t &ctx) : dbtool_t(ctx), generator(ctx) {
		// arguments and options
		arg_inputDatabase = NULL;
		arg_numNodes = 0;
		arg_outputDatabase = NULL;
		opt_force = 0;
		opt_generate = 1;
		opt_saveIndex = 1;
		opt_taskId = 0;
		opt_taskLast = 0;
		opt_load = NULL;
		opt_sidHi = 0;
		opt_sidLo = 0;
		opt_text = 0;
		opt_unsafe = 0;
		opt_windowHi = 0;
		opt_windowLo = 0;

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

			unsigned Q = treeR.N[treeR.root].Q;
			{
				const char *pComponentName = treeR.encode(Q, skin);
				unsigned ix = pStore->lookupMember(pComponentName);

				pMember->Qmid = pStore->memberIndex[ix];
				pMember->Qsid = pStore->members[pMember->Qmid].sid;

				// member is unsafe if component not found or unsafe
				if (pMember->Qmid == 0 || pMember->Qsid == 0 || (pStore->members[pMember->Qmid].flags & signature_t::SIGMASK_UNSAFE))
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
			}

			unsigned To = treeR.N[treeR.root].T & ~IBIT;
			{
				const char *pComponentName = treeR.encode(To, skin);
				unsigned ix = pStore->lookupMember(pComponentName);

				pMember->Tmid = pStore->memberIndex[ix];
				pMember->Tsid = pStore->members[pMember->Tmid].sid ^ (treeR.N[treeR.root].T & IBIT);

				// member is unsafe if component not found or unsafe
				if (pMember->Tmid == 0 || (pMember->Tsid & ~IBIT) == 0 || (pStore->members[pMember->Tmid].flags & signature_t::SIGMASK_UNSAFE))
					pMember->flags |= signature_t::SIGMASK_UNSAFE;
			}

			unsigned F = treeR.N[treeR.root].F;
			{
				const char *pComponentName = treeR.encode(F, skin);
				unsigned ix = pStore->lookupMember(pComponentName);

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
				unsigned select = 1 << treeR.root | 1 << 0; // selected nodes to extract nodes
				unsigned nextPlaceholderPlaceholder = tinyTree_t::TINYTREE_KSTART;
				uint32_t what[tinyTree_t::TINYTREE_NEND];
				what[0] = 0; // replacement for zero

				// scan tree for needed nodes, ignoring `hot` node
				for (unsigned k = treeR.root; k >= tinyTree_t::TINYTREE_NSTART; k--) {
					if (k != hot && (select & (1 << k))) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q = pNode->Q;
						const unsigned To = pNode->T & ~IBIT;
						const unsigned F = pNode->F;

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
				for (unsigned k = tinyTree_t::TINYTREE_NSTART; k <= treeR.root; k++) {
					if (k != hot && select & (1 << k)) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q = pNode->Q;
						const unsigned To = pNode->T & ~IBIT;
						const unsigned Ti = pNode->T & IBIT;
						const unsigned F = pNode->F;

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
				unsigned ix = pStore->lookupMember(name);
				unsigned midHead = pStore->memberIndex[ix];
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

		unsigned mid = freeMemberRoot;
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
					 * For `5n9-pure` it turns out that the chance of finding safe replacements is rare.
					 * And you need to collect all non-safe members if the group is unsafe.
					 * Orphaning them depletes resources too fast.
					 *
					 * Reuse `members[]`.
					 * Field `nextMember` is perfect for that.
					 */
					while (pSignature->firstMember) {
						// remove all references to
						for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
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
		if (opt_text == 1)
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
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreeMember(const generatorTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u | hash=%.3f",
				        ctx.timeAsString(), ctx.progress, perSecond,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe - numEmpty,
				        skipDuplicate, skipSize, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u | hash=%.3f %s",
				        ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - treeR.windowLo) * 100.0 / (ctx.progressHi - treeR.windowLo), etaH, etaM, etaS,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe - numEmpty,
				        skipDuplicate, skipSize, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash, pNameR);
			}

			if (ctx.restartTick) {
				// passed a restart point
				fprintf(stderr, "\n");
				ctx.restartTick = 0;
			}

			ctx.tick = 0;
		}

		/*
		 * test  for duplicates
		 */

		unsigned ix = pStore->lookupMember(pNameR);
		if (pStore->memberIndex[ix] != 0) {
			// duplicate candidate name
			skipDuplicate++;
			return true;
		}

		/*
		 * Find the matching signature group. It's layout only so ignore transformId.
		 */

		unsigned sid = 0;
		unsigned tid = 0;

		if (!pStore->lookupImprintAssociative(&treeR, this->pEvalFwd, this->pEvalRev, &sid, &tid))
			return true;

		signature_t *pSignature = pStore->signatures + sid;

		// only if group is safe reject if structure is too large
		if ((~pSignature->flags & signature_t::SIGMASK_UNSAFE) && treeR.count - tinyTree_t::TINYTREE_NSTART > pSignature->size) {
			skipSize++;
			return true;
		}

		/*
		 * Allocate and populate member
		 */

		// test if in "collect without store" mode.
		if (pStore->members == NULL)
			return true;

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

		return true;
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
		context_t *pApp = static_cast<context_t *>(arg);

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
	void rebuildImprints(unsigned unsafeOnly) {
		// clear signature and imprint index
		::memset(pStore->imprintIndex, 0, sizeof(*pStore->imprintIndex) * pStore->imprintIndexSize);

		if (pStore->numSignature < 2)
			return; //nothing to do

		// skip reserved entry
		pStore->numImprint = 1;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
			if (unsafeOnly)
				fprintf(stderr, "[%s] Rebuilding imprints for empty/unsafe signatures\n", ctx.timeAsString());
			else
				fprintf(stderr, "[%s] Rebuilding imprints\n", ctx.timeAsString());
		}

		/*
		 * Create imprints for signature groups
		 */

		generatorTree_t tree(ctx);

		// show window
		if (opt_sidLo || opt_sidHi) {
			if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] Sid window: %u-%u\n", ctx.timeAsString(), opt_sidLo, opt_sidHi ? opt_sidHi : pStore->numSignature);
		}

		// reset ticker
		ctx.setupSpeed(pStore->numSignature);
		ctx.tick = 0;

		// re-calculate
		numEmpty = numUnsafe = 0;

		// create imprints for signature groups
		ctx.progress++; // skip reserved
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
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

				ctx.tick = 0;
			}

			if ((opt_sidLo && iSid < opt_sidLo) || (opt_sidHi && iSid >= opt_sidHi)) {
				ctx.progress++;
				continue;
			}

			const signature_t *pSignature = pStore->signatures + iSid;

			/*
			 * Add to imprint index, either all or empty/unsafe only
			 */

			if (!unsafeOnly || (pSignature->flags & signature_t::SIGMASK_UNSAFE)) {

				// avoid `"storage full"`. Give warning later
				if (pStore->maxImprint - pStore->numImprint <= pStore->interleave && opt_sidHi == 0) {
					// break now, display text later/ Leave progress untouched
					assert(iSid == ctx.progress);
					break;
				}

				tree.decodeFast(pSignature->name);

				unsigned sid, tid;

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

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;

		char name[64];
		unsigned sid, size, numPlaceholder, numEndpoint, numBackRef;

		skipDuplicate = skipSize = skipUnsafe = 0;

		// <sid> <candidateName> <size> <numPlaceholder> <numEndpoint> <numBackRef>
		while (fscanf(f, "%u %s %u %u %u %u\n", &sid, name, &size, &numPlaceholder, &numEndpoint, &numBackRef) == 6) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u | hash=%.3f",
				        ctx.timeAsString(), ctx.progress, perSecond,
				        pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				        numEmpty, numUnsafe - numEmpty,
				        skipDuplicate, skipSize, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash);

				ctx.tick = 0;
			}

			/*
			 * test  for duplicates
			 */

			unsigned ix = pStore->lookupMember(name);
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

		// apply restart data for > `4n9`
		unsigned ofs = 0;
		if (this->arg_numNodes > 4 && this->arg_numNodes < tinyTree_t::TINYTREE_MAXNODES)
			ofs = restartIndex[this->arg_numNodes][(ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0];
		if (ofs)
			generator.pRestartData = restartData + ofs;

		// reset progress
		if (generator.windowHi) {
			ctx.setupSpeed(generator.windowHi);
		} else {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, arg_numNodes);
			ctx.setupSpeed(pMetrics ? pMetrics->numProgress : 0);
		}
		ctx.tick = 0;

		/*
		 * Generate candidates
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", ctx.timeAsString(), arg_numNodes, MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE ? "-pure" : "");

		if (arg_numNodes == 0) {
			generator.root = 0; // "0"
			foundTreeMember(generator, "0", 0, 0, 0);
			generator.root = 1; // "a"
			foundTreeMember(generator, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = arg_numNodes * 2 + 1;

			generator.initialiseGenerator(ctx.flags & context_t::MAGICMASK_PURE);
			generator.clearGenerator();
			generator.generateTrees(arg_numNodes, endpointsLeft, 0, 0, this, static_cast<generatorTree_t::generateTreeCallback_t>(&genmemberContext_t::foundTreeMember));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.progress != ctx.progressHi && this->opt_windowLo == 0 && this->opt_windowHi == 0) {
			// can only test if windowing is disabled
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%u}\n",
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
	 * Rebuild members by compacting them (removing orphans), sorting and re-chaining them.
	 *
	 * This should have no effect pre-loaded members (they were already sorted)
	 *
	 * Groups may contain (unsafe) members that got orphaned when accepting a safe member.
	 */
	void finaliseMembers(void) {
		tinyTree_t tree(ctx);

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Sorting members\n", ctx.timeAsString());

		// sort entries.
		assert(pStore->numMember >= 1);
		qsort_r(pStore->members + 1, pStore->numMember - 1, sizeof(*pStore->members), comparMember, this);

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Indexing members\n", ctx.timeAsString());

		unsigned lastMember = pStore->numMember;

		// clear member index and linked-list
		::memset(pStore->memberIndex, 0, pStore->memberIndexSize * sizeof(*pStore->memberIndex));
		for (unsigned iSid = 0; iSid < pStore->numSignature; iSid++)
			pStore->signatures[iSid].firstMember = 0;
		pStore->numMember = 1;
		skipDuplicate = skipSize = skipUnsafe = 0;

		// reload everything
		ctx.setupSpeed(lastMember);
		ctx.tick = 0;

		ctx.progress++; // skip reserved
		for (unsigned iMid = 1; iMid < lastMember; iMid++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
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

				ctx.tick = 0;
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
					unsigned ix = pStore->lookupMember(pMember->name);
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
			fprintf(stderr, "[%s] Indexed members. numMember=%u skipUnsafe=%u\n",
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

		if (opt_text == 2) {
			/*
			 * Display members of complete dataset
			 *
			 * <memberName> <numPlaceholder>
			 */
			for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
				member_t *pMember = pStore->members + iMid;

				tree.decodeFast(pMember->name);
				printf("%u\t%s\t%u\t%u\t%u\t%u\n", pMember->sid, pMember->name, tree.count - tinyTree_t::TINYTREE_NSTART, pMember->numPlaceholder, pMember->numEndpoint, pMember->numBackRef);
			}
		}

		if (opt_text == 3) {
			/*
			 * Display full members, grouped by signature
			 */
			for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
				for (unsigned iMid = pStore->signatures[iSid].firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
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
			fprintf(stderr, "[%s] {\"numSlot\":%u,\"pure\":%u,\"interleave\":%u,\"numNode\":%u,\"numImprint\":%u,\"numSignature\":%u,\"numMember\":%u,\"numEmpty\":%u,\"numUnsafe\":%u}\n",
			        ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, pStore->interleave, arg_numNodes, pStore->numImprint, pStore->numSignature, pStore->numMember, numEmpty, numUnsafe);

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
genmemberContext_t app(ctx);

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
	if (app.arg_outputDatabase) {
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
 * @date 2020-03-14 11:17:04
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>           Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --maximprint=<number>           Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxmember=<number>            Maximum number of members [default=%u]\n", app.opt_maxMember);
		fprintf(stderr, "\t   --memberindexsize=<number>      Size of member index [default=%u]\n", app.opt_memberIndexSize);
		fprintf(stderr, "\t   --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --prepare                       Prepare dataset for empty/unsafe groups\n");
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say more\n");
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --[no-]saveindex                Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --sid=[<low>,]<high>            Sid range upper bound  [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --task=sge                      Get task settings from SGE environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>              Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                          Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --[no-]unsafe                   Reindex imprints based onempty/unsafe signature groups [default=%s]\n", (ctx.flags & context_t::MAGICMASK_UNSAFE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-v --verbose                       Say less\n");
		fprintf(stderr, "\t   --window=[<low>,]<high>         Upper end restart window [default=%lu,%lu]\n", app.opt_windowLo, app.opt_windowHi);
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
			LO_LOAD,
			LO_MAXIMPRINT,
			LO_MAXMEMBER,
			LO_MEMBERINDEXSIZE,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_NOUNSAFE,
			LO_PARANOID,
			LO_PURE,
			LO_RATIO,
			LO_SAVEINDEX,
			LO_SID,
			LO_SIGNATUREINDEXSIZE,
			LO_TASK,
			LO_TEXT,
			LO_TIMER,
			LO_UNSAFE,
			LO_WINDOW,
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
			{"generate",           0, 0, LO_GENERATE},
			{"help",               0, 0, LO_HELP},
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",         1, 0, LO_INTERLEAVE},
			{"load",               1, 0, LO_LOAD},
			{"maximprint",         1, 0, LO_MAXIMPRINT},
			{"maxmember",          1, 0, LO_MAXMEMBER},
			{"memberindexsize",    1, 0, LO_MEMBERINDEXSIZE},
			{"no-generate",        0, 0, LO_NOGENERATE},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-pure",            0, 0, LO_NOPURE},
			{"no-saveindex",       0, 0, LO_NOSAVEINDEX},
			{"no-unsafe",          0, 0, LO_NOUNSAFE},
			{"paranoid",           0, 0, LO_PARANOID},
			{"pure",               0, 0, LO_PURE},
			{"quiet",              2, 0, LO_QUIET},
			{"ratio",              1, 0, LO_RATIO},
			{"saveindex",          0, 0, LO_SAVEINDEX},
			{"sid",                1, 0, LO_SID},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"task",               1, 0, LO_TASK},
			{"text",               2, 0, LO_TEXT},
			{"timer",              1, 0, LO_TIMER},
			{"unsafe",             0, 0, LO_UNSAFE},
			{"verbose",            2, 0, LO_VERBOSE},
			{"window",             1, 0, LO_WINDOW},
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
				ctx.opt_debug = ::strtoul(optarg, NULL, 0);
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
				app.opt_imprintIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
				break;
			case LO_INTERLEAVE:
				app.opt_interleave = ::strtoul(optarg, NULL, 0);
				if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
					ctx.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
				break;
			case LO_LOAD:
				app.opt_load = optarg;
				break;
			case LO_MAXIMPRINT:
				app.opt_maxImprint = ctx.dToMax(::strtod(optarg, NULL));
				break;
			case LO_MAXMEMBER:
				app.opt_maxMember = ctx.dToMax(::strtod(optarg, NULL));
				break;
			case LO_MEMBERINDEXSIZE:
				app.opt_memberIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
				break;
			case LO_NOGENERATE:
				app.opt_generate = 0;
				break;
			case LO_NOPARANOID:
				ctx.flags &= ~context_t::MAGICMASK_PARANOID;
				break;
			case LO_NOPURE:
				ctx.flags &= ~context_t::MAGICMASK_PURE;
				break;
			case LO_NOUNSAFE:
				ctx.flags &= ~context_t::MAGICMASK_UNSAFE;
				break;
			case LO_PARANOID:
				ctx.flags |= context_t::MAGICMASK_PARANOID;
				break;
			case LO_PURE:
				ctx.flags |= context_t::MAGICMASK_PURE;
				break;
			case LO_QUIET:
				ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
				break;
			case LO_RATIO:
				app.opt_ratio = strtof(optarg, NULL);
				break;
			case LO_NOSAVEINDEX:
				app.opt_saveIndex = 0;
				break;
			case LO_SAVEINDEX:
				app.opt_saveIndex = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_saveIndex + 1;
				break;
			case LO_SID: {
				unsigned m, n;

				int ret = sscanf(optarg, "%u,%u", &m, &n);
				if (ret == 2) {
					app.opt_sidLo = m;
					app.opt_sidHi = n;
				} else if (ret == 1) {
					app.opt_sidHi = m;
				} else {
					usage(argv, true);
					exit(1);
				}

				break;
			}
			case LO_SIGNATUREINDEXSIZE:
				app.opt_signatureIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
				break;
			case LO_TASK:
				if (::strcmp(optarg, "sge") == 0) {
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

					if (app.opt_taskId < 1 || app.opt_taskId > app.opt_taskLast) {
						fprintf(stderr, "sge id/last out of bounds: %u,%u\n", app.opt_taskId, app.opt_taskLast);
						exit(1);
					}

					// set ticker interval to 60 seconds
					ctx.opt_timer = 60;
				} else {
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
				}
				break;
			case LO_TEXT:
				app.opt_text = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_text + 1;
				break;
			case LO_TIMER:
				ctx.opt_timer = ::strtoul(optarg, NULL, 0);
				break;
			case LO_UNSAFE:
				ctx.flags |= context_t::MAGICMASK_UNSAFE;
				break;
			case LO_VERBOSE:
				ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
				break;
			case LO_WINDOW: {
				uint64_t m, n;

				int ret = sscanf(optarg, "%lu,%lu", &m, &n);
				if (ret == 2) {
					app.opt_windowLo = m;
					app.opt_windowHi = n;
				} else if (ret == 1) {
					app.opt_windowHi = m;
				} else {
					usage(argv, true);
					exit(1);
				}

				break;
			}

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
		app.arg_numNodes = ::strtoul(argv[optind++], &endptr, 0);

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
	 * `--task` post-processing
	 */
	if (app.opt_taskId || app.opt_taskLast) {
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, app.arg_numNodes);
		if (!pMetrics)
			ctx.fatal("no preset for --task\n");

		// split progress into chunks
		uint64_t taskSize = pMetrics->numProgress / app.opt_taskLast;
		if (taskSize == 0)
			taskSize = 1;
		app.opt_windowLo = taskSize * (app.opt_taskId - 1);
		app.opt_windowHi = taskSize * app.opt_taskId;

		// last task is open ended in case metrics are off
		if (app.opt_taskId == app.opt_taskLast)
			app.opt_windowHi = 0;
	}
	if (app.opt_windowHi && app.opt_windowLo >= app.opt_windowHi) {
		fprintf(stderr, "--window low exceeds high\n");
		exit(1);
	}

	if (app.opt_windowLo || app.opt_windowHi) {
		if (app.arg_numNodes > tinyTree_t::TINYTREE_MAXNODES || restartIndex[app.arg_numNodes][(ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0] == 0) {
			fprintf(stderr, "No restart data for --window\n");
			exit(1);
		}
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

	// test readOnly mode
	app.readOnlyMode = (app.arg_outputDatabase == NULL);

	// allow for copy-on-write
	if (!app.readOnlyMode)
		app.copyOnWrite = 1;

	db.open(app.arg_inputDatabase, app.copyOnWrite);

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

#if defined(ENABLE_JANSSON)
	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));
#endif

	/*
	 * @date 2020-04-21 00:16:34
	 *
	 * create output
	 *
	 * Transforms, signature, hint and imprint data never change and can be inherited
	 * Members can be inherited when nothing is added (missing output database)
	 *
	 * Sections can be inherited if their data or index settings remain unchanged
	 *
	 * NOTE: Signature data must be writable when `firstMember` changes (output database present)
	 */

	database_t store(ctx);

	// which sections are primary goal and need to be writable to be collect or sort
	if (app.arg_outputDatabase != NULL || app.opt_text == 2 || app.opt_text == 3)
		app.primarySections = database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_MEMBER;
	else
		app.primarySections = 0;

	// input database will always have a minimal node size of 4.
	unsigned minNodes = app.arg_numNodes > 4 ? app.arg_numNodes : 4;

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, minNodes);

	if (app.rebuildSections && app.readOnlyMode)
		ctx.fatal("readOnlyMode and database sections [%s] require rebuilding\n", store.sectionToText(app.rebuildSections));

	// determine if sections are rebuild, inherited or copied (copy-on-write)
	app.modeDatabaseSections(store, db);

	/*
	 * Finalise allocations and create database
	 */

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("genmemberContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		// Assuming with database allocations included
		size_t allocated = ctx.totalAllocated + store.estimateMemoryUsage(app.inheritSections);

		struct sysinfo info;
		if (sysinfo(&info) == 0) {
			double percent = 100.0 * allocated / info.freeram;
			if (percent > 80)
				fprintf(stderr, "WARNING: using %.1f%% of free memory minus cache\n", percent);
		}
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] Store create: interleave=%u maxSignature=%u maxMember=%u\n", ctx.timeAsString(), store.interleave, store.maxSignature, store.maxMember);

	// actual create
	store.create(app.inheritSections);
	app.pStore = &store;

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS && (~app.rebuildSections & ~app.inheritSections)) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Allocated %lu memory. freeMemory=%lu.\n", ctx.timeAsString(), ctx.totalAllocated, info.freeram);
	}

	/*
	 * Inherit/copy sections
	 */

	app.populateDatabaseSections(store, db);

	// initialize evaluator
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, store.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, store.revTransformData);

	/*
	 * count empty/unsafe
	 */

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
	 * Rebuild sections
	 */

	if (!app.readOnlyMode) {
		if (app.rebuildSections & database_t::ALLOCMASK_IMPRINT) {
			// rebuild imprints
			app.rebuildImprints(ctx.flags & context_t::MAGICMASK_UNSAFE);
			app.rebuildSections &= ~(database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX);
		}
		if (app.rebuildSections)
			store.rebuildIndices(app.rebuildSections);
	} else if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		if (app.rebuildSections)
			fprintf(stderr, "[%s] WARNING: readOnlyMode and database sections [%s] are missing.", ctx.timeAsString(), store.sectionToText(app.rebuildSections));
	}

	/*
	 * Where to look for new candidates
	 */

	// if input is empty, skip reserved entries
	if (!app.readOnlyMode) {
		assert(store.numMember > 0);
	}

	if (app.opt_load)
		app.membersFromFile();
	if (app.opt_generate)
		app.membersFromGenerator();

	/*
	 * re-order and re-index members
	 */

	if (!app.readOnlyMode) {
		// compact, sort and reindex members
		app.finaliseMembers();

		/*
		 * Check that all unsafe groups have no safe members (or the group would have been safe)
		 */
		for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
			if (store.signatures[iSid].flags & signature_t::SIGMASK_UNSAFE) {
				for (unsigned iMid = store.signatures[iSid].firstMember; iMid; iMid = store.members[iMid].nextMember) {
					assert(store.members[iMid].flags & signature_t::SIGMASK_UNSAFE);
				}
			}
		}
	}

	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		if (!app.opt_saveIndex) {
			store.signatureIndexSize = 0;
			store.hintIndexSize = 0;
			store.imprintIndexSize = 0;
			store.numImprint = 0;
			store.interleave = 0;
			store.interleaveStep = 0;
		}

		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

	if (app.opt_taskLast)
		fprintf(stderr, "{\"done\":\"%s\",\"taskId\":%u,\"taskLast\":%u,\"windowLo\":\"%lu\",\"windowHi\":\"%lu\"}\n", argv[0], app.opt_taskId, app.opt_taskLast, app.opt_windowLo, app.opt_windowHi);
	else if (app.opt_windowLo || app.opt_windowHi)
		fprintf(stderr, "{\"done\":\"%s\",\"windowLo\":\"%lu\",\"windowHi\":\"%lu\"}\n", argv[0], app.opt_windowLo, app.opt_windowHi);
	else
		fprintf(stderr, "{\"done\":\"%s\"}\n", argv[0]);

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
