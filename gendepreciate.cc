// # pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2021-06-27 15:56:11
 * 
 * Mark excess members as depreciated.
 * Excess members are those that when removed, the remaining collection spans all signature groups.
 * The goal is to have a collection wih the minimal amount of components,
 *   i.e. members that are used to construct other members, either in part or as a whole.
 * The `rewritedata[]` pre-processor can use this as a first-attempt to reduce the most obvious mirrors and transforms.
 * The mechanics behind this is: if structures are never created (because other structures have the same effect),
 *   they can be excluded from the system and safely ignored.
 *
 * The collection is pruned by removing he component members one at a time.
 * If the remaining collection has at least one member per signature group,
 *   then the component is considered excess and can be safely ignored (depreciated).
 * However, if the collection becomes incomplete then the component is considered critical and locked.
 *
 * Several algorithms have been tried to determine the order of members to examine.
 * Trying members with the greatest effect when removed are considered first.
 * In order or priority:
 *   - Smallest structures first as they are the most versatile building blocks
 *   - Members that have the highest reference count
 *   - Most versatile members first (lowest memberId)
 *
 * The "safe" 5n9 collection consists of 6533489 members, of which 684839 are used as component.
 * Rebuilding a collection where some are excluded, is an extreme time-consuming two-pass operation.
 * The first pass is to determine which members are part of the new set, the second pass to flag those that were left behind.
 * The speed is around 11 operations per second, which would take some 19 hours.
 *
 * However, the number of members to exclude does not effect the speed of the operation.
 * The main optimisation is to exclude members in bursts.
 * If the exclusion should fail because the burst includes an undetected locked member,
 *   then the burst is reduced in size in expectation that the remaining (smaller) burst will succeed.
 * This approach reduces the overall computation to about 8 hours.
 *
 * The second challenge is the speed of updating the reference counts to update the prune ordering.
 * Sorting some 300k-700k elements is also highly time-consuming.
 * An alternative approach is to determine the relative distance in the waiting queue, and using memmove() to shift the intermediate areas
 *
 * Runtimes:
 *   numNode=4, about 15 minutes
 *   numNode=5, about 6 hours
 *
 * Text modes:
 *
 *  <flags> can be:
 *     'D' depreciated (member must be excluded)
 *     'L' Locked (member must be included)

 * `--text[=1]` Brief mode that shows selected members that have their flags adjusted
 *              Also intended for transport and checkpoint/restarting
 *              Can be used for the `--load=<file>` option.
 *              Output can be converted to other text modes by `"./gendepreciate <input.db> <numNode> [<output.db>]--load=<inputList> --no-generate --text=<newMode> '>' <outputList>
 *
 *              <name> <flags>
 *
 * `--text=2`   Full mode of all members as they are being processed
 *
 *              <flags> <numComponents> <mid> <refcnt> <name>
 *
 * `--text=3`   Selected and sorted members, included all implied and cascaded
 *              NOTE: same format as `--text=1`
 *              NOTE: requires sorting and will copy (not inherit) member section
 *
 *              <name> <flags>
 *
 * `--text=4`   Selected and sorted signatures that are written to the output database
 *              NOTE: requires sorting and will copy (not inherit) imprint section
 *
 *              <sid> <mid> <tid> <name> <flags>
 * Ticker:
 *   [TIMESTAMP] position( speed/s) procent% eta=EstimatedTime | numComponents=n numDepr=n | cntDepr=n cntLock=n | refcnt=n mid=n name/expression
 *   numComponents: number of prime components, the lower the better
 *   cntDepr: number of components depreciated.
 *   numDepr: also including all members referencing the component (total)
 *   cntLock: number of component locked.

 * #endif

 * @date 2021-07-02 22:50:02
 *
 * when restarted for 4n9 at position 6397, and restarted and restarted at 4164
 * and concatenating the 3 ists and reloading, the final resut is different.
 * Also 589497 instead of 589536 when created in a single run.
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2017-2021, xyzzy@rockingship.org
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
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include "config.h"
#include "database.h"
#include "dbtool.h"
#include "tinytree.h"

/**
 * @date 2020-03-14 11:10:15
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct gendepreciateContext_t : dbtool_t {

	enum {
		/// @constant {number} - `--text` modes
		OPTTEXT_WON     = 1,
		OPTTEXT_COMPARE = 2,
		OPTTEXT_BRIEF   = 3,
		OPTTEXT_VERBOSE = 4,

		/// @constant {number} - First sid (and mid) that belongs to 4n9 space
		SID_1N9 = 3,
		/// @constant {number} - First sid that belongs to 4n9 space (should be extracted from metrics, but too lazy)
		SID_3N9 = 5666,
		/// @constant {number} - First sid that belongs to 5n9 space (should be extracted from metrics, but too lazy)
		SID_4N9 = 791647,
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
	/// @var {number} number of members to bundle when calling `countSafeExcludeSelected()`
	unsigned   opt_burst;
	/// @var {number} force overwriting of database if already exists
	unsigned   opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned   opt_generate;
	/// @var {string} name of file containing members
	const char *opt_load;
	/// @var {number} operation mode
	unsigned opt_mode;
	/// @var {number} reverse order of signatures
	unsigned opt_reverse;
	/// @var {number} save level-1 indices (hintIndex, signatureIndex, ImprintIndex) and level-2 index (imprints)
	unsigned   opt_saveIndex;
	/// @var {number} --text, textual output instead of binary database
	unsigned   opt_text;

	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for reverse transforms
	footprint_t *pEvalRev;
	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	/// @var {unsigned} - active index for `hints[]`
	unsigned        activeHintIndex;
	/// @var {number} - Head of list of free members to allocate
	unsigned        freeMemberRoot;
	/// @var {number} - Number of empty signatures left
	unsigned        numEmpty;
	/// @var {number} - Number of unsafe signatures left
	unsigned        numUnsafe;
	/// @var {number} `foundTree()` duplicate by name
	unsigned        skipDuplicate;
	/// @var {number} `foundTree()` too large for signature
	unsigned        skipSize;
	/// @var {number} `foundTree()` unsafe abundance
	unsigned        skipUnsafe;

	uint32_t iVersionSafe;
	uint32_t *pSafeSid;
	uint32_t *pSafeMid;
	uint32_t *pSafeMap;

	uint32_t iVersionSelect;
	uint32_t *pSelect;

	/**
	 * Constructor
	 */
	gendepreciateContext_t(context_t &ctx) : dbtool_t(ctx) {
		// arguments and options
		arg_inputDatabase  = NULL;
		arg_numNodes       = 0;
		arg_outputDatabase = NULL;
		opt_burst          = 0; // zero is arg_numNode determined
		opt_force          = 0;
		opt_generate       = 1;
		opt_mode           = 3;
		opt_load           = NULL;
		opt_reverse        = 0;
		opt_saveIndex      = 1;
		opt_text           = 0;

		pStore      = NULL;
		pEvalFwd    = NULL;
		pEvalRev    = NULL;

		activeHintIndex  = 0;
		freeMemberRoot   = 0;
		numUnsafe        = 0;
		skipDuplicate    = 0;
		skipSize         = 0;
		skipUnsafe       = 0;

		iVersionSafe   = 1;
		pSafeSid       = NULL;
		pSafeMid       = NULL;
		pSafeMap       = NULL;
		iVersionSelect = 1;
		pSelect        = NULL;
	}

	/**
	 * @date 2021-07-02 21:55:16
	 *
	 * Load list of members and their explicit flags
	 *
	 * File format:
	 * 	<name> <flags>
	 * Supported flags:
	 * 	'D' Depreciate
	 * 	'L' Locked
	 */
	void /*__attribute__((optimize("O0")))*/ depreciateFromFile(void) {

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading members from file\n", ctx.timeAsString());

		FILE *f = fopen(this->opt_load, "r");
		if (f == NULL) {
			ctx.fatal("\n{\"error\":\"fopen('%s') failed\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n",
				  this->opt_load, __FUNCTION__, __FILE__, __LINE__);
		}

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;

		unsigned numDepr = 0;

		// <name> <flag>
		for (;;) {
			static char line[512];
			static char     name[64];
			static char     flags[64];

			if (::fgets(line, sizeof(line), f) == 0)
				break; // end-of-input

			name[0] = 0; // incase sscanf() fails
			int ret = ::sscanf(line, "%s\t%s\n", name, flags);

			if (ret != 2)
				ctx.fatal("\n{\"error\":\"bad/empty line\",\"where\":\"%s:%s:%d\",\"linenr\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress);

			ctx.progress++;
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | %s", ctx.timeAsString(), ctx.progress, perSecond, name);
				ctx.tick = 0;
			}

			// lookup member
			unsigned ix  = pStore->lookupMember(name);
			unsigned mid = pStore->memberIndex[ix];

			if (mid == 0) {
				ctx.fatal("\n{\"error\":\"member not found\",\"where\":\"%s:%s:%d\",\"linenr\":%lu,\"name\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress, name);
			}

			if (strcmp(flags, "D") == 0) {
				// depreciate
				pStore->members[mid].flags |= member_t::MEMMASK_DEPR;
			} else if (strcmp(flags, "L") == 0) {
				// locked
				pStore->members[mid].flags |= member_t::MEMMASK_LOCKED;
			} else {
				ctx.fatal("\n{\"error\":\"invalid flags\",\"where\":\"%s:%s:%d\",\"linenr\":%lu,\"flags\":\"%s\"}\n",
				  __FUNCTION__, __FILE__, __LINE__, ctx.progress, flags);
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		fclose(f);

		/*
		 * Walk through members, any depreciated component makes the member depreciated
		 */

		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (pMember->flags & member_t::MEMMASK_DEPR) {
				// member already depreciated
				numDepr++;
			} else if ((pMember->Q != 0 && (pStore->members[pMember->Q].flags & member_t::MEMMASK_DEPR))
				   || (pMember->T != 0 && (pStore->members[pMember->T].flags & member_t::MEMMASK_DEPR))
				   || (pMember->F != 0 && (pStore->members[pMember->F].flags & member_t::MEMMASK_DEPR))
				   || (pMember->heads[0] != 0 && (pStore->members[pMember->heads[0]].flags & member_t::MEMMASK_DEPR))
				   || (pMember->heads[1] != 0 && (pStore->members[pMember->heads[1]].flags & member_t::MEMMASK_DEPR))
				   || (pMember->heads[2] != 0 && (pStore->members[pMember->heads[2]].flags & member_t::MEMMASK_DEPR))
				   || (pMember->heads[3] != 0 && (pStore->members[pMember->heads[3]].flags & member_t::MEMMASK_DEPR))
				   || (pMember->heads[4] != 0 && (pStore->members[pMember->heads[4]].flags & member_t::MEMMASK_DEPR))) {
				assert(member_t::MAXHEAD == 5);

				// components are depreciated, so member is also depreciated
				pMember->flags |= member_t::MEMMASK_DEPR;
				numDepr++;
			}
		}

		/*
		 * test that all sids have at least a single active member
		 */
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			signature_t *pSignature = pStore->signatures + iSid;

			unsigned cntActive = 0; // number of active members for this signature

			for (unsigned iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
				if (!(pStore->members[iMid].flags & member_t::MEMMASK_DEPR))
					cntActive++;
			}

			if (cntActive == 0) {
				ctx.fatal("\n{\"error\":\"signature becomes unsafe\",\"where\":\"%s:%s:%d\",\"linenr\":%lu,\"sid\":%u,\"name\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress, iSid, pSignature->name);
			}
		}

		/*
		 * Determine locked members (single active member groups)
		 */
		unsigned numLocked = updateLocked();

		/*
		 * Determine number of active components
		 */
		unsigned numComponent = 0;

		for (unsigned iMid=1; iMid<pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (!(pMember->flags & member_t::MEMMASK_DEPR) && (pMember->flags & member_t::MEMMASK_COMP))
				numComponent++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "\r\e[K[%s] numComponent=%u numDepr=%u numLocked=%u\n", ctx.timeAsString(), numComponent, numDepr, numLocked);
	}

	unsigned __attribute__((optimize("O0")))  updateLocked(void) {

		unsigned cntLocked        = 0;

		/*
		 * count already present locked members
		 */
		for (unsigned j = SID_1N9; j < pStore->numMember; j++) {
			if (pStore->members[j].flags & member_t::MEMMASK_LOCKED)
				cntLocked++;
		}

		/*
		 * Find single active member signatures and lock them and their head+tail components
		 */
		for (unsigned iSid = pStore->numSignature-1; iSid >= SID_1N9 ; iSid--) {
			signature_t *pSignature = pStore->signatures + iSid;

			++iVersionSelect;

			unsigned cntActive = 0; // number of active members for this signature
			unsigned lastActive = 0; // last encountered active member
			/*
			 * count active members
			 */

			for (unsigned iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
				if (!(pStore->members[iMid].flags & member_t::MEMMASK_DEPR)) {
					cntActive++;
					lastActive = iMid;
				}
			}

			if (cntActive == 1 && !(pStore->members[lastActive].flags & member_t::MEMMASK_LOCKED)) {
				pStore->members[lastActive].flags |= member_t::MEMMASK_LOCKED;
				cntLocked++;
			}
		}

		/*
		 * Propagate locked
		 */
		for (unsigned iMid = pStore->numMember - 1; iMid >= 1; --iMid) {
			member_t *pMember = pStore->members + iMid;

			if (pStore->members[iMid].flags & member_t::MEMMASK_LOCKED) {
				if (pMember->Q && !(pStore->members[pMember->Q].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->Q].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (pMember->T && !(pStore->members[pMember->T].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->T].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (pMember->F && !(pStore->members[pMember->F].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->F].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (pMember->heads[0] && !(pStore->members[pMember->heads[0]].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->heads[0]].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (pMember->heads[1] && !(pStore->members[pMember->heads[1]].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->heads[1]].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (pMember->heads[2] && !(pStore->members[pMember->heads[2]].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->heads[2]].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (pMember->heads[3] && !(pStore->members[pMember->heads[3]].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->heads[3]].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (pMember->heads[4] && !(pStore->members[pMember->heads[4]].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[pMember->heads[4]].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
			}
		}

		return cntLocked;
	}

	/*
	 * @date 2021-06-30 13:32:19
	 *
	 * For signature groups containing components, drop all the non-components
	 * They are not referenced, have nothing to contribute and nothing is lost.
	 */
	bool __attribute__((optimize("O0")))  modeComponent(void) {

		bool     somethingChanged = false;
		unsigned cntSelected      = 0;
		unsigned cntSid, cntMid;

		/*
		 * Count number of steps for ticker
		 */
		for (unsigned iSid = SID_1N9; iSid<pStore->numSignature; iSid++) {
			signature_t *pSignature = pStore->signatures + iSid;

			++iVersionSelect; // select nothing

			bool hasComponent = false;
			for (unsigned iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
				member_t *pMember = pStore->members + iMid;

				if (pMember->flags & member_t::MEMMASK_DEPR)
					continue;
				if (pMember->flags & member_t::MEMMASK_COMP)
					hasComponent = true;
			}

			if (hasComponent) {
				for (unsigned iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
					member_t *pMember = pStore->members + iMid;

					if (pMember->flags & member_t::MEMMASK_DEPR)
						continue;
					if (!(pMember->flags & member_t::MEMMASK_COMP) && !(pMember->flags & member_t::MEMMASK_LOCKED)) {
						pSelect[iMid] = iVersionSelect;
						cntSelected++;
					}
				}
			}
		}

		if (cntSelected) {
			// Select/exclude all lesser
			countSafeExcludeSelected(cntSid, cntMid);

			assert (cntSid == pStore->numSignature - 1);


			for (unsigned j=SID_1N9; j < pStore->numMember; j++) {
				member_t *pMem = pStore->members + j;

				// depreciate all (new) orphans
				if (pSafeMid[j] != iVersionSafe && !(pMem->flags & member_t::MEMMASK_DEPR)) {
					pMem->flags |= member_t::MEMMASK_DEPR;
					somethingChanged = true;
				}
			}
		}

		return somethingChanged;
	}

	struct refcnt_t {
		member_t *pMember;
		uint32_t refcnt;
		uint32_t heapIdx; // index in heap

		refcnt_t() : refcnt(0), heapIdx(0) {
		}

		int compar(const refcnt_t &rhs) const {
			int cmp = (int) this->refcnt - (int) rhs.refcnt;

			/*
			 * @date 2021-07-01 13:58:02
			 * `&rhs-this` does slightly better with 599759 4n9 components, as opposed to 600040 with `this-&rhs`.
			 */
			if (cmp == 0)
				cmp = &rhs - this;
			return cmp;
		}
	};

	static int comparHeap(const void *lhs, const void *rhs) {
		if (lhs == rhs)
			return 0;

		const refcnt_t **pNodeL = (const refcnt_t **)(lhs);
		const refcnt_t **pNodeR = (const refcnt_t **)(rhs);

		return pNodeL[0]->compar(*pNodeR[0]);
	}

	/*
	 * @date 2021-07-01 20:48:16
	 *
	 * Implement a heap-sorted verson of a refcnt_t vector.
	 * It maintains a (partial) sorted vector with an ever changing values of refcnt.
	 * Element[1] is the first as has the best sorting, the other elements are less sorted when more distant.
	 *
	 * @date 2021-07-01 20:52:02
	 *
	 * In bulk mode, the top `burst` elements are used to call `countSafeExcludeSelected()`
	 * Only the elements further than [1] are less sorted on `mid`.
	 * This gives a mind-boggling effect of a better final result than with the slower  but accurate `qsort()`.
	 *
	 * The outcome seems valid, so no asking questions just yet.
	 */
	struct heap_t {
		unsigned count;
		refcnt_t **buf;

		heap_t(unsigned count, refcnt_t *pArr) {
			this->count = 0;
			this->buf   = (refcnt_t **) calloc(count, sizeof *this->buf);

			// assign non-zero refcounts
			for (unsigned i = 0; i < count; i++) {
				if (pArr[i].refcnt > 0)
					this->buf[this->count++] = pArr + i;
			}

			// initial sort
			qsort(this->buf, this->count, sizeof *this->buf, comparHeap);

			// assign indices
			for (unsigned i = 0; i < this->count; i++)
				this->buf[i]->heapIdx = i;
		}

		~heap_t() {
			if (buf != 0) {
				free(buf);
				buf = NULL;
			}
		}

		void down(refcnt_t *p) {
			unsigned lo = p->heapIdx;
			unsigned hi = this->count - 1;

			while (lo < hi) {
				unsigned t = (lo + hi) >> 1;
				if (p->compar(*buf[t]) > 0)
					lo = t + 1;
				else
					hi = t;
			}
			assert(lo == hi);


			if (lo != p->heapIdx) {
				// rotate
				memmove(buf + lo+1, buf+lo, (p->heapIdx-lo) * sizeof *buf);
				buf[lo] = p;

				// update bck-references
				for (unsigned ix = p->heapIdx; ix > lo; --ix)
					buf[ix]->heapIdx = ix;
				buf[lo]->heapIdx = lo;
			}
		}

		refcnt_t* pop(void) {
			if (this->count == 0)
				return NULL;

			return this->buf[--this->count];
		}

	};

	bool /*__attribute__((optimize("O0")))*/ depreciateFromGenerator(void) {

		unsigned numComponents = 0;

		for (unsigned iMid=1; iMid<pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (!(pMember->flags & member_t::MEMMASK_DEPR) && (pMember->flags & member_t::MEMMASK_COMP))
				numComponents++;
		}

		/*
		 * Update ref counts
		 */

		// allocate
		refcnt_t *pRefcnts = (refcnt_t*) calloc(pStore->numMember, sizeof *pRefcnts);
		// populate
		for (unsigned iMid=1; iMid<pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			pRefcnts[iMid].pMember = pMember;

			if (arg_numNodes && pMember->size != arg_numNodes)
				continue;

			if (!(pMember->flags & member_t::MEMMASK_DEPR)) {
				if (pMember->Q) pRefcnts[pMember->Q].refcnt++;
				if (pMember->T) pRefcnts[pMember->T].refcnt++;
				if (pMember->F) pRefcnts[pMember->F].refcnt++;
				if (pMember->heads[0]) pRefcnts[pMember->heads[0]].refcnt++;
				if (pMember->heads[1]) pRefcnts[pMember->heads[1]].refcnt++;
				if (pMember->heads[2]) pRefcnts[pMember->heads[2]].refcnt++;
				if (pMember->heads[3]) pRefcnts[pMember->heads[3]].refcnt++;
				if (pMember->heads[4]) pRefcnts[pMember->heads[4]].refcnt++;
			}
		}
		// remove locked
		for (unsigned iMid=1; iMid<pStore->numMember; iMid++) {
			if (pStore->members[iMid].flags & member_t::MEMMASK_LOCKED)
				pRefcnts[iMid].refcnt = 0;
		}

		heap_t heap(pStore->numMember, pRefcnts);
		fprintf(stderr, "%u\n", heap.count);

		unsigned cntSid, cntMid;
		unsigned cntDepr=0, cntLock = 0;
		unsigned numDepr;
		unsigned burstSize = 0;
		unsigned lastRefCount = 0;

		// determine initial value
		++iVersionSelect; // exclude nothing
		countSafeExcludeSelected(cntSid, cntMid);
		numDepr = pStore->numMember - 1 - cntMid;

		ctx.setupSpeed(heap.count);
		ctx.tick = 0;

		for (;;) {
			// remove leading empties
			while (heap.count > 0 && heap.buf[heap.count - 1]->refcnt == 0) {
				heap.pop();
			}
			if (heap.count == 0)
				break;

			refcnt_t *pCurr = heap.buf[heap.count - 1];

			// separate lines at exact points for performance comparison
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && pCurr->refcnt < 32 && lastRefCount != pCurr->refcnt)
				ctx.tick = 1;

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				unsigned iMid     = pCurr - pRefcnts;
				member_t *pMember = pStore->members + iMid;

				fprintf(stderr, "\r\e[K[%s] %lu(%3d/s) %.5f%% eta=%d:%02d:%02d | numComponents=%u numDepr=%u | cntDepr=%u cntLock=%u | refcnt=%u mid=%u %s",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, numComponents, numDepr, cntDepr, cntLock, pCurr->refcnt, iMid, pMember->name);

				ctx.tick = 0;
			}

			// separate lines at exact points for performance comparison
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && pCurr->refcnt < 32 && lastRefCount != pCurr->refcnt) {
				if (lastRefCount != 0)
					fprintf(stderr, "\n");
				lastRefCount = pCurr->refcnt;
			}

			/*
			 * collect as many members as possible with same refcount
			 *
			 * @date 2021-07-01 17:38:31
			 *
			 * Note that the heap is only partly sorted, The first entry is sorted, the others are estimated and likely lightly out-of-order
			 * This may give different results for different burst settings
			 */

			// reset burst when all members are exausted.
			if (burstSize == 0)
				burstSize = opt_burst;

			// collect as many members as possible with same refcount
			unsigned cntSelect = 0;
			++iVersionSelect;

			for (int k = heap.count - 1; k >= 0 && cntSelect < burstSize && heap.buf[k]->refcnt == heap.buf[heap.count - 1]->refcnt; --k) {
				unsigned iMid = heap.buf[k] - pRefcnts;
				assert(!(pStore->members[iMid].flags & member_t::MEMMASK_LOCKED));
				pSelect[iMid] = iVersionSelect;
				cntSelect++;
			}
			burstSize = cntSelect; // other considerations may reduce the burst size

			// is it possible?
			countSafeExcludeSelected(cntSid, cntMid);

			/*
			 * If excluding failed, then reduce the burst size and retry.
			 * On the other hand, if excluding succeeded, and burst size was reduced,
			 * then an unflagged locked member is still in the remaining part.
			 * In such a case, anticipate this by reducing the burst size too (or the next round is certain to fail).
			 */

			if (cntSid == pStore->numSignature - 1) {
				// update
				numDepr = pStore->numMember - 1 - cntMid;

				// update burst size
				if (burstSize != opt_burst)
					burstSize >>= 1;

				// display what was selected
				for (unsigned k = 0; k < cntSelect; k++) {
					refcnt_t *pCurr = heap.pop();
					unsigned iMid     = pCurr - pRefcnts;
					member_t *pMember = pStore->members + iMid;

					if (opt_text == OPTTEXT_COMPARE)
						printf("D\t%u\t%u\t%u\t%s\n", numComponents, iMid, pCurr->refcnt, pMember->name);
					else if (opt_text == OPTTEXT_WON)
						printf("%s\tD\n", pMember->name);
					cntDepr++;
					ctx.progress++;
				}

				// update ref counts
				for (unsigned j = SID_1N9; j < pStore->numMember; j++) {
					member_t *pMember = pStore->members + j;

					// depreciate all (new) orphans
					if (pSafeMid[j] != iVersionSafe && !(pMember->flags & member_t::MEMMASK_DEPR)) {
						assert(!(pMember->flags & member_t::MEMMASK_LOCKED));

						pMember->flags |= member_t::MEMMASK_DEPR;

						if (pMember->flags & member_t::MEMMASK_COMP)
							--numComponents;

						if (!(pMember->flags & member_t::MEMMASK_DEPR)) {
							if (pMember->Q) {
								pRefcnts[pMember->Q].refcnt--;
								heap.down(pRefcnts + pMember->Q);
							}
							if (pMember->T) {
								pRefcnts[pMember->T].refcnt--;
								heap.down(pRefcnts + pMember->T);
							}
							if (pMember->F) {
								pRefcnts[pMember->F].refcnt--;
								heap.down(pRefcnts + pMember->F);
							}
							if (pMember->heads[0]) {
								pRefcnts[pMember->heads[0]].refcnt--;
								heap.down(pRefcnts + pMember->heads[0]);
							}
							if (pMember->heads[1]) {
								pRefcnts[pMember->heads[1]].refcnt--;
								heap.down(pRefcnts + pMember->heads[1]);
							}
							if (pMember->heads[2]) {
								pRefcnts[pMember->heads[2]].refcnt--;
								heap.down(pRefcnts + pMember->heads[2]);
							}
							if (pMember->heads[3]) {
								pRefcnts[pMember->heads[3]].refcnt--;
								heap.down(pRefcnts + pMember->heads[3]);
							}
							if (pMember->heads[4]) {
								pRefcnts[pMember->heads[4]].refcnt--;
								heap.down(pRefcnts + pMember->heads[4]);
							}
							assert(member_t::MAXHEAD == 5);
						}

					}
				}

			} else if (cntSelect == 1) {
				// pop the member and mark as locked
				refcnt_t *pCurr = heap.pop();
				unsigned iMid     = pCurr - pRefcnts;
				member_t *pMember = pStore->members + iMid;

				if (pMember->flags & member_t::MEMMASK_LOCKED)
					continue;
				pMember->flags |= member_t::MEMMASK_LOCKED;

				if (opt_text == OPTTEXT_COMPARE)
					printf("L\t%u\t%u\t%u\t%s\n", numComponents, iMid, pCurr->refcnt, pMember->name);
				else if (opt_text == OPTTEXT_WON)
					printf("%s\tL\n", pMember->name);

				cntLock++;
				ctx.progress++;

				// reset burst size
				burstSize = opt_burst;
			} else {
				// decrease burst size and try again
				burstSize >>= 1;

			}
		}
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		unsigned numLocked = updateLocked();

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numComponents=%u numDepr=%u numLocked=%u | cntDepr=%u cntLock=%u\n", ctx.timeAsString(), numComponents, numDepr, numLocked, cntDepr, cntLock);

		return false;
	}

	/*
	 * @date 2021-06-27 17:39:31
	 */
	void countSafeExcludeSelected(unsigned &cntSid, unsigned &cntMid) {

		cntSid = cntMid = 0;

		++iVersionSafe;

		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (pMember->flags & member_t::MEMMASK_DEPR) {
				// depreciated, ignore
				continue;
			} else if (pSelect[iMid] == iVersionSelect) {
				// explicitly excluded
				assert (pMember->flags & member_t::MEMMASK_COMP); // must be a component
				assert (!(pMember->flags & member_t::MEMMASK_LOCKED)); // may not be locked
				continue;
			} else if (pMember->sid == 1 || pMember->sid == 2) {
				// "0" or "a"
				pSafeMid[iMid] = iVersionSafe;
				cntMid++;
				if (pSafeSid[pMember->sid] != iVersionSafe) {
					pSafeSid[pMember->sid] = iVersionSafe;
					cntSid++;
				}
			} else if ((pMember->Q == 0 || pSafeMid[pMember->Q] == iVersionSafe)
					&& (pMember->T == 0 || pSafeMid[pMember->T] == iVersionSafe)
					&& (pMember->F == 0 || pSafeMid[pMember->F] == iVersionSafe)
					&& (pMember->heads[0] == 0 || pSafeMid[pMember->heads[0]] == iVersionSafe)
					&& (pMember->heads[1] == 0 || pSafeMid[pMember->heads[1]] == iVersionSafe)
					&& (pMember->heads[2] == 0 || pSafeMid[pMember->heads[2]] == iVersionSafe)
					&& (pMember->heads[3] == 0 || pSafeMid[pMember->heads[3]] == iVersionSafe)
					&& (pMember->heads[4] == 0 || pSafeMid[pMember->heads[4]] == iVersionSafe)) {

				assert(member_t::MAXHEAD == 5);

				pSafeMid[iMid] = iVersionSafe;
				cntMid++;
				if (pSafeSid[pMember->sid] != iVersionSafe) {
					pSafeSid[pMember->sid] = iVersionSafe;
					cntSid++;
				}
			} else if (pMember->flags & member_t::MEMMASK_LOCKED) {
				// a locked member not being safe (part of the final collection) is an error
				cntSid = cntMid = 0;
				return;
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
 * @global {gendepreciateContext_t} Application context
 */
gendepreciateContext_t app(ctx);

/**
 * @date 2020-03-11 23:06:35
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int __attribute__ ((unused)) sig) {
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
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --burst=<number>                Burst size for excluding members [default=%u, 0=determined by <numnode>]\n", app.opt_burst);
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>           Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --maximprint=<number>           Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxmember=<number>            Maximum number of members [default=%u]\n", app.opt_maxMember);
		fprintf(stderr, "\t   --memberindexsize=<number>      Size of member index [default=%u]\n", app.opt_memberIndexSize);
		fprintf(stderr, "\t   --mode=<number>                 Operational mode [default=%u]\n", app.opt_mode);
		fprintf(stderr, "\t   --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --reverse                       Reverse order of signatures\n");
		fprintf(stderr, "\t   --[no-]saveindex                Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --text                          Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --[no-]unsafe                   Reindex imprints based on empty/unsafe signature groups [default=%s]\n", (ctx.flags & context_t::MAGICMASK_UNSAFE) ? "enabled" : "disabled");
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
			LO_BURST = 1,
			LO_DEBUG,
			LO_FORCE,
			LO_GENERATE,
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_LOAD,
			LO_MAXIMPRINT,
			LO_MAXMEMBER,
			LO_MEMBERINDEXSIZE,
			LO_MODE,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_NOUNSAFE,
			LO_PARANOID,
			LO_PURE,
			LO_RATIO,
			LO_REVERSE,
			LO_SAVEINDEX,
			LO_SIGNATUREINDEXSIZE,
			LO_TEXT,
			LO_TIMER,
			LO_UNSAFE,
			// short opts
			LO_HELP    = 'h',
			LO_QUIET   = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"burst",              1, 0, LO_BURST},
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
			{"mode",               1, 0, LO_MODE},
			{"no-generate",        0, 0, LO_NOGENERATE},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-pure",            0, 0, LO_NOPURE},
			{"no-saveindex",       0, 0, LO_NOSAVEINDEX},
			{"no-unsafe",          0, 0, LO_NOUNSAFE},
			{"paranoid",           0, 0, LO_PARANOID},
			{"pure",               0, 0, LO_PURE},
			{"quiet",              2, 0, LO_QUIET},
			{"ratio",              1, 0, LO_RATIO},
			{"reverse",            0, 0, LO_REVERSE},
			{"saveindex",          0, 0, LO_SAVEINDEX},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"text",               2, 0, LO_TEXT},
			{"timer",              1, 0, LO_TIMER},
			{"unsafe",             0, 0, LO_UNSAFE},
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

		*cp = '\0';

		// parse long options
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_BURST:
			app.opt_burst = ::strtoul(optarg, NULL, 0);
			break;
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
		case LO_MODE:
			app.opt_mode = ::strtoul(optarg, NULL, 0);
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
		case LO_REVERSE:
			app.opt_reverse++;
			break;
		case LO_NOSAVEINDEX:
			app.opt_saveIndex = 0;
			break;
		case LO_SAVEINDEX:
			app.opt_saveIndex = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_saveIndex + 1;
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
		case LO_UNSAFE:
			ctx.flags |= context_t::MAGICMASK_UNSAFE;
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

	if (app.arg_inputDatabase == NULL || app.arg_numNodes == 0) {
		usage(argv, false);
		exit(1);
	}

	// Default `--burst` depends on the size of the prune collection
	if (app.opt_burst == 0) {
		if (app.arg_numNodes == 5)
			app.opt_burst = 16;
		else if (app.arg_numNodes == 4)
			app.opt_burst = 2;
		else
			app.opt_burst = 1;
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
	app.readOnlyMode = (app.arg_outputDatabase == NULL && app.opt_text != app.OPTTEXT_BRIEF && app.opt_text != app.OPTTEXT_VERBOSE);

	db.open(app.arg_inputDatabase, !app.readOnlyMode);

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

	// will be using `lookupSignature()`, `lookupImprintAssociative()` and `lookupMember()`
	app.inheritSections &= ~(database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_MEMBER | database_t::ALLOCMASK_MEMBERINDEX);
	// signature indices are used read-only, remove from inherit if sections are empty
	if (!db.signatureIndexSize)
		app.inheritSections &= ~database_t::ALLOCMASK_SIGNATUREINDEX;
	if (!db.numImprint)
		app.inheritSections &= ~database_t::ALLOCMASK_IMPRINT;
	if (!db.imprintIndexSize)
		app.inheritSections &= ~database_t::ALLOCMASK_IMPRINTINDEX;
	// will require local copy of members
	app.rebuildSections |= database_t::ALLOCMASK_MEMBER;

	// inherit signature/member size
	if (!app.readOnlyMode) {
		app.opt_maxSignature = db.numSignature;
		app.opt_maxMember = db.numMember;
	}

	if (db.numTransform == 0)
		ctx.fatal("Missing transform section: %s\n", app.arg_inputDatabase);
	if (db.numSignature == 0)
		ctx.fatal("Missing signature section: %s\n", app.arg_inputDatabase);
	if (db.numMember == 0)
		ctx.fatal("Missing member section: %s\n", app.arg_inputDatabase);

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, 5);

	/*
	 * Finalise allocations and create database
	 */

	// allocate evaluators
	app.pEvalFwd    = (footprint_t *) ctx.myAlloc("gendepreciateContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev    = (footprint_t *) ctx.myAlloc("gendepreciateContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	// allocate big arrays
	app.pSafeSid      = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSafeSid", store.maxSignature, sizeof(*app.pSafeSid));
	app.pSafeMid      = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSafeMid", store.maxMember, sizeof(*app.pSafeMid));
	app.pSafeMap      = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSafeMap", store.maxMember, sizeof(*app.pSafeMap));
	app.pSelect       = (uint32_t *) ctx.myAlloc("gendepreciateContext_t::pSelect", store.maxMember, sizeof(*app.pSelect));

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

	// actual create
	store.create(app.inheritSections);
	app.pStore = &store;

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS && !(app.rebuildSections & ~app.inheritSections)) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Allocated %.3fG memory. freeMemory=%.3fG.\n", ctx.timeAsString(), ctx.totalAllocated / 1e9, info.freeram / 1e9);
	}

	// initialize evaluator early using input database
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, db.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, db.revTransformData);

	/*
	 * Inherit/copy sections
	 */

	app.populateDatabaseSections(store, db);

	/*
	 * Rebuild sections
	 */

	if (app.rebuildSections & database_t::ALLOCMASK_MEMBER) {
		store.numMember = db.numMember;
		::memcpy(store.members, db.members, store.numMember * sizeof(*store.members));
	}
	if (app.rebuildSections)
		store.rebuildIndices(app.rebuildSections);

	/*
	 * count empty/unsafe
	 */

	app.numEmpty = app.numUnsafe = 0;
	for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
		if (store.signatures[iSid].firstMember == 0)
			app.numEmpty++;
		if (!(store.signatures[iSid].flags & signature_t::SIGMASK_SAFE))
			app.numUnsafe++;
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] numMember=%u(%.0f%%) numEmpty=%u numUnsafe=%u\n",
			ctx.timeAsString(),
			store.numMember, store.numMember * 100.0 / store.maxMember,
			app.numEmpty, app.numUnsafe - app.numEmpty);

	/*
	 * Validate, all members should be safe and properly ordered
	 */
	{
		unsigned cntUnsafe = 0;
		unsigned cntSid, cntMid;

		// select appreciated
		++app.iVersionSelect; // select/exclude none
		app.countSafeExcludeSelected(cntSid, cntMid);

		for (unsigned iMid = 3; iMid < store.numMember; iMid++) {
			member_t *pMember = store.members + iMid;

			if (!(pMember->flags & member_t::MEMMASK_SAFE))
				cntUnsafe++;

			if (pMember->flags & member_t::MEMMASK_DEPR) {
				assert(app.pSafeMid[iMid] != app.iVersionSafe);
			} else {
				assert(app.pSafeMid[iMid] == app.iVersionSafe);
			}

			assert(pMember->Q == 0 || pMember->Q < iMid);
			assert(pMember->T == 0 || pMember->T < iMid);
			assert(pMember->F == 0 || pMember->F < iMid);
			assert(pMember->heads[0] == 0 || pMember->heads[0] < iMid);
			assert(pMember->heads[1] == 0 || pMember->heads[1] < iMid);
			assert(pMember->heads[2] == 0 || pMember->heads[2] < iMid);
			assert(pMember->heads[3] == 0 || pMember->heads[3] < iMid);
			assert(pMember->heads[4] == 0 || pMember->heads[4] < iMid);
			assert(member_t::MAXHEAD == 5);
		}
		if (cntUnsafe > 0)
			fprintf(stderr,"WARNING: Found %u unsafe members\n", cntUnsafe);
	}

	/*
	 * Where to look for new candidates
	 */

	// if input is empty, skip reserved entries
	if (!app.readOnlyMode) {
		assert(store.numMember > 0);
	}

	// update locking
	app.updateLocked();

	if (app.opt_load)
		app.depreciateFromFile();
	if (app.opt_generate) {
		app.depreciateFromGenerator();
	}

	/*
	 * re-order and re-index members
	 */

	if (!app.readOnlyMode) {
		/*
		 * Check that all unsafe groups have no safe members (or the group would have been safe)
		 */
		for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
			if (!(store.signatures[iSid].flags & signature_t::SIGMASK_SAFE)) {
				for (unsigned iMid = store.signatures[iSid].firstMember; iMid; iMid = store.members[iMid].nextMember) {
					assert(!(store.members[iMid].flags & member_t::MEMMASK_SAFE));
				}
			}
		}

		if (app.opt_text == app.OPTTEXT_BRIEF) {
			/*
			 * Display depreciated components
			 *
			 * <memberName>
			 */
			for (unsigned iMid = 1; iMid < store.numMember; iMid++) {
				member_t *pMember = store.members + iMid;

				if (pMember->flags & member_t::MEMMASK_COMP) {
					if (pMember->flags & member_t::MEMMASK_DEPR)
						printf("%s\tD\n", pMember->name);
					else if (pMember->flags & member_t::MEMMASK_LOCKED)
						printf("%s\tL\n", pMember->name);
				}
			}
		}

		if (app.opt_text == app.OPTTEXT_VERBOSE) {
			/*
			 * Display full members, grouped by signature
			 */
			for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
				const signature_t *pSignature = store.signatures + iSid;

				for (unsigned iMid = pSignature->firstMember; iMid; iMid = store.members[iMid].nextMember) {
					member_t *pMember = store.members + iMid;

					printf("%u\t%u\t%u\t%s\t", iSid, iMid, pMember->tid, pMember->name);

					if (pSignature->flags & signature_t::SIGMASK_SAFE) {
						if (pMember->flags & member_t::MEMMASK_SAFE)
							printf("S");
						else
							printf("s");
					}
					if (pMember->flags & member_t::MEMMASK_COMP)
						printf("C");
					if (pMember->flags & member_t::MEMMASK_LOCKED)
						printf("L");
					if (pMember->flags & member_t::MEMMASK_DEPR)
						printf("D");
					if (pMember->flags & member_t::MEMMASK_DELETE)
						printf("X");
					printf("\n");
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
			store.hintIndexSize      = 0;
			store.imprintIndexSize   = 0;
			store.numImprint         = 0;
			store.interleave         = 0;
			store.interleaveStep     = 0;
		}

		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		json_t *jResult = json_object();
		json_object_set_new_nocheck(jResult, "done", json_string_nocheck(argv[0]));
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.jsonInfo(jResult);
		fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}

	return 0;
}
