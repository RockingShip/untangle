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
 *   [TIMESTAMP] position( speed/s) procent% eta=EstimatedTime | numComponent=n numDepr=n | cntDepr=n cntLock=n | refcnt=n mid=n name/expression
 *   numComponents: number of prime components, the lower the better
 *   cntDepr: number of components depreciated.
 *   numDepr: also including all members referencing the component (total)
 *   cntLock: number of component locked.
 *
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
		SID_1N9 = 1,
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
	/// @var {number} Tree size in nodes to be generated for this invocation (0=any)
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
	unsigned   opt_mode;
	/// @var {number} reverse order of signatures
	unsigned   opt_reverse;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {string} only lookup signatures are safe
	unsigned opt_lookupSafe;

	/// @var {database_t} - Database store to place results
	database_t *pStore;

	/// @var {unsigned} - active index for `hints[]`
	unsigned activeHintIndex;
	/// @var {number} - Head of list of free members to allocate
	unsigned freeMemberRoot;
	/// @var {number} - Number of empty signatures left
	unsigned numEmpty;
	/// @var {number} - Number of unsafe signatures left
	unsigned numUnsafe;
	/// @var {number} `foundTree()` duplicate by name
	unsigned skipDuplicate;
	/// @var {number} `foundTree()` too large for signature
	unsigned skipSize;
	/// @var {number} `foundTree()` unsafe abundance
	unsigned skipUnsafe;

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
		opt_text           = 0;
		opt_lookupSafe     = 0;

		pStore      = NULL;

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

	/*
	 * @date 2021-07-04 07:29:13
	 * Display counts for comparison
	 */
	void showCounts() {
		unsigned numComponent = 0;
		unsigned numDepr      = 0;
		unsigned numLocked    = updateLocked();

		/*
		 * Walk through members, any depreciated component makes the member depreciated, count active components
		 */

		for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (pMember->flags & member_t::MEMMASK_DEPR) {
				// member already depreciated
				numDepr++;
			} else if ((pMember->Qmt != 0 && (pStore->members[pStore->pairs[pMember->Qmt].id].flags & member_t::MEMMASK_DEPR))
				   || (pMember->Tmt != 0 && (pStore->members[pStore->pairs[pMember->Tmt].id].flags & member_t::MEMMASK_DEPR))
				   || (pMember->Fmt != 0 && (pStore->members[pStore->pairs[pMember->Fmt].id].flags & member_t::MEMMASK_DEPR))
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

			if (!(pMember->flags & member_t::MEMMASK_DEPR) && (pMember->flags & member_t::MEMMASK_COMP))
				numComponent++;

		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "\r\e[K[%s] numMember=%u numComponent=%u numLocked=%u\n", ctx.timeAsString(), pStore->numMember - numDepr, numComponent, numLocked);
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
			static char name[64];
			static char flags[64];

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
			uint32_t ix  = pStore->lookupMember(name);
			uint32_t mid = pStore->memberIndex[ix];

			if (mid == 0) {
				ctx.fatal("\n{\"error\":\"member not found\",\"where\":\"%s:%s:%d\",\"linenr\":%lu,\"name\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress, name);
			}

			for (const char *pFlag = flags; *pFlag; pFlag++) {
				if (*pFlag == 'C') {
					pStore->members[mid].flags |= member_t::MEMMASK_COMP; // component
				} else if (*pFlag == 'L') {
					pStore->members[mid].flags |= member_t::MEMMASK_LOCKED; // locked
				} else if (*pFlag == 'D') {
					pStore->members[mid].flags |= member_t::MEMMASK_DEPR; // depreciated
				} else  {
					ctx.fatal("\n{\"error\":\"invalid flag\",\"where\":\"%s:%s:%d\",\"linenr\":%lu,\"flag\":\"%c\"}\n",
						  __FUNCTION__, __FILE__, __LINE__, ctx.progress, *pFlag);
				}
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		fclose(f);

		/*
		 * Walk through members, any depreciated component makes the member depreciated
		 */

		for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (pMember->flags & member_t::MEMMASK_DEPR) {
				// member already depreciated
				numDepr++;
			} else if ((pMember->Qmt != 0 && (pStore->members[pStore->pairs[pMember->Qmt].id].flags & member_t::MEMMASK_DEPR))
				   || (pMember->Tmt != 0 && (pStore->members[pStore->pairs[pMember->Tmt].id].flags & member_t::MEMMASK_DEPR))
				   || (pMember->Fmt != 0 && (pStore->members[pStore->pairs[pMember->Fmt].id].flags & member_t::MEMMASK_DEPR))
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
		for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
			signature_t *pSignature = pStore->signatures + iSid;

			unsigned cntActive = 0; // number of active members for this signature

			for (uint32_t iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
				if (!(pStore->members[iMid].flags & member_t::MEMMASK_DEPR))
					cntActive++;
			}

			if (cntActive == 0) {
				if (!opt_lookupSafe || (pSignature->flags & signature_t::SIGMASK_KEY))
					ctx.fatal("\n{\"error\":\"signature becomes empty\",\"where\":\"%s:%s:%d\",\"linenr\":%lu,\"sid\":%u,\"name\":\"%s\"}\n",
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

		for (uint32_t iMid=1; iMid<pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			// depr/locked is mutual-exclusive
			assert(!(pMember->flags & member_t::MEMMASK_DEPR) || !(pMember->flags & member_t::MEMMASK_LOCKED));

			if (!(pMember->flags & member_t::MEMMASK_DEPR) && (pMember->flags & member_t::MEMMASK_COMP))
				numComponent++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "\r\e[K[%s] numMember=%u numComponent=%u numLocked=%u\n", ctx.timeAsString(), pStore->numMember - numDepr, numComponent, numLocked);
	}

	/*
	 * @date 2021-07-26 21:49:19
	 *
	 * Update flags
	 */
	unsigned /*__attribute__((optimize("O0")))*/ updateLocked(void) {

		unsigned cntLocked = 0;

		/*
		 * Lock the roots
		 */

		uint32_t ix = pStore->lookupMember("0");
		if (pStore->memberIndex[ix] != 0)
			pStore->members[pStore->memberIndex[ix]].flags |= member_t::MEMMASK_LOCKED | member_t::MEMMASK_COMP;

		ix = pStore->lookupMember("a");
		if (pStore->memberIndex[ix] != 0)
			pStore->members[pStore->memberIndex[ix]].flags |= member_t::MEMMASK_LOCKED | member_t::MEMMASK_COMP;

		/*
		 * count already present locked members
		 */
		for (uint32_t j = 1; j < pStore->numMember; j++) {
			if (pStore->members[j].flags & member_t::MEMMASK_LOCKED)
				cntLocked++;
		}

		/*
		 * Find and lock single active member groups
		 *
		 * @date 2021-08-06 21:53:02
		 * Only for `SIGMASK_LOOKUP` signatures, others might be optimised away
		 */
		for (uint32_t iSid = pStore->numSignature - 1; iSid >= 1; --iSid) {
			signature_t *pSignature = pStore->signatures + iSid;

			if (opt_lookupSafe && !(pSignature->flags & signature_t::SIGMASK_KEY))
				continue;

			unsigned cntActive  = 0; // number of active members for this signature
			unsigned lastActive = 0; // last encountered active member

			/*
			 * count active members
			 */

			for (uint32_t iMid = pSignature->firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
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
		for (uint32_t iMid = pStore->numMember - 1; iMid >= 1; --iMid) {
			member_t *pMember = pStore->members + iMid;

			if (pStore->members[iMid].flags & member_t::MEMMASK_LOCKED) {
				uint32_t Qmid = pStore->pairs[pMember->Qmt].id;
				uint32_t Tmid = pStore->pairs[pMember->Tmt].id;
				uint32_t Fmid = pStore->pairs[pMember->Fmt].id;

				if (Qmid && !(pStore->members[Qmid].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[Qmid].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (Tmid && !(pStore->members[Tmid].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[Tmid].flags |= member_t::MEMMASK_LOCKED;
					cntLocked++;
				}
				if (Fmid && !(pStore->members[Fmid].flags & member_t::MEMMASK_LOCKED)) {
					pStore->members[Fmid].flags |= member_t::MEMMASK_LOCKED;
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
				assert(member_t::MAXHEAD == 5);
			}
		}

		return cntLocked;
	}

	struct refcnt_t {
		unsigned refcnt;
		int heapIdx; // index in heap, -1 if not

		refcnt_t() : refcnt(0), heapIdx(0) {
		}

		int compar(const refcnt_t &rhs) const {
			int cmp = (int) this->refcnt - (int) rhs.refcnt;

			/*
			 * @date 2021-07-01 13:58:02
			 * `&rhs-this` does slightly better with 599759 4n9 components, as opposed to 600040 with `this-&rhs`.
			 *
			 * @date 2021-08-08 13:10:00
			 * inverted second ordering to make result more aligned to keep 'lowest' names for last so they have greater chance of surviving
			 *
			 * @date 2021-08-08 22:27:47
			 *
			 * "this - &rhs" (also chooses final names better)
			 *
			 * 2n9 [00:02:26] numMember=33200385 numComponent=652668 numLocked=33114
			 * 3n9 [00:17:33] numMember=15972090 numComponent=402853 numLocked=43632
			 * 4n9 [02:18:07] numMember=588159 numComponent=102316 numLocked=172819
			 * 5n9 [00:17:18] numMember=208976 numComponent=86409 numLocked=208956
			 *
			 * "&rhs - this"
			 *
			 * 2n9 [00:02:29] numMember=33200385 numComponent=652668 numLocked=33114
			 * 3n9 [00:17:08] numMember=15972098 numComponent=402856 numLocked=43632
			 * 4n9 [02:44:23] numMember=590174 numComponent=102308 numLocked=172984
			 * 5n9 [00:21:38] numMember=208986 numComponent=82249 numLocked=208968
			 */
			if (cmp == 0)
				cmp = this - &rhs;
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
		}

		~heap_t() {
			if (buf != 0) {
				free(buf);
				buf = NULL;
			}
		}

		void down(refcnt_t *p) {
			if (p->heapIdx < 0)
				return; // entry not added to heap

			unsigned lo = 0;
			unsigned hi = p->heapIdx;

			while (lo < hi) {
				unsigned t = (lo + hi) >> 1;
				if (p->compar(*buf[t]) > 0)
					lo = t + 1;
				else
					hi = t;
			}
			assert(lo == hi);


			if (lo != (unsigned) p->heapIdx) {
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

			refcnt_t *p = this->buf[--this->count];

			// mark removed from heap
			p->heapIdx = -1;

			return p;
		}

	};

	bool /*__attribute__((optimize("O0")))*/ depreciateFromGenerator(void) {

		unsigned numComponents = 0;

		for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (!(pMember->flags & member_t::MEMMASK_DEPR) && (pMember->flags & member_t::MEMMASK_COMP))
				numComponents++;
		}

		/*
		 * Update ref counts
		 */

		// allocate
		refcnt_t *pRefcnts = (refcnt_t*) ctx.myAlloc("pRefcnts", pStore->numMember, sizeof *pRefcnts);

		// populate
		for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
			member_t *pMember = pStore->members + iMid;

			if (!(pMember->flags & member_t::MEMMASK_DEPR)) {
				if (pMember->Qmt) pRefcnts[pStore->pairs[pMember->Qmt].id].refcnt++;
				if (pMember->Tmt) pRefcnts[pStore->pairs[pMember->Tmt].id].refcnt++;
				if (pMember->Fmt) pRefcnts[pStore->pairs[pMember->Fmt].id].refcnt++;
				if (pMember->heads[0]) pRefcnts[pMember->heads[0]].refcnt++;
				if (pMember->heads[1]) pRefcnts[pMember->heads[1]].refcnt++;
				if (pMember->heads[2]) pRefcnts[pMember->heads[2]].refcnt++;
				if (pMember->heads[3]) pRefcnts[pMember->heads[3]].refcnt++;
				if (pMember->heads[4]) pRefcnts[pMember->heads[4]].refcnt++;
				assert(member_t::MAXHEAD == 5);
			}
		}

		// construct initial heap
		heap_t heap(pStore->numMember, pRefcnts);

		{
			// add candidates to heap
			for (uint32_t iMid = 1; iMid < pStore->numMember; iMid++) {
				member_t *pMember = pStore->members + iMid;

				// entry not on heap
				pRefcnts[iMid].heapIdx = -1;

				// put entry on heap?
				if (arg_numNodes > 0 && pMember->size != arg_numNodes)
					continue;
				if (pMember->flags & member_t::MEMMASK_LOCKED)
					continue;

				// yes
				heap.buf[heap.count++] = pRefcnts + iMid;
			}

			// initial sort
			qsort(heap.buf, heap.count, sizeof *heap.buf, comparHeap);

			// assign back-references
			for (unsigned i = 0; i < heap.count; i++)
				heap.buf[i]->heapIdx = i;
		}

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

		int countDown = 60 * 10; // 10 minutes before restarting

		while (heap.count > 0) {
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

				fprintf(stderr, "\r\e[K[%s] %lu(%3d/s) %.5f%% eta=%d:%02d:%02d | numMember=%u numComponent=%u | cntDepr=%u cntLock=%u | refcnt=%u mid=%u heap=%u %s",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, pStore->numMember - numDepr, numComponents, cntDepr, cntLock, pCurr->refcnt, iMid, heap.count, pMember->name);

				/*
				 * @date 2021-08-08 11:18:04
				 * Speed cack - restart every 10 minutes to shrink the number of members
				 */
				countDown -= ctx.opt_timer;
				if (countDown < 0) {
					fprintf(stderr, "\n[%s] restart\n", ctx.timeAsString());

					ctx.myFree("pRefcnts", pRefcnts);
					return true;
				}

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
			bool allSafe = true;
			if (opt_lookupSafe) {
				// only the lookup signatures must be safe
				for (uint32_t k = 1; k < pStore->numSignature; k++) {
					if (pStore->signatures[k].flags & signature_t::SIGMASK_KEY)
						if (pSafeSid[k] != iVersionSafe) {
							allSafe = false; // rewrites must be safe
							break;
						}
				}
			} else {
				// all signature groups must be safe
				allSafe = (cntSid == pStore->numSignature - 1);
			}

			if (allSafe) {
				// update
				numDepr = pStore->numMember - 1 - cntMid;

				// update burst size
				if (burstSize != opt_burst)
					burstSize >>= 1;

				// display what was selected
				for (unsigned k = 0; k < cntSelect; k++) {
					refcnt_t *pCurr   = heap.pop();
					unsigned iMid     = pCurr - pRefcnts;
					member_t *pMember = pStore->members + iMid;

					ctx.progress++;

					if (opt_text == OPTTEXT_COMPARE)
						printf("D\t%u\t%u\t%u\t%s\n", numComponents, iMid, pCurr->refcnt, pMember->name);
					else if (opt_text == OPTTEXT_WON)
						printf("%s\tD\n", pMember->name);
					cntDepr++;
				}

				// update ref counts
				for (uint32_t iDepr = pStore->numMember - 1; iDepr >= 1; iDepr--) {
					member_t *pDepr = pStore->members + iDepr;

					// depreciate all (new) orphans
					if (pSafeMid[iDepr] != iVersionSafe && !(pDepr->flags & member_t::MEMMASK_DEPR)) {
						assert(!(pDepr->flags & member_t::MEMMASK_LOCKED));
						assert(pRefcnts[iDepr].refcnt == 0);

						// mark depreciated
						pDepr->flags |= member_t::MEMMASK_DEPR;

						// release references and reposition them in list of candidates
						uint32_t mid;

						mid = pStore->pairs[pDepr->Qmt].id;
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						mid = pStore->pairs[pDepr->Tmt].id;
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						mid = pStore->pairs[pDepr->Fmt].id;
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						mid = pDepr->heads[0];
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						mid = pDepr->heads[1];
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						mid = pDepr->heads[2];
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						mid = pDepr->heads[3];
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						mid = pDepr->heads[4];
						if (mid) {
							assert(pRefcnts[mid].refcnt > 0);
							pRefcnts[mid].refcnt--;
							heap.down(pRefcnts + mid);
						}
						assert(member_t::MAXHEAD == 5);

						// if a component, update counter
						if (pDepr->flags & member_t::MEMMASK_COMP)
							--numComponents;
					}
				}

			} else if (cntSelect == 1) {
				// pop the member and mark as locked
				refcnt_t *pCurr = heap.pop();
				unsigned iMid     = pCurr - pRefcnts;
				member_t *pMember = pStore->members + iMid;

				ctx.progress++;

				if (!(pMember->flags & member_t::MEMMASK_LOCKED)) {
					pMember->flags |= member_t::MEMMASK_LOCKED;

					if (opt_text == OPTTEXT_COMPARE)
						printf("L\t%u\t%u\t%u\t%s\n", numComponents, iMid, pCurr->refcnt, pMember->name);
					else if (opt_text == OPTTEXT_WON)
						printf("%s\tL\n", pMember->name);

					cntLock++;
				}

				// reset burst size
				burstSize = opt_burst;
			} else {
				// decrease burst size and try again
				burstSize >>= 1;

			}
		}
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * Empty signatures lose their SAFE state
		 */
		for (uint32_t iSid = 1; iSid < pStore->numSignature; iSid++) {
			signature_t *pSignature = pStore->signatures + iSid;

			if (pSignature->firstMember == 0)
				pSignature->flags &= ~signature_t::SIGMASK_SAFE;

		}

		unsigned numLocked = updateLocked();

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numMember=%u numComponent=%u numLocked=%u | cntDepr=%u cntLock=%u\n", ctx.timeAsString(), pStore->numMember - numDepr, numComponents, numLocked, cntDepr, cntLock);

		ctx.myFree("pRefcnts", pRefcnts);

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
			} else if ((pMember->Qmt == 0 || pSafeMid[pStore->pairs[pMember->Qmt].id] == iVersionSafe)
					&& (pMember->Tmt == 0 || pSafeMid[pStore->pairs[pMember->Tmt].id] == iVersionSafe)
					&& (pMember->Fmt == 0 || pSafeMid[pStore->pairs[pMember->Fmt].id] == iVersionSafe)
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
