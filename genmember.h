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
 * I always thought that the goal motivation was to replace structures with smallest nodesize but that might not be the case.
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
 * `genmember` actually needs two modes: preparation of an imprint index (done by master) and collecting (done by workers).
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
 * `4n9-pure` has 695291 empty and 499 unsafe groups
 * `5n9-pure` has .. empty and .. unsafe groups
 * now scanning `6n9-pure` for the last 46844.
 * that is needs to get as low as possible, searching `7n9` is far above my resources.
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
 *   it can be translated with penalty (extra nodes)
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
 *
 * @date 2020-04-22 21:37:03
 *
 * Text modes:
 *
 * `--text[=1]` Brief mode that show selected candidates passed to `foundTreeSignature()`.
 *              Selected candidates are those that challenge and win the current display name.
 *              Also intended for transport and merging when broken into multiple tasks.
 *              Can be used for the `--load=<file>` option.
 *              Output can be converted to other text modes by `"./gensignature <input.db> <numNode> [<output.db>]--load=<inputList> --no-generate --text=<newMode> '>' <outputList>
 *
 *              <name>
 *
 * `--text=2`   Full mode of all candidates passed to `foundTreeSignature()` including what needed to compare against the display name.
 *
 *              <cid> <sid> <cmp> <name> <size> <numPlaceholder> <numEndpoint> <numBackRef>

 *              where:
 *                  <cid> is the candidate id assigned by the generator.
 *                  <sid> is the signature id assigned by the associative lookup.
 *                  <cmp> is the result of `comparSignature()` between the candidate and the current display name.
 *
 *              <cmp> can be:
 *                  cmp = '*'; // candidate has excess placeholders
 *                  cmp = '<'; // worse, group safe, candidate unsafe
 *                  cmp = '-'; // worse, candidate too large for group
 *                  cmp = '='; // equal, group unsafe, candidate unsafe
 *                  cmp = '+'; // equal, group safe, candidate safe
 *                  cmp = '>'; // better, group unsafe, candidate safe
 *
 * `--text=3`   Selected and sorted signatures that are written to the output database.
 *              NOTE: same format as `--text=1`
 *              NOTE: requires sorting and will copy (not inherit) imprint section
 *
 *              <name>
 *
 * `--text=4`   Selected and sorted signatures that are written to the output database
 *              NOTE: requires sorting and will copy (not inherit) imprint section
 *
 *              <mid> <sid> <tid> <name> <Qmid> <Tmid> <Fmid> <HeadMids> <Safe/Nonsafe-member> <Safe/Nonsafe-signature>
 *
 * @date 2021-08-03 23:24:58
 *
 *   `--listlookup` in combination with `--pure`
 *   List missing member that would otherwise make a signature safe.
 *   Those members lie outside 4n9-pure.
 *   As a member it is a placeholder signature name.
 *   Mark this signature SAFE, second-pass: target rescan 5n9 signature space for group representative.
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
#include <jansson.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "database.h"
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
struct genmemberContext_t : dbtool_t {

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
	/// @var {number} --altgen, Alternative generator for 7n9 space (EXPERIMENTAL!)
	unsigned   opt_altgen;
	/// @var {number} --cascade, Apply cascading dyadic normalisation
	unsigned   opt_cascade;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned   opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned   opt_generate;
	/// @var {string} let `findHeadTail()` show what is missing instead of failing
	unsigned   opt_listLookup;
	/// @var {string} name of file containing members
	const char *opt_load;
	/// @var {string} --mixed, Consider/accept top-level mixed members only
	unsigned   opt_mixed;
	/// @var {string} --safe, Consider/accept safe members only
	unsigned   opt_safe;
	/// @var {number} Sid range upper bound
	unsigned   opt_sidHi;
	/// @var {number} Sid range lower bound
	unsigned   opt_sidLo;
	/// @var {number} task Id. First task=1
	unsigned   opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned   opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned   opt_text;
	/// @var {number} truncate on database overflow
	double     opt_truncate;
	/// @var {number} generator upper bound
	uint64_t   opt_windowHi;
	/// @var {number} generator lower bound
	uint64_t   opt_windowLo;

	/// @var {uint16_t} - score of signature group members. NOTE: size+score may differ from signature
	uint16_t    *pSafeSize;
	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	/// @var {unsigned} - active index for `hints[]`
	unsigned    activeHintIndex;
	/// @var {number} - Head of list of free members to allocate
	unsigned    freeMemberRoot;
	/// @var {number} - THE generator
	generator_t generator;
	/// @var {number} - Number of empty signatures left
	unsigned    numEmpty;
	/// @var {number} - Number of unsafe signatures left
	unsigned    numUnsafe;
	/// @var {number} cascading dyadics
	unsigned    skipCascade;
	/// @var {number} `foundTree()` duplicate by name
	unsigned    skipDuplicate;
	/// @var {number} `foundTree()` too large for signature
	unsigned    skipSize;
	/// @var {number} `foundTree()` unsafe abundance
	unsigned    skipUnsafe;
	/// @var {number} Where database overflow was caught
	uint64_t    truncated;
	/// @var {number} Name of signature causing overflow
	char        truncatedName[tinyTree_t::TINYTREE_NAMELEN + 1];

	/**
	 * Constructor
	 */
	genmemberContext_t(context_t &ctx) : dbtool_t(ctx), generator(ctx) {
		// arguments and options
		arg_inputDatabase  = NULL;
		arg_numNodes       = 0;
		arg_outputDatabase = NULL;
		opt_altgen         = 0;
		opt_cascade        = 0;
		opt_force          = 0;
		opt_generate       = 1;
		opt_taskId         = 0;
		opt_taskLast       = 0;
		opt_listLookup     = 0;
		opt_load           = NULL;
		opt_mixed          = 0;
		opt_safe           = 0;
		opt_sidHi          = 0;
		opt_sidLo          = 0;
		opt_text           = 0;
		opt_truncate       = 0;
		opt_windowHi       = 0;
		opt_windowLo       = 0;

		pSafeSize = NULL;
		pStore    = NULL;

		activeHintIndex  = 0;
		freeMemberRoot   = 0;
		numUnsafe        = 0;
		skipCascade      = 0;
		skipDuplicate    = 0;
		skipSize         = 0;
		skipUnsafe       = 0;
		truncated        = 0;
		truncatedName[0] = 0;
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
	 * Filter them out (by utilizing that `saveString()` does not order)
	 *
	 * example of unsafe components: `"ebcabc?!ad1!!"`
	 *   components are `"a"`, `"bcabc?"` and `"adbcabc?!!"`
	 *   `"adbcabc?!!"` is unsafe because it can be rewritten as `"cdab^!/bcad"`
	 *
	 * @param {member_t} pMember - Member to process
	 * @param {tinyTree_t} treeR - candidate tree
	 * @return {bool} - true for found, false to drop candidate
	 */
	bool /*__attribute__((optimize("O0")))*/ findHeadTail(member_t *pMember, const tinyTree_t &treeR) {

		assert(!(treeR.root & IBIT));

		// safe until proven otherwise
		pMember->flags |= member_t::MEMMASK_SAFE;

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

			pMember->tid  = 0;

			// root entries have no references
			pMember->Qmt = pMember->Tmt = pMember->Fmt = 0;

			for (unsigned j = 0; j < member_t::MAXHEAD; j++)
				pMember->heads[j] = 0;

			return true;
		}
		if (treeR.root == tinyTree_t::TINYTREE_KSTART) {
			assert(::strcmp(pMember->name, "a") == 0); // must be reserved name
			assert(pMember->sid == 2); // must be reserved entry

			pMember->tid  = 0;

			// root entries have no references
			pMember->Qmt = pMember->Tmt = pMember->Fmt = 0;

			for (unsigned j = 0; j < member_t::MAXHEAD; j++)
				pMember->heads[j] = 0;

			return true;
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
		tinyTree_t tree(ctx);
		tinyTree_t tree2(ctx);
		char skin[MAXSLOTS + 1];
		char name[tinyTree_t::TINYTREE_NAMELEN + 1];
		uint32_t Qmid = 0, Qtid = 0, Tmid = 0, Ttid = 0, Fmid = 0, Ftid = 0;

		{
			unsigned Q = treeR.N[treeR.root].Q;
			{
				// fast
				treeR.saveString(Q, name, skin);
				uint32_t ix = pStore->lookupMember(name);
				if (pStore->memberIndex[ix] == 0) {
					/*
					 * @date 2021-07-07 20:40:12
					 * Example: member "ab+bc^d2^", the T gets saved as "ab^dca+!" which is not normalised.
					 *          This is because it is using backlink "2" which is ordered within the context of a different skin.
					 *          Reload it so it should become "ab^dac+!".
					 */
					// slow
					treeR.saveString(Q, name, NULL); // save F which is not normalised
					tree2.loadStringSafe(name); // reload to normalise
					tree2.saveString(tree2.root, name, skin); // save with skin, byt dyadics are not normalised
					tree2.loadStringSafe(name); // reload to normalise
					tree2.saveString(tree2.root, name, NULL); // save again
					ix = pStore->lookupMember(name);
				}

				Qmid = pStore->memberIndex[ix];

				// member is unsafe if component not found or unsafe
				if (Qmid == 0 && opt_listLookup) {
					printf("%s\n", name);
					pMember->flags &= ~member_t::MEMMASK_SAFE;
				} else if (Qmid == 0 || (!(pStore->members[Qmid].flags & member_t::MEMMASK_SAFE))) {
					pMember->flags &= ~member_t::MEMMASK_SAFE;
					return false;
				}

				Qtid = pStore->lookupFwdTransform(skin);
			}

			unsigned Tu = treeR.N[treeR.root].T & ~IBIT;
			if (Tu != 0) {
				// fast
				treeR.saveString(Tu, name, skin);
				uint32_t ix = pStore->lookupMember(name);
				if (pStore->memberIndex[ix] == 0) {
					// slow
					treeR.saveString(Tu, name, NULL); // save F which is not normalised
					tree2.loadStringSafe(name); // reload to normalise
					tree2.saveString(tree2.root, name, skin); // save with skin, byt dyadics are not normalised
					tree2.loadStringSafe(name); // reload to normalise
					tree2.saveString(tree2.root, name, NULL); // save again
					ix = pStore->lookupMember(name);
				}

				Tmid = pStore->memberIndex[ix];

				// member is unsafe if component not found or unsafe
				if (Tmid == 0 && opt_listLookup) {
					printf("%s\n", name);
					pMember->flags &= ~member_t::MEMMASK_SAFE;
				} else if (Tmid == 0 || (!(pStore->members[Tmid].flags & member_t::MEMMASK_SAFE))) {
					pMember->flags &= ~member_t::MEMMASK_SAFE;
					return false;
				}

				Ttid = pStore->lookupFwdTransform(skin);
			}

			unsigned F = treeR.N[treeR.root].F;
			if (F != 0 && F != Tu) {
				// fast
				treeR.saveString(F, name, skin);
				uint32_t ix = pStore->lookupMember(name);
				if (pStore->memberIndex[ix] == 0) {
					// slow
					treeR.saveString(F, name, NULL); // save F which is not normalised
					tree2.loadStringSafe(name); // reload to normalise
					tree2.saveString(tree2.root, name, skin); // save with skin, byt dyadics are not normalised
					tree2.loadStringSafe(name); // reload to normalise
					tree2.saveString(tree2.root, name, NULL); // save again
					ix = pStore->lookupMember(name);
				}


				Fmid = pStore->memberIndex[ix];

				// member is unsafe if component not found or unsafe
				if (Fmid == 0 && opt_listLookup) {
					printf("%s\n", name);
					pMember->flags &= ~member_t::MEMMASK_SAFE;
				} else if (Fmid == 0 || (!(pStore->members[Fmid].flags & member_t::MEMMASK_SAFE))) {
					pMember->flags &= ~member_t::MEMMASK_SAFE;
					return false;
				}

				Ftid = pStore->lookupFwdTransform(skin);
			}
		}

		/*
		 * @date  2021-06-27 19:56:00
		 * Erase heads, they may contain random values
		 */
		for (unsigned j = 0; j < member_t::MAXHEAD; j++)
			pMember->heads[j] = 0;

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
			unsigned   numHead = 0; // number of unique found heads

			/*
			 * In turn, select each node to become "hot"
			 * Hot nodes are replaced with an endpoint placeholder
			 * Basically cutting of parts of the tree
			 */

			// replace `hot` node with placeholder
			for (unsigned iHead = tinyTree_t::TINYTREE_NSTART; iHead < treeR.root; iHead++) {
				unsigned select                     = 1 << treeR.root | 1 << 0; // selected nodes to extract nodes
				unsigned nextPlaceholderPlaceholder = tinyTree_t::TINYTREE_KSTART;
				uint32_t what[tinyTree_t::TINYTREE_NEND];
				what[0] = 0; // replacement for zero

				// scan tree for needed nodes, ignoring `hot` node
				for (unsigned k = treeR.root; k >= tinyTree_t::TINYTREE_NSTART; k--) {
					if (k != iHead && (select & (1 << k))) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q  = pNode->Q;
						const unsigned Tu = pNode->T & ~IBIT;
						const unsigned F  = pNode->F;

						if (Q >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << Q;
						if (Tu >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << Tu;
						if (F >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << F;
					}
				}

				// prepare for extraction
				tree.clearTree();
				// remove `hot` node from selection
				select &= ~(1 << iHead);

				/*
				 * Extract head.
				 * Replacing references by placeholders changes dyadic ordering.
				 * `what[hot]` is not a reference but a placeholder
				 */
				for (unsigned k = tinyTree_t::TINYTREE_NSTART; k <= treeR.root; k++) {
					if (k != iHead && select & (1 << k)) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q  = pNode->Q;
						const unsigned Tu = pNode->T & ~IBIT;
						const unsigned Ti = pNode->T & IBIT;
						const unsigned F  = pNode->F;

						// assign placeholder to endpoint or `hot`
						if (!(select & (1 << Q))) {
							what[Q] = nextPlaceholderPlaceholder++;
							select |= 1 << Q;
						}
						if (!(select & (1 << Tu))) {
							what[Tu] = nextPlaceholderPlaceholder++;
							select |= 1 << Tu;
						}
						if (!(select & (1 << F))) {
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
						 *  [ 8] a ? ~b : b                  "^" NE
						 *  [ 9] a ? ~b : c                  "!" QnTF
						 *  [16] a ?  b : 0                  "&" AND
						 *  [19] a ?  b : c                  "?" QTF
						 */

						// perform dyadic ordering
						if (Tu == 0 && Ti && tree.compare(what[Q], &tree, what[F], tinyTree_t::CASCADE_OR) > 0) {
							// reorder OR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (Tu == F && tree.compare(what[Q], &tree, what[F], tinyTree_t::CASCADE_NE) > 0) {
							// reorder XOR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = what[Q] ^ IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (F == 0 && !Ti && tree.compare(what[Q], &tree, what[Tu], tinyTree_t::CASCADE_AND) > 0) {
							// reorder AND
							tree.N[tree.count].Q = what[Tu];
							tree.N[tree.count].T = what[Q];
							tree.N[tree.count].F = 0;
						} else {
							// default
							tree.N[tree.count].Q = what[Q];
							tree.N[tree.count].T = what[Tu] ^ Ti;
							tree.N[tree.count].F = what[F];
						}

						tree.count++;
					}
				}

				// set root
				tree.root = tree.count - 1;

				/*
				 * @date 2021-06-14 18:56:37
				 *
				 * This doesn't got well for sid=221 "dab+c1&!"
				 * When replacing "ab+" with a placeholder the result is "dxcx&!"
				 * making the head effectively "cbab&!" instead of "caab&!".
				 * this also introduces a layer of transforms.
				 *
				 * This makes it clear that heads should not be used for structure creation
				 * and therefore be a sid/tid combo instead of a references to a template member
				 * This sadly adds 5-6 entryes to `member_t`.
				 *
				 * This change should be safe because the components have already been tested for validity.
				 */

				// fast path: lookup skin-free head name/notation
				tree.saveString(tree.root, name, skin);
				uint32_t ix = pStore->lookupMember(name);
				if (pStore->memberIndex[ix] == 0) {
					/*
					 * @date 2021-06-18 21:29:50
					 *
					 * NOTE/WARNING the extracted component may have non-normalised dyadic ordering
					 * because in the context of the original trees, the endpoints were locked by the now removed node
					 *
					 * @date 2021-07-25 00:18:44
					 * Originally this part would call `SwapNameSKin()` which is highly expensive
					 *
					 */
					tree2.loadStringSafe(name);
					// structure is now okay
					tree2.saveString(tree2.root, name, skin);
					// endpoints are now okay
					tree2.loadStringSafe(name); // reload to normalise
					tree2.saveString(tree2.root, name, NULL); // save again

					ix = pStore->lookupMember(name);
				}
				unsigned midHead = pStore->memberIndex[ix];

				if (midHead == 0 && opt_listLookup) {
					// component not found
					printf("%s\n", name);
					pMember->flags &= ~member_t::MEMMASK_SAFE;
				} else if (midHead == 0 || !(pStore->members[midHead].flags & member_t::MEMMASK_SAFE)) {
					// component unsafe
					pMember->flags &= ~member_t::MEMMASK_SAFE;
					return false;
				}

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

		/*
		 * @date 2021-08-02 14:13:43
		 *
		 * Only when all ok lookup/add pairs
		 */

		if (!Qmid) {
			pMember->Qmt = 0;
		} else {
			// convert mid/tid to pair
			uint32_t ix = pStore->lookupPair(Qmid, Qtid);
			if (pStore->pairIndex[ix] == 0 && !readOnlyMode) {
				// new
				pStore->pairIndex[ix] = pStore->addPair(Qmid, Qtid);
			}
			pMember->Qmt = pStore->pairIndex[ix];
		}

		if (!Tmid) {
			pMember->Tmt = 0;
		} else {
			// convert mid/tid to pair
			uint32_t ix = pStore->lookupPair(Tmid, Ttid);
			if (pStore->pairIndex[ix] == 0 && !readOnlyMode) {
				// new
				pStore->pairIndex[ix] = pStore->addPair(Tmid, Ttid);
			}
			pMember->Tmt = pStore->pairIndex[ix];
		}

		if (!Fmid) {
			pMember->Fmt = 0;
		} else {
			// convert mid/tid to pair
			uint32_t ix = pStore->lookupPair(Fmid, Ftid);
			if (pStore->pairIndex[ix] == 0 && !readOnlyMode) {
				// new
				pStore->pairIndex[ix] = pStore->addPair(Fmid, Ftid);
			}
			pMember->Fmt = pStore->pairIndex[ix];
		}

		if (ctx.flags & context_t::MAGICMASK_PARANOID) {
			unsigned iMid = pMember - pStore->members;

			assert(pMember->Qmt == 0 || pStore->pairs[pMember->Qmt].id < iMid);
			assert(pMember->Tmt == 0 || pStore->pairs[pMember->Tmt].id < iMid);
			assert(pMember->Fmt == 0 || pStore->pairs[pMember->Fmt].id < iMid);

			for (unsigned k = 0; k < member_t::MAXHEAD; k++)
				assert(pMember->heads[k] == 0 || pMember->heads[k] < iMid);
		}

		return true;
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
			pMember        = pStore->members + mid;
			freeMemberRoot = pMember->nextMember; // pop from free list
			::strcpy(pMember->name, pName); // populate with name
		} else {
			mid     = pStore->addMember(pName); // allocate new member
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
	bool /*__attribute__((optimize("O0")))*/ foundTreeMember(tinyTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {

		if (this->truncated)
			return false; // quit as fast as possible

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numPair=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipCascade=%u | hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond,
					pStore->numPair, pStore->numPair * 100.0 / pStore->maxPair,
					pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
					numEmpty, numUnsafe,
					skipDuplicate, skipSize, skipUnsafe, skipCascade, (double) ctx.cntCompare / ctx.cntHash, pNameR);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numPair=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipCascade=%u | hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - generator.windowLo) * 100.0 / (ctx.progressHi - generator.windowLo), etaH, etaM, etaS,
					pStore->numPair, pStore->numPair * 100.0 / pStore->maxPair,
					pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
					numEmpty, numUnsafe,
					skipDuplicate, skipSize, skipUnsafe, skipCascade, (double) ctx.cntCompare / ctx.cntHash, pNameR);
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

		uint32_t mix = pStore->lookupMember(pNameR);
		if (pStore->memberIndex[mix] != 0) {
			// duplicate candidate name
			skipDuplicate++;
			return true;
		}

		/*
		 * Test for database overflow
		 */
		if (this->opt_truncate) {
			// avoid `"storage full"`. Give warning later
			if (pStore->maxPair - pStore->numPair <= 3 || pStore->maxMember - pStore->numMember <= 1) {
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
		/*
		 * test or cascading dyadics (cascaded operators may not fork)
		 *
		 * @date 2021-08-30 10:15:53
		 * Disabled because cascades are now integrated
		 */
		if (opt_cascade) {
			for (unsigned k = tinyTree_t::TINYTREE_NSTART; k < treeR.count; k++) {
				const tinyNode_t *pNode = treeR.N + k;
				uint32_t Q = pNode->Q;
				uint32_t T = pNode->T;
				uint32_t F = pNode->F;

				// OR
				if (pNode->isOR() && treeR.isOR(F)) {
					skipCascade++;
					return true;
				}

				// NE
				if (pNode->isNE() && treeR.isNE(F)) {
					skipCascade++;
					return true;
				}

				// AND
				if (pNode->isAND() && treeR.isAND(T)) {
					skipCascade++;
					return true;
				}
			}
		}
#endif

		/*
		 * Find the matching signature group. It's layout only so ignore transformId.
		 */

		unsigned sid     = 0;
		unsigned tid     = 0;
		unsigned markSid = pStore->numSignature;

		if ((ctx.flags & context_t::MAGICMASK_AINF) && !this->readOnlyMode) {
			/*
			 * @date 2020-04-25 22:00:29
			 *
			 * WARNING: add-if-not-found only checks tid=0 to determine if (not-)found.
			 *          This creates false-positives.
			 *          Great for high-speed loading, but not for perfect duplicate detection.
			 *          To get better results, re-run with next increment interleave.
			 */
			// add to imprints to index
			sid = pStore->addImprintAssociative(&treeR, pStore->fwdEvaluator, pStore->revEvaluator, markSid);
		} else {
			pStore->lookupImprintAssociative(&treeR, pStore->fwdEvaluator, pStore->revEvaluator, &sid, &tid);
		}

		if (sid == 0)
			return true; // not found

		/*
		 * @date 2021-09-22 15:22:35
		 * Generator also creates patterns used by detectors.
		 * Is structure what is says it is?
		 * This test is expensive, do after determining it is a viable candidate (i.e. sid != 0) 
		 */
		treeR.loadStringSafe(pNameR);
		if (strcmp(treeR.saveString(treeR.root), pNameR) != 0) {
			// can also indicate folding because of `--pure` or `--cascade`
			return true;
		}


		signature_t *pSignature = pStore->signatures + sid;
		unsigned cmp = 0;

		/*
		 * early-reject
		 */

		if (numPlaceholder > pSignature->numPlaceholder) {
			/*
			 * @date 2021-07-31 17:33:50
			 * Don't add unnecessary placeholders
			 */
			cmp = '*'; // reject
		} else if (pSignature->flags & signature_t::SIGMASK_SAFE) {
			/*
			 * @date 2021-06-20 19:06:44
			 * Just like primes with component dependency chains, members can be larger than signatures
			 * Larger candidates will always be rejected, so reject now before doing expensive testing
			 * Grouping can be either by node size or score
			 */
			if (treeR.count - tinyTree_t::TINYTREE_NSTART > pSafeSize[sid]) {
				cmp = '-'; // reject
			}
		}

		if (cmp) {
			if (opt_text == OPTTEXT_COMPARE)
				printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", ctx.progress, sid, cmp, pNameR, treeR.count - tinyTree_t::TINYTREE_NSTART, numPlaceholder, numEndpoint, numBackRef);
			skipSize++;
			return true;
		}

		/*
		 * @date 2021-07-24 11:52:14
		 * Revalidate name if signature has swaps. Do so there because it's expensive
		 */
		if (pSignature->swapId != 0) {
			// ugh, pNameR is const, adding `strcpy()` to an expensive path would be hardly noticeable
			char tmpName[tinyTree_t::TINYTREE_NAMELEN + 1];
			strcpy(tmpName, pNameR);

			if (pStore->normaliseNameSkin(tmpName, NULL, pSignature)) {
				// name changed, reject
				skipSize++;
				return true;
			}
		}

		/*
		 * Determine if safe when heads/tails are all safe
		 * NOTE: need temporary storage because database member section might be readOnly
		 */

		member_t tmpMember;
		::memset(&tmpMember, 0, sizeof(tmpMember));

		/*
		 * @date 2021-07-12 21:25:02
		 * `sid`/`tid` == `pNameR`
		 *
		 * However, in the member table, it is intended to be `sid`/`tid` == `pNameR`
		 */
		::strcpy(tmpMember.name, pNameR);
		tmpMember.sid            = sid;
		tmpMember.tid            = tid;
		tmpMember.size           = treeR.count - tinyTree_t::TINYTREE_NSTART;
		tmpMember.numPlaceholder = numPlaceholder;
		tmpMember.numEndpoint    = numEndpoint;
		tmpMember.numBackRef     = numBackRef;


		bool found = findHeadTail(&tmpMember, treeR);
		if (!found)
			found = false; // for debugger breakpoint

		if (!found && opt_safe) {
			skipUnsafe++;
			return true;
		}

		/*
		 * Verify if candidate member is acceptable
		 */

		if (pSignature->firstMember == 0) {
			// group is empty, this is first member
			numEmpty--;
			if (tmpMember.flags & member_t::MEMMASK_SAFE) {
				// group is empty, candidate is safe. Accept
				cmp = '>';
			} else {
				// group is empty, candidate is unsafe. Accept.
				cmp = '=';
				numUnsafe++;
			}
		} else if (pSignature->flags & signature_t::SIGMASK_SAFE) {
			if (!(tmpMember.flags & member_t::MEMMASK_SAFE)) {
				// group is safe, candidate not. Reject
				cmp = '<';
				skipUnsafe++;
			} else {
				// group and candidate both safe. Accept
				cmp = '+';
			}
		} else {
			if (tmpMember.flags & member_t::MEMMASK_SAFE) {
				// group is unsafe, candidate is safe. Accept
				cmp = '>';
				numUnsafe--;
			} else {
				// group and candidate both unsafe. Accept.
				cmp = '=';
			}
		}

		if (opt_text == OPTTEXT_COMPARE)
			printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", ctx.progress, tmpMember.sid, cmp, tmpMember.name, tmpMember.size, tmpMember.numPlaceholder, tmpMember.numEndpoint, tmpMember.numBackRef);

		if (cmp == '<' || cmp == '-')
			return true;  // lost challenge

		// won challenge
		if (opt_text == OPTTEXT_WON)
			printf("%s\n", pNameR);

		if (cmp == '>') {
			/*
			 * group changes from unsafe to save, or safe group flush: remove all (unsafe) members
			 */

			if (pSignature->firstMember) {
				// remove all unsafe members

				if (this->readOnlyMode) {
					// member chain cannot be modified
					// pretend signature becomes safe or keeps unsafe members
					pSignature->firstMember = 0;
				} else {
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
					 *
					 * to reduce multiple sweeps of members:
					 * Flag members to be released first,
					 * then loop once through all members to undo references
					 * then release flagged
					 */

					for (unsigned iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
						assert(!(pStore->members[iMid].flags & member_t::MEMMASK_SAFE));
						pStore->members[iMid].flags |= member_t::MEMMASK_DELETE;
					}

					// remove all references to the deleted
					for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
						member_t *p = pStore->members + iMid;

						if (pStore->members[pStore->pairs[p->Qmt].id].flags & member_t::MEMMASK_DELETE) {
							assert(!(p->flags & member_t::MEMMASK_SAFE));
							p->Qmt = 0;
						}
						if (pStore->members[pStore->pairs[p->Tmt].id].flags & member_t::MEMMASK_DELETE) {
							assert(!(p->flags & member_t::MEMMASK_SAFE));
							p->Tmt = 0;
						}
						if (pStore->members[pStore->pairs[p->Fmt].id].flags & member_t::MEMMASK_DELETE) {
							assert(!(p->flags & member_t::MEMMASK_SAFE));
							p->Fmt = 0;
						}
					}

					// release deleted
					while (pSignature->firstMember) {
						// release head of chain
						member_t *p = pStore->members + pSignature->firstMember;

						pSignature->firstMember = p->nextMember;

						this->memberFree(p);
					}
				}
			}

			// mark group safe
			pSignature->flags |= signature_t::SIGMASK_SAFE;

		}

		/*
		 * promote candidate to member
		 */

		if (this->readOnlyMode != 0) {
			// link a fake member to mark non-empty
			pSignature->firstMember = 1;
		} else {
			// allocate
			member_t *pMember = this->memberAlloc(pNameR);

			// populate
			*pMember = tmpMember;

			// link
			pMember->nextMember     = pSignature->firstMember;
			pSignature->firstMember = pMember - pStore->members;

			// index
			pStore->memberIndex[mix] = pMember - pStore->members;
		}

		/*
		 * update global score
		 */
		pSafeSize[sid] = treeR.count - tinyTree_t::TINYTREE_NSTART;

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
		context_t      *pApp     = static_cast<context_t *>(arg);

		int cmp = 0;

		/*
		 * depreciates go last
		 */
		if ((pMemberL->flags & member_t::MEMMASK_DEPR) && !(pMemberR->flags & member_t::MEMMASK_DEPR))
			return +1;
		if (!(pMemberL->flags & member_t::MEMMASK_DEPR) && (pMemberR->flags & member_t::MEMMASK_DEPR))
			return -1;

		/*
		 * compare scores
		 */

		unsigned scoreL = tinyTree_t::calcScoreName(pMemberL->name);
		unsigned scoreR = tinyTree_t::calcScoreName(pMemberR->name);

		cmp = (int)scoreL - (int)scoreR;
		if (cmp)
			return cmp;

		/*
		 * Compare trees
		 */

		// load trees
		tinyTree_t treeL(*pApp);
		tinyTree_t treeR(*pApp);

		treeL.loadStringFast(pMemberL->name);
		treeR.loadStringFast(pMemberR->name);

		cmp = treeL.compare(treeL.root, &treeR, treeR.root);
		return cmp;
	}

	/**
	 * @date 2020-04-02 21:52:34
	 */
	void rebuildImprints(unsigned unsafeOnly) {
		// clear signature and imprint index
		::memset(pStore->imprintIndex, 0, pStore->imprintIndexSize * sizeof(*pStore->imprintIndex));

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

		tinyTree_t tree(ctx);

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
						numEmpty, numUnsafe, (double) ctx.cntCompare / ctx.cntHash);
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
						numEmpty, numUnsafe, (double) ctx.cntCompare / ctx.cntHash);
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

			if (!unsafeOnly || !(pSignature->flags & signature_t::SIGMASK_SAFE)) {
				// avoid `"storage full"`. Give warning later
				if (pStore->maxImprint - pStore->numImprint <= pStore->interleave && opt_sidHi == 0 && this->opt_truncate) {
					// break now, display text later/ Leave progress untouched
					assert(iSid == ctx.progress);
					break;
				}

				tree.loadStringFast(pSignature->name);

				unsigned sid, tid;

				if (!pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sid, &tid))
					pStore->addImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, iSid);
			}

			// stats
			if (pSignature->firstMember == 0)
				numEmpty++;
			else if (!(pSignature->flags & signature_t::SIGMASK_SAFE))
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
				numEmpty, numUnsafe, (double) ctx.cntCompare / ctx.cntHash);
	}

	/**
	 * @date 2020-04-20 19:57:08
	 *
	 * Compare function for `qsort_r`
	 *
	 * Compare two hints.
	 * Do not compare them directly, but use the arguments as index to `database_t::hints[]`.
	 *
	 * @param {signature_t} lhs - left hand side hint index
	 * @param {signature_t} rhs - right hand side hint index
	 * @param {genhintContext_t} arg - Application context
	 * @return "<0" if "L<R", "0" if "L==R", ">0" if "L>R"
	 */
	static int comparHint(const void *lhs, const void *rhs, void *arg) {
		if (lhs == rhs)
			return 0;

		genmemberContext_t *pApp        = static_cast<genmemberContext_t *>(arg);
		// Arguments are signature offsets
		const signature_t  *pSignatureL = pApp->pStore->signatures + *(unsigned *) lhs;
		const signature_t  *pSignatureR = pApp->pStore->signatures + *(unsigned *) rhs;
		const hint_t       *pHintL      = pApp->pStore->hints + pSignatureL->hintId;
		const hint_t       *pHintR      = pApp->pStore->hints + pSignatureR->hintId;

		int cmp;

		// first compare active index (lowest first)
		cmp = pHintL->numStored[pApp->activeHintIndex] - pHintR->numStored[pApp->activeHintIndex];
		if (cmp)
			return cmp;

		// then compare inactive indices (highest first)
		for (unsigned j = 0; j < hint_t::MAXENTRY; j++) {
			if (j != pApp->activeHintIndex) {
				cmp = pHintR->numStored[j] - pHintL->numStored[j];
				if (cmp)
					return cmp;
			}
		}

		// identical
		return 0;
	}

	/**
	 * @date 2020-05-02 18:40:01
	 */
	void rebuildImprintsWithHints(void) {
		assert(pStore->numHint >= 2);

		// clear signature and imprint index
		::memset(pStore->imprintIndex, 0, pStore->imprintIndexSize * sizeof(*pStore->imprintIndex));

		if (pStore->numSignature < 2)
			return; //nothing to do

		// skip reserved entry
		pStore->numImprint = 1;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS) {
			fprintf(stderr, "[%s] Rebuilding imprints with hints\n", ctx.timeAsString());
		}

		/*
		 * Create ordered vector to hints
		 */

		unsigned *pHintMap = (unsigned *) ctx.myAlloc("pHintMap", pStore->maxSignature, sizeof(*pHintMap));

		// locate which hint index
		this->activeHintIndex = 0;
		for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {
			if (pInterleave->numStored == pStore->interleave) {
				this->activeHintIndex = pInterleave - metricsInterleave;
				break;
			}
		}

		// fill map with offsets to signatures
		unsigned      numHint = 0;
		for (unsigned iSid    = 1; iSid < pStore->numSignature; iSid++) {
			const signature_t *pSignature = pStore->signatures + iSid;

			if (!(pSignature->flags & signature_t::SIGMASK_SAFE))
				pHintMap[numHint++] = iSid;

		}

		// sort entries.
		qsort_r(pHintMap, numHint, sizeof(*pHintMap), comparHint, this);

		/*
		 * Create imprints for signature groups
		 */

		tinyTree_t tree(ctx);

		// reset ticker
		ctx.setupSpeed(numHint);
		ctx.tick = 0;

		// re-calculate
		numEmpty = numUnsafe = 0;

		// create imprints for signature groups
		for (unsigned iHint = 0; iHint < numHint; iHint++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				if (perSecond == 0 || ctx.progress > ctx.progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numImprint=%u(%.0f%%) numEmpty=%u numUnsafe=%u | hash=%.3f",
						ctx.timeAsString(), ctx.progress, perSecond,
						pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
						numEmpty, numUnsafe, (double) ctx.cntCompare / ctx.cntHash);
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
						numEmpty, numUnsafe, (double) ctx.cntCompare / ctx.cntHash);
				}

				ctx.tick = 0;
			}

			// get signature
			unsigned          iSid        = pHintMap[iHint];
			const signature_t *pSignature = pStore->signatures + iSid;

			/*
			 * Add to imprint index, either all or empty/unsafe only
			 */

			if (!(pSignature->flags & signature_t::SIGMASK_SAFE)) {
				// avoid `"storage full"`. Give warning later
				if (pStore->maxImprint - pStore->numImprint <= pStore->interleave && opt_sidHi == 0) {
					// break now, display text later/ Leave progress untouched
					assert(iHint == ctx.progress);
					break;
				}

				tree.loadStringFast(pSignature->name);

				unsigned sid = 0, tid;

				if (!pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &sid, &tid))
					pStore->addImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, iSid);
			}

			// stats
			if (pSignature->firstMember == 0)
				numEmpty++;
			else if (!(pSignature->flags & signature_t::SIGMASK_SAFE))
				numUnsafe++;

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.progress != ctx.progressHi && opt_sidHi == 0) {
			fprintf(stderr, "[%s] WARNING: Imprint storage full. Truncating at %u \"%s\"\n",
				ctx.timeAsString(), (unsigned) ctx.progress, pStore->signatures[ctx.progress].name);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Created imprints. numImprint=%u(%.0f%%) numEmpty=%u numUnsafe=%u | hash=%.3f\n",
				ctx.timeAsString(),
				pStore->numImprint, pStore->numImprint * 100.0 / pStore->maxImprint,
				numEmpty, numUnsafe, (double) ctx.cntCompare / ctx.cntHash);

		ctx.myFree("pSignatureIndex", pHintMap);
	}

	/**
	 * @date 2020-03-22 01:00:05
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add newly unique signatures to the database
	 */
	void /*__attribute__((optimize("O0")))*/ membersFromFile(void) {

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading members from file\n", ctx.timeAsString());

		FILE *f = fopen(this->opt_load, "r");
		if (f == NULL)
			ctx.fatal("\n{\"error\":\"fopen('%s') failed\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n",
				  this->opt_load, __FUNCTION__, __FILE__, __LINE__);

		// apply settings for `--window`
		generator.windowLo = this->opt_windowLo;
		generator.windowHi = this->opt_windowHi;

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;
		skipDuplicate = skipSize = skipUnsafe = skipCascade = 0;

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
			 * call `foundTreeMember()`
			 */

			if (!foundTreeMember(tree, name, newPlaceholder, newEndpoint, newBackRef))
				break;

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (truncated) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: Pair/Member storage full. Truncating at progress=%lu \"%s\"\n",
					ctx.timeAsString(), this->truncated, this->truncatedName);

			// save position for final status
			this->opt_windowHi = this->truncated;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Read %lu members. numSignature=%u(%.0f%%) numPair=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipCascade=%u\n",
				ctx.timeAsString(),
				ctx.progress,
				pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				pStore->numPair, pStore->numPair * 100.0 / pStore->maxPair,
				pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				numEmpty, numUnsafe,
				skipDuplicate, skipSize, skipUnsafe, skipCascade);
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
		skipDuplicate = skipSize = skipUnsafe = skipCascade = 0;

		/*
		 * Generate candidates
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", ctx.timeAsString(), arg_numNodes, MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE ? "-pure" : "");

		if (arg_numNodes == 0) {
			tinyTree_t tree(ctx);

			tree.root = 0; // "0"
			foundTreeMember(tree, "0", 0, 0, 0);
			tree.root = 1; // "a"
			foundTreeMember(tree, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = arg_numNodes * 2 + 1;

			generator.initialiseGenerator();
			generator.clearGenerator();
			generator.generateTrees(arg_numNodes, endpointsLeft, 0, 0, this, static_cast<generator_t::generateTreeCallback_t>(&genmemberContext_t::foundTreeMember));
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
				fprintf(stderr, "[%s] WARNING: Pair/Member storage full. Truncating at progress=%lu \"%s\"\n",
					ctx.timeAsString(), this->truncated, this->truncatedName);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%u pure=%u numNode=%u numCandidate=%lu numPair=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipCascade=%u\n",
				ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, arg_numNodes, ctx.progress,
				pStore->numPair, pStore->numPair * 100.0 / pStore->maxPair,
				pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				numEmpty, numUnsafe,
				skipDuplicate, skipSize, skipUnsafe, skipCascade);
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
	void /*__attribute__((optimize("O0")))*/ finaliseMembers(void) {
		tinyTree_t tree(ctx);

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Sorting members\n", ctx.timeAsString());

		/*
		 * @date 2021-07-25 17:26:25
		 * cannot `assert(mid < iMid)` here because reusing released members disrupts that ordering
		 * test it later
		 */

		/*
		 * Compress members before sorting
		 */

		unsigned lastMember = pStore->numMember;
		pStore->numMember = 1;

		for (unsigned iMid = 1; iMid < lastMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (pMember->flags & member_t::MEMMASK_DELETE)
				continue; // explicit delete
			if (pMember->sid == 0)
				continue; // implicit delete
			if (pMember->flags & member_t::MEMMASK_DEPR)
				continue; // depreciated

			// save
			pStore->members[pStore->numMember++] = *pMember;
		}

		// clear pair section
		pStore->numPair = 1;
		::memset(pStore->pairIndex, 0, pStore->pairIndexSize * sizeof(*pStore->pairIndex));

		// clear member index and linked-list, mark signatures unsafe
		::memset(pStore->memberIndex, 0, pStore->memberIndexSize * sizeof(*pStore->memberIndex));
		for (unsigned iSid = 0; iSid < pStore->numSignature; iSid++) {
			pStore->signatures[iSid].firstMember = 0;
			pStore->signatures[iSid].flags &= ~signature_t::SIGMASK_SAFE;
		}
		skipDuplicate = skipSize = skipUnsafe = skipCascade = 0;

		// sort entries (skipping first)
		assert(pStore->numMember >= 1);
		qsort_r(pStore->members + 1, pStore->numMember - 1, sizeof(*pStore->members), comparMember, this);

		// lower lastMember, skipping all the deleted
		while (pStore->numMember > 1 && pStore->members[pStore->numMember - 1].sid == 0)
			--pStore->numMember;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Indexing members\n", ctx.timeAsString());

		/*
		 * rebuild references
		 */

		ctx.setupSpeed(pStore->numMember);
		ctx.tick = 0;

		ctx.progress++; // skip reserved
		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;
			signature_t *pSignature = pStore->signatures + pMember->sid;

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				if (perSecond == 0 || ctx.progress > ctx.progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numMember=%u skipUnsafe=%u | hash=%.3f %s",
						ctx.timeAsString(), ctx.progress, perSecond, pStore->numMember, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash, pMember->name);
				} else {
					int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numMember=%u skipUnsafe=%u | hash=%.3f %s",
						ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, pStore->numMember, skipUnsafe, (double) ctx.cntCompare / ctx.cntHash, pMember->name);
				}

				ctx.tick = 0;
			}

			assert(pMember->sid);

			// calculate head/tail
			tree.loadStringFast(pMember->name);
			bool wasSafe = pMember->flags & member_t::MEMMASK_SAFE;
			bool isSafe = findHeadTail(pMember, tree);

			// safe member must remain safe
			assert(!wasSafe || isSafe);

			/*
			 * member should be unsafe
			 *
			 * @date 2021-06-23 09:46:35
			 *
			 * assert will fail when reading members from a list that is not properly ordered.
			 * and the list contains primes that are longer than the signatures.
			 * this will signatures to reject seeing primes as safe on the first pass.
			 * For the moment this is experimental code, so issue a warning instead of aborting a lengthy run
			 * (input list was ordered by sid instead of mid)
			 */
			if (pSignature->firstMember == 0) {
				if (pMember->flags & member_t::MEMMASK_SAFE) {
					// first member safe, then signature safe
					pSignature->flags |= signature_t::SIGMASK_SAFE;
				} else {
					// first member unsafe, then signature unsafe
				}
			} else if ((pMember->flags & member_t::MEMMASK_SAFE) && (pSignature->flags & signature_t::SIGMASK_SAFE)) {
				// adding safe members to safe signature
			} else if (!(pMember->flags & member_t::MEMMASK_SAFE) && !(pSignature->flags & signature_t::SIGMASK_SAFE)) {
				// adding unsafe members to unsafe signature
			} else if ((pMember->flags & member_t::MEMMASK_SAFE) && !(pSignature->flags & signature_t::SIGMASK_SAFE)) {
				// adding safe members to unsafe signature
				fprintf(stderr,"\r\e[K[%s] WARNING: Adding safe member %u:%s to unsafe signature %u:%s\n", ctx.timeAsString(), iMid, pMember->name, pMember->sid, pSignature->name);
				pSignature->flags |= signature_t::SIGMASK_SAFE;
			} else {
				/*
				 * Reject adding unsafe member to safe group
				 */
				skipUnsafe++;
				ctx.progress++;
				continue;
			}

			// add to index
			uint32_t ix = pStore->lookupMember(pMember->name);
			assert(pStore->memberIndex[ix] == 0);
			pStore->memberIndex[ix] = iMid;

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * String all the members to signatures, best one is first in list
		 */
		for (unsigned iMid = pStore->numMember - 1; iMid >= 1; --iMid) {
			member_t *pMember = pStore->members + iMid;
			signature_t *pSignature = pStore->signatures + pMember->sid;

			// add to group
			pMember->nextMember     = pSignature->firstMember;
			pSignature->firstMember = iMid;
		}

		/*
		 * Be paranoid
		 */


		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			if (!(pStore->signatures[iSid].flags & signature_t::SIGMASK_SAFE)) {
				for (unsigned iMid = pStore->signatures[iSid].firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
					assert(!(pStore->members[iMid].flags & member_t::MEMMASK_SAFE));
				}
			}
		}

		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (pMember->flags & member_t::MEMMASK_SAFE) {
				if (pMember->Qmt) {
					unsigned m = pStore->pairs[pMember->Qmt].id;
					assert(m < iMid);
					assert(pStore->members[m].sid != 0);
				}
				if (pMember->Tmt) {
					unsigned m = pStore->pairs[pMember->Tmt].id;
					assert(m < iMid);
					assert(pStore->members[m].sid != 0);
				}
				if (pMember->Fmt) {
					unsigned m = pStore->pairs[pMember->Fmt].id;
					assert(m < iMid);
					assert(pStore->members[m].sid != 0);
				}
				for (unsigned k = 0; k < member_t::MAXHEAD; k++) {
					if (pMember->heads[k]) {
						assert(pMember->heads[k] < iMid);
						assert(pStore->members[pMember->heads[k]].sid != 0);
					}
				}
			}
		}

		/*
		 * Flag component members
		 */

		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			pMember->flags &= ~member_t::MEMMASK_COMP;

			if (pMember->flags & member_t::MEMMASK_SAFE) {
				if (pMember->Qmt)
					pStore->members[pStore->pairs[pMember->Qmt].id].flags |= member_t::MEMMASK_COMP;
				if (pMember->Tmt)
					pStore->members[pStore->pairs[pMember->Tmt].id].flags |= member_t::MEMMASK_COMP;
				if (pMember->Fmt)
					pStore->members[pStore->pairs[pMember->Fmt].id].flags |= member_t::MEMMASK_COMP;

				for (unsigned k = 0; k < member_t::MAXHEAD; k++) {
					if (pMember->heads[k])
						pStore->members[pMember->heads[k]].flags |= member_t::MEMMASK_COMP;

				}
			}
		}

		/*
		 * Recalculate empty/unsafe groups
		 */

		unsigned numIncomplete = 0;

		numEmpty = numUnsafe = 0;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			signature_t *pSignature = pStore->signatures + iSid;

			if (pSignature->firstMember == 0)
				numEmpty++;
			else if (!(pSignature->flags & signature_t::SIGMASK_SAFE))
				numUnsafe++;
			if ((pSignature->flags & signature_t::SIGMASK_KEY) && !(pSignature->flags & signature_t::SIGMASK_SAFE))
				numIncomplete++;
		}

		/*
		 * Done
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] {\"numSlot\":%u,\"pure\":%u,\"interleave\":%u,\"numNode\":%u,\"numImprint\":%u,\"numSignature\":%u,\"numPair\":%u,\"numMember\":%u,\"numEmpty\":%u,\"numUnsafe\":%u}\n",
				ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, pStore->interleave, arg_numNodes, pStore->numImprint, pStore->numSignature, pStore->numPair, pStore->numMember, numEmpty, numUnsafe);

		if (numEmpty || numUnsafe || numIncomplete) {
			if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
				fprintf(stderr, "[%s] WARNING: %u empty, %u unsafe and %u incomplete signature groups\n", ctx.timeAsString(), numEmpty, numUnsafe, numIncomplete);
		}
	}

	/*
	 * @date 2021-07-31 19:44:50
	 *
	 * Experimental generator that creates new safe members based on already existing safe members.
	 * Instead on using safe components(tails) it used a safe head and adds a node in all possible combinations.
	 * This is an attempt for find missing members in 6n9/7n9 space.
	 * Metrics don't really match and a `Assertion `!wasSafe || isSafe'` happened with `../genmember m-3n9.db 4 m-4n9.db --pure`
	 *
	 * task/window selection not yet supported.
	 *
	 */
	void membersFromAltGenerator() {

		if (opt_taskId || opt_taskLast || opt_windowLo || opt_windowHi)
			ctx.fatal("--task and --window not supported in combination with --altgen\n");

		// determine loop size
		unsigned numTest = 0, numTree = 0;

		for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			/*
			 * member must be safe and right size
			 */
			if (!(pMember->flags & member_t::MEMMASK_SAFE))
				continue;
			if (pMember->size != arg_numNodes - 1)
				continue;

			numTree++;

			/*
			 * Generate first node
			 */

			// @formatter:off
			for (uint32_t F = 0; F < tinyTree_t::TINYTREE_NSTART; F++)
			for (uint32_t Ti = 1; Ti < 2; Ti++)
			for (uint32_t Tu = 0; Tu < tinyTree_t::TINYTREE_NSTART; Tu++)
			for (uint32_t Q = 0; Q < tinyTree_t::TINYTREE_NSTART; Q++) {
			// @formatter:on

				// test if combo is normalised
				if (Q == Tu)
					continue;  // Q?Q:F or Q?~Q:F
				if (Q == F)
					continue; // Q?T:Q or Q?~T:Q
				if (Q == 0)
					continue; // 0?X:Y
				if (Tu == F && F == 0)
					continue; // Q?~0:0
				if (Tu == F && !Ti)
					continue; // "SELF" Q?F:F
				if (Tu == 0 && !Ti)
					continue; // "LT" Q?0:F -> F?~Q:0

				numTest++;
			}
		}

		fprintf(stderr, "numTree=%u, numTest=%u\n", numTree, numTest);
		// reset ticker
		ctx.setupSpeed(numTest);
		ctx.tick = 0;

		/*
		 * Main loop
		 */

		tinyTree_t treeR(ctx);

		for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			/*
			 * member must be safe and right size
			 */
			if (!(pMember->flags & member_t::MEMMASK_SAFE))
				continue;
			if (pMember->size != arg_numNodes - 1)
				continue;

			/*
			 * Generate first node
			 */

			// @formatter:off
			for (uint32_t F = 0; F < tinyTree_t::TINYTREE_NSTART; F++)
			for (uint32_t Ti = 1; Ti < 2; Ti++)
			for (uint32_t Tu = 0; Tu < tinyTree_t::TINYTREE_NSTART; Tu++)
			for (uint32_t Q = 0; Q < tinyTree_t::TINYTREE_NSTART; Q++) {
			// @formatter:on

				// test if combo is normalised
				if (Q == Tu)
					continue;  // Q?Q:F or Q?~Q:F
				if (Q == F)
					continue; // Q?T:Q or Q?~T:Q
				if (Q == 0)
					continue; // 0?X:Y
				if (Tu == F && F == 0)
					continue; // Q?~0:0
				if (Tu == F && !Ti)
					continue; // "SELF" Q?F:F
				if (Tu == 0 && !Ti)
					continue; // "LT" Q?0:F -> F?~Q:0

				ctx.progress++;

				/*
				 * Create a mask bases of number of endpoints
				 */
				unsigned maskHi = 1 << pMember->numEndpoint;

				for (unsigned mask = 1; mask < maskHi; mask++) {

					// add new node as first
					treeR.clearTree();
					treeR.root = treeR.addNormaliseNode(Q, Ti ? Tu ^ IBIT : Tu, F);
					assert(treeR.root == tinyTree_t::TINYTREE_NSTART);

					/*
					 * Inject member with substituted endpoints on top of this
					 */
					{
						// state storage for postfix notation
						uint32_t stack[tinyTree_t::TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
						int      stackPos    = 0;
						uint32_t beenThere[tinyTree_t::TINYTREE_NEND]; // track id's of display operators.
						unsigned nextNode    = tinyTree_t::TINYTREE_NSTART; // next visual node
						unsigned numEndpoint = 0;

						// walk through the notation until end or until placeholder/skin separator
						for (const char *pCh = pMember->name; *pCh; pCh++) {

							assert(!isalnum(*pCh) || stackPos < tinyTree_t::TINYTREE_MAXSTACK);
							assert(isalnum(*pCh) || treeR.count < tinyTree_t::TINYTREE_NEND);

							switch (*pCh) {
							case '0':
								stack[stackPos++] = 0;
								break;
							case 'a':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 0);
								break;
							case 'b':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 1);
								break;
							case 'c':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 2);
								break;
							case 'd':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 3);
								break;
							case 'e':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 4);
								break;
							case 'f':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 5);
								break;
							case 'g':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 6);
								break;
							case 'h':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 7);
								break;
							case 'i':
								if (mask & (1 << numEndpoint++))
									stack[stackPos++] = tinyTree_t::TINYTREE_NSTART;
								else
									stack[stackPos++] = (unsigned) (tinyTree_t::TINYTREE_KSTART + 8);
								break;
							case '1':
								stack[stackPos++] = beenThere[nextNode - ('1' - '0')];
								break;
							case '2':
								stack[stackPos++] = beenThere[nextNode - ('2' - '0')];
								break;
							case '3':
								stack[stackPos++] = beenThere[nextNode - ('3' - '0')];
								break;
							case '4':
								stack[stackPos++] = beenThere[nextNode - ('4' - '0')];
								break;
							case '5':
								stack[stackPos++] = beenThere[nextNode - ('5' - '0')];
								break;
							case '6':
								stack[stackPos++] = beenThere[nextNode - ('6' - '0')];
								break;
							case '7':
								stack[stackPos++] = beenThere[nextNode - ('7' - '0')];
								break;
							case '8':
								stack[stackPos++] = beenThere[nextNode - ('8' - '0')];
								break;
							case '9':
								stack[stackPos++] = beenThere[nextNode - ('9' - '0')];
								break;

							case '>': {
								// GT (appreciated)
								assert (stackPos >= 2);

								//pop operands
								unsigned R = stack[--stackPos]; // right hand side
								unsigned L = stack[--stackPos]; // left hand side

								// create operator
								unsigned nid = treeR.addBasicNode(L, R ^ IBIT, 0);

								stack[stackPos++]     = nid; // push
								beenThere[nextNode++] = nid; // save actual index for back references
								break;
							}
							case '+': {
								// OR (appreciated)
								assert (stackPos >= 2);

								// pop operands
								unsigned R = stack[--stackPos]; // right hand side
								unsigned L = stack[--stackPos]; // left hand side

								// create operator
								unsigned nid = treeR.addBasicNode(L, 0 ^ IBIT, R);

								stack[stackPos++]     = nid; // push
								beenThere[nextNode++] = nid; // save actual index for back references
								break;
							}
							case '^': {
								// XOR/NE (appreciated)
								assert (stackPos >= 2);

								//pop operands
								unsigned R = stack[--stackPos]; // right hand side
								unsigned L = stack[--stackPos]; // left hand side

								// create operator
								unsigned nid = treeR.addBasicNode(L, R ^ IBIT, R);

								stack[stackPos++]     = nid; // push
								beenThere[nextNode++] = nid; // save actual index for back references
								break;
							}
							case '!': {
								// QnTF (appreciated)
								assert (stackPos >= 3);

								// pop operands
								unsigned F = stack[--stackPos];
								unsigned T = stack[--stackPos];
								unsigned Q = stack[--stackPos];

								// create operator
								unsigned nid = treeR.addBasicNode(Q, T ^ IBIT, F);

								// push
								stack[stackPos++]     = nid; // push
								beenThere[nextNode++] = nid; // save actual index for back references
								break;
							}
							case '&': {
								// AND (depreciated)
								assert (stackPos >= 2);

								// pop operands
								unsigned R = stack[--stackPos]; // right hand side
								unsigned L = stack[--stackPos]; // left hand side

								// create operator
								unsigned nid = treeR.addBasicNode(L, R, 0);

								stack[stackPos++]     = nid; // push
								beenThere[nextNode++] = nid; // save actual index for back references
								break;
							}
							case '<': {
								// LT (obsolete)
								assert (stackPos >= 2);

								//pop operands
								unsigned R = stack[--stackPos]; // right hand side
								unsigned L = stack[--stackPos]; // left hand side

								// create operator
								unsigned nid = treeR.addBasicNode(L, 0, R);

								stack[stackPos++]     = nid; // push
								beenThere[stackPos++] = nid; // save actual index for back references
								break;
							}
							case '?': {
								// QTF (depreciated)
								assert (stackPos >= 3);

								// pop operands
								unsigned F = stack[--stackPos];
								unsigned T = stack[--stackPos];
								unsigned Q = stack[--stackPos];

								// create operator
								unsigned nid = treeR.addBasicNode(Q, T, F);

								stack[stackPos++]     = nid; // push
								beenThere[nextNode++] = nid; // save actual index for back references
								break;
							}
							case '~': {
								// NOT (support)
								assert (stackPos >= 1);

								// invert top-of-stack
								stack[stackPos - 1] ^= IBIT;
								break;
							}

							case '/':
								// separator between placeholder/skin
								while (pCh[1])
									pCh++;
								break;
							case ' ':
								// skip spaces
								break;
							default:
								assert(0);
							}
						}

						assert (stackPos == 1);

						assert(treeR.count <= tinyTree_t::TINYTREE_NEND);

						// store result into root
						treeR.root = stack[stackPos - 1];
					}

					if (treeR.count - tinyTree_t::TINYTREE_NSTART != arg_numNodes)
						continue;

					/*
					 * Normalise the name
					 */
					char skin[MAXSLOTS + 1];
					char name[tinyTree_t::TINYTREE_NAMELEN + 1];

					treeR.saveString(treeR.root, name, skin); // save it because tree is not normalised
					treeR.loadStringSafe(name); // reload to normalise
					treeR.saveString(treeR.root, name, skin); // save with skin, byt dyadics are not normalised
					treeR.loadStringSafe(name); // reload to normalise

					if (treeR.count - tinyTree_t::TINYTREE_NSTART != arg_numNodes)
						continue;

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

					if (strcmp(name, "aaabc!d!d!") == 0)
						fprintf(stderr,".");
					/*
					 * call `foundTreeMember()`
					 */

					if (!foundTreeMember(treeR, name, newPlaceholder, newEndpoint, newBackRef))
						break;

				}
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (truncated) {
			if (ctx.opt_verbose >= ctx.VERBOSE_WARNING)
				fprintf(stderr, "[%s] WARNING: Pair/Member storage full. Truncating at progress=%lu \"%s\"\n",
					ctx.timeAsString(), this->truncated, this->truncatedName);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%u pure=%u numNode=%u numCandidate=%lu numPair=%u(%.0f%%) numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u | skipDuplicate=%u skipSize=%u skipUnsafe=%u skipcascade=%u\n",
				ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, arg_numNodes, ctx.progress,
				pStore->numPair, pStore->numPair * 100.0 / pStore->maxPair,
				pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				numEmpty, numUnsafe,
				skipDuplicate, skipSize, skipUnsafe, skipCascade);
	}

};
