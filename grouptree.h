#ifndef _GROUPTREE_H
#define _GROUPTREE_H

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
	 * Index hash, for fast lookups like when deleting.
	 * NOTE: `SID_SELF`, the list headers, are never indexed. 
	 */
	uint32_t hashIX;

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

struct groupTree_t {

	/*
	 * Constants for 
	 */
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
	#define GROUPTREE_DEFAULT_MAXNODE 100000000
	#endif

	/**
	 * Renumbering/remapping node id's is a major operation within `baseTree_t`.
	 * Instead of allocating new maps with chance of memory fragmentation, reuse previously released maps.
	 * The are two pools of maps, one specifically for containing node id's, the other for containing versioned memory id's.
	 * Maps are `--maxnode=` entries large.
	 *
	 * @constant {number} MAXPOOLARRAY
	 */
	#if !defined(GROUPTREE_MAXPOOLARRAY)
	#define GROUPTREE_MAXPOOLARRAY 128
	#endif

	enum {
		DEFAULT_MAXNODE = GROUPTREE_DEFAULT_MAXNODE,
		MAXPOOLARRAY = GROUPTREE_MAXPOOLARRAY
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
	// primary fields
	uint32_t                 kstart;                // first input key id.
	uint32_t                 ostart;                // first output key id.
	uint32_t                 estart;                // first external/extended key id. Roots from previous tree in chain.
	uint32_t                 nstart;                // id of first node
	uint32_t                 ncount;                // number of nodes in use
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
	unsigned                 numPoolMap;            // Number of node-id pools in use
	uint32_t                 **pPoolMap;            // Pool of available node-id maps
	unsigned                 numPoolVersion;        // Number of version-id pools in use
	uint32_t                 **pPoolVersion;        // Pool of available version-id maps
	uint32_t                 mapVersionNr;          // Version number
	// slots, for `addNormaliseNode()` because of too many exit points
	uint32_t                 *slotMap;              // slot position of endpoint 
	uint32_t                 *slotVersion;          // versioned memory for addNormaliseNode - content version
	uint32_t                 slotVersionNr;         // active version number

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
		// slots
		slotMap(NULL),
		slotVersion(NULL),
		slotVersionNr(1)
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
		pPoolVersion((uint32_t **) ctx.myAlloc("groupTree_t::pPoolVersion", MAXPOOLARRAY, sizeof(*pPoolVersion))),
		mapVersionNr(0),
		// slots
		slotMap(allocMap()),
		slotVersion(allocMap()),  // allocate as node-id map because of local version numbering
		slotVersionNr(1)
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
			pNode->hashIX = 0xffffffff;
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
	uint32_t *allocVersion(void) {
		uint32_t *pVersion;

		if (numPoolVersion > 0) {
			// get first free version map
			pVersion = pPoolVersion[--numPoolVersion];
		} else {
			// allocate new map
			pVersion = (uint32_t *) ctx.myAlloc("groupTree_t::versionMap", maxNodes, sizeof *pVersion);
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
	int compare(uint32_t lhs, uint32_t sidRhs, const uint32_t *pSlotsRhs) {

		if (lhs < this->nstart) {
			// endpoint is always lover
			return -1;
		}

		if (this->N[lhs].sid < sidRhs) {
			return -1;
		} else if (this->N[lhs].sid > sidRhs) {
			return +1;
		}

		/*
		 * SID_SELF needs special handling or it will recurse on itself 
		 */
		if (this->N[lhs].sid == db.SID_SELF) {
			if (this->N[lhs].slots[0] < pSlotsRhs[0]) {
				return -1;
			} else if (this->N[lhs].slots[0] > pSlotsRhs[0]) {
				return +1;
			} else {
				return 0;
			}
		}

		/*
		 * simple compare
		 * todo: cache results
		 */
		const signature_t *pSignature = db.signatures + this->N[lhs].sid;

		for (unsigned iSlot=0; iSlot<pSignature->numPlaceholder; iSlot++) {
			int ret = this->compare(this->N[lhs].slots[iSlot], this, pSlotsRhs[iSlot]);
			if (ret != 0)
				return ret;
		}

		return 0;
	}

	/*
	 * @date  2021-11-10 00:05:51
	 * 
	 * Add node to list
	 */
	inline void linkNode(uint32_t headId, uint32_t nodeId) {

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
	inline void unlinkNode(uint32_t nodeId) {
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
	inline uint32_t lookupNode(uint32_t sid, const uint32_t slots[]) {
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
	inline uint32_t newNode(uint32_t sid, const uint32_t slots[]) {
		uint32_t nid = this->ncount++;

		assert(nid < maxNodes);
		assert(MAXSLOTS == 9);

		if (sid != db.SID_SELF) {
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
		}

		if (nid > maxNodes - 10) { // 10 is arbitrary
			fprintf(stderr, "[OVERFLOW]\n");
			printf("{\"error\":\"overflow\",\"maxnode\":%d}\n", maxNodes);
			exit(1);
		}

		groupNode_t *pNode = this->N + nid;

		pNode->gid  = 0;
		pNode->next = nid;
		pNode->prev = nid;
		pNode->hashIX = 0xffffffff;
		pNode->sid    = sid;

		pNode->slots[0] = slots[0];
		pNode->slots[1] = slots[1];
		pNode->slots[2] = slots[2];
		pNode->slots[3] = slots[3];
		pNode->slots[4] = slots[4];
		pNode->slots[5] = slots[5];
		pNode->slots[6] = slots[6];
		pNode->slots[7] = slots[7];
		pNode->slots[8] = slots[8];

		return nid;
	}

	/*
	 * @date 2021-11-10 01:26:25
	 */
	inline void deleteNode(uint32_t nodeId) {
		groupNode_t *pNode = this->N + nodeId;

		// remove from index
		if (pNode->hashIX != 0xffffffff)
			this->nodeIndex[pNode->hashIX] = db.IDDELETED;

		// unlink
		unlinkNode(nodeId);

		// zero data
		memset(pNode, 0, sizeof(*pNode));
	}

	/*
	 * @date 2021-05-13 00:30:42
	 *
	 * lookup/create a restriction-free, unmodified node
	 */
	inline uint32_t addNode(uint32_t Q, uint32_t T, uint32_t F) {
		assert(!"placeholder");
		return 0;
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
	 * @param {groupTree_t*} pTree - Tree containing nodes
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t addBasicNode(uint32_t Q, uint32_t T, uint32_t F, unsigned *pFailCount, unsigned depth) {
		assert(!"placeholder");
		return 0;
	}

	/*
	 * @date 2021-11-04 00:44:47
	 *
	 * lookup/create and normalise any combination of Q, T and F, inverted or not.
	 * 
	 * Returns main node id, which might be outdated as effect of internal rewriting.
	 * 
	 * NOTE: this function is called recursively: `addNormaliseNode()`/`expandSignature()`
	 * NOTE: the return value may be inverted
	 * NOTE: do NOT forget to update gid after calling this function
	 * 
	 * 	uint32_t nid = addNormaliseNode(q,t,f,gid);
	 * 	gid = nid;
	 * 	while (gid != this->N[gid].gid)
	 *		gid = this->N[gid].gid;
	 */
	uint32_t addNormaliseNode(uint32_t Q, uint32_t T, uint32_t F, uint32_t gid = 0, unsigned depth = 0) {
		depth++;
		assert(depth < 30);

		printf("%.*sQ=%u%s T=%u%s F=%u%s\n",
		       depth - 1, "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t",
		       Q & ~IBIT, (Q & IBIT) ? "~" : "",
		       T & ~IBIT, (T & IBIT) ? "~" : "",
		       F & ~IBIT, (F & IBIT) ? "~" : "");

		/*
	  	 * @date 2021-11-04 01:58:34
		 * 
		 * First step: Apply same normalisation as the database generators  
		 */

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
			return addNormaliseNode(Q, T, F, gid, depth) ^ IBIT;
		}

		// split `T` into unsigned and invert-bit
		uint32_t Tu = T & ~IBIT;
		uint32_t Ti = T & IBIT;

		/*
		 * use the latest lists
		 */

		while (this->N[Q].gid != Q)
			Q = this->N[Q].gid;
		while (this->N[Tu].gid != Tu)
			Tu = this->N[Tu].gid;
		while (this->N[F].gid != F)
			F = this->N[F].gid;

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

		if (Ti) {
			// as you might notice, once `Ti` is set, it stays set

			if (Tu == 0) {
				if (Q == F) {
					// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
					return Q;
				} else if (F == 0) {
					// [ 0] a ? !0 : 0  ->  a
					return Q;
				} else {
					// [ 2] a ? !0 : b  -> "+" OR
					tlSid = db.SID_OR;
				}
			} else if (Q == Tu) {
				if (Q == F) {
					// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
					return 0;
				} else if (F == 0) {
					// [ 3] a ? !a : 0  ->  0
					return 0;
				} else {
					// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
					Q = F;
					F = 0;
					tlSid = db.SID_GT;
				}
			} else {
				if (Q == F) {
					// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
					F = 0;
					tlSid = db.SID_GT;
				} else {
					if (F == 0) {
						// [ 6] a ? !b : 0  -> ">" greater-than
						tlSid = db.SID_GT;
					} else if (Tu == F) {
						// [ 8] a ? !b : b  -> "^" not-equal/xor
						tlSid = db.SID_NE;
					} else {
						// [ 9] a ? !b : c  -> "!" QnTF
						tlSid = db.SID_QNTF;
					}
				}
			}

		} else {

			if (Tu == 0) {
				if (Q == F) {
					// [11] a ?  0 : a -> 0
					return 0;
				} else if (F == 0) {
					// [10] a ?  0 : 0 -> 0
					assert(0); // already tested
					return 0;
				} else {
					// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
					Tu = Q;
					Ti = IBIT;
					Q = F;
					F = 0;
					tlSid = db.SID_GT;
				}
			} else if (Q == Tu) {
				if (Q == F) {
					// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
					assert(0); // already tested
					return Q;
				} else if (F == 0) {
					// [13] a ?  a : 0 -> a
					return Q;
				} else {
					// [15] a ?  a : b -> a ? !0 : b -> "+" OR
					Tu = 0;
					Ti = IBIT;
					tlSid = db.SID_OR;
				}
			} else {
				if (Q == F) {
					// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
					F = 0;
					tlSid = db.SID_AND;
				} else {
					if (F == 0) {
						// [16] a ?  b : 0             "&" and
						tlSid = db.SID_AND;
					} else {
						// [18] a ?  b : b -> b        ALREADY TESTED		
						// [19] a ?  b : c             "?" QTF
						tlSid = db.SID_QTF;
					}
				}
			}
		}

		// Q/T/F/gid must be within range
		assert(gid == 0 || (Q < gid && Tu < gid && F < gid));
		assert(gid == this->N[gid].gid);

		/*
		 * Lookup if 1n9 already exists.
		 * This is a fast test to find duplicates
		 */

		uint32_t tlSlots[MAXSLOTS] = {0}; // zero contents
		assert(tlSlots[MAXSLOTS - 1] == 0);

		// set slots
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
		uint32_t ix = this->lookupNode(tlSid, tlSlots);
		if (this->nodeIndex[ix] != 0) {
			uint32_t nid = this->nodeIndex[ix];

			if (gid == 0 || isSameGroup(gid, nid))
				return nid; // node already exists

			// merge groups
			if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);
			mergeGroups(gid, this->N[nid].gid, depth);
			if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

			// todo: need to rebuild, but delay that till top-level is finished
			
			return nid;
		}

		/* 
		 * When adding a new node, check if it would be accepted by `addToCollection()`.
		 */
		if (gid != 0) {
			uint32_t id = orphanLesser(gid, tlSid, tlSlots);
			if (id != 0) {
				// yes, `id`=appreciated node
				assert(gid == 0 || isSameGroup(gid, id));

				// node has better replacement
				return id;
			}
		}

		/*
		 * Fallback code and validation. 
		 * Add QTF as single `1n9` node.
		 */
		if (0) {
			/*
			 * Emit list header
			 */
			uint32_t    gid    = this->ncount++;
			groupNode_t *pNode = this->N + gid;

			if (gid > maxNodes - 10) {
				fprintf(stderr, "[OVERFLOW]\n");
				printf("{\"error\":\"overflow\",\"maxnode\":%d}\n", maxNodes);
				exit(1);
			}

			memset(pNode, 0, sizeof(*pNode));
			pNode->gid  = gid;
			pNode->next = pNode->prev = gid + 1; // link to next node
			pNode->sid  = db.SID_SELF;
			pNode->slots[0] = gid;

			/*
			 * Followed by top-level sid
			 */

			pNode = this->N + this->ncount++;

			if (gid > maxNodes - 10) {
				fprintf(stderr, "[OVERFLOW]\n");
				printf("{\"error\":\"overflow\",\"maxnode\":%d}\n", maxNodes);
				exit(1);
			}

			memset(pNode, 0, sizeof(*pNode));
			pNode->gid  = gid;
			pNode->next = pNode->prev = gid; // link to head (previous node)

			// set sid/slots
			if (tlSid == db.SID_OR || tlSid == db.SID_NE) {
				pNode->slots[0] = Q;
				pNode->slots[1] = F;
			} else if (tlSid == db.SID_GT || tlSid == db.SID_AND) {
				pNode->slots[0] = Q;
				pNode->slots[1] = Tu;
			} else {
				pNode->slots[0] = Q;
				pNode->slots[1] = Tu;
				pNode->slots[2] = F;
			}

			return gid;
		}

		/*
		 * @date 2021-11-04 02:08:51
		 * 
		 * Second step: create cross-products of Q/T/F group lists
		 */

		uint32_t first1n9 = 0;

		// @formatter:off
		unsigned iQ  = Q;  do {
		unsigned iTu = Tu; do {
		unsigned iF  = F;  do {
		// @formatter:on

			/*
			 * Build slots and lookup signature
			 */
			uint32_t finalSlots[MAXSLOTS];
			uint32_t sid = constructSlots(this->N + iQ, Ti, this->N + iTu, this->N + iF, finalSlots);

			if (sid == 0)
				continue; // combo not found

			/*
			 * @date 2021-11-16 16:22:03
			 * To prevent a recursive loop because this candidate is a lesser alternative, test that first
			 * Example: `abcde^^!/[b acd^^ a c d]` which will fold to `ab^/[b acd^^]` which is lesser than `ab^/[a bcd^^]`
			 */
			if (gid != 0 && orphanLesser(gid, sid, finalSlots) != 0)
				continue; // a better alternative was found (can safely continue as no changes have been made)

			/*
			 * @date 2021-11-08 00:00:19
			 * 
			 * `ab^c^`: is stored as `abc^^/[a/[c] ab^/[a b]]` which is badly ordered.
			 * Proper is: `abc^^/[a/[a] ab^/[b c]]`, but requires creation of `ab^[b c]`.
			 * 
			 * A suggested method to properly sort is to take the sid/slot combo and re-create it using the signature, 
			 * implicitly creating better ordered components.
			 * 
			 * This might create many duplicates.
			 */

			if (db.signatures[sid].size > 1) {
				gid = expandSignature(sid, finalSlots, gid, depth);
				while (gid != this->N[gid].gid)
					gid = this->N[gid].gid;
			}

			/*
			 * Add final sid/slot to collection
			 */

			uint32_t oldCount = this->ncount;

			if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);
			uint32_t nid = addToCollection(sid, finalSlots, gid, depth);
			if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

			// update current group id to that of head of list
			gid = nid;
			while (gid != this->N[gid].gid)
				gid = this->N[gid].gid;

			if (this->ncount != oldCount) {
				// if (ctx.opt_debug & ctx.DEBUG_ROW)
				printf("%.*s%u\t%u\t%u\t%u\t%u\t%u:%s/[%u %u %u %u %u %u %u %u %u]\n",
				       depth - 1, "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t",
				       gid, nid,
				       iQ, iTu, iF,
				       sid, db.signatures[sid].name,
				       finalSlots[0], finalSlots[1], finalSlots[2], finalSlots[3], finalSlots[4], finalSlots[5], finalSlots[6], finalSlots[7], finalSlots[8]);
			}

			// remember first `1n9` (which should always be the first combo created)
			if (first1n9 == 0 && iQ == Q && iTu == Tu && iF == F) {
				first1n9 = nid;
				assert(
					sid == db.SID_ZERO ||
					sid == db.SID_SELF ||
					sid == db.SID_OR ||
					sid == db.SID_GT ||
					sid == db.SID_NE ||
					sid == db.SID_AND ||
					sid == db.SID_QNTF ||
					sid == db.SID_QTF
				);
			}

			/*
			 * Merging groups change Q/T/F headers, possibly invalidating loop end conditions.
			 * It could be that `addToCollection()` orphaned iQ/iTu/iF
			 */
			if (Q != this->N[iQ].gid) {
				while (iQ != this->N[iQ].gid)
					Q = iQ = this->N[iQ].gid; // restart with new list
			}
			if (Tu != this->N[iTu].gid) {
				while (iTu != this->N[iTu].gid)
					Tu = iTu = this->N[iTu].gid; // restart with new list
			}
			if (F != this->N[iF].gid) {
				while (iF != this->N[iF].gid)
					F = iF = this->N[iF].gid; // restart with new list
			}

		// @formatter:off
		// iQ/iT/iF are allowed to start with 0, when that happens, don't loop forever.
		} while (iF = this->N[iF].next, this->N[iF].gid != iF);
		} while (iTu = this->N[iTu].next, this->N[iTu].gid != iTu);
		} while (iQ = this->N[iQ].next, this->N[iQ].gid != iQ);
		// @formatter:on

		// The detector must detect at least one pattern
		assert(first1n9 && this->N[first1n9].gid == gid);
		return first1n9;
	}

	/*
	 * @date 2021-11-16 20:20:38
	 * 
	 * Construct slots based on Q/T/F and find matching signature.
	 * Ti must be 0/IBIT and may flip within this function
	 * Return sid+pFinal or 0 if no match found
	 */
	uint32_t constructSlots(const groupNode_t *pNodeQ, uint32_t Ti, const groupNode_t *pNodeT, const groupNode_t *pNodeF, uint32_t *pFinal) {

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

		const groupNode_t *pZero   = this->N + db.SID_ZERO;

		if (Ti) {

			if (pNodeT == pZero) {
				if (pNodeQ == pNodeF) {
					// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
					return pNodeQ->gid;
				} else if (pNodeF == pZero) {
					// [ 0] a ? !0 : 0  ->  a
					return pNodeQ->gid;
				} else {
					// [ 2] a ? !0 : b  -> "+" OR
				}
			} else if (pNodeT == pNodeQ) {
				if (pNodeQ == pNodeF) {
					// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
					return pZero->gid;
				} else if (pNodeF == pZero) {
					// [ 3] a ? !a : 0  ->  0
					return pZero->gid;
				} else {
					// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
					pNodeQ = pNodeF;
					pNodeF = pZero;
				}
			} else {
				if (pNodeQ == pNodeF) {
					// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
					pNodeF = pZero;
				} else {
					// [ 6] a ? !b : 0  -> ">" greater-than
					// [ 8] a ? !b : b  -> "^" not-equal
					// [ 9] a ? !b : c  -> "!" QnTF
				}
			}

		} else {

			if (pNodeT == pZero) {
				if (pNodeQ == pNodeF) {
					// [11] a ?  0 : a -> 0
					return pZero->gid;
				} else if (pNodeF == pZero) {
					// [10] a ?  0 : 0 -> 0
					assert(0); // already tested
					return pZero->gid;
				} else {
					// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
					pNodeT = pNodeQ;
					Ti     = IBIT;
					pNodeQ = pNodeF;
					pNodeF = pZero;
				}
			} else if (pNodeQ == pNodeT) {
				if (pNodeQ == pNodeF) {
					// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
					assert(0); // already tested
					return pNodeQ->gid;
				} else if (pNodeF == pZero) {
					// [13] a ?  a : 0 -> a
					return pNodeQ->gid;
				} else {
					// [15] a ?  a : b -> a ? !0 : b -> "+" OR
					pNodeT = pZero;
					Ti     = IBIT;
				}
			} else {
				if (pNodeQ == pNodeF) {
					// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
					pNodeF = pZero;
				} else {
					// [16] a ?  b : 0             "&" and
					// [18] a ?  b : b -> b        ALREADY TESTED		
					// [19] a ?  b : c             "?" QTF
				}
			}
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
		// resulting slots containing gid's
		uint32_t    slotsR[MAXSLOTS];
		// nodes already processed
		unsigned    nextSlot = 0;
		const signature_t *pSignature;

		/*
		 * Slot population as `fragmentTree_t` would do
		 */

		bool overflow = false;

		// NOTE: `slotQ` is always `tid=0`, so `slotsQ[]` is not needed
		pSignature = db.signatures + pNodeQ->sid;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			uint32_t endpoint = pNodeQ->slots[iSlot];
			assert(endpoint != 0);

			// was it seen before
			if (slotVersion[endpoint] != thisVersion) {
				slotVersion[endpoint] = thisVersion;
				slotMap[endpoint]     = 'a' + nextSlot; // assign new placeholder
				slotsR[nextSlot]      = endpoint; // put endpoint in result
				nextSlot++;
			}
		}

		pSignature = db.signatures + pNodeT->sid;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			uint32_t endpoint = pNodeT->slots[iSlot];
			assert(endpoint != 0);

			// was it seen before
			if (slotVersion[endpoint] != thisVersion) {
				overflow = (nextSlot >= MAXSLOTS);
				if (overflow)
					break;
				slotVersion[endpoint] = thisVersion;
				slotMap[endpoint]     = 'a' + nextSlot;
				slotsR[nextSlot]      = endpoint;
				nextSlot++;
			}
			slotsT[iSlot] = (char) slotMap[endpoint];
		}
		slotsT[pSignature->numPlaceholder] = 0;

		// test for slot overflow
		if (overflow)
			return 0;

		/*
		 * Lookup `patternFirst`
		 */

		uint32_t tidSlotT = db.lookupFwdTransform(slotsT);
		assert(tidSlotT != IBIT);

		uint32_t ixFirst = db.lookupPatternFirst(pNodeQ->sid, pNodeT->sid ^ Ti, tidSlotT);
		if (db.patternFirstIndex[ixFirst] == 0)
			return 0; // not found

		uint32_t idFirst = db.patternFirstIndex[ixFirst];

		/*
		 * Add `F` to slots
		 */

		pSignature = db.signatures + pNodeF->sid;
		for (uint32_t iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++) {
			// get slot value
			uint32_t endpoint = pNodeF->slots[iSlot];
			assert(endpoint != 0);

			// was it seen before
			if (slotVersion[endpoint] != thisVersion) {
				overflow = (nextSlot >= MAXSLOTS);
				if (overflow)
					break;
				slotVersion[endpoint] = thisVersion;
				slotMap[endpoint]     = 'a' + nextSlot;
				slotsR[nextSlot]      = endpoint;
				nextSlot++;
			}
			slotsF[iSlot] = (char) slotMap[endpoint];
		}
		slotsF[pSignature->numPlaceholder] = 0;

		// test for slot overflow
		if (overflow)
			return 0;

		/*
		 * Lookup `patternSecond`
		 */

		uint32_t tidSlotF = db.lookupFwdTransform(slotsF);
		assert(tidSlotF != IBIT);

		uint32_t ixSecond = db.lookupPatternSecond(idFirst, pNodeF->sid, tidSlotF);
		if (db.patternSecondIndex[ixSecond] == 0)
			return 0; // not found
		uint32_t        idSecond = db.patternSecondIndex[ixSecond];
		patternSecond_t *pSecond = db.patternsSecond + idSecond;

		/*
		 * test for collapse, (result is `0n9`)
		 * NOTE: it seems that `expandSignature()` mitigates this scenario 
		 */
		if (pSecond->sidR == db.SID_ZERO) {
			assert(!"SID_ZERO");
		} else if (pSecond->sidR == db.SID_SELF) {
			assert(!"SID_SELF");
		}

		/*
		 * @date 2021-11-05 02:42:24
		 * 
		 * Fifth step: Extract result out of `slotsR[]` and apply signature based endpoint swapping
		 */

		pSignature = db.signatures + pSecond->sidR;
		const char *pTransformExtract = db.fwdTransformNames[pSecond->tidSlotR];

		assert(nextSlot >= pSignature->numPlaceholder);

		// zero unused entries
		while (nextSlot < MAXSLOTS)
			slotsR[nextSlot++] = 0;

		// extract

		for (unsigned iSlot = 0; iSlot < pSignature->numPlaceholder; iSlot++)
			pFinal[iSlot] = slotsR[pTransformExtract[iSlot] - 'a'];
		for (unsigned iSlot = pSignature->numPlaceholder; iSlot < MAXSLOTS; iSlot++)
			pFinal[iSlot] = 0;

		/*
		 * Apply endpoint swapping
		 */
		if (pSignature->swapId)
			applySwapping(pSignature, pFinal);

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
	 *   the cross-product loop calls this function (low gid), which below adds intermediates (high gid)
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
	 */
	uint32_t expandSignature(uint32_t sid, const uint32_t *pSlots, uint32_t gid, unsigned depth) {

		signature_t *pSignature = db.signatures + sid;

		/*
		 * init
		 */

		uint32_t numStack = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap();
		uint32_t *pMap    = allocMap();
		uint32_t buildGid = 0;

		/*
		 * Load string
		 */
		for (const char *pattern = pSignature->name; *pattern; pattern++) {

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
				uint32_t v = (*pattern - 'a');

				if (v >= pSignature->numPlaceholder)
					ctx.fatal("[endpoint out of range: %d]\n", v);
				if (numStack >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[numStack++] = pSlots[v];
				break;

			}

			case '+': {
				// OR (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				if (pattern[1]) {
					uint32_t id = addNormaliseNode(L, IBIT, R, 0, depth + 1);
					// get latest group
					while (id != this->N[id].gid)
						id = this->N[id].gid;
					pStack[numStack++] = pMap[nextNode++] = id;
				} else {
					assert(numStack == 0);
					// get latest group
					while (gid != this->N[gid].gid)
						gid = this->N[gid].gid;
					buildGid = addNormaliseNode(L, IBIT, R, gid, depth + 1);
				}

				break;
			}
			case '>': {
				// GT (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				if (pattern[1]) {
					uint32_t id = addNormaliseNode(L, R ^ IBIT, 0, 0, depth + 1);
					// get latest group
					while (id != this->N[id].gid)
						id = this->N[id].gid;
					pStack[numStack++] = pMap[nextNode++] = id;
				} else {
					assert(numStack == 0);
					// get latest group
					while (gid != this->N[gid].gid)
						gid = this->N[gid].gid;
					buildGid = addNormaliseNode(L, R ^ IBIT, 0, gid, depth + 1);
				}

				break;
			}
			case '^': {
				// XOR/NE (appreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				if (pattern[1]) {
					uint32_t id = addNormaliseNode(L, R ^ IBIT, R, 0, depth + 1);
					// get latest group
					while (id != this->N[id].gid)
						id = this->N[id].gid;
					pStack[numStack++] = pMap[nextNode++] = id;
				} else {
					assert(numStack == 0);
					// get latest group
					while (gid != this->N[gid].gid)
						gid = this->N[gid].gid;
					buildGid = addNormaliseNode(L, R ^ IBIT, R, gid, depth + 1);
				}

				break;
			}
			case '!': {
				// QnTF (appreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--numStack];
				uint32_t T = pStack[--numStack];
				uint32_t Q = pStack[--numStack];

				if (pattern[1]) {
					uint32_t id = addNormaliseNode(Q, T ^ IBIT, F, 0, depth + 1);
					// get latest group
					while (id != this->N[id].gid)
						id = this->N[id].gid;
					pStack[numStack++] = pMap[nextNode++] = id;
				} else {
					assert(numStack == 0);
					// get latest group
					while (gid != this->N[gid].gid)
						gid = this->N[gid].gid;
					buildGid = addNormaliseNode(Q, T ^ IBIT, F, gid, depth + 1);
				}

				break;
			}
			case '&': {
				// AND (depreciated)
				if (numStack < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--numStack];
				uint32_t L = pStack[--numStack];

				if (pattern[1]) {
					uint32_t id = addNormaliseNode(L, R, 0, 0, depth + 1);
					// get latest group
					while (id != this->N[id].gid)
						id = this->N[id].gid;
					pStack[numStack++] = pMap[nextNode++] = id;
				} else {
					assert(numStack == 0);
					// get latest group
					while (gid != this->N[gid].gid)
						gid = this->N[gid].gid;
					buildGid = addNormaliseNode(L, R, 0, gid, depth + 1);
				}

				break;
			}
			case '?': {
				// QTF (depreciated)
				if (numStack < 3)
					ctx.fatal("[stack underflow]\n");

				uint32_t F = pStack[--numStack];
				uint32_t T = pStack[--numStack];
				uint32_t Q = pStack[--numStack];

				if (pattern[1]) {
					uint32_t id = addNormaliseNode(Q, T, F, 0, depth + 1);
					// get latest group
					while (id != this->N[id].gid)
						id = this->N[id].gid;
					pStack[numStack++] = pMap[nextNode++] = id;
				} else {
					assert(numStack == 0);
					// get latest group
					while (gid != this->N[gid].gid)
						gid = this->N[gid].gid;
					buildGid = addNormaliseNode(Q, T, F, gid, depth + 1);
				}

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
		if (numStack != 0)
			ctx.fatal("[stack not empty]\n");

		freeMap(pStack);
		freeMap(pMap);

		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

		assert(buildGid != 0);
		return buildGid;
	}

	/*
	 * @date 2021-11-19 19:21:13
	 * 
	 * Verify sid/slots to determine if they have collapsed, assuming `expandSignature()` created them.
	 * return IBIT if signature folded
	 */
	uint32_t verifySignature(uint32_t sid, const uint32_t *pSlots) {

		signature_t *pSignature = db.signatures + sid;

		/*
		 * init
		 */

		uint32_t numStack = 0;
		uint32_t nextNode = this->nstart;
		uint32_t *pStack  = allocMap(); // evaluation stack
		uint32_t *pMap    = allocMap(); // id of intermediates
		uint32_t *pActive = allocVersion(); // collection of used id's

		// bump versioned memory
		uint32_t thisVersion = ++slotVersionNr;
		if (thisVersion == 0) {
			// version overflow, clear
			memset(pActive, 0, this->maxNodes * sizeof(*pActive));

			thisVersion = ++slotVersionNr;
		}

		// add slot entries
		for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
			uint32_t id = pSlots[iSlot];
			if (id == 0)
				break;

			pActive[id] = thisVersion;
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
				uint32_t v = (*pattern - 'a');

				if (v >= pSignature->numPlaceholder)
					ctx.fatal("[endpoint out of range: %d]\n", v);
				if (numStack >= this->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[numStack++] = pSlots[v];
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
			default:
				ctx.fatal("[bad token '%c']\n", *pattern);
			} // end-switch


			/*
			 * Only arrive here when Q/T/F have been set 
			 */

			/*
			 * use the latest lists
			 */

			while (this->N[Q].gid != Q)
				Q = this->N[Q].gid;
			while (this->N[Tu].gid != Tu)
				Tu = this->N[Tu].gid;
			while (this->N[F].gid != F)
				F = this->N[F].gid;

			/*
			 * Perform normalisation
			 */
			uint32_t cSid		  = 0; // 0=error/folded
			uint32_t cSlots[MAXSLOTS] = {0}; // zero contents
			assert(cSlots[MAXSLOTS - 1] == 0);

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

			if (Q == 0) {
				// level-1 collapse
				cSid = 0;
			} else if (Ti) {
				if (Tu == 0) {
					if (Q == F) {
						// [ 1] a ? !0 : a  ->  a ? !0 : 0 -> a
						cSid = 0;
					} else if (F == 0) {
						// [ 0] a ? !0 : 0  ->  a
						cSid = 0;
					} else {
						// [ 2] a ? !0 : b  -> "+" OR
						cSid = db.SID_OR;
					}
				} else if (Q == Tu) {
					if (Q == F) {
						// [ 4] a ? !a : a  ->  a ? !a : 0 -> 0
						cSid = 0;
					} else if (F == 0) {
						// [ 3] a ? !a : 0  ->  0
						cSid = 0;
					} else {
						// [ 5] a ? !a : b  ->  b ? !a : b -> b ? !a : 0  ->  ">" GREATER-THAN
						Q = F;
						F = 0;
						cSid = db.SID_GT;
					}
				} else {
					if (Q == F) {
						// [ 7] a ? !b : a  ->  a ? !b : 0  ->  ">" GREATER-THAN
						F = 0;
						cSid = db.SID_GT;
					} else {
						if (F == 0) {
							// [ 6] a ? !b : 0  -> ">" greater-than
							cSid = db.SID_GT;
						} else if (Tu == F) {
							// [ 8] a ? !b : b  -> "^" not-equal/xor
							cSid = db.SID_NE;
						} else {
							// [ 9] a ? !b : c  -> "!" QnTF
							cSid = db.SID_QNTF;
						}
					}
				}

			} else {

				if (Tu == 0) {
					if (Q == F) {
						// [11] a ?  0 : a -> 0
						cSid = 0;
					} else if (F == 0) {
						// [10] a ?  0 : 0 -> 0
						cSid = 0;
					} else {
						// [12] a ?  0 : b -> b ? !a : 0  ->  ">" GREATER-THAN
						Tu = Q;
						Ti = IBIT;
						Q = F;
						F = 0;
						cSid = db.SID_GT;
					}
				} else if (Q == Tu) {
					if (Q == F) {
						// [14] a ?  a : a -> a ?  a : 0 -> a ? !0 : 0 -> a
						cSid = 0;
					} else if (F == 0) {
						// [13] a ?  a : 0 -> a
						cSid = 0;
					} else {
						// [15] a ?  a : b -> a ? !0 : b -> "+" OR
						Tu = 0;
						Ti = IBIT;
						cSid = db.SID_OR;
					}
				} else {
					if (Q == F) {
						// [17] a ?  b : a -> a ?  b : 0 -> "&" AND
						F = 0;
						cSid = db.SID_AND;
					} else {
						if (F == 0) {
							// [16] a ?  b : 0             "&" and
							cSid = db.SID_AND;
						} else {
							// [18] a ?  b : b -> b        ALREADY TESTED		
							// [19] a ?  b : c             "?" QTF
							cSid = db.SID_QTF;
						}
					}
				}
			}

			// have operands folded?
			if (cSid == 0) {
				// yes	
				freeMap(pStack);
				freeMap(pMap);
				freeVersion(pActive);
				return IBIT;
			}
			
			/*
			 * Lookup if 1n9 exists.
			 */

			// set slots
			if (cSid == db.SID_OR || cSid == db.SID_NE) {
				cSlots[0] = Q;
				cSlots[1] = F;
			} else if (cSid == db.SID_GT || cSid == db.SID_AND) {
				cSlots[0] = Q;
				cSlots[1] = Tu;
			} else {
				cSlots[0] = Q;
				cSlots[1] = Tu;
				cSlots[2] = F;
			}

			// test if node already exists
			uint32_t ix  = this->lookupNode(cSid, cSlots);
			uint32_t id = this->nodeIndex[ix]; // 0 if not found

			// get latest
			while (id != this->N[id].gid)
				id = this->N[id].gid;
			
			// does id exist and is it new
			if (id == 0 || pActive[id] == thisVersion) {
				// no
				freeMap(pStack);
				freeMap(pMap);
				freeVersion(pActive);
				return IBIT;
			}

			// remember
			pMap[nextNode++] = id;
			pActive[id] = thisVersion;
			
			if (numStack > maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (numStack != 1)
			ctx.fatal("[stack not empty]\n");

		// release and return
		uint32_t ret = pStack[0];
		assert(ret >= this->nstart);
		
		freeMap(pStack);
		freeMap(pMap);
		freeVersion(pActive);

		return ret;
	}

	/*
	 * @date 2021-11-05 03:09:35
	 * 
	 * Add a node to the group list
	 * Create a new list if necessary (gid=0)
	 * Return id of list header 
	 * 
	 * Handle merging of lists if sit/slot combo already belongs to a different list
	 * 
	 * @date 2021-11-07 14:41:06
	 * 
	 * With slot entry subsets, remove largest, don't prune because it provides sid diversity  
	 * 35      35      2       0       5       34      33      49:abc^d!/[5 7 9 33 0 0 0 0 0]
	 * 35      35      2       0       5       34      34      22:abc^^/[5 7 9 0 0 0 0 0 0]
	 * 
	 * With multiple sids of same node size, select lowest slots, prune as the don't contribute to cross-product
	 * 54      61      3       0       19      53      53      194:abcd^^^/[3 5 19 33 0 0 0 0 0] b d aceg^^^ fh^ ^^^
	 * 54      68      3       0       32      40      40      194:abcd^^^/[2 4 12 40 0 0 0 0 0] a c eg^ bdfh^^^ ^^^
         * 54      74      3       0       20      41      41      194:abcd^^^/[2 3 14 35 0 0 0 0 0] a b ceg^^ dfh^^ ^^^
         * 
         * Prune layers on collapsing
         * 
         * For final selection: nodes with highest layer and lowest slot entries
         * 
         * if gid=0, create new group, otherwise add node to group
         * return node id, which might change group
         * NOTE: caller must update group id `gid = this->N[nid].gid`.
	 */
	uint32_t addToCollection(uint32_t sid, uint32_t *pSlots, uint32_t gid, unsigned depth) {
		uint32_t ix = this->lookupNode(sid, pSlots);
		uint32_t nid = this->nodeIndex[ix];

		/*
		 * Test if node already exists
		 */
		if (nid != 0) {
			// node already exists, test if same group
			if (gid == 0 || this->N[nid].gid == gid)
				return nid; // duplicate

			printf("%s %s\n", this->saveString(gid).c_str(), this->saveString(N[nid].gid).c_str());

			// lhs(gid) is list header, rhs(nid) is a node, find its group 
			uint32_t rhs = nid;
			while (rhs != this->N[rhs].gid)
				rhs = this->N[rhs].gid;

			// merge groups lists
			if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);
			mergeGroups(gid, rhs, depth);
			if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);

			assert(this->N[nid].gid == gid);

			// return original, which should have relocated to a different group
			return nid;
		}

		/*
		 * Optimise similars already in group list
		 */
		
		if (gid != 0) {
			/*
			 * Check if sid already in group list
			 * If present: Lowest gets onto the list, highest gets orphaned
			 */
			for (uint32_t id = this->N[gid].next; id != this->N[id].gid; id = this->N[id].next) {
				const groupNode_t *pNode = this->N + id;

				if (pNode->sid == sid) {
					assert(pNode->sid != db.SID_SELF);

					/*
					 * Choose the lowest of the two.
					 */
					int cmp = this->compare(id, sid, pSlots);
					assert(cmp != 0);

					if (cmp < 0) {
						// list has lowest
						// rollback is o avoid newly created node from being orphaned 
						return id;
					}

					break;
				}
			}
		}

		/*
		 * Optionally create new group list plus header 
		 */

		if (gid == 0) {
			/*
			 * Start new group, never add SID_SELF nodes to index
			 */
			uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zerod
			assert(selfSlots[MAXSLOTS - 1] == 0);

			gid = this->newNode(db.SID_SELF, selfSlots);
			this->N[gid].gid = gid;
		}

		/*
		 * Point of no return 
		 */

		// create node
		nid = this->newNode(sid, pSlots);
		groupNode_t *pNode = this->N + nid;

		// add to list, keep it simple, SID_SELF is always first of list
		pNode->gid = gid;

		// add node to list
		linkNode(gid, nid);

		// add node to index
		pNode->hashIX              = ix;
		this->nodeIndex[ix]        = nid;
		this->nodeIndexVersion[ix] = this->nodeIndexVersionNr;

		return nid;
	}

	/*
	 * @date 2021-11-09 22:48:38
	 * 
	 * Merge two groups into one.
	 * Might cause node rewriting with a cascading effect
	 * 
	 * NOTE: until optimised, both lists are orphaned 
	 */
	uint32_t mergeGroups(uint32_t lhs, uint32_t rhs, unsigned depth) {

		assert(this->N[lhs].gid == lhs);
		assert(this->N[rhs].gid == rhs);

		/*
		 * Create new group header/list
		 */

		uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zeroed
		assert(selfSlots[MAXSLOTS - 1] == 0);

		uint32_t mergeGid = this->newNode(db.SID_SELF, selfSlots);
		this->N[mergeGid].gid = mergeGid;

		printf("MERGE %u %u -> %u\n", lhs, rhs, mergeGid);

		/*
		 * Relocate nodes to new head 
		 */

		// get left list
		uint32_t tmpListL = this->N[lhs].next;
		// unlink head from the list (relatively seeing, this empties the list)
		unlinkNode(lhs);
		// append list after last node of new group
		linkNode(this->N[mergeGid].prev, tmpListL);

		// get right list
		uint32_t tmpListR = this->N[rhs].next;
		// unlink head from the list (relatively seeing, this empties the list)
		unlinkNode(rhs);
		// append list after last node of new group
		linkNode(this->N[mergeGid].prev, tmpListR);

		// original lists should now be empty
		assert(this->N[lhs].next == lhs);
		assert(this->N[rhs].next == rhs);

		/*
		 * Update gid of all nodes in list
		 */

		this->N[lhs].gid = mergeGid;
		this->N[lhs].slots[0] = mergeGid;
		this->N[rhs].gid = mergeGid;
		this->N[rhs].slots[0] = mergeGid;

		for (uint32_t iNode = this->N[mergeGid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next)
			this->N[iNode].gid = mergeGid;

		/*
		 * Select better of multiple sids
		 */
		for (uint32_t iNode = this->N[mergeGid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
			const groupNode_t *pNode = this->N + iNode;

			/*
			 * Compare all list nodes with own list,
			 * all lesser nodes (not being self, thus breaking the loop) will be orphaned
			 */
			orphanLesser(mergeGid, pNode->sid, pNode->slots);
		}

		/*
		 * rebuild groups to resolve forward references 
		 */
		rebuildGroups();

		// rebuilding creates new groups 
		while (mergeGid != this->N[mergeGid].gid)
			mergeGid = this->N[mergeGid].gid;

		for (uint32_t iNode = this->N[mergeGid].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
			const groupNode_t *pNode = this->N + iNode;
			printf("M %u\t%u\t%u:%s/[%u %u %u %u %u %u %u %u %u]\n",
			       pNode->gid, iNode,
			       pNode->sid, db.signatures[pNode->sid].name,
			       pNode->slots[0], pNode->slots[1], pNode->slots[2], pNode->slots[3], pNode->slots[4], pNode->slots[5], pNode->slots[6], pNode->slots[7], pNode->slots[8]);
		}

		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);
		return mergeGid;
	}

	/*
	 * @date 2021-11-11 23:19:34
	 * 
	 * Rebuild groups that have nodes that have forward references
	 */
	void rebuildGroups(uint32_t forceGid = 0) {

		if (forceGid)
			printf("REBUILD %u\n", forceGid);
		else
			printf("CHECK/REBUILD\n");

		/*
		 * Walk through tree and search for outdated lists
		 * NOTE: this is about renumbering nodes, structures/patterns stay unchanged.
		 */
		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			// find group headers
			if (this->N[iGroup].gid == iGroup) {

				// is list up-to-date
				bool outdated = false;

				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;

					printf("C %u\t%u\t%u:%s/[",
					       pNode->gid, iNode,
					       pNode->sid, db.signatures[pNode->sid].name);

					for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
						uint32_t id = pNode->slots[iSlot];
						if (id == 0)
							break;

						if (iSlot != 0)
							putchar(' '); // delimiter
						printf("%u", id);

						if (id != this->N[id].gid) {
							outdated = true;
							if (id == forceGid)
								printf("<forced:new=%u>", this->N[id].gid);
							else
								printf("<outdated:new=%u>", this->N[id].gid);
						}
					}

					putchar(']');

					if (iGroup == forceGid) {
						outdated = true;
						printf("<forced>");
					}

					putchar('\n');

					if (outdated)
						break;
				}

				/*
				 * Group list is outdated, update
				 */
				if (outdated) {
					/*
					 * create new list header
					 */
					uint32_t selfSlots[MAXSLOTS] = {this->ncount}; // other slots are zeroed
					assert(selfSlots[MAXSLOTS - 1] == 0);

					uint32_t newGid = this->newNode(db.SID_SELF, selfSlots);
					this->N[newGid].gid = newGid;

					/*
					 * Walk and update the list, relocating or orphaning nodes
					 */

					printf("REBUILD %u->%u\n", iGroup, newGid);

					// breakpoint after displaying above line
					static int loopCount;
					if (loopCount++ > 20) {
						printf("LOOP\n");
						validateTree(0);
						exit(1);
					}


					char delimiter = 0;

					printf("nodes:");
					for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
						groupNode_t *pNode    = this->N + iNode;
						bool        nodeDated = false;

						for (unsigned iSlot = 0; iSlot < MAXSLOTS && !nodeDated; iSlot++) {
							uint32_t id = pNode->slots[iSlot];
							if (id == 0)
								break;

							if (id != this->N[id].gid) {
								nodeDated = true;
								break;
							}
						}

						// display delimiter
						if (delimiter)
							putchar(delimiter);
						delimiter = ',';

						if (nodeDated) {
							// create new node
							uint32_t newSlots[MAXSLOTS] = {0}; // other slots are zeroed
							assert(newSlots[MAXSLOTS - 1] == 0);

							for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
								uint32_t id = pNode->slots[iSlot];
								if (id == 0)
									break;

								while (id != this->N[id].gid)
									id = this->N[id].gid;

								newSlots[iSlot] = id;
							}

							// create new replacement
							uint32_t newNid = this->newNode(pNode->sid, newSlots);
							this->N[newNid].gid =  newGid;

							// add to list
							linkNode(this->N[newGid].prev, newNid);

							// add to index
							uint32_t ix = this->lookupNode(pNode->sid, newSlots);
							assert(ix != 0);

							this->N[newNid].hashIX     = ix;
							this->nodeIndex[ix]        = newNid;
							this->nodeIndexVersion[ix] = this->nodeIndexVersionNr;

							// let old node forward to replacement, which will forward to replacement header
							pNode->gid = newNid;

							printf("%u->%u", iNode, newNid);
						} else {
							// remember position next node in list
							uint32_t iNextNode = pNode->next;

							// unlink old node (invalidating next position)
							unlinkNode(iNode);

							// link to new list
							linkNode(this->N[newGid].prev, iNode);

							// part of new list
							pNode->gid = newGid;

							printf("%u", iNode);

							// reposition
							iNode = this->N[iNextNode].prev;
						}
					}

					printf("\n"); // end-of-list

					/*
					 * Let old and head (containing all the orphans) forward to new 
					 */
					this->N[iGroup].gid = newGid;
				}
			}
		}

		printf("/REBUILD\n");

		if (ctx.flags & context_t::MAGICMASK_PARANOID) validateTree(__LINE__);
	}

	/*
	 * @date 2021-11-13 01:41:11
	 * 
	 * Sids need to be unique in group lists
	 * If the argument is not found return 0
	 * If the argument is found and worse, orphan it and return 0
	 * If the argument is found and better return node id
	 *
	 *  if (orphanLesser(candidate) == 0)
	 *     ignoreCandidate();
	 */
	uint32_t orphanLesser(uint32_t gid, uint32_t sid, const uint32_t *pSlots) {

		for (uint32_t id = this->N[gid].next; id != this->N[id].gid; id = this->N[id].next) {
			const groupNode_t *pNode = this->N + id;

			if (pNode->sid == sid) {
				/*
				 * Choose the lowest of the two.
				 */
				int cmp = this->compare(id, sid, pSlots);

				if (cmp <= 0) {
					// list has best or self 
					return id;
				}

				// list has worse, orphan it
				unlinkNode(id);
				return 0;
			}
		}

		return 0;
	}

	/*
	 * @date 2021-11-16 13:21:47
	 * 
	 * Apply signature based endpoint swapping to slots
	 */
	void applySwapping(const signature_t *pSignature, uint32_t *pSlots) {
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
						if (this->compare(pSlots[i], this, pSlots[pTransformSwap[i] - 'a']) > 0) {
							needSwap = true;
							break;
						}
						if (this->compare(pSlots[i], this, pSlots[pTransformSwap[i] - 'a']) < 0) {
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

						changed = true;
					}
				}
			} while (changed);
		}
	}

	/*
	 * @date 2021-11-11 16:44:08
	 * 
	 * For debugging
	 */
	void validateTree(unsigned lineNr) {
		uint32_t *pVersion   = allocVersion();
		uint32_t thisVersion = ++mapVersionNr;
		int      errors      = 0;

		if (lineNr == 0)
			errors++;

		// clear version map when wraparound
		if (thisVersion == 0) {
			::memset(pVersion, 0, maxNodes * sizeof *pVersion);
			thisVersion = ++mapVersionNr;
		}

		// mark endpoints as defined
		for (uint32_t iKey = 0; iKey < this->nstart; iKey++) {
			assert(this->N[iKey].gid == iKey);
			assert(this->N[iKey].next == iKey);
			pVersion[iKey] = thisVersion;
		}

		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			// find group headers
			if (this->N[iGroup].gid == iGroup) {

				// does group have `1n9`
				bool has1n9 = false;
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;

					// ignore lists under construction
					if (pNode->sid == db.SID_OR || pNode->sid == db.SID_GT || pNode->sid == db.SID_NE || pNode->sid == db.SID_QNTF || pNode->sid == db.SID_AND || pNode->sid == db.SID_QTF) {
						has1n9 = true;
						break;
					}
				}
				if (!has1n9)
					errors++;
				
				// is list up-to-date
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;

					// test correct group (for broken group management)
					if (pNode->gid != iGroup)
						errors++;

					// test double defined (for broken linked lists)
					if (pVersion[iNode] == thisVersion)
						errors++;

					bool outdated = false;
					for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
						uint32_t id = pNode->slots[iSlot];
						if (id == 0)
							break;

						// update
						if (id != this->N[id].gid) {
							outdated = true;
							while (id != this->N[id].gid)
								id = this->N[id].gid;
						}
							
						if (pVersion[id] != thisVersion) {
							// reference not defined
							errors++;
						} else if (id != this->N[id].gid) {
							// reference orphaned
							errors++;
						}
					}
					
					// test if node folds
					if (outdated) {
						uint32_t nid = verifySignature(pNode->sid, pNode->slots);
						assert(nid != IBIT); // todo: do better
					}

					// mark node found
					pVersion[iNode] = thisVersion;
				}

				// test double defined
				if (pVersion[iGroup] == thisVersion)
					errors++;

				// mark header found
				pVersion[iGroup] = thisVersion;
			}
		}

		if (errors == 0) {
			freeVersion(pVersion);
			return;
		}

		printf("INVALIDTREE at line %u\n", lineNr);

		// bump version
		thisVersion = ++mapVersionNr;

		if (thisVersion == 0) {
			::memset(pVersion, 0, maxNodes * sizeof *pVersion);
			thisVersion = ++mapVersionNr;
		}

		// mark endpoints as defined
		for (uint32_t iKey = 0; iKey < this->nstart; iKey++)
			pVersion[iKey] = thisVersion;

		for (uint32_t iGroup = this->nstart; iGroup < this->ncount; iGroup++) {
			// find group headers
			if (this->N[iGroup].gid == iGroup) {

				// does group have `1n9`
				bool has1n9 = false;
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;

					// ignore lists under construction
					if (pNode->sid == db.SID_OR || pNode->sid == db.SID_GT || pNode->sid == db.SID_NE || pNode->sid == db.SID_QNTF || pNode->sid == db.SID_AND || pNode->sid == db.SID_QTF) {
						has1n9 = true;
						break;
					}
				}
				if (!has1n9)
					printf("<MISSING-1N9 gid=%u>\n", iGroup);

				// is list up-to-date
				for (uint32_t iNode = this->N[iGroup].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;

					// test double defined
					if (pVersion[iNode] == thisVersion)
						printf("<DOUBLE nid=%u>", iNode);

					// test correct group
					if (pNode->gid != iGroup)
						printf("<GROUP>");

					printf("%u\t%u\t%u:%s/[",
					       pNode->gid, iNode,
					       pNode->sid, db.signatures[pNode->sid].name);

					char delimiter = 0;

					for (unsigned iSlot = 0; iSlot < MAXSLOTS; iSlot++) {
						uint32_t id = pNode->slots[iSlot];
						if (id == 0)
							break;

						if (delimiter)
							putchar(delimiter);
						delimiter = ' ';

						printf("%u", pNode->slots[iSlot]);

						if (pVersion[id] != thisVersion) {
							// reference not defined
							printf("<MISSING>");
						} else if (id != this->N[id].gid) {
							// reference orphaned
							printf("<ERROR:gid=%u>", this->N[id].gid);
						}
					}

					printf("]\n");

					// mark node found
					pVersion[iNode] = thisVersion;
				}

				// test double defined
				if (pVersion[iGroup] == thisVersion)
					printf("<DOUBLE gid=%u>", iGroup);

				// mark header found
				pVersion[iGroup] = thisVersion;
			}
		}

		freeVersion(pVersion);

		exit(1);
	}

	/*
	 * @date 2021-11-16 19:36:42
	 * 
	 * Test arguments belong to the same group
	 */
	inline bool isSameGroup(uint32_t gid, uint32_t nid) const {
		while (nid != this->N[nid].gid)
			nid = this->N[nid].gid;

		return gid == nid;
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
	 * @date 2021-11-04 01:15:46
	 *
	 * Export a sub-tree with a given head id as siting.
	 * Optionally endpoint normalised with a separate transform.
	 * Return string is static allocated
	 * 
	 * NOTE: Tree is expressed in terms of 1n9 nodes
	 * NOTE: `std::string` usage exception because this is NOT speed critical code AND strings can become gigantically large
	 */
	std::string saveString(uint32_t id, std::string *pTransform = NULL) {

		// get latest gid
		uint32_t gid = id & ~IBIT;
		while (gid != this->N[gid].gid)
			gid = this->N[gid].gid;

		// should be same as argument
		assert(this->N[id & ~IBIT].gid == gid);

		std::string name;

		/*
		 * Endpoints are simple
		 */
		if (gid < this->nstart) {
			if (pTransform) {
				pTransform->clear();
				if (gid == 0) {
					name += '0';
				} else {
					uint32_t value = gid - this->kstart;

					if (value < 26) {
						*pTransform += (char) ('a' + value);
					} else {
						encodePrefix(*pTransform, value / 26);
						*pTransform += (char) ('a' + (value % 26));
					}

					name += 'a';
				}

			} else {
				if (gid == 0) {
					name += '0';
				} else {
					uint32_t value = gid - this->kstart;
					if (value < 26) {
						name += (char) ('a' + value);
					} else {
						encodePrefix(name, value / 26);
						name += (char) ('a' + (value % 26));
					}
				}
			}


			// test for invert
			if (id & IBIT)
				name += '~';

			return name;
		}

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
		pStack[numStack++] = gid;

		do {
			// pop stack
			uint32_t curr = pStack[--numStack];

			// get latest
			while (curr != this->N[curr].gid)
				curr = this->N[curr].gid;
			
			assert(curr != 0);

			// if endpoint then emit
			if (curr < this->nstart) {
				uint32_t value;

				if (!pTransform) {
					// endpoint
					value = curr - this->kstart;
				} else {
					// placeholder
					if (pVersion[curr] != thisVersion) {
						pVersion[curr] = thisVersion;
						pMap[curr]     = nextPlaceholder++;

						value = curr - this->kstart;
						if (value < 26) {
							*pTransform += (char) ('a' + value);
						} else {
							encodePrefix(*pTransform, value / 26);
							*pTransform += (char) ('a' + (value % 26));
						}
					}

					value = pMap[curr] - this->kstart;
				}

				// convert id to (prefixed) letter
				if (value < 26) {
					name += (char) ('a' + value);
				} else {
					encodePrefix(name, value / 26);
					name += (char) ('a' + (value % 26));
				}

				continue;
			}

			/*
			 * First node in group list is SID_SELF,
			 * Second node is 1n9
			 */

			// find group headers
			assert(this->N[curr].gid == curr); // must be header
			assert(this->N[curr].next != curr); // may not be empty

			// top-level components	
			uint32_t Q = 0, Tu = 0, Ti = 0, F = 0;

			// walk through group list in search of a `1n9` node
			for (uint32_t iNode = this->N[curr].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
				const groupNode_t *pNode = this->N + iNode;

				if (pNode->sid == db.SID_OR) {
					Q  = pNode->slots[0];
					Tu = 0;
					Ti = IBIT;
					F  = pNode->slots[1];
					break;
				} else if (pNode->sid == db.SID_GT) {
					Q  = pNode->slots[0];
					Tu = pNode->slots[1];
					Ti = IBIT;
					F  = 0;
					break;
				} else if (pNode->sid == db.SID_NE) {
					Q  = pNode->slots[0];
					Tu = pNode->slots[1];
					Ti = IBIT;
					F  = pNode->slots[1];
					break;
				} else if (pNode->sid == db.SID_AND) {
					Q  = pNode->slots[0];
					Tu = pNode->slots[1];
					Ti = 0;
					F  = 0;
					break;
				} else if (pNode->sid == db.SID_QNTF) {
					Q  = pNode->slots[0];
					Tu = pNode->slots[1];
					Ti = IBIT;
					F  = pNode->slots[2];
					break;
				} else if (pNode->sid == db.SID_QTF) {
					Q  = pNode->slots[0];
					Tu = pNode->slots[1];
					Ti = 0;
					F  = pNode->slots[2];
					break;
				}
			}
			if (Q == 0) {
				for (uint32_t iNode = this->N[curr].next; iNode != this->N[iNode].gid; iNode = this->N[iNode].next) {
					const groupNode_t *pNode = this->N + iNode;
					printf("E %u\t%u\t%u:%s/[%u %u %u %u %u %u %u %u %u]\n",
					       pNode->gid, iNode,
					       pNode->sid, db.signatures[pNode->sid].name,
					       pNode->slots[0], pNode->slots[1], pNode->slots[2], pNode->slots[3], pNode->slots[4], pNode->slots[5], pNode->slots[6], pNode->slots[7], pNode->slots[8]);
				}

				ctx.fatal("\n{\"error\":\"group misses 1n9\",\"where\":\"%s:%s:%d\",\"gid\":%u}\n",
					  __FUNCTION__, __FILE__, __LINE__, curr);
			}

			// determine if node already handled
			if (pVersion[curr] != thisVersion) {
				// first time
				pVersion[curr] = thisVersion;
				pMap[curr]     = 0;

				// push id so it visits again after expanding
				pStack[numStack++] = curr;

				assert(N[Q].gid == Q);
				assert(N[Tu].gid == Tu);
				assert(N[F].gid == F);

				// push non-zero endpoints
				if (F >= this->kstart)
					pStack[numStack++] = F;
				if (Tu != F && Tu >= this->kstart)
					pStack[numStack++] = Tu;
				if (Q >= this->kstart)
					pStack[numStack++] = Q;

				assert(numStack < maxNodes);

			} else if (pMap[curr] == 0) {
				// node complete, output operator
				pMap[curr] = nextNode++;

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
				// back-reference to previous node
				uint32_t dist = nextNode - pMap[curr];

				// convert id to (prefixed) back-link
				if (dist < 10) {
					name += (char) ('0' + dist);
				} else {
					encodePrefix(name, dist / 10);
					name += (char) ('0' + (dist % 10));
				}
			}

		} while (numStack > 0);

		assert(nextPlaceholder <= this->nstart);

		// test for inverted-root
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
		pPoolVersion  = (uint32_t **) ctx.myAlloc("groupTree_t::pPoolVersion", MAXPOOLARRAY, sizeof(*pPoolVersion));
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

			// output keys
			for (uint32_t iKey = 0; iKey < nstart; iKey++) {
				// get remapped
				groupNode_t wrtNode;
				wrtNode.sid = 2; // 0=reserved, 1="0", 2="a"
				wrtNode.slots[0] = iKey;
				for (unsigned i = 1; i < MAXSLOTS; i++)
					wrtNode.slots[i] = 0;

				pMap[iKey] = nextId++;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.sid));
				for (unsigned i = 1; i < MAXSLOTS; i++)
					__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.slots[i]));
			}

			// output keys
			for (uint32_t iNode = nstart; iNode < ncount; iNode++) {
				const groupNode_t *pNode = this->N + iNode;

				// get remapped
				groupNode_t wrtNode;
				wrtNode.sid = pNode->sid;
				for (unsigned i = 0; i < MAXSLOTS; i++)
					wrtNode.slots[i] = pMap[pNode->slots[i]];

				pMap[iNode] = nextId++;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.sid));
				for (unsigned i = 1; i < MAXSLOTS; i++)
					__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.slots[i]));

			}
		} else {
			uint32_t *pStack     = allocMap();
			uint32_t *pVersion   = allocVersion();
			uint32_t thisVersion = ++mapVersionNr;

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
				groupNode_t wrtNode;
				wrtNode.sid = 2; // 0=reserved, 1="0", 2="a"
				wrtNode.slots[0] = iKey;
				for (unsigned i = 1; i < MAXSLOTS; i++)
					wrtNode.slots[i] = 0;

				size_t len = sizeof wrtNode;
				fwrite(&wrtNode, len, 1, outf);
				fpos += len;

				pMap[iKey] = nextId++;

				__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.sid));
				for (unsigned i = 1; i < MAXSLOTS; i++)
					__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(wrtNode.slots[i]));
			}

			assert(!"placeholder");

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
