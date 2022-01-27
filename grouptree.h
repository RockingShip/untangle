#ifndef _GROUPTREE_H
#define _GROUPTREE_H

/*
 * Terminology used:
 * 
 * Folding:
 * 	Single-node with multiple (identical) references that can be eliminated.
 * Collapse:
 *	Multi-node structures with references that can be optimised to form smaller structures
 * Self collapse:
 * 	Structures with direct or indirect references to the group it participates in.
 * 	These all collapse to `a/[self]` which is the group header and always present
 * Endpoint collapse
 * 	Structures with multiple references that optimise to a single reference.
 * 	This also causes the group it participates in to collapse to that reference.
 * Entrypoint collapse:
 * 	Special variant of an endpoint-collapse referencingone of the entry points (id < nstart).
 * Active group:
 * 	Group that has nodes and is non-redirecting. `this->N[gid].gid == gid`.
 * Closed group:
 *	Group which is being referenced and cannot be extended with new nodes. `this->pGidRefCount[gid] != 0`.
 * Open Group:
 * 	Group under construction  `this->pGidRefCount[gid] == 0`.
 * Silently ignore:
 * 	A result can be created, which will be ignored.
 * 	
 * Although `groupTree_t` avoids using 0 as node it, it is a valid reference.
 * IBIT is often used as not-found indicator, or as default value instead of 0.
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

#include <fcntl.h>
#include <jansson.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>

#include "context.h"
#include "database.h"

/*
 * Version number of data file
 */
#define GROUPTREE_MAGIC 0x20220112

struct groupNode_t {

	/*
	 * Group to which this node belongs.
	 * Each group is a list of nodes.
	 * The group id is the node id of the first node in the list.
	 * The list is unordered, except for the first node, 
	 * The first node is always `0n9` (being either `SID_ZERO` or `SID_SELF`)
	 * Each active list should have at least one `1n9` node.
	 * 
	 * Lists consist of several layers, each layer representing a signature node size.
	 * 
	 * If `groupId`==`nodeId`, then the node is a group list header.
	 * Nodes are sometimes relocated to other groups.
	 * If `gid` is different than the group id of the list, then a better alternative has been found.
	 * Updating is strongly suggested with `while (id != N[id])...`
	 * 
	 * @date 2022-01-24 13:05:05
	 * 
	 * For orphans (N[id].next == id) only `gid, `sid` and `slots` are valid.
	 * Entrypoints (id < nstart) are orphans, and have ALL members valid.
	 * Zero (id == 0) is an entrypoint and has all members zero, except `sid` which is `db.SID_ZERO`.
	 * Groups under-construction (N[id].gid == id && N[id].gid == IBIT) have `weight` set to max (+inf) and `hiSlotId` set to 0. 
	 */
	uint32_t gid;

	/*
	 * Double linked list of nodes
	 * These are the only id that are allowed to be a forward reference.
	 * The last node in the list has `next=0`.
	 * NOTE: Because double linked list, the list header `.prev` always points to the last entry of the list
	 */
	uint32_t prev;
	uint32_t next;

	/*
	 * Used for debuging to track the original node
	 */
	uint32_t oldId;

	/*
	 * weight/score. Sum of weights of all references. Less is better
	 */
	double weight;

	/*
	 * Highest group id found in slots
	 */
	uint32_t hiSlotId;

	/*
	 * The signature describing the behaviour of the node
	 */
	uint32_t sid;

	/*
	 * Signature endpoints.
	 * Endpoints are group id's, always referencing the first node in lists.
	 * Unused entries should always be zero.
	 */
	uint32_t slots[MAXSLOTS];

	/*
	 * @date 2022-01-09 03:42:50
	 * `slots` MUST be last to maintain forward version compatibility. 
	 */
};

/*
 * @date 2021-12-06 15:08:00
 *
 * Versioned memory is a vector where each entry has an associated version number.
 * Any entry not matching the current version is considered "default value".
 * This structure holds the version meta data.
 * All values must be less-equal than the current version.
 */
struct versionMemory_t {
	uint32_t version;        // Current version
	uint32_t numMemory;      // Number of elements
	uint32_t mem[];          // vector (size allocated on demand)

	/*
	 * Bump the next version number
	 */
	uint32_t nextVersion(void) {
		// bump
		++version;

		// clear version map when wraparound
		if (version == 0) {
			::memset(mem, 0, numMemory * sizeof mem[0]);
			++version;
		}

		return version;
	}

};

/*
 * The database file header
 */
struct groupTreeHeader_t {
	// meta
	uint32_t magic;               // magic+version
	uint32_t magic_flags;         // conditions it was created
	uint32_t sidCRC;              // CRC of database containing sid descriptions
	uint32_t system;              // node of balanced system (0 if none)
	uint32_t crc32;               // crc of nodes/roots, calculated during save
	uint32_t maxDepth;            // maxDepth for `expandSignature()`. NOTE: Loaded trees are read-only 

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

/*
 *
 */
struct groupTree_t {

	/*
	 * Constants for 
	 */

	/**
	 * Part of the core algorithm in detecting identical groups, is to expand nodes based on signature members.
	 * Members are considered the minimal collection of structures and their components to reach all signature id's.
	 * Recursively expanding structures turns out to escalate and requires some form of dampening.
	 * 
	 * 2021-12-25 13:33:53
	 * 
	 * Setting this to non-zero turn out to have an opposite effect.
	 * 
	 * 'DvDnBjEteBcBhEl^ByEd^!2Bc3&^?56Bc>eBce7?^!?BhB0EtBcEl^^eBc^ElEt^^!e5B65^!!!BjeC1BcEt^>C2^3C3C4!!BhEdEt^B4ByEl^^^ByEt^B8EdEl^^^!!!DnBjEteElBcBh^^D81^!E03C8E1^!!BhC3E3E3EleEl^!!eC6E6El!ElC7>E8^!!!BjEtE0^BhBy^C4^^BheElD6+F7^D8F8El?!eEdEtByBcEl&^ElBcBy^By!?ByB8^!C1!!!!!'
	 *
	 * Overview resulting size:
	 * configuration                 | size | ncount
	 * ------------------------------+------+-------
	 * ExpandSignature(), maxDepth=0 | 57   | 904
	 * ExpandSignature(), maxDepth=1 | 35   | 3493
	 * ExpandSignature(), maxDepth=2 | 25   | 5940
	 * ExpandMember(),    maxDepth=0 | 57   | 904
	 * ExpandMember(),    maxDepth=1 | 53   | 3246
	 * ExpandMember(),    maxDepth=2 | 27   | 7269
	 * 
 	 * @constant {number} DEFAULT_MAXDEPTH
	 */
	#if !defined(GROUPTREE_DEFAULT_MAXDEPTH)
	#define GROUPTREE_DEFAULT_MAXDEPTH 5
	#endif

	/**
	 * The maximum number of nodes a writable tree can hold is indicated with the `--maxnode=n` option.
	 * When saving, trees become read-only and are shrink to fit.
	 * This is the default value for `--maxnode=`.
	 *
	 * NOTE: for `groupTree_t` this will allocate at least 11 arrays of DEFAULT_MAXNODE*sizeof(uint32_t)
	 *
	 * @constant {number} DEFAULT_MAXNODE
	 */
	// todo: measure runtime usage to tune this value
	#if !defined(GROUPTREE_DEFAULT_MAXNODE)
	#define GROUPTREE_DEFAULT_MAXNODE 200000000
	#endif

	/**
	 * Renumbering/remapping node id's is a major operation within `baseTree_t`.
	 * Instead of allocating new maps with chance of memory fragmentation, reuse previously released maps.
	 * The are two pools of maps, one specifically for containing node id's, the other for containing versioned memory id's.
	 * Maps are `--maxnode=` entries large.
	 *
	 * @constant {number} GROUPTREE_MAXPOOLARRAY
	 */
	#if !defined(GROUPTREE_MAXPOOLARRAY)
	#define GROUPTREE_MAXPOOLARRAY 128
	#endif

	/**
	 * When creating groups, top-level need to be created after component nodes.
	 * These nodes are temporarily stored in a heap.
	 *
	 * @constant {number} MAXPOOLARRAY
	 */
	#if !defined(GROUPTREE_MAXHEAPNODE)
	#define GROUPTREE_MAXHEAPNODE 1000
	#endif

	enum {
		DEFAULT_MAXDEPTH = GROUPTREE_DEFAULT_MAXDEPTH,
		DEFAULT_MAXNODE  = GROUPTREE_DEFAULT_MAXNODE,
		MAXPOOLARRAY     = GROUPTREE_MAXPOOLARRAY,
		MAXHEAPNODE      = GROUPTREE_MAXHEAPNODE,
	};

	/*
	 * Flags to indicate if sections were allocated or mapped
	 */
	enum {
		ALLOCFLAG_NAMES = 0,    // key/root names
		ALLOCFLAG_NODES,        // nodes
		ALLOCFLAG_ROOTS,        // roots
		ALLOCFLAG_HISTORY,      // history
		ALLOCFLAG_INDEX,        // node index/lookup table

		ALLOCMASK_NAMES   = 1 << ALLOCFLAG_NAMES,
		ALLOCMASK_NODES   = 1 << ALLOCFLAG_NODES,
		ALLOCMASK_ROOTS   = 1 << ALLOCFLAG_ROOTS,
		ALLOCMASK_HISTORY = 1 << ALLOCFLAG_HISTORY,
		ALLOCMASK_INDEX   = 1 << ALLOCFLAG_INDEX,
	};

	// resources
	context_t                &ctx;                  // resource context
	database_t               &db;                   // database for table lookups
	int                      hndl;                  // file handle
	uint8_t                  *rawData;              // base location of mmap segment
	groupTreeHeader_t        *fileHeader;           // file header
	// meta
	uint32_t                 flags;                 // creation constraints
	uint32_t                 allocFlags;            // memory constraints
	uint32_t                 system;                // node of balanced system
	unsigned                 maxDepth;                // Max node expansion depth
	// primary fields
	uint32_t                 kstart;                // first input key id.
	uint32_t                 ostart;                // first output key id.
	uint32_t                 estart;                // first external/extended key id. Roots from previous tree in chain.
	uint32_t                 nstart;                // id of first node
	uint32_t                 ncount;                // number of nodes in use (as in id)
	uint32_t                 gcount;                // number of groups in use (as in count)
	uint32_t                 maxNodes;              // maximum tree capacity
	uint32_t                 numRoots;              // entries in roots[]
	// names
	std::vector<std::string> keyNames;              // sliced version of `keyNameData`
	std::vector<std::string> rootNames;             // sliced version of `rootNameData`
	// primary storage
	groupNode_t              *N;                    // nodes
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
	unsigned                 numPoolMap;            // number of node-id pools in use
	uint32_t                 **pPoolMap;            // pool of available node-id maps
	unsigned                 numPoolVersion;        // number of version-id pools in use
	versionMemory_t          **pPoolVersion;        // pool of available version-id maps
	// slots, for `addNormaliseNode()` because of too many exit points
	uint32_t                 *slotMap;              // slot position of endpoint 
	uint32_t                 *slotVersion;          // versioned memory for addNormaliseNode - content version
	uint32_t                 slotVersionNr;         // active version number
	// statistics
	uint64_t                 cntOutdated;           // `constructSlots()` detected and updated outdated Q/T/F
	uint64_t                 cntRestart;            // C-product got confused, restart  
	uint64_t                 cntUpdateGroupCollapse;// `updateGroup()` triggered an endpoint collapse
	uint64_t                 cntUpdateGroupMerge;   // `updateGroup()` triggered a cascading `mergeGroup()`  
	uint64_t                 cntApplySwapping;      // number of times to `applySwapping()` was applied
	uint64_t                 cntApplyFolding;       // number of times to `applyFolding()` was applied
	uint64_t                 cntMergeGroups;        // number of calls to `mergeGroup()`
	uint64_t                 cntAddNormaliseNode;   // number of calls to `addNormaliseNode()`
	uint64_t                 cntAddBasicNode;       // number of calls to `addBasicNode()`
	uint32_t                 cntUpdateGroup;        // number of calls to `updateGroup()`
	uint32_t                 cntValidate;           // counter of last valid tree
	uint32_t                 cntCproduct;           // counter number of Cproduct iterations
	//
	uint32_t		 overflowGroup;		// group causing overflow

	/**
	 * @date 2021-06-13 00:01:50
	 *
	 * Copy/Assign constructors not supported.
	 * Let usage trigger a "linker not found" error
	 */
	groupTree_t(const groupTree_t &rhs);
	groupTree_t &operator=(const groupTree_t &rhs);

	/*
	 * Create an empty tree, placeholder for reading from file
	 */
	groupTree_t(context_t &ctx, database_t &db) :
		ctx(ctx),
		db(db),
		hndl(-1),
		rawData(NULL),
		fileHeader(NULL),
		// meta
		flags(0),
		allocFlags(0),
		system(0),
		maxDepth(DEFAULT_MAXDEPTH),
		// primary fields
		kstart(0),
		ostart(0),
		estart(0),
		nstart(0),
		ncount(0),
		gcount(0),
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
		// slots
		slotMap(NULL),
		slotVersion(NULL),
		slotVersionNr(1),
		// statistics
		cntOutdated(0),
		cntRestart(0),
		cntUpdateGroupCollapse(0),
		cntUpdateGroupMerge(0),
		cntApplySwapping(0),
		cntApplyFolding(0),
		cntMergeGroups(0),
		cntAddNormaliseNode(0),
		cntAddBasicNode(0),
		cntUpdateGroup(0),
		cntValidate(0),
		cntCproduct(0),
		overflowGroup(0)
	{
	}

	/*
	 * Create a memory stored tree
	 */
	groupTree_t(context_t &ctx, database_t &db, uint32_t kstart, uint32_t ostart, uint32_t estart, uint32_t nstart, uint32_t numRoots, uint32_t maxNodes, unsigned maxDepth, uint32_t flags) :
		ctx(ctx),
		db(db),
		hndl(-1),
		rawData(NULL),
		fileHeader(NULL),
		// meta
		flags(flags),
		allocFlags(0),
		system(0),
		maxDepth(maxDepth),
		// primary fields
		kstart(kstart),
		ostart(ostart),
		estart(estart),
		nstart(nstart),
		ncount(nstart),
		gcount(0),
		maxNodes(maxNodes),
		numRoots(numRoots),
		// names
		keyNames(),
		rootNames(),
		// primary storage (allocated by storage context)
		N((groupNode_t *) ctx.myAlloc("groupTree_t::N", maxNodes, sizeof *N)),
		roots((uint32_t *) ctx.myAlloc("groupTree_t::roots", numRoots, sizeof *roots)),
		// history
		numHistory(0),
		posHistory(0),
		history((uint32_t *) ctx.myAlloc("groupTree_t::history", nstart, sizeof *history)),
		// node index  NOTE: reserve 4G for the node+version index
		nodeIndexSize(536870879), // first prime number before 0x20000000-8 (so that 4*this does not exceed 0x80000000-32),
		nodeIndex((uint32_t *) ctx.myAlloc("groupTree_t::nodeIndex", nodeIndexSize, sizeof *nodeIndex)),
		nodeIndexVersion((uint32_t *) ctx.myAlloc("groupTree_t::nodeIndexVersion", nodeIndexSize, sizeof *nodeIndexVersion)),
		nodeIndexVersionNr(1), // own version because longer life span
		// pools
		numPoolMap(0),
		pPoolMap((uint32_t **) ctx.myAlloc("groupTree_t::pPoolMap", MAXPOOLARRAY, sizeof(*pPoolMap))),
		numPoolVersion(0),
		pPoolVersion((versionMemory_t **) ctx.myAlloc("groupTree_t::pPoolVersion", MAXPOOLARRAY, sizeof(*pPoolVersion))),
		// slots
		slotMap(allocMap()),
		slotVersion(allocMap()),  // allocate as node-id map because of local version numbering
		slotVersionNr(1),
		// statistics
		cntOutdated(0),
		cntRestart(0),
		cntUpdateGroupCollapse(0),
		cntUpdateGroupMerge(0),
		cntApplySwapping(0),
		cntApplyFolding(0),
		cntMergeGroups(0),
		cntAddNormaliseNode(0),
		cntAddBasicNode(0),
		cntUpdateGroup(0),
		cntValidate(0),
		cntCproduct(0),
		overflowGroup()
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
		memset(this->N + 0, 0, sizeof(*this->N));
		this->N[0].sid = db.SID_ZERO;

		for (unsigned iKey = 1; iKey < nstart; iKey++) {
			groupNode_t *pNode = this->N + iKey;

			memset(pNode, 0, sizeof(*pNode));

			pNode->gid      = iKey;
			pNode->next     = iKey;
			pNode->prev     = iKey;
			pNode->weight   = 1;
			pNode->hiSlotId = iKey;
			pNode->sid      = db.SID_SELF;
			pNode->slots[0] = iKey;
		}

		// setup default roots
		for (unsigned iRoot = 0; iRoot < numRoots; iRoot++)
			roots[iRoot] = iRoot;
	}

	/*
	 * Release system resources
	 */
	virtual ~groupTree_t() {
		// check if entrypoints are compromised
		assert(this->N[0].gid == 0);
		assert(this->N[0].next == 0);
		assert(this->N[0].prev == 0);
		assert(this->N[0].weight == 0);
		for (uint32_t iNode = this->kstart; iNode < this->nstart; iNode++) {
			groupNode_t *pNode = this->N + iNode;
			assert(pNode->gid == iNode);
			assert(pNode->next == iNode);
			assert(pNode->prev == iNode);
			assert(pNode->weight == 1);
		}

		// release allocations if not mmapped
		if (allocFlags & ALLOCMASK_NODES)
			ctx.myFree("groupTree_t::N", this->N);
		if (allocFlags & ALLOCMASK_ROOTS)
			ctx.myFree("groupTree_t::roots", this->roots);
		if (allocFlags & ALLOCMASK_HISTORY)
			ctx.myFree("groupTree_t::history", this->history);
		if (allocFlags & ALLOCMASK_INDEX) {
			ctx.myFree("groupTree_t::nodeIndex", this->nodeIndex);
			ctx.myFree("groupTree_t::nodeIndexVersion", this->nodeIndexVersion);
		}

		// release maps
		if (slotMap)
			freeMap(slotMap);
		if (slotVersion)
			freeMap(slotVersion);

		// release pools
		while (numPoolMap > 0)
			ctx.myFree("groupTree_t::nodeMap", pPoolMap[--numPoolMap]);
		while (numPoolVersion > 0)
			ctx.myFree("groupTree_t::versionMap", pPoolVersion[--numPoolVersion]);

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
		rawData          = NULL;
		fileHeader       = NULL;
		N                = NULL;
		roots            = NULL;
		history          = NULL;
		nodeIndex        = NULL;
		nodeIndexVersion = NULL;
		pPoolMap         = NULL;
		pPoolVersion     = NULL;
		slotMap          = NULL;
		slotVersion      = NULL;
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
	 * @date 2021-12-13 02:35:12
	 * 
	 * Update id to latest group id
	 * NOTE: id=IBIT means group under construction
	 */
	inline uint32_t updateToLatest(uint32_t id) const {
		while (id != this->N[id].gid)
			id = this->N[id].gid;
		return id;
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
			pMap = (uint32_t *) ctx.myAlloc("groupTree_t::versionMap", maxNodes, sizeof *pMap);
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
	 * Returned map is guaranteed to contain values < version.
	 * Write only values <= version.
	 * 
	 * NOTE: caller needs clear map on `mapVersionNr` wraparound
	 *
	 * @return {uint32_t*} - Uninitialised map for versioned memory id's
	 */
	versionMemory_t *allocVersion(void) {
		versionMemory_t *pVersion;

		if (numPoolVersion > 0) {
			// get first free version map
			pVersion = pPoolVersion[--numPoolVersion];
		} else {
			// allocate new map
			pVersion = (versionMemory_t *) ctx.myAlloc("groupTree_t::versionMap", maxNodes + sizeof(versionMemory_t), sizeof(*pVersion));
			pVersion->version   = 0;
			pVersion->numMemory = maxNodes;
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
	void freeVersion(versionMemory_t *&pVersion) {
		if (numPoolVersion >= MAXPOOLARRAY)
			ctx.fatal("context.h:MAXPOOLARRAY too small\n");

		// store the current root in Version
		pPoolVersion[numPoolVersion++] = pVersion;
		pVersion = NULL;
	}

	/*
	 * @date 2021-11-10 18:27:04
	 * 
	 * Compare a node (lhs) with sid/slots/weight (rhs)
	 * 
	 * return:
	 *      -1 L < R
	 *       0 L = R
	 *      +1 L > R
	 */
	int compare(uint32_t lhs, uint32_t rhsSid, const uint32_t *rhsSlots, const double &weight) const {
		const groupNode_t *pLhs = this->N + lhs;

		// left-hand-side must be latest in latest group
		assert(pLhs->gid == IBIT || this->N[pLhs->gid].gid == pLhs->gid);

		/*
		 * Compare sid
		 */

		int cmp = (int) pLhs->sid - (int) rhsSid;
		if (cmp != 0)
			return cmp;

		/*
		 * Compare weight
		 */
#if 1 // todo: wait until after ucList
		if (pLhs->weight < weight)
			return -1;
		if (pLhs->weight > weight)
			return +1;
#endif

		/*
		 * compare slots
		 */
		unsigned numPlaceholder = db.signatures[rhsSid].numPlaceholder;

		// lhs must be latest
		for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
			uint32_t lid = pLhs->slots[iSlot];
			if (this->N[lid].gid != lid)
				return +1;
		}

		for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
			uint32_t lid = pLhs->slots[iSlot];
			uint32_t rid = rhsSlots[iSlot];

			// right-hand-side must be latest
			assert(this->N[rid].gid == rid);

			// is there a difference
			cmp = (int) lid - (int) rid;
			if (cmp != 0)
				return cmp;
		}

		return 0;
	}

	/*
	 * @date 2021-11-10 00:05:51
	 * 
	 * Add node to list
	 */
	inline void linkNode(uint32_t headId, uint32_t nodeId) const {

		assert(headId != nodeId);
		assert(headId >= this->nstart);
		assert(nodeId >= this->nstart);

		groupNode_t *pHead = this->N + headId;
		groupNode_t *pNode = this->N + nodeId;

		uint32_t headIdNext = pHead->next;
		uint32_t nodeIdLast = pNode->prev;

		this->N[headIdNext].prev = nodeIdLast;
		this->N[nodeIdLast].next = headIdNext;

		pHead->next = nodeId;
		pNode->prev = headId;
	}

	/*
	 * @date 2021-11-10 00:38:00
	 */
	inline void unlinkNode(uint32_t nodeId) const {
		groupNode_t *pNode = this->N + nodeId;

		uint32_t    headId = pNode->prev;
		groupNode_t *pHead = this->N + headId;

		this->N[pNode->next].prev = headId;
		pHead->next               = pNode->next;

		pNode->next = pNode->prev = nodeId;
	}

	/*
	 * @date 2021-05-13 00:38:48
	 *
	 * Lookup a node.
	 * 
	 * NOTE: About deleting entries.
	 * 
	 * An entry is invalidated by changing one of it's key values.
	 * This will cause the key CRC to change, making it highly likely to fail key matching.
	 * There are pathological situations that a bucket overflow jump "might" jump to the deleted record.
	 *
	 * Zeroing the index entry will break bucket overflows, hiding entries from the index.
	 * To keep overflow consistency, use the reserved `IDDELETED`.
	 * However, using it will pollute the index and might break the assumption "number of records < number of index entries"
	 * Breaking will cause the overflow jumping to loop forever.
	 * 
	 * todo: count the number of used `IDDELETED` and rebuild index when there are too many 
	 */
	inline uint32_t lookupNode(uint32_t sid, const uint32_t slots[]) const {
		ctx.cntHash++;

		// starting position
		uint32_t crc32 = 0;
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(sid));
		for (unsigned iSlot = 0; iSlot < MAXSLOTS && slots[iSlot]; iSlot++)
			__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(slots[iSlot]));

		uint32_t ix   = (uint32_t) (crc32 % this->nodeIndexSize);
		uint32_t bump = ix;
		if (!bump) bump++;

		for (;;) {
			ctx.cntCompare++;
			if (this->nodeIndexVersion[ix] != this->nodeIndexVersionNr) {
				// let caller finalise index
				return ix;
			}

			if (this->nodeIndex[ix] != db.IDDELETED) {
				const groupNode_t *pNode = this->N + this->nodeIndex[ix];
				assert(MAXSLOTS == 9);

				if (pNode->sid == sid &&
				    pNode->slots[0] == slots[0] && pNode->slots[1] == slots[1] && pNode->slots[2] == slots[2] &&
				    pNode->slots[3] == slots[3] && pNode->slots[4] == slots[4] && pNode->slots[5] == slots[5] &&
				    pNode->slots[6] == slots[6] && pNode->slots[7] == slots[7] && pNode->slots[8] == slots[8])
					return ix;
			}

			ix += bump;
			if (ix >= this->nodeIndexSize)
				ix -= this->nodeIndexSize;
		}
		assert(0);
		return 0;
	}

	/*
	 * @date 2021-05-13 00:44:53
	 *
	 * Create a new node
	 */
	inline uint32_t newNode(uint32_t sid, const uint32_t slots[], const double weight) {
		uint32_t nid = this->ncount++;

		assert(nid < maxNodes);
		assert(MAXSLOTS == 9);

		if ((ctx.flags & context_t::MAGICMASK_PARANOID) && sid != db.SID_SELF) {
			unsigned numPlaceholder = db.signatures[sid].numPlaceholder;
			// may not be zero
			assert(numPlaceholder < 1 || slots[0] != 0);
			assert(numPlaceholder < 2 || slots[1] != 0);
			assert(numPlaceholder < 3 || slots[2] != 0);
			assert(numPlaceholder < 4 || slots[3] != 0);
			assert(numPlaceholder < 5 || slots[4] != 0);
			assert(numPlaceholder < 6 || slots[5] != 0);
			assert(numPlaceholder < 7 || slots[6] != 0);
			assert(numPlaceholder < 8 || slots[7] != 0);
			assert(numPlaceholder < 9 || slots[8] != 0);
			// test referencing to group headers
			assert(N[slots[0]].gid == slots[0]);
			assert(N[slots[1]].gid == slots[1]);
			assert(N[slots[2]].gid == slots[2]);
			assert(N[slots[3]].gid == slots[3]);
			assert(N[slots[4]].gid == slots[4]);
			assert(N[slots[5]].gid == slots[5]);
			assert(N[slots[6]].gid == slots[6]);
			assert(N[slots[7]].gid == slots[7]);
			assert(N[slots[8]].gid == slots[8]);
			// they may not be orphaned
			assert(slots[0] < nstart || N[slots[0]].next != slots[0]);
			assert(slots[1] < nstart || N[slots[1]].next != slots[1]);
			assert(slots[2] < nstart || N[slots[2]].next != slots[2]);
			assert(slots[3] < nstart || N[slots[3]].next != slots[3]);
			assert(slots[4] < nstart || N[slots[4]].next != slots[4]);
			assert(slots[5] < nstart || N[slots[5]].next != slots[5]);
			assert(slots[6] < nstart || N[slots[6]].next != slots[6]);
			assert(slots[7] < nstart || N[slots[7]].next != slots[7]);
			assert(slots[8] < nstart || N[slots[8]].next != slots[8]);
			// Single occurrences only
			assert(slots[1] == 0 || (slots[1] != slots[0]));
			assert(slots[2] == 0 || (slots[2] != slots[0] && slots[2] != slots[1]));
			assert(slots[3] == 0 || (slots[3] != slots[0] && slots[3] != slots[1] && slots[3] != slots[2]));
			assert(slots[4] == 0 || (slots[4] != slots[0] && slots[4] != slots[1] && slots[4] != slots[2] && slots[4] != slots[3]));
			assert(slots[5] == 0 || (slots[5] != slots[0] && slots[5] != slots[1] && slots[5] != slots[2] && slots[5] != slots[3] && slots[5] != slots[4]));
			assert(slots[6] == 0 || (slots[6] != slots[0] && slots[6] != slots[1] && slots[6] != slots[2] && slots[6] != slots[3] && slots[6] != slots[4] && slots[6] != slots[5]));
			assert(slots[7] == 0 || (slots[7] != slots[0] && slots[7] != slots[1] && slots[7] != slots[2] && slots[7] != slots[3] && slots[7] != slots[4] && slots[7] != slots[5] && slots[7] != slots[6]));
			assert(slots[8] == 0 || (slots[8] != slots[0] && slots[8] != slots[1] && slots[8] != slots[2] && slots[8] != slots[3] && slots[8] != slots[4] && slots[8] != slots[5] && slots[8] != slots[6] && slots[8] != slots[7]));
		}

		if (nid > maxNodes - 10) { // 10 is arbitrary
			fprintf(stderr, "[OVERFLOW overflowGroup=%u]\n", this->overflowGroup);
			printf("{\"error\":\"overflow\",\"maxnode\":%d,\"group\":%u}\n", maxNodes, this->overflowGroup);
			assert(0);
		}

		groupNode_t *pNode = this->N + nid;

		pNode->gid      = IBIT;
		pNode->next     = nid;
		pNode->prev     = nid;
		pNode->oldId    = 0;
		pNode->weight   = weight;
		pNode->hiSlotId = 0;
		pNode->sid      = sid;

		// assign slots
		for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
			uint32_t id = slots[iSlot];
			pNode->slots[iSlot] = id;

			// update max
			if (id > pNode->hiSlotId)
				pNode->hiSlotId = id;
		}

		/*
		 * @date 2022-01-26 15:54:07
		 * For 1n9, do an exact node count
		 */
		if (db.signatures[sid].size == 1) {
			versionMemory_t *pVersion   = allocVersion();
			uint32_t        thisVersion = pVersion->nextVersion();

			uint32_t iGroup = 0;
			uint32_t count = 1;

			if (slots[0] >= this->nstart) {
				if (slots[0] > iGroup)
					iGroup = slots[0];
				pVersion->mem[slots[0]] = thisVersion;
			}

			if (slots[1] >= this->nstart) {
				if (slots[1] > iGroup)
					iGroup = slots[1];
				pVersion->mem[slots[1]] = thisVersion;
			}

			if (slots[2] != 0 && slots[2] >= this->nstart) {
				if (slots[2] > iGroup)
					iGroup = slots[2];
				pVersion->mem[slots[2]] = thisVersion;
			}

			while (iGroup >= this->nstart) {

				if (this->N[iGroup].gid != iGroup || pVersion->mem[iGroup] != thisVersion) {
					iGroup--;
					continue; // not a group header
				}

				count++;

				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					groupNode_t *pN = this->N + iNode;

					if (db.signatures[pN->sid].size != 1)
						continue; // not 1n9

					if (pVersion->mem[pN->slots[0]] != thisVersion)
						pVersion->mem[pN->slots[0]] = thisVersion;
					if (pVersion->mem[pN->slots[1]] != thisVersion)
						pVersion->mem[pN->slots[1]] = thisVersion;
					if (pN->slots[2] && pVersion->mem[pN->slots[2]] != thisVersion)
						pVersion->mem[pN->slots[2]] = thisVersion;
				}

				iGroup--;
			}

			pNode->weight = count;

			freeVersion(pVersion);
		}

		return nid;
	}

	/*
	 * @date 2021-12-21 20:22:46
	 * 
	 * Context containing resources for a group under construction.
	 * 
	 * @date 2022-01-20 23:56:28
	 * 
	 * With regard to the champion index:
	 * 
	 * `groupTree_t::updateGroup()` and `groupTree_t::addToGroup()` will exit with a valid index
	 * `groupTree_t::addBasicNode()` and `groupTree_t::addToGroup()` require a valid index on entry
	 * 
	 * @date 2022-01-16 14:25:48
	 * (taken from deleted `addToGroup()`)
	 *
	 * Layers are groups under construction.
	 * The naming is chosen because with recursive calling a stack of such groups can arise.
	 * Layers can be chained to bridge recursive calls and aid in locking and structural loop detection.
	 *
	 * Groups under construction try to delay the allocation of a group id as long as possible.
	 * The idea being that creating alternatives (through `expandSignature()`) might find similar existing groups that can be reused and merged,
	 *
	 * Delaying the creation of group ids is also important for self-collapse detection.
	 * As long as the nodes under construction cannot be referenced, they cannot be used to create dependency loops.
	 * If, after creating all Cartesian products, the group is still under construction, then a new group id is assigned which is guarenteed loop free.
	 *
	 * Nested calls might (from their context) find a match with a node under construction.
	 * In such cases, they grab and merge the pending nodes into their own group.
	 * From a caller point of view, this is an endpint-collapse and processed accordingly.
	 *
	 * NOTE: `layer.gid` lags behind the actual group id the nodes are member of.
	 *
	 * NOTE: must return either a collapse or a node id
	 */

	struct groupLayer_t {
		/*
		 * Tree owning the resources 
		 */
		groupTree_t &tree;

		/*
		 * Link to previous scope 
		 * 
		 * @date 2021-12-23 01:51:20
		 * 
		 * Mainly for debugging, 
		 * Possibly also to detect self-collapse
		 * Might also be related to disabling generation of nodes
		 * silently ignore if requested gid is already under construction.
		 */
		groupLayer_t *pPrevious;

		/*
		 * Group id of current group under construction. 
		 * Also used as locking guard and to authenticate `pChampionMap[]` 
		 */
		uint32_t gid;

		/*
		 * @date 2021-11-04 02:08:51
		 * 
		 * Second step: create Cartesian products of Q/T/F group lists
		 * 
		 * @date 2022-01-15 23:38:01
		 * 
		 * Keep all newly created nodes (with gid==IBIT) in a private list.
		 * This list does not have a header as that would imply the creation of `this->N[IBIT].gid = IBIT`.
		 * If some deeper component finds and wants to merge with one of these nodes, it is attempting an end-point collapse.
		 * After the loop, any nodes still in the list get relocated to ta new group.
		 * 
		 * This is necessary as the component  `addBasicNode()` within `expandSignature() will perform an `resolveForwards()`,
		 *  and if the nodes under construction would have an actual group, the unrolling might cause group re-creation,
		 *  which in turn outdates gid, which will trigger further resolving, which in rare cases will loop forever.
		 *  
		 * This routine has many exit points, it is possible that the list might get orphaned on exit,
		 * in which case, the list will remain dormant with gid=IBIT until it is claimed by some future call.
		 */
		uint32_t ucList; // hidden list of under construction nodes. 

		/*
		 * Champion index.
		 * Primary by sid unique, secondary by min(weight) 
		 * To instantly find node with identical sids so they can challenge for better/worse.
		 * Nodes losing a challenge are orphaned.
		 */
		uint32_t        *pChampionMap;
		versionMemory_t *pChampionVersion;

		/*
		 * Lowest weight across all nodes, will be written when flushed
		 */
		double loWeight;

		/*
		 * Highest slot id across all nodes, will be written when flushed
		 */
		double hiSlotId;

		/*
		 * @date 2021-12-21 20:39:38
		 * 
		 * Constructor
		 */
		groupLayer_t(groupTree_t &tree, groupLayer_t *pPrevious) : tree(tree), pPrevious(pPrevious) {

			gid              = IBIT;
			ucList           = IBIT;
			pChampionMap     = tree.allocMap();
			pChampionVersion = tree.allocVersion();
			loWeight         = 1.0 / 0.0; // + inf
			hiSlotId         = 0;

			// bump version
			pChampionVersion->nextVersion();
		}

		/*
		 * @date 2021-12-21 20:48:56
		 * 
		 * Destructor
		 */
		~groupLayer_t() {
			tree.freeMap(pChampionMap);
			tree.freeVersion(pChampionVersion);
		}

		/*
		 * @date 2021-12-21 22:29:41
		 * 
		 * Find sid.
		 * Return IBIT if not found
		 */
		inline uint32_t findChampion(uint32_t sid) const {
			if (pChampionVersion->mem[sid] != pChampionVersion->version)
				return IBIT; // index entry not set

			uint32_t nid = pChampionMap[sid];
			if (tree.N[nid].next == nid)
				return IBIT; // node was orphaned

			assert(tree.N[nid].gid == this->gid); // node/group must match (which might be IBIT)
			return nid;
		}
	};

	/*
	 * @date 2022-01-25 21:28:28
	 * 
	 * Orphan a node
	 * returns the previous node in list
	 * NOTE: set gid (if any) BEFORE calling
	 */
	uint32_t  orphanNode(groupLayer_t &layer, uint32_t rhs) {

		groupNode_t *pNode = this->N + rhs;

		/*
		 * remove orphan as first of under-construction list 
		 */
		if (layer.ucList == rhs) {
			layer.ucList = pNode->next;

			// erase list if it was only node
			if (layer.ucList == rhs)
				layer.ucList = IBIT;
		}

		/*
		 * Orphan node
		 */
		uint32_t prevId = pNode->prev;
		unlinkNode(rhs);

		/*
		 * Orphans must forward to something
		 */
		if (pNode->gid == IBIT) {
			if (layer.gid != IBIT) {
				// use group id
				pNode->gid = layer.gid;

			} else if (layer.ucList != IBIT && this->N[layer.ucList].gid != IBIT) {
				// use pending adoption
				pNode->gid = this->N[layer.ucList].gid;

			} else {
				// invalidate sid so the index will not find it
				pNode->sid = 0;
			}

			return prevId;
		}

		/*
		 * Need to update loWeight/hiSlotId?
		 */


		if (layer.gid != IBIT) {
			groupNode_t *pGroup = this->N + layer.gid;

			if (pGroup->weight == pNode->weight || pGroup->hiSlotId == pNode->hiSlotId) {
				double   gWeight   = 1.0 / 0.0; // +inf
				uint32_t gHiSlotId = 0;

				for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;

					// must have valid weight
					if (pNode->weight < gWeight)
						gWeight   = pNode->weight;
					if (pNode->hiSlotId > gHiSlotId)
						gHiSlotId = pNode->hiSlotId;
				}

				// NOTE: group might be empty
				pGroup->weight   = gWeight;
				pGroup->hiSlotId = gHiSlotId;
			}
		}

		return prevId;
	}

	/*
	 * @date 2022-01-05 17:29:57
	 * 
	 * Scan group and build indices
	 */
	void rebuildLayer(groupLayer_t &layer) {

		// Attached to group?
		if (layer.gid == IBIT) {
			// no

			/*
			 * @date 2022-01-25 22:05:38
			 * If under-construction list was adopted, then attach to its group
			 */

			if (layer.ucList != IBIT && this->N[layer.ucList].gid != IBIT) {
				// attach
				layer.gid    = this->N[layer.ucList].gid;
				layer.ucList = IBIT;
			} else {
				// nothing to do
				return; // no
			}
		}

		assert(layer.ucList == IBIT); // when gid set, ucList is not allowed

		// update to latest
		layer.gid      = updateToLatest(layer.gid);
		layer.loWeight = 1.0 / 0.0; // + inf
		layer.hiSlotId = 0;

		// Is gid an unmodifiable entrypoint
		if (layer.gid < this->nstart) {
			layer.loWeight = (layer.gid == 0) ? 0 : 1;
			layer.hiSlotId = layer.gid;
			return; // yes
		}

		assert(this->N[layer.gid].next != layer.gid); // may not be orphaned

		// bump versioned memory
		layer.pChampionVersion->nextVersion();

		// scan group for initial champion and minimum weight
		for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
			groupNode_t *pNode = this->N + iNode;

			// load initial champion index
			layer.pChampionMap[pNode->sid]          = iNode;
			layer.pChampionVersion->mem[pNode->sid] = layer.pChampionVersion->version;

			// update weight
			if (pNode->weight < layer.loWeight)
				layer.loWeight = pNode->weight;
			// update hiSlotId
			if (pNode->hiSlotId > layer.hiSlotId)
				layer.hiSlotId = pNode->hiSlotId;
		}

		// update weight
		assert(layer.loWeight < 1.0 / 0.0);
		this->N[layer.gid].weight   = layer.loWeight;
		this->N[layer.gid].hiSlotId = layer.hiSlotId;
	}

	/*
	 * @date 2022-01-21 21:35:02
	 * 
	 * Add an existing node/orphan/group to layer.
	 * 
	 * There are 3 situations: `layer.gid`, `N[layer.ucList].gid`, `N[rhs].gid`
	 * `IBIT` signals under-construction, which means that the nodes are safe (have not been referenced) and will not create loops
	 * 
	 * NOTE: gid can be an entrypoint and should cause a group collapse.
	 * NOTE: layer&rhs can refer to the same group.
	 *       `mergeGroups() is entrypoint and duplicate aware.
	 *       
	 * layer | ucList | rhs | action
	 * ------+--------+-----+-------
	 * IBIT      -     IBIT   [0] ignore
	 * IBIT      -      id    [1] connect to rhs
	 * IBIT     IBIT   IBIT   [2] ignore
	 * IBIT     IBIT    id    [3] connect to rhs & merge ucList
	 * IBIT      id    IBIT   [4] connect to ucList & adopt rhs
	 * IBIT      id     id    [5] connect to ucList & merge rhs
	 *  id       -     IBIT   [6] adopt rhs
	 *  id       -      id    [7] merge rhs
	 *  id      IBIT    *     [8] ucList violation
	 *  id       id     *     [9] ucList violation
	 *  
	 *  Only [5] and [6] merge two different groups and need `resolveForwards()`.
	 *  
	 *  [4] and [6] adopt/steal the under-construction list of another layer
	 *  [4] and [5] have their under-construction list adopted/stolen
	 */
	void addOldNode(groupLayer_t &layer, uint32_t rhs) {

		assert(rhs != IBIT);
		assert(this->N[rhs].gid != IBIT); // node must be assigned to a group (old)

		/*
		 * Was ucList adopted?
		 * [4] connect to ucList & adopt rhs -> [6]
		 * [5] connect to ucList & merge rhs -> [7]
		 */
		if (layer.gid == IBIT && layer.ucList != IBIT && this->N[layer.ucList].gid != IBIT) {
			// yes, connect to it

			layer.gid    = updateToLatest(layer.ucList);
			layer.ucList = IBIT;

			// NOTE: [6] does `updateGroup()` and [7] does `mergeGroups()`. No need to rebuild layer
		}

		/*
		 * Is layer connected to gid?
		 *  [6] adopt rhs
		 *  [7] merge rhs
		 *  [8] ucList violation
		 *  [9] ucList violation
		 */
		if (layer.gid != IBIT) {
			// yes
			assert(this->N[layer.gid].gid == layer.gid); // must be latest
			assert(layer.gid >= this->nstart); // entrypoints should have been handled
			assert(layer.ucList == IBIT); // [8]/[9] list should have been merged

			// how to connect
			if (this->N[rhs].gid == IBIT) {
				// [6] adopt rhs under-construction

				// append rhs to group
				linkNode(this->N[layer.gid].prev, rhs);

				// update id's. 
				for (uint32_t iNode = rhs; iNode != this->N[iNode].gid; iNode = this->N[iNode].next)
					this->N[iNode].gid = layer.gid;

				// cleanup
				updateGroup(layer, NULL, /*allowForward=*/true);

			} else {
				uint32_t rhsLatest = updateToLatest(rhs);
				if (layer.gid != rhsLatest) {
					// [7] merge other group

					mergeGroups(layer, rhsLatest);
					if (layer.gid >= this->nstart)
						updateGroup(layer, NULL, /*allowForward=*/true);

				} else {
					// [7] refresh node of same group
					groupNode_t *pNode = this->N + rhs;

					uint32_t champion = layer.findChampion(pNode->sid);

					if (champion != IBIT && champion != rhs) {
						int cmp = this->compare(champion, pNode->sid, pNode->slots, pNode->weight);

						if (cmp < 0) {
							// champion is better, silently ignore
							return; // silently ignore

						} else if (cmp > 0) {
							// rhs is better (lighter weight), dismiss champion 
							orphanNode(layer, champion);

						} else if (cmp == 0) {
							assert(champion == rhs); // should have been detected
						}
					}

					// set as new champion
					layer.pChampionMap[pNode->sid]          = rhs;
					layer.pChampionVersion->mem[pNode->sid] = layer.pChampionVersion->version;
				}
			}

			return;
		}

		/*
		 * Is rhs under-construction, possibly a duplicate?
		 * 
		 * [0] ignore
		 * [2] ignore
		 */
		if (this->N[rhs].gid == IBIT) {
			// yes, ignore
			return;
		}

		/*
		 * [1] connect to rhs
		 * [3] connect to rhs & merge ucList
		 */

		// connect to rhs gid
		layer.gid = updateToLatest(rhs);

		if (layer.gid < this->nstart) {
			// entrypoint collapse

			// orphan all nodes waiting construction and redirect to entrypoint
			while (layer.ucList != IBIT) {
				// get id
				uint32_t    nid    = layer.ucList;
				groupNode_t *pNode = this->N + nid;

				// unlink
				if (pNode->next == nid) {
					// this is last
					layer.ucList = IBIT;
				} else {
					// unlink from list
					layer.ucList = pNode->next;
					unlinkNode(nid);
				}

				// redirect to entrypoint
				pNode->gid = layer.gid;
			}

		} else if (layer.ucList == IBIT) {
			// [1] connect to rhs
			rebuildLayer(layer);

		} else {
			// [3] connect to rhs & merge ucList
			assert(this->N[layer.ucList].gid == IBIT); //  [4]/[5] already handled

			// reassign under-construction list to group
			linkNode(this->N[layer.gid].prev, layer.ucList);

			// update id's. 
			for (uint32_t iNode = layer.ucList; iNode != this->N[iNode].gid; iNode = this->N[iNode].next)
				this->N[iNode].gid = layer.gid;

			layer.ucList = IBIT;

			updateGroup(layer, NULL, /*allowForward=*/ true);
		}
	}

	/*
	 * @date 2022-01-25 19:03:49
	 * 
	 * Collapse a group
	 */

	void addCollapse(groupLayer_t &layer, uint32_t rhs) {
		assert(rhs != IBIT);
		assert(this->N[rhs].gid != IBIT); // node must be assigned to a group (old)

		uint32_t rhsLatest = updateToLatest(rhs);

		assert(layer.gid != rhsLatest); // may not self-collapse

		/*
		 * collapse the under-constructon list
		 */
		if (layer.ucList != IBIT) {

			// orphan all nodes waiting construction and redirect to endpoint
			while (layer.ucList != IBIT) {
				// get id
				uint32_t    nid    = layer.ucList;
				groupNode_t *pNode = this->N + nid;

				// unlink
				if (pNode->next == nid) {
					// this is last
					layer.ucList = IBIT;
				} else {
					// unlink from list
					layer.ucList = pNode->next;
					unlinkNode(nid);
				}

				// redirect to entrypoint
				pNode->gid = rhsLatest;
			}
		}

		/*
		 * Collapse the group
		 */
		if (layer.gid != IBIT) {

			// orphan all nodes and redirect to endpoint
			for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode = this->N + iNode;

				// redirect to entrypoint
				pNode->gid = rhsLatest;

				// orphan node
				iNode = orphanNode(layer, iNode); // returns previous node
			}

			// redirect group header
			this->N[layer.gid].gid = rhsLatest;
		}

		// redirect layer
		layer.gid = rhsLatest;
		if (rhsLatest >= this->nstart)
			rebuildLayer(layer);
	}

	/*
	 * @date 2022-01-21 21:35:02
	 * 
	 * Add a newly created node to layer.
	 * 
	 * There are 3 situations: `layer.gid`, `N[layer.ucList].gid`
	 * `IBIT` signals under-construction, which means that the nodes are safe (have not been referenced) and will not create loops
	 * 
	 * NOTE: gid can be an entrypoint and should cause a group collapse.
	 *       
	 * layer | ucList | action
	 * ------+--------+-------
	 * IBIT      -      [0] create ucList
	 * IBIT     IBIT    [1] append to ucList
	 * IBIT      id     [2] connect to ucList & append
	 *  id       -      [3] append layer.gid
	 *  id      IBIT    [4] ucList violation
	 *  id       id     [5] ucList violation
	 * 
	 * [2] has had its under-construction list adopted/stolen
	 */
	void addNewNode(groupLayer_t &layer, uint32_t nix, uint32_t nid) {

		assert(nid != IBIT);
		assert(this->N[nid].gid == IBIT); // node must be new

		groupNode_t *pNode = this->N + nid;

#if 1
		// DEACTIVATE `ucList` 			
		if (layer.gid == IBIT) {
			// create group header
			uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zerod
			assert(selfSlots[MAXSLOTS - 1] == 0);

			this->gcount++;
			uint32_t gid = this->newNode(db.SID_SELF, selfSlots, 1.0 / 0.0);
			assert(gid == this->N[gid].slots[0]);
			this->N[gid].gid = gid;

			// attach to layer
			layer.gid = gid;

			// bump versioned memory
			layer.pChampionVersion->nextVersion();
		}
#endif

		if (layer.gid == IBIT) {

			if (layer.ucList == IBIT) {
				// [0] create ucList

				layer.ucList = nid;

			} else if (this->N[layer.ucList].gid == IBIT) {
				// [1] append to ucList

				linkNode(this->N[layer.ucList].prev, nid);

			} else {
				// [2] connect to ucList & append
				layer.gid    = updateToLatest(layer.ucList);
				layer.ucList = IBIT;

				// set gid
				pNode->gid = layer.gid;

				// append rhs to group
				linkNode(this->N[layer.gid].prev, nid);

				rebuildLayer(layer);
			}

		} else {
			// [3] append layer.gid
			assert(this->N[layer.gid].gid == layer.gid); // layer must be latest
			assert(layer.gid >= this->nstart); // entrypoints should have been handled
			assert(layer.ucList == IBIT); // [4]/[5] list should have been merged

			// set gid
			pNode->gid = layer.gid;

			// append rhs node to group
			linkNode(this->N[layer.gid].prev, nid);
		}

		// add node to index
		this->nodeIndex[nix]        = nid;
		this->nodeIndexVersion[nix] = this->nodeIndexVersionNr;

		// add to champion index
		layer.pChampionMap[pNode->sid]          = nid;
		layer.pChampionVersion->mem[pNode->sid] = layer.pChampionVersion->version;

		if (pNode->weight < layer.loWeight)
			layer.loWeight = pNode->weight;
		if (pNode->hiSlotId > layer.hiSlotId)
			layer.hiSlotId = pNode->hiSlotId;
	}

	/*
	 * @date 2022-01-22 22:09:22
	 * 
	 * Convert under-construction to newly created group
	 */
	void flushLayer(groupLayer_t &layer) {

		/*
		 * Is there a group header?
		 */
		if (layer.gid != IBIT) {
			// yes, finalise

			/*
			 * @date 2022-01-25 20:34:50
			 * If a referenced group merges, it's id goes lower.
			 * This leaves `hiSlotid` unchanged, making it undetected.
			 * This optimisation needs a different mechanism
			 */
			if (true || layer.hiSlotId > layer.gid) {
				resolveForwards(layer, layer.gid);
				// gid might have outdated
				layer.gid = updateToLatest(layer.gid);
			}

		} else if (layer.ucList != IBIT) {
			// no, create group

			if (this->N[layer.ucList].gid != IBIT) {
				// attach to existing group
				layer.gid = updateToLatest(layer.ucList);
				layer.ucList = IBIT;

			} else {
				// create new group

				// create group header
				uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zerod
				assert(selfSlots[MAXSLOTS - 1] == 0);

				this->gcount++;
				uint32_t gid = this->newNode(db.SID_SELF, selfSlots, /*weight=*/ 1.0 / 0.0); // +inf
				assert(gid == this->N[gid].slots[0]);
				this->N[gid].gid = gid;

				layer.gid = gid;
			}
		}

		assert(this->N[layer.gid].gid == layer.gid); // must be latest

		/*
		 * Is there an under-construction list?
		 */

		if (layer.ucList != IBIT) {
			// yes, need to combine with under-construction list 

			// how to merge?
			if (this->N[layer.ucList].gid != IBIT) {
				// regular merge
				uint32_t ucLatest = updateToLatest(layer.ucList);
				layer.ucList = IBIT;

				assert(layer.gid != ucLatest);
				mergeGroups(layer, ucLatest);
				resolveForwards(layer, layer.gid);

			} else if (layer.gid < this->nstart) {
				assert(0); // does this happen
				// orphan/redirect under-construction list to entrypoint

				while (layer.ucList != IBIT) {
					// get id
					uint32_t    nid    = layer.ucList;
					groupNode_t *pNode = this->N + nid;

					// unlink
					if (pNode->next == nid) {
						// this is last
						layer.ucList = IBIT;
					} else {
						// unlink from list
						layer.ucList = pNode->next;
						unlinkNode(nid);
					}

					// redirect to entrypoint
					pNode->gid = layer.gid;
				}

			} else {
				// relocate under-construction to group and update

				// reassign list to group
				linkNode(this->N[layer.gid].prev, layer.ucList);

				// update id's. 
				for (uint32_t iNode = layer.ucList; iNode != this->N[iNode].gid; iNode = this->N[iNode].next)
					this->N[iNode].gid = layer.gid;

				// empty list
				layer.ucList = IBIT;

				// update and de-dup
				updateGroup(layer, NULL, /*allowForward=*/true);
			}
		}

		if (layer.gid >= this->nstart) {
			/*
			 * @date 2022-01-24 18:46:11
			 * De-throning champions will keep weight correct, but might invalidate the highest slot.
			 * Sadly, need to re-calculate
			 */

			double   loWeight = 1.0 / 0.0; // + inf
			uint32_t hiSlotId = 0;

			for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode = this->N + iNode;

				if (pNode->weight < loWeight)
					loWeight = pNode->weight;
				if (pNode->hiSlotId > hiSlotId)
					hiSlotId = pNode->hiSlotId;
			}

			layer.loWeight = loWeight;
			layer.hiSlotId = hiSlotId;

			// finalise group header
			groupNode_t *pHeader = this->N + layer.gid;
			pHeader->weight   = layer.loWeight;
			pHeader->hiSlotId = layer.hiSlotId;
		}
	}

	/*
	 * @date 2021-11-16 20:20:38
	 * 
	 * Construct slots based on Q/T/F and find matching signature.
	 * Ti must be 0/IBIT and may flip within this function
	 * Return sid+pFinal or 0 if no match found
	 * 
	 * @param {uint32_t}     gid     - group id under construction (may be IBIT) any reference to this is considered a fold.collapse
	 * @param {groupNode_t*} pNodeQ  - Q component of Cartesian product
	 * @param {groupNode_t*} pNodeTu - Tu component of Cartesian product
	 * @param {uint32_t}     Ti      - IBIT to indicate that Tu is inverted.
	 * @param {groupNode_t*} pNodeF  - F component of Cartesian product
	 * @param {uint32_t*}    pFinal  - Constructed slots ordered such that tid=0.
	 * @param {double*}      pWeight - Weight of all references including self
	 * @return {uint32_t}            - signature id, 0 if none found or folded..
	 */
	uint32_t constructSlots(groupLayer_t &layer, const groupNode_t *pNodeQ, const groupNode_t *pNodeT, uint32_t Ti, const groupNode_t *pNodeF, uint32_t *pFinal, double *pWeight) {

		/*
		 * @date 2021-11-05 01:32:20
		 * 
		 * Third step: Apply basic normalisation
		 * Any collapses will result in an early return with the current group being orphaned
		 */

		// bump versioned memory
		uint32_t thisVersion = ++slotVersionNr;
		if (thisVersion == 0) {
			// version overflow, clear
			memset(slotVersion, 0, this->maxNodes * sizeof(*slotVersion));

			thisVersion = ++slotVersionNr;
		}

		/*
		 * @date 2021-11-05 01:05:27
		 * 
		 * Fourth step: load product Q/T/F into slots and perform database lookup
		 * 
		 */

		// reassembly transforms
		char              slotsT[MAXSLOTS + 1];
		char              slotsF[MAXSLOTS + 1];
		// resulting slots containing gids
		uint32_t          slotsR[MAXSLOTS];
		// slotsR entries in use
		unsigned          nextSlot = 0;
		const signature_t *pSignature;

		/*
		 * Slot population as `groupTree_t` would do
		 */

		// NOTE: `slotsQ` is always `tid=0`, so `slotsQ[]` is not needed, load directly into `slotsR[]`.
		pSignature = db.signatures + pNodeQ->sid;
		for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			uint32_t endpoint = updateToLatest(pNodeQ->slots[iSlot]);
			assert(endpoint != 0);

			// is it an endpoint-collapse
			if (endpoint == layer.gid) {
				// yes
				pFinal[0] = endpoint;
				return db.SID_SELF;
			}

			// was it seen before
			if (slotVersion[endpoint] != thisVersion) {
				slotVersion[endpoint] = thisVersion;
				slotMap[endpoint]     = 'a' + nextSlot; // assign new placeholder
				slotsR[nextSlot]      = endpoint; // put endpoint in result
				nextSlot++;
			} else {
				/*
				 * @date 2021-12-09 03:02:42
				 * duplicate id in Q
				 * This because Q doesn't go through `lookupFwdTransform()` 
				 */
				return 0;
			}
		}

		pSignature = db.signatures + pNodeT->sid;
		for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			uint32_t endpoint = updateToLatest(pNodeT->slots[iSlot]);
			assert(endpoint != 0);

			// is it an endpoint-collapse
			if (endpoint == layer.gid) {
				// yes
				pFinal[0] = endpoint;
				return db.SID_SELF;
			}

			// was it seen before
			if (slotVersion[endpoint] != thisVersion) {
				if (nextSlot >= MAXSLOTS)
					return 0; // overflow
				slotVersion[endpoint] = thisVersion;
				slotMap[endpoint]     = 'a' + nextSlot;
				slotsR[nextSlot]      = endpoint;
				nextSlot++;
			}
			slotsT[iSlot] = (char) slotMap[endpoint];
		}
		slotsT[pSignature->numPlaceholder] = 0;

		// order slots
		if (pSignature->swapId)
			applySwapping(pNodeT->sid, slotsT);

		/*
		 * Lookup `patternFirst`
		 * 
		 * @date 2021-12-09 03:04:00
		 * Instead of doing complicated to detect duplicate latest in T, just check if the transform is valid
		 */

		uint32_t tidSlotT = db.lookupFwdTransform(slotsT);
		if (tidSlotT == IBIT) {
			this->cntOutdated++;
			return 0; // invalid slots (duplicate entries)
		}

		uint32_t ixFirst = db.lookupPatternFirst(pNodeQ->sid, pNodeT->sid ^ Ti, tidSlotT);
		uint32_t idFirst = db.patternFirstIndex[ixFirst];

		if (idFirst == 0)
			return 0; // not found

		/*
		 * Add `F` to slots
		 */

		pSignature = db.signatures + pNodeF->sid;
		for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			uint32_t endpoint = updateToLatest(pNodeF->slots[iSlot]);
			assert(endpoint != 0);

			// is it an endpoint-collapse
			if (endpoint == layer.gid) {
				// yes
				pFinal[0] = endpoint;
				return db.SID_SELF;
			}

			// was it seen before
			if (slotVersion[endpoint] != thisVersion) {
				if (nextSlot >= MAXSLOTS)
					return 0; // overflow
				slotVersion[endpoint] = thisVersion;
				slotMap[endpoint]     = 'a' + nextSlot;
				slotsR[nextSlot]      = endpoint;
				nextSlot++;
			}
			slotsF[iSlot] = (char) slotMap[endpoint];
		}
		slotsF[pSignature->numPlaceholder] = 0;

		// order slots
		if (pSignature->swapId)
			applySwapping(pNodeF->sid, slotsF);

		/*
		 * Lookup `patternSecond`
		 */

		uint32_t tidSlotF = db.lookupFwdTransform(slotsF);
		if (tidSlotF == IBIT) {
			this->cntOutdated++;
			return 0; // invalid slots (duplicate entries)
		}

		uint32_t ixSecond = db.lookupPatternSecond(idFirst, pNodeF->sid, tidSlotF);
		uint32_t idSecond = db.patternSecondIndex[ixSecond];

		if (idSecond == 0)
			return 0; // not found

		patternSecond_t *pSecond = db.patternsSecond + idSecond;

		/*
		 * @date 2021-11-05 02:42:24
		 * 
		 * Fifth step: Extract result out of `slotsR[]` and apply signature based endpoint swapping
		 * 
		 * NOTE: sid can also be SID_ZERO/SID_SELF
		 */

		pSignature = db.signatures + pSecond->sidR;
		const char *pTransformExtract = db.fwdTransformNames[pSecond->tidExtract];

		assert(nextSlot >= pSignature->numPlaceholder);

		// zero unused entries
		while (nextSlot < MAXSLOTS)
			slotsR[nextSlot++] = 0;

		// extract final slots and determine weight
		double weight = db.signatures[pSecond->sidR].size;
		for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			uint32_t id = slotsR[pTransformExtract[iSlot] - 'a'];
			assert(id != 0);

			pFinal[iSlot] = id;
			if (id)
				weight += this->N[id].weight;
		}
		for (unsigned iSlot = pSignature->numPlaceholder; iSlot < MAXSLOTS; iSlot++)
			pFinal[iSlot] = 0;

		/*
		 * Apply endpoint swapping
		 */
		if (pSignature->swapId)
			applySwapping(pSecond->sidR, pFinal);

		// don't forget (calculated) weight
		assert(weight < 1.0 / 0.0);
		*pWeight = weight;

		return pSecond->sidR;
	}

	/*
	 * @date 2021-11-08 00:00:19
	 * 
	 * `ab^c^`: is stored as `abc^^/[a/[c] ab^/[a b]]` which is badly ordered.
	 * Proper is: `abc^^/[a/[a] ab^/[b c]]`, but requires creation of `ab^[b c]`.
	 * 
	 * A suggested method to properly order is to take the sid/slot combo and re-create it using the signature, 
	 * implicitly creating better ordered components.
	 * 
	 * This might (and is expected to) create many duplicates.
	 * 
	 * @date 2021-11-11 23:08:10
	 * 
	 * NOTE:
	 *   the Cartesian product loop calls this function (low gid), which below adds intermediates (high gid)
	 *   These intermediates might be reused in the following tree evaluation, causing forward references, which is not allowed.
	 *   After each call to `addNormaliseNode()` verify bound and rebuild where necessary
	 *   
	 * @date 2021-11-18 21:37:50
	 * 
	 * Return IBIT if signature folded, caller should silently ignore alternative
	 * 
	 * @date 2021-11-19 01:24:33
	 * 
	 *   - re-evaluates the signature and slots
	 *   - there should be a group for every signature node (ids should be unique)
	 *   - merge and prune groups when alternatives are detected.
	 *   - create intermediate components
	 *   - And component referencing `gid` is considered a collapse to self.
	 *   
	 * @date 2022-01-23 19:18:32
	 * 
	 * Due to delayed group id creation, it is not always possible to return an id on success.
	 * On success return 0.
	 */
	uint32_t __attribute__((used)) expandSignature(groupLayer_t &layer, uint32_t sid, const uint32_t *pSlots, unsigned depth) {

		signature_t *pSignature    = db.signatures + sid;
		unsigned    numPlaceholder = pSignature->numPlaceholder;

		// group id must be latest
		assert(layer.gid == IBIT || layer.gid == this->N[layer.gid].gid);

		/*
		 * init
		 */

		unsigned numStack = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap(); // evaluation stack
		uint32_t *pMap    = allocMap(); // node id of intermediates

		// component sid/slots
		uint32_t cSid             = 0; // 0=error/folded
		uint32_t cSlots[MAXSLOTS] = {0}; // zero contents
		assert(cSlots[MAXSLOTS - 1] == 0);

		/*
		 * Load string
		 */
		for (const char *pattern = pSignature->name; *pattern; pattern++) {

			uint32_t Q, Tu, Ti, F;

			switch (*pattern) {
			case '0': //
				/*
				 * Push zero
				 */
				assert (numStack < maxNodes);
				pStack[numStack++] = 0;
				continue; // for

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

				assert (v >= this->nstart && v < nextNode);

				assert (numStack < maxNodes);
				pStack[numStack++] = pMap[v];
				continue; // for
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
				uint32_t v = (*pattern - 'a');

				assert(v < numPlaceholder);

				assert (numStack < maxNodes);
				pStack[numStack++] = pSlots[v];
				continue; // for

			}

			case '+': {
				// OR (appreciated)
				assert(numStack >= 2);

				F  = pStack[--numStack];
				Tu = 0;
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '>': {
				// GT (appreciated)
				assert(numStack >= 2);

				F  = 0;
				Tu = pStack[--numStack];
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '^': {
				// XOR/NE (appreciated)
				assert(numStack >= 2);

				F  = pStack[--numStack];
				Tu = F;
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '!': {
				// QnTF (appreciated)
				assert(numStack >= 3);

				F  = pStack[--numStack];
				Tu = pStack[--numStack];
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '&': {
				// AND (depreciated)
				assert(numStack >= 2);

				F  = 0;
				Tu = pStack[--numStack];
				Ti = 0;
				Q  = pStack[--numStack];
				break;
			}
			case '?': {
				// QTF (depreciated)
				assert(numStack >= 3);

				F  = pStack[--numStack];
				Tu = pStack[--numStack];
				Ti = 0;
				Q  = pStack[--numStack];
				break;
			}
			default:
				assert(0);
			} // end-switch

			/*
			 * Only arrive here when Q/T/F have been set 
			 */

			/*
			 * use the latest lists
			 */

			// get latest group
			Q  = updateToLatest(Q);
			Tu = updateToLatest(Tu);
			F  = updateToLatest(F);

			// did a deeper component merge groups that triggers an endpoint-collapse now?
			if (Q == layer.gid || Tu == layer.gid || F == layer.gid) {
				// yes

				freeMap(pStack);
				freeMap(pMap);
				return IBIT ^ layer.gid;
			}

			/*
			 * Perform normalisation
			 */

			/*
			 * Level 2 normalisation: single node rewrites
			 *
			 * appreciated:
			 *
			 *  [ 0] a ? !0 : 0  ->  a
			 *  [ 1] a ? !0 : a  ->  a ? !0 : 0
			 *  [ 2] a ? !0 : b                  "+" OR
			 *  [ 3] a ? !a : 0  ->  0
			 *  [ 4] a ? !a : a  ->  a ? !a : 0
			 *  [ 5] a ? !a : b  ->  b ? !a : b
			 *  [ 6] a ? !b : 0                  ">" GREATER-THAN
			 *  [ 7] a ? !b : a  ->  a ? !b : 0
			 *  [ 8] a ? !b : b                  "^" NOT-EQUAL/XOR
			 *  [ 9] a ? !b : c                  "!" QnTF
			 *
			 * depreciated:
			 *  [10] a ?  0 : 0 -> 0
			 *  [11] a ?  0 : a -> 0
			 *  [12] a ?  0 : b -> b ? !a : 0
			 *  [13] a ?  a : 0 -> a
			 *  [14] a ?  a : a -> a ?  a : 0
			 *  [15] a ?  a : b -> a ? !0 : b
			 *  [16] a ?  b : 0                  "&" AND
			 *  [17] a ?  b : a -> a ?  b : 0
			 *  [18] a ?  b : b -> b
			 *  [19] a ?  b : c                  "?" QTF
			 *
			  * ./eval --raw 'a0a!' 'a0b!' 'aaa!' 'aab!' 'aba!' 'abb!' 'abc!' 'a0a?' 'a0b?' 'aaa?' 'aab?' 'aba?' 'abb?' 'abc?'
			  *
			 */

			/*
			 * @date 2022-01-12 13:47:09
			 * NOTE: for cSid==SID_SELF, `nid` will hold the id
			 */
			uint32_t nid = 0;

			if (Q == 0) {
				// level-1 fold
				nid  = F;
				cSid = db.SID_SELF;
			} else if (Ti) {
				if (Tu == 0) {
					if (Q == F || F == 0) {
						// [ 0] a ? !0 : 0  ->  a
						// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
						nid  = Q;
						cSid = db.SID_SELF;
					} else {
						// [ 2] a ? !0 : b  -> "+" OR
						cSid = db.SID_OR;
						// swap
						if (Q > F) {
							uint32_t savQ = Q;
							Q             = F;
							F             = savQ;
						}
					}
				} else if (Q == Tu) {
					if (Q == F || F == 0) {
						// [ 3] a ? !a : 0  ->  0
						// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
						nid  = 0;
						cSid = db.SID_SELF;
					} else {
						// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
						// WARNING: Converted LESS-THAN
						Q    = F;
						F    = 0;
						cSid = db.SID_GT;
					}
				} else {
					if (Q == F || F == 0) {
						// [ 6] a ? !b : 0  ->  ">" GREATER-THAN
						// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
						F    = 0;
						cSid = db.SID_GT;
					} else if (Tu == F) {
						// [ 8] a ? !b : b  -> "^" NOT-EQUAL/XOR
						cSid = db.SID_NE;
						// swap
						if (Q > F) {
							uint32_t savQ = Q;
							Q             = F;
							F             = savQ;
							Tu            = savQ;
						}
					} else {
						// [ 9] a ? !b : c  -> "!" QnTF
						cSid = db.SID_QNTF;
					}
				}

			} else {

				if (Tu == 0) {
					if (Q == F || F == 0) {
						// [10] a ?  0 : 0 -> 0
						// [11] a ?  0 : a -> 0
						nid  = 0;
						cSid = db.SID_SELF;
					} else {
						// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
						// WARNING: Converted LESS-THAN
						Tu   = Q;
						Ti   = IBIT;
						Q    = F;
						F    = 0;
						cSid = db.SID_GT;
					}
				} else if (Q == Tu) {
					if (Q == F || F == 0) {
						// [13] a ?  a : 0 -> a
						// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
						nid  = Q;
						cSid = db.SID_SELF;
					} else {
						// [15] a ?  a : b -> a ? !0 : b -> "+" OR
						Tu   = 0;
						Ti   = IBIT;
						cSid = db.SID_OR;
						// swap
						if (Q > F) {
							uint32_t savQ = Q;
							Q             = F;
							F             = savQ;
						}
					}
				} else {
					if (Q == F || F == 0) {
						// [16] a ?  b : 0             "&" and
						// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
						F    = 0;
						cSid = db.SID_AND;
						// swap
						if (Q > Tu) {
							uint32_t savQ = Q;
							Q             = Tu;
							Tu            = savQ;
						}
					} else if (Tu == F) {
						// [18] a ?  b : b -> b        ENDPOINT
						nid  = F;
						cSid = db.SID_SELF;
					} else {
						// [19] a ?  b : c             "?" QTF
						cSid = db.SID_QTF;
					}
				}
			}

			if (cSid == db.SID_SELF) {
				// folding occurred and result is in nid

				// Push onto stack
				pStack[numStack++] = nid;
				pMap[nextNode++]   = nid;

			} else if (pattern[1]) {

				// allocate storage for scope
				groupLayer_t newLayer(*this, &layer);

				// endpoint collapse?
				if (layer.gid == Q || layer.gid == Tu || layer.gid == F) {
					// yes
					freeMap(pStack);
					freeMap(pMap);
					return IBIT;
				}

				// call
				uint32_t ret = addBasicNode(newLayer, cSid, Q, Tu, Ti, F, depth + 1);

				/*
				 * @date 2022-01-24 19:38:12
				 * if nothing was found then some loop was detected
				 */
				if (newLayer.gid == IBIT && newLayer.ucList == IBIT) {
					freeMap(pStack);
					freeMap(pMap);
					return IBIT ^ (IBIT - 1); // return silently-ignore
				}

				// finalise
				flushLayer(newLayer);

				/*
				 * @date 2022-01-20 14:25:35
				 * layer.gid might now be outdated
				 */
				if (layer.gid != IBIT)
					rebuildLayer(layer);

				/*
				 * @date 2022-01-16 00:32:17
				 * returns "silently-ignore" if a potential loop was detected
				 * See matching timestamp in `addBasicNode()`.
				 * 
				 * NOTE: returning a collapse will drastically reduce runtime, but is less accurate
				 */
				if (ret == (IBIT ^ (IBIT - 1))) {
					// yes, silently ignore
					freeMap(pStack);
					freeMap(pMap);
					return ret;
				}

				// Push onto stack
				pStack[numStack++] = newLayer.gid;
				pMap[nextNode++]   = newLayer.gid;

				/*
				 * @date 2022-01-12 13:21:33
				 * (position outdated)
				 *
				 * If the component folds to an entrypoint:
				 *	The original idea of `expandSignature()` is to create alternatives in an attempty to match and join groups.
				 *	Although a structural collapse is unexpected - the signature did not expect it - it might be caused due to component group merging.
				 *	So, although the final expand is different than intended, it is still valid, and might even enhance matching possibilities.
				 * If the component folds to self:
				 *	This implies that the layer under construction is actually an endpoint. 
				 *	This should self-collapse the signature, which is a group collapse.
				 *	But that is too complicated now as there are loads of other issues to be fixed.
				 *	todo: differentiate between "self-collapse" (group) and "silently-ignore" (node)
				 * If the component folds to an endpoint (pActive->mem)
				 * 	Might be the result of deeper components merging groups
				 * 	This too might be a good thing.
				 * 	todo: let it happen instead of rejecting
				 */

			} else {
				assert(numStack == 0);

				// endpoint collapse?
				if (layer.gid == Q || layer.gid == Tu || layer.gid == F) {
					// yes
					freeMap(pStack);
					freeMap(pMap);
					return IBIT;
				}

				// NOTE: top-level, use same depth/indent as caller
				uint32_t ret = addBasicNode(layer, cSid, Q, Tu, Ti, F, depth);

				freeMap(pStack);
				freeMap(pMap);

				return ret;
			}
		}

		/*
		 * This path is taken when the last node was a SID_SELF
		 */

		assert(numStack == 1); // only result on stack

		uint32_t ret = pStack[0]; // save before releasing

		if (layer.gid == ret) {
			// yes
			freeMap(pStack);
			freeMap(pMap);
			return IBIT;
		}

		freeMap(pStack);
		freeMap(pMap);

		// merge result into group under construction
		addOldNode(layer, ret);

		return 0; // return success
	}

	/*
	 * @date 2021-12-03 19:54:05
	 */
	uint32_t __attribute__((used)) expandMember(groupLayer_t &layer, uint32_t mid, const uint32_t *pSlots, unsigned depth) {

		assert(mid != 0);

		member_t   *pMember          = db.members + mid;
		unsigned   numPlaceholder    = db.signatures[pMember->sid].numPlaceholder;
		const char *pMemberTransform = db.revTransformNames[pMember->tid];

		// group id must be latest
		assert(layer.gid == IBIT || layer.gid == this->N[layer.gid].gid);

		/*
		 * init
		 */

		unsigned numStack = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap(); // evaluation stack
		uint32_t *pMap    = allocMap(); // node id of intermediates

		// component sid/slots
		uint32_t cSid             = 0; // 0=error/folded
		uint32_t cSlots[MAXSLOTS] = {0}; // zero contents
		assert(cSlots[MAXSLOTS - 1] == 0);

		/*
		 * Load string
		 */
//		printf("M %u:%s\n", mid, pMember->name);
		for (const char *pattern = pMember->name; *pattern; pattern++) {

			uint32_t Q, Tu, Ti, F;

			switch (*pattern) {
			case '0': //
				/*
				 * Push zero
				 */
				assert (numStack < maxNodes);
				pStack[numStack++] = 0;
				continue; // for

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

				assert (v >= this->nstart && v < nextNode);

				assert (numStack < maxNodes);
				pStack[numStack++] = pMap[v];
				continue; // for
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
				uint32_t v = (*pattern - 'a');

				assert(v < numPlaceholder);

				// apply member transform
				v = (pMemberTransform[v] - 'a');

				assert (numStack < maxNodes);
				pStack[numStack++] = pSlots[v];
				continue; // for

			}

			case '+': {
				// OR (appreciated)
				assert(numStack >= 2);

				F  = pStack[--numStack];
				Tu = 0;
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '>': {
				// GT (appreciated)
				assert(numStack >= 2);

				F  = 0;
				Tu = pStack[--numStack];
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '^': {
				// XOR/NE (appreciated)
				assert(numStack >= 2);

				F  = pStack[--numStack];
				Tu = F;
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '!': {
				// QnTF (appreciated)
				assert(numStack >= 3);

				F  = pStack[--numStack];
				Tu = pStack[--numStack];
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '&': {
				// AND (depreciated)
				assert(numStack >= 2);

				F  = 0;
				Tu = pStack[--numStack];
				Ti = 0;
				Q  = pStack[--numStack];
				break;
			}
			case '?': {
				// QTF (depreciated)
				assert(numStack >= 3);

				F  = pStack[--numStack];
				Tu = pStack[--numStack];
				Ti = 0;
				Q  = pStack[--numStack];
				break;
			}
			default:
				assert(0);
			} // end-switch

			/*
			 * Only arrive here when Q/T/F have been set 
			 */

			/*
			 * use the latest lists
			 */

			// get latest group
			Q  = updateToLatest(Q);
			Tu = updateToLatest(Tu);
			F  = updateToLatest(F);

			// did a deeper component merge groups that triggers an endpoint-collapse now?
			if (Q == layer.gid || Tu == layer.gid || F == layer.gid) {
				// yes

				freeMap(pStack);
				freeMap(pMap);
				return IBIT ^ layer.gid;
			}

			/*
			 * Perform normalisation
			 */

			/*
			 * Level 2 normalisation: single node rewrites
			 *
			 * appreciated:
			 *
			 *  [ 0] a ? !0 : 0  ->  a
			 *  [ 1] a ? !0 : a  ->  a ? !0 : 0
			 *  [ 2] a ? !0 : b                  "+" OR
			 *  [ 3] a ? !a : 0  ->  0
			 *  [ 4] a ? !a : a  ->  a ? !a : 0
			 *  [ 5] a ? !a : b  ->  b ? !a : b
			 *  [ 6] a ? !b : 0                  ">" GREATER-THAN
			 *  [ 7] a ? !b : a  ->  a ? !b : 0
			 *  [ 8] a ? !b : b                  "^" NOT-EQUAL/XOR
			 *  [ 9] a ? !b : c                  "!" QnTF
			 *
			 * depreciated:
			 *  [10] a ?  0 : 0 -> 0
			 *  [11] a ?  0 : a -> 0
			 *  [12] a ?  0 : b -> b ? !a : 0
			 *  [13] a ?  a : 0 -> a
			 *  [14] a ?  a : a -> a ?  a : 0
			 *  [15] a ?  a : b -> a ? !0 : b
			 *  [16] a ?  b : 0                  "&" AND
			 *  [17] a ?  b : a -> a ?  b : 0
			 *  [18] a ?  b : b -> b
			 *  [19] a ?  b : c                  "?" QTF
			 *
			  * ./eval --raw 'a0a!' 'a0b!' 'aaa!' 'aab!' 'aba!' 'abb!' 'abc!' 'a0a?' 'a0b?' 'aaa?' 'aab?' 'aba?' 'abb?' 'abc?'
			  *
			 */

			/*
			 * @date 2022-01-12 13:47:09
			 * NOTE: for cSid==SID_SELF, `nid` will hold the id
			 */
			uint32_t nid = 0;

			if (Q == 0) {
				// level-1 fold
				nid  = F;
				cSid = db.SID_SELF;
			} else if (Ti) {
				if (Tu == 0) {
					if (Q == F || F == 0) {
						// [ 0] a ? !0 : 0  ->  a
						// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
						nid  = Q;
						cSid = db.SID_SELF;
					} else {
						// [ 2] a ? !0 : b  -> "+" OR
						cSid = db.SID_OR;
						// swap
						if (Q > F) {
							uint32_t savQ = Q;
							Q             = F;
							F             = savQ;
						}
					}
				} else if (Q == Tu) {
					if (Q == F || F == 0) {
						// [ 3] a ? !a : 0  ->  0
						// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
						nid  = 0;
						cSid = db.SID_SELF;
					} else {
						// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
						// WARNING: Converted LESS-THAN
						Q    = F;
						F    = 0;
						cSid = db.SID_GT;
					}
				} else {
					if (Q == F || F == 0) {
						// [ 6] a ? !b : 0  ->  ">" GREATER-THAN
						// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
						F    = 0;
						cSid = db.SID_GT;
					} else if (Tu == F) {
						// [ 8] a ? !b : b  -> "^" NOT-EQUAL/XOR
						cSid = db.SID_NE;
						// swap
						if (Q > F) {
							uint32_t savQ = Q;
							Q             = F;
							F             = savQ;
							Tu            = savQ;
						}
					} else {
						// [ 9] a ? !b : c  -> "!" QnTF
						cSid = db.SID_QNTF;
					}
				}

			} else {

				if (Tu == 0) {
					if (Q == F || F == 0) {
						// [10] a ?  0 : 0 -> 0
						// [11] a ?  0 : a -> 0
						nid  = 0;
						cSid = db.SID_SELF;
					} else {
						// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
						// WARNING: Converted LESS-THAN
						Tu   = Q;
						Ti   = IBIT;
						Q    = F;
						F    = 0;
						cSid = db.SID_GT;
					}
				} else if (Q == Tu) {
					if (Q == F || F == 0) {
						// [13] a ?  a : 0 -> a
						// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
						nid  = Q;
						cSid = db.SID_SELF;
					} else {
						// [15] a ?  a : b -> a ? !0 : b -> "+" OR
						Tu   = 0;
						Ti   = IBIT;
						cSid = db.SID_OR;
						// swap
						if (Q > F) {
							uint32_t savQ = Q;
							Q             = F;
							F             = savQ;
						}
					}
				} else {
					if (Q == F || F == 0) {
						// [16] a ?  b : 0             "&" and
						// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
						F    = 0;
						cSid = db.SID_AND;
						// swap
						if (Q > Tu) {
							uint32_t savQ = Q;
							Q             = Tu;
							Tu            = savQ;
						}
					} else if (Tu == F) {
						// [18] a ?  b : b -> b        ENDPOINT
						nid  = F;
						cSid = db.SID_SELF;
					} else {
						// [19] a ?  b : c             "?" QTF
						cSid = db.SID_QTF;
					}
				}
			}

			if (cSid == db.SID_SELF) {
				// folding occurred and result is in nid

				// Push onto stack
				pStack[numStack++] = nid;
				pMap[nextNode++]   = nid;

			} else if (pattern[1]) {

				// allocate storage for scope
				groupLayer_t newLayer(*this, &layer);

				// endpoint collapse?
				if (layer.gid == Q || layer.gid == Tu || layer.gid == F) {
					// yes
					freeMap(pStack);
					freeMap(pMap);
					return IBIT;
				}

				// call
				uint32_t ret = addBasicNode(newLayer, cSid, Q, Tu, Ti, F, depth + 1);

				/*
				 * @date 2022-01-24 19:38:12
				 * if nothing was found then some loop was detected
				 */
				if (newLayer.gid == IBIT && newLayer.ucList == IBIT) {
					freeMap(pStack);
					freeMap(pMap);
					return IBIT ^ (IBIT - 1); // return silently-ignore
				}

				// finalise
				flushLayer(newLayer);

				/*
				 * @date 2022-01-20 14:25:35
				 * layer.gid might now be outdated
				 */
				if (layer.gid != IBIT)
					rebuildLayer(layer);

				/*
				 * @date 2022-01-16 00:32:17
				 * returns "silently-ignore" if a potential loop was detected
				 * See matching timestamp in `addBasicNode()`.
				 * 
				 * NOTE: returning a collapse will drastically reduce runtime, but is less accurate
				 */
				if (ret == (IBIT ^ (IBIT - 1))) {
					// yes, silently ignore
					freeMap(pStack);
					freeMap(pMap);
					return ret;
				}

				// Push onto stack
				pStack[numStack++] = newLayer.gid;
				pMap[nextNode++]   = newLayer.gid;

				/*
				 * @date 2022-01-12 13:21:33
				 * (position outdated)
				 *
				 * If the component folds to an entrypoint:
				 *	The original idea of `expandSignature()` is to create alternatives in an attempty to match and join groups.
				 *	Although a structural collapse is unexpected - the signature did not expect it - it might be caused due to component group merging.
				 *	So, although the final expand is different than intended, it is still valid, and might even enhance matching possibilities.
				 * If the component folds to self:
				 *	This implies that the layer under construction is actually an endpoint. 
				 *	This should self-collapse the signature, which is a group collapse.
				 *	But that is too complicated now as there are loads of other issues to be fixed.
				 *	todo: differentiate between "self-collapse" (group) and "silently-ignore" (node)
				 * If the component folds to an endpoint (pActive->mem)
				 * 	Might be the result of deeper components merging groups
				 * 	This too might be a good thing.
				 * 	todo: let it happen instead of rejecting
				 */

			} else {
				assert(numStack == 0);

				// endpoint collapse?
				if (layer.gid == Q || layer.gid == Tu || layer.gid == F) {
					// yes
					freeMap(pStack);
					freeMap(pMap);
					return IBIT;
				}

				// NOTE: top-level, use same depth/indent as caller
				uint32_t ret = addBasicNode(layer, cSid, Q, Tu, Ti, F, depth);

				freeMap(pStack);
				freeMap(pMap);

				return ret;
			}
		}

		/*
		 * This path is taken when the last node was a SID_SELF
		 */

		assert(numStack == 1); // only result on stack

		uint32_t ret = pStack[0]; // save before releasing

		if (layer.gid == ret) {
			// yes
			freeMap(pStack);
			freeMap(pMap);
			return IBIT;
		}

		freeMap(pStack);
		freeMap(pMap);

		// merge result into group under construction
		addOldNode(layer, ret);

		return 0; // return success
	}

	/*
	 * @date 2021-11-04 00:44:47
	 *
	 * lookup/create and normalise any combination of Q, T and F, inverted or not.
	 * 
	 * Returns main node id, which might be outdated as effect of internal rewriting.
	 */
	uint32_t addNormaliseNode(uint32_t Q, uint32_t T, uint32_t F) {
		this->cntAddNormaliseNode++;

		assert ((Q & ~IBIT) < this->ncount);
		assert ((T & ~IBIT) < this->ncount);
		assert ((F & ~IBIT) < this->ncount);

		/*
		 * Fast test for endpoints
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
		}
		if (Q == 0) {
			// "0?T:F" -> "F"
			return F;
		}

		if (F & IBIT) {
			// "Q?T:!F" -> "!(Q?!T:F)"
			F ^= IBIT;
			T ^= IBIT;
			return addNormaliseNode(Q, T, F) ^ IBIT;
		}

		// split `T` into unsigned and invert-bit
		uint32_t Tu = T & ~IBIT;
		uint32_t Ti = T & IBIT;

		/*
		 * use the latest lists
		 */

		Q  = updateToLatest(Q);
		Tu = updateToLatest(Tu);
		F  = updateToLatest(F);

		// may not be empty
		assert(Q < this->nstart || Q != this->N[Q].next);
		assert(Tu < this->nstart || Tu != this->N[Tu].next);
		assert(F < this->nstart || F != this->N[F].next);

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

		uint32_t tlSid = 0;

		// NOTE: Q/T/F comparisons are value based, so OR/NR/AND swapping is easily implemented 
		if (Ti) {
			// as you might notice, once `Ti` is set, it stays set

			if (Tu == 0) {
				if (Q == F || F == 0) {
					// [ 0] a ? !0 : 0  ->  a
					// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
					return Q;
				} else {
					// [ 2] a ? !0 : b  -> "+" OR
					tlSid = db.SID_OR;
					// swap
					if (Q > F) {
						uint32_t savQ = Q;
						Q             = F;
						F             = savQ;
					}
				}
			} else if (Q == Tu) {
				if (Q == F || F == 0) {
					// [ 3] a ? !a : 0  ->  0
					// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
					return 0;
				} else {
					// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
					// WARNING: Converted LESS-THAN
					Q     = F;
					F     = 0;
					tlSid = db.SID_GT;
				}
			} else {
				if (Q == F || F == 0) {
					// [ 6] a ? !b : 0  ->  ">" GREATER-THAN
					// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
					F     = 0;
					tlSid = db.SID_GT;
				} else if (Tu == F) {
					// [ 8] a ? !b : b  -> "^" NOT-EQUAL/XOR
					tlSid = db.SID_NE;
					// swap
					if (Q > F) {
						uint32_t savQ = Q;
						Q             = F;
						F             = savQ;
						Tu            = savQ;
					}
				} else {
					// [ 9] a ? !b : c  -> "!" QnTF
					tlSid = db.SID_QNTF;
				}
			}

		} else {

			if (Tu == 0) {
				if (Q == F || F == 0) {
					// [10] a ?  0 : 0 -> 0
					// [11] a ?  0 : a -> 0
					return 0;
				} else {
					// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
					// WARNING: Converted LESS-THAN
					Tu    = Q;
					Ti    = IBIT;
					Q     = F;
					F     = 0;
					tlSid = db.SID_GT;
				}
			} else if (Q == Tu) {
				if (Q == F || F == 0) {
					// [13] a ?  a : 0 -> a
					// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
					return Q;
				} else {
					// [15] a ?  a : b -> a ? !0 : b -> "+" OR
					Tu    = 0;
					Ti    = IBIT;
					tlSid = db.SID_OR;
					// swap
					if (Q > F) {
						uint32_t savQ = Q;
						Q             = F;
						F             = savQ;
					}
				}
			} else {
				if (Q == F || F == 0) {
					// [16] a ?  b : 0             "&" and
					// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
					F     = 0;
					tlSid = db.SID_AND;
					// swap
					if (Q > Tu) {
						uint32_t savQ = Q;
						Q             = Tu;
						Tu            = savQ;
					}
				} else if (Tu == F) {
					// [18] a ?  b : b -> b        ENDPOINT
					return F;
				} else {
					// [19] a ?  b : c             "?" QTF
					tlSid = db.SID_QTF;
				}
			}
		}

		// update to latest
		Q  = updateToLatest(Q);
		Tu = updateToLatest(Tu);
		F  = updateToLatest(F);

		/*
		 * allocate storage for `addBasicNode()`
		 */
		groupLayer_t layer(*this, NULL);

		/*
		 * call worker
		 */
		addBasicNode(layer, tlSid, Q, Tu, Ti, F, /*depth=*/0);

		// finalise
		flushLayer(layer);

		// regular calls should have a group header
		assert(!(layer.gid & IBIT));

		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

		return layer.gid;
	}

	/**
	 * @date 2021-08-13 13:33:13
	 *
	 * Add a node to tree
	 *
	 * NOTE: `addNormaliseNode()` can return IBIT to indicate a collapse, 
	 *       this function returns IBIT to indicated an inverted result. 
	 * NOTE: this function is called recursively from `expandSignature()`/expandMember()`.
	 * NOTE: do NOT forget to update gid after calling this function
	 * 
	 * 	uint32_t nid = addBasicNode(gid,...);
	 * 	gid = updateToLatest(gid);
	 * 
	 * Return value is one of:
	 *   IBIT not set: node representing arguments
	 *   IBIT^(IBIT-1): self-collapse, silently ignore
	 *   IBIT^id: group has collapsed to id
	 *
	 * @date 2022-01-23 19:18:32
	 * 
	 * Due to delayed group id creation, it is not always possible to return an id on success.
	 * On success return 0.
	 * 
	 * @param {uint32_t}         gid - group id to add node to. May be IBIT to create new group
	 * @param {uint32_t}         tlSid - `1n9` sid describing Q/T/F
	 * @param {uint32_t}         Q - component
	 * @param {uint32_t}         Tu - component
	 * @param {uint32_t}         Ti - T is inverted
	 * @param {uint32_t}         F - component
	 * @param {uint32_t*}        pChampionMap - Lookup index for all sids in group
	 * @param {versionMemory_t*} pChampionVersion - versioned memory for `pChampionMap`
	 * @param {unsigned}         depth - Recursion depth
	 * @return {number} newly created node Id, or IBIT when collapsed.
	 */
	uint32_t addBasicNode(groupLayer_t &layer, uint32_t tlSid, uint32_t Q, uint32_t Tu, uint32_t Ti, uint32_t F, unsigned depth) {
		this->cntAddBasicNode++;

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			fprintf(stderr, "[%s] cntAddNormaliseNode=%lu cntAddBasicNode=%lu ncount=%u gcount=%u | cntOutdated=%lu cntRestart=%lu cntUpdateGroupCollapse=%lu cntUpdateGroupMerge=%lu cntApplySwapping=%lu cntApplyFolding=%lu cntMergeGroups=%lu\n", ctx.timeAsString(),
				this->cntAddNormaliseNode,
				this->cntAddBasicNode,
				this->ncount,
				this->gcount,

				this->cntOutdated,
				this->cntRestart,
				this->cntUpdateGroupCollapse,
				this->cntUpdateGroupMerge,
				this->cntApplySwapping,
				this->cntApplyFolding,
				this->cntMergeGroups
			);
			ctx.tick = 0;

			if (depth == 0)
				validateTree(__LINE__);
		}

		assert(depth < 30);

		// should be latest
		assert(this->N[Q].gid == Q);
		assert(this->N[Tu].gid == Tu);
		assert(this->N[F].gid == F);
		// may not be orphaned
		assert(Q < this->nstart || Q != this->N[Q].next);
		assert(Tu < this->nstart || Tu != this->N[Tu].next);
		assert(F < this->nstart || F != this->N[F].next);
		// gid must be latest
		assert(layer.gid == IBIT || this->N[layer.gid].gid == layer.gid);

		// is it an argument-collapse?
		if (Q == layer.gid || Tu == layer.gid || F == layer.gid)
			return IBIT ^ layer.gid; // yes

		uint32_t tlSlots[MAXSLOTS] = {0}; // zero contents
		assert(tlSlots[MAXSLOTS - 1] == 0);
		double tlWeight;

		if (ctx.opt_debug & context_t::DEBUGMASK_CARTESIAN) {
			printf("%.*sQ=%u T=%u%s F=%u",
			       depth, "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t",
			       Q, Tu, Ti ? "~" : "", F);
			if (layer.gid != IBIT)
				printf(" G=%u", layer.gid);
			printf("\n");
		}

		// set (and order) slots
		if (tlSid == db.SID_OR || tlSid == db.SID_NE) {
			tlSlots[0] = Q;
			tlSlots[1] = F;
			tlWeight = 1 + this->N[Q].weight + this->N[F].weight;
		} else if (tlSid == db.SID_GT || tlSid == db.SID_AND) {
			tlSlots[0] = Q;
			tlSlots[1] = Tu;
			tlWeight = 1 + this->N[Q].weight + this->N[Tu].weight;
		} else {
			tlSlots[0] = Q;
			tlSlots[1] = Tu;
			tlSlots[2] = F;
			tlWeight = 1 + this->N[Q].weight + this->N[Tu].weight + this->N[F].weight;
		}
		assert(tlWeight < 1.0 / 0.0);

		// test if node already exists
		uint32_t nix = this->lookupNode(tlSid, tlSlots);
		uint32_t nid = this->nodeIndex[nix];

		if (nid != 0) {
			// is node under construction? 
			if (this->N[nid].gid == IBIT)
				return IBIT ^ (IBIT - 1); // yes. silently ignore

			addOldNode(layer, nid); // no, attach to that of the node

			return 0; // success
		}

		/*
		 * When hitting the deepest depth, simply create node 
		 */
		if (depth >= this->maxDepth) {

			assert(layer.gid == IBIT);

			// create node
			nid = this->newNode(tlSid, tlSlots, tlWeight);

			addNewNode(layer, nix, nid);

			return 0; // success
		}
		depth++;

		/* 
		 * Before adding a new node to current group, check if it would make a chance to win the challenge.
		 */
		{
			// does group have a node with better sid?
			uint32_t champion = layer.findChampion(tlSid);
			if (champion != IBIT) {
				int cmp = this->compare(champion, tlSid, tlSlots, tlWeight);

				if (cmp < 0) {
					// existing is better
					return layer.gid;

				} else if (cmp == 0) {
					assert(0); // should have been detected by `lookupNode()`
				}
			}
		}

		/*
		 * @date 2021-11-04 02:08:51
		 * 
		 * Second step: create Cartesian products of Q/T/F group lists
		 * 
		 * All nodes of the list need to be processed
		 * With `for` the node containing the end-condition is skipped.
		 * `while{}do()` evaluates the condition after the iteration, allowing all nodes to be iterated.
		 * Iterators can change groups as effect of group merging.
		 * iQ/iTu/iF are the iterator nodes for the Cartesian product.  
		 * Q/T/F are considered iterator group id's.
		 * Group changes invalidates positioning, iterators need to start at the beginning of their new lists.
		 * 
		 */
		unsigned iQ  = Q;
		unsigned iTu = Tu;
		unsigned iF  = F;
		for (;;) {
			do {

				/*
				 * @date 2022-01-24 13:14:34
				 * 
				 * The majority of asserts is what would be queries during a debugging session to validate correct operation.
				 * Race conditions and path choices might double/forget actions needed for correct operation.
				 * `cntCproduct` marks the last good starting point, without diving deeper into recursion.
				 * If an assert triggers, then this is the place to start.
				 * The offending assert can be on this recursion level or (most likely) higher.
				 */
				restart:

				uint32_t cntCproduct = this->cntCproduct++;
				(void) cntCproduct;

				/*
				 * @date 2022-01-14 02:10:15
				 * did iterators change
				 */

				if (this->N[Q].gid != Q) {
					// group change
					iQ      = Q = updateToLatest(this->N[iQ].gid);
				} else if (this->N[iQ].next == iQ && iQ >= this->nstart) {
					// orphaned
					iQ      = Q;
				}

				if (this->N[Tu].gid != Tu) {
					// group change
					iTu     = Tu = updateToLatest(this->N[iTu].gid);
				} else if (this->N[iTu].next == iTu && iTu >= nstart) {
					// orphaned
					iTu     = Tu;
				}

				if (Tu != F) {
					if (this->N[F].gid != F) {
						// group change
						iF      = F = updateToLatest(this->N[iF].gid);
					} else if (this->N[iF].next == iF && iF >= nstart) {
						// orphaned
						iF      = F;
					}
				}

				// is it an iterator collapse?
				if (layer.gid == Q || layer.gid == Tu || layer.gid == F) {
					// yes
					return IBIT; // return collapse
				}

				/*
				 * Analyse Q/T/F combo 
				 */

				/*
				 * @date 2021-12-27 14:04:37
				 * Clamp F when handling NE/XOR. 
				 * or it will create combos that this code is set out to detect and reduce 
				 */
				if (Tu == F)
					iF = iTu;

				if (ctx.flags & context_t::MAGICMASK_PARANOID) {
					// iterators must match arguments
					assert(this->N[iQ].gid == Q);
					assert(this->N[iTu].gid == Tu);
					assert(this->N[iF].gid == F);
					// iterators must be in up-to-date
					assert(this->N[Q].gid == Q);
					assert(this->N[Tu].gid == Tu);
					assert(this->N[F].gid == F);
					// iterators may not be orphaned
					assert(iQ < this->nstart || this->N[iQ].next != iQ);
					assert(iTu < this->nstart || this->N[iTu].next != iTu);
					assert(iF < this->nstart || this->N[iF].next != iF);
					// iterators may not be current group
					assert(Q != layer.gid);
					assert(Tu != layer.gid);
					assert(F != layer.gid);
				}

				/*
				 * Normalise (test for folding), when this happens collapse/invalidate the whole group and redirect to the folded result.
				 * Requires temporary Q/T/F because normalising might swap loop iterators. 
				 */
				uint32_t folded = IBIT;
				uint32_t normQ  = 0, normTi = 0, normTu = 0, normF = 0;
				if (Ti) {

					if (iTu == 0) {
						if (iQ == iF || iF == 0) {
							// [ 0] a ? !0 : 0  ->  a
							// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
							folded = iQ;
						} else {
							// [ 2] a ? !0 : b  -> "+" OR
							if (iQ < iF) {
								normQ  = iQ;
								normTu = 0;
								normTi = IBIT;
								normF  = iF;
							} else {
								normQ  = iF;
								normTu = 0;
								normTi = IBIT;
								normF  = iQ;
							}
						}
					} else if (iQ == iTu) {
						if (iQ == iF || iF == 0) {
							// [ 3] a ? !a : 0  ->  0
							// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
							folded = 0;
						} else {
							// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
							// WARNING: Converted LESS-THAN
							normQ  = iF;
							normTu = iTu;
							normTi = Ti;
							normF  = 0;
						}
					} else {
						if (iQ == iF || iF == 0) {
							// [ 6] a ? !b : 0  ->  ">" GREATER-THAN
							// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
							normQ  = iQ;
							normTu = iTu;
							normTi = Ti;
							normF  = 0;
						} else if (iTu == iF) {
							// [ 8] a ? !b : b  -> "^" NOT-EQUAL/XOR
							if (iQ < iF) {
								normQ  = iQ;
								normTu = iF;
								normTi = IBIT;
								normF  = iF;
							} else {
								normQ  = iF;
								normTu = iQ;
								normTi = IBIT;
								normF  = iQ;
							}
						} else {
							// [ 9] a ? !b : c  -> "!" QnTF
							normQ  = iQ;
							normTu = iTu;
							normTi = Ti;
							normF  = iF;
						}
					}

				} else {

					if (iTu == 0) {
						if (iQ == iF || iF == 0) {
							// [10] a ?  0 : 0 -> 0
							// [11] a ?  0 : a -> 0
							folded = 0;
						} else {
							// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
							// WARNING: Converted LESS-THAN
							normQ  = iF;
							normTu = iQ;
							normTi = IBIT;
							normF  = 0;
						}
					} else if (iQ == iTu) {
						if (iQ == iF || iF == 0) {
							// [13] a ?  a : 0 -> a
							// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
							folded = iQ;
						} else {
							// [15] a ?  a : b -> a ? !0 : b -> "+" OR
							if (iQ < iF) {
								normQ  = iQ;
								normTu = 0;
								normTi = IBIT;
								normF  = iF;
							} else {
								normQ  = iF;
								normTu = 0;
								normTi = IBIT;
								normF  = iQ;
							}
						}
					} else {
						if (iQ == iF || iF == 0) {
							// [16] a ?  b : 0                  "&" AND
							// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
							if (iQ < iF) {
								normQ  = iQ;
								normTu = iTu;
								normTi = 0;
								normF  = 0;
							} else {
								normQ  = iTu;
								normTu = iQ;
								normTi = 0;
								normF  = 0;
							}
						} else if (iTu == iF) {
							// [18] a ?  b : b -> b        ENDPOINT
							assert(0); // already tested
						} else {
							// [19] a ?  b : c             "?" QTF
							normQ  = iQ;
							normTu = iTu;
							normTi = Ti;
							normF  = iF;
						}
					}
				}

				/*
				 * Folding into an iterator is a full structural collapse
				 * This collapses the group as whole
				 */
				if (folded != IBIT) {
					// folded to one of the iterators or zero.
					// iterator-collapse is group collapse

					assert(0); // todo: does this path get walked?

					addCollapse(layer, folded);

					return IBIT; // return collapse
				}

				/*
				 * Build slots and lookup signature
				 */

				uint32_t finalSlots[MAXSLOTS];
				double   weight = +1.0 / 0.0; // +inf

				uint32_t sid = constructSlots(layer, this->N + normQ, this->N + normTu, normTi, this->N + normF, finalSlots, &weight);

				// combo not found or folded
				if (sid == 0)
					continue; // yes, silently ignore

				/*
				 * Test for an endpoint collapse, which collapses the group as whole
				 */
				if (sid == db.SID_ZERO || sid == db.SID_SELF) {
					uint32_t endpoint = (sid == db.SID_ZERO) ? 0 : finalSlots[0];

					// if layer==endpoint, then it is a self-collapse, else it is an endpoint collapse
					if (layer.gid != endpoint)
						addCollapse(layer, endpoint);

					return IBIT; // return collapse
				}

				/*
				 * Does sid/finalSlots make a chance reaching the end mark
				 */
				{
					// does group have a node with better sid?
					uint32_t champion = layer.findChampion(sid);
					if (champion != IBIT) {
						int cmp = this->compare(champion, sid, finalSlots, weight);

						if (cmp < 0) {
							// existing is better
							continue; // no, silently ignore

						} else if (cmp == 0) {
							continue; // no, duplicate
						}
					}
				}

				signature_t *pSignature = db.signatures + sid;

				/*
				 * @date 2022-01-14 00:01:39
				 * 
				 * Expand signature/members in an attempt for merge with other groups
				 */
				if (depth + 1 < this->maxDepth && pSignature->size > 1) {

					if (true) {
						uint32_t ret = expandSignature(layer, sid, finalSlots, depth + 1);

						// silently ignore
						if (ret == (IBIT ^ (IBIT - 1)))
							continue; // yes

						// is it a collapse?
						if ((ret & IBIT) || layer.gid < this->nstart) {
							// yes
							return IBIT; // return collapse
						}

					} else {

						for (uint32_t mid = pSignature->firstMember; mid != 0; mid = db.members[mid].nextMember) {
							uint32_t ret = expandMember(layer, mid, finalSlots, depth + 1);

							// silently ignore
							if (ret == (IBIT ^ (IBIT - 1)))
								continue; // yes

							// is it a collapse?
							if ((ret & IBIT) || layer.gid < this->nstart) {
								// yes
								return IBIT; // return collapse
							}
						}

					}

					/*
					 * Did iterators change?
					 */
					if (this->N[Q].gid != Q || this->N[Tu].gid != Tu || this->N[F].gid != F)
						goto restart; // yes

					/*
					 * were groups merged that no becomes a self-collapse? 
					 */if (layer.gid == Q || layer.gid == Tu || layer.gid == F)
						return IBIT; // endpoint-collapse

					/*
					 * did node reference in slots change group?
					 */
					for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
						uint32_t id = finalSlots[iSlot];

						// did reference merge groups?
						if (this->N[id].gid != id) {
							//
							printf("finalSlots restart\n");
							goto restart;
						}

						// is there an endpoint collapse
						if (id == layer.gid)
							return IBIT; // yes
					}
				}

				/*
				 * Does sid/finalSlots exist?
				 */

				// lookup slots
				nix = this->lookupNode(sid, finalSlots);
				nid = this->nodeIndex[nix];

				if (nid != 0) {
					// is node under construction?
					if (this->N[nid].gid == IBIT)
						continue; // yes, silently ignore

					uint32_t latest = updateToLatest(nid);

					if (latest == Q || latest == Tu || latest == F) {
						/*
						 * @date 2022-01-13 22:14:02
						 * 
						 * Iterator-collapse
						 * An existing node was found that belongs to one of the iterators.
						 * Either the current group, or another group.
						 * There are no references to this group because it is under construction.
						 * That makes it possible to promote the iterator as result
						 * 
						 * This is related to self-collapse "@date 2022-01-12 22:34:26".
						 * The difference is that there the group already exists and a group-collapse would orphan the group invalidating all references to it.
						 * Whereas here, the group is under construction, and suddenly detected it's an iterator/endpoint 
						 * 
						 * @date 2022-01-25 22:02:16
						 * 
						 * If `latest == layer.gid` then it is a self-collapse, otherwise it is an endpoint-collapse
						 */

						if (layer.gid != latest)
							addCollapse(layer, latest);

						return IBIT; // return collapse
					}

					// add node and continue

					// todo: schedule for removal: consider groupmerge a collapse (which it actually isnt)
					uint32_t oldGid = layer.gid;
					addOldNode(layer, nid);
					if (layer.gid != oldGid) {
						assert(layer.gid != IBIT);
						return IBIT ^ layer.gid;
					}
					continue;
				}

				/*
				 * Will new node survive a challenge
				 */

				{
					uint32_t champion = layer.findChampion(sid);
					if (champion != IBIT) {
						// yes
						int cmp = this->compare(champion, sid, finalSlots, weight);

						if (cmp < 0) {
							// champion is better
							continue; // silently ignore

						} else if (cmp > 0) {
							// argument is better, dismiss champion 
							orphanNode(layer, champion);

						} else if (cmp == 0) {
							assert(0); // should have been detected by `lookupNode()`.
						}
					}
				}

				/*
				 * Create new node
				 */

				nid = this->newNode(sid, finalSlots, weight);

				addNewNode(layer, nix, nid);

				if (ctx.opt_debug & ctx.DEBUGMASK_GROUPNODE) {
					printf("%.*sgid=%u\tnid=%u\tQ=%u\tT=%u\tF=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] siz=%u wgt=%lf\n",
					       depth, "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t",
					       layer.gid, nid,
					       iQ, iTu, iF,
					       sid, pSignature->name,
					       finalSlots[0], finalSlots[1], finalSlots[2], finalSlots[3], finalSlots[4], finalSlots[5], finalSlots[6], finalSlots[7], finalSlots[8],
					       pSignature->size, weight);
				}

				/*
				 * Merging groups change Q/T/F headers, possibly invalidating loop end conditions.
				 * It could be that `addToCollection()` merged Q/Tu/F into another group
				 * 
				 * NOTE: wrap above within a `do{}while()` so `continue` will update Q/T/F
				 */
			} while (false);

			/*
			 * detect iterator-group change
			 * this might happen when `mergeGroups()` involves an iterator group
			 */

			/*
			 * Bump iterators
			 * iQ/iT/iF are allowed to start with 0, when that happens, don't loop forever.
			 * node 0 is a single node list containing SID_ZERO.
			 */

			/*
			 * @date 2021-12-27 14:04:37
			 * Clamp F when handling NE/XOR. 
			 */
			if (Tu != F) {
				iF = this->N[iF].next;
				if (iF != this->N[iF].gid)
					continue;
			}

			iTu = this->N[iTu].next;
			if (iTu != this->N[iTu].gid)
				continue;

			iQ = this->N[iQ].next;
			if (iQ != this->N[iQ].gid)
				continue;

			break;
		}

		// return success
		return 0;
	}

	/*
	 * @date 2021-12-19 00:28:19
	 * 
	 * 1: flood fill
	 * 2: scan left/right, remove all referenced to flood (self)
	 *    tree is now safe against cyclic loops
	 * 3: walk tree, scrub group:
	 * 4:   update slots -> newSlots
	 * 5:   sidSwap
	 * 6:   sidFold
	 * 7:   if need to merge, abort current and goto 1
	 * 
	 * @date 2021-12-21 00:45:12
	 * 
	 * This routine was extremely complex to formalise.
	 * Strangely, runtime inspections are still unneeded.
	 * 
	 * If after calling, forward loops are still present, `updateGroups()` would loop, not happened yet.
	 * Not checked yet, remove too many nodes
	 * strange that applyFolding() still not called.
	 * strange that the test tree size for maxdepth=0 (67) was smaller than for maxdepth=2 (73) 
	 * validateTree() would complain if resolveForwards() would loop
	 * 
	 * @date 2022-01-01 00:40:21
	 * 
	 * Merge towards the lowest of lhs/rhs. 
	 * This should contain run-away `resolveForwards()`.
	 */
	void mergeGroups(groupLayer_t &layer, uint32_t rhs) {

		assert(layer.gid != IBIT && layer.ucList == IBIT); // must have id
		assert(this->N[layer.gid].gid == layer.gid); // must be latest

		// is it a self-collapse
		if (layer.gid == rhs)
			return; // yes

		if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("MERGEGROUP %u %u\n", layer.gid, rhs);

		uint32_t lhs = layer.gid;

		assert(this->N[lhs].gid == lhs); // lhs must be latest header
		assert(this->N[rhs].gid == rhs); // rhs must be latest header
		assert(rhs < this->nstart || this->N[rhs].next != rhs); // rhs may not be empty (lhs may be empty for entrypoints)

		this->cntMergeGroups++;

		/*
		 * @date 2021-12-28 21:11:47
		 * 
		 * New gid is lowest of lhs/rhs.
		 * This is to reuse group id's as much as possible and also to avoid runaway updates to latest. 
		 */
		uint32_t gid;
		if (lhs < rhs) {
			gid = lhs;
		} else {
			gid = rhs;
			rhs = lhs;
			lhs = gid;
		}

//		printf("mergeGroups=1 ./eval \"%s\" \"%s\"\n", this->saveString(lhs).c_str(), this->saveString(N[rhs].gid).c_str());

		/*
		 * entrypoint collapse?
		 */
		if (lhs < this->nstart && rhs < this->nstart) {
			assert(0);
		} else if (lhs < this->nstart) {
			// yes
			for (uint32_t iNode = this->N[rhs].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode = this->N + iNode;

				// redirect to entrypoint
				pNode->gid = lhs;

				// orphan node
				if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("mergegroup unlink rhs=%u\n", iNode);
				iNode = orphanNode(layer, iNode); // returns previous node
			}

			// let header redirect
			if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("mergegroup redirect rhs=%u lhs=%u\n", this->N[rhs].gid, lhs);
			this->N[rhs].gid = lhs;

			// update layer
			layer.gid = gid;
			// gid is an entrypoint, no rebuild needed

			return;
		} else if (rhs < this->nstart) {
			// yes
			for (uint32_t iNode = this->N[lhs].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode = this->N + iNode;

				// redirect to entrypoint
				pNode->gid = rhs;

				// orphan node
				if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("mergegroup unlink lhs=%u\n", iNode);
				iNode = orphanNode(layer, iNode); // returns previous node
			}

			// let header redirect
			if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("mergegroup redirect lhs=%u rhs=%u\n", this->N[lhs].gid, rhs);
			this->N[lhs].gid = rhs;

			// update layer
			layer.gid = gid;
			// gid is an entrypoint, no rebuild needed

			return;
		}


		/*
		 * Flood-fill
		 */
		if (lhs != rhs) {
			versionMemory_t *pVersion   = allocVersion();
			uint32_t        thisVersion = pVersion->nextVersion();

			pVersion->mem[lhs] = thisVersion;
			pVersion->mem[rhs] = thisVersion;

			// flood-fill area between lhs/rhs, flag everything referencing the flood

			// NOTE: lhs..rhs inclusive
			for (uint32_t iGroup = lhs; iGroup <= rhs; iGroup++) {
				if (this->N[iGroup].gid != iGroup)
					continue; // not start of list
				if (pVersion->mem[iGroup] == thisVersion)
					continue; // already processed 

				bool found = false;

				// process nodes of group	
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode         = this->N + iNode;
					unsigned          numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					// examine references
					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t id = updateToLatest(pNode->slots[iSlot]);

						// does it touch flood
						if (pVersion->mem[id] == thisVersion) {
							// yes
							pVersion->mem[iGroup] = thisVersion;
							found = true;
							break;
						}
					}
					if (found)
						break;
				}
			}

			/*
			 * Orphan all nodes (both lhs and rhs) with references to flood.
			 * Flood represent self. Referencing self will collapse to `a/[self]` which is the group header and always present.
			 */

			for (uint32_t iNode = this->N[lhs].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode         = this->N + iNode;
				unsigned    numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

				// redirecting to gid
				pNode->gid = gid;

				bool found = false;
				for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
					uint32_t id = updateToLatest(pNode->slots[iSlot]);

					if (pVersion->mem[id] == thisVersion) {
						found = true;
						break;
					}
				}

				if (found) {
					// collapse, orphan

					// unlink
					iNode = orphanNode(layer, iNode); // returns previous node
				}
			}

			for (uint32_t iNode = this->N[rhs].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode         = this->N + iNode;
				unsigned    numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

				// redirecting to gid
				pNode->gid = gid;

				bool found = false;
				for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
					uint32_t id = updateToLatest(pNode->slots[iSlot]);

					if (pVersion->mem[id] == thisVersion) {
						found = true;
						break;
					}
				}

				if (found) {
					// collapse, orphan

					// unlink
					if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("mergegroup unlink iNode=%u\n", iNode);
					iNode = orphanNode(layer, iNode); // returns previous node
				}
			}

			freeVersion(pVersion);
		}

		/*
		 * Simple merge so everything gets onto one list
		 */

		if (lhs == gid) {
			if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("mergegroup merge lhsprev=%u rhs=%u lhs=%u gid=%u\n", this->N[lhs].prev, rhs, lhs, gid);
			linkNode(this->N[lhs].prev, rhs);
			unlinkNode(rhs);
			// let rhs redirect to gid
			this->N[rhs].gid = gid;
		} else {
			if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("mergegroup merge rhsprev=%u lhs=%u rhs=%u gid=%u\n", this->N[rhs].prev, lhs, rhs, gid);
			linkNode(this->N[rhs].prev, lhs);
			unlinkNode(lhs);
			// let lhs redirect to gid
			this->N[lhs].gid = gid;
		}

		// update layer
		layer.gid = gid;
	}

	/*
	 * @date 2021-12-22 22:49:13
	 * 
	 * Update group by replacing all outdated nodes, re-create champion index to resolve sid challenges.
	 * 
	 * lhs/rhs is the range where forwarding is critical
	 * Merging will try to reassign the highest to the lowest
	 * If reassigning would create forward references, then recreate the group and return true.
	 * 
	 * pLhs/pRhs are the range limits used by `resolveForwards()`.
	 * 
	 * @date 2022-01-16 01:55:42
	 * When `allowForward` set, then recreate the group with a new id to relax forward references
	 */
	void updateGroup(groupLayer_t &layer, uint32_t *pRestartId, bool allowForward) {

		assert(layer.gid != IBIT && layer.ucList == IBIT); // must have id
		assert(this->N[layer.gid].gid == layer.gid); // must be latest
		assert(layer.gid >= this->nstart); // entrypoints should have been detected

		bool     hasSelf    = false;     // a self-collapse is a group collapse
		double   gWeight    = 1.0 / 0.0; // group weight. +inf
		uint32_t gHiSlotId  = 0;         // highest slot id

		this->cntUpdateGroup++;

		restart:

		/*
		 * Walk through all nodes of group
		 */

		// bump versioned memory
		layer.pChampionVersion->nextVersion();

		uint32_t nextId; // to allow unlinking of iterator
		for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = nextId) {
			groupNode_t *pNode         = this->N + iNode;
			unsigned    numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

			/*
			 * @date 2022-01-13 20:06:22
			 * If iterator got orphaned, then it was a dethroned champion during the previous iteration.
			 * Restart the loop
			 */
			if (pNode->next == iNode) {
				nextId = this->N[layer.gid].next;
				continue;
			}

			// save next iterator value in case list changes
			nextId = pNode->next;

			/*
			 * Detect if node is outdated
			 */

			uint32_t newSlots[MAXSLOTS];
			bool     changed     = false;
			double   newWeight   = db.signatures[pNode->sid].size;
			uint32_t newHiSlotId = 0;

			for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
				uint32_t id = pNode->slots[iSlot];

				if (this->N[id].gid != id) {
					id      = updateToLatest(id);
					changed = true;
				}

				newSlots[iSlot] = id;

				newWeight += this->N[id].weight;
				if (id > newHiSlotId)
					newHiSlotId = id;

				if (id == layer.gid)
					hasSelf = true;
			}
			assert(newWeight < 1.0 / 0.0); // +inf

			/*
			 * @date 2022-01-27 10:00:04
			 * Calculating `1n9` exact weights is expensive, do NOT overwrite with a fast approximation 
			 */
			if (db.signatures[pNode->sid].size > 1)
				pNode->weight = newWeight; // adding nodes to existing groups will change weights of references

			// is it a self-collapse:
			if (hasSelf) {
				// yes
				/*
				 * @date 2022-01-12 22:34:26
				 * 
				 * An endpoint being self is a "self-collapse" and is (as of now) a node collapse and should be silently ignores.
				 * 
				 * What theoretically happens:
				 *  - Node A with a higher id references node B with a lower id.
				 *  - At some point (most likely `expandSignature()`) concludes that both groups are identical.
				 *  - `mergegroups()` will join both an "pulls" the higher id of A down to the lower id of B.
				 *  - If A directly references B it references itself basically folding to `a/[id]`.
				 *  
				 *  Note to myself: a self-collapse is a node collapse and should be ignored, overruling the previous assumption that it could be a group collapse.  
				 */

				if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("updategroup self=%u\n", iNode);
				orphanNode(layer, iNode);

				continue;
			}

			/*
			 * Is original still usable and does it survive a re-challenge (if any)
			 * This is to determine if they win/lose against nodes from another group after being merged
			 */

			if (!changed) {
				uint32_t champion = layer.findChampion(pNode->sid);

				if (champion != IBIT) {
					int cmp = this->compare(champion, pNode->sid, pNode->slots, newWeight);

					if (cmp < 0) {
						// champion is better, orphan node (orphan might already have been orphaned)

						// update minimal group weight
						if (this->N[champion].weight < gWeight)
							gWeight = this->N[champion].weight;
						if (this->N[champion].hiSlotId > gHiSlotId)
							gHiSlotId = this->N[champion].hiSlotId;

						if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("updategroup old lose=%u champion=%u\n", iNode, champion);
						orphanNode(layer, iNode);
						continue;

					} else if (cmp > 0) {
						// the original unchanged node is better, `champion` is incorrect, update it 
						if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("updategroup old win=%u champion=%u\n", iNode, champion);
						orphanNode(layer, champion);

					} else if (cmp == 0) {
						assert(champion == iNode);
					}
				}

				// update minimal group weight
				if (pNode->weight < gWeight)
					gWeight = pNode->weight;
				// update highest slot id
				if (pNode->hiSlotId > gHiSlotId)
					gHiSlotId = pNode->hiSlotId;

				// make node the new champion
				layer.pChampionMap[pNode->sid]          = iNode;
				layer.pChampionVersion->mem[pNode->sid] = layer.pChampionVersion->version;

				continue;
			}

			// orphan outdated node
			if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("updategroup changed=%u\n", iNode);
			orphanNode(layer, iNode);

			/*
			 * Finalise new node
			 */

			// pad with zeros
			for (unsigned iSlot = numPlaceholder; iSlot < MAXSLOTS; iSlot++)
				newSlots[iSlot] = 0;

			uint32_t newSid = pNode->sid;

			// perform sid swap
			applySwapping(newSid, newSlots);

			// perform folding. NOTE: newSid/newSlots might both change
			applyFolding(&newSid, newSlots);

			// is it an endpoint-collapse
			if (newSid == db.SID_SELF) {
				// yes
				uint32_t newGid = newSlots[0];

				// orphan group and redirect to endpoint
				for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					groupNode_t *pNode = this->N + iNode;

					// redirect to endpoint
					pNode->gid = newGid;

					// orphan node
					iNode = orphanNode(layer, iNode); // returns previous node
				}

				// let header redirect
				this->N[layer.gid].gid = newGid;

				// update layer
				layer.gid = newGid;
				rebuildLayer(layer);

				if (pRestartId) {
					if (layer.gid < this->nstart)
						*pRestartId = this->nstart;
					else if (layer.gid < *pRestartId)
						*pRestartId = layer.gid;
				}

				return;
			}

			/*
			 * Is updated node existing?
			 */

			uint32_t nix = this->lookupNode(newSid, newSlots);
			uint32_t nid = this->nodeIndex[nix];

			// does updated node exist?
			if (nid != 0) {
				// yes

				// NOTE: cannot use addOldNode/addNewNode because it will recurse

				if (this->N[nid].gid == IBIT) {
					// nid is part of a list under-construction, append it to this list for later processing

					// append rhs to group
					linkNode(this->N[layer.gid].prev, nid);

					// update id's. 
					for (uint32_t iNode2 = nid; iNode2 != this->N[iNode2].gid; iNode2 = this->N[iNode2].next)
						this->N[iNode2].gid = layer.gid;

					// update nextId
					nextId = this->N[layer.gid].next;

				} else {
					uint32_t latest = updateToLatest(nid);
					if (layer.gid != latest) {
						// nid is member of another group, merge and restart

						// merge
						mergeGroups(layer, latest);

						if (pRestartId) {
							if (layer.gid < this->nstart)
								*pRestartId = this->nstart;
							else if (layer.gid < *pRestartId)
								*pRestartId = layer.gid;
						}

						// restart
						goto restart;

					} else {
						// nid is member of this group, let it handle itself
					}
				}

				continue;
			}

			/*
			 * Will new node survive a challenge
			 */

			uint32_t champion = layer.findChampion(newSid);
			if (champion != IBIT) {
				// yes
				int cmp = this->compare(champion, newSid, newSlots, newWeight);

				if (cmp < 0) {
					// champion is better
					continue; // silently ignore

				} else if (cmp > 0) {
					// updated node is better, dismiss champion 
					orphanNode(layer, champion);

				} else if (cmp == 0) {
					assert(0); // should have been detected by `lookupNode()`.
				}
			}

			/*
			 * Create new node
			 */

			nid = this->newNode(newSid, newSlots, newWeight);
			groupNode_t *pNew = this->N + nid;

			pNew->gid   = layer.gid;
			pNew->oldId = pNode->oldId ? pNode->oldId : iNode;

			// add node to index
			this->nodeIndex[nix]        = nid;
			this->nodeIndexVersion[nix] = this->nodeIndexVersionNr;

			// set as new champion
			layer.pChampionMap[newSid]          = nid;
			layer.pChampionVersion->mem[newSid] = layer.pChampionVersion->version;

			// add node immediately before the next, so it acts as a replacement and avoids getting double processed
			if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("updategroup link iNode=%u prev=%u newNid=%u\n", iNode, this->N[nextId].prev, nid);
			linkNode(this->N[nextId].prev, nid);

			// update minimal group weight
			if (pNew->weight < gWeight)
				gWeight = pNew->weight;
			// update highest slot id
			if (pNew->hiSlotId > gHiSlotId)
				gHiSlotId = pNew->hiSlotId;

			if (ctx.opt_debug & ctx.DEBUGMASK_GROUPNODE) {
				printf("gid=%u\tnid=%u\told=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] siz=%u wgt=%f\n",
				       layer.gid, nid,
				       pNew->oldId,
				       newSid, db.signatures[newSid].name,
				       newSlots[0], newSlots[1], newSlots[2], newSlots[3], newSlots[4], newSlots[5], newSlots[6], newSlots[7], newSlots[8],
				       db.signatures[newSid].size, pNew->weight);
			}
		}

		// group may not become empty
		assert(this->N[layer.gid].next != layer.gid);

		// group weight is min of all nodes
		assert(gWeight < 1.0 / 0.0);
		layer.loWeight = gWeight;
		layer.hiSlotId = gHiSlotId;
		this->N[layer.gid].weight = gWeight;

		/*
		 * @date 2022-01-03 00:18:25
		 * 
		 * If group has forward references, recreate group with new id to resolve forward
		 * This can happen when components get created out of order (will be fixed later)
		 * Or groups were merged and the highest id went lower.
		 */

		if (gHiSlotId > layer.gid && !allowForward) {
			/*
			 * create new list header
			 */

			uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zeroed
			assert(selfSlots[MAXSLOTS - 1] == 0);

			this->gcount++;
			uint32_t newGid = this->newNode(db.SID_SELF, selfSlots, 1.0 / 0.0); // +inf
			assert(newGid == this->N[newGid].slots[0]);

			this->N[newGid].gid   = newGid;
			this->N[newGid].oldId = this->N[layer.gid].oldId ? this->N[layer.gid].oldId : layer.gid;

			/*
			 * Walk and update the list
			 */

			if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("updategroup forward oldGid=%u newGid=%u\n", layer.gid, newGid);

			// relocate group
			linkNode(newGid, layer.gid);
			unlinkNode(layer.gid);

			// redirect to new group
			this->N[layer.gid].gid = newGid;

			// update gids
			for (uint32_t iNode = this->N[newGid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next)
				this->N[iNode].gid = newGid;

			layer.gid = newGid;
			// no need to `rebuild()`, as only some basics have changed
			this->N[newGid].weight = gWeight;
			this->N[newGid].hiSlotId = gHiSlotId;
		}
	}

	/*
	 * @date 2022-01-12 21:30:00
	 * 
	 * This is an expensive validation.
	 * And should only be needed in case the resolving rolls on forever.
	 * 
	 * It will multi-pass the tree, applying a flood to test if there are no loops
	 */
	void testAndUnlock(groupLayer_t &layer) {
		versionMemory_t *pVersion   = allocVersion();
		uint32_t        thisVersion = pVersion->nextVersion();

		// mark entrypoints
		for (uint32_t iNode = this->kstart; iNode < this->nstart; iNode++)
			pVersion->mem[iNode] = thisVersion;

		restart:

		// flood fill
		bool changed;
		do {
			changed = false;

			for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
				if (this->N[iGroup].gid != iGroup)
					continue; // not start of list

				// already processed?
				if (pVersion->mem[iGroup] == thisVersion)
					continue; // yes

				bool groupOk = true;

				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					groupNode_t *pNode         = this->N + iNode;
					unsigned    numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					// already processed?
					if (pVersion->mem[iNode] == thisVersion)
						continue; // yes

					bool nodeOk = true;

					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t id = updateToLatest(pNode->slots[iSlot]);
						if (pVersion->mem[id] != thisVersion) {
							nodeOk = false;
							break;
						}
					}

					// all references resolved
					if (nodeOk) {
						// yes, mark processed
						pVersion->mem[iNode] = thisVersion;
					} else {
						// no, group failed
						groupOk = false;
					}
				}

				// all nodes resolved
				if (groupOk) {
					// yes
					pVersion->mem[iGroup] = thisVersion;
					changed = true;
				}
			}

		} while (changed);

		/*
		 * @date 2022-01-14 15:54:21
		 * Scan for locked groups, and when found delete the locking nodes
		 */
		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			if (this->N[iGroup].gid != iGroup)
				continue; // not start of list

			// already processed?
			if (pVersion->mem[iGroup] == thisVersion)
				continue; // yes

			// Skip if group is single member
			if (this->N[iGroup].next == this->N[iGroup].prev)
				continue; // yes

			changed = false;

			for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode = this->N + iNode;

				// already processed?
				if (pVersion->mem[iNode] == thisVersion)
					continue; // yes

				showLine(pNode->gid, iNode, NULL, NULL, NULL);
				printf(" break-resolveForwards\n");

				iNode = orphanNode(layer, iNode); // returns previous node

				changed = true;
			}

			/*
			 * @date 2022-01-24 21:41:21
			 * this is nasty. unlinking a node might effect `hiSlotId`. Rescan to correct 
			 */
			if (changed) {
				double   gLoWeight = 1.0 / 0.0; // +inf
				uint32_t gHiSlotId = 0;

				// scan for highest id
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					groupNode_t *pNode = this->N + iNode;

					if (pNode->weight < gLoWeight)
						gLoWeight = pNode->weight;
					if (pNode->hiSlotId > gHiSlotId)
						gHiSlotId = pNode->hiSlotId;
				}

				// who to update
				if (layer.gid == iGroup) {
					// the layer
					layer.loWeight = gLoWeight;
					layer.hiSlotId = gHiSlotId;
				} else {
					// the group header
					this->N[iGroup].weight   = gLoWeight;
					this->N[iGroup].hiSlotId = gHiSlotId;
				}
			}

			// restart if locked nodes removed
			if (changed)
				goto restart;
		}

		freeVersion(pVersion);
	}

	/*
	 * @date 2021-11-11 23:19:34
	 * 
	 * Rebuild groups that have nodes that have forward references
	 * 
	 * NOTE: `layer` is only needed for the layer connectivity
	 */
	void resolveForwards(groupLayer_t &layer, uint32_t gstart) {

		assert(layer.gid != IBIT && layer.ucList == IBIT); // must have id
		assert(this->N[layer.gid].gid == layer.gid); // must be latest

		/*
		 * @date 2022-01-14 13:55:38
		 * In case this call hangs in a loop, include the group in the error message
		 */
		this->overflowGroup = gstart;

		uint32_t iGroup  = gstart;           // group being processed
		uint32_t firstId = gstart;           // lowest group in sweep

		groupLayer_t newLayer(*this, &layer); // separate layer for recursive calls 

		if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("resolveForwards gid=%u gstart=%u ncount=%u\n", layer.gid, gstart, this->ncount);

		if (gstart < this->nstart) {
			// for endpoints, sweep the whole tree
			firstId = iGroup = this->nstart;
		}

		/*
		 * Walk through tree and search for outdated groups
		 */

		uint32_t mark = this->ncount;
		unsigned numMark = 0;

		while (iGroup < this->ncount) {

			// stop at group headers
			if (this->N[iGroup].gid != iGroup) {
				iGroup++;
				continue;
			}

			/*
			 * Test if mark reached
			 */
			if (iGroup >= mark) {
				// yes
				mark = this->ncount;
				numMark++;

				if (numMark > 3) {
					//* DOUBLE paranoid, passed more than 3x through tree, which might indicate a cyclic loop
					// NOTE: if it does this too often, consider raising the threshold.
					fprintf(stderr, "[%s] checkLocked numMark=%u gstart=%u group=%u ncount=%u\n", ctx.timeAsString(), numMark, gstart, iGroup, this->ncount);
					testAndUnlock(layer);
					// restart
					iGroup = this->nstart;
					continue;
				}
			}

			assert(this->N[iGroup].next != iGroup); // group may not be empty

			/*
			 * @date 2022-01-25 11:49:09
			 * No need to update group if it only references the past
			 * 
			 * @date 2022-01-25 20:34:50
			 * If a referenced group merges, it's id goes lower.
			 * This leaves `hiSlotid` unchanged, making it undetected.
			 * This optimisation needs a different mechanism
			 */
			if (false && this->N[iGroup].hiSlotId > iGroup) {
				iGroup++;
				continue;
			}

			/*
			 * @date 2022-01-14 00:58:29
			 * restartId will only change as a result of a nested `mergeGroups()`.
			 * And it may point to the current `iGroup` in case a restart was requested
			 * 
			 * @date 2022-01-25 23:00:37
			 * 
			 * If iteration group is the layer, use that context
			 * Else use a scratch context
			 */
			uint32_t restartId  = this->ncount;

			if (layer.gid == iGroup) {
				// use 
				updateGroup(layer, &restartId, /*allowForward=*/false);
			} else {
				newLayer.gid = iGroup;
				updateGroup(newLayer, &restartId, /*allowForward=*/false);
			}

			// update lowest
			if (restartId < firstId)
				firstId = restartId;

			// if something merged, reposition to the start of the range
			if (restartId <= iGroup) {
				// yes, jump
				if (ctx.opt_debug & context_t::DEBUGMASK_GTRACE) printf("resolveForwards rewind=%u ncount=%u\n", restartId, this->ncount);
				iGroup = restartId;
				continue;
			} else {
				// no, continue to next node
				iGroup++;
			}
		}

//		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

		this->overflowGroup = 0;
	}

	void __attribute__((used)) whoHas(uint32_t id) {
		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			if (this->N[iGroup].gid != iGroup)
				continue; // not start of list

			for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode         = this->N + iNode;
				unsigned    numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

				for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
					if (updateToLatest(pNode->slots[iSlot]) == id) {

						printf("gid=%u\tnid=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] siz=%u wgt=%lf\n",
						       pNode->gid, iNode,
						       pNode->sid, db.signatures[pNode->sid].name,
						       pNode->slots[0], pNode->slots[1], pNode->slots[2], pNode->slots[3], pNode->slots[4], pNode->slots[5], pNode->slots[6], pNode->slots[7], pNode->slots[8],
						       db.signatures[pNode->sid].size, pNode->weight);
						continue;
					}
				}
			}
		}
	}

	void __attribute__((used)) listGroup(uint32_t iGroup) {
		if (this->N[iGroup].gid != iGroup || this->N[iGroup].next == iGroup)
			return;

		for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
			groupNode_t *pNode = this->N + iNode;

			showLine(pNode->gid, iNode, NULL, NULL, NULL);
			printf("\n");

		}
	}

	void __attribute__((used)) listHistory(uint32_t iNode) {

		uint32_t oldId = this->N[iNode].oldId;
		if (!oldId)
			return;

		for (uint32_t iNode = oldId; iNode < this->ncount; iNode++) {
			groupNode_t *pNode = this->N + iNode;

			if (pNode->oldId == oldId) {
				showLine(pNode->gid, iNode, NULL, NULL, NULL);
				printf("\n");
			}
		}
	}

	/*
	 * @date 2021-11-16 13:21:47
	 * 
	 * Apply signature based endpoint swapping to slots
	 * Return true if something changed
	 */
	bool applySwapping(uint32_t sid, uint32_t *pSlots) {
		const signature_t *pSignature = db.signatures + sid;

		bool anythingChanged = false;

		/*
		 * Apply endpoint swapping
		 */
		if (pSignature->swapId) {
			swap_t *pSwap = db.swaps + pSignature->swapId;

			bool changed;
			do {
				changed = false;

				for (unsigned iSwap = 0; iSwap < swap_t::MAXENTRY && pSwap->tids[iSwap]; iSwap++) {
					unsigned tid = pSwap->tids[iSwap];

					// get the transform string
					const char *pTransformSwap = db.fwdTransformNames[tid];

					// test if swap needed
					bool needSwap = false;

					for (unsigned i = 0; i < pSignature->numPlaceholder; i++) {
						const groupNode_t *pRhs = this->N + pSlots[pTransformSwap[i] - 'a'];
						int cmp = this->compare(pSlots[i], pRhs->sid, pRhs->slots, pRhs->weight);

						if (cmp > 0) {
							needSwap = true;
							break;
						}
						if (cmp < 0) {
							needSwap = false;
							break;
						}
					}

					if (needSwap) {
						uint32_t newSlots[MAXSLOTS];

						for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
							newSlots[i] = pSlots[pTransformSwap[i] - 'a'];
						for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
							pSlots[i] = newSlots[i];

						changed = anythingChanged = true;
					}
				}
			} while (changed);

			if (anythingChanged)
				this->cntApplySwapping++;
		}

		return anythingChanged;
	}

	/*
	 * @date 2021-12-02 16:12:21
	 * 
	 * Apply signature based endpoint swapping to slots.
	 * `char` version.
	 * Return true if something changed
	 */
	bool applySwapping(uint32_t sid, char *pSlots) {
		const signature_t *pSignature = db.signatures + sid;

		bool anythingChanged = false;

		/*
		 * Apply endpoint swapping
		 */
		if (pSignature->swapId) {
			swap_t *pSwap = db.swaps + pSignature->swapId;

			bool changed;
			do {
				changed = false;

				for (unsigned iSwap = 0; iSwap < swap_t::MAXENTRY && pSwap->tids[iSwap]; iSwap++) {
					unsigned tid = pSwap->tids[iSwap];

					// get the transform string
					const char *pTransformSwap = db.fwdTransformNames[tid];

					// test if swap needed
					bool needSwap = false;

					for (unsigned i = 0; i < pSignature->numPlaceholder; i++) {
						int cmp = (int) pSlots[i] - (int) pSlots[pTransformSwap[i] - 'a'];

						if (cmp > 0) {
							needSwap = true;
							break;
						}
						if (cmp < 0) {
							needSwap = false;
							break;
						}
					}

					if (needSwap) {
						char newSlots[MAXSLOTS];

						for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
							newSlots[i] = pSlots[pTransformSwap[i] - 'a'];
						for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
							pSlots[i] = newSlots[i];

						changed = anythingChanged = true;
					}
				}
			} while (changed);

			if (anythingChanged)
				this->cntApplySwapping++;
		}

		return anythingChanged;
	}

	/*
	 * @date 2021-12-20 00:18:27
	 * 
	 * 
	 * Apply signature based slot folding
 	 * Return true if something changed
	 */
	bool applyFolding(uint32_t *pSid, uint32_t *pSlots) {

		const signature_t *pSignature    = db.signatures + *pSid;
		unsigned          numPlaceholder = pSignature->numPlaceholder;

		if (numPlaceholder < 2)
			return false; // nothing to do

#if 1
		// may not be zero
		assert(numPlaceholder < 1 || pSlots[0] != 0);
		assert(numPlaceholder < 2 || pSlots[1] != 0);
		assert(numPlaceholder < 3 || pSlots[2] != 0);
		assert(numPlaceholder < 4 || pSlots[3] != 0);
		assert(numPlaceholder < 5 || pSlots[4] != 0);
		assert(numPlaceholder < 6 || pSlots[5] != 0);
		assert(numPlaceholder < 7 || pSlots[6] != 0);
		assert(numPlaceholder < 8 || pSlots[7] != 0);
		assert(numPlaceholder < 9 || pSlots[8] != 0);
		// test referencing to group headers
		assert(N[pSlots[0]].gid == pSlots[0]);
		assert(N[pSlots[1]].gid == pSlots[1]);
		assert(N[pSlots[2]].gid == pSlots[2]);
		assert(N[pSlots[3]].gid == pSlots[3]);
		assert(N[pSlots[4]].gid == pSlots[4]);
		assert(N[pSlots[5]].gid == pSlots[5]);
		assert(N[pSlots[6]].gid == pSlots[6]);
		assert(N[pSlots[7]].gid == pSlots[7]);
		assert(N[pSlots[8]].gid == pSlots[8]);
		// they may not be orphaned
		assert(pSlots[0] < nstart || N[pSlots[0]].next != pSlots[0]);
		assert(pSlots[1] < nstart || N[pSlots[1]].next != pSlots[1]);
		assert(pSlots[2] < nstart || N[pSlots[2]].next != pSlots[2]);
		assert(pSlots[3] < nstart || N[pSlots[3]].next != pSlots[3]);
		assert(pSlots[4] < nstart || N[pSlots[4]].next != pSlots[4]);
		assert(pSlots[5] < nstart || N[pSlots[5]].next != pSlots[5]);
		assert(pSlots[6] < nstart || N[pSlots[6]].next != pSlots[6]);
		assert(pSlots[7] < nstart || N[pSlots[7]].next != pSlots[7]);
		assert(pSlots[8] < nstart || N[pSlots[8]].next != pSlots[8]);
#endif

		bool changed, anythingChanged = false;
		do {
			changed = false;

			// bump versioned memory
			uint32_t thisVersion = ++slotVersionNr;
			if (thisVersion == 0) {
				// version overflow, clear
				memset(slotVersion, 0, this->maxNodes * sizeof(*slotVersion));

				thisVersion = ++slotVersionNr;
			}

			/*
			 * Finding the fold index:
			 * Entry  offset for first  occurrence [0, 1, 2, 3, 4,  5,  6,  7,  8]
			 * Add to offset for second occurrence [0, 0, 1, 3, 6, 10, 15, 21, 28] (geometric series)
			 */

			unsigned iFold = 0;
			for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
				uint32_t id = pSlots[iSlot];

				if (slotVersion[id] == thisVersion) {
					// found duplicate

					// finalise offset
					iFold += slotMap[id];

					// get folding sid/tid
					uint32_t foldSid = pSignature->folds[iFold].sid;
					uint32_t foldTid = pSignature->folds[iFold].tid;

					// update current
					*pSid = foldSid;
					pSignature     = db.signatures + *pSid;
					numPlaceholder = pSignature->numPlaceholder;

					// update slots
					uint32_t   tmpSlots[MAXSLOTS];
					const char *fwdTransform = db.fwdTransformNames[foldTid];

					for (unsigned j = 0; j < numPlaceholder; j++)
						tmpSlots[j] = pSlots[fwdTransform[j] - 'a'];
					for (unsigned j = 0; j < numPlaceholder; j++)
						pSlots[j] = tmpSlots[j];

					// zero padding
					for (unsigned j = numPlaceholder; j < MAXSLOTS; j++)
						pSlots[j] = 0;

					changed = anythingChanged = true;
					break;
				} else {
					// update first occurrence
					slotMap[id]     = iSlot;
					slotVersion[id] = thisVersion;
					// update second occurrence
					iFold += iSlot;
				}
			}
		} while (changed);

		if (anythingChanged)
			this->cntApplyFolding++;

		return anythingChanged;
	}

	void showLine(uint32_t iGroup, uint32_t iNode, const uint32_t *pSidCount, versionMemory_t *pFlood, versionMemory_t *pCycle) {
		const groupNode_t *pNode      = this->N + iNode;
		const signature_t *pSignature = db.signatures + pNode->sid;

		// test correct group
		if (pNode->gid != iGroup)
			printf("<GROUP=%u>", iGroup);

		printf("%u(%u)\t%u(%u)\t%u:%s/[",
		       pNode->gid, this->N[pNode->gid].oldId, iNode, pNode->oldId,
		       pNode->sid, pSignature->name);

		char delimiter = 0;

		for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			uint32_t id = pNode->slots[iSlot];

			if (delimiter)
				putchar(delimiter);
			delimiter = ' ';

			printf("%u", id);

			uint32_t latest = updateToLatest(id);

			if (latest != id)
				printf("<LATEST=%u>", latest);
			if (pFlood != NULL && pCycle != NULL && pFlood->mem[latest] == pFlood->version && pCycle->mem[latest] == pCycle->version)
				printf("<CYCLE>");


			if (latest == 0) {
				// slots references zero
				printf("<ZERO>");
			} else if (latest == iGroup) {
				// slots references own group
				printf("<SELF>");
			} else if (latest > iGroup) {
				// forward reference
				printf("<FORWARD>");
			}
		}
		printf("] wgt=%lf", pNode->weight);
		if (pSidCount != NULL && pSidCount[pNode->sid] > 1)
			printf("<MULTI>");
	}

	/*
	 * @date 2021-11-11 16:44:08
	 * 
	 * For debugging
	 */
	void validateTree(unsigned lineNr) {
		uint32_t        *pSidCount   = allocMap();
		versionMemory_t *pSidVersion = allocVersion();
		versionMemory_t *pNodeFound  = allocVersion();
		bool            anyError     = false;
		bool            hasForward   = false;

		/*
		 * Check nodes
		 */

		pNodeFound->nextVersion(); // bump version

		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			// find group headers
			if (this->N[iGroup].gid != iGroup)
				continue;

			pNodeFound->mem[iGroup] = pNodeFound->version; // mark node found

			// may not be empty
			if (this->N[iGroup].next == iGroup) {
				if (!anyError)
					printf("INVALIDTREE at line %u\n", lineNr);

				printf("<GID %u EMPTY>\n", iGroup);
				anyError = true;
				continue;
			}

			// count sids
			pSidVersion->nextVersion(); // bump version
			for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				const groupNode_t *pNode = this->N + iNode;
				if (pSidVersion->mem[pNode->sid] != pSidVersion->version)
					pSidCount[pNode->sid] = 1;
				else
					pSidCount[pNode->sid]++;
			}

			// does group have errors
			double   gWeight   = 1.0 / 0.0; // +inf
			uint32_t gHiSlotId = 0;
			for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				const groupNode_t *pNode         = this->N + iNode;
				unsigned          numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

				pNodeFound->mem[iNode] = pNodeFound->version; // mark node found

				bool error = false;

				// test correct group
				if (pNode->gid != iGroup)
					error = true;

				double   nWeight   = db.signatures[pNode->sid].size;
				uint32_t nHiSlotId = 0;

				for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
					uint32_t id = pNode->slots[iSlot];

					// must have valid weight
					nWeight += this->N[id].weight;
					if (id > nHiSlotId)
						nHiSlotId = id;

					uint32_t latest = updateToLatest(id);

					if (latest != id)
						error = true; // id not latest

					if (latest == 0) {
						// slots references zero
						error = true; // id zero
					} else if (latest == iGroup) {
						error = true;// self reference
					} else if (latest > iGroup) {
						error      = true; // forward
						hasForward = true;
					}
				}

				// node must have correct weight/hiSlotId
//				assert(pNode->weight == nWeight); // 2022-01-25 20:34:50/2022-01-26 15:54:07 related
				assert(pNode->hiSlotId == nHiSlotId);

				// must have valid weight
				assert(nWeight < 1.0 / 0.0);
				if (pNode->weight < gWeight)
					gWeight = pNode->weight;
				if (pNode->hiSlotId > gHiSlotId)
					gHiSlotId = pNode->hiSlotId;

				if (pSidCount[pNode->sid] > 1)
					error = true; // multi-sid

				if (error) {
					if (!anyError)
						printf("INVALIDTREE at line %u\n", lineNr);

					showLine(iGroup, iNode, pSidCount, /*pFlood=*/NULL, /*pCycle=*/NULL);
					printf("\n");
					anyError = true;
				}
			}

			// group must have correct weight/hiSlotId
//			assert(this->N[iGroup].weight == gWeight); // 2022-01-25 20:34:50/2022-01-26 15:54:07 related
			assert(this->N[iGroup].hiSlotId == gHiSlotId);
		}

		/*
		 * Check orphans
		 */

		for (uint32_t iNode = this->nstart; iNode < this->ncount; iNode++) {
			const groupNode_t *pNode = this->N + iNode;

			if (pNode->gid == IBIT)
				continue; // under construction
			if (pNodeFound->mem[iNode] == pNodeFound->version)
				continue; // already processed
			if (this->N[iNode].next == iNode)
				continue; // orphans live in solitude

			if (!anyError)
				printf("INVALIDTREE at line %u\n", lineNr);

			printf("<ORPHAN>\n");
			showLine(pNode->gid, iNode, /*pSidCount=*/NULL, /*pFlood=*/NULL, /*pCycle=*/NULL);
			printf("\n");

			anyError = true;
		}

		/*
		 * Display cycles
		 * Since it is an error situation, speed is a non-issue
		 * Maintain two flood-fills,
		 *   One is the low end of the cycle going up, 
		 *   The other is the high end going down.
		 *   
		 *   NOTE: too much output to be useful
		 */
		if (0 && hasForward) {
			// reuse resources
			versionMemory_t *pFlood = pSidVersion;
			versionMemory_t *pCycle = pNodeFound;

			printf("CYCLES:\n");

			uint32_t floodVersion = pFlood->nextVersion();
			uint32_t cycleVersion = pCycle->nextVersion();

			// find and mark the forwards
			for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
				if (this->N[iGroup].gid != iGroup)
					continue; // not a group header

				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode         = this->N + iNode;
					unsigned          numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t latest = updateToLatest(pNode->slots[iSlot]);

						// add new cycle to fill?
						if (latest > iGroup) {
							// yes
							pFlood->mem[iGroup] = floodVersion;
							pCycle->mem[latest] = cycleVersion;
						}

						// if node touches fill, then group touches fill
						if (pFlood->mem[latest] == floodVersion)
							pFlood->mem[iGroup] = floodVersion;
					}
				}
			}

			// Walk back tracking the actual cycle
			for (uint32_t iGroup = this->ncount - 1; iGroup >= this->nstart; --iGroup) {
				if (this->N[iGroup].gid != iGroup)
					continue; // not a group header

				// Is group part of the cycle
				if (pCycle->mem[iGroup] != cycleVersion)
					continue; // no, continue

				// does node touch fill	
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode         = this->N + iNode;
					unsigned          numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					// extend the cycle
					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t latest = updateToLatest(pNode->slots[iSlot]);
						pCycle->mem[latest] = cycleVersion;
					}
				}
			}

			// display nodes (in order)
			for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
				if (this->N[iGroup].gid != iGroup)
					continue; // not a group header

				// Is group part of the flood
				if (pFlood->mem[iGroup] != floodVersion)
					continue; // no, continue

				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode         = this->N + iNode;
					unsigned          numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t latest = updateToLatest(pNode->slots[iSlot]);

						// if node part of the flood
						if (pFlood->mem[latest] == floodVersion && pCycle->mem[latest] == cycleVersion) {
							showLine(pNode->gid, iNode, /*pSidCount=*/NULL, pFlood, pCycle);
							printf("\n");
							break;
						}
					}
				}
			}
		}

		/*
		 * @date 2022-01-13 23:54:25
		 * Resolve forward loop detect (Improved version of above)
		 * Although all nodes might have back-references,
		 *   groups have multiple nodes, and they can back-reference out-of-order,
		 *   creating the (rare) possibility that a path exists that does have a forward reference. 
		 */

		if (true) {
			versionMemory_t *pVersion   = pSidVersion; // NOTE: reuse storage
			uint32_t        thisVersion = pVersion->nextVersion();

			// mark entrypoints
			for (uint32_t iNode = this->kstart; iNode < this->nstart; iNode++)
				pVersion->mem[iNode] = thisVersion;

			// flood fill
			bool changed;
			do {
				changed = false;

				for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
					if (this->N[iGroup].gid != iGroup)
						continue; // not start of list

					// already processed?
					if (pVersion->mem[iGroup] == thisVersion)
						continue; // yes

					bool groupOk = true;

					for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
						groupNode_t *pNode         = this->N + iNode;
						unsigned    numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

						// already processed?
						if (pVersion->mem[iNode] == thisVersion)
							continue; // yes

						bool nodeOk = true;

						for (unsigned iSlot  = 0; iSlot < numPlaceholder; iSlot++) {
							uint32_t id = updateToLatest(pNode->slots[iSlot]);
							if (pVersion->mem[id] != thisVersion) {
								nodeOk = false;
								break;
							}
						}

						// all references resolved
						if (nodeOk) {
							// yes, mark processed
							pVersion->mem[iNode] = thisVersion;
						} else {
							// no, group failed
							groupOk = false;
						}
					}

					// all nodes resolved
					if (groupOk) {
						// yes
						pVersion->mem[iGroup] = thisVersion;
						changed = true;
					}
				}

			} while (changed);

			// all nodes filled
			bool found = false;
			for (uint32_t iNode = this->nstart; iNode < this->ncount; iNode++) {
				groupNode_t *pNode = this->N + iNode;

				// is it an active node?
				if (pNode->next == iNode || pNode->gid == iNode || pNode->gid == IBIT)
					continue; // no

				// already processed?
				if (pVersion->mem[iNode] != thisVersion) {
					// no
					if (!found) {
						printf("ERROR resolveForwards:\n");
						found = true;
					}
					showLine(pNode->gid, iNode, NULL, NULL, NULL);
					printf("\n");
				}
			}
			if (found)
				exit(1);
		}

		/*
		 * Done
		 */

		freeMap(pSidCount);
		freeVersion(pSidVersion);
		freeVersion(pNodeFound);

		if (anyError) {
			printf("<LASTGOOD=%u>\n", this->cntValidate);
			if (lineNr) {
				assert(!"Invalid tree");
			}
		}
		this->cntValidate++;
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
	 * @date 2021-12-12 12:24:22
	 * 
	 * Export/serialize a tree as string.
	 * This is a wrapper around `saveStringNode`, which is recursive, and handles allocation of shared resources.
	 * 
	 * NOTE: id/references are as-is and not updated to latest
	 */
	std::string saveString(uint32_t id, std::string *pTransform = NULL) {

		uint32_t        nextPlaceholder  = this->kstart;        // next placeholder for `pTransform`
		uint32_t        nextExportNodeId = this->nstart;        // next nodeId for exported name
		uint32_t        *pMap            = allocMap();          // maps internal to exported node id 
		versionMemory_t *pVersion        = allocVersion();      // version data for `pMap`.
		std::string     name;

		// bump version number
		pVersion->nextVersion();

		// call core code
		saveStringNode(id & ~IBIT, nextExportNodeId, name, pVersion, pMap, pTransform, nextPlaceholder);

		// test for inverted-root
		if (id & IBIT)
			name += '~';

		freeMap(pMap);
		freeVersion(pVersion);

		// return name
		return name;
	}

	/**
	 * @date 2021-12-12 12:24:22
	 * 
	 * Called by `saveString()` do serialize a tree node.
	 * 
	 * @param {uint32_t}  nid              - Id of node to export
	 * @param {string}    exportName       - Output being constructed
	 * @param {uint32_t&} nextExportNodeId - Next external node id to be assigned
	 * @param {versionMemory_t*} pVersion  - Version info for `pMap`
	 * @param {uint32_t*} pMap             - The external node id for internal nodes.
	 * @param {uint32_t*} pTransform       - Endpoints for transform placeholders [DISABLED WHEN NULL]
	 * @param {uint32_t&} nextPlaceholder  - Next `pTransform` placeholder to be assigned
	 */
	void saveStringNode(uint32_t nid, uint32_t &nextExportNodeId, std::string &exportName, versionMemory_t *pVersion, uint32_t *pMap, std::string *pTransform, uint32_t &nextPlaceholder) {

		// caller did `nextVersion()`, get (that) current version 
		uint32_t thisVersion = pVersion->version;

		/*
		 * Nodes for the exported structure described by this node. (analogue to `tinyTree_t::N[]`) 
		 * It contains node id's of the exported tree. 
		 * WARNING: To reduce recursive scope storage, the first node starts at 0 (instead of nstart).
		 */
		uint32_t localNodes[tinyTree_t::TINYTREE_MAXNODES];

		/*
		 * Number of active nodes in `localNode[]`.
		 * WARNING: Initial value is 0 because `localNode[]` starts at 0 (instead of nstart).
		 */
		unsigned nextLocalNodeId = 0;

		/*
		 * First node in group list is SID_SELF
		 */

		// update to latest only for orphaned groups
		while (nid >= this->nstart && nid == this->N[nid].next)
			nid = this->N[nid].gid;

		// list header is non-info. Skip to next node
		assert(this->N[nid].next != nid);
		if (nid >= this->nstart && this->N[nid].sid == db.SID_SELF) {
			/*
			 * @date 2022-01-20 00:57:37
			 * Find first node that matches group weight
			 */
			for (uint32_t iNode = this->N[nid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				if (this->N[iNode].weight == this->N[nid].weight) {
					nid = iNode;
					break;
				}
			}
			assert (this->N[nid].sid != db.SID_SELF);
		}

		/*
		 * Load string
		 */
		for (const char *pattern = db.signatures[this->N[nid].sid].name; *pattern; pattern++) {

			switch (*pattern) {
			case '0': //
				exportName += '0';
				break;

				// @formatter:off
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				// @formatter:on
			{
				/*
				 * Push-back reference
				 */
				// get node of local structure
				uint32_t v = nextLocalNodeId - (*pattern - '0');
				assert(v >= 0 && v < tinyTree_t::TINYTREE_MAXNODES);

				// determine distance as export would see it
				uint32_t dist = nextExportNodeId - localNodes[v];

				// convert (prefixed) back-link
				if (dist < 10) {
					exportName += (char) ('0' + dist);
				} else {
					encodePrefix(exportName, dist / 10);
					exportName += (char) ('0' + (dist % 10));
				}
				break;
			}

				// @formatter:off
			case 'a': case 'b': case 'c':
			case 'd': case 'e': case 'f':
			case 'g': case 'h': case 'i':
			case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o':
			case 'p': case 'q': case 'r':
			case 's': case 't': case 'u':
			case 'v': case 'w': case 'x':
			case 'y': case 'z':
				// @formatter:on
			{
				// get node id of export
				uint32_t gid = this->N[nid].slots[(unsigned) (*pattern - 'a')];

				// if endpoint then emit
				if (gid < this->nstart) {
					uint32_t value;

					if (!pTransform) {
						// endpoint
						value = gid - this->kstart;
					} else {
						// placeholder
						if (pVersion->mem[gid] != thisVersion) {
							pVersion->mem[gid] = thisVersion;
							pMap[gid]          = nextPlaceholder++;

							value = gid - this->kstart;
							if (value < 26) {
								*pTransform += (char) ('a' + value);
							} else {
								encodePrefix(*pTransform, value / 26);
								*pTransform += (char) ('a' + (value % 26));
							}
						}

						value = pMap[gid] - this->kstart;
					}

					// convert id to (prefixed) letter
					if (value < 26) {
						exportName += (char) ('a' + value);
					} else {
						encodePrefix(exportName, value / 26);
						exportName += (char) ('a' + (value % 26));
					}

					continue;
				}

				// determine if node already handled
				if (pVersion->mem[gid] != thisVersion) {
					// first time

					// Call recursive
					saveStringNode(gid, nextExportNodeId, exportName, pVersion, pMap, pTransform, nextPlaceholder);

					// link internal to external node id
					pVersion->mem[gid] = thisVersion;
					pMap[gid]          = nextExportNodeId - 1; // last *assigned* nodeId 

				} else {
					// back-reference to earlier node
					uint32_t dist = nextExportNodeId - pMap[gid];

					// convert id to (prefixed) back-link
					if (dist < 10) {
						exportName += (char) ('0' + dist);
					} else {
						encodePrefix(exportName, dist / 10);
						exportName += (char) ('0' + (dist % 10));
					}
				}

				break;

			}

			case '+': case '>': case '^': case '!':
			case '&': case '?': {
				// output operator
				exportName += *pattern;

				// assign it an external node id
				localNodes[nextLocalNodeId++] = nextExportNodeId++;
				break;
			}

			default:
				assert(0);
			}
		}
	}

	/*
	 * @date 2021-12-06 13:30:28 
	 * For debugging
	 */
	std::string __attribute__((used)) dumpGroup(uint32_t gid) {
		while (gid != this->N[gid].gid)
			gid = this->N[gid].gid;

		assert(gid == this->N[gid].gid);

		char itxt[16];
		sprintf(itxt, "%u", gid);
		std::string ret = "dumpgroup=";
		ret += itxt;
		ret += " ./eval";

		for (uint32_t iNode = this->N[gid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
			ret += " \"";
			ret += saveString(iNode);
			ret += "\"";
		}

		printf("%s\n", ret.c_str());
		return ret;
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
		uint32_t *transformList = (uint32_t *) ctx.myAlloc("groupTree_t::transformList", nstart, sizeof *transformList);

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

			uint32_t Q, Tu, Ti, F;

			switch (*pattern) {
			case '0': //
				/*
				 * Push zero
				 */
				pStack[numStack++] = 0;
				continue; // for

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
				continue; // for
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
				continue; // for
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
				continue; // for
			}

			case '+': {
				// OR (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				F  = pStack[--numStack];
				Tu = 0;
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '>': {
				// GT (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				F  = 0;
				Tu = pStack[--numStack];
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '^': {
				// XOR/NE (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				F  = pStack[--numStack];
				Tu = F;
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '!': {
				// QnTF (appreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				F  = pStack[--numStack];
				Tu = pStack[--numStack];
				Ti = IBIT;
				Q  = pStack[--numStack];
				break;
			}
			case '&': {
				// AND (depreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				F  = 0;
				Tu = pStack[--numStack];
				Ti = 0;
				Q  = pStack[--numStack];
				break;
			}
			case '?': {
				// QTF (depreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				F  = pStack[--numStack];
				Tu = pStack[--numStack];
				Ti = 0;
				Q  = pStack[--numStack];
				break;
			}
			case '~': {
				// NOT (support)
				if (numStack < 1)
					ctx.fatal("[stack underflow]\n");

				pStack[numStack - 1] ^= IBIT;
				continue; // for
			}

			case '/':
				// separator between pattern/transform
				while (pattern[1])
					pattern++;
				continue; // for
			case ' ':
				// skip spaces
				continue; // for
			default:
				ctx.fatal("[bad token '%c']\n", *pattern);
			}
			assert(numStack >= 0);

			/*
			 * Only arrive here when Q/T/F have been set 
			 */

			/*
			 * use the latest lists
			 */

			nid = addNormaliseNode(Q, Tu ^ Ti, F);

			uint32_t latest = nid;
			while (latest != this->N[latest].gid)
				latest = this->N[latest].gid;

			if (ctx.opt_debug & ctx.DEBUGMASK_GROUPEXPR)
				printf("### %s\n", saveString(latest).c_str());

			if (ctx.opt_debug & ctx.DEBUGMASK_GROUPNODE) {
				for (uint32_t iNode = this->N[latest].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					groupNode_t *pNode = this->N + iNode;
					printf("#gid=%u\tnid=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] wgt=%lf\n",
					       pNode->gid, iNode,
					       pNode->sid, db.signatures[pNode->sid].name,
					       pNode->slots[0], pNode->slots[1], pNode->slots[2], pNode->slots[3], pNode->slots[4], pNode->slots[5], pNode->slots[6], pNode->slots[7], pNode->slots[8],
					       pNode->weight);
				}
			}

			// remember
			pStack[numStack++] = nid;
			pMap[nextNode++]   = nid;

			if ((unsigned) numStack > maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (numStack != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[numStack - 1];

		freeMap(pStack);
		freeMap(pMap);
		if (transformList)
			ctx.myFree("groupTree_t::transformList", transformList);

		/*
		 * Return most recent group
		 */
		while (ret != this->N[ret].gid)
			ret = this->N[ret].gid;

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
		return loadStringSafe(pName, pSkin);
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
			ctx.fatal("groupTree_t::loadFile() on non-initial tree\n");

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

		fileHeader = (groupTreeHeader_t *) rawData;
		if (fileHeader->magic != GROUPTREE_MAGIC)
			ctx.fatal("baseTree version mismatch. Expected %08x, Encountered %08x\n", GROUPTREE_MAGIC, fileHeader->magic);
		if (fileHeader->offEnd != (uint64_t) stbuf.st_size)
			ctx.fatal("baseTree size mismatch. Expected %lu, Encountered %lu\n", fileHeader->offEnd, (uint64_t) stbuf.st_size);

		flags      = fileHeader->magic_flags;
		system     = fileHeader->system;
		maxDepth   = fileHeader->maxDepth;
		kstart     = fileHeader->kstart;
		ostart     = fileHeader->ostart;
		estart     = fileHeader->estart;
		nstart     = fileHeader->nstart;
		ncount     = fileHeader->ncount;
		numRoots   = fileHeader->numRoots;
		numHistory = fileHeader->numHistory;
		posHistory = fileHeader->posHistory;

		if (fileHeader->sidCRC != db.fileHeader.magic_sidCRC)
			ctx.fatal("database/tree sidCRC mismatch. Expected %08x, Encountered %08x\n", db.fileHeader.magic_sidCRC, fileHeader->sidCRC);

		// @date 2021-05-14 21:46:35 Tree is read-only
		maxNodes = ncount; // used for map allocations

		// primary
		N             = (groupNode_t *) (rawData + fileHeader->offNodes);
		roots         = (uint32_t *) (rawData + fileHeader->offRoots);
		history       = (uint32_t *) (rawData + fileHeader->offHistory);
		// pools
		pPoolMap      = (uint32_t **) ctx.myAlloc("groupTree_t::pPoolMap", MAXPOOLARRAY, sizeof(*pPoolMap));
		pPoolVersion  = (versionMemory_t **) ctx.myAlloc("groupTree_t::pPoolVersion", MAXPOOLARRAY, sizeof(*pPoolVersion));
		// slots
		slotMap       = allocMap();
		slotVersion   = allocMap(); // allocate as node-id map because of local version numbering
		slotVersionNr = 1;

		// make all `keyNames`+`rootNames` indices valid
		keyNames.resize(nstart);
		rootNames.resize(numRoots);

		// slice names
		{
			const char *pData = (const char *) (rawData + fileHeader->offNames);

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

		/*
		 * @date 2022-01-11 15:53:49
		 * Tree is read-only, and has no node index
		 */

		return 0;
	}

	/*
	 * @date 2021-05-13 12:06:33
	 *
	 * Save database to binary data file
	 * NOTE: Tree is compacted on writing
	 * NOTE: With larger trees over NFS, this may take fome time
	 * TODO: optional: sort lists before writing
	 * first entry of lists is always SID_SELF, with the
	 * SID_ZERO also included because it represents the reference value
	 * 0 as a value, reference and operator are inter changable. 
	 */
	void __attribute__((used)) saveFile(const char *fileName, bool showProgress = true) {
		assert(numRoots > 0);

		/*
		 * File header
		 */

		groupTreeHeader_t header;
		memset(&header, 0, sizeof header);

		// zeros for alignment
		uint8_t  zero16[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		// current file position
		size_t   fpos       = 0;
		// crc for nodes/roots
		uint32_t crc32      = 0;

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

		if (1) {
			/*
			 * In case of emergency and the tree needs to be saved verbatim
			 */

			// output entrypoints and nodes
			for (uint32_t iNode = 0; iNode < this->ncount; iNode++) {
				const groupNode_t *pNode = this->N + iNode;

				size_t len = sizeof(*pNode);
				fwrite(pNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(pNode->sid));
				for (unsigned i = 1; i < MAXSLOTS; i++)
					__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(pNode->slots[i]));

			}
			nextId = this->ncount;

		} else {

			/*
			 * Scan and assign output ids
			 */

			nextId = 0;

			for (uint32_t iGroup = 0; iGroup < this->ncount; iGroup++) {
				if (this->N[iGroup].gid != iGroup)
					continue; // not start of list

				assert(iGroup < this->nstart || this->N[iGroup].next != iGroup); // may not be empty

				// id header	
				pMap[iGroup] = nextId++;

				// list nodes
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode         = this->N + iNode;
					unsigned          numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					// id node
					pMap[iGroup] = nextId++;

					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t id = pNode->slots[iSlot];
						assert(this->N[id].gid == id); // must be latest
						assert(id < this->nstart || this->N[id].next != id); // may not be orphaned
						assert(id < iGroup); // may not be forward
					}
				}
			}

			/*
			 * Write nodes
			 */

			for (uint32_t iGroup = 0; iGroup < this->ncount; iGroup++) {
				if (this->N[iGroup].gid != iGroup)
					continue; // not start of list

				groupNode_t *pNode = this->N + iGroup;

				/*
				 * Output remapped header
				 */

				groupNode_t wrtNode;
				wrtNode.gid      = pMap[iGroup];
				wrtNode.prev     = pMap[pNode->prev];
				wrtNode.next     = pMap[pNode->next];
				wrtNode.sid      = pNode->sid;
				wrtNode.weight   = pNode->weight;
				wrtNode.hiSlotId = pNode->hiSlotId;

				for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++)
					wrtNode.slots[iSlot] = pMap[pNode->slots[iSlot]];

				size_t len = sizeof(wrtNode);
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.sid));
				for (unsigned i = 1; i < MAXSLOTS; i++)
					__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.slots[i]));

				/*
				 * Output remapped nodes
				 */

				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					pNode = this->N + iNode;

					wrtNode.gid      = pMap[iGroup];
					wrtNode.prev     = pMap[pNode->prev];
					wrtNode.next     = pMap[pNode->next];
					wrtNode.sid      = pNode->sid;
					wrtNode.weight   = pNode->weight;
					wrtNode.hiSlotId = pNode->hiSlotId;

					for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++)
						wrtNode.slots[iSlot] = pMap[pNode->slots[iSlot]];

					fwrite(&wrtNode, len, 1, outf);
					fpos += len;

					__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.sid));
					for (unsigned i = 1; i < MAXSLOTS; i++)
						__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.slots[i]));
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
		 * Last root is a virtual root representing "system"
		 */
		header.offRoots = fpos;

		for (unsigned iRoot = 0; iRoot < numRoots; iRoot++) {
			uint32_t R = roots[iRoot];

			// new wrtRoot
			uint32_t wrtRoot = pMap[R & ~IBIT] ^ (R & IBIT);

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

		header.magic       = GROUPTREE_MAGIC;
		header.magic_flags = flags;
		header.sidCRC      = db.fileHeader.magic_sidCRC;
		header.system      = pMap[system & ~IBIT] ^ (system & IBIT);
		header.crc32       = crc32;
		header.maxDepth    = this->maxDepth;
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
			ctx.fatal("groupTree_t::loadFileJson() on non-initial tree\n");

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

		json_object_set_new_nocheck(jResult, "flags", json_integer(fileHeader->magic_flags));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader->offEnd));
		json_object_set_new_nocheck(jResult, "maxdepth", json_integer(fileHeader->maxDepth));
		{
			char crcstr[32];
			sprintf(crcstr, "%08x", fileHeader->crc32);
			json_object_set_new_nocheck(jResult, "crc", json_string_nocheck(crcstr));
		}
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

		/*
		 * History
		 */

		json_t *jHistory = json_array();


		for (uint32_t i = 0; i < this->numHistory; i++) {
			json_array_append_new(jHistory, json_string_nocheck(keyNames[this->history[i]].c_str()));
		}

		json_object_set_new_nocheck(jResult, "history", jHistory);

		return jResult;
	}
};

#endif
