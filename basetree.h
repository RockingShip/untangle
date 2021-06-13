#ifndef _BASETREE_H
#define _BASETREE_H

#include <fcntl.h>
#include <jansson.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>
#include "context.h"

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

/*
 * Version number of data file
 */
#define BASETREE_MAGIC 0x20210613

#if !defined(DEFAULT_MAXNODE)
/**
 * The maximum number of nodes a writable tree can hold is indicated with the `--maxnode=n` option.
 * When saving, trees become read-only and are shrink to fit.
 * This is the default value for `--maxnode=`.
 *
 * NOTE: for `baseTree_t` this will allocate at least 11 arrays of DEFAULT_MAXNODE*sizeof(uint32_t)
 *
 * @constant {number} DEFAULT_MAXNODE
 */
#define DEFAULT_MAXNODE 100000000
#endif

#if !defined(MAXPOOLARRAY)
/**
 * Renumbering/remapping node id's is a major operation within `baseTree_t`.
 * Instead of allocating new maps with chance of memory fragmentation, reuse previously released maps.
 * The are two pools of maps, one specifically for containing node id's, the other for containing versioned memory id's.
 * Maps are `--maxnode=` entries large.
 *
 * @constant {number} MAXPOOLARRAY
 */
#define MAXPOOLARRAY 128
#endif

/*
 * The structure based compare is also the reference implementation on how to walk the tree.
 * This is a speed optimisation and enables code to display the decisions taken while walking.
 * Displaying is performed when running with `--debug=`.
 */
#ifndef ENABLE_DEBUG_COMPARE
#define ENABLE_DEBUG_COMPARE 0
#endif

struct baseNode_t {
	uint32_t Q;                // the question
	uint32_t T;                // the answer if true (may be inverted)
	uint32_t F;                // the answer if false

	// OR (L?~0:R) is first because it has the QnTF signature
	inline bool __attribute__((pure)) isOR(void) const {
		return T == IBIT;
	}

	// GT (L?~R:0) is second because it has the QnTF signature
	inline bool __attribute__((pure)) isGT(void) const {
		return (T & IBIT) && F != 0;
	}

	// NE (L?~R:R) third because Ti is set (QnTF) but Tu==F
	inline bool __attribute__((pure)) isNE(void) const {
		return (T & ~IBIT) == F;
	}

	// AND (L?R:0) last because not QnTF
	inline bool __attribute__((pure)) isAND(void) const {
		return !(T & IBIT) && F == 0;
	}

};

/*
 * The database file header
 */
struct baseTreeHeader_t {
	// meta
	uint32_t magic;               // magic+version
	uint32_t magic_flags;         // conditions it was created
	uint32_t unused1;             //
	uint32_t system;              // node of balanced system (0 if none)
	uint32_t crc32;               // crc of nodes/roots, calculated during save

	// primary fields
	uint32_t kstart;              // first input key id
	uint32_t ostart;              // first output key id
	uint32_t estart;              // first external/extended key id.
	uint32_t nstart;              // id of first node
	uint32_t ncount;              // number of nodes in use
	uint32_t numRoots;            // entries in roots[]

	uint32_t numHistory;          //
	uint32_t posHistory;          //

	// section offsets
	uint64_t offNames;            // length stored in `strlen((char*)header+offNames)`
	uint64_t offNodes;            // length stored in `count`
	uint64_t offRoots;            // length stored in `numRoots`
	uint64_t offHistory;          // length stored in `numHistory`

	uint64_t offEnd;
};

struct baseTree_t {

	/*
	 * Flags to indicate if sections were allocated or mapped
	 */
	enum {
		//@formatter:off
		ALLOCFLAG_NAMES = 0,	// key/root names
		ALLOCFLAG_NODES,	// nodes
		ALLOCFLAG_ROOTS,	// roots
		ALLOCFLAG_HISTORY,	// history
		ALLOCFLAG_INDEX,	// node index/lookup table

		ALLOCMASK_NAMES   = 1 << ALLOCFLAG_NAMES,
		ALLOCMASK_NODES   = 1 << ALLOCFLAG_NODES,
		ALLOCMASK_ROOTS   = 1 << ALLOCFLAG_ROOTS,
		ALLOCMASK_HISTORY = 1 << ALLOCFLAG_HISTORY,
		ALLOCMASK_INDEX   = 1 << ALLOCFLAG_INDEX,
		//@formatter:on
	};

	//@formatter:off
	// resources
	context_t  &ctx;		// resource context
	int        hndl;                // file handle
	uint8_t    *rawDatabase;        // base location of mmap segment
	baseTreeHeader_t *fileHeader;   // file header
	// meta
	uint32_t   flags;		// creation constraints
	uint32_t   allocFlags;		// memory constraints
	uint32_t   unused1;		//
	uint32_t   system;		// node of balanced system
	// primary fields
	uint32_t   kstart;		// first input key id.
	uint32_t   ostart;		// first output key id.
	uint32_t   estart;		// first external/extended key id. Roots from previous tree in chain.
	uint32_t   nstart;		// id of first node
	uint32_t   ncount;		// number of nodes in use
	uint32_t   maxNodes;		// maximum tree capacity
	uint32_t   numRoots;		// entries in roots[]
	// names
	std::vector<std::string>keyNames;  // sliced version of `keyNameData`
	std::vector<std::string>rootNames; // sliced version of `rootNameData`
	// primary storage
	baseNode_t *N;			// nodes
	uint32_t   *roots;		// entry points. can be inverted. first estart entries should match keys
	// history
	uint32_t   numHistory;		//
	uint32_t   posHistory;		//
	uint32_t   *history;		//
	// node index
	uint32_t   nodeIndexSize;	// hash/cache size. MUST BE PRIME!
	uint32_t   *nodeIndex;		// index to nodes
	uint32_t   *nodeIndexVersion;	// content version
	uint32_t   nodeIndexVersionNr;	// active version number
	// pools
	unsigned   numPoolMap;		// Number of node-id pools in use
	uint32_t   **pPoolMap;		// Pool of available node-id maps
	unsigned   numPoolVersion;	// Number of version-id pools in use
	uint32_t   **pPoolVersion;	// Pool of available version-id maps
	uint32_t   mapVersionNr;	// Version number
	// structure based compare
	uint32_t   *stackL;		// id of lhs
	uint32_t   *stackR;		// id of rhs
	uint32_t   *compNodeL;		// versioned memory for compare - visited node id Left
	uint32_t   *compNodeR;
	uint32_t   *compVersionL;	// versioned memory for compare - content version
	uint32_t   *compVersionR;
	uint32_t   compVersionNr;	// versioned memory for compare - active version number
	uint64_t   numCompare;		// number of compares performed
	// reserved for evaluator

	//@formatter:on

	/**
	 * @date 2021-06-13 00:01:50
	 *
	 * Copy/Assign constructors not supported.
	 * Let usage trigger a "linker not found" error
	 */
	baseTree_t(const baseTree_t &rhs);
	baseTree_t &operator=(const baseTree_t &rhs);
\
	/*
	 * Create an empty tree, placeholder for reading from file
	 */
	baseTree_t(context_t &ctx) :
	//@formatter:off
		ctx(ctx),
		hndl(-1),
		rawDatabase(NULL),
		fileHeader(NULL),
		// meta
		flags(0),
		allocFlags(0),
		unused1(0),
		system(0),
		// primary fields
		kstart(0),
		ostart(0),
		estart(0),
		nstart(0),
		ncount(0),
		maxNodes(0),
		numRoots(0),
		// names
		keyNames(),
		rootNames(),
		// primary storage (allocated by storage context)
		N(NULL),
		roots(NULL),
		// history
		numHistory(0),
		posHistory(0),
		history(NULL),
		// node index
		nodeIndexSize(0),
		nodeIndex(NULL),
		nodeIndexVersion(NULL),
		nodeIndexVersionNr(1),
		// pools
		numPoolMap(0),
		pPoolMap(NULL),
		numPoolVersion(0),
		pPoolVersion(NULL),
		mapVersionNr(0),
		// structure based compare (NOTE: needs to go after pools!)
		stackL(NULL),
		stackR(NULL),
		compNodeL(NULL),
		compNodeR(NULL),
		compVersionL(NULL), // allocate as node-id map because of local version numbering
		compVersionR(NULL),  // allocate as node-id map because of local version numbering
		compVersionNr(1),
		numCompare(0)
	//@formatter:on
	{
	}

	/*
	 * Create a memory stored tree
	 */
	baseTree_t(context_t &ctx, uint32_t kstart, uint32_t ostart, uint32_t estart, uint32_t nstart, uint32_t numRoots, uint32_t maxNodes, uint32_t flags) :
	//@formatter:off
		ctx(ctx),
		hndl(-1),
		rawDatabase(NULL),
		fileHeader(NULL),
		// meta
		flags(flags),
		allocFlags(0),
		unused1(0),
		system(0),
		// primary fields
		kstart(kstart),
		ostart(ostart),
		estart(estart),
		nstart(nstart),
		ncount(nstart),
		maxNodes(maxNodes),
		numRoots(numRoots),
		// names
		keyNames(),
		rootNames(),
		// primary storage (allocated by storage context)
		N((baseNode_t *) ctx.myAlloc("baseTree_t::N", maxNodes, sizeof *N)),
		roots((uint32_t *) ctx.myAlloc("baseTree_t::roots", numRoots, sizeof *roots)),
		// history
		numHistory(0),
		posHistory(0),
		history((uint32_t *) ctx.myAlloc("baseTree_t::history", nstart, sizeof *history)),
		// node index  NOTE: reserve 4G for the node+version index
		nodeIndexSize(536870879), // first prime number before 0x20000000-8 (so that 4*this does not exceed 0x80000000-32),
		nodeIndex((uint32_t *) ctx.myAlloc("baseTree_t::nodeIndex", nodeIndexSize, sizeof *nodeIndex)),
		nodeIndexVersion((uint32_t *) ctx.myAlloc("baseTree_t::nodeIndexVersion", nodeIndexSize, sizeof *nodeIndexVersion)),
		nodeIndexVersionNr(1), // own version because longer life span
		// pools
		numPoolMap(0),
		pPoolMap((uint32_t **) ctx.myAlloc("baseTree_t::pPoolMap", MAXPOOLARRAY, sizeof(*pPoolMap))),
		numPoolVersion(0),
		pPoolVersion( (uint32_t **) ctx.myAlloc("baseTree_t::pPoolVersion", MAXPOOLARRAY, sizeof(*pPoolVersion))),
		mapVersionNr(0),
		// structure based compare (NOTE: needs to go after pools!)
		stackL(allocMap()),
		stackR(allocMap()),
		compNodeL(allocMap()),
		compNodeR(allocMap()),
		compVersionL(allocMap()), // allocate as node-id map because of local version numbering
		compVersionR(allocMap()),  // allocate as node-id map because of local version numbering
		compVersionNr(1),
		numCompare(0)
	//@formatter:on
	{
		if (this->N)
			allocFlags |= ALLOCMASK_NODES;
		if (this->roots)
			allocFlags |= ALLOCMASK_ROOTS;
		if (this->history)
			allocFlags |= ALLOCMASK_HISTORY;
		if (this->nodeIndex)
			allocFlags |= ALLOCMASK_INDEX;

		// make all `keyNames`+`rootNames` indices valid
		keyNames.resize(nstart);
		rootNames.resize(numRoots);

		// setup default keys
		for (unsigned iKey = 0; iKey < nstart; iKey++) {
			N[iKey].Q = 0;
			N[iKey].T = IBIT;
			N[iKey].F = iKey;
		}

		// setup default roots
		for (unsigned iRoot = 0; iRoot < numRoots; iRoot++)
			roots[iRoot] = iRoot;

#if ENABLE_BASEEVALUATOR
		gBaseEvaluator = new baseEvaluator_t(kstart, ostart, nstart, maxNodes);
#endif
	}

	/*
	 * Release system resources
	 */
	~baseTree_t() {
		// release allocations if not mmapped
		if (allocFlags & ALLOCMASK_NODES)
			ctx.myFree("baseTree_t::N", this->N);
		if (allocFlags & ALLOCMASK_ROOTS)
			ctx.myFree("baseTree_t::roots", this->roots);
		if (allocFlags & ALLOCMASK_HISTORY)
			ctx.myFree("baseTree_t::history", this->history);
		if (allocFlags & ALLOCMASK_INDEX) {
			ctx.myFree("baseTree_t::nodeIndex", this->nodeIndex);
			ctx.myFree("baseTree_t::nodeIndexVersion", this->nodeIndexVersion);
		}

		// release maps
		if (stackL)
			freeMap(stackL);
		if (stackR)
			freeMap(stackR);
		if (compVersionL)
			freeMap(compVersionL);
		if (compVersionR)
			freeMap(compVersionR);
		if (compNodeL)
			freeMap(compNodeL);
		if (compNodeR)
			freeMap(compNodeR);

		// release pools
		while (numPoolMap > 0)
			ctx.myFree("baseTree_t::nodeMap", pPoolMap[--numPoolMap]);
		while (numPoolVersion > 0)
			ctx.myFree("baseTree_t::versionMap", pPoolVersion[--numPoolVersion]);

		// release resources
		if (hndl >= 0) {
			/*
			 * Database is open an `mmap()`
			 */
			int ret;
			ret = ::munmap((void *) rawDatabase, fileHeader->offEnd);
			if (ret)
				ctx.fatal("munmap() returned: %m\n");
			ret = ::close(hndl);
			if (ret)
				ctx.fatal("close() returned: %m\n");
		} else if (rawDatabase) {
			/*
			 * Database was read into `malloc()` buffer
			 */
			ctx.myFree("baseTreeFile_t::rawDatabase", rawDatabase);
		}

		// zombies need to trigger SEGV
		rawDatabase      = NULL;
		fileHeader       = NULL;
		N                = NULL;
		roots            = NULL;
		history          = NULL;
		nodeIndex        = NULL;
		nodeIndexVersion = NULL;
		pPoolMap         = NULL;
		pPoolVersion     = NULL;
		stackL           = NULL;
		stackR           = NULL;
		compNodeL        = NULL;
		compNodeR        = NULL;
		compVersionL     = NULL;
		compVersionR     = NULL;
	}

	/*
	 * Rewind, reset nodes and invalidate node cache
	 */
	void rewind(void) {
		// rewind nodes
		this->ncount = this->nstart;
		// invalidate lookup cache
		++this->nodeIndexVersionNr;

	}

	/*
	 * Pool management
	 */

	/**
	 * @date 2021-05-12 00:44:46
	 *
	 * Allocate a map that can hold node id's
	 * Returned map is uninitialised
	 *
	 * @return {uint32_t*} - Uninitialised map for node id's
	 */
	uint32_t *allocMap(void) {
		uint32_t *pMap;

		if (numPoolMap > 0) {
			// get first free node map
			pMap = pPoolMap[--numPoolMap];
		} else {
			// allocate new map
			pMap = (uint32_t *) ctx.myAlloc("baseTree_t::versionMap", maxNodes, sizeof *pMap);
		}

		return pMap;
	}

	/**
	 * @date 2021-05-12 00:46:05
	 *
	 * Release a node-id map
	 *
	 * @param {uint32_t*} pMap
	 */
	void freeMap(uint32_t *&pMap) {
		if (numPoolMap >= MAXPOOLARRAY)
			ctx.fatal("context.h:MAXPOOLARRAY too small\n");

		// store the current root in map
		pPoolMap[numPoolMap++] = pMap;
		pMap = NULL;
	}

	/**
	 * @date 2021-05-12 00:45:03
	 *
	 * Allocate a map that can hold versioned memory id's
	 * Returned map is uninitialised and should ONLY contain previous (lower) version numbers

	 * NOTE: caller needs clear map on `mapVersionNr` wraparound
	 *
	 * @return {uint32_t*} - Uninitialised map for versioned memory id's
	 */
	uint32_t *allocVersion(void) {
		uint32_t *pVersion;

		if (numPoolVersion > 0) {
			// get first free version map
			pVersion = pPoolVersion[--numPoolVersion];
		} else {
			// allocate new map
			pVersion = (uint32_t *) ctx.myAlloc("baseTree_t::versionMap", maxNodes, sizeof *pVersion);
		}

		return pVersion;
	}

	/**
	 * @date 2021-05-12 00:49:00
	 *
	 * Release a version-id map
	 *
	 * @param {uint32_t*} pMap
	 */
	void freeVersion(uint32_t *&pVersion) {
		if (numPoolVersion >= MAXPOOLARRAY)
			ctx.fatal("context.h:MAXPOOLARRAY too small\n");

		// store the current root in Version
		pPoolVersion[numPoolVersion++] = pVersion;
		pVersion = NULL;
	}

	/*
	 * @date 2021-05-12 01:23:06
	 *
	 * Compare two-subtrees
	 *
	 * NOTE: Only key id's can be compared, node id's cannot be compared and need to be expanded
	 *
	 * return:
	 *      -3 structure leftHandSide LESS rightHandSide
	 *      -2 same structure but endpoints leftHandSide LESS rightHandSide
	 *      -1 leftHandSide fits in rightHandSide
	 *       0 EQUAL
	 *      +1 rightHandSide fits in leftHandSide
	 *      +2 same structure but endpoints leftHandSide GREATER rightHandSide
	 *      +3 structure leftHandSide GREATER rightHandSide
	 */
	static int compare(baseTree_t *treeL, uint32_t lhs, baseTree_t *treeR, uint32_t rhs) {

		context_t &ctx = treeL->ctx; // use resources from L

		/*
		 * This code is a resource hit, but worth the effort.
		 * With the new QnTF components, there are only 3 things that differentiate left.
		 * Zero as grounding stop this function recursing all the way to the endpoints.
		 * This will hopefully stop swap-hotspot oscillation making nested `grows` synchronise.
		 *
		 * sidenote:
		 * The detector will reorder left/right appropriately, so no chance a swapped version will be created anywhere else.
		 *
		 * This function is designed to be not recursive, so static variables are no longer required.
		 */

		uint32_t thisVersionL = ++treeL->compVersionNr;
		uint32_t thisVersionR = ++treeR->compVersionNr;
		// version overflow, clear
		if (thisVersionL == 0) {
			::memset(treeL->compVersionL, 0, treeL->maxNodes * sizeof *compVersionL);
			thisVersionL = ++treeL->compVersionNr;
		}
		if (thisVersionR == 0) {
			::memset(treeR->compVersionR, 0, treeR->maxNodes * sizeof *compVersionR);
			thisVersionR = ++treeR->compVersionNr;
		}
		treeL->numCompare++; // only for L

		int secondary = 0;

		assert(~lhs & IBIT);
		assert(~rhs & IBIT);

		// push arguments on stack
		treeL->stackL[0] = lhs;
		treeR->stackR[0] = rhs;

		uint32_t numStack = 1; // top of stack
		uint32_t nextNode = 1; // relative node

		if (ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE))
			fprintf(stderr, "compare(%x,%x)\n", lhs, rhs);

		do {
			// pop stack
			numStack--;
			uint32_t L = treeL->stackL[numStack];
			uint32_t R = treeR->stackR[numStack];

			// for same tree, identical lhs/rhs implies equal
			if (L == R)
				continue;

			if (ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE))
				fprintf(stderr, "%x:[%x %x %x] %x:[%x %x %x]\n",
					L, treeL->N[L].Q, treeL->N[L].T, treeL->N[L].F,
					R, treeR->N[R].Q, treeR->N[R].T, treeR->N[R].F);

			// compare known/unknown
			if ((treeL->compVersionL[L] == thisVersionL) && (treeR->compVersionR[R] != thisVersionR))
				return -1;
			if ((treeL->compVersionL[L] != thisVersionL) && (treeR->compVersionR[R] == thisVersionR))
				return +1;

			// compare endpoint/tree
			if (L < treeL->nstart && R >= treeR->nstart && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1a\n");
			if (L < treeL->nstart && R >= treeR->nstart)
				return -1;
			if (L >= treeL->nstart && R < treeR->nstart && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1a\n");
			if (L >= treeL->nstart && R < treeR->nstart)
				return +1;

			if (L < treeL->nstart && R < treeR->nstart) {
				// compare endpoint/endpoint
				if (secondary == 0) {
					if (ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) { if (L < R) fprintf(stderr, "-2\n"); else if (L > R) fprintf(stderr, "+2\n"); }
					// compare contents, not structure
					if (L < R)
						secondary = -2;
					else if (L > R)
						secondary = +2;
				}
				continue;
			} else {
				// compare relative node numbers
				if (treeL->compVersionL[L] == thisVersionL) {
					if (treeL->compNodeL[L] < treeR->compNodeR[R])
						return -1;
					if (treeL->compNodeL[L] > treeR->compNodeR[R])
						return +1;
				}
			}

			// determine if already handled
			if (treeL->compVersionL[L] == thisVersionL)
				continue;
			treeL->compVersionL[L] = thisVersionL;
			treeR->compVersionR[R] = thisVersionR;
			treeL->compNodeL[L]    = nextNode;
			treeR->compNodeR[R]    = nextNode;
			nextNode++;

			const baseNode_t *pNodeL = treeL->N + L;
			const baseNode_t *pNodeR = treeR->N + R;

			// compare Ti
			if ((pNodeL->T & IBIT) && (~pNodeR->T & IBIT) && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1b\n");
			if ((pNodeL->T & IBIT) && (~pNodeR->T & IBIT))
				return -1;
			if ((~pNodeL->T & IBIT) && (pNodeR->T & IBIT) && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1b\n");
			if ((~pNodeL->T & IBIT) && (pNodeR->T & IBIT))
				return +1;

			// compare OR
			if (pNodeL->T == IBIT && pNodeR->T != IBIT && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1c\n");
			if (pNodeL->T == IBIT && pNodeR->T != IBIT)
				return -1;
			if (pNodeL->T != IBIT && pNodeR->T == IBIT && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1c\n");
			if (pNodeL->T != IBIT && pNodeR->T == IBIT)
				return +1;

			// compare LESS-THAN
			if (pNodeL->F == 0 && pNodeR->F != 0 && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1d\n");
			if (pNodeL->F == 0 && pNodeR->F != 0)
				return -1;
			if (pNodeL->F != 0 && pNodeR->F == 0 && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1d\n");
			if (pNodeL->F != 0 && pNodeR->F == 0)
				return +1;

			// compare NOT-EQUAL
			if ((pNodeL->T & ~IBIT) == pNodeL->F && (pNodeR->T & ~IBIT) != pNodeR->F && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1e\n");
			if ((pNodeL->T & ~IBIT) == pNodeL->F && (pNodeR->T & ~IBIT) != pNodeR->F)
				return -1;
			if ((pNodeL->T & ~IBIT) != pNodeL->F && (pNodeR->T & ~IBIT) == pNodeR->F && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1e\n");
			if ((pNodeL->T & ~IBIT) != pNodeL->F && (pNodeR->T & ~IBIT) == pNodeR->F)
				return +1;

			// compare component
			if (pNodeL->F != pNodeR->F) {
				treeL->stackL[numStack] = pNodeL->F;
				treeR->stackR[numStack] = pNodeR->F;
				numStack++;
			}

			if ((pNodeL->T & ~IBIT) != (pNodeR->T & ~IBIT)) {
				treeL->stackL[numStack] = (pNodeL->T & ~IBIT);
				treeR->stackR[numStack] = (pNodeR->T & ~IBIT);
				numStack++;
			}

			if (pNodeL->Q != pNodeR->Q) {
				treeL->stackL[numStack] = pNodeL->Q;
				treeR->stackR[numStack] = pNodeR->Q;
				numStack++;
			}

		} while (numStack > 0);

		assert(secondary || lhs == rhs); // secondary==0 implies lhs==rhs

		// structure identical, return comparison based on contents
		if (ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "secondary:%d\n", secondary);

		return secondary;
	}

	/*
	 * @date 2021-05-12 18:18:58
	 *
	 * Difference between these tests and the `baseNode_t` versions, is that these also test is arguments are keys
	 */

	// OR is first because it has the QnTF signature
	inline bool __attribute__((pure)) isOR(uint32_t i) const {
		return i >= nstart && N[i].isOR();
	}

	// NE second because Ti is set (QnTF) buf Tu==F
	inline bool __attribute__((pure)) isNE(uint32_t i) const {
		return i >= nstart && N[i].isNE();
	}

	// AND last because not QnTF
	inline bool __attribute__((pure)) isAND(uint32_t i) const {
		return i >= nstart && N[i].isAND();
	}

	inline bool __attribute__((const)) isOR(uint32_t Q, uint32_t T, uint32_t F) const {
		return T == IBIT;
	}

	inline bool __attribute__((const)) isNE(uint32_t Q, uint32_t T, uint32_t F) const {
		return (T & ~IBIT) == F;
	}

	inline bool __attribute__((const)) isAND(uint32_t Q, uint32_t T, uint32_t F) const {
		return !(T & IBIT) && F == 0;
	}

	/*
	 * @date 2021-05-13 00:38:48
	 *
	 * Lookup a node
	 */
	inline uint32_t lookupNode(uint32_t Q, uint32_t T, uint32_t F) {

		ctx.cntHash++;

		// starting position
		uint32_t crc32 = 0;
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(Q));
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(T));
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(F));

		uint32_t ix   = (uint32_t) (crc32 % this->nodeIndexSize);
		uint32_t bump = ix;
		if (!bump) bump++;

		for (;;) {
			ctx.cntCompare++;
			if (this->nodeIndexVersion[ix] != this->nodeIndexVersionNr) {
				// let caller finalise index
				this->nodeIndex[ix] = 0;
				return ix;
			}

			const baseNode_t *pNode = this->N + this->nodeIndex[ix];
			if (pNode->Q == Q && pNode->T == T && pNode->F == F)
				return ix;

			ix += bump;
			if (ix >= this->nodeIndexSize)
				ix -= this->nodeIndexSize;
		}
	}

	/*
	 * @date 2021-05-13 00:44:53
	 *
	 * Create a new node
	 */
	inline uint32_t newNode(uint32_t Q, uint32_t T, uint32_t F) {

		uint32_t id = this->ncount++;

		if (id > maxNodes - 10) {
			fprintf(stderr, "[OVERFLOW]\n");
			printf("{\"error\":\"overflow\",\"maxnode\":%d}\n", maxNodes);
			exit(1);
		}

		assert(id < maxNodes);

		this->N[id].Q = Q;
		this->N[id].T = T;
		this->N[id].F = F;

		return id;
	}

	/*
	 * @date 2021-05-13 00:30:42
	 *
	 * lookup/create a basic (normalised) node.
	 */
	inline uint32_t basicNode(uint32_t Q, uint32_t T, uint32_t F) {

		/*
		 *  [ 2] a ? !0 : b                  "+" or
		 *  [ 6] a ? !b : 0                  ">" greater-than
		 *  [ 8] a ? !b : b                  "^" not-equal
		 *  [ 9] a ? !b : c                  "#" QnTF
		 *  [12] a ?  0 : b -> b ? !a : 0
		 *  [16] a ?  b : 0                  "&" and
		 *  [19] a ?  b : c                  "?" QTF
		 */

		if (this->flags & ctx.MAGICMASK_PARANOID) {
			assert (!Q || Q >= this->kstart);
			assert (!(T & ~IBIT) || (T & ~IBIT) >= this->kstart);
			assert (!F || F >= this->kstart);

			assert (Q < this->ncount);
			assert ((T & ~IBIT) < this->ncount);
			assert (F < this->ncount);

			assert(~Q & IBIT);            // Q not inverted
			assert((T & IBIT) || !(this->flags & ctx.MAGICMASK_PURE));
			assert(~F & IBIT);            // F not inverted
			assert(Q != 0);               // Q not zero
			assert(T != 0);               // Q?0:F -> F?!Q:0
			assert(F != 0 || T != IBIT);  // Q?!0:0 -> Q
			assert(Q != (T & ~IBIT));     // Q/T collapse
			assert(Q != F);               // Q/F collapse
			assert(T != F);               // T/F collapse

			assert((T & ~IBIT) != F || this->compare(this, Q, this, F) < 0);     // NE ordering
			assert(F != 0 || (T & IBIT) || this->compare(this, Q, this, T) < 0); // AND ordering
			assert(T != IBIT || this->compare(this, Q, this, F) < 0);            // OR ordering

			// OR ordering and basic chain
			if (T == IBIT)
				assert(this->compare(this, Q, this, F) < 0);
			// NE ordering
			if ((T & ~IBIT) == F)
				assert(this->compare(this, Q, this, F) < 0);
			// AND ordering
			if (F == 0 && (~T & IBIT))
				assert(this->compare(this, Q, this, T) < 0);

			if (this->flags & ctx.MAGICMASK_CASCADE) {
				if (T == IBIT)
					assert (!this->isOR(Q) || !this->isOR(F));
				if ((T & ~IBIT) == F)
					assert (!this->isNE(Q) || !this->isNE(F));
				if (F == 0 && (~T & IBIT))
					assert (!this->isAND(Q) || !this->isAND(T & ~IBIT));
			}
		}

		ctx.cntHash++;

		// lookup
		uint32_t ix = lookupNode(Q, T, F);
		if (this->nodeIndex[ix] == 0) {

#if ENABLE_BASEEVALUATOR
			baseFootprint_t *v = gBaseEvaluator->evalData64;

			const uint64_t   *vQ     = v[Q].bits;
			const uint64_t   *vT     = v[T & ~IBIT].bits;
			const uint64_t   *vF     = v[F].bits;

			uint64_t         *vR     = v[this->count].bits;

			if (T & IBIT) {
				for (uint32_t j=0; j<BASEQUADPERFOOTPRINT; j++)
					vR[j] = (~vQ[j] & vF[j]) ^ (vQ[j] & ~vT[j]);
			} else {
				for (uint32_t j=0; j<BASEQUADPERFOOTPRINT; j++)
					vR[j] = (~vQ[j] & vF[j]) ^ (vQ[j] & vT[j]);
			}

			// test if already found
			for (uint32_t i=0; i<this->ncount; i++) {
				if (v[i].equals(v[this->count])) {
					fprintf(stderr,".");
					assert (i != 27);
					return i;
				}
			}
#endif

			// create
			this->nodeIndex[ix]        = newNode(Q, T, F);
			this->nodeIndexVersion[ix] = this->nodeIndexVersionNr;
		}

		if ((this->flags & ctx.MAGICMASK_PARANOID) && (this->flags & ctx.MAGICMASK_CASCADE)) {
			// ordered chains
			uint32_t iNode = this->nodeIndex[ix];

			if (this->isOR(iNode)) {
				uint32_t top = 0;

				for (;;) {
					const baseNode_t *pNode = this->N + iNode;
					const uint32_t   Q      = pNode->Q;
//					const uint32_t   Tu     = pNode->T & ~IBIT;
//					const uint32_t   Ti     = pNode->T & IBIT;
					const uint32_t   F      = pNode->F;

					if (this->isOR(Q)) {
						if (top) { assert(this->compare(this, F, this, top) < 0); }
						top   = F;
						iNode = Q;
					} else if (this->isOR(F)) {
						if (top) { assert(this->compare(this, Q, this, top) < 0); }
						top   = Q;
						iNode = F;
					} else {
						if (top) { assert(this->compare(this, F, this, top) < 0); }
						break;
					}
				}

			} else if (this->isNE(iNode)) {
				uint32_t top = 0;

				for (;;) {
					const baseNode_t *pNode = this->N + iNode;
					const uint32_t   Q      = pNode->Q;
//					const uint32_t   Tu     = pNode->T & ~IBIT;
//					const uint32_t   Ti     = pNode->T & IBIT;
					const uint32_t   F      = pNode->F;

					if (this->isNE(Q)) {
						if (top) { assert(this->compare(this, F, this, top) < 0); }
						top   = F;
						iNode = Q;
					} else if (this->isNE(F)) {
						if (top) { assert(this->compare(this, Q, this, top) < 0); }
						top   = Q;
						iNode = F;
					} else {
						if (top) { assert(this->compare(this, F, this, top) < 0); }
						break;
					}
				}

			} else if (this->isAND(iNode)) {
				uint32_t top = 0;

				for (;;) {
					const baseNode_t *pNode = this->N + iNode;
					const uint32_t   Q      = pNode->Q;
					const uint32_t   Tu     = pNode->T & ~IBIT;
//					const uint32_t   Ti     = pNode->T & IBIT;
//					const uint32_t   F      = pNode->F;

					if (this->isAND(Q)) {
						if (top) { assert(this->compare(this, Tu, this, top) < 0); }
						top   = Tu;
						iNode = Q;
					} else if (this->isAND(Tu)) {
						if (top) { assert(this->compare(this, Q, this, top) < 0); }
						top   = Q;
						iNode = Tu;
					} else {
						if (top) { assert(this->compare(this, Tu, this, top) < 0); }
						break;
					}
				}

			}
		}

		return this->nodeIndex[ix];
	}

	/*
	 * @date  2021-05-12 18:08:34
	 *
	 * lookup/create and normalise any combination of Q, T and F, inverted or not.
	 * NOTE: the return value may be inverted
	 * This call is the isolation layer between the existence of inverts.
	 * The callers of this function should propagate invert to the root
	 * The workers of this function: invert no longer exists!! remove all the code and logic related.
	 * Level 3 rewrites make `lookupNode()` lose it's meaning
	 */
	uint32_t normaliseNode(uint32_t Q, uint32_t T, uint32_t F) {

		assert ((Q & ~IBIT) < this->ncount);
		assert ((T & ~IBIT) < this->ncount);
		assert ((F & ~IBIT) < this->ncount);

		/*
		 * Level 1 normalisation: invert propagation
		 *
		 * !a ?  b :  c  ->  a ? c : b
		 *  0 ?  b :  c  ->  c
		 *  a ?  b : !c  ->  !(a ? !b : c)
		 */

		if (Q & IBIT) {
			// "!Q?T:F" -> "Q?F:T"
			uint32_t savT = T;
			T = F;
			F = savT;
			Q ^= IBIT;
		}
		if (Q == 0) {
			// "0?T:F" -> "F"
			return F;
		}

		uint32_t ibit = 0;

		if (F & IBIT) {
			// "Q?T:!F" -> "!(Q?!T:F)"
			F ^= IBIT;
			T ^= IBIT;
			ibit ^= IBIT;
		}

		/*
		 * Level 2 normalisation: single node rewrites
		 *
		 * appreciated:
		 *
		 *  [ 0] a ? !0 : 0  ->  a
		 *  [ 1] a ? !0 : a  ->  a ? !0 : 0
		 *  [ 2] a ? !0 : b                  "+" or
		 *  [ 3] a ? !a : 0  ->  0
		 *  [ 4] a ? !a : a  ->  a ? !a : 0
		 *  [ 5] a ? !a : b  ->  b ? !a : b
		 *  [ 6] a ? !b : 0                  ">" greater-than
		 *  [ 7] a ? !b : a  ->  a ? !b : 0
		 *  [ 8] a ? !b : b                  "^" not-equal
		 *  [ 9] a ? !b : c                  "#" QnTF
		 *
		 * depreciated:
		 *  [10] a ?  0 : 0 -> 0
		 *  [11] a ?  0 : a -> 0
		 *  [12] a ?  0 : b -> b ? !a : 0
		 *  [13] a ?  a : 0 -> a
		 *  [14] a ?  a : a -> a ?  a : 0
		 *  [15] a ?  a : b -> a ? !0 : b
		 *  [16] a ?  b : 0                  "&" and
		 *  [17] a ?  b : a -> a ?  b : 0
		 *  [18] a ?  b : b -> b
		 *  [19] a ?  b : c                  "?" QTF
		 *
 		 * ./eval --raw 'a0a#' 'a0b#' 'aaa#' 'aab#' 'aba#' 'abb#' 'abc#' 'a0a?' 'a0b?' 'aaa?' 'aab?' 'aba?' 'abb?' 'abc?'
 		 *
		 */

		if (this->flags & ctx.MAGICMASK_REWRITE) {
			/*
			 * @date 2021-05-13 01:25:13
			 * todo: MAGICMASK_REWRITE placeholder
			 */
			assert(!"todo: MAGICMASK_REWRITE placeholder");
			baseNode_t normalised; // = normaliseRewrite(this, Q, T, F);

			// test for collapse
			if (normalised.Q == normalised.T)
				return normalised.Q ^ ibit;

			Q = normalised.Q;
			T = normalised.T;
			F = normalised.F;

		} else if (T & IBIT) {

			if (T == IBIT) {
				if (F == Q || F == 0) {
					// SELF
					// "Q?!0:Q" [1] -> "Q?!0:0" [0] -> Q
					return Q ^ ibit;
				} else {
					// OR
					// "Q?!0:F" [2]
				}
			} else if ((T & ~IBIT) == Q) {
				if (F == Q || F == 0) {
					// ZERO
					// "Q?!Q:Q" [4] -> "Q?!Q:0" [3] -> "0"
					return 0 ^ ibit;
				} else {
					// LESS-THAN
					// "Q?!Q:F" [5] -> "F?!Q:F" -> "F?!Q:0"
					Q = F;
					F = 0;
				}
			} else {
				if (F == Q || F == 0) {
					// GREATER-THAN
					// "Q?!T:Q" [7] -> "Q?!T:0" [6]
					F = 0;
				} else if ((T & ~IBIT) == F) {
					// NOT-EQUAL
					// "Q?!F:F" [8]
				} else {
					// QnTF (new unified operator)
					// "Q?!T:F" [9]
				}
			}

		} else {

			if (T == 0) {
				if (F == Q || F == 0) {
					// ZERO
					// "Q?0:Q" [11] -> "Q?0:0" [10] -> "0"
					return 0 ^ ibit;
				} else {
					// LESS-THAN
					// "Q?0:F" [12] -> "F?!Q:0" [6]
					T = Q ^ IBIT;
					Q = F;
					F = 0;
				}

			} else if (T == Q) {
				if (F == Q || F == 0) {
					// SELF
					// "Q?Q:Q" [14] -> Q?Q:0" [13] -> "Q"
					return Q ^ ibit;
				} else {
					// OR
					// "Q?Q:F" [15] -> "Q?!0:F" [2]
					T = 0 ^ IBIT;
				}
			} else {
				if (F == Q || F == 0) {
					// AND
					// "Q?T:Q" [17] -> "Q?T:0" [16]
					F = 0;
				} else if (T == F) {
					// SELF
					// "Q?F:F" [18] -> "F"
					return F ^ ibit;
				} else {
					// QTF (old unified operator)
					// "Q?T:F" [19]
				}
			}
		}

		/*
		 * Rewrite QTF into QTnF
		 */
		if (this->flags & ctx.MAGICMASK_PURE) {
			/*
			 * rewrite  "a ? b : c" into "a? !(a ? !b : c) : c"
			 * ./eval "abc?" "aabc!c!"
			 */
			if (!(T & IBIT)) {
				// Q?T:F -> Q?!(Q?!T:F):F)
				T = normaliseNode(Q, T ^ IBIT, F) ^ IBIT;
			}
#if 0
			/*
			 * The following is experimental,
			 * It rewrites NE so that T and F are different
			 * But it might cause headaches and waits until the project is more mature to investigate
			 *
			 * a ? !b : b -> a?!b:(a?!0:b)
			 * ./eval "ab^" "abba>!" "aba0b!!"
			 */
			if ((T & ~IBIT) == F) {
				// NE
				// Q?!F:F -> Q?!F:(Q?!0:F)
				F = normaliseNode(Q, IBIT, F); abab>!
			}
#endif
		}


		/*
		 * Level 3 normalisation: cascade OR/NE/AND
		 */

		static int xcnt;
		xcnt++;

		// OR
		if (T == IBIT) {
			// test for slow path
			if (this->flags & ctx.MAGICMASK_CASCADE) {
				if (isOR(Q)) {
					if (isOR(F))
						return mergeOR(Q, F) ^ ibit; // cascades may never fork

					// test if F is properly ordered
					if (!isOR(N[Q].F)) {
						if (compare(this, F, this, N[Q].F) <= 0)
							return mergeOR(Q, F) ^ ibit;
					} else if (!isOR(N[Q].Q)) {
						if (compare(this, F, this, N[Q].Q) <= 0)
							return mergeOR(Q, F) ^ ibit;
					}

				} else if (isOR(F)) {

					// test if Q is properly ordered
					if (!isOR(N[F].F)) {
						if (compare(this, Q, this, N[F].F) <= 0)
							return mergeOR(Q, F) ^ ibit;
					} else if (!isOR(N[F].Q)) {
						if (compare(this, Q, this, N[F].Q) <= 0)
							return mergeOR(Q, F) ^ ibit;
					}
				}
			}

			// otherwise fast path
			if (compare(this, Q, this, F) > 0) {
				// swap
				uint32_t savQ = Q;
				Q = F;
				F = savQ;
			}
		}

		// NE
		if ((T & ~IBIT) == F) {
			// test for slow path
			if (this->flags & ctx.MAGICMASK_CASCADE) {
				if (isNE(Q)) {
					if (isNE(F))
						return mergeNE(Q, F) ^ ibit; // cascades may never fork

					// test if F is properly ordered
					if (!isNE(N[Q].F)) {
						if (compare(this, F, this, N[Q].F) <= 0)
							return mergeNE(Q, F) ^ ibit;
					} else if (!isNE(N[Q].Q)) {
						if (compare(this, F, this, N[Q].Q) <= 0)
							return mergeNE(Q, F) ^ ibit;
					}

				} else if (isNE(F)) {

					// test if Q is properly ordered
					if (!isNE(N[F].F)) {
						if (compare(this, Q, this, N[F].F) <= 0)
							return mergeNE(Q, F) ^ ibit;
					} else if (!isNE(N[F].Q)) {
						if (compare(this, Q, this, N[F].Q) <= 0)
							return mergeNE(Q, F) ^ ibit;
					}
				}
			}

			// otherwise fast path
			if (compare(this, Q, this, F) > 0) {
				// swap
				uint32_t savQ = Q;
				Q = F;
				F = savQ;
				T = savQ ^ IBIT;
			}
		}

		// AND
		if ((~T & IBIT) && F == 0) {
			// test for slow path
			if (this->flags & ctx.MAGICMASK_CASCADE) {
				if (isAND(Q)) {
					if (isAND(T))
						return mergeAND(Q, T) ^ ibit; // cascades may never fork

					// test if T is properly ordered
					if (!isAND(N[Q].T)) {
						if (compare(this, T, this, N[Q].T) <= 0)
							return mergeAND(Q, T) ^ ibit;
					} else if (!isAND(N[Q].Q)) {
						if (compare(this, T, this, N[Q].Q) <= 0)
							return mergeAND(Q, T) ^ ibit;
					}

				} else if (isAND(T)) {

					// test if Q is properly ordered
					if (!isAND(N[T].T)) {
						if (compare(this, Q, this, N[T].T) <= 0)
							return mergeAND(Q, T) ^ ibit;
					} else if (!isAND(N[T].Q)) {
						if (compare(this, Q, this, N[T].Q) <= 0)
							return mergeAND(Q, T) ^ ibit;
					}
				}
			}

			// otherwise fast path
			if (compare(this, Q, this, T) > 0) {
				// swap
				uint32_t savQ = Q;
				Q = T;
				T = savQ;
			}
		}

		return this->basicNode(Q, T, F) ^ ibit;
	}

	/*
	 * @date 2021-05-13 00:25:01
	 *
	 * Merge two OR chains by sort/merging lhs+rhs
	 * Forks in chains are not allowed.
	 * Duplicate nodes are merged into one (a OR a = a)
	 */
	uint32_t mergeOR(uint32_t lhs, uint32_t rhs) {
		uint32_t *pStackL  = allocMap();
		uint32_t *pStackR  = allocMap();
		uint32_t numStackR = 0;
		uint32_t numStackL = 0;

		if (!isOR(rhs)) {
			pStackR[numStackR++] = rhs;
		} else {
			for (;;) {
				if (!isOR(N[rhs].F))
					pStackR[numStackR++] = N[rhs].F;
				if (!isOR(N[rhs].Q))
					pStackR[numStackR++] = N[rhs].Q;

				if (isOR(N[rhs].Q))
					rhs = N[rhs].Q;
				else if (isOR(N[rhs].F))
					rhs = N[rhs].F;
				else
					break;
			}
		}

		if (!isOR(lhs)) {
			pStackL[numStackL++] = lhs;
		} else {
			for (;;) {
				if (!isOR(N[lhs].F))
					pStackL[numStackL++] = N[lhs].F;
				if (!isOR(N[lhs].Q))
					pStackL[numStackL++] = N[lhs].Q;

				if (isOR(N[lhs].Q))
					lhs = N[lhs].Q;
				else if (isOR(N[lhs].F))
					lhs = N[lhs].F;
				else
					break;
			}
		}


		uint32_t Z = 0;

		while (numStackL && numStackR) {

			// two OR's merge to one
			if (numStackL >= 2 && pStackL[numStackL - 1] == pStackL[numStackL - 2]) {
				numStackL--;
			} else if (numStackR >= 2 && pStackR[numStackR - 1] == pStackR[numStackR - 2]) {
				numStackR--;
			} else if (pStackL[numStackL - 1] == pStackR[numStackR - 1]) {
				numStackL--;
			} else if (compare(this, pStackL[numStackL - 1], this, pStackR[numStackR - 1]) < 0) {

				uint32_t C = pStackL[--numStackL];

				assert(!isOR(C));
				Z = normaliseNode(C, IBIT, Z);

			} else {

				uint32_t C = pStackR[--numStackR];

				assert(!isOR(C));
				Z = normaliseNode(C, IBIT, Z);
			}
		}

		while (numStackL) {
			uint32_t C = pStackL[--numStackL];

			assert(!isOR(C));
			Z = normaliseNode(C, IBIT, Z);
		}
		while (numStackR) {
			uint32_t C = pStackR[--numStackR];

			assert(!isOR(C));
			Z = normaliseNode(C, IBIT, Z);
		}

		freeMap(pStackR);
		freeMap(pStackL);
		return Z;
	}

	/*
	 * @date 2021-05-13 00:27:36
	 *
	 * Merge two NE chains by sort/merging lhs+rhs
	 * Forks in chains are not allowed.
	 * Duplicate nodes are removed (a NE a = 0)
	 */
	uint32_t mergeNE(uint32_t lhs, uint32_t rhs) {

		/*
		 * setup starting position
		 * NOTE: "current positions" are candidate for what gets pushed and is never NE
		 * NOTE: nodes never have both Q/F being NE
		 * NOTE: "compare(N[x].Q, N[x].F)" should always return <0
		 */

		uint32_t *pStackL  = allocMap();
		uint32_t *pStackR  = allocMap();
		uint32_t numStackR = 0;
		uint32_t numStackL = 0;

		if (!isNE(rhs)) {
			pStackR[numStackR++] = rhs;
		} else {
			for (;;) {
				if (!isNE(N[rhs].F))
					pStackR[numStackR++] = N[rhs].F;
				if (!isNE(N[rhs].Q))
					pStackR[numStackR++] = N[rhs].Q;

				if (isNE(N[rhs].Q))
					rhs = N[rhs].Q;
				else if (isNE(N[rhs].F))
					rhs = N[rhs].F;
				else
					break;
			}
		}

		if (!isNE(lhs)) {
			pStackL[numStackL++] = lhs;
		} else {
			for (;;) {
				if (!isNE(N[lhs].F))
					pStackL[numStackL++] = N[lhs].F;
				if (!isNE(N[lhs].Q))
					pStackL[numStackL++] = N[lhs].Q;

				if (isNE(N[lhs].Q))
					lhs = N[lhs].Q;
				else if (isNE(N[lhs].F))
					lhs = N[lhs].F;
				else
					break;
			}
		}


		uint32_t Z = 0;

		while (numStackL && numStackR) {

			// two NE's collapse
			if (numStackL >= 2 && pStackL[numStackL - 1] == pStackL[numStackL - 2]) {
				numStackL -= 2;
			} else if (numStackR >= 2 && pStackR[numStackR - 1] == pStackR[numStackR - 2]) {
				numStackR -= 2;
			} else if (pStackL[numStackL - 1] == pStackR[numStackR - 1]) {
				numStackL--;
				numStackR--;
			} else if (compare(this, pStackL[numStackL - 1], this, pStackR[numStackR - 1]) < 0) {

				uint32_t C = pStackL[--numStackL];

				assert(!isNE(C));
				Z = normaliseNode(C, Z ^ IBIT, Z);

			} else {

				uint32_t C = pStackR[--numStackR];

				assert(!isNE(C));
				Z = normaliseNode(C, Z ^ IBIT, Z);
			}
		}

		while (numStackL) {
			uint32_t C = pStackL[--numStackL];

			assert(!isNE(C));
			Z = normaliseNode(C, Z ^ IBIT, Z);
		}
		while (numStackR) {
			uint32_t C = pStackR[--numStackR];

			assert(!isNE(C));
			Z = normaliseNode(C, Z ^ IBIT, Z);
		}

		freeMap(pStackR);
		freeMap(pStackL);
		return Z;
	}

	/*
	 * @date 2021-05-13 00:29:01
	 *
	 * Merge two AND chains by sort/merging lhs+rhs
	 * Forks in chains are not allowed.
	 * Duplicate nodes are merged into one (a AND a = a)
	 */
	uint32_t mergeAND(uint32_t lhs, uint32_t rhs) {
		uint32_t *pStackL  = allocMap();
		uint32_t *pStackR  = allocMap();
		uint32_t numStackR = 0;
		uint32_t numStackL = 0;

		if (!isAND(rhs)) {
			pStackR[numStackR++] = rhs;
		} else {
			for (;;) {
				if (!isAND(N[rhs].T))
					pStackR[numStackR++] = N[rhs].T;
				if (!isAND(N[rhs].Q))
					pStackR[numStackR++] = N[rhs].Q;

				if (isAND(N[rhs].Q))
					rhs = N[rhs].Q;
				else if (isAND(N[rhs].T))
					rhs = N[rhs].T;
				else
					break;
			}
		}

		if (!isAND(lhs)) {
			pStackL[numStackL++] = lhs;
		} else {
			for (;;) {
				if (!isAND(N[lhs].T))
					pStackL[numStackL++] = N[lhs].T;
				if (!isAND(N[lhs].Q))
					pStackL[numStackL++] = N[lhs].Q;

				if (isAND(N[lhs].Q))
					lhs = N[lhs].Q;
				else if (isAND(N[lhs].T))
					lhs = N[lhs].T;
				else
					break;
			}
		}


		uint32_t Z = 0;

		while (numStackL && numStackR) {

			// two AND's merge to one
			if (numStackL >= 2 && pStackL[numStackL - 1] == pStackL[numStackL - 2]) {
				numStackL--;
			} else if (numStackR >= 2 && pStackR[numStackR - 1] == pStackR[numStackR - 2]) {
				numStackR--;
			} else if (pStackL[numStackL - 1] == pStackR[numStackR - 1]) {
				numStackL--;
			} else if (compare(this, pStackL[numStackL - 1], this, pStackR[numStackR - 1]) < 0) {

				uint32_t C = pStackL[--numStackL];

				assert(!isAND(C));
				if (Z == 0) Z = C;
				else
					Z = normaliseNode(C, Z, 0);

			} else {

				uint32_t C = pStackR[--numStackR];

				assert(!isAND(C));
				if (Z == 0) Z = C;
				else
					Z = normaliseNode(C, Z, 0);
			}
		}

		while (numStackL) {
			uint32_t C = pStackL[--numStackL];

			assert(!isAND(C));
			if (Z == 0) Z = C;
			else
				Z = normaliseNode(C, Z, 0);
		}
		while (numStackR) {
			uint32_t C = pStackR[--numStackR];

			if (Z == 0) Z = C;
			else
				Z = normaliseNode(C, Z, 0);
		}

		freeMap(pStackR);
		freeMap(pStackL);
		return Z;
	}

	/*
	 * @date 2021-05-22 19:10:33
	 *
	 * Encode a prefix into given parameter `pNode`.
	 *
	 * NOTE: `std::string` usage exception because this is NOT speed critical code AND strings can become gigantically large
	 */
	void encodePrefix(std::string &name, unsigned value) const {

		// NOTE: 0x7fffffff = `GYTISXx`

		// creating is right-to-left. Storage to reverse
		char stack[10], *pStack = stack;

		// push terminator
		*pStack++ = 0;

		// process the value
		do {
			*pStack++ = 'A' + (value % 26);
			value /= 26;
		} while (value);

		// append, including trailing zero
		while (*--pStack) {
			name += *pStack;
		}
	}

	/*
	 * @date 2021-05-22 19:18:33
	 *
	 * Export a sub-tree with a given head id as siting.
	 * Optionally endpoint normalised with a separate transform.
	 * Return string is static allocated
	 *
	 * NOTE: `std::string` usage exception because this is NOT speed critical code AND strings can become gigantically large
	 */
	std::string saveString(uint32_t id, std::string *pTransform = NULL) {

		std::string name;
		uint32_t    nextKey  = this->kstart;
		uint32_t    nextNode = this->nstart;

		/*
		 * Endpoints are simple
		 */
		if ((id & ~IBIT) < this->nstart) {
			if (pTransform) {
				pTransform->clear();
				if ((id & ~IBIT) == 0) {
					name += '0';
				} else {
					uint32_t value = (id & ~IBIT) - this->kstart;

					if (value < 26) {
						*pTransform += (char) ('a' + value);
					} else {
						encodePrefix(*pTransform, value / 26);
						*pTransform += 'a' + (value % 26);
					}

					name += 'a';
				}

			} else {
				if ((id & ~IBIT) == 0) {
					name += '0';
				} else {
					uint32_t value = (id & ~IBIT) - this->kstart;
					if (value < 26) {
						name += (char) ('a' + value);
					} else {
						encodePrefix(name, value / 26);
						name += 'a' + (value % 26);
					}
				}
			}


			// test for invert
			if (id & IBIT)
				name += '~';

			return name;
		}

		uint32_t *pStack     = allocMap();
		uint32_t *pMap       = allocMap();
		uint32_t *pVersion   = allocVersion();
		uint32_t thisVersion = ++mapVersionNr;
		uint32_t numStack    = 0; // top of stack

		// clear version map when wraparound
		if (thisVersion == 0) {
			::memset(pVersion, 0, maxNodes * sizeof *pVersion);
			thisVersion = ++mapVersionNr;
		}

		/*
		 * For transforms, walk the tree depth-first to collect the transform map
		 */
		if (pTransform) {
			numStack = 0;
			pStack[numStack++] = id & ~IBIT;

			do {
				// pop stack
				uint32_t curr = pStack[--numStack];

				if (curr < this->nstart) {
					// ignore
					continue;
				}

				const baseNode_t *pNode = this->N + curr;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				// determine if already handled
				if (pVersion[curr] != thisVersion) {
					/*
					 * First time visit
					 */
					pVersion[curr] = thisVersion;
					pMap[curr]     = 0;

					// push id so it visits again a second time
					pStack[numStack++] = curr;

					if (Ti) {
						if (Tu == 0) {
							// Q?!0:F
							pStack[numStack++] = F;
							pStack[numStack++] = Q;
						} else if (F == 0) {
							// Q?!T:0
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						} else if (F == Tu) {
							// Q?!F:F
							pStack[numStack++] = F;
							pStack[numStack++] = Q;

						} else {
							// Q?!T:F
							pStack[numStack++] = F;
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						}
					} else {
						if (F == 0) {
							// Q?T:0
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						} else {
							// Q?T:F
							pStack[numStack++] = F;
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						}
					}
					assert(numStack < maxNodes);

				} else if (pMap[curr] == 0) {
					/*
					 * Second time visit
					 */
					pMap[curr] = nextNode++;

					// node complete, assign slots
					if (Q && Q < this->nstart && pVersion[Q] != thisVersion) {
						pVersion[Q] = thisVersion;
						pMap[Q]     = nextKey++;

						uint32_t value = Q - this->kstart;
						if (value < 26) {
							*pTransform += (char) ('a' + value);
						} else {
							encodePrefix(*pTransform, value / 26);
							*pTransform += 'a' + (value % 26);
						}
					}

					if (Tu && Tu < this->nstart && pVersion[Tu] != thisVersion) {
						// not for NE
						if (!Ti || Tu != F) {
							pVersion[Tu] = thisVersion;
							pMap[Tu]     = nextKey++;

							uint32_t value = Tu - this->kstart;
							if (value < 26) {
								*pTransform += (char) ('a' + value);
							} else {
								encodePrefix(*pTransform, value / 26);
								*pTransform += 'a' + (value % 26);
							}
						}
					}

					if (F && F < this->nstart && pVersion[F] != thisVersion) {
						pVersion[F] = thisVersion;
						pMap[F]     = nextKey++;

						uint32_t value = F - this->kstart;
						if (value < 26) {
							*pTransform += (char) ('a' + value);
						} else {
							encodePrefix(*pTransform, value / 26);
							*pTransform += 'a' + (value % 26);
						}
					}

					assert(numStack < maxNodes);

				}

			} while (numStack > 0);

			// bump version, need to walk tree again
			thisVersion = ++mapVersionNr;

			// clear version map when wraparound
			if (thisVersion == 0) {
				::memset(pVersion, 0, maxNodes * sizeof *pVersion);
				thisVersion = ++mapVersionNr;
			}
		}

		numStack = 0;
		pStack[numStack++] = id & ~IBIT;

		/*
		 * Walk the tree depth-first
		 */
		do {
			// pop stack
			uint32_t curr = pStack[--numStack];

			if (curr < this->nstart) {

				if (curr == 0) {
					name += '0';
				} else {
					uint32_t value;

					if (!pTransform)
						value = curr - this->kstart;
					else
						value = pMap[curr] - this->kstart;

					// convert id to (prefixed) letter
					if (value < 26) {
						name += 'a' + value;
					} else {
						encodePrefix(name, value / 26);
						name += 'a' + (value % 26);
					}
				}

				continue;
			}

			const baseNode_t *pNode = this->N + curr;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			// determine if already handled
			if (pVersion[curr] != thisVersion) {
				/*
				 * First time visit
				 */
				pVersion[curr] = thisVersion;
				pMap[curr]     = 0;

				// push id so it visits again after expanding
				pStack[numStack++] = curr;

				if (Ti) {
					if (Tu == 0) {
						// Q?!0:F
						pStack[numStack++] = F;
						pStack[numStack++] = Q;
					} else if (F == 0) {
						// Q?!T:0
						pStack[numStack++] = Tu;
						pStack[numStack++] = Q;
					} else if (F == Tu) {
						// Q?!F:F
						pStack[numStack++] = F;
						pStack[numStack++] = Q;

					} else {
						// Q?!T:F
						pStack[numStack++] = F;
						pStack[numStack++] = Tu;
						pStack[numStack++] = Q;
					}
				} else {
					if (F == 0) {
						// Q?T:0
						pStack[numStack++] = Tu;
						pStack[numStack++] = Q;
					} else {
						// Q?T:F
						pStack[numStack++] = F;
						pStack[numStack++] = Tu;
						pStack[numStack++] = Q;
					}
				}
				assert(numStack < maxNodes);

			} else if (pMap[curr] == 0) {
				/*
				 * Second time visit
				 */
				pMap[curr] = nextNode++;

				if (Ti) {
					if (Tu == 0) {
						// Q?!0:F
						name += '+';
					} else if (F == 0) {
						// Q?!T:0
						name += '>';
					} else if (F == Tu) {
						// Q?!F:F
						name += '^';
					} else {
						// Q?!T:F
						name += '#';
					}
				} else {
					if (F == 0) {
						// Q?T:0
						name += '&';
					} else {
						// Q?T:F
						name += '?';
					}
				}
				assert(numStack < maxNodes);

			} else {

				uint32_t dist = nextNode - pMap[curr];

				// convert id to (prefixed) back-link
				if (dist < 10) {
					name += '0' + dist;
				} else {
					encodePrefix(name, dist / 10);
					name += '0' + (dist % 10);
				}
			}

		} while (numStack > 0);

		// test for invert
		if (id & IBIT)
			name += '~';

		freeMap(pMap);
		freeMap(pStack);
		freeVersion(pVersion);

		return name;
	}

	/*
	 * @date 2021-05-22 21:25:45
	 *
	 * Find the highest endpoint in a pattern, excluding any transform (relative)
	 *
	 * return highest id, or -1 if name was "0"
	 *
	 * NOTE: first endpoint `a` will return 0.
	 * NOTE: static to allow calling without loaded tree
	 */
	static int highestEndpoint(context_t &ctx, const char *pPattern) {
		int highest = -1;

		while (*pPattern) {

			switch (*pPattern) {
			case '0': //
				pPattern++;
				break;

				// @formatter:off
			case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8':
			case '9':
			// @formatter:on
				// back-link
				pPattern++;
				break;

				// @formatter:off
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			// @formatter:on
			{
				// endpoint
				int v = (*pPattern - 'a');
				if (v > highest)
					highest = v;

				pPattern++;
				break;

			}

				// @formatter:off
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
			// @formatter:on
			{
				// prefix
				int v = 0;
				while (isupper(*pPattern))
					v = v * 26 + *pPattern++ - 'A';

				if (isdigit(*pPattern)) {
					// follow by back-link
				} else if (islower(*pPattern)) {
					// followed by endpoint
					v = (v * 26 + *pPattern - 'a');

					if (v > highest)
						highest = v;
				} else {
					ctx.fatal("[bad token '%c' in pattern]\n", *pPattern);
				}

				pPattern++;
				break;
			}

			case '+':
			case '>':
			case '#':
			case '^':
			case '&':
			case '?':
			case '!':
			case '~':
				pPattern++;
				break;
			case '/': {
				// scan transform
				pPattern++;

				while (*pPattern) {
					if (*pPattern != ' ') {
						// prefix
						int v = 0;
						while (isupper(*pPattern))
							v = v * 26 + *pPattern++ - 'A';

						if (!islower(*pPattern))
							ctx.fatal("[bad token '%c' in transform]\n", *pPattern);

						// followed by endpoint
						v = (v * 26 + *pPattern - 'a');

						if (v > highest)
							highest = v;
					}
					pPattern++;
				}

				return highest;
			}
			case ' ':
				// skip spaces
				pPattern++;
				break;
			default:
				ctx.fatal("[bad token '%c' in pattern]\n", *pPattern);
			}
		}

		return highest;
	}

	/*
	 * @date 2021-05-22 23:17:43
	 *
	 * Unpack transform string into an array
	 *
	 * NOTE: static to allow calling before loading trees
	 */
	static uint32_t *decodeTransform(context_t &ctx, uint32_t kstart, uint32_t nstart, const char *pTransform) {
		uint32_t *transformList = (uint32_t *) ctx.myAlloc("baseTree_t::transformList", nstart, sizeof *transformList);

		// invalidate list, except for `0`
		transformList[0] = 0;

		// invalidate all entries
		for (uint32_t i = kstart; i < nstart; i++)
			transformList[i] = 1 /* KERROR */;

		// start decoding
		for (uint32_t t = kstart; t < nstart; t++) {
			if (!*pTransform) {
				// transform string is shorter than available keys
				// assume uninitialised entries are unused
				break;
			}

			if (islower(*pTransform)) {
				// endpoint
				transformList[t] = *pTransform++ - 'a' + kstart;

			} else if (isupper(*pTransform)) {
				// prefix
				unsigned value = 0;

				while (isupper(*pTransform))
					value = value * 26 + *pTransform++ - 'A';

				if (!islower(*pTransform))
					ctx.fatal("[transform string non alphabetic]\n");

				transformList[t] = (value + 1) * 26 + *pTransform++ - 'a' + kstart;

			} else {
				ctx.fatal("[bad token '%c' in transform]\n", *pTransform);
			}
		}

		if (*pTransform)
			ctx.fatal("[transform string too long]\n");

		return transformList;
	}

	/*
	 * @date 2021-05-22 21:22:41
	 *
	 * NOTE: !!! Apply any changes here also to `loadBasicString()`
	 *
	 * Import a string into tree.
	 * NOTE: Will use `normaliseNode()`.
	 */
	uint32_t loadNormaliseString(const char *pPattern, const char *pTransform = NULL) {

		// modify if transform is present
		uint32_t *transformList = NULL;
		if (pTransform && *pTransform)
			transformList = decodeTransform(ctx, kstart, nstart, pTransform);

		/*
		 * init
		 */

		uint32_t stackpos = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap();
		uint32_t *pMap    = allocMap();
		uint32_t nid;

		/*
		 * Load string
		 */
		for (const char *pattern = pPattern; *pattern; pattern++) {

			switch (*pattern) {
			case '0': //
				pStack[stackpos++] = 0;
				break;

				// @formatter:off
			case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8':
			case '9':
			// @formatter:on
			{
				/*
				 * Push back-reference
				 */
				uint32_t v = nextNode - (*pattern - '0');

				if (v < this->nstart || v >= nextNode)
					ctx.fatal("[node out of range: %d]\n", v);
				if (stackpos >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[stackpos++] = pMap[v];

				break;
			}

				// @formatter:off
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			// @formatter:on
			{
				/*
				 * Push endpoint
				 */
				uint32_t v = this->kstart + (*pattern - 'a');

				if (v < this->kstart || v >= this->nstart)
					ctx.fatal("[endpoint out of range: %d]\n", v);
				if (stackpos >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				if (transformList)
					pStack[stackpos++] = transformList[v];
				else
					pStack[stackpos++] = v;
				break;

			}

				// @formatter:off
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
			// @formatter:on
			{
				/*
				 * Prefix
				 */
				uint32_t v = 0;
				while (isupper(*pattern))
					v = v * 26 + *pattern++ - 'A';

				if (isdigit(*pattern)) {
					/*
					 * prefixed back-reference
					 */
					v = nextNode - (v * 10 + *pattern - '0');

					if (v < this->nstart || v >= nextNode)
						ctx.fatal("[node out of range: %d]\n", v);
					if (stackpos >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					pStack[stackpos++] = pMap[v];
				} else if (islower(*pattern)) {
					/*
					 * prefixed endpoint
					 */
					v = this->kstart + (v * 26 + *pattern - 'a');

					if (v < this->kstart || v >= this->nstart)
						ctx.fatal("[endpoint out of range: %d]\n", v);
					if (stackpos >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					if (transformList)
						pStack[stackpos++] = transformList[v];
					else
						pStack[stackpos++] = v;
				} else {
					ctx.fatal("[bad token '%c']\n", *pattern);
				}
				break;
			}

			case '+': {
				// OR
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				if (compare(this, Q, this, F) < 0)
					nid = normaliseNode(Q, IBIT, F);
				else
					nid = normaliseNode(F, IBIT, Q);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '>': {
				// GT
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = normaliseNode(Q, T ^ IBIT, 0);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '#': {
				// QnTF
				if (stackpos < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = normaliseNode(Q, T ^ IBIT, F);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '^': {
				// NE
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				if (compare(this, Q, this, F) < 0)
					nid = normaliseNode(Q, F ^ IBIT, F);
				else
					nid = normaliseNode(F, Q ^ IBIT, Q);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				if (compare(this, Q, this, T) < 0)
					nid = normaliseNode(Q, T, 0);
				else
					nid = normaliseNode(T, Q, 0);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF
				if (stackpos < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = normaliseNode(Q, T, F);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '!': {
				// QTnF
				if (stackpos < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = normaliseNode(Q, T, F ^ IBIT);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT
				if (stackpos < 1)
					ctx.fatal("[stack underflow]\n");

				pStack[stackpos - 1] ^= IBIT;
				break;
			}
			case '/':
				// separator between pattern/transform
				while (pattern[1])
					pattern++;
				break;
			case ' ':
				// skip spaces
				break;
			default:
				ctx.fatal("[bad token '%c']\n", *pattern);
			}

			if (stackpos > maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (stackpos != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[stackpos - 1];

		freeMap(pStack);
		freeMap(pMap);
		if (transformList)
			freeMap(transformList);

		return ret;
	}

	/*
	 * @date 2021-05-26 21:01:25
	 *
	 * NOTE: !!! Apply any changes here also to `loadNormaliseString()`
	 *
	 * Import a string into tree.
	 * NOTE: Will use `basicNode()`.
	 */
	uint32_t loadBasicString(const char *pPattern, const char *pTransform = NULL) {

		// modify if transform is present
		uint32_t *transformList = NULL;
		if (pTransform)
			transformList = decodeTransform(ctx, kstart, nstart, pTransform);

		/*
		 * init
		 */

		uint32_t stackpos = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap();
		uint32_t *pMap    = allocMap();
		uint32_t nid;

		/*
		 * Load string
		 */
		for (const char *pattern = pPattern; *pattern; pattern++) {

			switch (*pattern) {
			case '0': //
				pStack[stackpos++] = 0;
				break;

				// @formatter:off
			case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8':
			case '9':
			// @formatter:on
			{
				/*
				 * Push back-reference
				 */
				uint32_t v = nextNode - (*pattern - '0');

				if (v < this->nstart || v >= nextNode)
					ctx.fatal("[node out of range: %d]\n", v);
				if (stackpos >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[stackpos++] = pMap[v];

				break;
			}

				// @formatter:off
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
			// @formatter:on
			{
				/*
				 * Push endpoint
				 */
				uint32_t v = this->kstart + (*pattern - 'a');

				if (v < this->kstart || v >= this->nstart)
					ctx.fatal("[endpoint out of range: %d]\n", v);
				if (stackpos >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				if (transformList)
					pStack[stackpos++] = transformList[v];
				else
					pStack[stackpos++] = v;
				break;

			}

				// @formatter:off
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
			// @formatter:on
			{
				/*
				 * Prefix
				 */
				uint32_t v = 0;
				while (isupper(*pattern))
					v = v * 26 + *pattern++ - 'A';

				if (isdigit(*pattern)) {
					/*
					 * prefixed back-reference
					 */
					v = nextNode - (v * 10 + *pattern - '0');

					if (v < this->nstart || v >= nextNode)
						ctx.fatal("[node out of range: %d]\n", v);
					if (stackpos >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					pStack[stackpos++] = pMap[v];
				} else if (islower(*pattern)) {
					/*
					 * prefixed endpoint
					 */
					v = this->kstart + ((v + 1) * 26 + *pattern - 'a');

					if (v < this->kstart || v >= this->nstart)
						ctx.fatal("[endpoint out of range: %d]\n", v);
					if (stackpos >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					if (transformList)
						pStack[stackpos++] = transformList[v];
					else
						pStack[stackpos++] = v;
				} else {
					ctx.fatal("[bad token '%c']\n", *pattern);
				}
				break;
			}

			case '+': {
				// OR
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = basicNode(Q, IBIT, F);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '>': {
				// GT
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = basicNode(Q, T ^ IBIT, 0);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '#': {
				// QnTF
				if (stackpos < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = basicNode(Q, T ^ IBIT, F);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '^': {
				// NE
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = basicNode(Q, F ^ IBIT, F);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND
				if (stackpos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = basicNode(Q, T, 0);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF
				if (stackpos < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = basicNode(Q, T, F);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '!': {
				// QTnF
				if (stackpos < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--stackpos];
				uint32_t T = pStack[--stackpos];
				uint32_t Q = pStack[--stackpos];

				nid = basicNode(Q, T, F ^ IBIT);

				pStack[stackpos++] = pMap[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT
				if (stackpos < 1)
					ctx.fatal("[stack underflow]\n");

				pStack[stackpos - 1] ^= IBIT;
				break;
			}
			case '/':
				// separator between pattern/transform
				while (pattern[1])
					pattern++;
				break;
			case ' ':
				// skip spaces
				break;
			default:
				ctx.fatal("[bad token '%c']\n", *pattern);
			}

			if (stackpos > maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (stackpos != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[stackpos - 1];

		freeMap(pStack);
		freeMap(pMap);
		if (transformList)
			freeMap(transformList);

		return ret;
	}

	/*
	 * count the number of active nodes in the tree.
	 * Used to determine the best candidate for folding.
	 */
	unsigned countActive(void) {
		uint32_t *pSelect    = this->allocMap();
		uint32_t thisVersion = ++this->mapVersionNr;

		if (thisVersion == 0) {
			::memset(pSelect, 0, this->maxNodes * sizeof *pSelect);
			thisVersion = ++this->mapVersionNr;
		}

		unsigned numCount = 0;

		// select the heads
		// add artificial root for system
		for (unsigned iRoot = this->kstart; iRoot <= this->numRoots; iRoot++) {
			uint32_t R = (iRoot < this->numRoots) ?  this->roots[iRoot] :  this->system;
			uint32_t Ru = R & ~IBIT;

			if (Ru >= this->nstart && pSelect[Ru] != thisVersion)
				numCount++;
			pSelect[Ru] = thisVersion;
		}

		for (uint32_t iNode = this->ncount - 1; iNode >= this->nstart; --iNode) {
			if (pSelect[iNode] != thisVersion)
				continue;

			const baseNode_t *pNode = this->N + iNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
//			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			if (Q >= this->nstart && pSelect[Q] != thisVersion)
				numCount++;
			pSelect[Q] = thisVersion;

			if (Tu >= this->nstart && pSelect[Tu] != thisVersion)
				numCount++;
			pSelect[Tu] = thisVersion;

			if (F >= this->nstart && pSelect[F] != thisVersion)
				numCount++;
			pSelect[F] = thisVersion;
		}

		this->freeMap(pSelect);
		return numCount;
	}

	/*
	 * Import the active area of another tree
	 * Both trees have/need synced metrics.
	 * Same tree-walk logic as with `saveFile()`.
	 * Used when promoting candidate,
	 * to cleanup and serialize the tree walking order.
	 */
	void importActive(baseTree_t *RHS) {
		/*
		 * Select  active nodes
		 */

		uint32_t *pMap       = RHS->allocMap();
		uint32_t *pStack     = RHS->allocMap();
		uint32_t *pVersion   = RHS->allocVersion();
		uint32_t thisVersion = ++RHS->mapVersionNr;
		uint32_t numStack    = 0; // top of stack

		// clear version map when wraparound
		if (thisVersion == 0) {
			::memset(pVersion, 0, RHS->maxNodes * sizeof *pVersion);
			thisVersion = ++RHS->mapVersionNr;
		}

		for (uint32_t iKey = 0; iKey < this->nstart; iKey++)
			pMap[iKey] = iKey;

		/*
		 * @date 2021-06-05 18:24:37
		 *
		 * trace roots, one at a time.
		 * Last root is a artificial root representing "system"
		 */
		for (uint32_t iRoot = 0; iRoot <= this->numRoots; iRoot++) {

			uint32_t R = (iRoot < this->numRoots) ? RHS->roots[iRoot] : RHS->system;

			numStack = 0;
			pStack[numStack++] = R & ~IBIT;

			/*
			 * Walk the tree depth-first
			 */
			do {
				// pop stack
				uint32_t curr = pStack[--numStack];

				if (curr < this->nstart)
					continue; // endpoints have already been output

				const baseNode_t *pNode = RHS->N + curr;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				// determine if already handled
				if (pVersion[curr] != thisVersion) {
					/*
					 * First time visit
					 */
					pVersion[curr] = thisVersion;
					pMap[curr]     = 0;

					// push id so it visits again after expanding
					pStack[numStack++] = curr;

					if (Ti) {
						if (Tu == 0) {
							// Q?!0:F
							pStack[numStack++] = F;
							pStack[numStack++] = Q;
						} else if (F == 0) {
							// Q?!T:0
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						} else if (F == Tu) {
							// Q?!F:F
							pStack[numStack++] = F;
							pStack[numStack++] = Q;

						} else {
							// Q?!T:F
							pStack[numStack++] = F;
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						}
					} else {
						if (F == 0) {
							// Q?T:0
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						} else {
							// Q?T:F
							pStack[numStack++] = F;
							pStack[numStack++] = Tu;
							pStack[numStack++] = Q;
						}
					}
					assert(numStack < maxNodes);

				} else if (pMap[curr] == 0) {
					/*
					 * Second time visit
					 */
					pMap[curr] = this->normaliseNode(pMap[Q], pMap[Tu] ^ Ti, pMap[F]);
				}

			} while (numStack > 0);
		}

		RHS->freeVersion(pVersion);
		RHS->freeMap(pStack);

		/*
		 * write roots
		 * Last root is a virtual root representing "system"
		 */
		for (unsigned iRoot = 0; iRoot <= this->numRoots; iRoot++) {

			if (iRoot < this->numRoots)
				this->roots[iRoot] = pMap[RHS->roots[iRoot] & ~IBIT] ^ (RHS->roots[iRoot] & IBIT);
			else
				this->system = pMap[RHS->system & ~IBIT] ^ (RHS->system & IBIT);
		}

		RHS->freeMap(pMap);
	}

	/*
	 * import/fold
	 */
	void importFold(baseTree_t *RHS, uint32_t iFold) {

		uint32_t *pMapSet = RHS->allocMap();
		uint32_t *pMapClr = RHS->allocMap();

		/*
		 * Prepare tree
		 */
		this->rewind();

		// prepare maps
		for (unsigned iKey = 0; iKey < RHS->nstart; iKey++)
			pMapSet[iKey] = pMapClr[iKey] = iKey;

		// make fold constant
		pMapSet[iFold] = IBIT;
		pMapClr[iFold] = 0;

		/*
		 * Copy all nodes
		 */
		for (uint32_t iNode = RHS->nstart; iNode < RHS->ncount; iNode++) {
			const baseNode_t *pNode = RHS->N + iNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			pMapSet[iNode] = this->normaliseNode(pMapSet[Q], pMapSet[Tu] ^ Ti, pMapSet[F]);
			pMapClr[iNode] = this->normaliseNode(pMapClr[Q], pMapClr[Tu] ^ Ti, pMapClr[F]);
		}

		/*
		 * Set roots
		 */
		for (uint32_t iRoot = 0; iRoot < RHS->numRoots; iRoot++) {
			uint32_t Ru = RHS->roots[iRoot] & ~IBIT;
			uint32_t Ri = RHS->roots[iRoot] & IBIT;

			this->roots[iRoot] = this->normaliseNode(iFold, pMapSet[Ru], pMapClr[Ru]) ^ Ri;
		}

		if (RHS->system) {
			uint32_t Ru = RHS->system & ~IBIT;
			uint32_t Ri = RHS->system & IBIT;

			this->system = this->normaliseNode(iFold, pMapSet[Ru], pMapClr[Ru]) ^ Ri;
		}

		RHS->freeMap(pMapSet);
		RHS->freeMap(pMapClr);
	}

	/*
	 * @date 2021-05-14 21:18:32
	 *
	 * Load database from binary data file
	 *
	 * return 0 for ok.
	 */
	unsigned loadFile(const char *fileName, bool shared = true) {

		if (!keyNames.empty() || !rootNames.empty() || allocFlags || hndl >= 0)
			ctx.fatal("baseTree_t::loadFile() on non-initial tree\n");

		/*
		 * Open/attach/read file
		 */

		hndl = open(fileName, O_RDONLY);
		if (hndl == -1)
			ctx.fatal("fopen(\"%s\",\"r\") returned: %m\n", fileName);

		struct stat stbuf;
		if (fstat(hndl, &stbuf))
			ctx.fatal("fstat(\"%s\") returned: %m\n", fileName);

		if (shared) {
			/*
			 * Open and load using mmap()
			 */
			void *pMemory = mmap(NULL, (size_t) stbuf.st_size, PROT_READ, MAP_SHARED | MAP_NORESERVE, hndl, 0);
			if (pMemory == MAP_FAILED)
				ctx.fatal("mmap(PROT_READ, MAP_SHARED|MAP_NORESERVE,%s) returned: %m\n", fileName);
			/*
			 * Hint how memory will be used
			 */
			if (madvise(pMemory, (size_t) stbuf.st_size, MADV_RANDOM | MADV_DONTDUMP))
				ctx.fatal("madvise(MADV_RANDOM|MADV_DONTDUMP) returned: %m\n");

			rawDatabase = (uint8_t *) pMemory;
		} else {
			/*
			 * Read the contents
			 */
			rawDatabase = (uint8_t *) ctx.myAlloc("baseTreeFile_t::rawDatabase", 1, (size_t) stbuf.st_size);
			uint64_t progressHi = stbuf.st_size;
			uint64_t progress   = 0;

			// read in chunks of 1024*1024 bytes
			uint64_t dataLength = stbuf.st_size;
			uint8_t  *dataPtr   = rawDatabase;
			while (dataLength > 0) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					fprintf(stderr, "\r\e[K%.5f%%", progress * 100.0 / progressHi);
					ctx.tick = 0;
				}

				/*
				 * Determine bytes to read
				 */
				size_t sliceLength = dataLength;
				if (sliceLength > 1024 * 1024)
					sliceLength = 1024 * 1024;

				/*
				 * Write
				 */
				if ((uint64_t) read(hndl, dataPtr, sliceLength) != sliceLength)
					ctx.fatal("[Failed to read %lu bytes: %m]\n", sliceLength);

				/*
				 * Update
				 */
				dataPtr += sliceLength;
				dataLength -= sliceLength;
				progress += sliceLength;
			}

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
				fprintf(stderr, "\r\e[K"); // erase progress

			/*
			 * Close
			 */
			close(hndl);
			hndl = -1;
		}

		fileHeader = (baseTreeHeader_t *) rawDatabase;
		if (fileHeader->magic != BASETREE_MAGIC)
			ctx.fatal("baseTree version mismatch. Expected %08x, Encountered %08x\n", BASETREE_MAGIC, fileHeader->magic);
		if (fileHeader->offEnd != (uint64_t) stbuf.st_size)
			ctx.fatal("baseTree size mismatch. Expected %lu, Encountered %lu\n", fileHeader->offEnd, (uint64_t) stbuf.st_size);

		flags      = fileHeader->magic_flags;
		unused1    = fileHeader->unused1;
		system     = fileHeader->system;
		kstart     = fileHeader->kstart;
		ostart     = fileHeader->ostart;
		estart     = fileHeader->estart;
		nstart     = fileHeader->nstart;
		ncount     = fileHeader->ncount;
		numRoots   = fileHeader->numRoots;
		numHistory = fileHeader->numHistory;
		posHistory = fileHeader->posHistory;

		// @date 2021-05-14 21:46:35 Tree is read-only
		maxNodes = ncount; // used for map allocations

		// primary
		N             = (baseNode_t *) (rawDatabase + fileHeader->offNodes);
		roots         = (uint32_t *) (rawDatabase + fileHeader->offRoots);
		history       = (uint32_t *) (rawDatabase + fileHeader->offHistory);
		// pools
		pPoolMap      = (uint32_t **) ctx.myAlloc("baseTree_t::pPoolMap", MAXPOOLARRAY, sizeof(*pPoolMap));
		pPoolVersion  = (uint32_t **) ctx.myAlloc("baseTree_t::pPoolVersion", MAXPOOLARRAY, sizeof(*pPoolVersion));
		// structure based compare
		stackL        = allocMap();
		stackR        = allocMap();
		compNodeL     = allocMap();
		compNodeR     = allocMap();
		compVersionL  = allocMap(); // allocate as node-id map because of local version numbering
		compVersionR  = allocMap();  // allocate as node-id map because of local version numbering
		compVersionNr = 1;

		// make all `keyNames`+`rootNames` indices valid
		keyNames.resize(nstart);
		rootNames.resize(numRoots);

		// slice names
		{
			const char *pData = (const char *) (rawDatabase + fileHeader->offNames);

			for (uint32_t iKey  = 0; iKey < nstart; iKey++) {
				assert(*pData != 0);
				keyNames[iKey] = pData;
				pData += strlen(pData) + 1;
			}
			for (uint32_t iRoot = 0; iRoot < numRoots; iRoot++) {
				assert(*pData != 0);
				rootNames[iRoot] = pData;
				pData += strlen(pData) + 1;
			}

			// expect terminator
			assert(*pData == 0);
		}

		return 0;
	}

	/*
	 * @date 2021-05-13 12:06:33
	 *
	 * Save database to binary data file
	 * NOTE: Tree is compacted on writing
	 * NOTE: With larger trees over NFS, this may take fome time
	 */
	void saveFile(const char *fileName, bool showProgress = true) {

		assert(numRoots > 0);

		/*
		 * File header
		 */

		static baseTreeHeader_t header;
		memset(&header, 0, sizeof header);

		// zeros for alignment
		static uint8_t zero16[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		// current file position
		size_t         fpos       = 0;
		// crc for nodes/roots
		uint32_t       crc32      = 0;

//		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
//			fprintf(stderr, "[%s] Writing %s\n", ctx.timeAsString(), fileName);

		/*
		 * Open output file
		 */

//		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
//			fprintf(stderr, "\r\e[Kopening");

		FILE *outf = fopen(fileName, "w");
		if (!outf)
			ctx.fatal("Failed to open %s: %m\n", fileName);

		/*
		** Write empty header (overwritten later)
		*/
		fwrite(&header, sizeof header, 1, outf);
		fpos += sizeof header;

		/*
		 * Align
		 */
		size_t fillLen = 16 - (fpos & 15);
		if (fillLen < 16) {
			fwrite(zero16, fillLen, 1, outf);
			fpos += fillLen;
		}

		/*
		 * Write names
		 */
		header.offNames = fpos;

		// write keyNames
		for (uint32_t i = 0; i < nstart; i++) {
			size_t len = keyNames[i].length() + 1;
			assert(len > 1);
			fwrite(keyNames[i].c_str(), len, 1, outf);
			fpos += len;
		}
		// write rootNames
		for (uint32_t i = 0; i < numRoots; i++) {
			size_t len = rootNames[i].length() + 1;
			assert(len > 1);
			fwrite(rootNames[i].c_str(), len, 1, outf);
			fpos += len;
		}
		// write zero byte
		fwrite(zero16, 1, 1, outf);
		fpos += 1;

		/*
		 * Align
		 */
		fillLen = 16 - (fpos & 15);
		if (fillLen < 16) {
			fwrite(zero16, fillLen, 1, outf);
			fpos += fillLen;
		}

		/*
		 * Write nodes
		 */
		header.offNodes = fpos;

		/*
		 * Select  active nodes
		 */

		uint32_t *pMap  = allocMap();
		uint32_t nextId = 0; // next assignable node id

		if (0) {
			/*
			 * In case of emergency and the tree needs to be saved verbatim
			 */

			// output keys
			for (uint32_t iKey = 0; iKey < nstart; iKey++) {
				// get remapped
				baseNode_t wrtNode;
				wrtNode.Q = 0;
				wrtNode.T = IBIT;
				wrtNode.F = iKey;

				pMap[iKey] = nextId++;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.Q));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.T));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.F));

			}

			// output keys
			for (uint32_t iNode = nstart; iNode < ncount; iNode++) {
				const baseNode_t *pNode = this->N + iNode;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				// get remapped
				baseNode_t wrtNode;
				wrtNode.Q = pMap[Q];
				wrtNode.T = pMap[Tu] ^ Ti;
				wrtNode.F = pMap[F];

				pMap[iNode] = nextId++;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.Q));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.T));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.F));

			}
		} else {
			uint32_t *pStack     = allocMap();
			uint32_t *pVersion   = allocVersion();
			uint32_t thisVersion = ++mapVersionNr;
			uint32_t numStack    = 0; // top of stack

			// clear version map when wraparound
			if (thisVersion == 0) {
				::memset(pVersion, 0, maxNodes * sizeof *pVersion);
				thisVersion = ++mapVersionNr;
			}

			// output keys
			for (uint32_t iKey = 0; iKey < nstart; iKey++) {
				pVersion[iKey] = thisVersion;
				pMap[iKey]     = iKey;

				// get remapped
				baseNode_t wrtNode;
				wrtNode.Q = 0;
				wrtNode.T = IBIT;
				wrtNode.F = iKey;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				pMap[iKey] = nextId++;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.Q));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.T));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.F));

			}

			/*
			 * @date 2021-06-05 18:24:37
			 *
			 * trace roots, one at a time.
			 * Last root is a artificial root representing "system"
			 */
			for (uint32_t iRoot = 0; iRoot <= numRoots; iRoot++) {

				uint32_t R = (iRoot < numRoots) ? roots[iRoot] : system;

				numStack = 0;
				pStack[numStack++] = R & ~IBIT;

				/*
				 * Walk the tree depth-first
				 */
				do {
					// pop stack
					uint32_t curr = pStack[--numStack];

					if (curr < this->nstart)
						continue; // endpoints have already been output

					const baseNode_t *pNode = this->N + curr;
					const uint32_t   Q      = pNode->Q;
					const uint32_t   Tu     = pNode->T & ~IBIT;
					const uint32_t   Ti     = pNode->T & IBIT;
					const uint32_t   F      = pNode->F;

					// determine if already handled
					if (pVersion[curr] != thisVersion) {
						/*
						 * First time visit
						 */
						pVersion[curr] = thisVersion;
						pMap[curr]     = 0;

						// push id so it visits again after expanding
						pStack[numStack++] = curr;

						if (F)
							pStack[numStack++] = F;
						if (Tu != F && Tu)
							pStack[numStack++] = Tu;
						pStack[numStack++]         = Q;

						assert(numStack < maxNodes);

					} else if (pMap[curr] == 0) {
						/*
						 * Second time visit
						 */

						baseNode_t wrtNode;
						wrtNode.Q = pMap[Q];
						wrtNode.T = pMap[Tu] ^ Ti;
						wrtNode.F = pMap[F];

						size_t len = sizeof wrtNode;
						fwrite(&wrtNode, len, 1, outf);
						fpos += len;

						pMap[curr] = nextId++;

						__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.Q));
						__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.T));
						__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.F));
					}

				} while (numStack > 0);
			}
			freeVersion(pVersion);
			freeMap(pStack);
		}

		/*
		 * Align
		 */
		fillLen = 16 - (fpos & 15);
		if (fillLen < 16) {
			fwrite(zero16, fillLen, 1, outf);
			fpos += fillLen;
		}

		/*
		 * write roots
		 * Last root is a virtual root representing "system"
		 */
		header.offRoots = fpos;

		for (unsigned iRoot = 0; iRoot < numRoots; iRoot++) {
			uint32_t R = roots[iRoot];

			// new wrtRoot
			uint32_t wrtRoot = pMap[R & ~IBIT] ^(R & IBIT);

			__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtRoot));

			size_t len = sizeof wrtRoot;
			fwrite(&wrtRoot, len, 1, outf);
			fpos += len;
		}

		if (numHistory) {
			/*
			 * Align
			 */
			fillLen = 16 - (fpos & 15);
			if (fillLen < 16) {
				fwrite(zero16, fillLen, 1, outf);
				fpos += fillLen;
			}

			/*
			 * write history
			 */

			size_t len = sizeof(*history) * numHistory;
			fwrite(history, len, 1, outf);
			fpos += len;
		}

		/*
		 * Rewrite header and close
		 */

		header.magic       = BASETREE_MAGIC;
		header.magic_flags = flags;
		header.unused1     = unused1;
		header.system      = pMap[system & ~IBIT] ^ (system & IBIT);
		header.crc32       = crc32;
		header.kstart      = kstart;
		header.ostart      = ostart;
		header.estart      = estart;
		header.nstart      = nstart;
		header.ncount      = nextId; // NOTE: count equals the nodes actually written
		header.numRoots    = numRoots;
		header.numHistory  = numHistory;
		header.posHistory  = posHistory;
		header.offEnd      = fpos;

		// rewrite header
		fseek(outf, 0, SEEK_SET);
		fwrite(&header, sizeof header, 1, outf);

		// test for errors, most likely disk-full
		if (feof(outf) || ferror(outf)) {
			unlink(fileName);
			ctx.fatal("[ferror(%s,\"w\") returned: %m]\n", fileName);
		}

//		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
//			fprintf(stderr, "\r\e[Kclosing");

		// close
		if (fclose(outf)) {
			unlink(fileName);
			ctx.fatal("[fclose(%s,\"w\") returned: %m]\n", fileName);
		}

		if (showProgress && ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K"); // erase showProgress

		// release maps
		freeMap(pMap);


//		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
//			fprintf(stderr, "[%s] Written %s, %u nodes, %lu bytes\n", ctx.timeAsString(), fileName, ncount - nstart, header.offEnd);

		// make header available
		fileHeader = &header;
	}

	/*
	 * @date 2021-05-27 12:33:41
	 *
	 * Load metadata from text json file.
	 * NOTE: Tree has no other allocations and is unsuited to work with
	 * NOTE: `inputFilename` is only for error messages
	 */
	void loadFileJson(json_t *jInput, const char *inputFilename) {

		if (!keyNames.empty() || !rootNames.empty() || allocFlags || hndl >= 0)
			ctx.fatal("baseTree_t::loadFileJson() on non-initial tree\n");

		/*
		 * import dimensions
		 */
		kstart   = json_integer_value(json_object_get(jInput, "kstart"));
		ostart   = json_integer_value(json_object_get(jInput, "ostart"));
		estart   = json_integer_value(json_object_get(jInput, "estart"));
		nstart   = json_integer_value(json_object_get(jInput, "nstart"));
		ncount   = json_integer_value(json_object_get(jInput, "ncount"));
		numRoots = json_integer_value(json_object_get(jInput, "numroots"));

		if (kstart == 0 || kstart >= ncount) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("kstart out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "kstart", json_integer(kstart));
			json_object_set_new_nocheck(jError, "ncount", json_integer(ncount));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}
		if (ostart < kstart || ostart >= ncount) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("ostart out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "kstart", json_integer(kstart));
			json_object_set_new_nocheck(jError, "ostart", json_integer(ostart));
			json_object_set_new_nocheck(jError, "ncount", json_integer(ncount));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}
		if (estart < ostart || estart >= ncount) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("estart out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "ostart", json_integer(ostart));
			json_object_set_new_nocheck(jError, "estart", json_integer(estart));
			json_object_set_new_nocheck(jError, "ncount", json_integer(ncount));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}
		if (nstart < estart || nstart >= ncount) {
			/*
			 * NOTE: this test should be dropped as extended keys should always be set to uninitialised
			 */
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("nstart out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "estart", json_integer(estart));
			json_object_set_new_nocheck(jError, "nstart", json_integer(nstart));
			json_object_set_new_nocheck(jError, "ncount", json_integer(ncount));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}
		if (numRoots < estart) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("numroots out of range"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "numroots", json_integer(numRoots));
			json_object_set_new_nocheck(jError, "estart", json_integer(estart));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		// make all `keyNames`+`rootNames` indices valid
		keyNames.resize(nstart);
		rootNames.resize(numRoots);

		/*
		 * Reserved names
		 */
		keyNames[0]            = "0";
		keyNames[1 /*KERROR*/] = "KERROR";

		/*
		 * import knames
		 */

		json_t *jNames = json_object_get(jInput, "knames");
		if (!jNames) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag 'knames'"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		unsigned numNames = json_array_size(jNames);
		if (numNames != ostart - kstart) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incorrect number of knames"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "expected", json_integer(ostart - kstart));
			json_object_set_new_nocheck(jError, "encountered", json_integer(numNames));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		for (uint32_t iName = 0; iName < numNames; iName++)
			keyNames[kstart + iName] = json_string_value(json_array_get(jNames, iName));

		/*
		 * import onames
		 */

		jNames = json_object_get(jInput, "onames");
		if (!jNames) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag 'onames'"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		numNames = json_array_size(jNames);
		if (numNames != estart - ostart) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incorrect number of onames"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "expected", json_integer(estart - ostart));
			json_object_set_new_nocheck(jError, "encountered", json_integer(numNames));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		for (uint32_t iName = 0; iName < numNames; iName++)
			keyNames[ostart + iName] = json_string_value(json_array_get(jNames, iName));

		/*
		 * import enames
		 */

		jNames = json_object_get(jInput, "enames");
		if (!jNames) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag 'enames'"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		numNames = json_array_size(jNames);
		if (numNames != nstart - estart) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incorrect number of enames"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "expected", json_integer(nstart - estart));
			json_object_set_new_nocheck(jError, "encountered", json_integer(numNames));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		for (uint32_t iName = 0; iName < numNames; iName++)
			keyNames[estart + iName] = json_string_value(json_array_get(jNames, iName));

		/*
		 * import rnames (extended root names)
		 */

		// copy fixed part
		for (unsigned iRoot = 0; iRoot < estart; iRoot++)
			rootNames[iRoot] = keyNames[iRoot];

		jNames = json_object_get(jInput, "rnames");
		if (!jNames) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Missing tag 'rnames'"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		}

		numNames = json_array_size(jNames);

		if (json_is_string(jNames) && strcasecmp(json_string_value(jNames), "enames") == 0) {
			// roots identical to keys
			if (nstart != numRoots) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("rnames == enames AND nstart != numRoots"));
				json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
				json_object_set_new_nocheck(jError, "nstart", json_integer(nstart));
				json_object_set_new_nocheck(jError, "numroots", json_integer(numRoots));
				printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
				exit(1);
			}
			// copy collection
			rootNames = keyNames;
		} else if (numNames != numRoots - estart) {
			// count mismatch
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("Incorrect number of rnames"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			json_object_set_new_nocheck(jError, "expected", json_integer(numRoots - estart));
			json_object_set_new_nocheck(jError, "encountered", json_integer(numNames));
			printf("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			exit(1);
		} else {
			// load root names
			for (uint32_t iName = 0; iName < numNames; iName++)
				rootNames[estart + iName] = json_string_value(json_array_get(jNames, iName));
		}
	}

	/*
	 * Extract details into json
	 */
	json_t *headerInfo(json_t *jResult) {
		if (jResult == NULL)
			jResult = json_object();

		char crcstr[32];
		sprintf(crcstr, "%08x", fileHeader->crc32);

		json_object_set_new_nocheck(jResult, "flags", json_integer(fileHeader->magic_flags));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader->offEnd));
		json_object_set_new_nocheck(jResult, "crc", json_string_nocheck(crcstr));
		json_object_set_new_nocheck(jResult, "kstart", json_integer(fileHeader->kstart));
		json_object_set_new_nocheck(jResult, "ostart", json_integer(fileHeader->ostart));
		json_object_set_new_nocheck(jResult, "estart", json_integer(fileHeader->estart));
		json_object_set_new_nocheck(jResult, "nstart", json_integer(fileHeader->nstart));
		json_object_set_new_nocheck(jResult, "ncount", json_integer(fileHeader->ncount));
		json_object_set_new_nocheck(jResult, "numnodes", json_integer(fileHeader->ncount - fileHeader->nstart));
		json_object_set_new_nocheck(jResult, "numroots", json_integer(fileHeader->numRoots));
		json_object_set_new_nocheck(jResult, "system", json_integer(fileHeader->system));
		json_object_set_new_nocheck(jResult, "numhistory", json_integer(fileHeader->numHistory));
		json_object_set_new_nocheck(jResult, "poshistory", json_integer(fileHeader->posHistory));

		return jResult;
	}

	/*
	 * Extract details into json
	 */
	json_t *extraInfo(json_t *jResult) {
		if (jResult == NULL)
			jResult = json_object();

		/*
		 * Key/root names
		 */
		json_t *jKeyNames = json_array();

		// input key names
		for (uint32_t iKey = kstart; iKey < ostart; iKey++)
			json_array_append_new(jKeyNames, json_string_nocheck(keyNames[iKey].c_str()));
		json_object_set_new_nocheck(jResult, "knames", jKeyNames);

		jKeyNames = json_array();

		// output key names
		for (uint32_t iKey = ostart; iKey < estart; iKey++)
			json_array_append_new(jKeyNames, json_string_nocheck(keyNames[iKey].c_str()));
		json_object_set_new_nocheck(jResult, "onames", jKeyNames);

		jKeyNames = json_array();

		// extended key names
		for (uint32_t iKey = estart; iKey < nstart; iKey++)
			json_array_append_new(jKeyNames, json_string_nocheck(keyNames[iKey].c_str()));
		json_object_set_new_nocheck(jResult, "enames", jKeyNames);

		// extended root names (which might be identical to enames)
		bool rootsDiffer = (nstart != numRoots);
		if (!rootsDiffer) {
			for (uint32_t iKey = 0; iKey < nstart; iKey++) {
				if (keyNames[iKey].compare(rootNames[iKey]) != 0) {
					rootsDiffer = true;
					break;
				}
			}
		}

		if (rootsDiffer) {
			// either roots are different or an empty set.
			jKeyNames = json_array();

			for (uint32_t iRoot = estart; iRoot < numRoots; iRoot++)
				json_array_append_new(jKeyNames, json_string_nocheck(rootNames[iRoot].c_str()));
			json_object_set_new_nocheck(jResult, "rnames", jKeyNames);
		} else {
			json_object_set_new_nocheck(jResult, "rnames", json_string_nocheck("enames"));
		}

#if 0


		/*
		 * Roots
		 */

		json_t *jRoots = json_array();
		json_t *jRelease = json_array();


		for (uint32_t i = 0; i < this->xstart; i++) {
			if (this->roots[i] == BASENODE_ERROR) {
				json_array_append_new(jRelease, json_string_nocheck(this->kToStr(i)));
			} else if (this->roots[i] != i) {
				json_array_append_new(jRoots, json_string_nocheck( this->kToStr(i)));
			}
		}

		json_object_set_new_nocheck(jResult, "roots", jRoots);
		json_object_set_new_nocheck(jResult, "release", jRelease);

		/*
		 * History
		 */

		json_t *jHistory = json_array();


		for (uint32_t i = 0; i < this->numHistory; i++) {
			json_array_append_new(jHistory, json_string_nocheck( this->kToStr(this->history[i])));
		}

		json_object_set_new_nocheck(jResult, "history", jHistory);
#endif

		/*
		 * Refcounts
		 */

		uint32_t *pRefCount = allocMap();

		for (uint32_t iKey = 0; iKey < this->nstart; iKey++)
			pRefCount[iKey] = 0;

		for (uint32_t k = this->nstart; k < this->ncount; k++) {
			const baseNode_t *pNode = this->N + k;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
//			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			pRefCount[Q]++;
			if (Tu != F)
				pRefCount[Tu]++;
			pRefCount[F]++;
		}

		json_t *jRefCount = json_object();

		for (uint32_t iKey = this->kstart; iKey < this->nstart; iKey++) {
			if (pRefCount[iKey])
				json_object_set_new_nocheck(jRefCount, keyNames[iKey].c_str(), json_integer(pRefCount[iKey]));
		}
		json_object_set_new_nocheck(jResult, "refcount", jRefCount);

#if 0
		/*
		 * Requires (as decimal number)
		 */

		json_t *jRequires = json_array();

		for (uint32_t i = this->xstart; i < this->nstart; i++) {
			if (pRefCounts[i])
				json_array_append_new(jRequires, json_integer(i));
		}

		json_object_set_new_nocheck(jResult, "requires", jRequires);

		/*
		 * Providers (as decimal number)
		 */

		json_t *jProvides = json_array();

		for (uint32_t i = this->xstart; i < this->numRoots; i++) {
			if (this->roots[i] != BASENODE_ERROR) {
				json_array_append_new(jProvides, json_integer(i));
			}
		}

		json_object_set_new_nocheck(jResult, "provides", jProvides);

		freeMap(pRefCounts);
#endif

		return jResult;
	}
};

#endif
