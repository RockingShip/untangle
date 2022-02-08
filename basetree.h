#ifndef _BASETREE_H
#define _BASETREE_H

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

#include <fcntl.h>
#include <jansson.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>

#include "context.h"
#include "rewritedata.h"

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

/*
 * The top-level node rewriting is a level-2 normalisation replacement that examines Q/T?F components
 * This is a speed optimisation and enables code to display the decisions taken while walking.
 * Displaying is performed when running with `--debug=`.
 */
#ifndef ENABLE_DEBUG_REWRITE
#define ENABLE_DEBUG_REWRITE 0
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
		return (T & IBIT) && F == 0;
	}

	// NE (L?~R:R) third because Ti is set (QnTF) but Tu==F
	inline bool __attribute__((pure)) isNE(void) const {
		return (T ^ IBIT) == F;
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
		ALLOCFLAG_NAMES = 0,	// entry/root names
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

	// resources
	context_t                &ctx;                  // resource context
	int                      hndl;                  // file handle
	uint8_t                  *rawData;              // base location of mmap segment
	baseTreeHeader_t         *fileHeader;           // file header
	// meta
	uint32_t                 flags;                 // creation constraints
	uint32_t                 allocFlags;            // memory constraints
	uint32_t                 unused1;               //
	uint32_t                 system;                // node of balanced system
	// primary fields
	uint32_t                 kstart;                // first input key id.
	uint32_t                 ostart;                // first output key id.
	uint32_t                 estart;                // first external/extended key id. Roots from previous tree in chain.
	uint32_t                 nstart;                // id of first node
	uint32_t                 ncount;                // number of nodes in use
	uint32_t                 maxNodes;              // maximum tree capacity
	uint32_t                 numRoots;              // entries in roots[]
	// names
	std::vector<std::string> entryNames;            // sliced version of `entryNameData`
	std::vector<std::string> rootNames;             // sliced version of `rootNameData`
	// primary storage
	baseNode_t               *N;                    // nodes
	uint32_t                 *roots;                // entry points. can be inverted. first estart entries should match keys
	// history
	uint32_t                 numHistory;            //
	uint32_t                 posHistory;            //
	uint32_t                 *history;              //
	// node index
	uint32_t                 nodeIndexSize;         // hash/cache size. MUST BE PRIME!
	uint32_t                 *nodeIndex;            // index to nodes
	uint32_t                 *nodeIndexVersion;     // content version
	uint32_t                 nodeIndexVersionNr;    // active version number
	// pools
	unsigned                 numPoolMap;            // Number of node-id pools in use
	uint32_t                 **pPoolMap;            // Pool of available node-id maps
	unsigned                 numPoolVersion;        // Number of version-id pools in use
	uint32_t                 **pPoolVersion;        // Pool of available version-id maps
	uint32_t                 mapVersionNr;          // Version number
	// structure based compare
	uint32_t                 *stackL;               // id of lhs
	uint32_t                 *stackR;               // id of rhs
	uint32_t                 *compBeenWhatL;        // versioned memory for compare - visited node id Left
	uint32_t                 *compBeenWhatR;
	uint32_t                 *compVersionL;         // versioned memory for compare - content version
	uint32_t                 *compVersionR;
	uint32_t                 compVersionNr;         // versioned memory for compare - active version number
	uint64_t                 numCompare;            // number of compares performed
	// rewrite normalisation
	uint32_t                 *rewriteMap;           // results of intermediate lookups
	uint32_t                 *rewriteVersion;       // versioned memory for rewrites
	uint32_t                 iVersionRewrite;       // active version number
	uint64_t                 numRewrite;            // number of rewrites performed

	/**
	 * @date 2021-06-13 00:01:50
	 *
	 * Copy/Assign constructors not supported.
	 * Let usage trigger a "linker not found" error
	 */
	baseTree_t(const baseTree_t &rhs);
	baseTree_t &operator=(const baseTree_t &rhs);

	/*
	 * Create an empty tree, placeholder for reading from file
	 */
	baseTree_t(context_t &ctx) :
		ctx(ctx),
		hndl(-1),
		rawData(NULL),
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
		entryNames(),
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
		compBeenWhatL(NULL),
		compBeenWhatR(NULL),
		compVersionL(NULL), // allocate as node-id map because of local version numbering
		compVersionR(NULL),  // allocate as node-id map because of local version numbering
		compVersionNr(1),
		numCompare(0),
		// rewrite normalisation
		rewriteMap(NULL),
		rewriteVersion(NULL),
		iVersionRewrite(1),
		numRewrite(0) {
	}

	/*
	 * Create a memory stored tree
	 */
	baseTree_t(context_t &ctx, uint32_t kstart, uint32_t ostart, uint32_t estart, uint32_t nstart, uint32_t numRoots, uint32_t maxNodes, uint32_t flags) :
	//@formatter:off
		ctx(ctx),
		hndl(-1),
		rawData(NULL),
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
		entryNames(),
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
		compBeenWhatL(allocMap()),
		compBeenWhatR(allocMap()),
		compVersionL(allocMap()), // allocate as node-id map because of local version numbering
		compVersionR(allocMap()),  // allocate as node-id map because of local version numbering
		compVersionNr(1),
		numCompare(0),
		// rewrite normalisation
		rewriteMap(allocMap()),
		rewriteVersion(allocMap()), // allocate as node-id map because of local version numbering
		iVersionRewrite(1),
		numRewrite(0)
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

		// make all `entryNames`+`rootNames` indices valid
		entryNames.resize(nstart);
		rootNames.resize(numRoots);

		// setup default entrypoints
		for (unsigned iEntry = 0; iEntry < nstart; iEntry++) {
			N[iEntry].Q = 0;
			N[iEntry].T = IBIT;
			N[iEntry].F = iEntry;
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
	virtual ~baseTree_t() {
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
		if (compBeenWhatL)
			freeMap(compBeenWhatL);
		if (compBeenWhatR)
			freeMap(compBeenWhatR);

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
			ret = ::munmap((void *) rawData, fileHeader->offEnd);
			if (ret)
				ctx.fatal("munmap() returned: %m\n");
			ret = ::close(hndl);
			if (ret)
				ctx.fatal("close() returned: %m\n");
		} else if (rawData) {
			/*
			 * Database was read into `malloc()` buffer
			 */
			ctx.myFree("baseTreeFile_t::rawData", rawData);
		}

		// zombies need to trigger SEGV
		rawData    = NULL;
		fileHeader = NULL;
		N                = NULL;
		roots            = NULL;
		history          = NULL;
		nodeIndex        = NULL;
		nodeIndexVersion = NULL;
		pPoolMap         = NULL;
		pPoolVersion     = NULL;
		stackL           = NULL;
		stackR           = NULL;
		compBeenWhatL    = NULL;
		compBeenWhatR    = NULL;
		compVersionL     = NULL;
		compVersionR     = NULL;
		rewriteMap       = NULL;
		rewriteVersion   = NULL;
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
	 * Types of communicative dyadics/cascades
	 */
	enum { CASCADE_NONE, CASCADE_OR, CASCADE_NE, CASCADE_AND, CASCADE_SYNC };

	/*
	 * @date 2021-05-12 01:23:06
	 *
	 * Compare two-subtrees
	 * When encountering a cascade, walk all terms which might result in a different path for left/right
	 * The `topLevelCascade` is used by `addOrderNode()` to simulate that arguments belong to same cascade
	 * Otherwise things like `addOrderedNE(`ab^`,`b`) will fail to complete the left-hand-side cascade walk.
	 *
	 * NOTE: Only key id's can be compared, node id's can only compare equality (=)
	 *       comparing enumeration requires walking the tree.
	 *
	 * return:
	 *      -1 L < R
	 *       0 L = R
	 *      +1 L > R
	 */
	int compare(uint32_t lhs, baseTree_t *treeR, uint32_t rhs, unsigned topLevelCascade = CASCADE_NONE) {

		/*
		 * This code is a resource hit, but worth the effort.
		 * With the new QnTF components, there are only 3 things that differentiate left.
		 * Zero as grounding stop this function recursing all the way to the endpoints.
		 * This will hopefully stop swap-hotspot oscillation making nested `grows` synchronise.
		 *
		 * NOTE:
		 * The detector will reorder left/right appropriately, so no chance a swapped version will be created anywhere else.
		 *
		 * This function is designed to be not recursive, so static variables are no longer required.
		 */

		uint32_t thisVersionL = ++this->compVersionNr;
		uint32_t thisVersionR = ++treeR->compVersionNr;
		// version overflow, clear
		if (thisVersionL == 0) {
			::memset(this->compVersionL, 0, this->maxNodes * sizeof *compVersionL);
			thisVersionL = ++this->compVersionNr;
		}
		if (thisVersionR == 0) {
			::memset(treeR->compVersionR, 0, treeR->maxNodes * sizeof *compVersionR);
			thisVersionR = ++treeR->compVersionNr;
		}
		this->numCompare++; // only for L

		assert(!(lhs & IBIT));
		assert(!(rhs & IBIT));

		uint32_t numStackL      = 0; // top of stack
		uint32_t numStackR      = 0; // top of stack
		uint32_t parentCascadeL = CASCADE_NONE; // parent of current cascading node
		uint32_t parentCascadeR = CASCADE_NONE; // parent of current cascading node

		// push arguments on stack
		this->stackL[numStackL++] = topLevelCascade;
		this->stackL[numStackL++] = lhs;
		treeR->stackR[numStackR++] = topLevelCascade;
		treeR->stackR[numStackR++] = rhs;

		if (ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE))
			fprintf(stderr, "compare(%x,%x)\n", lhs, rhs);

		do {
			uint32_t         L, R;
			const baseNode_t *pNodeL, *pNodeR;

			/*
			 * sync left/right to traverse cascade border
			 * unwind node if part of the parent cascade until border reached
			 * This should align cascades. eg: `abc++` and `ab+c+`, `ab+cd++` and `abd++`.
			 */
			for (;;) {
				L              = this->stackL[--numStackL];
				parentCascadeL = this->stackL[--numStackL];

				pNodeL = this->N + L;

				if (L < this->nstart) {
					break;
				} else if (parentCascadeL == CASCADE_SYNC) {
					break;
				} else if (parentCascadeL == CASCADE_OR && pNodeL->isOR()) {
					this->stackL[numStackL++] = parentCascadeL;
					this->stackL[numStackL++] = pNodeL->F;
					this->stackL[numStackL++] = parentCascadeL;
					this->stackL[numStackL++] = pNodeL->Q;
				} else if (parentCascadeL == CASCADE_NE && pNodeL->isNE()) {
					this->stackL[numStackL++] = parentCascadeL;
					this->stackL[numStackL++] = pNodeL->F;
					this->stackL[numStackL++] = parentCascadeL;
					this->stackL[numStackL++] = pNodeL->Q;
				} else if (parentCascadeL == CASCADE_AND && pNodeL->isAND()) {
					this->stackL[numStackL++] = parentCascadeL;
					this->stackL[numStackL++] = pNodeL->T;
					this->stackL[numStackL++] = parentCascadeL;
					this->stackL[numStackL++] = pNodeL->Q;
				} else {
					break;
				}
			}
			for (;;) {
				R              = treeR->stackR[--numStackR];
				parentCascadeR = treeR->stackR[--numStackR];

				pNodeR = treeR->N + R;

				if (R < treeR->nstart) {
					break;
				} else if (parentCascadeR == CASCADE_SYNC) {
					break;
				} else if (parentCascadeR == CASCADE_OR && pNodeR->isOR()) {
					treeR->stackR[numStackR++] = parentCascadeR;
					treeR->stackR[numStackR++] = pNodeR->F;
					treeR->stackR[numStackR++] = parentCascadeR;
					treeR->stackR[numStackR++] = pNodeR->Q;
				} else if (parentCascadeR == CASCADE_NE && pNodeR->isNE()) {
					treeR->stackR[numStackR++] = parentCascadeR;
					treeR->stackR[numStackR++] = pNodeR->F;
					treeR->stackR[numStackR++] = parentCascadeR;
					treeR->stackR[numStackR++] = pNodeR->Q;
				} else if (parentCascadeR == CASCADE_AND && pNodeR->isAND()) {
					treeR->stackR[numStackR++] = parentCascadeR;
					treeR->stackR[numStackR++] = pNodeR->T;
					treeR->stackR[numStackR++] = parentCascadeR;
					treeR->stackR[numStackR++] = pNodeR->Q;
				} else {
					break;
				}
			}

			/*
			 * Test if cascades are exhausted
			 */
			if (parentCascadeL != parentCascadeR) {
				if (numStackL < numStackR || parentCascadeL == CASCADE_SYNC)
					return -1; // `lhs` exhausted
				if (numStackL > numStackR || parentCascadeR == CASCADE_SYNC)
					return +1; // `rhs` exhausted
				assert(0);
			}

			if (ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE))
				fprintf(stderr, "%x:[%x %x %x] %x:[%x %x %x]\n",
					L, this->N[L].Q, this->N[L].T, this->N[L].F,
					R, treeR->N[R].Q, treeR->N[R].T, treeR->N[R].F);

			// for same tree, identical lhs/rhs implies equal
			if (L == R && this == treeR)
				continue;

			/*
			 * compare if either is an endpoint
			 */
			if (L < this->nstart && R >= treeR->nstart && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1a\n");
			if (L < this->nstart && R >= treeR->nstart)
				return -1; // `end` < `ref`
			if (L >= this->nstart && R < treeR->nstart && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1a\n");
			if (L >= this->nstart && R < treeR->nstart)
				return +1; // `ref` > `end`

			/*
			 * compare contents
			 */
			if (L < this->nstart) {
				if (ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) { if (L < R) fprintf(stderr, "-1\n"); else if (L > R) fprintf(stderr, "+1\n"); }
				if (L < R)
					return -1; // `lhs` < `rhs`
				if (L > R)
					return +1; // `lhs` < `rhs`

				// continue with next stack entry
				continue;
			}

			/*
			 * Been here before
			 */
			if (this->compVersionL[L] == thisVersionL && treeR->compVersionR[R] == thisVersionR && this->compBeenWhatL[L] == R && treeR->compBeenWhatR[R] == L)
				continue; // yes

			this->compVersionL[L]   = thisVersionL;
			treeR->compVersionR[R]  = thisVersionR;
			this->compBeenWhatL[L]  = R;
			treeR->compBeenWhatR[R] = L;

			// decode L and R
			pNodeL = this->N + L;
			pNodeR = treeR->N + R;

			/*
			 * Reminder:
			 *  [ 2] a ? ~0 : b                  "+" OR
			 *  [ 6] a ? ~b : 0                  ">" GT
			 *  [ 8] a ? ~b : b                  "^" XOR
			 *  [ 9] a ? ~b : c                  "!" QnTF
			 *  [16] a ?  b : 0                  "&" AND
			 *  [19] a ?  b : c                  "?" QTF
			 */

			/*
			 * compare structure
			 */

			// compare Ti
			if ((pNodeL->T & IBIT) && !(pNodeR->T & IBIT) && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1b\n");
			if ((pNodeL->T & IBIT) && !(pNodeR->T & IBIT))
				return -1; // `QnTF` < `QTF`
			if (!(pNodeL->T & IBIT) && (pNodeR->T & IBIT) && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1b\n");
			if (!(pNodeL->T & IBIT) && (pNodeR->T & IBIT))
				return +1; // `QTF` > `QnTF`

			// compare OR
			if (pNodeL->T == IBIT && pNodeR->T != IBIT && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1c\n");
			if (pNodeL->T == IBIT && pNodeR->T != IBIT)
				return -1; // `OR` < !`OR`
			if (pNodeL->T != IBIT && pNodeR->T == IBIT && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1c\n");
			if (pNodeL->T != IBIT && pNodeR->T == IBIT)
				return +1; // !`OR` > `OR`

			// compare GT
			if (pNodeL->F == 0 && pNodeR->F != 0 && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1d\n");
			if (pNodeL->F == 0 && pNodeR->F != 0)
				return -1; // `GT` < !`GT` or `AND` < !`AND`
			if (pNodeL->F != 0 && pNodeR->F == 0 && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1d\n");
			if (pNodeL->F != 0 && pNodeR->F == 0)
				return +1; // !`GT` > `GT` or !`AND` > `AND`

			// compare NE
			if ((pNodeL->T ^ IBIT) == pNodeL->F && (pNodeR->T ^ IBIT) != pNodeR->F && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "-1e\n");
			if ((pNodeL->T ^ IBIT) == pNodeL->F && (pNodeR->T ^ IBIT) != pNodeR->F)
				return -1; // `NE` < !`NE`
			if ((pNodeL->T ^ IBIT) != pNodeL->F && (pNodeR->T ^ IBIT) == pNodeR->F && ENABLE_DEBUG_COMPARE && (ctx.opt_debug & ctx.DEBUGMASK_COMPARE)) fprintf(stderr, "+1e\n");
			if ((pNodeL->T ^ IBIT) != pNodeL->F && (pNodeR->T ^ IBIT) == pNodeR->F)
				return +1; // !`NE` > `NE`

			/*
			 * what is current cascade
			 */
			unsigned thisCascade = CASCADE_NONE;

			if (pNodeL->T & IBIT) {
				if (pNodeL->T == IBIT)
					thisCascade = CASCADE_OR; // OR
				else if ((pNodeL->T ^ IBIT) == pNodeL->F)
					thisCascade = CASCADE_NE; // NE
			} else if (pNodeL->F == 0) {
				thisCascade = CASCADE_AND; // AND
			}

			/*
			 * @date 2021-08-28 19:18:04
			 * Push a sync when starting a new cascade to detect an exausted right-hand-side cascade
			 */
			if (thisCascade != parentCascadeL && thisCascade != CASCADE_NONE) {
				this->stackL[numStackL++]  = CASCADE_SYNC;
				this->stackL[numStackL++]  = 0;
				treeR->stackR[numStackR++] = CASCADE_SYNC;
				treeR->stackR[numStackR++] = 0;
			}

			/*
			 * Push Q/T/F components for deeper processing
			 * Test if result is cached
			 */
			if (pNodeL->F != 0 && (pNodeL->T ^ IBIT) != pNodeL->F) {
				L = pNodeL->F;
				R = pNodeR->F;
				if (this->compVersionL[L] != thisVersionL || treeR->compVersionR[R] != thisVersionR || this->compBeenWhatL[L] != R || treeR->compBeenWhatR[R] != L) {
					this->stackL[numStackL++]  = thisCascade;
					this->stackL[numStackL++]  = L;
					treeR->stackR[numStackR++] = thisCascade;
					treeR->stackR[numStackR++] = R;
				}
			}

			if ((pNodeL->T & ~IBIT) != 0) {
				L = pNodeL->T & ~IBIT;
				R = pNodeR->T & ~IBIT;
				if (this->compVersionL[L] != thisVersionL || treeR->compVersionR[R] != thisVersionR || this->compBeenWhatL[L] != R || treeR->compBeenWhatR[R] != L) {
					this->stackL[numStackL++]  = thisCascade;
					this->stackL[numStackL++]  = L;
					treeR->stackR[numStackR++] = thisCascade;
					treeR->stackR[numStackR++] = R;
				}
			}

			{
				L = pNodeL->Q;
				R = pNodeR->Q;
				if (this->compVersionL[L] != thisVersionL || treeR->compVersionR[R] != thisVersionR || this->compBeenWhatL[L] != R || treeR->compBeenWhatR[R] != L) {
					this->stackL[numStackL++]  = thisCascade;
					this->stackL[numStackL++]  = L;
					treeR->stackR[numStackR++] = thisCascade;
					treeR->stackR[numStackR++] = R;
				}
			}

		} while (numStackL > 0 && numStackR > 0);

		/*
		 * test if exhausted
		 */
		if (numStackL < numStackR)
			return -1;
		if (numStackL > numStackR)
			return +1;

		return 0;
	}

	/*
	 * @date 2021-05-12 18:18:58
	 *
	 * Difference between these tests and the `baseNode_t` versions, is that these also test is arguments are keys
	 */

	// OR (L?~0:R)
	inline bool __attribute__((pure)) isOR(uint32_t i) const {
		return i >= nstart && N[i].isOR();
	}

	// GT (L?~R:0)
	inline bool __attribute__((pure)) isGT(uint32_t i) const {
		return i >= nstart && N[i].isGT();
	}

	// NE (L?~R:R)
	inline bool __attribute__((pure)) isNE(uint32_t i) const {
		return i >= nstart && N[i].isNE();
	}

	// AND (L?R:0)
	inline bool __attribute__((pure)) isAND(uint32_t i) const {
		return i >= nstart && N[i].isAND();
	}

	// OR (L?~0:R)
	inline bool __attribute__((const)) isOR(uint32_t Q, uint32_t T, uint32_t F) const {
		return T == IBIT;
	}

	// GT (L?~R:0)
	inline bool __attribute__((const)) isGT(uint32_t Q, uint32_t T, uint32_t F) const {
		return (T & IBIT) && F == 0;
	}

	// NE (L?~R:R)
	inline bool __attribute__((const)) isNE(uint32_t Q, uint32_t T, uint32_t F) const {
		return (T ^ IBIT) == F;
	}

	// AND (L?R:0)
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
	 * lookup/create a restriction-free, unmodified node
	 */
	inline uint32_t addNode(uint32_t Q, uint32_t T, uint32_t F) {

		/*
		 *  [ 2] a ? !0 : b                  "+" or
		 *  [ 6] a ? !b : 0                  ">" greater-than
		 *  [ 8] a ? !b : b                  "^" not-equal
		 *  [ 9] a ? !b : c                  "!" QnTF
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

			assert(!(Q & IBIT));          // Q not inverted
			assert((T & IBIT) || !(this->flags & ctx.MAGICMASK_PURE));
			assert(!(F & IBIT));          // F not inverted
			assert(Q != 0);               // Q not zero
			assert(T != 0);               // Q?0:F -> F?!Q:0
			assert(F != 0 || T != IBIT);  // Q?!0:0 -> Q
			assert(Q != (T & ~IBIT));     // Q/T fold
			assert(Q != F);               // Q/F fold
			assert(T != F);               // T/F fold

			assert((T & ~IBIT) != F || this->compare(Q, this, F) < 0);     // NE ordering
			assert(F != 0 || (T & IBIT) || this->compare(Q, this, T) < 0); // AND ordering
			assert(T != IBIT || this->compare(Q, this, F) < 0);            // OR ordering

			// OR ordering and basic chain
			if (T == IBIT)
				assert(this->compare(Q, this, F) < 0);
			// NE ordering
			if ((T & ~IBIT) == F)
				assert(this->compare(Q, this, F) < 0);
			// AND ordering
			if (F == 0 && !(T & IBIT))
				assert(this->compare(Q, this, T) < 0);

			if (this->flags & ctx.MAGICMASK_CASCADE) {
				if (T == IBIT)
					assert (!this->isOR(Q) || !this->isOR(F));
				if ((T & ~IBIT) == F)
					assert (!this->isNE(Q) || !this->isNE(F));
				if (F == 0 && !(T & IBIT))
					assert (!this->isAND(Q) || !this->isAND(T & ~IBIT));
			}
		}

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
						if (top) { assert(this->compare(F, this, top) < 0); }
						top   = F;
						iNode = Q;
					} else if (this->isOR(F)) {
						if (top) { assert(this->compare(Q, this, top) < 0); }
						top   = Q;
						iNode = F;
					} else {
						if (top) { assert(this->compare(F, this, top) < 0); }
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
						if (top) { assert(this->compare(F, this, top) < 0); }
						top   = F;
						iNode = Q;
					} else if (this->isNE(F)) {
						if (top) { assert(this->compare(Q, this, top) < 0); }
						top   = Q;
						iNode = F;
					} else {
						if (top) { assert(this->compare(F, this, top) < 0); }
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
						if (top) { assert(this->compare(Tu, this, top) < 0); }
						top   = Tu;
						iNode = Q;
					} else if (this->isAND(Tu)) {
						if (top) { assert(this->compare(Q, this, top) < 0); }
						top   = Q;
						iNode = Tu;
					} else {
						if (top) { assert(this->compare(Tu, this, top) < 0); }
						break;
					}
				}

			}
		}

		return this->nodeIndex[ix];
	}

	/**
	 * @date 2021-08-13 13:33:13
	 *
	 * Add a node to tree
	 *
	 * If the node already exists then use that.
	 * Otherwise, add a node to tree if it has the expected node ID.
	 * Otherwise, Something changed since the recursion was invoked, re-analyse
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t addBasicNode(uint32_t Q, uint32_t T, uint32_t F, unsigned *pFailCount, unsigned depth) {
		if (pFailCount == NULL) {
			assert(!(Q & IBIT));                   // Q not inverted
			assert((T & IBIT) || !(this->flags & context_t::MAGICMASK_PURE));
			assert(!(F & IBIT));                   // F not inverted
			assert(Q != 0);                        // Q not zero
			assert(T != 0);                        // Q?0:F -> F?!Q:0
			assert(T != IBIT || F != 0);           // Q?!0:0 -> Q
			assert(Q != (T & ~IBIT));              // Q/T fold
			assert(Q != F);                        // Q/F fold
			assert(T != F);                        // T/F fold

			if (this->isOR(Q, T, F)) {
				if (this->flags & ctx.MAGICMASK_CASCADE) {
					assert(!this->isOR(F));
					assert(compare(Q,this, F, CASCADE_OR) < 0);
				} else {
					assert(compare(Q, this, F, CASCADE_NONE) < 0);
				}
			}
			if (this->isNE(Q, T, F)) {
				if (this->flags & ctx.MAGICMASK_CASCADE) {
					assert(!this->isNE(F));
					assert(compare(Q,this, F, CASCADE_NE) < 0);
				} else {
					assert(compare(Q, this, F, CASCADE_NONE) < 0);
				}
			}
			if (this->isAND(Q, T, F)) {
				if (this->flags & ctx.MAGICMASK_CASCADE) {
					assert(!this->isAND(T));
					assert(compare(Q,this, T, CASCADE_AND) < 0);
				} else {
					assert(compare(Q, this, T, CASCADE_NONE) < 0);
				}
			}
		}

		// lookup
		uint32_t ix = this->lookupNode(Q, T, F);
		if (this->nodeIndex[ix] != 0) {
			// node already exists
			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"old\":{\"qtf\":[%u,%s%u,%u],N:%u}", Q, T & IBIT ? "~" : "", T & ~IBIT, F, this->nodeIndex[ix]);
			return this->nodeIndex[ix];
		} else if (pFailCount != NULL) {
			/*
			 * Simulate the creation of a new node.
			 * The returned node id must be unique and must not be an end condition `ncount`.
			 */
			uint32_t nid = this->ncount + (*pFailCount)++;
			assert(nid < this->maxNodes);
			// set temporary node but do not add to cache
			this->N[nid].Q = Q;
			this->N[nid].T = T;
			this->N[nid].F = F;
			return nid;
		} else {
			// situation is stable, create node
			uint32_t ret = this->addNode(Q, T, F);
			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"new\":{\"qtf\":[%u,%s%u,%u],N:%u}", Q, T & IBIT ? "~" : "", T & ~IBIT, F, ret);
			return ret;
		}
	}

	/**
	 * @date 2021-08-13 13:44:17
	 *
	 * Apply communicative dyadics ordering on a low level.
	 * Cascades are left-hand-side only
	 *   left+right made code highly complex
	 *   right-hand-side was open-ranged and triggering recursion
	 * with LHS, all the cascaded left hand terms are less than the right hand term
	 * drawback, reduced detector range.
	 *
	 * Important NOTE (only relevant for right-hand-side cascading):
	 * The structure "dcab^^^" will cause oscillations.
	 * Say that this is the top of a longer cascading chain, then `b` is also a "^".
	 * Within the current detect span ("dcab^^^"), it is likely that `b` and `d` will swap positions.
	 * The expanded resulting structure will look like "xy^cad^^^", who's head is "xy^cz^^". (`b`="xy^",`z`="ad^")
	 * This new head would trigger a rewrite to "zcxy^^^" making the cycle complete.
	 *
	 * @date 2021-08-19 11:08:05
	 * All structures below top-level are ordered
	 *
	 * @date 2021-08-23 23:21:35
	 * cascades make it complex because endpoints can be placeholders for deeper cascading.
	 * In an attempt to simplify logic, let only the left hand side continue cascading, and the lowest (oldest) values located deeper.
	 * Example `ab+c+d+`
	 * This also makes the names more naturally readable.
	 * This also means that for `ab+c+', if `b` continues the cascade, the terms will all be "less than" `c`.
	 * Would the right hand side continue, then there is no indicator of highest value in th deeper levels.
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	bool cascadeQTF(uint32_t *Q, uint32_t *T, uint32_t *F, unsigned *pFailCount, unsigned depth) {

		// OR (L?~0:R)
		if (this->isOR(*Q, *T, *F)) {
			if (this->isOR(*Q) && this->isOR(*F)) {
				// AB+CD++
				uint32_t AB = *Q; // may cascade
				uint32_t CD = *F; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // may cascade
				uint32_t C  = this->N[CD].Q; // does not cascade
				uint32_t D  = this->N[CD].F; // does not cascade

				if (A == CD) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=CD\",\"N\":%u}}", A, B, C, D, AB);
					*Q = *T = *F = AB;
					return true;
				} else if (B == CD) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"B=CD\",\"N\":%u}}", A, B, C, D, AB);
					/*
					 * @date 2021-09-16 15:57:46
					 * CD cascades which implies that B also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = AB;
					return true;
				} else if (C == AB) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C=AB\",\"N\":%u}}", A, B, C, D, CD);
					*Q = *T = *F = CD;
					return true;
				} else if (D == AB) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"D=AB\",\"N\":%u}}", A, B, C, D, CD);
					/*
					 * @date 2021-09-16 15:57:46
					 * AB cascades which implies that D also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = CD;
					return true;
				}

				if (A == C) {
					if (B == D) {
						// A=C<B=D
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=C<B=D\",\"N\":%u}}", A, B, C, D, *Q);
						/*
						 * @date 2021-09-16 00:21:52
						 * This implies that `Q==F` which has already been tested
						 */
						assert(0);
						*Q = *T = *F = AB;
					} else if (compare(B, this, D, CASCADE_OR) < 0) {
						// C=A<B<D
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C=A<B<D\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
						// already *Q = AB;
						// already *T = IBIT;
						*F = D;
					} else {
						// A=C<D<B
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=C<D<B\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
						*Q = CD;
						// already *T = IBIT;
						*F = B;
					}
					return true;
				} else if (A == D) {
					// C<D=A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C<D=A<B\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = CD;
					// already *T = IBIT;
					*F = B;
					return true;
				} else if (B == C) {
					// A<B=C<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<B=C<D\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					// already *Q = AB;
					// already *T = IBIT;
					*F = D;
					return true;
				} else if (B == D) {
					// A<C<D=B or C<A<B=D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<D=B\"", A, B, C, D);
					// A and C can cascade, neither will exceed B/D
					if (this->isOR(A) || this->isOR(C)) {
						// cascades, merge AC, B last
						*Q = A;
						// already *T = IBIT;
						*F = C;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac+\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = IBIT;
							*F = B;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac+b+\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							// already *T = IBIT;
							*F = B;
						}
					} else if (compare(A, this, C, CASCADE_OR) < 0) {
						// A<C<B
						*Q = addNormaliseNode(A, IBIT, C, pFailCount, depth);
						// already *T = IBIT;
						*F = B;
					} else {
						// C<A<B
						*Q = addNormaliseNode(C, IBIT, A, pFailCount, depth);
						// already *T = IBIT;
						*F = B;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				}

				/*
				 * 4! permutations where A<B and C<D has 6 candidates
				 */
				if (compare(B, this, C, CASCADE_OR) < 0) {
					// A<B<C<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<B<C<D\"", A, B, C, D);
					if (this->isOR(C)) {
						// C cascades and unusable for right-hand-side
						// merge ABC, D last
						*Q = AB;
						// already *T = IBIT;
						*F = C;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab+c+\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, D no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = IBIT;
							*F = D;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab+c+d+\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							// already *T = IBIT;
							*F = D;
						}
					} else {
						// simple rewrite
						*Q = addNormaliseNode(AB, IBIT, C, pFailCount, depth);
						// already *T = IBIT;
						*F = D;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(D, this, A, CASCADE_OR) < 0) {
					// C<D<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C<D<A<B\"", A, B, C, D);
					if (this->isOR(A)) {
						// A cascades and unusable for right-hand-side
						// merge CDA, B last
						*Q = CD;
						// already *T = IBIT;
						*F = A;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"cd+a+\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = IBIT;
							*F = B;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"cd+a+b+\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							// already *T = IBIT;
							*F = B;
						}
					} else {
						// simple rewrite
						*Q = addNormaliseNode(CD, IBIT, A, pFailCount, depth);
						// already *T = IBIT;
						*F = B;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(B, this, D, CASCADE_OR) < 0) {
					// A<C<B<D or C<A<B<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<B<D\"", A, B, C, D);
					// merge ABC, D last
					// already *Q = AB;
					// already *T = IBIT;
					*F = C;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab+c+\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, D no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = IBIT;
						*F = D;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab+c+d+\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						// already *T = IBIT;
						*F = D;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else {
					// A<C<D<B or C<A<D<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<D<B\"", A, B, C, D);
					// merge ACD, B last
					*Q = A;
					// already *T = IBIT;
					// already *F = CD;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"acd++\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = IBIT;
						*F = B;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"acd++b+\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						// already *T = IBIT;
						*F = B;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				}

			} else if (this->isOR(*Q)) {
				// AB+C+
				uint32_t AB = *Q; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // does not cascade
				uint32_t C  = *F; // may cascade

				assert(!this->isOR(B));
				assert(!this->isOR(C));

				if (C == A) {
					// C=A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"C=A<B\",\"N\":%u}}", A, B, C, AB);
					*Q = *T = *F = AB;
					return true;
				} else if (C == B) {
					// A<B=C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"A<B=C\",\"N\":%u}}", A, B, C, AB);
					*Q = *T = *F = AB;
					return true;
				}

				/*
				 * 3! permutations where A<B has 3 candidates
				 */
				if (compare(B, this, *F, CASCADE_OR) < 0) {
					// A<B<C
					// natural order
					// already *Q = AB;
					// already *T = IBIT;
					// already *F = C;
					return false;
				} else if (this->isOR(A)) {
					// A<C<B or C<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"A<C<B\"", A, B, C);
					// cascade, merge AC, B last
					*Q = A;
					// already *T = IBIT;
					// already *F = C;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac+\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = IBIT;
						*F = B;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac+b+\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						// already *T = IBIT;
						*F = B;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(A, this, C, CASCADE_OR) < 0) {
					// A<C<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"A<C<B\"}", A, B, C);
					*Q = addNormaliseNode(A, IBIT, C, pFailCount, depth);
					// already *T = IBIT;
					*F = B;
					return true;
				} else {
					// C<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"C<A<B\"}", A, B, C);
					*Q = addNormaliseNode(C, IBIT, A, pFailCount, depth);
					// already *T = IBIT;
					*F = B;
					return true;
				}

			} else if (this->isOR(*F)) {
				// ABC++
				uint32_t BC = *F; // may cascade
				uint32_t A  = *Q; // does not cascade
				uint32_t B  = this->N[BC].Q; // may cascade
				uint32_t C  = this->N[BC].F; // does not cascade

				assert (!this->isOR(A));
				assert (!this->isOR(C));

				if (A == B) {
					// A=B<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"A=B<C\",\"N\":%u}}", A, B, C, BC);
					*Q = *T = *F = BC;
					return true;
				} else if (A == C) {
					// B<C=A
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"B<C=A\",\"N\":%u}}", A, B, C, BC);
					*Q = *T = *F = BC;
					return true;
				}

				/*
				 * 3! permutations where B<C has 3 candidates
				 */
				if (compare(C, this, A, CASCADE_OR) < 0) {
					// B<C<A
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"B<C<A\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = BC;
					// already *T = IBIT;
					*F = A;
					return true;
				} else if (this->isOR(B)) {
					// A<Q<B or Q<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"A<B<C\"", A, B, C);
					// cascade, merge AB, C last
					// already *Q = A;
					// already *T = IBIT;
					*F = B;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab+\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, C no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = IBIT;
						*F = C;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab+c+\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						// already *T = IBIT;
						*F = C;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(A, this, B, CASCADE_OR) < 0) {
					// A<B<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"A<B<C\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = addNormaliseNode(A, IBIT, B, pFailCount, depth);
					// already *T = IBIT;
					*F = C;
					return true;
				} else {
					// B<A<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u,%u],\"order\":\"B<A<C\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = addNormaliseNode(B, IBIT, A, pFailCount, depth);
					// already *T = IBIT;
					*F = C;
					return true;
				}

			} else if (compare(*F, this, *Q, CASCADE_OR) < 0) {
				// swap
				if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"or\":{\"slot\":[%u,%u],\"order\":\"F<Q\",\"qtf\":[%u,%s%u,%u]}", *Q, *F, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
				uint32_t savQ = *Q;
				*Q = *F;
				// already *T = IBIT;
				*F = savQ;
				return true;
			} else {
				// no change
				return false;
			}
		}

		// NE (L?~R:R)
		if (this->isNE(*Q, *T, *F)) {
			if (this->isNE(*Q) && this->isNE(*F)) {
				// AB^CD^^
				uint32_t AB = *Q; // may cascade
				uint32_t CD = *F; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // may cascade
				uint32_t C  = this->N[CD].Q; // does not cascade
				uint32_t D  = this->N[CD].F; // does not cascade

				if (A == CD) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=CD\",\"N\":%u}}", A, B, C, D, B);
					*Q = *T = *F = B;
					return true;
				} else if (B == CD) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"B=CD\",\"N\":%u}}", A, B, C, D, A);
					/*
					 * @date 2021-09-16 15:57:46
					 * CD cascades which implies that B also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = A;
					return true;
				} else if (C == AB) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C=AB\",\"N\":%u}}", A, B, C, D, D);
					*Q = *T = *F = D;
					return true;
				} else if (D == AB) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"D=AB\",\"N\":%u}}", A, B, C, D, C);
					/*
					 * @date 2021-09-16 15:57:46
					 * AB cascades which implies that D also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = C;
					return true;
				}

				if (A == C) {
					if (B == D) {
						// A=C<B=D
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=C<B=D\",\"N\":%u}}", A, B, C, D, 0);
						/*
						 * @date 2021-09-16 00:21:52
						 * This implies that `Q==F` which has already been tested
						 */
						assert(0);
						*Q = *T = *F = 0;
					} else if (compare(B, this, D, CASCADE_NE) < 0) {
						// C=A<B<D
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C=A<B<D\",\"N\":%u}}", A, B, C, D, 0);
						*Q = B;
						*T = D ^ IBIT;
						*F = D;
					} else {
						// A=C<D<B
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=C<D<B\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
						*Q = D;
						*T = B ^ IBIT;
						*F = B;
					}
					return true;
				} else if (A == D) {
					// C<D=A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C<D=A<B\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = C;
					*T = B ^ IBIT;
					*F = B;
					return true;
				} else if (B == C) {
					// A<B=C<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<B=C<D\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = A;
					*T = D ^ IBIT;
					*F = D;
					return true;
				} else if (B == D) {
					// A<C<D=B or C<A<B=D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<D=B\"", A, B, C, D);
					// A and C can cascade, neither will exceed B/D
					if (this->isNE(A) || this->isNE(C)) {
						*Q = A;
						*T = C ^ IBIT;
						*F = C;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"cd^a^b^\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else if (compare(A, this, C, CASCADE_NE) < 0) {
						// A<C
						*Q = A;
						*T = C ^ IBIT;
						*F = C;
					} else {
						// C<A
						*Q = C;
						*T = A ^ IBIT;
						*F = A;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				}

				/*
				 * 4! permutations where A<B and C<D has 6 candidates
				 */
				if (compare(B, this, C, CASCADE_NE) < 0) {
					// A<B<C<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<B<C<D\"", A, B, C, D);
					if (this->isNE(C)) {
						// C cascades and unusable for right-hand-side
						// merge ABC, D last
						*Q = AB;
						*T = C ^ IBIT;
						*F = C;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab^c^\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, D no longer assumed highest
							// append last placeholder
							// todo: found testdata 'abcdefgh^^^^^^^ijklmnopq^^^^^^^^^'
							// assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = D ^ IBIT;
							*F = D;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab^c^d^\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = D ^ IBIT;
							*F = D;
						}
					} else {
						// simple rewrite
						*Q = addNormaliseNode(AB, C ^ IBIT, C, pFailCount, depth);
						*T = D ^ IBIT;
						*F = D;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(D, this, A, CASCADE_NE) < 0) {
					// C<D<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C<D<A<B\"", A, B, C, D);
					if (this->isNE(A)) {
						// A cascades and unusable for right-hand-side
						// merge CDA, B last
						*Q = CD;
						*T = A ^ IBIT;
						*F = A;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"cd^a^\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							// assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet @date 2021-10-11 found it deep within "kfold des.dat". 
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = B ^ IBIT;
							*F = B;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"cd^a^b^\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = B ^ IBIT;
							*F = B;
						}
					} else {
						// simple rewrite
						*Q = addNormaliseNode(CD, A ^ IBIT, A, pFailCount, depth);
						*T = B ^ IBIT;
						*F = B;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(B, this, D, CASCADE_NE) < 0) {
					// A<C<B<D or C<A<B<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<B<D\"", A, B, C, D);
					// A and C can react and exceed B, D is definitely last
					// merge ABC, D last
					// already *Q = AB;
					*T = C ^ IBIT;
					*F = C;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab^c^\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, D no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = D ^ IBIT;
						*F = D;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab^c^d^\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = D ^ IBIT;
						*F = D;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else {
					// A<C<D<B or C<A<D<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<D<B\"", A, B, C, D);
					// merge ACD, B last
					*Q = A;
					// already *T = CD ^ IBIT;
					// already *F = CD;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"acd^^\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B ^ IBIT;
						*F = B;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"acd^^b^\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B ^ IBIT;
						*F = B;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				}

			} else if (this->isNE(*Q)) {
				// AB^C^
				uint32_t AB = *Q; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].F; // does not cascade
				uint32_t C  = *F; // may cascade

				assert(!this->isNE(B));
				assert(!this->isNE(C));

				if (C == A) {
					// C=A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"C=A<B\",\"N\":%u}}", A, B, C, B);
					*Q = *T = *F = B;
					return true;
				} else if (C == B) {
					// A<B=C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"A<B=C\",\"N\":%u}}", A, B, C, A);
					*Q = *T = *F = A;
					return true;
				}

				/*
				 * 3! permutations where A<B has 3 candidates
				 */
				if (compare(B, this, *F, CASCADE_NE) < 0) {
					// A<B<C
					// natural order
					// already *Q = AB;
					// already *T = C ^ IBIT;
					// already *F = C;
					return false;
				} else if (this->isNE(A)) {
					// A<C<B or C<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"A<C<B\"", A, B, C);
					// cascade, merge AC, B last
					*Q = A;
					// already *T = C ^ IBIT;
					// already *F = C;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac^\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B ^ IBIT;
						*F = B;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac^b^\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B ^ IBIT;
						*F = B;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(A, this, C, CASCADE_NE) < 0) {
					// A<C<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"A<C<B\"", A, B, C);
					*Q = addNormaliseNode(A, C ^ IBIT, C, pFailCount, depth);
					*T = B ^ IBIT;
					*F = B;
					return true;
				} else {
					// C<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"C<A<B\"", A, B, C);
					*Q = addNormaliseNode(C, A ^ IBIT, A, pFailCount, depth);
					*T = B ^ IBIT;
					*F = B;
					return true;
				}

			} else if (this->isNE(*F)) {
				// ABC^^
				uint32_t BC = *F; // may cascade
				uint32_t A  = *Q; // does not cascade
				uint32_t B  = this->N[BC].Q; // may cascade
				uint32_t C  = this->N[BC].F; // does not cascade

				assert (!this->isNE(A));
				assert (!this->isNE(C));

				if (A == B) {
					// A=B<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"A=B<C\",\"N\":%u}}", A, B, C, C);
					*Q = *T = *F = C;
					return true;
				} else if (A == C) {
					// B<C=A
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"B<C=A\",\"N\":%u}}", A, B, C, B);
					*Q = *T = *F = B;
					return true;
				}

				/*
				 * 3! permutations where B<C has 3 candidates
				 */
				if (compare(C, this, A, CASCADE_NE) < 0) {
					// B<C<A
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"B<C<A\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = BC;
					*T = A ^ IBIT;
					*F = A;
					return true;
				} else if (this->isNE(B)) {
					// A<Q<B or Q<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"A<B<C\"", A, B, C);
					// cascade, merge AB, C last
					// already *Q = A;
					*T = B ^ IBIT;
					*F = B;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab^\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, C no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = C ^ IBIT;
						*F = C;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab^c^\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = C ^ IBIT;
						*F = C;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(A, this, B, CASCADE_NE) < 0) {
					// A<B<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"A<B<C\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = addNormaliseNode(A, B ^ IBIT, B, pFailCount, depth);
					*T = C ^ IBIT;
					*F = C;
					return true;
				} else {
					// B<A<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u,%u],\"order\":\"B<A<C\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = addNormaliseNode(B, A ^ IBIT, A, pFailCount, depth);
					*T = C ^ IBIT;
					*F = C;
					return true;
				}

			} else if (compare(*F, this, *Q, CASCADE_NE) < 0) {
				// swap
				if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"ne\":{\"slot\":[%u,%u],\"order\":\"F<Q\",\"qtf\":[%u,%s%u,%u]}", *Q, *F, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
				uint32_t savQ = *Q;
				*Q = *F;
				*T = savQ ^ IBIT;
				*F = savQ;
				return true;
			} else {
				// no change
				return false;
			}
		}

		// AND (L?T:0)
		if (this->isAND(*Q, *T, *F)) {
			if (this->isAND(*Q) && this->isAND(*T)) {
				// AB&CD&&
				uint32_t AB = *Q; // may cascade
				uint32_t CD = *T; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].T; // may cascade
				uint32_t C  = this->N[CD].Q; // does not cascade
				uint32_t D  = this->N[CD].T; // does not cascade

				if (A == CD) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=CD\",\"N\":%u}}", A, B, C, D, AB);
					*Q = *T = *F = AB;
					return true;
				} else if (B == CD) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"B=CD\",\"N\":%u}}", A, B, C, D, AB);
					/*
					 * @date 2021-09-16 15:57:46
					 * CD cascades which implies that B also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = AB;
					return true;
				} else if (C == AB) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C=AB\",\"N\":%u}}", A, B, C, D, CD);
					*Q = *T = *F = CD;
					return true;
				} else if (D == AB) {
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"D=AB\",\"N\":%u}}", A, B, C, D, CD);
					/*
					 * @date 2021-09-16 15:57:46
					 * AB cascades which implies that D also cascades, which is not allowed
					 */
					assert(0);
					*Q = *T = *F = CD;
					return true;
				}

				if (A == C) {
					if (B == D) {
						// A=C<B=D
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=C<B=D\",\"N\":%u}}", A, B, C, D, *Q);
						/*
						 * @date 2021-09-16 00:21:52
						 * This implies that `Q==F` which has already been tested
						 */
						assert(0);
						*Q = *T = *F = AB;
					} else if (compare(B, this, D, CASCADE_AND) < 0) {
						// C=A<B<D
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C=A<B<D\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
						// already *Q = AB;
						*T = D;
						// already *F = 0;
					} else {
						// A=C<D<B
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A=C<D<B\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
						*Q = CD;
						*T = B;
						// already *F = 0;
					}
					return true;
				} else if (A == D) {
					// C<D=A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C<A=D<B\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*T = B;
					// already *F = 0;
					return true;
				} else if (B == C) {
					// A<B=C<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<B=C<D\",\"qtf\":[%u,%s%u,%u]}", A, B, C, D, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = AB;
					*T = D;
					//already *F = 0;
					return true;
				} else if (B == D) {
					// A<C<D=B or C<A<B=D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<D=B\"", A, B, C, D);
					// A and C can cascade, neither will exceed B/D
					if (this->isAND(A) || this->isAND(C)) {
						// cascades, merge AC, B last
						*Q = A;
						*T = C;
						// already *F = 0;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac&\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = B;
							*F = 0;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac&b&\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = B;
							// already *F = 0;
						}
					} else if (compare(A, this, C, CASCADE_AND) < 0) {
						// A<C<B
						*Q = addNormaliseNode(A, C, 0, pFailCount, depth);
						*T = B;
						// already *F = 0;
					} else {
						// C<A<B
						*Q = addNormaliseNode(C, A, 0, pFailCount, depth);
						*T = B;
						// already *F = 0;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				}

				/*
				 * 4! permutations where A<B and C<D has 6 candidates
				 */
				if (compare(B, this, C, CASCADE_AND) < 0) {
					// A<B<C<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<B<C<D\"", A, B, C, D);
					if (this->isAND(C)) {
						// C cascades and unusable for right-hand-side
						// merge ABC, D last
						*Q = AB;
						*T = C;
						// already *F = 0;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab&c&\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, D no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = D;
							*F = 0;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab&c&d&\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = D;
							// already *F = 0;
						}
					} else {
						// simple rewrite
						*Q = addNormaliseNode(AB, C, 0, pFailCount, depth);
						*T = D;
						// already *F = 0;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(D, this, A, CASCADE_AND) < 0) {
					// C<D<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"C<D<A<B\"", A, B, C, D);
					if (this->isAND(A)) {
						// A cascades and unusable for right-hand-side
						// merge CDA, B last
						*Q = CD;
						*T = A;
						// already *F = 0;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"cd&a&\":\n");
						if (cascadeQTF(Q, T, F, pFailCount, depth)) {
							// *Q,*T,*F changed or folded, B no longer assumed highest
							// append last placeholder
							assert(0); // @date 2021-09-16 16:03:55 Haven't found test data yet
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = B;
							*F = 0;
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"cd&a&b&\":\n");
							cascadeQTF(Q, T, F, pFailCount, depth);
						} else {
							// append last placeholder
							*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
							*T = B;
							// already *F = 0;
						}
					} else {
						// simple rewrite
						*Q = addNormaliseNode(CD, A, 0, pFailCount, depth);
						*T = B;
						// already *F = 0;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(B, this, D, CASCADE_AND) < 0) {
					// A<C<B<D or C<A<B<D
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<B<D\"", A, B, C, D);
					// merge ABC, D last
					// already *Q = AB;
					*T = C;
					// already *F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab&c&\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, D no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = D;
						*F = 0;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab&c&d&\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = D;
						// already *F = 0;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else {
					// A<C<D<B or C<A<D<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u,%u],\"order\":\"A<C<D<B\"", A, B, C, D);
					// merge ACD, B last
					*Q = A;
					// already *T = CD;
					// already *F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"acd&&\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B;
						*F = 0;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"acd&&b&\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B;
						// already *F = 0;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				}

			} else if (this->isAND(*Q)) {
				// AB&C&
				uint32_t AB = *Q; // may cascade
				uint32_t A  = this->N[AB].Q; // may cascade
				uint32_t B  = this->N[AB].T; // does not cascade
				uint32_t C  = *T; // does not cascade

				assert(!this->isAND(B));
				assert(!this->isAND(C));

				if (C == A) {
					// C=A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"C=A<B\",\"N\":%u}}", A, B, C, AB);
					*Q = *T = *F = AB;
					return true;
				} else if (C == B) {
					// A<B=C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"A<B=C\",\"N\":%u}}", A, B, C, AB);
					*Q = *T = *F = AB;
					return true;
				}

				/*
				 * 3! permutations where A<B has 3 candidates
				 */
				if (compare(B, this, C, CASCADE_AND) < 0) {
					// A<B<C
					// natural order
					// already *Q = AB;
					// already *T = C;
					// already *F = 0;
					return false;
				} else if (this->isAND(A)) {
					// A<C<B or C<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"A<C<B\"", A, B, C);
					// cascade, merge AC, B last
					*Q = A;
					// already *T = C;
					// already *F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac&\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, B no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B;
						*F = 0;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ac&b&\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = B;
						// already *F = 0;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(A, this, C, CASCADE_AND) < 0) {
					// A<C<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"A<C<B\",\"af&\":", A, B, C);
					*Q = addNormaliseNode(A, C, 0, pFailCount, depth);
					*T = B;
					// already *F = 0;
					return true;
				} else {
					// C<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"T<A<B\",\"fa&\":", A, B, C);
					*Q = addNormaliseNode(C, A, 0, pFailCount, depth);
					*T = B;
					// already *F = 0;
					return true;
				}

			} else if (this->isAND(*T)) {
				// ABC&&
				uint32_t BC = *T; // may cascade
				uint32_t A  = *Q; // does not cascade
				uint32_t B  = this->N[BC].Q; // may cascade
				uint32_t C  = this->N[BC].T; // does not cascade

				assert(!this->isAND(A));
				assert(!this->isAND(C));

				if (A == B) {
					// A=B<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"A=B<C\",\"N\":%u}}", A, B, C, BC);
					*Q = *T = *F = BC;
					return true;
				} else if (A == C) {
					// B<C=A
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"B<C=A\",\"N\":%u}}", A, B, C, BC);
					*Q = *T = *F = BC;
					return true;
				}

				/*
						 * 3! permutations where B<C has 3 candidates
						 */
				if (compare(C, this, A, CASCADE_AND) < 0) {
					// B<C<A
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"B<C<A\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = BC;
					*T = A;
					//already *F = 0;
					return true;
				} else if (this->isAND(B)) {
					// A<C<B or C<A<B
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"A<B<C\"", A, B, C);
					// cascade, merge AB, C last
					// already *Q = A;
					*T = B;
					// already *F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab&\":\n");
					if (cascadeQTF(Q, T, F, pFailCount, depth)) {
						// *Q,*T,*F changed or folded, C no longer assumed highest
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = C;
						*F = 0;
						if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"ab&c&\":\n");
						cascadeQTF(Q, T, F, pFailCount, depth);
					} else {
						// append last placeholder
						*Q = addNormaliseNode(*Q, *T, *F, pFailCount, depth);
						*T = C;
						// already *F = 0;
					}
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					return true;
				} else if (compare(A, this, B, CASCADE_AND) < 0) {
					// A<B<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"A<B<C\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = addNormaliseNode(A, B, 0, pFailCount, depth);
					*T = C;
					// already *F = 0;
					return true;
				} else {
					// B<A<C
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u,%u],\"order\":\"B<A<C\",\"qtf\":[%u,%s%u,%u]}", A, B, C, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
					*Q = addNormaliseNode(B, A, 0, pFailCount, depth);
					*T = C;
					// already *F = 0;
					return true;
				}

			} else if (compare(*T, this, *Q, CASCADE_AND) < 0) {
				// swap
				if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"and\":{\"slot\":[%u,%u],\"order\":\"T<Q\",\"qtf\":[%u,%s%u,%u]}", *Q, *T, *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);
				uint32_t savQ = *Q;
				*Q = *T;
				*T = savQ;
				// already *F = 0;
				return true;
			} else {
				// no change
				return false;
			}
		}

		// unchanged
		return false;
	}

	/**
	 * @date 2021-10-08 00:00:19
	 *
	 * Apply structural rewriting.
	 * Lookup with index template "abc!def!ghi!!"
	 * Multiple results are all dry-run for their score.
	 * The winner is used as construction template.
	 * If a structural rewrite or collapse occurs,
	 *   the Q/T/F arguments hold the values for the final top-level node.
	 * Rewriting can create orphans and larger trees.
	 * 
	 * WARNING: A side-effect of the dry-run is that it will break the assertion `nodeId >= ncount`. 
	 * 
	 * return:
	 * Q/T/F are the `addBasicNode()` arguments.
	 * Collapse are returned as: `Q=T=F=id`. This can be tested with `if (T == F) {}`.
	 * returns true if Q/T/F changed.
	 * 
	 * WARNING: Always use the returned Q/T/F, even when function returns false.
	 * 
	 * Implementation in `rewritetree.h`.
	 * 
	 * @param {number*} Q - writable reference to `Q`
	 * @param {number*} T - writable reference to `T`
	 * @param {number*} F - writable reference to `F`
	 * @param {unsigned*} pFailCount - `non-null`: dry-run and increment on each missing node. `null`: apply changes, 
	 * @param {number} depth - Recursion depth
	 * @return {number} `true` if a rewrite was triggered
	 */
	virtual bool rewriteQTF(uint32_t *Q, uint32_t *T, uint32_t *F, unsigned *pFailCount, unsigned depth) {
		return false;
	}

	/*
	 * @date 2021-05-12 18:08:34
	 *
	 * lookup/create and normalise any combination of Q, T and F, inverted or not.
	 * NOTE: the return value may be inverted
	 * This call is the isolation layer between the existence of inverts.
	 * The callers of this function should propagate invert to the root
	 * The workers of this function: invert no longer exists!! remove all the code and logic related.
	 * Level 3 rewrites make `lookupNode()` lose it's meaning
	 */
	uint32_t addNormaliseNode(uint32_t Q, uint32_t T, uint32_t F, unsigned *pFailCount = NULL, unsigned depth = 0) {

		depth++;
		assert(depth < 240);

		assert ((Q & ~IBIT) < this->ncount);
		assert ((T & ~IBIT) < this->ncount);
		assert ((F & ~IBIT) < this->ncount);

		/*
		 * Test for endpoint
		 */

		if (T == F)
			return F;

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
			Q ^= IBIT;
			T = F;
			F = savT;
			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level1\":\"~Q\",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
		}
		if (Q == 0) {
			// "0?T:F" -> "F"
			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level1\":\"Q=0\",\"N\":%s%u}", (F & IBIT) ? "~" : "", (F & ~IBIT));
			return F;
		}

		if (F & IBIT) {
			// "Q?T:!F" -> "!(Q?!T:F)"
			F ^= IBIT;
			T ^= IBIT;
			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level1\":\"~F\",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			return addNormaliseNode(Q, T, F, pFailCount, depth) ^ IBIT;
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
		 *  [ 9] a ? !b : c                  "!" QnTF
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
 		 * ./eval --raw 'a0a!' 'a0b!' 'aaa!' 'aab!' 'aba!' 'abb!' 'abc!' 'a0a?' 'a0b?' 'aaa?' 'aab?' 'aba?' 'abb?' 'abc?'
 		 *
		 */

		if (T & IBIT) {

			if (T == IBIT) {
				if (Q == F) {
					// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"a0a!\",\"N\":%u}", Q);
					return Q;
				} else if (F == 0) {
					// [ 0] a ? !0 : 0  ->  a
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"a00!\",\"N\":%u}", Q);
					return Q;
				} else {
					// [ 2] a ? !0 : b  -> "+" OR
				}
			} else if ((T ^ IBIT) == Q) {
				if (Q == F) {
					// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aaa!\",\"N\":%u}", 0);
					return 0;
				} else if (F == 0) {
					// [ 3] a ? !a : 0  ->  0
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aa0!\",\"N\":%u}", 0);
					return 0;
				} else {
					// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
					Q = F;
					F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aab!\",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}
			} else {
				if (Q == F) {
					// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
					F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aba!\",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else {
					// [ 6] a ? !b : 0  -> ">" greater-than
					// [ 8] a ? !b : b  -> "^" not-equal
					// [ 9] a ? !b : c  -> "!" QnTF
				}
			}

		} else {

			if (T == 0) {
				if (Q == F) {
					// [11] a ?  0 : a -> 0
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"a0a?\",\"N\":%u}", 0);
					return 0;
				} else if (F == 0) {
					// [10] a ?  0 : 0 -> 0
					assert(0); // already tested
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"a00?\",\"N\":%u}", 0);
					return 0;
				} else {
					// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
					T = Q ^ IBIT;
					Q = F;
					F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"a0b?!\",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}
			} else if (Q == T) {
				if (Q == F) {
					// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
					assert(0); // already tested
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aaa?\",\"N\":%u}", Q);
					return Q;
				} else if (F == 0) {
					// [13] a ?  a : 0 -> a
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aa0?\",\"N\":%u}", Q);
					return Q;
				} else {
					// [15] a ?  a : b -> a ? !0 : b -> "+" OR
					T = IBIT;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aab?\",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				}
			} else {
				if (Q == F) {
					// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
					F = 0;
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level2\":\"aba?\",\"qtf\":[%u,%s%u,%u]}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
				} else {
					// [16] a ?  b : 0             "&" and
					// [18] a ?  b : b -> b        ALREADY TESTED		
					// [19] a ?  b : c             "?" QTF
				}
			}
		}

		/*
		 * Lookup if QTF combo already exists before performing expensive cascading and rewrites
		 */

		uint32_t ix = this->lookupNode(Q, T, F);
		if (this->nodeIndex[ix] != 0) {
			// node already exists
			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"old\":{\"qtf\":[%u,%s%u,%u],N:%u}", Q, T & IBIT ? "~" : "", T & ~IBIT, F, this->nodeIndex[ix]);
			return this->nodeIndex[ix];
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
				// Q?T:F -> Q?!(Q?!T:F):F
				T = addNormaliseNode(Q, T ^ IBIT, F, pFailCount, depth) ^ IBIT;
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

		bool somethingChanged = false;

		/*
		 * Cascading
		 */

		if (this->flags & ctx.MAGICMASK_CASCADE) {
			somethingChanged |= this->cascadeQTF(&Q, &T, &F, pFailCount, depth);
			if (T == F)
				return F; // collapse
		} else {
			if (this->isOR(Q, T, F)) {
				if (compare(Q, this, F) > 0) {
					uint32_t savQ = Q;
					Q = F;
					T = IBIT;
					F = savQ;
				}
			}
			if (this->isNE(Q, T, F)) {
				if (compare(Q, this, F) > 0) {
					uint32_t savQ = Q;
					Q = F;
					T = savQ ^ IBIT;
					F = savQ;
				}
			}
			if (this->isAND(Q, T, F)) {
				if (compare(Q, this, T) > 0) {
					uint32_t savQ = Q;
					Q = T;
					T = savQ;
					F = 0;
				}
			}

		}

		/*
		 * Database driven rewriting
		 */

		if (this->flags & ctx.MAGICMASK_REWRITE) {
			somethingChanged |= this->rewriteQTF(&Q, &T, &F, pFailCount, depth);
			if (T == F)
				return F; // collapse
		}

		/*
		 * recurse is something changed
		 */

		if (somethingChanged)
			return addNormaliseNode(Q, T, F, pFailCount, depth);

		/*
		 * add tree
		 */

		uint32_t retId = addBasicNode(Q, T, F, NULL, 0);
		
		if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf("\n");
		return retId;
	}

	/*
	 * @date 2021-06-13 10:28:49
	 *
	 * Rewrite Q/T/F based on top-level lookup tables.
	 * If a rewrite found
	 */
	uint32_t addRewriteNode(uint32_t Q, uint32_t T, uint32_t F) {
		// test if symbols available while linking
		if (rewriteMap == NULL) {
			assert(!"MAGICMASK_REWRITE requested, include \"rewritedata.h\"");
		} else {
			uint32_t slots[16], nextSlot;
			uint32_t Tu = T & ~IBIT;
			uint32_t Ti = T & IBIT;
			uint32_t ix = rewriteDataFirst;

			if (this->flags & context_t::MAGICMASK_PARANOID) {
				assert(!(Q & IBIT));          // Q not inverted
				assert(Ti || !(this->flags & context_t::MAGICMASK_PURE));
				assert(!(F & IBIT));          // F not inverted
				assert(Q != 0);               // Q not zero

				assert (Q < this->ncount);
				assert (Tu < this->ncount);
				assert (F < this->ncount);
			}

			// versioning
			uint32_t thisVersion = ++iVersionRewrite;
			if (thisVersion == 0) {
				// version overflow, clear
				memset(rewriteVersion, 0, this->maxNodes * sizeof(*rewriteVersion));

				thisVersion = ++iVersionRewrite;
			}
			numRewrite++;

			// setup slots
			slots[0] = 0;
			nextSlot = 0;
			rewriteMap[nextSlot++] = 0;

			if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
				fprintf(stderr, "%d: Q=%d T=%s%d F=%d ", this->ncount, Q, Ti ? "~" : "", Tu, F);

			/*
			 * Pull Q through state table
			 */
			if (Q < this->nstart) {

				if (Q && rewriteVersion[Q] != thisVersion) {
					slots[nextSlot]   = Q;
					rewriteVersion[Q] = thisVersion;
					rewriteMap[Q]     = nextSlot++;
				}

				// endpoint Q stores as 2 successive, no invert
				ix = rewriteData[ix + rewriteMap[Q]];
				ix = rewriteData[ix + rewriteMap[Q]];

			} else {

				const baseNode_t *pNode = this->N + Q;
				const uint32_t   QQ     = pNode->Q;
				const uint32_t   QTu    = pNode->T & ~IBIT;
				const uint32_t   QTi    = pNode->T & IBIT;
				const uint32_t   QF     = pNode->F;

				if (QQ && rewriteVersion[QQ] != thisVersion) {
					slots[nextSlot]    = QQ;
					rewriteVersion[QQ] = thisVersion;
					rewriteMap[QQ]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[QQ]];

				if (QTu && rewriteVersion[QTu] != thisVersion) {
					slots[nextSlot]     = QTu;
					rewriteVersion[QTu] = thisVersion;
					rewriteMap[QTu]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[QTu]];

				if (QF && rewriteVersion[QF] != thisVersion) {
					slots[nextSlot]    = QF;
					rewriteVersion[QF] = thisVersion;
					rewriteMap[QF]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[QF]];

				ix = rewriteData[ix + (QTi ? 1 : 0)];

				// placeholder
				if (rewriteVersion[Q] != thisVersion) {
					slots[nextSlot]   = Q;
					rewriteVersion[Q] = thisVersion;
					rewriteMap[Q]     = nextSlot++;
				}
			}

			/*
			 * Pull T through state table
			 */
			if (Tu < this->nstart || rewriteVersion[Tu] == thisVersion) {

				if (Tu && rewriteVersion[Tu] != thisVersion) {
					slots[nextSlot]    = Tu;
					rewriteVersion[Tu] = thisVersion;
					rewriteMap[Tu]     = nextSlot++;
				}

				// endpoint Tu stores as 2 successive, no invert
				ix = rewriteData[ix + rewriteMap[Tu]];
				ix = rewriteData[ix + rewriteMap[Tu]];
			} else {

				const baseNode_t *pNode = this->N + Tu;
				const uint32_t   TQ     = pNode->Q;
				const uint32_t   TTu    = pNode->T & ~IBIT;
				const uint32_t   TTi    = pNode->T & IBIT;
				const uint32_t   TF     = pNode->F;

				if (TQ && rewriteVersion[TQ] != thisVersion) {
					slots[nextSlot]    = TQ;
					rewriteVersion[TQ] = thisVersion;
					rewriteMap[TQ]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[TQ]];


				if (TTu && rewriteVersion[TTu] != thisVersion) {
					slots[nextSlot]     = TTu;
					rewriteVersion[TTu] = thisVersion;
					rewriteMap[TTu]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[TTu]];


				if (TF && rewriteVersion[TF] != thisVersion) {
					slots[nextSlot]    = TF;
					rewriteVersion[TF] = thisVersion;
					rewriteMap[TF]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[TF]];


				ix = rewriteData[ix + (TTi ? 1 : 0)];

				// placeholder
				slots[nextSlot]    = Tu;
				rewriteVersion[Tu] = thisVersion;
				rewriteMap[Tu]     = nextSlot++;
			}

			/*
			 * Pull F through state table
			 */
			if (F < this->nstart || rewriteVersion[F] == thisVersion) {

				if (F && rewriteVersion[F] != thisVersion) {
					slots[nextSlot]   = F;
					rewriteVersion[F] = thisVersion;
					rewriteMap[F]     = nextSlot++;
				}

				// endpoint F stores as 2 successive, no invert
				ix = rewriteData[ix + rewriteMap[F]];
				ix = rewriteData[ix + rewriteMap[F]];

			} else {

				const baseNode_t *pNode = this->N + F;
				const uint32_t   FQ     = pNode->Q;
				const uint32_t   FTu    = pNode->T & ~IBIT;
				const uint32_t   FTi    = pNode->T & IBIT;
				const uint32_t   FF     = pNode->F;

				if (FQ && rewriteVersion[FQ] != thisVersion) {
					slots[nextSlot]    = FQ;
					rewriteVersion[FQ] = thisVersion;
					rewriteMap[FQ]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[FQ]];


				if (FTu && rewriteVersion[FTu] != thisVersion) {
					slots[nextSlot]     = FTu;
					rewriteVersion[FTu] = thisVersion;
					rewriteMap[FTu]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[FTu]];


				if (FF && rewriteVersion[FF] != thisVersion) {
					slots[nextSlot]    = FF;
					rewriteVersion[FF] = thisVersion;
					rewriteMap[FF]     = nextSlot++;
				}
				ix = rewriteData[ix + rewriteMap[FF]];


				ix = rewriteData[ix + (FTi ? 1 : 0)];

				// placeholder
				slots[nextSlot]   = F;
				rewriteVersion[F] = thisVersion;
				rewriteMap[F]     = nextSlot++;
			}

			if (ENABLE_DEBUG_REWRITE && ctx.opt_debug & ctx.DEBUGMASK_REWRITE)
				fprintf(stderr, "-> [%d %d %d %d %d %d %d %d %d]",
					nextSlot < 1 ? 0 : slots[0],
					nextSlot < 2 ? 0 : slots[1],
					nextSlot < 3 ? 0 : slots[2],
					nextSlot < 4 ? 0 : slots[3],
					nextSlot < 5 ? 0 : slots[4],
					nextSlot < 6 ? 0 : slots[5],
					nextSlot < 7 ? 0 : slots[6],
					nextSlot < 8 ? 0 : slots[7],
					nextSlot < 9 ? 0 : slots[8]);

			/*
			 * top level inverted T.
			 * NOTE: Not an index but an offset
			 */
			//
			assert(ix);
			ix = ix + (Ti ? 1 : 0);

			if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
				fprintf(stderr, " -> ix=%x", ix);

			gLastRewriteIndex = ix;

			/*
			 * Respond to rewrite
			 */
			uint32_t data = rewriteData[ix];
			assert(data);

			gCountRewritePower[(data >> REWRITEFLAG_POWER) & 0xf]++;

			if (data & REWRITEMASK_TREE) {
				// destructive rewrite
				gCountRewriteTree++;

				uint64_t treedata = rewriteTree[data & 0xffffff];
				uint32_t temp[16];
				uint32_t nextNode = MAXSLOTS + 1;
				uint32_t r        = 0;

				if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
					fprintf(stderr, " -> tree=%lx {\n", treedata);

				while (treedata) {
					uint32_t F = treedata & 0xf;
					treedata >>= 4;
					uint32_t Tu = treedata & 0xf;
					treedata >>= 4;
					uint32_t Q = treedata & 0xf;
					treedata >>= 4;
					uint32_t Ti = (treedata & 0xf) ? IBIT : 0;
					treedata >>= 4;

					Q            = (Q >= 10) ? temp[Q] : slots[Q];
					Tu           = (Tu >= 10) ? temp[Tu] : slots[Tu];
					F            = (F >= 10) ? temp[F] : slots[F];

					// NOTE: tail-recursion, safe to call
					// NOTE: returns IBIT if QTF is /safe/ without creating instance

					r = this->addNormaliseNode(Q, Tu ^ Ti, F);

					temp[nextNode++] = r;
				}

				if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
					fprintf(stderr, "}\n");

				gCountRewriteTree++;
				return r;

			} else if (data & REWRITEMASK_COLLAPSE) {

				if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
					fprintf(stderr, " -> collapse=%x\n", data);

				// rewrite is a full collapse
				gCountRewriteCollapse++;
				return slots[data & 0xf];

			} else if (data & REWRITEMASK_FOUND) {

				if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
					fprintf(stderr, " -> weak=%x\n", data);

				// no rewrite needed
				gCountRewriteNo++;
				return IBIT;

			} else {

				if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
					fprintf(stderr, " -> order=%x {\n", data);

				// rewrite with available components
				uint32_t newF = slots[data & 15];
				data >>= 4;
				uint32_t newTu = slots[data & 15];
				data >>= 4;
				uint32_t newQ = slots[data & 15];
				data >>= 4;
				uint32_t newTi = (data & 1) ? IBIT : 0;

				// rewriteData[] only addresses structure, need `normaliseNode()` to address ordering
				// NOTE: tail-recursion, safe to call
				uint32_t r = this->addNormaliseNode(newQ, newTu ^ newTi, newF);

				if (ENABLE_DEBUG_REWRITE && (ctx.opt_debug & ctx.DEBUGMASK_REWRITE))
					fprintf(stderr, "}\n");

				gCountRewriteYes++;
				return r;

			}

			// should not reach here
			assert(0);
		}
	}

	/*
	 * @date 2021-05-22 19:10:33
	 *
	 * Encode a prefix into given parameter `pNode`.
	 *
	 * NOTE: `std::string` usage exception because this is NOT speed critical code AND strings can become gigantically large
	 */
	static void encodePrefix(std::string &name, unsigned value) {

		// NOTE: 0x7fffffff = `GYTISXx`

		// creating is right-to-left. Storage to reverse
		char stack[10], *pStack = stack;

		// push terminator
		*pStack++ = 0;

		// process the value
		do {
			*pStack++ = (char) ('A' + (value % 26));
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
	 * 
	 * @date 2022-02-08 03:50:57
	 * if `allRoots` is true, then ignore `id` and output all roots. 
	 */
	std::string saveString(uint32_t id, std::string *pTransform = NULL, bool allRoots = false) {

		std::string name;

		uint32_t nextPlaceholder = this->kstart;
		uint32_t nextNode        = this->nstart;
		uint32_t *pStack         = allocMap();
		uint32_t *pMap           = allocMap();
		uint32_t *pVersion       = allocVersion();
		uint32_t thisVersion     = ++mapVersionNr;
		uint32_t numStack        = 0; // top of stack

		// clear version map when wraparound
		if (thisVersion == 0) {
			::memset(pVersion, 0, maxNodes * sizeof *pVersion);
			thisVersion = ++mapVersionNr;
		}

		// starting point
		if (allRoots) {
			// push all roots in reverse order
			for (uint32_t iRoot = 0; iRoot < this->numRoots; iRoot++)
				pStack[numStack++] = this->roots[this->numRoots - 1 - iRoot];
		} else {
			// push single id
			pStack[numStack++] = id;
		}

		do {
			// pop stack
			uint32_t Ru = pStack[--numStack];
			uint32_t Ri = Ru & IBIT;
			Ru &= ~IBIT;

			// if endpoint then emit
			if (Ru == 0) {
				// zero
				name += '0';

				if (Ri)
					name + '~';

				continue;

			} else if (Ru < this->nstart) {
				uint32_t value;

				if (!pTransform) {
					// endpoint
					value = Ru - this->kstart;
				} else {
					// placeholder
					if (pVersion[Ru] != thisVersion) {
						pVersion[Ru] = thisVersion;
						pMap[Ru]     = nextPlaceholder++;

						value = Ru - this->kstart;
						if (value < 26) {
							*pTransform += (char) ('a' + value);
						} else {
							encodePrefix(*pTransform, value / 26);
							*pTransform += (char) ('a' + (value % 26));
						}
					}

					value = pMap[Ru] - this->kstart;
				}

				// convert id to (prefixed) letter
				if (value < 26) {
					name += (char) ('a' + value);
				} else {
					encodePrefix(name, value / 26);
					name += (char) ('a' + (value % 26));
				}

				if (Ri)
					name + '~';

				continue;
			}

			const baseNode_t *pNode = this->N + Ru;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			// determine if node already handled
			if (pVersion[Ru] != thisVersion) {
				// first time
				pVersion[Ru] = thisVersion;
				pMap[Ru]     = 0;

				// push id so it visits again after expanding
				pStack[numStack++] = Ru;

				// push non-zero endpoints
				if (F >= this->kstart)
					pStack[numStack++] = F;
				if (Tu != F && Tu >= this->kstart)
					pStack[numStack++] = Tu;
				if (Q >= this->kstart)
					pStack[numStack++] = Q;

				assert(numStack < maxNodes);
				continue;

			} else if (pMap[Ru] == 0) {
				// node complete, output operator
				pMap[Ru] = nextNode++;

				if (Ti) {
					if (Tu == 0) {
						// OR Q?!0:F
						name += '+';
					} else if (F == 0) {
						// GT Q?!T:0
						name += '>';
					} else if (F == Tu) {
						// NE Q?!F:F
						name += '^';
					} else {
						// QnTF Q?!T:F
						name += '!';
					}
				} else {
					if (Tu == 0) {
						// LT Q?0:F
						name += '<';
					} else if (F == 0) {
						// AND Q?T:0
						name += '&';
					} else if (F == Tu) {
						// SELF Q?F:F
						assert(!"Q?F:F");
					} else {
						// QTF Q?T:F
						name += '?';
					}
				}

			} else {
				// back-reference to earlier node
				uint32_t dist = nextNode - pMap[Ru];

				// convert id to (prefixed) back-link
				if (dist < 10) {
					name += (char) ('0' + dist);
				} else {
					encodePrefix(name, dist / 10);
					name += (char) ('0' + (dist % 10));
				}
			}

			if (Ri)
				name + '~';

		} while (numStack > 0);

		assert(nextPlaceholder <= this->nstart);

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
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
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

				transformList[t] = value * 26 + *pTransform++ - 'a' + kstart;

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
	 * Import/add a string into tree.
	 * NOTE: Will use `normaliseNode()`.
	 */
	uint32_t loadStringSafe(const char *pName, const char *pSkin = NULL) {

		assert(pName[0]); // disallow empty name

		// modify if transform is present
		uint32_t *transformList = NULL;
		if (pSkin && *pSkin)
			transformList = decodeTransform(ctx, kstart, nstart, pSkin);

		/*
		 * init
		 */

		uint32_t numStack = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap();
		uint32_t *pMap    = allocMap();
		uint32_t nid;

		/*
		 * Load string
		 */
		for (const char *pattern = pName; *pattern; pattern++) {

			switch (*pattern) {
			case '0': //
				pStack[numStack++] = 0;
				break;

				// @formatter:off
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				// @formatter:on
			{
				/*
				 * Push back-reference
				 */
				uint32_t v = nextNode - (*pattern - '0');

				if (v < this->nstart || v >= nextNode)
					ctx.fatal("[node out of range: %d]\n", v);
				if (numStack >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[numStack++] = pMap[v];

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
				if (numStack >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				if (transformList)
					pStack[numStack++] = transformList[v];
				else
					pStack[numStack++] = v;
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
					if (numStack >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					pStack[numStack++] = pMap[v];
				} else if (islower(*pattern)) {
					/*
					 * prefixed endpoint
					 */
					v = this->kstart + (v * 26 + *pattern - 'a');

					if (v < this->kstart || v >= this->nstart)
						ctx.fatal("[endpoint out of range: %d]\n", v);
					if (numStack >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					if (transformList)
						pStack[numStack++] = transformList[v];
					else
						pStack[numStack++] = v;
				} else {
					ctx.fatal("[bad token '%c']\n", *pattern);
				}
				break;
			}

			case '+': {
				// OR (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNormaliseNode(L, IBIT, R);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '>': {
				// GT (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNormaliseNode(L, R ^ IBIT, 0);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '^': {
				// XOR/NE (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNormaliseNode(L, R ^ IBIT, R);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '!': {
				// QnTF (appreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--numStack];
				uint32_t T = pStack[--numStack];
				uint32_t Q = pStack[--numStack];

				nid = addNormaliseNode(Q, T ^ IBIT, F);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND (depreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNormaliseNode(L, R, 0);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF (depreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--numStack];
				uint32_t T = pStack[--numStack];
				uint32_t Q = pStack[--numStack];

				nid = addNormaliseNode(Q, T, F);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT (support)
				if (numStack < 1)
					ctx.fatal("[stack underflow]\n");

				pStack[numStack - 1] ^= IBIT;
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

			if (numStack > maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (numStack != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[numStack - 1];

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
	 * Import/add a string into tree.
	 * NOTE: Will use `basicNode()`.
	 */
	uint32_t loadStringFast(const char *pName, const char *pSkin = NULL) {

		// modify if transform is present
		uint32_t *transformList = NULL;
		if (pSkin && *pSkin)
			transformList = decodeTransform(ctx, kstart, nstart, pSkin);

		/*
		 * init
		 */

		uint32_t numStack = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap();
		uint32_t *pMap    = allocMap();
		uint32_t nid;

		/*
		 * Load string
		 */
		for (const char *pattern = pName; *pattern; pattern++) {

			switch (*pattern) {
			case '0': //
				pStack[numStack++] = 0;
				break;

				// @formatter:off
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				// @formatter:on
			{
				/*
				 * Push back-reference
				 */
				uint32_t v = nextNode - (*pattern - '0');

				if (v < this->nstart || v >= nextNode)
					ctx.fatal("[node out of range: %d]\n", v);
				if (numStack >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[numStack++] = pMap[v];

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
				if (numStack >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				if (transformList)
					pStack[numStack++] = transformList[v];
				else
					pStack[numStack++] = v;
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
					if (numStack >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					pStack[numStack++] = pMap[v];
				} else if (islower(*pattern)) {
					/*
					 * prefixed endpoint
					 */
					v = this->kstart + (v * 26 + *pattern - 'a');

					if (v < this->kstart || v >= this->nstart)
						ctx.fatal("[endpoint out of range: %d]\n", v);
					if (numStack >= this->ncount)
						ctx.fatal("[stack overflow]\n");

					if (transformList)
						pStack[numStack++] = transformList[v];
					else
						pStack[numStack++] = v;
				} else {
					ctx.fatal("[bad token '%c']\n", *pattern);
				}
				break;
			}

			case '+': {
				// OR (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNode(L, IBIT, R);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '>': {
				// GT (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNode(L, R ^ IBIT, 0);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '^': {
				// XOR/NE (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNode(L, R ^ IBIT, R);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '!': {
				// QnTF (appreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--numStack];
				uint32_t T = pStack[--numStack];
				uint32_t Q = pStack[--numStack];

				nid = addNode(Q, T ^ IBIT, F);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND (depreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				nid = addNode(L, R, 0);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF (depreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--numStack];
				uint32_t T = pStack[--numStack];
				uint32_t Q = pStack[--numStack];

				nid = addNode(Q, T, F);

				pStack[numStack++] = pMap[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT (support)
				if (numStack < 1)
					ctx.fatal("[stack underflow]\n");

				pStack[numStack - 1] ^= IBIT;
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

			if (numStack > maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (numStack != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[numStack - 1];

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
		uint32_t *pSelect    = this->allocVersion();
		uint32_t thisVersion = ++this->mapVersionNr;

		if (thisVersion == 0) {
			::memset(pSelect, 0, this->maxNodes * sizeof *pSelect);
			thisVersion = ++this->mapVersionNr;
		}

		unsigned numCount = this->nstart;

		// select the heads
		// add artificial root for system
		for (unsigned iRoot = this->kstart; iRoot <= this->numRoots; iRoot++) {
			uint32_t R = (iRoot < this->numRoots) ?  this->roots[iRoot] :  this->system;
			uint32_t Ru = R & ~IBIT;

			if (Ru >= this->nstart && pSelect[Ru] != thisVersion) {
				numCount++;
				pSelect[Ru] = thisVersion;
			}
		}

		for (uint32_t iNode = this->ncount - 1; iNode >= this->nstart; --iNode) {
			if (pSelect[iNode] != thisVersion)
				continue;

			const baseNode_t *pNode = this->N + iNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
//			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			if (Q >= this->nstart && pSelect[Q] != thisVersion) {
				numCount++;
				pSelect[Q] = thisVersion;
			}

			if (Tu >= this->nstart && pSelect[Tu] != thisVersion) {
				numCount++;
				pSelect[Tu] = thisVersion;
			}

			if (F >= this->nstart && pSelect[F] != thisVersion) {
				numCount++;
				pSelect[F] = thisVersion;
			}
		}

		this->freeVersion(pSelect);
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
		uint32_t *pSelect    = RHS->allocVersion();
		uint32_t thisVersion = ++RHS->mapVersionNr;

		// clear version map when wraparound
		if (thisVersion == 0) {
			::memset(pSelect, 0, RHS->maxNodes * sizeof *pSelect);
			thisVersion = ++RHS->mapVersionNr;
		}

		/*
		 * mark active
		 */

		for (unsigned iRoot = 0; iRoot < RHS->numRoots; iRoot++)
			pSelect[RHS->roots[iRoot] & ~IBIT] = thisVersion;

		pSelect[RHS->system & ~IBIT]               = thisVersion;

		for (uint32_t iNode = RHS->ncount - 1; iNode >= RHS->nstart; --iNode) {
			if (pSelect[iNode] == thisVersion) {
				const baseNode_t *pNode = RHS->N + iNode;

				pSelect[pNode->Q]         = thisVersion;
				pSelect[pNode->T & ~IBIT] = thisVersion;
				pSelect[pNode->F]         = thisVersion;
			}
		}

		/*
		 * Copy selected nodes
		 */

		for (unsigned iRoot = 0; iRoot < RHS->nstart; iRoot++)
			pMap[iRoot] = iRoot;

		for (uint32_t iNode = RHS->nstart; iNode < RHS->ncount; iNode++) {
			if (pSelect[iNode] == thisVersion) {
				const baseNode_t *pNode = RHS->N + iNode;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				pMap[iNode] = this->addNode(pMap[Q], pMap[Tu] ^ Ti, pMap[F]);
			}
		}

		/*
		 * copy roots
		 */

		for (unsigned iRoot = 0; iRoot < this->numRoots; iRoot++)
			this->roots[iRoot] = pMap[RHS->roots[iRoot] & ~IBIT] ^ (RHS->roots[iRoot] & IBIT);

		this->system = pMap[RHS->system & ~IBIT] ^ (RHS->system & IBIT);

		RHS->freeVersion(pSelect);
		RHS->freeMap(pMap);
	}

	/*
	 * @date 2021-08-18 18:04:14
	 *
	 * Import node and its dependencies
	 */
	uint32_t importNodes(baseTree_t *RHS, uint32_t nodeId) {
		uint32_t *pMap       = RHS->allocMap();
		uint32_t *pSelect    = RHS->allocVersion();
		uint32_t thisVersion = ++RHS->mapVersionNr;

		// clear version map when wraparound
		if (thisVersion == 0) {
			::memset(pSelect, 0, RHS->maxNodes * sizeof *pSelect);
			thisVersion = ++RHS->mapVersionNr;
		}

		/*
		 * mark active
		 */
		pSelect[nodeId & ~IBIT] = thisVersion;

		for (uint32_t iNode = (nodeId & ~IBIT); iNode >= RHS->nstart; --iNode) {
			if (pSelect[iNode] == thisVersion) {
				const baseNode_t *pNode = RHS->N + iNode;

				pSelect[pNode->Q]         = thisVersion;
				pSelect[pNode->T & ~IBIT] = thisVersion;
				pSelect[pNode->F]         = thisVersion;
			}
		}

		/*
		 * Copy selected nodes
		 */

		for (unsigned iRoot = 0; iRoot < RHS->nstart; iRoot++)
			pMap[iRoot] = iRoot;

		for (uint32_t iNode = RHS->nstart; iNode <= (nodeId & ~IBIT); iNode++) {
			if (pSelect[iNode] == thisVersion) {
				const baseNode_t *pNode = RHS->N + iNode;
				const uint32_t   Q      = pNode->Q;
				const uint32_t   Tu     = pNode->T & ~IBIT;
				const uint32_t   Ti     = pNode->T & IBIT;
				const uint32_t   F      = pNode->F;

				pMap[iNode] = this->addNode(pMap[Q], pMap[Tu] ^ Ti, pMap[F]);
			}
		}

		uint32_t ret = pMap[nodeId & ~IBIT] ^ (nodeId & IBIT);

		RHS->freeVersion(pSelect);
		RHS->freeMap(pMap);

		return ret;
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
		for (unsigned iEntry = 0; iEntry < RHS->nstart; iEntry++)
			pMapSet[iEntry] = pMapClr[iEntry] = iEntry;

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

			pMapSet[iNode] = this->addNormaliseNode(pMapSet[Q], pMapSet[Tu] ^ Ti, pMapSet[F]);
			pMapClr[iNode] = this->addNormaliseNode(pMapClr[Q], pMapClr[Tu] ^ Ti, pMapClr[F]);
		}

		/*
		 * Set roots
		 */
		for (unsigned iRoot = 0; iRoot < RHS->numRoots; iRoot++) {
			uint32_t Ru = RHS->roots[iRoot] & ~IBIT;
			uint32_t Ri = RHS->roots[iRoot] & IBIT;

			this->roots[iRoot] = this->addNormaliseNode(iFold, pMapSet[Ru], pMapClr[Ru]) ^ Ri;
		}

		if (RHS->system) {
			uint32_t Ru = RHS->system & ~IBIT;
			uint32_t Ri = RHS->system & IBIT;

			this->system = this->addNormaliseNode(iFold, pMapSet[Ru], pMapClr[Ru]) ^ Ri;
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

		if (!entryNames.empty() || !rootNames.empty() || allocFlags || hndl >= 0)
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

			rawData = (uint8_t *) pMemory;
		} else {
			/*
			 * Read the contents
			 */
			rawData = (uint8_t *) ctx.myAlloc("baseTreeFile_t::rawData", 1, (size_t) stbuf.st_size);
			uint64_t progressHi = stbuf.st_size;
			uint64_t progress   = 0;

			// read in chunks of 1024*1024 bytes
			uint64_t dataLength = stbuf.st_size;
			uint8_t  *dataPtr   = rawData;
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

		fileHeader = (baseTreeHeader_t *) rawData;
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
		N             = (baseNode_t *) (rawData + fileHeader->offNodes);
		roots         = (uint32_t *) (rawData + fileHeader->offRoots);
		history       = (uint32_t *) (rawData + fileHeader->offHistory);
		// pools
		pPoolMap      = (uint32_t **) ctx.myAlloc("baseTree_t::pPoolMap", MAXPOOLARRAY, sizeof(*pPoolMap));
		pPoolVersion  = (uint32_t **) ctx.myAlloc("baseTree_t::pPoolVersion", MAXPOOLARRAY, sizeof(*pPoolVersion));
		// structure based compare
		stackL        = allocMap();
		stackR        = allocMap();
		compBeenWhatL = allocMap();
		compBeenWhatR = allocMap();
		compVersionL  = allocMap(); // allocate as node-id map because of local version numbering
		compVersionR  = allocMap();  // allocate as node-id map because of local version numbering
		compVersionNr = 1;

		// make all `entryNames`+`rootNames` indices valid
		entryNames.resize(nstart);
		rootNames.resize(numRoots);

		// slice names
		{
			const char *pData = (const char *) (rawData + fileHeader->offNames);

			for (unsigned iEntry  = 0; iEntry < nstart; iEntry++) {
				assert(*pData != 0);
				entryNames[iEntry] = pData;
				pData += strlen(pData) + 1;
			}
			for (unsigned iRoot = 0; iRoot < numRoots; iRoot++) {
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
		 * File header.
		 * Needs to be static because its address is returned
		 */

		static baseTreeHeader_t header;
		memset(&header, 0, sizeof header);

		// zeros for alignment
		uint8_t zero16[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
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

		// write entryNames
		for (uint32_t i = 0; i < nstart; i++) {
			size_t len = entryNames[i].length() + 1;
			assert(len > 1);
			fwrite(entryNames[i].c_str(), len, 1, outf);
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

			// output entrypoints
			for (unsigned iEntry = 0; iEntry < nstart; iEntry++) {
				// get remapped
				baseNode_t wrtNode;
				wrtNode.Q = 0;
				wrtNode.T = IBIT;
				wrtNode.F = iEntry;

				pMap[iEntry] = nextId++;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.Q));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.T));
				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.F));

			}

			// output nodes
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

			// output entrypoints
			for (unsigned iEntry = 0; iEntry < nstart; iEntry++) {
				pVersion[iEntry] = thisVersion;
				pMap[iEntry]     = iEntry;

				// get remapped
				baseNode_t wrtNode;
				wrtNode.Q = 0;
				wrtNode.T = IBIT;
				wrtNode.F = iEntry;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				pMap[iEntry] = nextId++;

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
			for (unsigned iRoot = 0; iRoot <= numRoots; iRoot++) {

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

		if (!entryNames.empty() || !rootNames.empty() || allocFlags || hndl >= 0)
			ctx.fatal("baseTree_t::loadFileJson() on non-initial tree\n");

		/*
		 * import flags
		 */
		flags = ctx.flagsFromJson(json_object_get(jInput, "flags"));

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

		// make all `entryNames`+`rootNames` indices valid
		entryNames.resize(nstart);
		rootNames.resize(numRoots);

		/*
		 * Reserved names
		 */
		entryNames[0]            = "0";
		entryNames[1 /*KERROR*/] = "KERROR";

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
			entryNames[kstart + iName] = json_string_value(json_array_get(jNames, iName));

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
			entryNames[ostart + iName] = json_string_value(json_array_get(jNames, iName));

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
			entryNames[estart + iName] = json_string_value(json_array_get(jNames, iName));

		/*
		 * import rnames (extended root names)
		 */

		// copy fixed part
		for (unsigned iRoot = 0; iRoot < estart; iRoot++)
			rootNames[iRoot] = entryNames[iRoot];

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
			rootNames = entryNames;
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
	json_t *summaryInfo(json_t *jResult) {
		if (jResult == NULL)
			jResult = json_object();

		json_object_set_new_nocheck(jResult, "flags", ctx.flagsToJson(this->flags));
		json_object_set_new_nocheck(jResult, "kstart", json_integer(this->kstart));
		json_object_set_new_nocheck(jResult, "ostart", json_integer(this->ostart));
		json_object_set_new_nocheck(jResult, "estart", json_integer(this->estart));
		json_object_set_new_nocheck(jResult, "nstart", json_integer(this->nstart));
		json_object_set_new_nocheck(jResult, "ncount", json_integer(this->ncount));
		json_object_set_new_nocheck(jResult, "numroots", json_integer(this->numRoots));
		json_object_set_new_nocheck(jResult, "size", json_integer(this->ncount - this->nstart));
		json_object_set_new_nocheck(jResult, "numhistory", json_integer(this->numHistory));
		json_object_set_new_nocheck(jResult, "poshistory", json_integer(this->posHistory));

		return jResult;
	}

	/*
	 * Extract details into json
	 */
	json_t *headerInfo(json_t *jResult) {
		if (jResult == NULL)
			jResult = json_object();

		char crcstr[32];
		sprintf(crcstr, "%08x", fileHeader->crc32);

		json_object_set_new_nocheck(jResult, "flags", ctx.flagsToJson(fileHeader->magic_flags));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader->offEnd));
		json_object_set_new_nocheck(jResult, "crc", json_string_nocheck(crcstr));
		json_object_set_new_nocheck(jResult, "kstart", json_integer(fileHeader->kstart));
		json_object_set_new_nocheck(jResult, "ostart", json_integer(fileHeader->ostart));
		json_object_set_new_nocheck(jResult, "estart", json_integer(fileHeader->estart));
		json_object_set_new_nocheck(jResult, "nstart", json_integer(fileHeader->nstart));
		json_object_set_new_nocheck(jResult, "ncount", json_integer(fileHeader->ncount));
		json_object_set_new_nocheck(jResult, "numroots", json_integer(fileHeader->numRoots));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader->ncount - fileHeader->nstart));
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
		 * entry/root names
		 */
		json_t *jEntrynames = json_array();

		// input entry names
		for (unsigned iEntry = kstart; iEntry < ostart; iEntry++)
			json_array_append_new(jEntrynames, json_string_nocheck(entryNames[iEntry].c_str()));
		json_object_set_new_nocheck(jResult, "knames", jEntrynames);

		jEntrynames = json_array();

		// output entry names
		for (unsigned iEntry = ostart; iEntry < estart; iEntry++)
			json_array_append_new(jEntrynames, json_string_nocheck(entryNames[iEntry].c_str()));
		json_object_set_new_nocheck(jResult, "onames", jEntrynames);

		jEntrynames = json_array();

		// extended entry names
		for (unsigned iEntry = estart; iEntry < nstart; iEntry++)
			json_array_append_new(jEntrynames, json_string_nocheck(entryNames[iEntry].c_str()));
		json_object_set_new_nocheck(jResult, "enames", jEntrynames);

		// extended root names (which might be identical to enames)
		bool rootsDiffer = (nstart != numRoots);
		if (!rootsDiffer) {
			for (unsigned iEntry = 0; iEntry < nstart; iEntry++) {
				if (entryNames[iEntry].compare(rootNames[iEntry]) != 0) {
					rootsDiffer = true;
					break;
				}
			}
		}

		if (rootsDiffer) {
			// either roots are different or an empty set.
			jEntrynames = json_array();

			for (unsigned iRoot = estart; iRoot < numRoots; iRoot++)
				json_array_append_new(jEntrynames, json_string_nocheck(rootNames[iRoot].c_str()));
			json_object_set_new_nocheck(jResult, "rnames", jEntrynames);
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
#endif

		/*
		 * History
		 */

		json_t *jHistory = json_array();


		for (uint32_t i = 0; i < this->numHistory; i++) {
			json_array_append_new(jHistory, json_string_nocheck(entryNames[this->history[i]].c_str()));
		}

		json_object_set_new_nocheck(jResult, "history", jHistory);

		/*
		 * Refcounts
		 */

		uint32_t *pRefCount = allocMap();

		for (unsigned iEntry = 0; iEntry < this->nstart; iEntry++)
			pRefCount[iEntry] = 0;

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

		for (unsigned iEntry = this->kstart; iEntry < this->nstart; iEntry++) {
			if (pRefCount[iEntry])
				json_object_set_new_nocheck(jRefCount, entryNames[iEntry].c_str(), json_integer(pRefCount[iEntry]));
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
