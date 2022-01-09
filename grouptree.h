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
#define GROUPTREE_MAGIC 0x20211102

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
	 * The size reduction of the database lookup.  
	 * `pattern.size - signature.size `
	 */
	uint32_t power;

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
	 * `slots` MUST be last to maintain version compatibility. 
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
	uint32_t version;	// Current version
	uint32_t numMemory;	// Number of elements
	uint32_t mem[];		// vector (size allocated on demand)

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
	uint32_t sidCRC;               // CRC of database containing sid descriptions
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
	#define GROUPTREE_DEFAULT_MAXDEPTH 0
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
		DEFAULT_MAXNODE = GROUPTREE_DEFAULT_MAXNODE,
		MAXPOOLARRAY = GROUPTREE_MAXPOOLARRAY,
		MAXHEAPNODE = GROUPTREE_MAXHEAPNODE,
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
	uint64_t                 cntMergeGroup;         // number of calls to `mergeGroup()`
	uint64_t                 cntAddNormaliseNode;   // number of calls to `addNormaliseNode()`
	uint64_t                 cntAddBasicNode;       // number of calls to `addbasicNode()`

	unsigned withPower;
	
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
		cntMergeGroup(0),
		cntAddNormaliseNode(0),
		cntAddBasicNode(0),
		withPower(0)
	{
	}

	/*
	 * Create a memory stored tree
	 */
	groupTree_t(context_t &ctx, database_t &db, uint32_t kstart, uint32_t ostart, uint32_t estart, uint32_t nstart, uint32_t numRoots, uint32_t maxNodes, uint32_t flags) :
		ctx(ctx),
		db(db),
		hndl(-1),
		rawData(NULL),
		fileHeader(NULL),
		// meta
		flags(flags),
		allocFlags(0),
		system(0),
		maxDepth(DEFAULT_MAXDEPTH),
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
		cntMergeGroup(0),
		cntAddNormaliseNode(0),
		cntAddBasicNode(0),
		withPower(0)
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

			pNode->gid  = iKey;
			pNode->next = iKey;
			pNode->prev = iKey;
			pNode->power  = 0;
			pNode->sid  = db.SID_SELF;
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
	int compare(uint32_t lhs, groupTree_t *treeR, uint32_t rhs) {
		assert(this == treeR && "to be implemented");

		if (lhs == rhs)
			return 0;

		if (lhs < this->nstart) {
			if (rhs >= this->nstart)
				return -1;
			else if (lhs < rhs)
				return -1;
			else
				return +1;
		} else if (rhs < this->nstart) {
			if (lhs >= this->nstart)
				return +1;
			else if (lhs < rhs)
				return -1;
			else
				return +1;

		}

		if (this->N[lhs].sid < treeR->N[rhs].sid) {
			return -1;
		} else if (this->N[lhs].sid > treeR->N[rhs].sid) {
			return +1;
		}

		/*
		 * SID_SELF needs special handling or it will recurse on itself 
		 */
		if (this->N[lhs].sid == db.SID_SELF) {
			if (this->N[lhs].slots[0] < treeR->N[rhs].slots[0]) {
				return -1;
			} else if (this->N[lhs].slots[0] > treeR->N[rhs].slots[0]) {
				return +1;
			} else {
				return 0;
			}
		}

		/*
		 * simple compare
		 * todo: cache results
		 * todo: nesting may be dead-code
		 */
		const signature_t *pSignature = db.signatures + this->N[lhs].sid;

		for (unsigned iSlot=0; iSlot<pSignature->numPlaceholder; iSlot++) {
			int ret = this->compare(this->N[lhs].slots[iSlot], treeR,  treeR->N[rhs].slots[iSlot]);
			if (ret != 0)
				return ret;
		}

		return 0;
	}

	/*
	 * @date 2021-11-10 18:27:04
	 * 
	 * variation that allows comparison with an anonymous node
	 */
	int compare(uint32_t lhs, uint32_t rhsSid, const uint32_t *rhsSlots) const {
		const groupNode_t *pLhs = this->N + lhs;

		// left-hand-side must be latest in latest group
		assert(this->N[pLhs->gid].gid == pLhs->gid);

		int cmp = (int)pLhs->sid - (int)rhsSid;
		if (cmp != 0)
			return cmp;

		/*
		 * simple compare
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
	 * @date  2021-11-10 00:05:51
	 * 
	 * Add node to list
	 */
	inline void linkNode(uint32_t headId, uint32_t nodeId) const {

		assert(headId != nodeId);
		assert(headId >= this->nstart);
		assert(nodeId >= this->nstart);

		groupNode_t *pHead  = this->N + headId;
		groupNode_t *pNode  = this->N + nodeId;

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
	inline uint32_t newNode(uint32_t sid, const uint32_t slots[], uint32_t power) {
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
			fprintf(stderr, "[OVERFLOW]\n");
			printf("{\"error\":\"overflow\",\"maxnode\":%d}\n", maxNodes);
			exit(1);
		}

		groupNode_t *pNode = this->N + nid;

		pNode->gid   = 0;
		pNode->next  = nid;
		pNode->prev  = nid;
		pNode->oldId = 0;
		pNode->power = power;
		pNode->sid   = sid;

		for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
			pNode->slots[iSlot] = slots[iSlot];
		}

		return nid;
	}

	/*
	 * @date 2021-12-21 20:22:46
	 * 
	 * Context containing resources for a group under construction.
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
		 * Also used as locking guard, to authenticate `pSidMap[]` and `minPower`. 
		 */
		uint32_t gid;

		/*
		 * Sid lookup index.
		 * To instantly find node with identical sids so they can challenge for better/worse.
		 * Nodes losing a challenge are orphaned.
		 */
		uint32_t        *pSidMap;
		versionMemory_t *pSidVersion;

		/*
		 * Minimal power levels for each wtructure size.
		 * Although TINYTREE_MAXNODES is 13, power applies to dataset size which is no higher than `5n9`.   
		 * 
		 * NOTE: only partly implemented until all stress tests have been successful.
		 */
		enum {
			LAYER_MAXNODE = 8 // taken from patternSecond_t.
		};
		unsigned        minPower[LAYER_MAXNODE];

		/*
		 * @date 2021-12-21 20:39:38
		 * 
		 * Constructor
		 */
		groupLayer_t(groupTree_t &tree, groupLayer_t *pPrevious) : tree(tree), pPrevious(pPrevious) {

			gid         = IBIT;
			pSidMap     = tree.allocMap();
			pSidVersion = tree.allocVersion();
			memset(minPower, 0, sizeof(minPower));

			// bump version
			pSidVersion->nextVersion();
		}

		/*
		 * @date 2021-12-21 20:48:56
		 * 
		 * Destructor
		 */
		~groupLayer_t() {
			tree.freeMap(pSidMap);
			tree.freeVersion(pSidVersion);
		}

		/*
		 * @date 2021-12-22 22:13:54
		 * 
		 * Set group id of current layer, and check it is unique
		 * 
		 * @date 2021-12-23 01:57:13
		 * 
		 * return false if gid is already under construction.
		 * when returned false, silently ignore 
		 * 
		 * @date 2022-01-03 01:06:31
		 * 
		 * Scans the group and prepares `pSidMap[]` and `minPower[]`.
		 */
		void setGid(uint32_t gid) {
			assert(tree.N[gid].gid == gid); // must be latest 			
// todo: re-enable			assert(findGid(gid) == NULL); // gid may not already be under construction 

			assert(gid != IBIT);
			this->gid = gid;

			rebuild();
		}

		/*
		 * @date  2022-01-05 17:29:57
		 * 
		 * Scan group and build indices
		 */
		void rebuild(void) {
			assert(this->gid != IBIT);

			// update gid
			this->gid = tree.updateToLatest(this->gid);
			
			// bump versioned memory
			this->pSidVersion->nextVersion();

			// clear power
			for (unsigned k = 0; k < LAYER_MAXNODE; k++)
				this->minPower[k] = 0;

			// scan group for initial sid lookup and power
			for (uint32_t iNode = tree.N[gid].next; iNode != tree.N[iNode].gid; iNode = tree.N[iNode].next) {
				groupNode_t       *pNode      = tree.N + iNode;
				const signature_t *pSignature = tree.db.signatures + pNode->sid;

				// load initial sid lookup index
				this->pSidMap[pNode->sid]          = iNode;
				this->pSidVersion->mem[pNode->sid] = this->pSidVersion->version;

				// update power levels
				if (tree.withPower) {
					//@formatter:off
					switch(pSignature->size) {
					case 0: if (this->minPower[0] < pNode->power) this->minPower[0] = pNode->power; // deliberate fall-through
					case 1: if (this->minPower[1] < pNode->power) this->minPower[1] = pNode->power;
					case 2: if (this->minPower[2] < pNode->power) this->minPower[2] = pNode->power;
					case 3: if (this->minPower[3] < pNode->power) this->minPower[3] = pNode->power;
					case 4: if (this->minPower[4] < pNode->power) this->minPower[4] = pNode->power;
					case 5: if (this->minPower[5] < pNode->power) this->minPower[5] = pNode->power;
					case 6: if (this->minPower[6] < pNode->power) this->minPower[6] = pNode->power;
					case 7: if (this->minPower[7] < pNode->power) this->minPower[7] = pNode->power;
						break;
					default:
						assert(0);
					}
					//@formatter:on
				}
			}
		}

		/*
		 * @date 2021-12-22 23:21:07
		 * 
		 * Find if gid as already under construction
		 */
		inline const groupLayer_t *findGid(uint32_t gid) const {
			for (const groupLayer_t *pLayer = this; pLayer; pLayer = pLayer->pPrevious) {
				if (tree.updateToLatest(pLayer->gid) == gid)
					return pLayer;
			}
			return NULL;
		}


		/*
		 * @date 2021-12-21 22:29:41
		 * 
		 * Find sid.
		 * Return IBIT if not found
		 */
		inline uint32_t findSid(uint32_t sid) const {
			assert(this->gid != IBIT);

			if (pSidVersion->mem[sid] != pSidVersion->version)
				return IBIT; // index not loaded

			uint32_t nid = pSidMap[sid];
			if (tree.N[nid].next == nid)
				return IBIT; // node was orphaned


			assert(tree.N[nid].gid == this->gid);
			return nid;
		}

	};


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
	 * @param {uint32_t*}    pPower  - structure size difference between what was detected and the signature.
	 * @return {uint32_t}            - signature id, 0 if none found or folded..
	 */
	uint32_t constructSlots(groupLayer_t &layer, const groupNode_t *pNodeQ, const groupNode_t *pNodeT, uint32_t Ti, const groupNode_t *pNodeF, uint32_t *pFinal, uint32_t *pPower) {

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
		char        slotsT[MAXSLOTS + 1];
		char        slotsF[MAXSLOTS + 1];
		// resulting slots containing gids
		uint32_t    slotsR[MAXSLOTS];
		// slotsR entries in use
		unsigned    nextSlot = 0;
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

			// is it a self-collapse
			if (endpoint == layer.gid)
				return 0; // yes

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

			// is it a self-collapse
			if (endpoint == layer.gid)
				return 0; // yes

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

			// is it a self-collapse
			if (endpoint == layer.gid)
				return 0; // yes

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

		// extract

		for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			pFinal[iSlot] = slotsR[pTransformExtract[iSlot] - 'a'];
			assert(pFinal[iSlot] != 0);
		}
		for (unsigned iSlot = pSignature->numPlaceholder; iSlot < MAXSLOTS; iSlot++)
			pFinal[iSlot] = 0;

		/*
		 * Apply endpoint swapping
		 */
		if (pSignature->swapId)
			applySwapping(pSecond->sidR, pFinal);

		// don't forget power
		*pPower = pSecond->power;

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
	 */
	uint32_t __attribute__((used)) expandSignature(groupLayer_t &layer, uint32_t sid, const uint32_t *pSlots, unsigned depth) {

		signature_t *pSignature    = db.signatures + sid;
		unsigned    numPlaceholder = pSignature->numPlaceholder;

		// group id must be latest
		assert(layer.gid == IBIT || layer.gid == this->N[layer.gid].gid);

		/*
		 * init
		 */

		unsigned        numStack    = 0;
		uint32_t        nextNode    = this->nstart;
		uint32_t        *pStack     = allocMap(); // evaluation stack
		uint32_t        *pMap       = allocMap(); // node id of intermediates
		versionMemory_t *pActive    = allocVersion(); // collection of used id's
		uint32_t        thisVersion = pActive->nextVersion(); // bump versioned memory

		// component sid/slots
		uint32_t cSid             = 0; // 0=error/folded
		uint32_t cSlots[MAXSLOTS] = {0}; // zero contents
		assert(cSlots[MAXSLOTS - 1] == 0);

		// mark slots to test against to detect structure collapses
		// intermediates must NOT be mentioned in this collection 
		for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
			uint32_t id = updateToLatest(pSlots[iSlot]);

			pActive->mem[id] = thisVersion;
		}

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

			// NOTE: `cSid = 0` means fold
			if (Q == 0) {
				// level-1 fold
				cSid = 0;
			} else if (Ti) {
				if (Tu == 0) {
					if (Q == F || F == 0) {
						// [ 0] a ? !0 : 0  ->  a
						// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
						cSid = 0;
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
						cSid = 0;
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
						cSid = 0;
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
						cSid = 0;
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
						printf("<endpoint line=%u/>\n", __LINE__);
						return IBIT ^ F; // todo: NOTE: 
						assert(0); // already tested
					} else {
						// [19] a ?  b : c             "?" QTF
						cSid = db.SID_QTF;
					}
				}
			}

			// have operands folded?
			if (cSid == 0 || Q == layer.gid || Tu == layer.gid || F == layer.gid) {
				// yes
				freeMap(pStack);
				freeMap(pMap);
				freeVersion(pActive);
				return IBIT ^ (IBIT - 1); // caller needs to silently ignore
			}

			uint32_t nid;
			if (pattern[1]) {

				// allocate storage for scope
				groupLayer_t newLayer(*this, &layer);

				// call
				nid = addBasicNode(newLayer, cSid, Q, Tu, Ti, F, /*isTopLevel=*/false, depth + 1);

				// did something happen?
				if (nid & IBIT) {
					// yes, let caller handle what
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);
					
					assert(nid == (IBIT ^ (IBIT - 1))); // does this happen

					/*
					 * @date 2021-12-29 00:15:14
					 * for components: Any kind of collapse should invalidate the final candidate structure. 
					 */
					return IBIT ^ (IBIT - 1);
				}

				uint32_t latest = updateToLatest(nid);

				// did it fold into one of the slot entries or gid?
				if (latest < this->nstart || latest == layer.gid || pActive->mem[latest] == thisVersion) {
					// yes, caller needs to silently ignore
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);
					return IBIT ^ (IBIT - 1);
				}

				// add intermediate to collection of rejects
				pActive->mem[latest] = thisVersion;

			} else {
				assert(numStack == 0);
				
				// refresh layer if outdated
				uint32_t latest = updateToLatest(layer.gid);
				if (layer.gid != latest)
					layer.rebuild();

				// NOTE: top-level, use same depth/indent as caller
				nid = addBasicNode(layer, cSid, Q, Tu, Ti, F, /*isTopLevel=*/true, depth);

				// did something happen?
				if (nid & IBIT) {
					// yes, let caller handle what
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);
					return nid;
				}

				latest = updateToLatest(nid);

				// did it fold into one of the slot entries?
				if (latest < this->nstart || pActive->mem[latest] == thisVersion) {
					// yes, caller needs to silently ignore
					assert(0); // return IBIT ^ entrypoint
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);
					return IBIT ^ (IBIT - 1);
				}
			}

			// Push result onto stack
			pStack[numStack++]   = nid;
			pMap[nextNode++]     = nid;
		}
		assert(numStack == 1); // only result on stack

		// release and return
		uint32_t ret = pStack[0];

		freeMap(pStack);
		freeMap(pMap);
		freeVersion(pActive);

		return ret;
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

		unsigned        numStack    = 0;
		uint32_t        nextNode    = this->nstart;
		uint32_t        *pStack     = allocMap(); // evaluation stack
		uint32_t        *pMap       = allocMap(); // node id of intermediates
		versionMemory_t *pActive    = allocVersion(); // collection of used id's
		uint32_t        thisVersion = pActive->nextVersion(); // bump versioned memory

		// component sid/slots
		uint32_t cSid             = 0; // 0=error/folded
		uint32_t cSlots[MAXSLOTS] = {0}; // zero contents
		assert(cSlots[MAXSLOTS - 1] == 0);

		// mark slots to test against to detect structure collapses
		// intermediates must NOT be mentioned in this collection 
		for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
			uint32_t id = updateToLatest(pSlots[iSlot]);

			pActive->mem[id] = thisVersion;
		}

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

			// NOTE: `cSid = 0` means fold
			if (Q == 0) {
				// level-1 fold
				cSid = 0;
			} else if (Ti) {
				if (Tu == 0) {
					if (Q == F || F == 0) {
						// [ 0] a ? !0 : 0  ->  a
						// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
						cSid = 0;
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
						cSid = 0;
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
						cSid = 0;
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
						cSid = 0;
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
						printf("<endpoint line=%u/>\n", __LINE__);
						return IBIT ^ F; // todo: NOTE: 
						assert(0); // already tested
					} else {
						// [19] a ?  b : c             "?" QTF
						cSid = db.SID_QTF;
					}
				}
			}

			// have operands folded?
			if (cSid == 0 || Q == layer.gid || Tu == layer.gid || F == layer.gid) {
				// yes
				freeMap(pStack);
				freeMap(pMap);
				freeVersion(pActive);
				return IBIT ^ (IBIT - 1); // caller needs to silently ignore
			}

			uint32_t nid;
			if (pattern[1]) {

				// allocate storage for scope
				groupLayer_t newLayer(*this, &layer);

				// call
				nid = addBasicNode(newLayer, cSid, Q, Tu, Ti, F, /*isTopLevel=*/false, depth + 1);

				// did something happen?
				if (nid & IBIT) {
					// yes, let caller handle what
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);

					assert(nid == (IBIT ^ (IBIT - 1))); // does this happen

					/*
					 * @date 2021-12-29 00:15:14
					 * for components: Any kind of collapse should invalidate the final candidate structure. 
					 */
					return IBIT ^ (IBIT - 1);
				}

				uint32_t latest = updateToLatest(nid);

				// did it fold into one of the slot entries or gid?
				if (latest < this->nstart || latest == layer.gid || pActive->mem[latest] == thisVersion) {
					// yes, caller needs to silently ignore
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);
					return IBIT ^ (IBIT - 1);
				}

				// add intermediate to collection of rejects
				pActive->mem[latest] = thisVersion;

			} else {
				assert(numStack == 0);
				
				// refresh layer if outdated
				uint32_t latest = updateToLatest(layer.gid);
				if (layer.gid != latest)
					layer.rebuild();

				// NOTE: top-level, use same depth/indent as caller
				nid = addBasicNode(layer, cSid, Q, Tu, Ti, F, /*isTopLevel=*/true, depth);

				// did something happen?
				if (nid & IBIT) {
					// yes, let caller handle what
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);
					return nid;
				}

				latest = updateToLatest(nid);

				// did it fold into one of the slot entries?
				if (latest < this->nstart || pActive->mem[latest] == thisVersion) {
					// yes, caller needs to silently ignore
					assert(0); // return IBIT ^ entrypoint
					freeMap(pStack);
					freeMap(pMap);
					freeVersion(pActive);
					return IBIT ^ (IBIT - 1);
				}
			}

			// Push result onto stack
			pStack[numStack++]   = nid;
			pMap[nextNode++]     = nid;
		}
		assert(numStack == 1); // only result on stack

		// release and return
		uint32_t ret = pStack[0];

		freeMap(pStack);
		freeMap(pMap);
		freeVersion(pActive);

		return ret;
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
		uint32_t ret = addBasicNode(layer, tlSid, Q, Tu, Ti, F, /*isTopLevel=*/false, /*depth=*/0);

		// regular calls may not collapse or ignore
		assert(!(ret & IBIT));

		validateTree(__LINE__);

		return ret;
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
	 * @param {uint32_t}         gid - group id to add node to. May be IBIT to create new group
	 * @param {uint32_t}         tlSid - `1n9` sid describing Q/T/F
	 * @param {uint32_t}         Q - component
	 * @param {uint32_t}         Tu - component
	 * @param {uint32_t}         Ti - T is inverted
	 * @param {uint32_t}         F - component
	 * @param {uint32_t*}        pSidMap - Lookup index for all sids in group
	 * @param {versionMemory_t*} pSidVersion - versioned memory for `pSidMap`
	 * @param {unsigned}         depth - Recursion depth
	 * @return {number} newly created node Id, or IBIT when collapsed.
	 */
	uint32_t addBasicNode(groupLayer_t &layer, uint32_t tlSid, uint32_t Q, uint32_t Tu, uint32_t Ti, uint32_t F, bool isTopLevel, unsigned depth) {
		this->cntAddBasicNode++;

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			fprintf(stderr, "\r\e[K[%s] cntAddNormaliseNode=%lu cntAddBasicNode=%lu ncount=%u gcount=%u | cntOutdated=%lu cntRestart=%lu cntUpdateGroupCollapse=%lu cntUpdateGroupMerge=%lu cntApplySwapping=%lu cntApplyFolding=%lu cntMergeGroup=%lu\n", ctx.timeAsString(),
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
				this->cntMergeGroup
			);
			ctx.tick = 0;
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
	
		// arguments may not fold to gid
		if (Q == layer.gid || Tu == layer.gid || F == layer.gid) {
			// this implies that layer.gid != IBIT
			printf("<argument-collapse gid=%u Q=%u T=%u F=%u/>\n", layer.gid, Q, Tu, F); // how often does this happen
			return IBIT ^ layer.gid; // collapse to argument
		}

		uint32_t tlSlots[MAXSLOTS] = {0}; // zero contents
		assert(tlSlots[MAXSLOTS - 1] == 0);

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
		} else if (tlSid == db.SID_GT || tlSid == db.SID_AND) {
			tlSlots[0] = Q;
			tlSlots[1] = Tu;
		} else {
			tlSlots[0] = Q;
			tlSlots[1] = Tu;
			tlSlots[2] = F;
		}

		// test if node already exists
		uint32_t nix = this->lookupNode(tlSid, tlSlots);
		uint32_t nid = this->nodeIndex[nix];
		if (nid != 0) {
			// is it under construction?
			if (this->N[nid].gid == IBIT) {
				// yes
				assert(depth > 0); // must be a recursve call
				return IBIT ^ (IBIT - 1); // silently ignore
			}
				
			// (possibly outdated) node already exists, test if same group
			assert(this->N[nid].gid != IBIT);

			// Cartesian product hasn't started yet, smiple return
			return nid;
		}

		/*
		 * When hitting deepest depth, simply create node 
		 */
		if (depth >= this->maxDepth) {

			// create group header
			uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zerod
			assert(selfSlots[MAXSLOTS - 1] == 0);

			this->gcount++;
			uint32_t gid = this->newNode(db.SID_SELF, selfSlots, /*power*/ 0);
			assert(gid == this->N[gid].slots[0]);
			this->N[gid].gid = gid;

			// create node
			nid = this->newNode(tlSid, tlSlots, 0); // todo: what for power?
			groupNode_t *pNode = this->N + nid;

			pNode->gid = gid;

			// add to list
			linkNode(gid, nid);

			// add node to index
			this->nodeIndex[nix]        = nid;
			this->nodeIndexVersion[nix] = this->nodeIndexVersionNr;

			return nid;
		}
		depth++;

		/* 
		 * Before adding a new node to current group, check if it would make a chance to win the challenge.
		 */
		if (layer.gid != IBIT) {
			// does group have a node with better sid?
			uint32_t challenge = layer.findSid(tlSid);
			if (challenge != IBIT) {
				int cmp = this->compare(challenge, tlSid, tlSlots);

				if (cmp < 0) {
					// existing is better
					return challenge;
				} else if (cmp == 0) {
					assert(0); // should have been detected by `lookupNode()`
				}
			}
		}

		/*
		 * @date 2021-11-04 02:08:51
		 * 
		 * Second step: create Cartesian products of Q/T/F group lists
		 */

		/*
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
					// iterators may not be orphaned
					assert(iQ < this->nstart || this->N[iQ].next != iQ);
					assert(iTu < this->nstart || this->N[iTu].next != iTu);
					assert(iF < this->nstart || this->N[iF].next != iF);
					// iterators may not be current group
					assert(this->N[iQ].gid != layer.gid);
					assert(this->N[iTu].gid != layer.gid);
					assert(this->N[iF].gid != layer.gid);
					// iterators must be in up-to-date
					assert(this->N[iQ].gid == this->N[this->N[iQ].gid].gid);
					assert(this->N[iTu].gid == this->N[this->N[iTu].gid].gid);
					assert(this->N[iF].gid == this->N[this->N[iF].gid].gid);
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

					// is there a current group
					if (isTopLevel)
						return IBIT ^ folded; // yes, let caller handle collapse to iterator

					// collapse to iterator and update
					mergeGroups(layer, folded);
					resolveForward(layer, layer.gid);

					return folded;
				}

				/*
				 * Build slots and lookup signature
				 */
				uint32_t finalSlots[MAXSLOTS];
				uint32_t power;
				uint32_t sid = constructSlots(layer, this->N + normQ, this->N + normTu, normTi, this->N + normF, finalSlots, &power);

				// combo not found or folded
				if (sid == 0)
					continue; // yes, silently ignore

				/*
				 * Test for an endpoint collapse, which collapses the group as whole
				 */
				if (sid == db.SID_ZERO || sid == db.SID_SELF) {
					uint32_t endpoint = (sid == db.SID_ZERO) ? 0 : finalSlots[0];

					// is this called recursively?
					if (isTopLevel)
						return IBIT ^ endpoint; // yes, let caller handle collapse to endpoint

					// collapse to endpoint and update
					if (layer.gid != IBIT) {
						mergeGroups(layer, endpoint);
						resolveForward(layer, layer.gid);
					}

					return endpoint;
				}


				// lookup slots
				nix  = this->lookupNode(sid, finalSlots);
				nid = this->nodeIndex[nix];
				
				/*
				 * @date 2022-01-09 15:39:52
				 * Ignore nodes under construction
				 */
				if (this->N[nid].gid == IBIT)
					continue; // silently ignore

				uint32_t latest = updateToLatest(nid);

				if (nid != 0 && (latest == Q || latest == Tu || latest == F)) {
					/*
					 * Iterator collapse.
					 * Iterators are endpoints, making this a group-collapse to `a/[id]`. 
					 */

					// is this called recursively?
					if (isTopLevel)
						return IBIT ^ latest; // yes, let caller handle collapse

					return latest;
				}

				/*
				 * Try to create node
				 */

				nid = addToGroup(layer, nix, nid, sid, finalSlots, power);

				// was there a collapse?
				if (nid & IBIT) {
					// yes

					// self-collapse?
					if (nid == (IBIT ^ (IBIT - 1)))
						continue; // yes, silently ignore

					// is this called recursively?
					if (isTopLevel)
						return nid; // yes, let caller handle collapse

					uint32_t endpoint = nid & ~IBIT;

					// merge and update groups
					mergeGroups(layer, endpoint);
					resolveForward(layer, layer.gid);

					return endpoint;
				}

				// was node freshly created?
				if (this->N[nid].gid == IBIT) {
					// yes

					// is there a current group?			
					if (layer.gid == IBIT) {
						// "node is new and no current group"

						// create group header
						uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zerod
						assert(selfSlots[MAXSLOTS - 1] == 0);

						this->gcount++;
						uint32_t gid = this->newNode(db.SID_SELF, selfSlots, /*power*/ 0);
						assert(gid == this->N[gid].slots[0]);
						this->N[gid].gid = gid;

						layer.setGid(gid); // set layer to empty gid
					}

					// finalise node
					groupNode_t *pNew = this->N + nid;

					// add node to list
					pNew->gid = layer.gid;
					linkNode(this->N[layer.gid].prev, nid);

					// update champion
					{
						// Orphan any prior champion
						uint32_t challenge = layer.findSid(sid);
						if (challenge != IBIT)
							unlinkNode(challenge);

						// add to sid lookup index
						layer.pSidMap[sid]          = nid;
						layer.pSidVersion->mem[sid] = layer.pSidVersion->version;
					}

					if (ctx.opt_debug & ctx.DEBUGMASK_GROUPNODE) {
						printf("%.*sgid=%u\tnid=%u\tQ=%u\tT=%u\tF=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] siz=%u pwr=%u\n",
						       depth, "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t",
						       layer.gid, nid,
						       iQ, iTu, iF,
						       sid, db.signatures[sid].name,
						       finalSlots[0], finalSlots[1], finalSlots[2], finalSlots[3], finalSlots[4], finalSlots[5], finalSlots[6], finalSlots[7], finalSlots[8],
						       db.signatures[sid].size, power);
					}
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
			 * this might happen when `mergeGroup()` involves an iterator group
			 */

			bool changed = false;

			if (this->N[Q].gid != Q) {
				// group change
				iQ      = Q = updateToLatest(this->N[iQ].gid);
				changed = true;
			} else if (this->N[iQ].next == iQ && iQ >= this->nstart) {
				// orphaned
				iQ      = Q;
				changed = true;
			}

			if (this->N[Tu].gid != Tu) {
				// group change
				iTu     = Tu = updateToLatest(this->N[iTu].gid);
				changed = true;
			} else if (this->N[iTu].next == iTu && iTu >= nstart) {
				// orphaned
				iTu     = Tu;
				changed = true;
			}

			if (Tu != F) {
				if (this->N[F].gid != F) {
					// group change
					iF      = F = updateToLatest(this->N[iF].gid);
					changed = true;
				} else if (this->N[iF].next == iF && iF >= nstart) {
					// orphaned
					iF      = F;
					changed = true;
				}
			}

			if (changed)
				continue;

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

		/*
		 * @date 2022-01-03 14:10:19
		 * todo: Temporary hack to migrate to resolve forward)
		 */
		if (layer.gid != IBIT)
			resolveForward(layer, layer.gid);

		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

		assert(layer.gid != IBIT);
		if (depth + 1 < this->maxDepth) {
			/*
			 * @date 2022-01-01 00:28:49
			 * Expand primary nodes. This is an important step of compressing and sorting trees
			 */
			if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

			for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				const groupNode_t *pNode      = this->N + iNode;
				const signature_t *pSignature = db.signatures + pNode->sid;

				// did iterator orphan or change
				if (this->N[iNode].next == iNode || this->N[iNode].gid != layer.gid) {
					// yes, restart
					iNode = layer.gid;
					continue;
				}

				if (pSignature->size > 1) {
					if (true) {
						uint32_t expand = expandSignature(layer, pNode->sid, pNode->slots, depth + 1);
//					for (uint32_t mid = db.signatures[pNode->sid].firstMember; mid != 0; mid = db.members[mid].nextMember) {
//						uint32_t expand = expandMember(layer, mid, pNode->slots, depth + 1);

						/*
						 * @date 2022-01-05 17:27:20
						 * expandSignature()/expandMember() will invalidate all layers
						 */
						if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);
						layer.rebuild();

						// is it a collapse?
						if (expand & IBIT) {
							// yes, was it a self-collapse?
							if (expand == (IBIT ^ (IBIT - 1)))
								continue; // silently ignore

							uint32_t endpoint = expand & ~IBIT;
							uint32_t latest   = updateToLatest(endpoint);

							// is this called recursively?
							if (isTopLevel)
								return expand; // yes, let caller handle collapse

							// merge and update groups
							mergeGroups(layer, latest);
							resolveForward(layer, layer.gid);

							return endpoint;
						}

						uint32_t latest = updateToLatest(expand);

						// test for entrypoint-collapse 
						if (latest < this->nstart) {
							// yes

							// is this called recursively?
							if (isTopLevel)
								return IBIT ^ latest; // yes, let caller handle collapse to entrypoint

							// merge and update groups
							mergeGroups(layer, latest);
							resolveForward(layer, layer.gid);

							return expand;
						}

						// group management
						if (layer.gid != latest) {
							// merge groups
							mergeGroups(layer, latest);
							resolveForward(layer, layer.gid);
						}
					}
				}
			}
		}

		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

		// return group id
		return layer.gid;
	}

	/*
	 * @date 2022-01-07 12:55:28
	 * 
	 * Add sid/slots to group.
	 * 
 	 * Callers needs to do this, and test slot entries against self-collapse
 	 * 
	 * ```
	 *  // lookup slots
	 *  uint32_t nix  = this->lookupNode(sid, finalSlots);
	 *  uint32_t nid = this->nodeIndex[nix];
	 *  uint32_t latest = updateToLatest(nid);
	 *  if (nid != 0 && (latest == Q || latest == Tu || latest == F))
	 *      selfCollapse();
	 * ``` 
	 * 
	 * Returns:
	 *	IBIT ^ (IBIT - 1), self-collapse/silently ignore
	 *	IBIT ^ entrypoint, group-collapse
	 *	IBIT ^ endpoint, group-collapse
	 *	id with gid=IBIT, newly created node
	 *	id with gid!=IBIT, existing node
	 *	
	 * @date 2022-01-08 19:15:54
	 * 
	 *  It is the responsibility of the caller to:
	 *  - link new nodes to a list
	 *  - add new nodes to the sid lookup index 
	 */
	uint32_t addToGroup(groupLayer_t &layer,
			    uint32_t nix, uint32_t nid,
			    uint32_t sid, const uint32_t *pSlots, unsigned power) {

		signature_t *pSignature = db.signatures + sid;

		/*
		 * @date 2021-12-25 00:53:05
		 * Detect if there is enough
		 */
		if (this->withPower && power < layer.minPower[pSignature->size])
			return IBIT ^ (IBIT - 1);

		/*
		 * General idea:
		 * 
		 * node is old and no current group:
		 *      attach to group
		 * node is old and belongs to same group
		 *      skip duplicate
		 * node is old and belongs to different group:
		 * 	merge groups
		 * node is new and no current group:
		 *      create group and add as first node
		 * node is new and current group
		 *      add to group
		 */

		if (nid != 0) {
			// node is old/existing
			uint32_t latest = updateToLatest(nid);

			if (this->N[nid].gid == IBIT) {
				// "node is under construction"
				assert(0); // does this happen?
				// ignore until made visable
				return IBIT ^ (IBIT - 1); // silently ignore
	
			} else if (latest < this->nstart) {
				// entrypoint collapse

				return IBIT ^ latest; // entrypoint collapse

			} else if (layer.gid == IBIT) {
				// "node is old and no current group"

				// attach to that group
				layer.setGid(latest);

				return latest; // existing node

			} else if (layer.gid != latest) {
				// "node is old and belongs to different group"

				return IBIT ^ latest; // group merge

			} else if (this->N[nid].next == nid) {
				// "node is old and orphaned"
				// if it was rejected then, it will be rejected now

				return IBIT ^ (IBIT - 1); // silently ignore

			} else {
				// "node is old and belongs to same group"
				uint32_t challenge = layer.findSid(sid);
				if (challenge != IBIT && challenge != nid) {
					/*
					 * Champion is outdated
					 * Might happen when called from `updateGroup()` after groups were merged
					 */
					assert(this->N[challenge].next != challenge);
					unlinkNode(challenge);
				}
				// update champion
				layer.pSidMap[sid]          = nid;
				layer.pSidVersion->mem[sid] = layer.pSidVersion->version;

				// duplicate
				return IBIT ^ (IBIT - 1); // silently ignore
			}

		}

		// connected to group?
		if (layer.gid != IBIT) {
			// yes, challenge the champion?
			uint32_t challenge = layer.findSid(sid);
			if (challenge != IBIT) {
				// yes
				int cmp = this->compare(challenge, sid, pSlots);

				if (cmp < 0) {
					// champion is better
					return IBIT ^ (IBIT - 1); // silently ignore
				} else if (cmp > 0) {
					// heap is better, orphan existing first and replace later with new
					/*
					 * @date 2022-01-08 18:34:14
					 * The caller is responsible for adding new nodes to the group and removing dethroned champions
					 */
				} else if (cmp == 0) {
					assert(0); // should have been detected by `lookupNode()`.
				}
			}
		}

		// create node
		nid = this->newNode(sid, pSlots, power);
		groupNode_t *pNode = this->N + nid;

		pNode->gid = IBIT;

		// add node to index
		this->nodeIndex[nix]        = nid;
		this->nodeIndexVersion[nix] = this->nodeIndexVersionNr;

		/*
		 * @date 2021-12-25 00:57:11
		 * 
		 * Update power levels
		 */
		if (this->withPower) {
			// @formatter:off
			switch(db.signatures[sid].size) {
			case 0: if (layer.minPower[0] < power) layer.minPower[0] = power; // deliberate fall-through
			case 1: if (layer.minPower[1] < power) layer.minPower[1] = power;
			case 2: if (layer.minPower[2] < power) layer.minPower[2] = power;
			case 3: if (layer.minPower[3] < power) layer.minPower[3] = power;
			case 4: if (layer.minPower[4] < power) layer.minPower[4] = power;
			case 5: if (layer.minPower[5] < power) layer.minPower[5] = power;
			case 6: if (layer.minPower[6] < power) layer.minPower[6] = power;
			case 7: if (layer.minPower[7] < power) layer.minPower[7] = power;
				break;
			default:
				assert(0);
			}
			// @formatter:on
		}

		return nid;
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
	 * validateTree() would complain if resolveForward() would loop
	 * 
	 * @date 2022-01-01 00:40:21
	 * 
	 * Merge towards the lowest of lhs/rhs. 
	 * This should contain run-away `resolveForward()`.
	 */
	void mergeGroups(groupLayer_t &layer, uint32_t rhs) {

		this->cntMergeGroup++;

		uint32_t lhs = layer.gid;
		
		assert(this->N[lhs].gid == lhs); // lhs must be latest header
		assert(this->N[rhs].gid == rhs); // rhs must be latest header
		assert(lhs != rhs); // groups must be different

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

				// orphan node
				uint32_t prevId = pNode->prev;
				unlinkNode(iNode);
				iNode = prevId;
				// redirect to entrypoint
				pNode->gid = lhs;
			}
			// let header redirect
			this->N[rhs].gid = lhs;

			// update layer
			layer.gid = gid;
			layer.rebuild();

			return;
		} else if (rhs < this->nstart) {
			// yes
			for (uint32_t iNode = this->N[lhs].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode = this->N + iNode;

				// orphan node
				uint32_t prevId = pNode->prev;
				unlinkNode(iNode);
				iNode = prevId;
				// redirect to entrypoint
				pNode->gid = rhs;
			}
			// let header redirect
			this->N[lhs].gid = rhs;

			// update layer
			layer.gid = gid;
			layer.rebuild();

			return;
		}
		
		/*
		 * Empty/initial/orphaned group?
		 */
		if (this->N[lhs].next == lhs) {
			assert(0);
		} else if (this->N[rhs].next == rhs) {
			assert(0);
		}

		assert(this->N[rhs].next != rhs); // rhs may not be empty (need to test after entrypoint test)

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
					uint32_t prevId = pNode->prev;
					unlinkNode(iNode);
					iNode = prevId;
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
					uint32_t prevId = pNode->prev;
					unlinkNode(iNode);
					iNode = prevId;
				}
			}

			freeVersion(pVersion);
		}

		/*
		 * Simple merge so everything gets onto one list
		 */

		if (lhs == gid) {
			linkNode(this->N[lhs].prev, rhs);
			unlinkNode(rhs);
			// let rhs redirect to gid
			this->N[rhs].gid = gid;
		} else {
			linkNode(this->N[rhs].prev, lhs);
			unlinkNode(lhs);
			// let lhs redirect to gid
			this->N[lhs].gid = gid;
		}
		
		// update layer
		layer.gid = gid;
		layer.rebuild();
	}

	/*
	 * @date 2021-12-22 22:49:13
	 * 
	 * Update group by replacing all outdated nodes, re-create sid lookup index to resolve sid challenges.
	 * 
	 * lhs/rhs is the range where forwarding is critical
	 * Merging will try to reassign the highest to the lowest
	 * If reassigning would create forward references, then recreate the group and return true.
	 * 
	 * pLhs/pRhs are the range limits used by `resolveForward()`.  
	 */
	bool updateGroup(groupLayer_t &layer, uint32_t *pLow, bool allowForward) {

		assert(this->N[layer.gid].gid == layer.gid); // must be latest

		bool hasForward = false; // set to `true` is a node has a forward reference

		/*
		 * Walk through all nodes of group
		 */
		uint32_t nextId;
		for (uint32_t iNode = this->N[layer.gid].next; iNode != this->N[iNode].gid; iNode = nextId) {
			groupNode_t       *pNode         = this->N + iNode;
			const signature_t *pSignature    = db.signatures + pNode->sid;
			unsigned          numPlaceholder = pSignature->numPlaceholder;

			// save next iterator value in case list changes
			nextId = pNode->next;

			/*
			 * Detect if node is outdated
			 */

			uint32_t newSid  = pNode->sid;
			uint32_t newSlots[MAXSLOTS];
			bool     changed = false;

			for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
				uint32_t id = pNode->slots[iSlot];
				if (this->N[id].gid != id) {
					id      = updateToLatest(id);
					changed = true;
				}
				if (id > layer.gid)
					hasForward = true;
				newSlots[iSlot] = id;
			}

			if (hasForward && !allowForward) {
				// silently ignore
				unlinkNode(iNode);
				hasForward= false;
				continue;
			}

			// pad with zeros
			for (unsigned iSlot = numPlaceholder; iSlot < MAXSLOTS; iSlot++)
				newSlots[iSlot] = 0;

			if (changed) {
				// perform sid swap
				applySwapping(newSid, newSlots);

				// perform folding. NOTE: newSid/newSlots might both change
				applyFolding(&newSid, newSlots);

				// orphan outdated node
				unlinkNode(iNode);
			}

			/*
			 * Is original still usable and does it survive a re-challenge (if any)
			 * This is the fast-path.
			 */

			if (!changed) {
				uint32_t challenge = layer.findSid(newSid);

				if (challenge == IBIT) {
					// champion absent, make node champion
					layer.pSidMap[newSid]          = iNode;
					layer.pSidVersion->mem[newSid] = layer.pSidVersion->version;
				} else {
					int cmp = this->compare(challenge, newSid, newSlots);

					if (cmp < 0) {
						// challenge is better, orphan node (orphan might already have been orphaned)
						unlinkNode(iNode);
					} else if (cmp > 0) {
						// newSlots (which is the original unchanged node) is better, `challenge` is incorrect, update it 

						// if the champion to-be-orphaned is next in list, position appropriately
						if (challenge == nextId)
							nextId = this->N[nextId].next;

						unlinkNode(challenge);
						layer.pSidMap[newSid]          = iNode;
						layer.pSidVersion->mem[newSid] = layer.pSidVersion->version;
					} else if (cmp == 0) {
						assert(challenge == iNode);
					}
				}

				// keep node
				continue;
			}

			/*
			 * @date 2022-01-09 15:59:22
			 * A self-collapse is a node collapse
			 */
			{
				// test for iterator/endpoint collapse
				bool hasSelf = false;
				for (unsigned iSlot = 0; iSlot < db.signatures[newSid].numPlaceholder; iSlot++) {
					if (newSlots[iSlot] == layer.gid) {
						hasSelf = true;
						break;
					}
				}
				if (hasSelf) {
					assert(changed);
					continue; // silently ignore
				}
			}

			/*
			 * Try to create node
			 */

			uint32_t newNix = this->lookupNode(newSid, newSlots);
			uint32_t newNid = this->nodeIndex[newNix];

			newNid = addToGroup(layer, newNix, newNid, newSid, newSlots, pNode->power); // TODO: power correction?

			// was there a collapse?
			if (newNid & IBIT) {
				// yes

				// self-collapse?
				if (newNid == (IBIT ^ (IBIT - 1)))
					continue; // yes, silently ignore

				uint32_t endpoint = newNid & ~IBIT;

				// merge and update groups
				mergeGroups(layer, endpoint);

				if (endpoint < this->nstart)
					*pLow = this->nstart;
				else if (endpoint < *pLow)
					*pLow = endpoint;

				// restart
				nextId = this->N[layer.gid].next;
				continue;
			}

			// should be freshly created
			assert (this->N[newNid].gid == IBIT);

			// finalise node
			groupNode_t *pNew = this->N + newNid;
			pNew->oldId = pNode->oldId ? pNode->oldId : iNode; 

			// add node to list
			pNew->gid = layer.gid;
			// add node immediately before the next, so it acts as a replacement and avoids getting double processed
			linkNode(this->N[nextId].prev, newNid);

			/*
			 * @date  2022-01-08 18:39:49
			 * Orphan any prior champion
			 */
			{
				// Orphan any prior champion
				uint32_t challenge = layer.findSid(newSid);
				if (challenge != IBIT)
					unlinkNode(challenge);

				// add to sid lookup index
				layer.pSidMap[newSid]          = iNode;
				layer.pSidVersion->mem[newSid] = layer.pSidVersion->version;
			}

			if (ctx.opt_debug & ctx.DEBUGMASK_GROUPNODE) {
				printf("gid=%u\tnid=%u\told=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] siz=%u pwr=%u\n",
				       layer.gid, newNid,
				       pNew->oldId,
				       newSid, db.signatures[newSid].name,
				       newSlots[0], newSlots[1], newSlots[2], newSlots[3], newSlots[4], newSlots[5], newSlots[6], newSlots[7], newSlots[8],
				       db.signatures[newSid].size, pNew->power);
			}
		}

		// group may not become empty
		assert(this->N[layer.gid].next != layer.gid);

		/*
		 * @date 2022-01-03 00:18:25
		 * 
		 * If group has forward references, recreate group with new id to resolve forward
		 * This can happen when components get created out of order (will be fixed later)
		 * Or groups were merged and the highest id went lower.
		 */

		if (hasForward) {
			/*
			 * create new list header
			 */

			uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zeroed
			assert(selfSlots[MAXSLOTS - 1] == 0);

			uint32_t newGid = this->newNode(db.SID_SELF, selfSlots, /*power*/ 0);
			assert(newGid == this->N[newGid].slots[0]);
			this->N[newGid].gid = newGid;

			this->N[newGid].oldId = this->N[layer.gid].oldId ? this->N[layer.gid].oldId : layer.gid;

			/*
			 * Walk and update the list
			 */

			// relocate group
			linkNode(newGid, layer.gid);
			unlinkNode(layer.gid);
			this->N[layer.gid].gid = newGid; // redirect to new group

			// update gids
			for (uint32_t iNode = this->N[newGid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t *pNode = this->N + iNode;
				pNode->gid = newGid;
			}

			layer.gid = newGid;
		}

		return hasForward;
	}

	/*
	 * @date 2021-11-11 23:19:34
	 * 
	 * Rebuild groups that have nodes that have forward references
	 * 
	 * NOTE: `layer` is only needed for the layer connectivity
	 */
	void resolveForward(groupLayer_t &layer, uint32_t gstart) {

		assert(this->N[layer.gid].gid == layer.gid); // lhs must be latest

		bool     hasForward = false;		// true is one of the group nodes has a forward reference
		uint32_t initialGid = layer.gid;	// initial gid, used to restore layer on exit
		uint32_t iGroup     = gstart;           // group being processed
		uint32_t firstId    = gstart;           // lowest group in sweep
		uint32_t lastId     = this->ncount;	// highest group in sweep

		if (initialGid < this->nstart) {
			// for endpoints, sweep the whole tree
			firstId = iGroup = this->nstart;
		}
		
		/*
		 * Walk through tree and search for outdated groups
		 */

		while (iGroup < lastId) {

			// stop at group headers
			if (this->N[iGroup].gid != iGroup) {
				iGroup++;
				continue;
			}

			assert(this->N[iGroup].next != iGroup); // group may not be empty

			/*
			 * Prepare layer for group and update
			 */
			
			layer.gid = iGroup;
			layer.rebuild();

			uint32_t lowId = iGroup;
			hasForward |= updateGroup(layer, &lowId, /*allowForward=*/true);

			// update lowest
			if (lowId < firstId)
				firstId = lowId;
			
			// if something merged, reposition to the start of the range
			if (lowId != iGroup) {
				// yes
				assert(lowId < iGroup); // group merging always lowers gid
				// jump
				iGroup = lowId;
				continue;
			}
			
			/*
			 * After processing rhs, if there were no forwards, then everything upto ncount is safe
			 */
			if (iGroup == firstId && !hasForward)
				break;
			
			// next node
			iGroup++;
		}

		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);
		
		/*
		 * Restore layer
		 */
		layer.gid = updateToLatest(initialGid);
		layer.rebuild();
	}

	void __attribute__((used)) whoHas(uint32_t id) {
		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			if (this->N[iGroup].gid != iGroup)
				continue; // not start of list

			for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				groupNode_t   *pNode = this->N + iNode;
				for (unsigned iSlot  = 0; iSlot < MAXSLOTS; iSlot++) {
					if (updateToLatest(pNode->slots[iSlot]) == id) {

						printf("gid=%u\tnid=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] siz=%u pwr=%u\n",
						       pNode->gid, iNode,
						       pNode->sid, db.signatures[pNode->sid].name,
						       pNode->slots[0],  pNode->slots[1],  pNode->slots[2],  pNode->slots[3],  pNode->slots[4],  pNode->slots[5],  pNode->slots[6],  pNode->slots[7],  pNode->slots[8],
						       db.signatures[pNode->sid].size, pNode->power);
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
			groupNode_t   *pNode = this->N + iNode;

			printf("gid=%u\tnid=%u\t%u:%s/[",
			       pNode->gid, iNode,
			       pNode->sid, db.signatures[pNode->sid].name);

			char delimiter = 0;

			for (unsigned iSlot = 0; iSlot < db.signatures[pNode->sid].numPlaceholder; iSlot++) {
				uint32_t id = pNode->slots[iSlot];

				if (delimiter)
					putchar(delimiter);
				delimiter = ' ';

				if (id > iGroup)
					printf("<%u>", id);
				else
					printf("%u", id);

				uint32_t latest = updateToLatest(id);

				if (latest != id)
					printf("(%u)", latest);

				if (latest == iGroup) {
					// slots references own group
					printf("<ERROR:gid=self>");
				} else if (latest == 0) {
					// slots references zero
					printf("<ERROR:gid=zero>");
				}
			}

			printf("]\n");

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
						int cmp = this->compare(pSlots[i], this, pSlots[pTransformSwap[i] - 'a']);

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
					pSignature = db.signatures + *pSid;
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

	/*
	 * @date 2021-11-11 16:44:08
	 * 
	 * For debugging
	 */
	void validateTree(unsigned lineNr) {
		versionMemory_t *pVersion   = allocVersion();
		uint32_t        thisVersion = pVersion->nextVersion();
		int             errors      = 0;

		if (lineNr == 0)
			errors++;

		/*
		 * Check sids
		 */
		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			// find group headers
			if (this->N[iGroup].gid == iGroup) {

				thisVersion = pVersion->nextVersion();
				
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode         = this->N + iNode;
					assert(pVersion->mem[pNode->sid] != thisVersion); // test multiply defined
					pVersion->mem[pNode->sid] = thisVersion;
				}
			}
		}

		/*
		 * Check orphans
		 */
		for (uint32_t iNode = this->nstart; iNode < this->ncount; iNode++) {
			const groupNode_t *pNode = this->N + iNode;

			if (pNode->gid == IBIT)
				continue; // under construction

			if (iNode == pNode->gid && pNode->next == iNode)
				assert(0); // empty group

			if (pNode->next != iNode && this->N[pNode->gid].gid != pNode->gid)
				assert(0); // node belongs to group that is NOT latest
		}

		/*
		 * Check references
		 */
		thisVersion = pVersion->nextVersion();

		// mark endpoints as defined
		for (uint32_t iKey = 0; iKey < this->nstart; iKey++) {
			assert(this->N[iKey].gid == iKey);
			assert(this->N[iKey].next == iKey);
			pVersion->mem[iKey] = thisVersion;
		}

		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			// find group headers
			if (this->N[iGroup].gid == iGroup) {

				// is list up-to-date
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;
					unsigned numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					// test correct group (for broken group management)
					if (pNode->gid != iGroup)
						errors++;

					// test double defined (for broken linked lists)
					if (pVersion->mem[iNode] == thisVersion)
						errors++;

					uint32_t newSlots[MAXSLOTS] = {0};

					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t id = updateToLatest(pNode->slots[iSlot]);

						if (id == iGroup || id == 0) {
							// self-reference to group or full-collapse 
							errors++;
						}

						if (pVersion->mem[id] != thisVersion) {
							// reference not defined
							errors++;
						}
					}

					// test slots are unique
					assert(newSlots[1] == 0 || (newSlots[1] != newSlots[0]));
					assert(newSlots[2] == 0 || (newSlots[2] != newSlots[0] && newSlots[2] != newSlots[1]));
					assert(newSlots[3] == 0 || (newSlots[3] != newSlots[0] && newSlots[3] != newSlots[1] && newSlots[3] != newSlots[2]));
					assert(newSlots[4] == 0 || (newSlots[4] != newSlots[0] && newSlots[4] != newSlots[1] && newSlots[4] != newSlots[2] && newSlots[4] != newSlots[3]));
					assert(newSlots[5] == 0 || (newSlots[5] != newSlots[0] && newSlots[5] != newSlots[1] && newSlots[5] != newSlots[2] && newSlots[5] != newSlots[3] && newSlots[5] != newSlots[4]));
					assert(newSlots[6] == 0 || (newSlots[6] != newSlots[0] && newSlots[6] != newSlots[1] && newSlots[6] != newSlots[2] && newSlots[6] != newSlots[3] && newSlots[6] != newSlots[4] && newSlots[6] != newSlots[5]));
					assert(newSlots[7] == 0 || (newSlots[7] != newSlots[0] && newSlots[7] != newSlots[1] && newSlots[7] != newSlots[2] && newSlots[7] != newSlots[3] && newSlots[7] != newSlots[4] && newSlots[7] != newSlots[5] && newSlots[7] != newSlots[6]));
					assert(newSlots[8] == 0 || (newSlots[8] != newSlots[0] && newSlots[8] != newSlots[1] && newSlots[8] != newSlots[2] && newSlots[8] != newSlots[3] && newSlots[8] != newSlots[4] && newSlots[8] != newSlots[5] && newSlots[8] != newSlots[6] && newSlots[8] != newSlots[7]));


					// mark node found
					pVersion->mem[iNode] = thisVersion;
				}

				// test double defined
				if (pVersion->mem[iGroup] == thisVersion)
					errors++;

				// mark header found
				pVersion->mem[iGroup] = thisVersion;
			}
		}

		if (errors == 0) {
			freeVersion(pVersion);
			return;
		}

		printf("INVALIDTREE at line %u\n", lineNr);

		// bump version
		thisVersion = pVersion->nextVersion();

		// mark endpoints as defined
		for (uint32_t iKey = 0; iKey < this->nstart; iKey++)
			pVersion->mem[iKey] = thisVersion;

		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			// find group headers
			if (this->N[iGroup].gid == iGroup) {

				// may not be empty
				if(this->N[iGroup].next == iGroup) {
					printf("<GID %d EMPTY>\n", iGroup);
					continue;
				}

				// is list up-to-date
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;
					unsigned numPlaceholder = db.signatures[pNode->sid].numPlaceholder;

					// test double defined
					if (pVersion->mem[iNode] == thisVersion)
						printf("<DOUBLE nid=%u>", iNode);

					// test correct group
					if (pNode->gid != iGroup)
						printf("<GROUP>");

					printf("%u\t%u(%u)\t%u:%s/[",
					       pNode->gid, iNode, pNode->oldId,
					       pNode->sid, db.signatures[pNode->sid].name);

					char delimiter = 0;

					for (unsigned iSlot = 0; iSlot < numPlaceholder; iSlot++) {
						uint32_t id = pNode->slots[iSlot];

						if (delimiter)
							putchar(delimiter);
						delimiter = ' ';

						printf("%u", id);

						uint32_t latest = updateToLatest(id);

						if (latest != id)
							printf("(%u)", latest);

						if (latest == iGroup) {
							// slots references own group
							printf("<ERROR:gid=self>");
						} else if (latest == 0) {
							// slots references zero
							printf("<ERROR:gid=zero>");
						} else {
							if (id != latest) {
								// reference orphaned
								printf("<OUTDATED>");
							}
							if (pVersion->mem[latest] != thisVersion) {
								// reference not defined
								printf("<MISSING>");
							}
						}
					}

					printf("]\n");
				}

				// test double defined
				if (pVersion->mem[iGroup] == thisVersion)
					printf("<DOUBLE gid=%u>", iGroup);

				// mark header found
				pVersion->mem[iGroup] = thisVersion;
			}
		}

		freeVersion(pVersion);

		assert(0);
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

		uint32_t        nextPlaceholder  = this->kstart;	// next placeholder for `pTransform`
		uint32_t        nextExportNodeId = this->nstart;	// next nodeId for exported name
		uint32_t        *pMap            = allocMap();		// maps internal to exported node id 
		versionMemory_t *pVersion        = allocVersion();	// version data for `pMap`.
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
		if (nid >= this->nstart && this->N[nid].sid == db.SID_SELF) {
			if (1) {
				// favour first node in list (most likely `1n9`), mostly shorter names, but deeper recursion
				nid = this->N[nid].next;
			} else {
				// favour last  node in list,(most likely `4n9`), may create longer names but less deeper recursion
				nid = this->N[nid].prev;
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
		while(gid != this->N[gid].gid)
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
					printf("#gid=%u\tnid=%u\t%u:%s/[%u %u %u %u %u %u %u %u %u] pwr=%u\n",
					       pNode->gid, iNode,
					       pNode->sid, db.signatures[pNode->sid].name,
					       pNode->slots[0], pNode->slots[1], pNode->slots[2], pNode->slots[3], pNode->slots[4], pNode->slots[5], pNode->slots[6], pNode->slots[7], pNode->slots[8],
					       pNode->power);
				}
			}

			// remember
			pStack[numStack++] = nid;
			pMap[nextNode++] = nid;

			if ((unsigned) numStack > maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (numStack != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[numStack - 1];

		freeMap(pStack);
		freeMap(pMap);
		if (transformList)
			freeMap(transformList);

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
	void saveFile(const char *fileName, bool showProgress = true) {
		assert(numRoots > 0);

		/*
		 * File header
		 */

		groupTreeHeader_t header;
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

			// output entrypoints and nodes
			for (uint32_t iNode = 0; iNode < ncount; iNode++) {
				const groupNode_t *pNode = this->N + iNode;

				size_t len = sizeof(*pNode);
				fwrite(pNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(pNode->sid));
				for (unsigned i = 1; i < MAXSLOTS; i++)
					__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(pNode->slots[i]));

			}
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
					
					for (unsigned iSlot=0; iSlot<numPlaceholder; iSlot++) {
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
				wrtNode.gid   = pMap[iGroup];
				wrtNode.prev  = pMap[pNode->prev];
				wrtNode.next  = pMap[pNode->next];
				wrtNode.sid   = pNode->sid;
				wrtNode.power = pNode->power;

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

					wrtNode.gid  = pMap[iGroup];
					wrtNode.prev = pMap[pNode->prev];
					wrtNode.next = pMap[pNode->next];
					wrtNode.sid  = pNode->sid;
					wrtNode.sid  = pNode->power;

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

		header.magic       = GROUPTREE_MAGIC;
		header.magic_flags = flags;
		header.sidCRC      = db.fileHeader.magic_sidCRC;
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
