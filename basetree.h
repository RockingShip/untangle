#ifndef _BASETREE_H
#define _BASETREE_H

#include <fcntl.h>
#include <jansson.h>
#include <sys/mman.h>
#include <sys/stat.h>
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
	uint32_t keysId;              //
	uint32_t rootsId;             //
	uint32_t crc32;               // crc of nodes/roots, calculated during save

	// primary fields
	uint32_t kstart;              // first input key id
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
	uint32_t   keysId;		// id of tree containing referenced keys
	uint32_t   rootsId;		// id of this tree containing roots/results
	// primary fields
	uint32_t   kstart;		// first input key id. Identical for all trees in chain.
	uint32_t   estart;		// first external/extended key id. Roots from previous tree in chain.
	uint32_t   nstart;		// id of first node
	uint32_t   ncount;		// number of nodes in use
	uint32_t   maxNodes;		// maximum tree capacity
	uint32_t   numRoots;		// entries in roots[]
	// names
	const char **keyNames;		// sliced version of `keyNameData`
	const char **rootNames;		// sliced version of `rootNameData`
	const char *nameData;		// unsliced key names
	// primary storage
	baseNode_t *N;			// nodes
	uint32_t   *roots;		// entry points. can be inverted
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
	uint32_t   **gPoolMap;		// Pool of available node-id maps
	unsigned   numPoolVersion;	// Number of version-id pools in use
	uint32_t   **gPoolVersion;	// Pool of available version-id maps
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
	//@formatter:on

	/*
	 * Create a memory stored tree
	 */
	baseTree_t(context_t &ctx, uint32_t kstart, uint32_t nstart, uint32_t numRoots, uint32_t maxNodes, uint32_t flags) :
	//@formatter:off
		ctx(ctx),
		hndl(-1),
		rawDatabase(NULL),
		fileHeader(NULL),
		// meta
		flags(flags),
		allocFlags(0),
		keysId(0),
		rootsId(0),
		// primary fields
		kstart(kstart),
		estart(nstart),
		nstart(nstart),
		ncount(nstart),
		maxNodes(maxNodes),
		numRoots(numRoots),
		// names
		keyNames((const char **) ctx.myAlloc("baseTree_t::keyNames", nstart, sizeof *keyNames) ),
		rootNames((const char **) ctx.myAlloc("baseTree_t::rootNames", numRoots, sizeof *rootNames) ),
		nameData(NULL),
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
		gPoolMap((uint32_t **) ctx.myAlloc("baseTree_t::gPoolMap", MAXPOOLARRAY, sizeof(*gPoolMap))),
		numPoolVersion(0),
		gPoolVersion( (uint32_t **) ctx.myAlloc("baseTree_t::gPoolVersion", MAXPOOLARRAY, sizeof(*gPoolVersion))),
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
		if (this->nameData)
			allocFlags |= ALLOCMASK_NAMES;
		if (this->N)
			allocFlags |= ALLOCMASK_NODES;
		if (this->roots)
			allocFlags |= ALLOCMASK_ROOTS;
		if (this->history)
			allocFlags |= ALLOCMASK_HISTORY;
		if (this->nodeIndex)
			allocFlags |= ALLOCMASK_INDEX;

#if ENABLE_BASEEVALUATOR
		gBaseEvaluator = new baseEvaluator_t(kstart, ostart, nstart, maxNodes);
#endif
	}

	/*
	 * Release system resources
	 */
	~baseTree_t() {
		// release allocations if not mmapped
		if (keyNames)
			ctx.myFree("baseTree_t::keyNames", this->keyNames);
		if (rootNames)
			ctx.myFree("baseTree_t::rootNames", this->rootNames);
		if (allocFlags & ALLOCMASK_NAMES)
			ctx.myFree("baseTree_t::nameData", (char *) this->nameData); // this has been `malloc()` and cast to const
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
		freeMap(stackL);
		freeMap(stackR);
		freeMap(compVersionL);
		freeMap(compVersionR);
		freeMap(compNodeL);
		freeMap(compNodeR);

		// release pools
		while (numPoolMap > 0)
			ctx.myFree("baseTree_t::nodeMap", gPoolMap[--numPoolMap]);
		while (numPoolVersion > 0)
			ctx.myFree("baseTree_t::versionMap", gPoolVersion[--numPoolVersion]);

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
			pMap = gPoolMap[--numPoolMap];
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
		gPoolMap[numPoolMap++] = pMap;
		pMap = NULL;
	}

	/**
	 * @date 2021-05-12 00:45:03
	 *
	 * Allocate a map that can hold versioned memory id's
	 * Returned map is uninitialised and should ONLY contain previous (lower) version numbers
	 *
	 * @return {uint32_t*} - Uninitialised map for versioned memory id's
	 */
	uint32_t *allocVersion(void) {
		uint32_t *pVersion;

		if (numPoolVersion > 0) {
			// get first free version map
			pVersion = gPoolVersion[--numPoolVersion];
		} else {
			// allocate new map
			pVersion = (uint32_t *) ctx.myAlloc("baseTree_t::versionMap", maxNodes, sizeof *pVersion);
		}

		// if version increment would cause a wraparound, clear map
		if (((mapVersionNr + 1) & 0xffffffff) == 0)
			::memset(pVersion, 0, maxNodes * sizeof *pVersion);

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
		gPoolVersion[numPoolVersion++] = pVersion;
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
	 * @date 2021-05-14 21:18:32
	 *
	 * Load database from file
	 *
	 * return 0 for ok.
	 */
	unsigned loadFile(const char *fileName, bool shared = true) {

		/*
		 * release allocations
		 */
		if (keyNames)
			ctx.myFree("baseTree_t::keyNames", keyNames);
		if (rootNames)
			ctx.myFree("baseTree_t::rootNames", rootNames);
		if (allocFlags & baseTree_t::ALLOCMASK_NAMES)
			ctx.myFree("baseTree_t::nameData", (char *) nameData); // this has been `malloc()` and cast to const
		if (allocFlags & baseTree_t::ALLOCMASK_NODES)
			ctx.myFree("baseTree_t::N", N);
		if (allocFlags & baseTree_t::ALLOCMASK_ROOTS)
			ctx.myFree("baseTree_t::roots", roots);
		if (allocFlags & baseTree_t::ALLOCMASK_HISTORY)
			ctx.myFree("baseTree_t::history", history);
		if (allocFlags & baseTree_t::ALLOCMASK_INDEX) {
			ctx.myFree("baseTree_t::nodeIndex", nodeIndex);
			ctx.myFree("baseTree_t::nodeIndexVersion", nodeIndexVersion);
		}

		allocFlags &= ~baseTree_t::ALLOCMASK_NAMES;
		allocFlags &= ~baseTree_t::ALLOCMASK_NODES;
		allocFlags &= ~baseTree_t::ALLOCMASK_ROOTS;
		allocFlags &= ~baseTree_t::ALLOCMASK_HISTORY;
		allocFlags &= ~baseTree_t::ALLOCMASK_INDEX;

		// release pools
		while (numPoolMap > 0)
			ctx.myFree("baseTree_t::nodeMap", gPoolMap[--numPoolMap]);
		while (numPoolVersion > 0)
			ctx.myFree("baseTree_t::versionMap", gPoolVersion[--numPoolVersion]);

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
		keysId     = fileHeader->keysId;
		rootsId    = fileHeader->rootsId;
		kstart     = fileHeader->kstart;
		estart     = fileHeader->estart;
		nstart     = fileHeader->nstart;
		ncount     = fileHeader->ncount;
		numRoots   = fileHeader->numRoots;
		numHistory = fileHeader->numHistory;
		posHistory = fileHeader->posHistory;

		// @date 2021-05-14 21:46:35 Tree is read-only
		maxNodes = ncount; // used for map allocations

		keyNames  = (const char **) ctx.myAlloc("baseTree_t::keyNames", nstart, sizeof *keyNames);
		rootNames = (const char **) ctx.myAlloc("baseTree_t::rootNames", numRoots, sizeof *rootNames);
		nameData  = (const char *) (rawDatabase + fileHeader->offNames);
		N         = (baseNode_t *) (rawDatabase + fileHeader->offNodes);
		roots     = (uint32_t *) (rawDatabase + fileHeader->offRoots);
		history   = (uint32_t *) (rawDatabase + fileHeader->offHistory);

		// slice names
		{
			const char *pData = nameData;

			for (uint32_t iKey  = 0; iKey < nstart; iKey++) {
				keyNames[iKey] = pData;
				pData += strlen(pData) + 1;
			}
			for (uint32_t iRoot = 0; iRoot < numRoots; iRoot++) {
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
	 * Save database to file
	 * NOTE: Tree is compacted on writing
	 * NOTE: With larger trees over NFS, this may take fome time
	 */
	void saveFile(const char *fileName) {

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
			size_t len = strlen(keyNames[i]) + 1;
			fwrite(keyNames[i], len, 1, outf);
			fpos += len;
		}
		// write rootNames
		for (uint32_t i = 0; i < numRoots; i++) {
			size_t len = strlen(rootNames[i]) + 1;
			fwrite(rootNames[i], len, 1, outf);
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
		 * Isolate active nodes
		 */

		uint32_t *pSelect    = allocVersion();
		uint32_t *pMap       = allocMap();
		uint32_t thisVersion = ++mapVersionNr; // may wraparound
		uint32_t numNodes    = 0;

		for (uint32_t iKey = 0; iKey < nstart; iKey++) {
			pSelect[iKey] = thisVersion;
			pMap[iKey]    = iKey;
			numNodes++;
		}

		// mark roots
		for (uint32_t i = 0; i < numRoots; i++)
			pSelect[roots[i] & ~IBIT] = thisVersion;

		// collect counts
		for (uint32_t i = ncount - 1; i >= nstart; --i) {
			if (pSelect[i] == thisVersion) {
				const baseNode_t *pNode = N + i;

				pSelect[pNode->Q]         = thisVersion;
				pSelect[pNode->T & ~IBIT] = thisVersion;
				pSelect[pNode->F]         = thisVersion;
				numNodes++;
			}
		}

		/*
		 * Write nodes
		 */
		header.offNodes = fpos;

		ctx.tick = 0; // clear ticker timer
		uint32_t nextId = 0; // next free node id

		for (uint32_t i = 0; i < ncount; i++) {
			if (pSelect[i] == thisVersion) {
				const baseNode_t *pNode = N + i;

				// get remapped
				baseNode_t newNode;
				newNode.Q = pMap[pNode->Q];
				newNode.T = pMap[pNode->T & ~IBIT] ^ (pNode->T & IBIT);
				newNode.F = pMap[pNode->F];
				pMap[i] = nextId++;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(newNode.Q));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(newNode.T));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(newNode.F));

				size_t len = sizeof newNode;
				fwrite(&newNode, len, 1, outf);
				fpos += len;

				// update ticker for slow files across NFS
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					fprintf(stderr, "\r\e[K%.5f%%", nextId * 100.0 / numNodes);
					ctx.tick = 0;
				}
			}
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
		 */
		header.offRoots = fpos;

		for (unsigned i = 0; i < numRoots; i++) {
			// new root
			uint32_t r = pMap[roots[i] & ~IBIT] ^(roots[i] & IBIT);

			__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(r));

			size_t len = sizeof r;
			fwrite(&r, len, 1, outf);
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

		// release maps
		freeMap(pMap);
		freeVersion(pSelect);

		/*
		 * Rewrite header and close
		 */

		header.magic       = BASETREE_MAGIC;
		header.magic_flags = flags;
		header.keysId      = keysId;
		header.rootsId     = rootsId;
		header.crc32       = crc32;
		header.kstart      = kstart;
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

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K"); // erase progress

//		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
//			fprintf(stderr, "[%s] Written %s, %u nodes, %lu bytes\n", ctx.timeAsString(), fileName, ncount - nstart, header.offEnd);

		// make header available
		fileHeader = &header;
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
		json_object_set_new_nocheck(jResult, "keysId", json_integer(fileHeader->keysId));
		json_object_set_new_nocheck(jResult, "rootsId", json_integer(fileHeader->rootsId));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader->offEnd));
		json_object_set_new_nocheck(jResult, "crc", json_string_nocheck(crcstr));
		json_object_set_new_nocheck(jResult, "kstart", json_integer(fileHeader->kstart));
		json_object_set_new_nocheck(jResult, "estart", json_integer(fileHeader->estart));
		json_object_set_new_nocheck(jResult, "nstart", json_integer(fileHeader->nstart));
		json_object_set_new_nocheck(jResult, "ncount", json_integer(fileHeader->ncount));
		json_object_set_new_nocheck(jResult, "numnodes", json_integer(fileHeader->ncount - fileHeader->nstart));
		json_object_set_new_nocheck(jResult, "numroots", json_integer(fileHeader->numRoots));
		json_object_set_new_nocheck(jResult, "numhistory", json_integer(fileHeader->numHistory));
		json_object_set_new_nocheck(jResult, "poshistory", json_integer(fileHeader->posHistory));

		return jResult;
	}

	/*
	 * Extract details into json
	 */
	json_t *extraInfo(json_t *jResult) const {
		if (jResult == NULL)
			jResult = json_object();

		/*
		 * Key names
		 */
		json_t *jKeyNames = json_array();

		for (uint32_t iKey = 0; iKey < this->nstart; iKey++)
			json_array_append_new(jKeyNames, json_string_nocheck(this->keyNames[iKey]));
		json_object_set_new_nocheck(jResult, "keys", jKeyNames);

		/*
		 * Root names
		 */
		json_t *jRootNames = json_array();

		for (uint32_t iRoot = 0; iRoot < this->numRoots; iRoot++)
			json_array_append_new(jRootNames, json_string_nocheck(this->rootNames[iRoot]));
		json_object_set_new_nocheck(jResult, "roots", jRootNames);

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

		/*
		 * Refcounts
		 */

		uint32_t *pRefCounts = allocMap();

		for (uint32_t i = 0; i < this->nstart; i++)
			pRefCounts[i] = 0;

		for (uint32_t k = this->nstart; k < this->xcount; k++) {
			const baseNode_t *pNode = this->N + k;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			if (Q && Q < this->nstart)
				pRefCounts[Q]++;
			if (Tu && Tu < this->nstart && Tu != F)
				pRefCounts[Tu]++;
			if (F && F < this->nstart)
				pRefCounts[F]++;

		}

		json_t *jRefCounts = json_object();

		for (uint32_t i = this->kstart; i < this->nstart; i++) {
			if (pRefCounts[i])
				json_object_set_new_nocheck(jRefCounts, this->kToStr(i), json_integer(pRefCounts[i]));
		}
		json_object_set_new_nocheck(jResult, "refcounts", jRefCounts);

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
