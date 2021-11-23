#ifndef _DATABASE_H
#define _DATABASE_H

/*
 * @date 2020-03-12 14:40:19
 *
 * `database.h` has all your database needs.
 *
 * - Creating/opening/reading/writing of `mmap()`-ed files
 * - Multiple collections
 * - Indexing of data
 * - Lookup/creating rows
 *
 * One of the prime objectives is to keep the database less than 32G bytes in size
 *
 * Most indices are hashtable lookup tables with overflow.
 * Index table sizes must be prime.
 *
 * Each collection has a number of administrative entry points
 *
 *  	`uint32_t      numCollection`      - Number of rows in collection
 *  	`uint32_t      maxCollection`      - Maximum number of rows the collection can contain
 *      `collection_t* collection`         - Pointer to first entry in collection
 *      `uint32_t      collectionIndexSize - Index size. This must be prime
 *      `uint32_t      collectionIndex     - Start of index
 *
 * @date 2020-03-23 14:03:12
 *
 * `mmap()` is used to exploit the use of shared memory.
 * When running parallel jobs the large imprint section can be shared.
 *
 * @date 2020-04-15 02:00:58
 *
 * The initial starting positions of the indices use crc as a hash function.
 * It doesn't really have to be crc,  as long as the result has some linear distribution over index.
 * crc32 was chosen because it has a single assembler instruction on x86 platforms.
 *
 * @date 2020-04-17 11:10:10
 *
 * Add support versioned memory for fast erasing and deleting of entries.
 * An entry is deleted if `"index[ix] == 0 && version != NULL && version[ix] == iVersion"`
 * An entry is empty(or deleted) if `"index[ix] == 0 || (version != NULL && version[ix] != iVersion)"`
 * An entry is valid if `"index[ix] != 0 && (version == NULL || version[ix] == iVersion)"`
 *
 * @date 2020-04-27 19:46:45
 *
 * Replace `::memcpy()` when possible with mmap copy-on-write.
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

#include <errno.h>
#include <fcntl.h>
#include <jansson.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "config.h"
#include "datadef.h"
#include "tinytree.h"

/// @constant {number} FILE_MAGIC - Database version. Update this when either the file header or one of the structures change
#define FILE_MAGIC        0x20210715
// NOTE: with next version, reposition `magic_sidCRC`
// NOTE: with next version, add `idFirst`

/*
 *  All components contributing and using the database should share the same dimensions
 */

/**
 * @date 2020-03-12 15:09:50
 *
 * The database file header
 *
 * @typedef {object} fileHeader_t
 */
struct fileHeader_t {
	// environment metrics
	uint32_t magic;                  // magic+version
	uint32_t magic_flags;            // conditions it was created
	uint32_t magic_maxSlots;
	uint32_t magic_sizeofSignature;
	uint32_t magic_sizeofSwap;
	uint32_t magic_sidCRC;           // crc of signature names
	uint32_t magic_sizeofImprint;
	uint32_t magic_sizeofPair;
	uint32_t magic_sizeofMember;
	uint32_t magic_sizeofPatternFirst;
	uint32_t magic_sizeofPatternSecond;
	uint32_t magic_sizeofGrow;

	// Associative index interleaving (for Imprints)
	uint32_t interleave;
	uint32_t interleaveStep;

	// section sizes
	uint32_t numTransform;          // for both fwd/rev
	uint32_t transformIndexSize;    // for both fwd/rev
	uint32_t numEvaluator;          // for both fwd/rev. Evaluator has no index.
	uint32_t numSignature;
	uint32_t signatureIndexSize;
	uint32_t numSwap;
	uint32_t swapIndexSize;
	uint32_t numUnused;             // unused
	uint32_t unusedIndexSize;       // unused
	uint32_t numImprint;
	uint32_t imprintIndexSize;
	uint32_t numPair;
	uint32_t pairIndexSize;
	uint32_t numMember;
	uint32_t memberIndexSize;
	uint32_t numPatternFirst;
	uint32_t patternFirstIndexSize;
	uint32_t numPatternSecond;
	uint32_t patternSecondIndexSize;
	uint32_t numGrow;
	uint32_t growIndexSize;

	// section offsets
	uint64_t offFwdTransforms;
	uint64_t offRevTransforms;
	uint64_t offFwdTransformNames;
	uint64_t offRevTransformNames;
	uint64_t offRevTransformIds;
	uint64_t offFwdTransformNameIndex;
	uint64_t offRevTransformNameIndex;
	uint64_t offFwdEvaluator;
	uint64_t offRevEvaluator;
	uint64_t offSignatures;
	uint64_t offSignatureIndex;
	uint64_t offSwaps;
	uint64_t offSwapIndex;
	uint64_t offUnused;             // unused
	uint64_t offUnusedIndex;        // unused
	uint64_t offImprints;
	uint64_t offImprintIndex;
	uint64_t offpairs;
	uint64_t offPairIndex;
	uint64_t offMember;
	uint64_t offMemberIndex;
	uint64_t offPatternFirst;
	uint64_t offPatternFirstIndex;
	uint64_t offPatternSecond;
	uint64_t offPatternSecondIndex;
	uint64_t offGrows;
	uint64_t offGrowIndex;

	uint64_t offEnd;
};


/**
 * @date 2020-03-12 15:17:55
 *
 * The *DATABASE*
 */
struct database_t {

	/**
	 * @date 2020-03-12 15:19:50
	 *
	 * Runtime flags to indicate which sections were allocated. If not then they are read-only mmapped.
	 */
	enum {
		ALLOCFLAG_TRANSFORM = 0,
		ALLOCFLAG_EVALUATOR,
		ALLOCFLAG_SIGNATURE,
		ALLOCFLAG_SIGNATUREINDEX,
		ALLOCFLAG_SWAP,
		ALLOCFLAG_SWAPINDEX,
		ALLOCFLAG_UNUSED, // unused
		ALLOCFLAG_UNUSEDINDEX, // unused
		ALLOCFLAG_IMPRINT,
		ALLOCFLAG_IMPRINTINDEX,
		ALLOCFLAG_PAIR,
		ALLOCFLAG_PAIRINDEX,
		ALLOCFLAG_MEMBER,
		ALLOCFLAG_MEMBERINDEX,
		ALLOCFLAG_PATTERNFIRST,
		ALLOCFLAG_PATTERNFIRSTINDEX,
		ALLOCFLAG_PATTERNSECOND,
		ALLOCFLAG_PATTERNSECONDINDEX,

		// @formatter:off
		ALLOCMASK_TRANSFORM          = 1 << ALLOCFLAG_TRANSFORM,
		ALLOCMASK_EVALUATOR          = 1 << ALLOCFLAG_EVALUATOR,
		ALLOCMASK_SIGNATURE          = 1 << ALLOCFLAG_SIGNATURE,
		ALLOCMASK_SIGNATUREINDEX     = 1 << ALLOCFLAG_SIGNATUREINDEX,
		ALLOCMASK_SWAP               = 1 << ALLOCFLAG_SWAP,
		ALLOCMASK_SWAPINDEX          = 1 << ALLOCFLAG_SWAPINDEX,
		ALLOCMASK_UNUSED             = 1 << ALLOCFLAG_UNUSED,
		ALLOCMASK_UNUSEDINDEX        = 1 << ALLOCFLAG_UNUSEDINDEX,
		ALLOCMASK_IMPRINT            = 1 << ALLOCFLAG_IMPRINT,
		ALLOCMASK_IMPRINTINDEX       = 1 << ALLOCFLAG_IMPRINTINDEX,
		ALLOCMASK_PAIR               = 1 << ALLOCFLAG_PAIR,
		ALLOCMASK_PAIRINDEX          = 1 << ALLOCFLAG_PAIRINDEX,
		ALLOCMASK_MEMBER             = 1 << ALLOCFLAG_MEMBER,
		ALLOCMASK_MEMBERINDEX        = 1 << ALLOCFLAG_MEMBERINDEX,
		ALLOCMASK_PATTERNFIRST       = 1 << ALLOCFLAG_PATTERNFIRST,
		ALLOCMASK_PATTERNFIRSTINDEX  = 1 << ALLOCFLAG_PATTERNFIRSTINDEX,
		ALLOCMASK_PATTERNSECOND      = 1 << ALLOCFLAG_PATTERNSECOND,
		ALLOCMASK_PATTERNSECONDINDEX = 1 << ALLOCFLAG_PATTERNSECONDINDEX,
		// @formatter:on
	};
	
	/*
	 * @date 2021-11-08 02:36:49
	 * 
	 * Each section starts with reserved entries
	 * Record 0, is all zeros. used as reference to indicate a new entry.
	 * Record 1, is all zeros. used as reference to indicate a deleted entry.
	 * 
	 * ignore records that are all zero 
	 */
	enum {
		IDFREE    = 0, // reserved for new entries				
		IDDELETED = 1, // reserved for deleted/unused entries
	};

	// I/O context
	context_t &ctx;

	int             hndl;
	const uint8_t   *rawData;                    // base location of mmap segment
	fileHeader_t    fileHeader;                  // file header
	size_t          fileSize;                    // size of original file
	uint32_t        creationFlags;               // creation constraints
	uint32_t        allocFlags;                  // memory constraints
	uint32_t	IDFIRST;                     // Advised starting id for first record
	// transforms
	uint32_t        numTransform;                // number of elements in collection
	uint32_t        maxTransform;                // maximum size of collection
	uint64_t        *fwdTransformData;           // forward transform (binary)
	uint64_t        *revTransformData;           // reverse transform (binary)
	transformName_t *fwdTransformNames;          // forward transform (string)
	transformName_t *revTransformNames;          // reverse transform (string)
	uint32_t        *revTransformIds;            // reverse transform (id)
	uint32_t        transformIndexSize;          // index size (must be prime)
	uint32_t        *fwdTransformNameIndex;      // fwdTransformNames index
	uint32_t        *revTransformNameIndex;      // revTransformNames index
	// evaluator store [COPY-ON-WRITE] Preloaded for a `tinyTree_t`.
	uint32_t        numEvaluator;                // number of evaluators (tinyTree_t::TINYTREE_NEND * MAXTRANSFORM)
	uint32_t        maxEvaluator;                // maximum size of collection
	footprint_t     *fwdEvaluator;               // evaluator for forward transforms
	footprint_t     *revEvaluator;               // evaluator for reverse transforms
	// signature store
	uint32_t        numSignature;                // number of signatures
	uint32_t        maxSignature;                // maximum size of collection
	signature_t     *signatures;                 // signature collection
	uint32_t        signatureIndexSize;          // index size (must be prime)
	uint32_t        *signatureIndex;             // index
	// swap store
	uint32_t        numSwap;                     // number of swaps
	uint32_t        maxSwap;                     // maximum size of collection
	swap_t          *swaps;                      // swap collection
	uint32_t        swapIndexSize;               // index size (must be prime)
	uint32_t        *swapIndex;                  // index
	// imprint store
	uint32_t        interleave;                  // imprint interleave factor (display value)
	uint32_t        interleaveStep;              // imprint interleave factor (interleave distance)
	uint32_t        numImprint;                  // number of elements in collection
	uint32_t        maxImprint;                  // maximum size of collection
	imprint_t       *imprints;                   // imprint collection
	uint32_t        imprintIndexSize;            // index size (must be prime)
	uint32_t        *imprintIndex;               // index
	// pair store
	uint32_t        numPair;                     // number of sid/tid pairs
	uint32_t        maxPair;                     // maximum size of collection
	pair_t          *pairs;                      // sid/tid pair collection
	uint32_t        pairIndexSize;               // index size (must be prime)
	uint32_t        *pairIndex;                  // index
	// member store
	uint32_t        numMember;                   // number of members
	uint32_t        maxMember;                   // maximum size of collection
	member_t        *members;                    // member collection
	uint32_t        memberIndexSize;             // index size (must be prime)
	uint32_t        *memberIndex;                // index
	// patternFirst store
	uint32_t        numPatternFirst;             // number of patternsFirst
	uint32_t        maxPatternFirst;             // maximum size of collection
	patternFirst_t  *patternsFirst;              // patternFirst collection
	uint32_t        patternFirstIndexSize;       // index size (must be prime)
	uint32_t        *patternFirstIndex;          // index
	// patternSecond store
	uint32_t        numPatternSecond;            // number of patternsSecond
	uint32_t        maxPatternSecond;            // maximum size of collection
	patternSecond_t *patternsSecond;             // patternSecond collection
	uint32_t        patternSecondIndexSize;      // index size (must be prime)
	uint32_t        *patternSecondIndex;         // index
	// versioned memory
	uint32_t        iVersion;                    // version current incarnation
	uint32_t        *imprintVersion;             // versioned memory for `imprintIndex`
	uint32_t        *signatureVersion;           // versioned memory for `signatureIndex`
	// reserved 1n9 SID id's
	uint32_t        SID_ZERO, SID_SELF, SID_OR, SID_GT, SID_NE, SID_AND, SID_QNTF, SID_QTF;

	/**
	 * Constructor
	 */
	database_t(context_t &ctx) : ctx(ctx) {
		hndl    = 0;
		rawData = NULL;
		::memset(&fileHeader, 0, sizeof(fileHeader));
		creationFlags = 0;
		allocFlags    = 0;
		IDFIRST       = 1;

		// transform store
		numTransform          = 0;
		maxTransform          = 0;
		transformIndexSize    = 0;
		fwdTransformData      = revTransformData      = NULL;
		fwdTransformNames     = revTransformNames     = NULL;
		fwdTransformNameIndex = revTransformNameIndex = NULL;
		revTransformIds       = NULL;

		// evaluator store [COPY-ON-WRITE]
		numEvaluator = 0;
		maxEvaluator = 0;
		fwdEvaluator = NULL;
		revEvaluator = NULL;

		// signature store
		numSignature       = 0;
		maxSignature       = 0;
		signatures         = NULL;
		signatureIndexSize = 0;
		signatureIndex     = NULL;

		// swap store
		numSwap       = 0;
		maxSwap       = 0;
		swaps         = NULL;
		swapIndexSize = 0;
		swapIndex     = NULL;

		// imprint store
		interleave       = 1;
		interleaveStep   = 1;
		numImprint       = 0;
		maxImprint       = 0;
		imprints         = NULL;
		imprintIndexSize = 0;
		imprintIndex     = NULL;

		// sid/tid store
		numPair = 0;
		maxPair         = 0;
		pairs         = NULL;
		pairIndexSize = 0;
		pairIndex     = NULL;

		// member store
		numMember       = 0;
		maxMember       = 0;
		members         = NULL;
		memberIndexSize = 0;
		memberIndex     = NULL;

		// patternFirst store
		numPatternFirst       = 0;
		maxPatternFirst       = 0;
		patternsFirst         = NULL;
		patternFirstIndexSize = 0;
		patternFirstIndex     = NULL;

		// patternSecond store
		numPatternSecond       = 0;
		maxPatternSecond       = 0;
		patternsSecond         = NULL;
		patternSecondIndexSize = 0;
		patternSecondIndex     = NULL;

		// versioned memory
		iVersion         = 0;
		imprintVersion   = NULL;
		signatureVersion = NULL;
		
		// 1n9 sids
		SID_ZERO = SID_SELF = SID_OR = SID_GT = SID_NE = SID_AND = SID_QNTF = SID_QTF = 0;
	};

	/**
	 * @date 2020-03-12 15:57:37
	 *
	 * Release system resources
	 */
	~database_t() {
		/*
		 * Free explicitly malloced sections
		 */
		if (allocFlags & ALLOCMASK_TRANSFORM) {
			ctx.myFree("database_t::fwdTransformData", fwdTransformData);
			ctx.myFree("database_t::revTransformData", revTransformData);
			ctx.myFree("database_t::fwdTransformNames", fwdTransformNames);
			ctx.myFree("database_t::revTransformNames", revTransformNames);
			ctx.myFree("database_t::revTransformIds", revTransformIds);
			ctx.myFree("database_t::fwdTransformNameIndex", fwdTransformNameIndex);
			ctx.myFree("database_t::revTransformNameIndex", revTransformNameIndex);
		}
		if (allocFlags & ALLOCMASK_EVALUATOR) {
			ctx.myFree("database_t::fwdEvaluator", fwdEvaluator);
			ctx.myFree("database_t::revEvaluator", revEvaluator);
		}
		if (allocFlags & ALLOCMASK_SIGNATURE)
			ctx.myFree("database_t::signatures", signatures);
		if (allocFlags & ALLOCMASK_SIGNATUREINDEX)
			ctx.myFree("database_t::signatureIndex", signatureIndex);
		if (allocFlags & ALLOCMASK_SWAP)
			ctx.myFree("database_t::swaps", swaps);
		if (allocFlags & ALLOCMASK_SWAPINDEX)
			ctx.myFree("database_t::swapIndex", swapIndex);
		if (allocFlags & ALLOCMASK_IMPRINT)
			ctx.myFree("database_t::imprints", imprints);
		if (allocFlags & ALLOCMASK_IMPRINTINDEX)
			ctx.myFree("database_t::imprintIndex", imprintIndex);
		if (allocFlags & ALLOCMASK_PAIR)
			ctx.myFree("database_t::pairs", pairs);
		if (allocFlags & ALLOCMASK_PAIRINDEX)
			ctx.myFree("database_t::pairIndex", pairIndex);
		if (allocFlags & ALLOCMASK_MEMBER)
			ctx.myFree("database_t::members", members);
		if (allocFlags & ALLOCMASK_MEMBERINDEX)
			ctx.myFree("database_t::memberIndex", memberIndex);
		if (allocFlags & ALLOCMASK_PATTERNFIRST)
			ctx.myFree("database_t::patternsFirst", patternsFirst);
		if (allocFlags & ALLOCMASK_PATTERNFIRSTINDEX)
			ctx.myFree("database_t::patternFirstIndex", patternFirstIndex);
		if (allocFlags & ALLOCMASK_PATTERNSECOND)
			ctx.myFree("database_t::patternsSecond", patternsSecond);
		if (allocFlags & ALLOCMASK_PATTERNSECONDINDEX)
			ctx.myFree("database_t::patternSecondIndex", patternSecondIndex);

		// release versioned memory
		disableVersioned();

		/*
		 * Release resources
		 */
		if (hndl) {
			/*
			 * Database was opened with `mmap()`
			 */
			if (::munmap((void *) rawData, this->fileSize))
				ctx.fatal("\n{\"error\":\"munmap()\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", __FUNCTION__, __FILE__, __LINE__);
			if (::close(hndl))
				ctx.fatal("\n{\"error\":\"close()\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", __FUNCTION__, __FILE__, __LINE__);
		} else if (rawData) {
			/*
			 * Database was loaded with `read()`
			 */
			ctx.myFree("database_t::rawData", (void *) rawData);
		}
	}

	/**
	 * @date 2020-04-17 00:54:09
	 *
	 * Enable versioned memory for selected indices.
	 * 
	 * This allows the single-instruction erasing of signature/imprints instead of memzeroing them. 
	 */
	inline void enableVersioned(void) {

		// allocate version indices
		if (allocFlags & ALLOCMASK_IMPRINTINDEX)
			imprintVersion   = (uint32_t *) ctx.myAlloc("database_t::imprintVersion", imprintIndexSize, sizeof(*imprintVersion));
		if (allocFlags & ALLOCMASK_SIGNATUREINDEX)
			signatureVersion = (uint32_t *) ctx.myAlloc("database_t::signatureVersion", signatureIndexSize, sizeof(*signatureVersion));

		// clear versioned memory
		iVersion = 0;
		InvalidateVersioned();
	}

	/**
	 * @date 2020-04-22 15:12:42
	 *
	 * Enable versioned memory for selected indices
	 */
	inline void disableVersioned(void) {

		if (signatureVersion) {
			ctx.myFree("database_t::signatureVersion", signatureVersion);
			signatureVersion = NULL;
		}
		if (imprintVersion) {
			ctx.myFree("database_t::imprintVersion", imprintVersion);
			imprintVersion = NULL;
		}
	}

	/**
	 * @date 2020-04-17 00:54:09
	 *
	 * Invalidate versioned memory effectively resetting the indices
	 */
	inline void InvalidateVersioned(void) {
		// clear versioned memory
		if (iVersion == 0) {
			if (imprintVersion)
				::memset(imprintVersion, 0, imprintIndexSize * sizeof(*imprintVersion));
			if (signatureVersion)
				::memset(signatureVersion, 0, signatureIndexSize * sizeof(*signatureVersion));
		}

		// bump version number.
		iVersion++;
	}

	/**
	 * @date 2020-03-15 22:25:41
	 *
	 * Inherit read-only sections from a source database.
	 *
	 * NOTE: call after calling `create()`
	 *
	 * @param {database_t} pFrom - Database to inherit from
	 * @param {string} pName - Name of database
	 * @param {number} inheritSections - set of sections to inherit
	 */
	void inheritSections(const database_t *pFrom, const char *pName, unsigned inheritSections) {

		// transform store
		if (inheritSections & ALLOCMASK_TRANSFORM) {
			if (pFrom->numTransform == 0)
				ctx.fatal("\n{\"error\":\"Missing transform section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			assert(maxTransform == 0);
			maxTransform = pFrom->maxTransform;
			numTransform = pFrom->numTransform;

			fwdTransformData  = pFrom->fwdTransformData;
			revTransformData  = pFrom->revTransformData;
			fwdTransformNames = pFrom->fwdTransformNames;
			revTransformNames = pFrom->revTransformNames;
			revTransformIds   = pFrom->revTransformIds;

			assert(transformIndexSize == 0);
			transformIndexSize = pFrom->transformIndexSize;

			fwdTransformNameIndex = pFrom->fwdTransformNameIndex;
			revTransformNameIndex = pFrom->revTransformNameIndex;
		}

		// evaluator store [COPY-ON-WRITE]
		if (inheritSections & ALLOCMASK_EVALUATOR) {
			if (pFrom->numEvaluator == 0)
				ctx.fatal("\n{\"error\":\"Missing evaluator section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			assert(maxEvaluator == 0);
			maxEvaluator = pFrom->maxEvaluator;
			numEvaluator = pFrom->numEvaluator;

			fwdEvaluator  = pFrom->fwdEvaluator;
			revEvaluator  = pFrom->revEvaluator;
		}

		// signature store
		if (inheritSections & (ALLOCMASK_SIGNATURE | ALLOCMASK_SIGNATUREINDEX)) {
			if (pFrom->numSignature == 0)
				ctx.fatal("\n{\"error\":\"Missing signature section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_SIGNATURE) {
				assert(!(allocFlags & ALLOCMASK_SIGNATURE));
				this->maxSignature = pFrom->maxSignature;
				this->numSignature = pFrom->numSignature;
				this->signatures   = pFrom->signatures;
			}

			if (inheritSections & ALLOCMASK_SIGNATUREINDEX) {
				assert(!(allocFlags & ALLOCMASK_SIGNATUREINDEX));
				this->signatureIndexSize = pFrom->signatureIndexSize;
				this->signatureIndex     = pFrom->signatureIndex;
			}
		}

		// swap store
		if (inheritSections & (ALLOCMASK_SWAP | ALLOCMASK_SWAPINDEX)) {
			if (pFrom->numSwap == 0)
				ctx.fatal("\n{\"error\":\"Missing swap section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_SWAP) {
				assert(!(allocFlags & ALLOCMASK_SWAP));
				this->maxSwap = pFrom->maxSwap;
				this->numSwap = pFrom->numSwap;
				this->swaps   = pFrom->swaps;
			}

			if (inheritSections & ALLOCMASK_SWAPINDEX) {
				assert(!(allocFlags & ALLOCMASK_SWAPINDEX));
				this->swapIndexSize = pFrom->swapIndexSize;
				this->swapIndex     = pFrom->swapIndex;
			}
		}

		// imprint store
		if (inheritSections & (ALLOCMASK_IMPRINT | ALLOCMASK_IMPRINTINDEX)) {
			if (pFrom->numImprint == 0)
				ctx.fatal("\n{\"error\":\"Missing imprint section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			this->interleave     = pFrom->interleave;
			this->interleaveStep = pFrom->interleaveStep;

			if (inheritSections & ALLOCMASK_IMPRINT) {
				assert(!(allocFlags & ALLOCMASK_IMPRINT));
				this->maxImprint = pFrom->maxImprint;
				this->numImprint = pFrom->numImprint;
				this->imprints   = pFrom->imprints;
			}

			if (inheritSections & ALLOCMASK_IMPRINTINDEX) {
				assert(!(allocFlags & ALLOCMASK_IMPRINTINDEX));
				this->imprintIndexSize = pFrom->imprintIndexSize;
				this->imprintIndex     = pFrom->imprintIndex;
			}
		}

		// sid/tid store
		if (inheritSections & (ALLOCMASK_PAIR | ALLOCMASK_PAIRINDEX)) {
			if (pFrom->numPair == 0)
				ctx.fatal("\n{\"error\":\"Missing sid/tid section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_PAIR) {
				assert(!(allocFlags & ALLOCMASK_PAIR));
				this->maxPair = pFrom->maxPair;
				this->numPair = pFrom->numPair;
				this->pairs   = pFrom->pairs;
			}

			if (inheritSections & ALLOCMASK_PAIRINDEX) {
				assert(!(allocFlags & ALLOCMASK_PAIRINDEX));
				this->pairIndexSize = pFrom->pairIndexSize;
				this->pairIndex     = pFrom->pairIndex;
			}
		}

		// member store
		if (inheritSections & (ALLOCMASK_MEMBER | ALLOCMASK_MEMBERINDEX)) {
			if (pFrom->numMember == 0)
				ctx.fatal("\n{\"error\":\"Missing member section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_MEMBER) {
				assert(!(allocFlags & ALLOCMASK_MEMBER));
				this->maxMember = pFrom->maxMember;
				this->numMember = pFrom->numMember;
				this->members   = pFrom->members;
			}

			if (inheritSections & ALLOCMASK_MEMBERINDEX) {
				assert(!(allocFlags & ALLOCMASK_MEMBERINDEX));
				this->memberIndexSize = pFrom->memberIndexSize;
				this->memberIndex     = pFrom->memberIndex;
			}
		}

		// patternFirst store
		if (inheritSections & (ALLOCMASK_PATTERNFIRST | ALLOCMASK_PATTERNFIRSTINDEX)) {
			if (pFrom->numPatternFirst == 0)
				ctx.fatal("\n{\"error\":\"Missing patternFirst section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_PATTERNFIRST) {
				assert(!(allocFlags & ALLOCMASK_PATTERNFIRST));
				this->maxPatternFirst = pFrom->maxPatternFirst;
				this->numPatternFirst = pFrom->numPatternFirst;
				this->patternsFirst   = pFrom->patternsFirst;
			}

			if (inheritSections & ALLOCMASK_PATTERNFIRSTINDEX) {
				assert(!(allocFlags & ALLOCMASK_PATTERNFIRSTINDEX));
				this->patternFirstIndexSize = pFrom->patternFirstIndexSize;
				this->patternFirstIndex     = pFrom->patternFirstIndex;
			}
		}

		// patternSecond store
		if (inheritSections & (ALLOCMASK_PATTERNSECOND | ALLOCMASK_PATTERNSECONDINDEX)) {
			if (pFrom->numPatternSecond == 0)
				ctx.fatal("\n{\"error\":\"Missing patternSecond section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_PATTERNSECOND) {
				assert(!(allocFlags & ALLOCMASK_PATTERNSECOND));
				this->maxPatternSecond = pFrom->maxPatternSecond;
				this->numPatternSecond = pFrom->numPatternSecond;
				this->patternsSecond   = pFrom->patternsSecond;
			}

			if (inheritSections & ALLOCMASK_PATTERNSECONDINDEX) {
				assert(!(allocFlags & ALLOCMASK_PATTERNSECONDINDEX));
				this->patternSecondIndexSize = pFrom->patternSecondIndexSize;
				this->patternSecondIndex     = pFrom->patternSecondIndex;
			}
		}
	}

	/**
	 * @date 2020-04-21 14:31:14
	 *
	 * Determine how much memory `create()` would require
	 *
  	 * @param {number} excludeSections - set of sections to exclude from allocating
	 * @return {number} Required memory
	 */
	size_t estimateMemoryUsage(unsigned excludeSections) const {

		size_t memUsage = 0;

		// transform store
		if (maxTransform && !(excludeSections & ALLOCMASK_TRANSFORM)) {
			memUsage += maxTransform * sizeof(*this->fwdTransformData);
			memUsage += maxTransform * sizeof(*this->revTransformData);
			memUsage += maxTransform * sizeof(*this->fwdTransformNames);
			memUsage += maxTransform * sizeof(*this->revTransformNames);
			memUsage += maxTransform * sizeof(*this->revTransformIds);
			memUsage += transformIndexSize * sizeof(*fwdTransformNameIndex);
			memUsage += transformIndexSize * sizeof(*revTransformNameIndex);
		}

		// evaluator store [COPY-ON-WRITE]
		if (maxEvaluator && !(excludeSections & ALLOCMASK_EVALUATOR)) {
			memUsage += maxEvaluator * sizeof(*this->fwdEvaluator);
			memUsage += maxEvaluator * sizeof(*this->revEvaluator);
		}

		// signature store
		if (maxSignature && !(excludeSections & ALLOCMASK_SIGNATURE))
			memUsage += maxSignature * sizeof(*signatures); // increase with 5%
		if (signatureIndexSize && !(excludeSections & ALLOCMASK_SIGNATUREINDEX))
			memUsage += signatureIndexSize * sizeof(*signatureIndex);

		// swap store
		if (maxSwap && !(excludeSections & ALLOCMASK_SWAP))
			memUsage += maxSwap * sizeof(*swaps); // increase with 5%
		if (swapIndexSize && !(excludeSections & ALLOCMASK_SWAPINDEX))
			memUsage += swapIndexSize * sizeof(*swapIndex);

		// imprint store
		if (maxImprint && !(excludeSections & ALLOCMASK_IMPRINT))
			memUsage += maxImprint * sizeof(*imprints); // increase with 5%
		if (imprintIndexSize && !(excludeSections & ALLOCMASK_IMPRINTINDEX))
			memUsage += imprintIndexSize * sizeof(*imprintIndex);

		// sid/tid store
		if (maxPair && !(excludeSections & ALLOCMASK_PAIR))
			memUsage += maxPair * sizeof(*pairs); // increase with 5%
		if (pairIndexSize && !(excludeSections & ALLOCMASK_PAIRINDEX))
			memUsage += pairIndexSize * sizeof(*pairIndex);

		// member store
		if (maxMember && !(excludeSections & ALLOCMASK_MEMBER))
			memUsage += maxMember * sizeof(*members); // increase with 5%
		if (memberIndexSize && !(excludeSections & ALLOCMASK_MEMBERINDEX))
			memUsage += memberIndexSize * sizeof(*memberIndex);

		// patternFirst store
		if (maxPatternFirst && !(excludeSections & ALLOCMASK_PATTERNFIRST))
			memUsage += maxPatternFirst * sizeof(*patternsFirst); // increase with 5%
		if (patternFirstIndexSize && !(excludeSections & ALLOCMASK_PATTERNFIRSTINDEX))
			memUsage += patternFirstIndexSize * sizeof(*patternFirstIndex);

		// patternSecond store
		if (maxPatternSecond && !(excludeSections & ALLOCMASK_PATTERNSECOND))
			memUsage += maxPatternSecond * sizeof(*patternsSecond); // increase with 5%
		if (patternSecondIndexSize && !(excludeSections & ALLOCMASK_PATTERNSECONDINDEX))
			memUsage += patternSecondIndexSize * sizeof(*patternSecondIndex);

		return memUsage;
	};

	/**
	 * @date 2020-03-12 16:05:37
	 *
	 * Create read-write database as memory store
	 *
  	 * @param {number} excludeSections - set of sections to exclude from allocating
	 */
	void create(unsigned excludeSections) {
		// copy user flags+debug settings
		creationFlags = ctx.flags;

		// transform store
		if (maxTransform && !(excludeSections & ALLOCMASK_TRANSFORM)) {
			assert(maxTransform == MAXTRANSFORM);
			fwdTransformData      = (uint64_t *) ctx.myAlloc("database_t::fwdTransformData", maxTransform, sizeof(*this->fwdTransformData));
			revTransformData      = (uint64_t *) ctx.myAlloc("database_t::revTransformData", maxTransform, sizeof(*this->revTransformData));
			fwdTransformNames     = (transformName_t *) ctx.myAlloc("database_t::fwdTransformNames", maxTransform, sizeof(*this->fwdTransformNames));
			revTransformNames     = (transformName_t *) ctx.myAlloc("database_t::revTransformNames", maxTransform, sizeof(*this->revTransformNames));
			revTransformIds       = (uint32_t *) ctx.myAlloc("database_t::revTransformIds", maxTransform, sizeof(*this->revTransformIds));
			fwdTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::fwdTransformNameIndex", transformIndexSize, sizeof(*fwdTransformNameIndex));
			revTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::revTransformNameIndex", transformIndexSize, sizeof(*revTransformNameIndex));
			allocFlags |= ALLOCMASK_TRANSFORM;
		}

		// evaluator store [COPY-ON-WRITE]
		if (maxEvaluator && !(excludeSections & ALLOCMASK_EVALUATOR)) {
			assert(maxTransform == MAXTRANSFORM);
			assert(maxEvaluator == tinyTree_t::TINYTREE_NEND * maxTransform);
			fwdEvaluator = (footprint_t *) ctx.myAlloc("database_t::fwdEvaluator", maxEvaluator, sizeof(*this->fwdEvaluator));
			revEvaluator = (footprint_t *) ctx.myAlloc("database_t::revEvaluator", maxEvaluator, sizeof(*this->revEvaluator));
		}

		// signature store
		if (maxSignature && !(excludeSections & ALLOCMASK_SIGNATURE)) {
			// increase with 5%
			numSignature = IDFIRST;
			signatures   = (signature_t *) ctx.myAlloc("database_t::signatures", maxSignature, sizeof(*signatures));
			allocFlags |= ALLOCMASK_SIGNATURE;
		}
		if (signatureIndexSize && !(excludeSections & ALLOCMASK_SIGNATUREINDEX)) {
			assert(ctx.isPrime(signatureIndexSize));
			signatureIndex = (uint32_t *) ctx.myAlloc("database_t::signatureIndex", signatureIndexSize, sizeof(*signatureIndex));
			allocFlags |= ALLOCMASK_SIGNATUREINDEX;
		}

		// swap store
		if (maxSwap && !(excludeSections & ALLOCMASK_SWAP)) {
			// increase with 5%
			numSwap = IDFIRST;
			swaps   = (swap_t *) ctx.myAlloc("database_t::swaps", maxSwap, sizeof(*swaps));
			allocFlags |= ALLOCMASK_SWAP;
		}
		if (swapIndexSize && !(excludeSections & ALLOCMASK_SWAPINDEX)) {
			assert(ctx.isPrime(swapIndexSize));
			swapIndex = (uint32_t *) ctx.myAlloc("database_t::swapIndex", swapIndexSize, sizeof(*swapIndex));
			allocFlags |= ALLOCMASK_SWAPINDEX;
		}

		// imprint store
		if (maxImprint && !(excludeSections & ALLOCMASK_IMPRINT)) {
			assert(interleave && interleaveStep);
			// increase with 5%
			numImprint = IDFIRST;
			imprints   = (imprint_t *) ctx.myAlloc("database_t::imprints", maxImprint, sizeof(*imprints));
			allocFlags |= ALLOCMASK_IMPRINT;
		}
		if (imprintIndexSize && !(excludeSections & ALLOCMASK_IMPRINTINDEX)) {
			assert(ctx.isPrime(imprintIndexSize));
			imprintIndex = (uint32_t *) ctx.myAlloc("database_t::imprintIndex", imprintIndexSize, sizeof(*imprintIndex));
			allocFlags |= ALLOCMASK_IMPRINTINDEX;
		}

		// sid/tid store
		if (maxPair && !(excludeSections & ALLOCMASK_PAIR)) {
			// increase with 5%
			numPair = IDFIRST;
			pairs   = (pair_t *) ctx.myAlloc("database_t::pairs", maxPair, sizeof(*pairs));
			allocFlags |= ALLOCMASK_PAIR;
		}
		if (pairIndexSize && !(excludeSections & ALLOCMASK_PAIRINDEX)) {
			assert(ctx.isPrime(pairIndexSize));
			pairIndex = (uint32_t *) ctx.myAlloc("database_t::pairIndex", pairIndexSize, sizeof(*pairIndex));
			allocFlags |= ALLOCMASK_PAIRINDEX;
		}

		// member store
		if (maxMember && !(excludeSections & ALLOCMASK_MEMBER)) {
			// increase with 5%
			numMember = IDFIRST;
			members   = (member_t *) ctx.myAlloc("database_t::members", maxMember, sizeof(*members));
			allocFlags |= ALLOCMASK_MEMBER;
		}
		if (memberIndexSize && !(excludeSections & ALLOCMASK_MEMBERINDEX)) {
			assert(ctx.isPrime(memberIndexSize));
			memberIndex = (uint32_t *) ctx.myAlloc("database_t::memberIndex", memberIndexSize, sizeof(*memberIndex));
			allocFlags |= ALLOCMASK_MEMBERINDEX;
		}

		// patternFirst store
		if (maxPatternFirst && !(excludeSections & ALLOCMASK_PATTERNFIRST)) {
			// increase with 5%
			numPatternFirst = IDFIRST;
			patternsFirst   = (patternFirst_t *) ctx.myAlloc("database_t::patternFirst", maxPatternFirst, sizeof(*patternsFirst));
			allocFlags |= ALLOCMASK_PATTERNFIRST;
		}
		if (patternFirstIndexSize && !(excludeSections & ALLOCMASK_PATTERNFIRSTINDEX)) {
			assert(ctx.isPrime(patternFirstIndexSize));
			patternFirstIndex = (uint32_t *) ctx.myAlloc("database_t::patternFirstIndex", patternFirstIndexSize, sizeof(*patternFirstIndex));
			allocFlags |= ALLOCMASK_PATTERNFIRSTINDEX;
		}

		// patternSecond store
		if (maxPatternSecond && !(excludeSections & ALLOCMASK_PATTERNSECOND)) {
			// increase with 5%
			numPatternSecond = IDFIRST;
			patternsSecond   = (patternSecond_t *) ctx.myAlloc("database_t::patternSecond", maxPatternSecond, sizeof(*patternsSecond));
			allocFlags |= ALLOCMASK_PATTERNSECOND;
		}
		if (patternSecondIndexSize && !(excludeSections & ALLOCMASK_PATTERNSECONDINDEX)) {
			assert(ctx.isPrime(patternSecondIndexSize));
			patternSecondIndex = (uint32_t *) ctx.myAlloc("database_t::patternSecondIndex", patternSecondIndexSize, sizeof(*patternSecondIndex));
			allocFlags |= ALLOCMASK_PATTERNSECONDINDEX;
		}

		/*
		 * @date 2021-07-23 22:49:21
		 * Index really needs to be larger than number of records
		 * index must be larger than maximum + 1%. Formulate such to avoid integer overflow occurs
		 */
		assert(this->signatureIndexSize - this->maxSignature / 100 >= this->maxSignature);
		assert(this->swapIndexSize - this->maxSwap / 100 >= this->maxSwap);
		assert(this->imprintIndexSize - this->maxImprint / 100 >= this->maxImprint);
		assert(this->pairIndexSize - this->maxPair / 100 >= this->maxPair);
		assert(this->memberIndexSize - this->maxMember / 100 >= this->maxMember);
		assert(this->patternFirstIndexSize - this->maxPatternFirst / 100 >= this->maxPatternFirst);
		assert(this->patternSecondIndexSize - this->maxPatternSecond / 100 >= this->maxPatternSecond);
	};

	/**
	 * @date 2020-03-12 16:07:44
	 *
	 * Create read-only database mmapped onto file
	 *
	 * @date 2020-04-27 19:52:27
	 *
	 * To reduce need to copy large chunks of data from input to output, make pages writable and enable copy-on-write
	 *
         * @param {string} fileName - database filename
         * @param {boolean} writable - Set access to R/W
	 */
	void open(const char *fileName) {

		/*
		 * Open file
		 */
		hndl = ::open(fileName, O_RDONLY);
		if (hndl == -1)
			ctx.fatal("\n{\"error\":\"fopen('%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);

		struct stat sbuf;
		if (::fstat(hndl, &sbuf))
			ctx.fatal("\n{\"error\":\"fstat('%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);
		
		/*
		 * @date 2021-10-23 23:55:11
		 * Remember file size as that is used for `mmap()`/`munmap()`.
		 */
		this->fileSize = sbuf.st_size;

#if defined(HAVE_MMAP)
		/*
		 * Load using mmap() and enable copy-on-write
		 */
		void *pMemory = ::mmap(NULL, (size_t) this->fileSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE, hndl, 0);
		if (pMemory == MAP_FAILED)
			ctx.fatal("\n{\"error\":\"mmap(PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_NORESERVE,'%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);

		// set memory usage preferances
		if (::madvise(pMemory, (size_t) this->fileSize, MADV_RANDOM))
			ctx.fatal("\n{\"error\":\"madvise(MADV_RANDOM,'%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);
		if (::madvise(pMemory, (size_t) this->fileSize, MADV_DONTDUMP))
			ctx.fatal("\n{\"error\":\"madvise(MADV_DONTDUMP,'%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);

		rawData = (const uint8_t *) pMemory;
#else
		/*
		 * Load using read()
		 */

		/*
		 * Allocate storage
		 */
		rawData = (uint8_t *) ctx.myAlloc("database_t::rawData", 1, this->fileSize);

		ctx.progressHi = (uint64_t) this->fileSize;
		ctx.progress = 0;

		readData(hndl, (uint8_t *) rawData, this->fileSize);

		/*
		 * Close
		 */
		::close(hndl);
		hndl = 0;
#endif

		::memcpy(&fileHeader, rawData, sizeof(fileHeader));
		if (fileHeader.magic != FILE_MAGIC)
			ctx.fatal("\n{\"error\":\"db version mismatch\",\"where\":\"%s:%s:%d\",\"encountered\":\"%08x\",\"expected\":\"%08x\"}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic, FILE_MAGIC);
		if (fileHeader.magic_maxSlots != MAXSLOTS)
			ctx.fatal("\n{\"error\":\"db magic_maxslots\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_maxSlots, MAXSLOTS);
		if (fileHeader.offEnd != this->fileSize)
			ctx.fatal("\n{\"error\":\"db size mismatch\",\"where\":\"%s:%s:%d\",\"encountered\":\"%lu\",\"expected\":\"%lu\"}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.offEnd, this->fileSize);
		if (fileHeader.magic_sizeofSignature != sizeof(signature_t) && fileHeader.numSignature > 0)
			ctx.fatal("\n{\"error\":\"db magic_sizeofSignature\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofSignature, (unsigned) sizeof(signature_t));
		if (fileHeader.magic_sizeofSwap != sizeof(swap_t) && fileHeader.numSwap > 0)
			ctx.fatal("\n{\"error\":\"db magic_sizeofSwap\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofSwap, (unsigned) sizeof(swap_t));
		if (fileHeader.magic_sizeofImprint != sizeof(imprint_t) && fileHeader.numImprint > 0)
			ctx.fatal("\n{\"error\":\"db magic_sizeofImprint\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofImprint, (unsigned) sizeof(imprint_t));
		if (fileHeader.magic_sizeofPair != sizeof(pair_t) && fileHeader.numPair > 0)
			ctx.fatal("\n{\"error\":\"db magic_sizeofPair\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofPair, (unsigned) sizeof(pair_t));
		if (fileHeader.magic_sizeofMember != sizeof(member_t) && fileHeader.numMember > 0)
			ctx.fatal("\n{\"error\":\"db magic_sizeofMember\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofMember, (unsigned) sizeof(member_t));
		if (fileHeader.magic_sizeofPatternFirst != sizeof(patternFirst_t) && fileHeader.numPatternFirst > 0)
			ctx.fatal("\n{\"error\":\"db magic_sizeofPatternFirst\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofPatternFirst, (unsigned) sizeof(patternFirst_t));
		if (fileHeader.magic_sizeofPatternSecond != sizeof(patternSecond_t) && fileHeader.numPatternSecond > 0)
			ctx.fatal("\n{\"error\":\"db magic_sizeofPatternSecond\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofPatternSecond, (unsigned) sizeof(patternSecond_t));

		creationFlags = fileHeader.magic_flags;

		/*
		 * map sections to starting positions in data
		 */

		// transforms
		maxTransform          = fileHeader.numTransform;
		numTransform          = fileHeader.numTransform;
		fwdTransformData      = (uint64_t *) (rawData + fileHeader.offFwdTransforms);
		revTransformData      = (uint64_t *) (rawData + fileHeader.offRevTransforms);
		fwdTransformNames     = (transformName_t *) (rawData + fileHeader.offFwdTransformNames);
		revTransformNames     = (transformName_t *) (rawData + fileHeader.offRevTransformNames);
		revTransformIds       = (uint32_t *) (rawData + fileHeader.offRevTransformIds);
		transformIndexSize    = fileHeader.transformIndexSize;
		fwdTransformNameIndex = (uint32_t *) (rawData + fileHeader.offFwdTransformNameIndex);
		revTransformNameIndex = (uint32_t *) (rawData + fileHeader.offRevTransformNameIndex);

		// evaluator store [COPY-ON-WRITE]
		maxEvaluator = fileHeader.numEvaluator;
		numEvaluator = fileHeader.numEvaluator;
		fwdEvaluator = (footprint_t *) (rawData + fileHeader.offFwdEvaluator);
		revEvaluator = (footprint_t *) (rawData + fileHeader.offRevEvaluator);

		// signatures
		maxSignature       = fileHeader.numSignature;
		numSignature       = fileHeader.numSignature;
		signatures         = (signature_t *) (rawData + fileHeader.offSignatures);
		signatureIndexSize = fileHeader.signatureIndexSize;
		signatureIndex     = (uint32_t *) (rawData + fileHeader.offSignatureIndex);

		// swap
		maxSwap       = fileHeader.numSwap;
		numSwap       = fileHeader.numSwap;
		swaps         = (swap_t *) (rawData + fileHeader.offSwaps);
		swapIndexSize = fileHeader.swapIndexSize;
		swapIndex     = (uint32_t *) (rawData + fileHeader.offSwapIndex);

		// imprints
		interleave       = fileHeader.interleave;
		interleaveStep   = fileHeader.interleaveStep;
		maxImprint       = numImprint = fileHeader.numImprint;
		imprints         = (imprint_t *) (rawData + fileHeader.offImprints);
		imprintIndexSize = fileHeader.imprintIndexSize;
		imprintIndex     = (uint32_t *) (rawData + fileHeader.offImprintIndex);

		// sid/tid
		maxPair       = fileHeader.numPair;
		numPair       = fileHeader.numPair;
		pairs         = (pair_t *) (rawData + fileHeader.offpairs);
		pairIndexSize = fileHeader.pairIndexSize;
		pairIndex     = (uint32_t *) (rawData + fileHeader.offPairIndex);

		// members
		maxMember       = fileHeader.numMember;
		numMember       = fileHeader.numMember;
		members         = (member_t *) (rawData + fileHeader.offMember);
		memberIndexSize = fileHeader.memberIndexSize;
		memberIndex     = (uint32_t *) (rawData + fileHeader.offMemberIndex);

		// patternFirst
		maxPatternFirst       = fileHeader.numPatternFirst;
		numPatternFirst       = fileHeader.numPatternFirst;
		patternsFirst         = (patternFirst_t *) (rawData + fileHeader.offPatternFirst);
		patternFirstIndexSize = fileHeader.patternFirstIndexSize;
		patternFirstIndex     = (uint32_t *) (rawData + fileHeader.offPatternFirstIndex);

		// patternSecond
		maxPatternSecond       = fileHeader.numPatternSecond;
		numPatternSecond       = fileHeader.numPatternSecond;
		patternsSecond         = (patternSecond_t *) (rawData + fileHeader.offPatternSecond);
		patternSecondIndexSize = fileHeader.patternSecondIndexSize;
		patternSecondIndex     = (uint32_t *) (rawData + fileHeader.offPatternSecondIndex);


		// lookup 1n9 sids
		for (uint32_t iSid = IDFIRST; iSid < IDFIRST + 10; iSid++) {
			const signature_t *pSignature = this->signatures + iSid;

			if (strcmp(pSignature->name, "0") == 0)
				this->SID_ZERO = iSid;
			else if (strcmp(pSignature->name, "a") == 0)
				this->SID_SELF = iSid;
			else if (strcmp(pSignature->name, "ab+") == 0)
				this->SID_OR = iSid;
			else if (strcmp(pSignature->name, "ab>") == 0)
				this->SID_GT = iSid;
			else if (strcmp(pSignature->name, "ab^") == 0)
				this->SID_NE = iSid;
			else if (strcmp(pSignature->name, "ab&") == 0)
				this->SID_AND = iSid;
			else if (strcmp(pSignature->name, "abc!") == 0)
				this->SID_QNTF = iSid;
			else if (strcmp(pSignature->name, "abc?") == 0)
				this->SID_QTF = iSid;

		}

		if (numSignature > IDFIRST) {
			// test they are available
			if (!this->SID_ZERO || !this->SID_SELF || !this->SID_OR || !this->SID_GT || !this->SID_NE || !this->SID_QNTF)
				fprintf(stderr, "[%s] WARNING: Database missing 1n9 sids\n", ctx.timeAsString());
		}
	};

	/**
	 * @date 2021-10-18 20:48:45
	 *
	 * (re-allocate) sections to enable growth
	 * NOTE: changed sections are zeroed, previous contents is injected
	 *
	 * @param {number} sections - set of sections to process
	 */
	void reallocateSections(unsigned sections) {
		// transform store
		if (sections & ALLOCMASK_TRANSFORM) {
			assert(maxTransform > 0);
			fwdTransformData      = (uint64_t *) ctx.myAlloc("database_t::fwdTransformData", maxTransform, sizeof(*this->fwdTransformData));
			revTransformData      = (uint64_t *) ctx.myAlloc("database_t::revTransformData", maxTransform, sizeof(*this->revTransformData));
			fwdTransformNames     = (transformName_t *) ctx.myAlloc("database_t::fwdTransformNames", maxTransform, sizeof(*this->fwdTransformNames));
			revTransformNames     = (transformName_t *) ctx.myAlloc("database_t::revTransformNames", maxTransform, sizeof(*this->revTransformNames));
			revTransformIds       = (uint32_t *) ctx.myAlloc("database_t::revTransformIds", maxTransform, sizeof(*this->revTransformIds));
			assert(transformIndexSize > 0);
			fwdTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::fwdTransformNameIndex", transformIndexSize, sizeof(*fwdTransformNameIndex));
			revTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::revTransformNameIndex", transformIndexSize, sizeof(*revTransformNameIndex));
			allocFlags |= ALLOCMASK_TRANSFORM;
		}

		// evaluator store [COPY-ON-WRITE]
		if (sections & ALLOCMASK_EVALUATOR) {
			assert(maxTransform == MAXTRANSFORM);
			assert(maxEvaluator == tinyTree_t::TINYTREE_NEND * maxTransform);
			fwdEvaluator = (footprint_t *) ctx.myAlloc("database_t::fwdEvaluator", maxEvaluator, sizeof(*this->fwdEvaluator));
			revEvaluator = (footprint_t *) ctx.myAlloc("database_t::revEvaluator", maxEvaluator, sizeof(*this->revEvaluator));
		}

		// signature store
		if (sections & ALLOCMASK_SIGNATURE) {
			assert(maxSignature && numSignature <= maxSignature);
			signature_t *origData = signatures;
			signatures = (signature_t *) ctx.myAlloc("database_t::signatures", maxSignature, sizeof(*signatures));
			allocFlags |= ALLOCMASK_SIGNATURE;
			if (numSignature > 0)
				memcpy(signatures, origData, numSignature * sizeof(*signatures));
		}
		if (sections & ALLOCMASK_SIGNATUREINDEX) {
			assert(signatureIndexSize && ctx.isPrime(signatureIndexSize));
			signatureIndex = (uint32_t *) ctx.myAlloc("database_t::signatureIndex", signatureIndexSize, sizeof(*signatureIndex));
			allocFlags |= ALLOCMASK_SIGNATUREINDEX;
		}

		// swap store
		if (sections & ALLOCMASK_SWAP) {
			assert(maxSwap && numSwap <= maxSwap);
			swap_t *origData = swaps;
			swaps = (swap_t *) ctx.myAlloc("database_t::swaps", maxSwap, sizeof(*swaps));
			allocFlags |= ALLOCMASK_SWAP;
			if (numSwap > 0)
				memcpy(swaps, origData, numSwap * sizeof(*swaps));
		}
		if (sections & ALLOCMASK_SWAPINDEX) {
			assert(swapIndexSize && ctx.isPrime(swapIndexSize));
			swapIndex = (uint32_t *) ctx.myAlloc("database_t::swapIndex", swapIndexSize, sizeof(*swapIndex));
			allocFlags |= ALLOCMASK_SWAPINDEX;
		}

		// imprint store
		if (sections & ALLOCMASK_IMPRINT) {
			assert(maxImprint && numImprint <= maxImprint);
			imprint_t *origData = imprints;
			imprints = (imprint_t *) ctx.myAlloc("database_t::imprints", maxImprint, sizeof(*imprints));
			allocFlags |= ALLOCMASK_IMPRINT;
			if (numImprint > 0)
				memcpy(imprints, origData, numImprint * sizeof(*imprints));
		}
		if (sections & ALLOCMASK_IMPRINTINDEX) {
			assert(imprintIndexSize && ctx.isPrime(imprintIndexSize));
			imprintIndex = (uint32_t *) ctx.myAlloc("database_t::imprintIndex", imprintIndexSize, sizeof(*imprintIndex));
			allocFlags |= ALLOCMASK_IMPRINTINDEX;
		}

		// sid/tid store
		if (sections & ALLOCMASK_PAIR) {
			assert(maxPair && numPair < maxPair);
			pair_t *origData = pairs;
			pairs = (pair_t *) ctx.myAlloc("database_t::pairs", maxPair, sizeof(*pairs));
			allocFlags |= ALLOCMASK_PAIR;
			if (numPair > 0)
				memcpy(pairs, origData, numPair * sizeof(*pairs));
		}
		if (sections & ALLOCMASK_PAIRINDEX) {
			assert(pairIndexSize && ctx.isPrime(pairIndexSize));
			pairIndex = (uint32_t *) ctx.myAlloc("database_t::pairIndex", pairIndexSize, sizeof(*pairIndex));
			allocFlags |= ALLOCMASK_PAIRINDEX;
		}

		// member store
		if (sections & ALLOCMASK_MEMBER) {
			assert(maxMember && numMember <= maxMember);
			member_t *origData = members;
			members = (member_t *) ctx.myAlloc("database_t::members", maxMember, sizeof(*members));
			allocFlags |= ALLOCMASK_MEMBER;
			if (numMember > 0)
				memcpy(members, origData, numMember * sizeof(*members));
		}
		if (sections & ALLOCMASK_MEMBERINDEX) {
			assert(memberIndexSize && ctx.isPrime(memberIndexSize));
			memberIndex = (uint32_t *) ctx.myAlloc("database_t::memberIndex", memberIndexSize, sizeof(*memberIndex));
			allocFlags |= ALLOCMASK_MEMBERINDEX;
		}

		// patternFirst store
		if (sections & ALLOCMASK_PATTERNFIRST) {
			assert(maxPatternFirst && numPatternFirst <= maxPatternFirst);
			patternFirst_t *origData = patternsFirst;
			patternsFirst = (patternFirst_t *) ctx.myAlloc("database_t::patternsFirst", maxPatternFirst, sizeof(*patternsFirst));
			allocFlags |= ALLOCMASK_PATTERNFIRST;
			if (numPatternFirst > 0)
				memcpy(patternsFirst, origData, numPatternFirst * sizeof(*patternsFirst));
		}
		if (sections & ALLOCMASK_PATTERNFIRSTINDEX) {
			assert(patternFirstIndexSize && ctx.isPrime(patternFirstIndexSize));
			patternFirstIndex = (uint32_t *) ctx.myAlloc("database_t::patternFirstIndex", patternFirstIndexSize, sizeof(*patternFirstIndex));
			allocFlags |= ALLOCMASK_PATTERNFIRSTINDEX;
		}

		// patternSecond store
		if (sections & ALLOCMASK_PATTERNSECOND) {
			assert(maxPatternSecond && numPatternSecond <= maxPatternSecond);
			patternSecond_t *origData = patternsSecond;
			patternsSecond = (patternSecond_t *) ctx.myAlloc("database_t::patternsSecond", maxPatternSecond, sizeof(*patternsSecond));
			allocFlags |= ALLOCMASK_PATTERNSECOND;
			if (numPatternSecond > 0)
				memcpy(patternsSecond, origData, numPatternSecond * sizeof(*patternsSecond));
		}
		if (sections & ALLOCMASK_PATTERNSECONDINDEX) {
			assert(patternSecondIndexSize && ctx.isPrime(patternSecondIndexSize));
			patternSecondIndex = (uint32_t *) ctx.myAlloc("database_t::patternSecondIndex", patternSecondIndexSize, sizeof(*patternSecondIndex));
			allocFlags |= ALLOCMASK_PATTERNSECONDINDEX;
		}

		/*
		 * @date 2021-07-23 22:49:21
		 * Index really needs to be larger than number of records
		 * index must be larger than maximum + 1%. Formulate such to avoid integer overflow occurs
		 */
		assert(!(sections & ALLOCMASK_SIGNATURE) || this->signatureIndexSize - this->maxSignature / 100 >= this->maxSignature);
		assert(!(sections & ALLOCMASK_SWAP) || this->swapIndexSize - this->maxSwap / 100 >= this->maxSwap);
		assert(!(sections & ALLOCMASK_IMPRINT) || this->imprintIndexSize - this->maxImprint / 100 >= this->maxImprint);
		assert(!(sections & ALLOCMASK_PAIR) || this->pairIndexSize - this->maxPair / 100 >= this->maxPair);
		assert(!(sections & ALLOCMASK_MEMBER) || this->memberIndexSize - this->maxMember / 100 >= this->maxMember);
		assert(!(sections & ALLOCMASK_PATTERNFIRST) || this->patternFirstIndexSize - this->maxPatternFirst / 100 >= this->maxPatternFirst);
		assert(!(sections & ALLOCMASK_PATTERNSECOND) || this->patternSecondIndexSize - this->maxPatternSecond / 100 >= this->maxPatternSecond);
	}

	/**
	 * @date 2020-04-16 20:41:47
	 *
	 * Raise size to next 32 byte alignment
	 *
	 * @param {number} size - amount to round up
	 *
	 * @return {number} rounded size.
	 */
	inline size_t align32(size_t size) {

		// test if already aligned
		if ((size & ~31ULL) == 0)
			return size;

		// return aligned size
		return (size + 32) & ~31ULL;
	}

	/**
	 * @date 2020-03-12 16:04:30
	 *
	 * Write database to file
	 *
	 * @param {string} fileName - File to write to
	 */
	void save(const char *fileName) {

		::memset(&fileHeader, 0, sizeof(fileHeader));

		/*
		 * Evaluators are dirty and need to be re-created (sanitised) before writing
		 */
		if (this->numEvaluator)
			initialiseEvaluators();

		/*
		 * Quick calculate file size
		 */
		ctx.progressHi = align32(sizeof(fileHeader));
		ctx.progressHi += align32(sizeof(*this->fwdTransformData) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->revTransformData) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->fwdTransformNames) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->revTransformNames) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->revTransformIds) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->fwdTransformNameIndex * this->transformIndexSize));
		ctx.progressHi += align32(sizeof(*this->revTransformNameIndex * this->transformIndexSize));
		ctx.progressHi += align32(sizeof(*this->fwdEvaluator) * this->numEvaluator);
		ctx.progressHi += align32(sizeof(*this->revEvaluator) * this->numEvaluator);
		ctx.progressHi += align32(sizeof(*this->signatures) * this->numSignature);
		ctx.progressHi += align32(sizeof(*this->signatureIndex) * this->signatureIndexSize);
		ctx.progressHi += align32(sizeof(*this->swaps) * this->numSwap);
		ctx.progressHi += align32(sizeof(*this->swapIndex) * this->swapIndexSize);
		ctx.progressHi += align32(sizeof(*this->imprints) * this->numImprint);
		ctx.progressHi += align32(sizeof(*this->imprintIndex) * this->imprintIndexSize);
		ctx.progressHi += align32(sizeof(*this->pairs) * this->numPair);
		ctx.progressHi += align32(sizeof(*this->pairIndex) * this->pairIndexSize);
		ctx.progressHi += align32(sizeof(*this->members) * this->numMember);
		ctx.progressHi += align32(sizeof(*this->memberIndex) * this->memberIndexSize);
		ctx.progressHi += align32(sizeof(*this->patternsFirst) * this->numPatternFirst);
		ctx.progressHi += align32(sizeof(*this->patternFirstIndex) * this->patternFirstIndexSize);
		ctx.progressHi += align32(sizeof(*this->patternsSecond) * this->numPatternSecond);
		ctx.progressHi += align32(sizeof(*this->patternSecondIndex) * this->patternSecondIndexSize);
		ctx.progress   = 0;
		ctx.tick       = 0;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Writing %s\n", ctx.timeAsString(), fileName);

		/*
		 * Open output file
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[Kopening");

		FILE *outf = fopen(fileName, "w");
		if (!outf)
			ctx.fatal("\n{\"error\":\"fopen('w','%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);

		/*
		 * Write empty header (overwritten later)
		 */
		uint64_t flen = 0;

		flen += writeData(outf, &fileHeader, sizeof(fileHeader), fileName, "header");

		/*
		 * write transforms
		 */
		if (this->numTransform) {
			fileHeader.numTransform = this->numTransform;

			// write forward/reverse transforms
			fileHeader.offFwdTransforms = flen;
			flen += writeData(outf, this->fwdTransformData, sizeof(*this->fwdTransformData) * this->numTransform, fileName, "transform");
			fileHeader.offRevTransforms = flen;
			flen += writeData(outf, this->revTransformData, sizeof(*this->revTransformData) * this->numTransform, fileName, "transform");

			// write forward/reverse names
			fileHeader.offFwdTransformNames = flen;
			flen += writeData(outf, this->fwdTransformNames, sizeof(*this->fwdTransformNames) * this->numTransform, fileName, "transform");
			fileHeader.offRevTransformNames = flen;
			flen += writeData(outf, this->revTransformNames, sizeof(*this->revTransformNames) * this->numTransform, fileName, "transform");

			// write inverted skins
			fileHeader.offRevTransformIds = flen;
			flen += writeData(outf, this->revTransformIds, sizeof(*this->revTransformIds) * this->numTransform, fileName, "transform");

			// write index
			if (transformIndexSize) {
				fileHeader.transformIndexSize = this->transformIndexSize;

				// write index
				fileHeader.offFwdTransformNameIndex = flen;
				flen += writeData(outf, this->fwdTransformNameIndex, sizeof(*this->fwdTransformNameIndex) * this->transformIndexSize, fileName, "transform");
				fileHeader.offRevTransformNameIndex = flen;
				flen += writeData(outf, this->revTransformNameIndex, sizeof(*this->revTransformNameIndex) * this->transformIndexSize, fileName, "transform");
			}
		}

		/*
		 * write evaluators [COPY-ON-WRITE]
		 */
		if (this->numEvaluator) {
			fileHeader.numEvaluator = this->numEvaluator;

			// write forward/reverse transforms
			fileHeader.offFwdEvaluator = flen;
			flen += writeData(outf, this->fwdEvaluator, sizeof(*this->fwdEvaluator) * this->numEvaluator, fileName, "evaluator");
			fileHeader.offRevEvaluator = flen;
			flen += writeData(outf, this->revEvaluator, sizeof(*this->revEvaluator) * this->numEvaluator, fileName, "evaluator");
		}

		/*
		 * write signatures
		 */
		uint32_t sidCRC = 0;
		if (this->numSignature) {
			// first entries must be zero
			signature_t zero;
			::memset(&zero, 0, sizeof(zero));
			for (uint32_t i = 0; i < IDFIRST; i++)
				assert(::memcmp(this->signatures + i, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numSignature  = this->numSignature;
			fileHeader.offSignatures = flen;
			flen += writeData(outf, this->signatures, sizeof(*this->signatures) * this->numSignature, fileName, "signature");
			if (this->signatureIndexSize) {
				// Index
				fileHeader.signatureIndexSize = this->signatureIndexSize;
				fileHeader.offSignatureIndex  = flen;
				flen += writeData(outf, this->signatureIndex, sizeof(*this->signatureIndex) * this->signatureIndexSize, fileName, "signatureIndex");
			}

			// calculate CRC of sid names
			for (uint32_t iSid = 1; iSid < this->numSignature; iSid++) {
				const signature_t *pSignature = this->signatures + iSid;

				for (const char *pName = pSignature->name; *pName; pName++)
					__asm__ __volatile__ ("crc32b %1, %0" : "+r"(sidCRC) : "rm"(*pName));
			}
		}

		/*
		 * write swaps
		 */
		if (this->numSwap) {
			// first entries must be zero
			swap_t zero;
			::memset(&zero, 0, sizeof(zero));
			for (uint32_t i = 0; i < IDFIRST; i++)
				assert(::memcmp(this->swaps + i, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numSwap  = this->numSwap;
			fileHeader.offSwaps = flen;
			flen += writeData(outf, this->swaps, sizeof(*this->swaps) * this->numSwap, fileName, "swap");
			if (this->swapIndexSize) {
				// Index
				fileHeader.swapIndexSize = this->swapIndexSize;
				fileHeader.offSwapIndex  = flen;
				flen += writeData(outf, this->swapIndex, sizeof(*this->swapIndex) * this->swapIndexSize, fileName, "swapIndex");
			}
		}

		/*
		 * write imprints
		 */
		if (this->numImprint) {
			fileHeader.interleave     = interleave;
			fileHeader.interleaveStep = interleaveStep;

			// first entries must be zero
			imprint_t zero;
			::memset(&zero, 0, sizeof(zero));
			for (uint32_t i = 0; i < IDFIRST; i++)
				assert(::memcmp(this->imprints + i, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numImprint  = this->numImprint;
			fileHeader.offImprints = flen;
			flen += writeData(outf, this->imprints, sizeof(*this->imprints) * this->numImprint, fileName, "imprint");
			if (this->imprintIndexSize) {
				// Index
				fileHeader.imprintIndexSize = this->imprintIndexSize;
				fileHeader.offImprintIndex  = flen;
				flen += writeData(outf, this->imprintIndex, sizeof(*this->imprintIndex) * this->imprintIndexSize, fileName, "imprintIndex");
			}
		} else {
			// interleave only when imprints present
			fileHeader.interleave     = 0;
			fileHeader.interleaveStep = 0;
		}

		/*
		 * write sid/tid pairs
		 */
		if (this->numPair) {
			// first entries must be zero
			pair_t zero;
			::memset(&zero, 0, sizeof(zero));
			for (uint32_t i = 0; i < IDFIRST; i++)
				assert(::memcmp(this->pairs + i, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numPair  = this->numPair;
			fileHeader.offpairs = flen;
			flen += writeData(outf, this->pairs, sizeof(*this->pairs) * this->numPair, fileName, "pair");
			if (this->pairIndexSize) {
				// Index
				fileHeader.pairIndexSize = this->pairIndexSize;
				fileHeader.offPairIndex  = flen;
				flen += writeData(outf, this->pairIndex, sizeof(*this->pairIndex) * this->pairIndexSize, fileName, "pairIndex");
			}
		}

		/*
		 * write members
		 */
		if (this->numMember) {
			// first entries must be zero
			member_t zero;
			::memset(&zero, 0, sizeof(zero));
			for (uint32_t i = 0; i < IDFIRST; i++)
				assert(::memcmp(this->members + i, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numMember = this->numMember;
			fileHeader.offMember = flen;
			flen += writeData(outf, this->members, sizeof(*this->members) * this->numMember, fileName, "member");
			if (this->memberIndexSize) {
				// Index
				fileHeader.memberIndexSize = this->memberIndexSize;
				fileHeader.offMemberIndex  = flen;
				flen += writeData(outf, this->memberIndex, sizeof(*this->memberIndex) * this->memberIndexSize, fileName, "memberIndex");
			}
		}

		/*
		 * write patternFirst
		 */
		if (this->numPatternFirst) {
			// first entries must be zero
			patternFirst_t zero;
			::memset(&zero, 0, sizeof(zero));
			for (uint32_t i = 0; i < IDFIRST; i++)
				assert(::memcmp(this->patternsFirst + i, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numPatternFirst = this->numPatternFirst;
			fileHeader.offPatternFirst = flen;
			flen += writeData(outf, this->patternsFirst, sizeof(*this->patternsFirst) * this->numPatternFirst, fileName, "patternFirst");
			if (this->patternFirstIndexSize) {
				// Index
				fileHeader.patternFirstIndexSize = this->patternFirstIndexSize;
				fileHeader.offPatternFirstIndex  = flen;
				flen += writeData(outf, this->patternFirstIndex, sizeof(*this->patternFirstIndex) * this->patternFirstIndexSize, fileName, "patternFirstIndex");
			}
		}

		/*
		 * write patternSecond
		 */
		if (this->numPatternSecond) {
			// Second entries must be zero
			patternSecond_t zero;
			::memset(&zero, 0, sizeof(zero));
			for (uint32_t i = 0; i < IDFIRST; i++)
				assert(::memcmp(this->patternsSecond + i, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numPatternSecond = this->numPatternSecond;
			fileHeader.offPatternSecond = flen;
			flen += writeData(outf, this->patternsSecond, sizeof(*this->patternsSecond) * this->numPatternSecond, fileName, "patternSecond");
			if (this->patternSecondIndexSize) {
				// Index
				fileHeader.patternSecondIndexSize = this->patternSecondIndexSize;
				fileHeader.offPatternSecondIndex  = flen;
				flen += writeData(outf, this->patternSecondIndex, sizeof(*this->patternSecondIndex) * this->patternSecondIndexSize, fileName, "patternSecondIndex");
			}
		}

		/*
		 * Rewrite header and close
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[Kclosing");

		fileHeader.magic                     = FILE_MAGIC;
		fileHeader.magic_flags               = creationFlags;
		fileHeader.magic_maxSlots            = MAXSLOTS;
		fileHeader.magic_sizeofSignature     = sizeof(signature_t);
		fileHeader.magic_sizeofSwap          = sizeof(swap_t);
		fileHeader.magic_sidCRC              = sidCRC;
		fileHeader.magic_sizeofImprint       = sizeof(imprint_t);
		fileHeader.magic_sizeofPair          = sizeof(pair_t);
		fileHeader.magic_sizeofMember        = sizeof(member_t);
		fileHeader.magic_sizeofPatternFirst  = sizeof(patternFirst_t);
		fileHeader.magic_sizeofPatternSecond = sizeof(patternSecond_t);
		fileHeader.offEnd                    = flen;

		// rewrite header
		fseek(outf, 0, SEEK_SET);
		fwrite(&fileHeader, sizeof(fileHeader), 1, outf);

		// test for errors, most likely disk-full
		if (feof(outf) || ferror(outf)) {
			int savErrno = errno;
			::remove(fileName);
			errno = savErrno;
			ctx.fatal("\n{\"error\":\"ferror('%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);
		}

		// close
		if (fclose(outf)) {
			int savErrno = errno;
			::remove(fileName);
			errno = savErrno;
			ctx.fatal("\n{\"error\":\"fclose('%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K"); // erase progress

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Written %s, %lu bytes\n", ctx.timeAsString(), fileName, fileHeader.offEnd);
	}

	/**
	 * @date 2020-03-12 15:54:57
	 *
	 * Read data from database file
	 *
	 * @param {number} hndl - OS file handle
	 * @param {void[]} data - Buffer to read to
	 * @param {number} dataLength - how much to read
	 * @return {number} total number of bytes read
	 */
	uint64_t readData(int hndl, void *data, size_t dataLength) {

		// read in chunks of 1024*1024 bytes
		uint64_t sumRead = 0;
		while (dataLength > 0) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				fprintf(stderr, "\r\e[K%.5f%%", ctx.progress * 100.0 / ctx.progressHi);
				ctx.tick = 0;
			}

			/*
			 * Determine bytes to write
			 */
			size_t sliceLength = dataLength;
			if (sliceLength > 1024 * 1024)
				sliceLength = 1024 * 1024;

			/*
			 * Write
			 */
			size_t ret = ::read(hndl, data, sliceLength);
			if (ret != sliceLength)
				ctx.fatal("\n{\"error\":\"read(%lu)\",\"where\":\"%s:%s:%d\",\"return\":\"%lu\"}\n", sliceLength, __FUNCTION__, __FILE__, __LINE__, ret);

			/*
			 * Update
			 */
			data = (void *) ((char *) data + sliceLength);
			dataLength -= sliceLength;
			ctx.progress += sliceLength;
			sumRead += sliceLength;
		}

		return sumRead;
	}

	/**
	 * @date 2020-03-12 15:56:46
	 *
	 * Write data to database file
	 *
	 * @param {number} hndl - OS file handle
	 * @param {void[]} data - Buffer to read to
	 * @param {number} dataLength - how much to write
	 * @param {string} fileName - file to delete on error
	 * @return {number} total number of bytes written
	 */
	uint64_t writeData(FILE *outf, const void *data, size_t dataLength, const char *fileName, const char *section) {

		// write in chunks of 1024*1024 bytes

		size_t written = 0;
		while (dataLength > 0) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				fprintf(stderr, "\r\e[K%.5f%% %s", ctx.progress * 100.0 / ctx.progressHi, section);
				ctx.tick = 0;
			}

			/*
			 * Determine bytes to write
			 */
			uint64_t sliceLength = dataLength;
			if (sliceLength > 1024 * 1024)
				sliceLength = 1024 * 1024;

			/*
			 * Write
			 */
			size_t ret = ::fwrite(data, 1, sliceLength, outf);
			if (ret != sliceLength) {
				int savErrno = errno;
				::remove(fileName);
				errno        = savErrno;
				ctx.fatal("\n{\"error\":\"fwrite(%lu)\",\"where\":\"%s:%s:%d\",\"return\":\"%lu\"}\n", sliceLength, __FUNCTION__, __FILE__, __LINE__, ret);
			}

			/*
			 * Update
			 */
			data = (void *) ((char *) data + sliceLength);
			dataLength -= sliceLength;
			written += sliceLength;
			ctx.progress += sliceLength;
		}

		/*
		 * 32-byte align for SIMD
		 */
		dataLength = 32U - (written & 31U);
		if (dataLength > 0) {
			uint8_t zero32[32] = {0};

			fwrite(zero32, dataLength, 1, outf);
			written += dataLength;
		}

		return written;
	}

	/*
	 * Transform store
	 */

	/**
	 * @date 2020-03-12 10:28:05
	 *
	 * Lookup a transform name and return its matching enumeration id.
	 * Transform names can be short meaning that trailing endpoints which are in sync can be omitted.
	 * Example: For `"bdacefghi"`, `"bdac"` is the minimum transform name and `"efghi"` is the "long" part.
	 *
	 * NOTE: Transform names must be syntactically correct:
	 *  - No longer than `MAXSLOTS` characters
	 *  - Consisting of exclusively the lowercase letters `'a'` to `'i'` (for `MAXSLOTS`==9)
	 *
	 * @param {string} pName - Transform name
	 * @param {number[MAXTRANSFORMINDEX]} pIndex - output name lookup index
	 * @return {number} - Transform enumeration id or `IBIT` if "not-found"
	 */
	inline uint32_t lookupTransform(const char *pName, uint32_t *pIndex) {
		assert(pIndex);

		// starting position in index
		unsigned pos = MAXSLOTS + 1;

		// walk through states
		while (*pName) {
			pos = pIndex[pos + *pName - 'a'];
			pName++;
		}

		// what to return
		if (pos == 0)
			return IBIT; // "not-found"
		else if (!(pos & IBIT))
			return pIndex[pos + MAXSLOTS] & ~IBIT; // short names
		else
			return pos & ~IBIT; // long name
	}


	/**
	 * @date 2020-05-04 10:06:58
	 *
	 * Lookup a name after applying a transform and return its matching enumeration id.
	 *
	 * @param {string} pName - Transform name
	 * @param {string} pSkin - Apply transform to pName
	 * @param {number[MAXTRANSFORMINDEX]} pIndex - output name lookup index
	 * @return {number} - Transform enumeration id or `IBIT` if "not-found"
	 */
	inline uint32_t lookupTransformName(const char *pName, const char *pSkin, uint32_t *pIndex) {
		assert(pIndex);

		// starting position in index
		unsigned pos = MAXSLOTS + 1;

		// walk through states
		while (*pName) {
			pos = pIndex[pos + pSkin[*pName - 'a'] - 'a'];
			pName++;
		}

		// what to return
		if (pos == 0)
			return IBIT; // "not-found"
		else if (!(pos & IBIT))
			return pIndex[pos + MAXSLOTS] & ~IBIT; // short names
		else
			return pos & ~IBIT; // long name
	}

	/**
	 * @date 2020-05-04 10:06:58
	 *
	 * Lookup a name after applying transform to slot indices and return its matching enumeration id.
	 *
	 * Example, for `transform="cab"`: "{slots[2],slots[0],slots[1]}"
	 *
	 * @param {string} pName - Transform name
	 * @param {string} pSkin - Apply transform to pName
	 * @param {number[MAXTRANSFORMINDEX]} pIndex - output name lookup index
	 * @return {number} - Transform enumeration id or `IBIT` if "not-found"
	 */
	inline uint32_t lookupTransformSlot(const char *pName, const char *pSkin, uint32_t *pIndex) {
		assert(pIndex);

		// transform indices
		char     newName[MAXSLOTS + 1];
		unsigned j;
		for (j = 0; pSkin[j]; j++)
			newName[pSkin[j] - 'a'] = pName[j];

		newName[j] = 0;

		// apply result
		pName = newName;

		// starting position in index
		unsigned pos = MAXSLOTS + 1;

		// walk through states
		while (*pName) {
			pos = pIndex[pos + *pName - 'a'];
			pName++;
		}

		// what to return
		if (pos == 0)
			return IBIT; // "not-found"
		else if (!(pos & IBIT))
			return pIndex[pos + MAXSLOTS] & ~IBIT; // short names
		else
			return pos & ~IBIT; // long name
	}

	/**
 	 * @date 2020-03-13 14:20:29
 	 *
	 * Lookup a forward transform name and return its matching enumeration id.
 	 *
	 * @param {string} pName - Transform name
 	 * @return {number} - Transform enumeration id or `IBIT` if "not-found"
 	 */
	inline uint32_t lookupFwdTransform(const char *pName) {
		return lookupTransform(pName, this->fwdTransformNameIndex);
	}

	/**
  	 * @date 2020-03-13 14:20:29
  	 *
	 * Lookup a reverse transform name and return its matching enumeration id.
  	 *
 	 * @param {string} pName - Transform name
  	 * @return {number} - Transform enumeration id or `IBIT` if "not-found"
  	 */
	inline uint32_t lookupRevTransform(const char *pName) {
		return lookupTransform(pName, this->revTransformNameIndex);
	}

	/*
	 * Evaluator store [COPY-ON-WRITE]
	 */

	/**
	 * Construct the dataset for the evaluator
	 */
	inline void initialiseEvaluators(void) {
		assert(this->numTransform == MAXTRANSFORM);
		assert(this->numEvaluator == tinyTree_t::TINYTREE_NEND * this->numTransform);
		tinyTree_t::initialiseEvaluator(ctx, this->fwdEvaluator, this->numTransform, this->fwdTransformData);
		tinyTree_t::initialiseEvaluator(ctx, this->revEvaluator, this->numTransform, this->revTransformData);
	}

	/*
	 * Signature store
	 */

	/**
	 * Perform signature lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the signature.
	 *
	 * @param v {string} v - key value
	 * @return {number} offset into index
	 */
	inline uint32_t lookupSignature(const char *name) {
		assert(this->numSignature);
		ctx.cntHash++;

		// calculate starting position
		uint32_t crc32 = 0;

		for (const char *pName = name; *pName; pName++)
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));

		uint32_t ix   = crc32 % signatureIndexSize;
		uint32_t bump = ix;
		if (bump == 0)
			bump = signatureIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		if (signatureVersion == NULL) {
			for (;;) {
				ctx.cntCompare++;
				if (this->signatureIndex[ix] == 0)
					return ix; // "not-found"

				const signature_t *pSignature = this->signatures + this->signatureIndex[ix];

				if (::strcmp(pSignature->name, name) == 0)
					return ix; // "found"

				// overflow, jump to next entry
				// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
				ix += bump;
				if (ix >= signatureIndexSize)
					ix -= signatureIndexSize;
			}

		} else {
			for (;;) {
				ctx.cntCompare++;
				if (this->signatureVersion[ix] != iVersion)
					return ix; // "not-found"

				if (this->signatureVersion[ix] != 0) {
					const signature_t *pSignature = this->signatures + this->signatureIndex[ix];

					if (::strcmp(pSignature->name, name) == 0)
						return ix; // "found"
				}
				// overflow, jump to next entry
				// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
				ix += bump;
				if (ix >= signatureIndexSize)
					ix -= signatureIndexSize;
			}
		}

	}

	/**
 	 * Add a new signature to the dataset
 	 *
	 * @param v {string} v - key value
	 * @return {number} signatureId
	 */
	inline uint32_t addSignature(const char *name) {
		signature_t *pSignature = this->signatures + this->numSignature++;

		if (this->numSignature > this->maxSignature)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxSignature\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxSignature);

		// clear before use
		::memset(pSignature, 0, sizeof(*pSignature));

		// only populate key fields
		assert(strlen(name) <= signature_t::SIGNATURENAMELENGTH);
		strcpy(pSignature->name, name);

		return (uint32_t) (pSignature - this->signatures);
	}

	/*
	 * Swap store
	 */

	/**
	 * @date 2020-05-04 14:37:11
	 *
	 * Perform swap lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the swap.
	 *
	 * @param v {swap_t} v - key value
	 * @return {number} offset into index
	 */
	inline uint32_t lookupSwap(const swap_t *pSwap) {
		assert(this->numSwap);
		ctx.cntHash++;

		// calculate starting position
		uint32_t crc32 = 0;

		for (unsigned j = 0; j < swap_t::MAXENTRY; j++)
			crc32 = __builtin_ia32_crc32si(crc32, pSwap->tids[j]);

		uint32_t ix   = crc32 % swapIndexSize;
		uint32_t bump = ix;
		if (bump == 0)
			bump = swapIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		for (;;) {
			ctx.cntCompare++;
			if (this->swapIndex[ix] == 0)
				return ix; // "not-found"

			if (this->swaps[this->swapIndex[ix]].equals(*pSwap))
				return ix; // "found"

			// overflow, jump to next entry
			// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
			ix += bump;
			if (ix >= swapIndexSize)
				ix -= swapIndexSize;
		}
	}

	/**
 	 * Add a new swap to the dataset
 	 *
	 * @param v {swap_t} v - key value
	 * @return {number} swapId
	 */
	inline uint32_t addSwap(swap_t *pSwap) {
		uint32_t swapId = this->numSwap++;

		if (this->numSwap > this->maxSwap)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxSwap\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxSwap);

		::memcpy(&this->swaps[swapId], pSwap, sizeof(*pSwap));

		return swapId;
	}

	/*
	 * Imprint store
	 */

	/**
	 * @date 2020-03-15 20:07:14
	 *
	 * Perform imprint lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the imprint.
	 *
	 * @param v {footprint_t} v - key value
	 * @return {number} offset into index
	 */
	inline uint32_t lookupImprint(const footprint_t &v) const {
		assert(this->numImprint);
		ctx.cntHash++;

		// starting position
		uint32_t crc = v.crc32();

		uint32_t ix = crc % imprintIndexSize;
		uint32_t bump = ix;
		if (bump == 0)
			bump = imprintIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		if (imprintVersion == NULL) {
			for (;;) {
				ctx.cntCompare++;
				if (this->imprintIndex[ix] == 0)
					return ix; // "not-found"

				const imprint_t *pImprint = this->imprints + this->imprintIndex[ix]; // point to data

				if (pImprint->footprint.equals(v))
					return ix; // "found"

				// overflow, jump to next entry
				// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
				ix += bump;
				if (ix >= imprintIndexSize)
					ix -= imprintIndexSize; // effectively modulo
			}
		} else {
			for (;;) {
				ctx.cntCompare++;
				if (this->imprintVersion[ix] != iVersion)
					return ix; // "not-found"

				if (this->imprintIndex[ix] != 0) {
					const imprint_t *pImprint = this->imprints + this->imprintIndex[ix]; // point to data

					if (pImprint->footprint.equals(v))
						return ix; // "found"
				}

				// overflow, jump to next entry
				// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
				ix += bump;
				if (ix >= imprintIndexSize)
					ix -= imprintIndexSize; // effectively modulo
			}
		}
	}

	/**
	 * Add a new imprint to the dataset
	 *
	 * @param v {footprint_t} v - key value
	 * @return {number} imprintId
	 */
	inline uint32_t addImprint(const footprint_t &v) {
		imprint_t *pImprint = this->imprints + this->numImprint++;

		if (this->numImprint > this->maxImprint)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxImprint\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxImprint);

		// only populate key fields
		pImprint->footprint = v;

		return (uint32_t) (pImprint - this->imprints);
	}

	/*
	 * @date 2020-03-17 18:16:51
	 *
	 *  Imprinting indexing has two modes, one stores key rows, the other key columns.
	 * `interleaveStep` is the distance between two adjacent rows and mode independent
	 * `interleave` is the number of imprints stored per footprint
	 *
	 * If interleave` == `interleaveStep` then the mode is "store key columns", otherwise "store key rows"
	 *
	 * A lot of effort has been put into interleaving because it serves for selftesting and preparation for scalability.
	 * (MAXSLOTS used to be 8, and preparation are for 10)
	 */

	/**
	 * @date 2020-03-16 21:20:18
	 *
	 * Associative lookup of a footprint
	 *
	 * Find any orientation of the footprint and return the matching structure and skin with identical effect
	 *
	 * @param {tinyTree_t} pTree - Tree containg expression
	 * @param {footprint_t[]} pFwdEvaluator - Evaluator with forward transforms (modified)
	 * @param {footprint_t[]} RevEvaluator - Evaluator with reverse transforms (modified)
	 * @param {number} sid - found structure id
	 * @param {number} tid - found transform id. what was queried can be reconstructed as `"sid/tid"`
	 * @return {boolean} - `true` if found, `false` if not.
	 */
	inline bool lookupImprintAssociative(const tinyTree_t *pTree, footprint_t *pFwdEvaluator, footprint_t *pRevEvaluator, uint32_t *sid, uint32_t *tid, uint32_t root = 0) {
		/*
		 * @date 2021-10-20 22:23:56
		 * NOTE: Any changes here should also be applied to `genpatternContext_t::foundTreePattern()`.
		 */
		/*
		 * According to `performSelfTestInterleave` the following is true:
	         *   fwdTransform[row + col] == fwdTransform[row][fwdTransform[col]]
	         *   revTransform[row][fwdTransform[row + col]] == fwdTransform[col]
		 */
		if (root == 0)
			root = pTree->root;

		if (this->interleave == this->interleaveStep) {
			/*
			 * index is populated with key cols, runtime scans rows
			 * Because of the jumps, memory cache might be killed
			 */

			// permutate all rows
			for (unsigned iRow = 0; iRow < MAXTRANSFORM; iRow += this->interleaveStep) {

				// find where the evaluator for the key is located in the evaluator store
				footprint_t *v = pRevEvaluator + iRow * tinyTree_t::TINYTREE_NEND;

				// apply the reverse transform
				pTree->eval(v);

				// search the resulting footprint in the cache/index
				uint32_t ix = this->lookupImprint(v[root]);

				/*
				 * Was something found
				 */
				if ((this->imprintVersion == NULL || this->imprintVersion[ix] == iVersion) && this->imprintIndex[ix] != 0) {
					/*
					 * Is so, then found the stripe which is the starting point. iTransform is relative to that
					 */
					const imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					*sid = pImprint->sid;
					*tid = pImprint->tid + iRow;
					return true;
				}
			}
		} else {
			/*
			 * index is populated with key rows, runtime scans cols
			 *
			 * @date 2020-04-18 22:51:05
			 *
			 * This path is cpu cache friendlier because of `iCol++`
			 */
			footprint_t *v = pFwdEvaluator;

			// permutate all colums
			for (unsigned iCol = 0; iCol < interleaveStep; iCol++) {

				// apply the tree to the store
				pTree->eval(v);

				// search the resulting footprint in the cache/index
				uint32_t ix = this->lookupImprint(v[root]);

				/*
				 * Was something found
				 */
				if ((this->imprintVersion == NULL || this->imprintVersion[ix] == iVersion) && this->imprintIndex[ix] != 0) {
					/*
					* Is so, then found the stripe which is the starting point. iTransform is relative to that
					*/
					const imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					*sid = pImprint->sid;
					/*
					* NOTE: Need to reverse the transform
					*/
					*tid = this->revTransformIds[pImprint->tid + iCol];
					return true;
				}

				v += tinyTree_t::TINYTREE_NEND;
			}
		}
		
		// not found
		sid = tid = 0;
		return false;
	}

	/**
	 * @date 2020-03-16 21:46:02
	 *
	 * Associative lookup of a footprint
	 *
	 * Find any orientation of the footprint and return the matching structure and skin with identical effect
	 *
	 * @date 2020-04-24 12:19:15
	 *
	 * There are a number of occasion where there are `add if not found` situations.
	 * This is done to prevent the `pStore->addImprintAssociative()` throwing an unrecoverable error.
	 *
	 * Typically, code looks like:
	 * 	```
	 *	if (!pStore->lookupImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, &sid, &tid))
	 * 		pStore->addImprintAssociative(&tree, this->pEvalFwd, this->pEvalRev, iSid);
	 *      ```
	 *
	 * However, in nearly most cases the lookup fails which makes the lookup extremely expensive, especially with low interleaves.
	 *
	 * Make addImprintAssociative more robust by assuming add-if-not-found situation.
	 * Every caller must/check the result.
	 *
	 * @date 2020-04-25 21:46:09
	 *
	 * WARNING: It turns out that add-if-not-found works partially.
	 *          detection for found is only performed for tid=0.
	 *          If an imprint is added for a signature with a different tid, that is not detected.
	 *          dd-if-not-found is ultra-fast in situations like joining lists but has the side effect of creating false positives
	 *
	 * @param {tinyTree_t} pTree - Tree containg expression
	 * @param {footprint_t[]} pFwdEvaluator - Evaluator with forward transforms (modified)
	 * @param {footprint_t[]} RevEvaluator - Evaluator with reverse transforms (modified)
	 * @param {number} sid - structure id to attach to imprints.
	 * @return {number} - zero for succeed, otherwise tree is already present with sid as return value.
	 */
	inline uint32_t addImprintAssociative(const tinyTree_t *pTree, footprint_t *pFwdEvaluator, footprint_t *pRevEvaluator, uint32_t sid) {
		/*
		 * According to `performSelfTestInterleave` the following is true:
	         *   fwdTransform[row + col] == fwdTransform[row][fwdTransform[col]]
	         *   revTransform[row][fwdTransform[row + col]] == fwdTransform[col]
		 */
		if (this->interleave == this->interleaveStep) {
			/*
			 * index is populated with key rows, runtime scans cols
			 *
			 * @date 2020-04-18 22:51:05
			 *
			 * This path is cpu cache friendlier because of `iCol++`
			 */

			footprint_t *v = pFwdEvaluator;

			// permutate cols
			for (unsigned iCol = 0; iCol < this->interleaveStep; iCol++) {

				// apply the tree to the store
				pTree->eval(v);

				// search the resulting footprint in the cache/index
				uint32_t ix = this->lookupImprint(v[pTree->root]);

				// add to the database is not there
				if (this->imprintIndex[ix] == 0 || (this->imprintVersion != NULL && this->imprintVersion[ix] != iVersion)) {
					this->imprintIndex[ix]           = this->addImprint(v[pTree->root]);
					if (this->imprintVersion)
						this->imprintVersion[ix] = iVersion;

					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// populate non-key fields
					pImprint->sid = sid;
					pImprint->tid = iCol;
				} else {
					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// test for similar. First imprint must be unique, others must have matching sid
					if (iCol == 0) {
						// signature already present, return found
						return pImprint->sid;
					} else if (pImprint->sid != sid) {
						ctx.fatal("\n{\"error\":\"index entry already in use\",\"where\":\"%s:%s:%d\",\"newsid\":\"%u\",\"newtid\":\"%u\",\"oldsid\":\"%u\",\"oldtid\":\"%u\",\"newname\":\"%s\",\"newname\":\"%s\"}\n",
							  __FUNCTION__, __FILE__, __LINE__, sid, iCol, pImprint->sid, pImprint->tid, this->signatures[pImprint->sid].name, this->signatures[sid].name);
					}
				}

				v += tinyTree_t::TINYTREE_NEND;
			}
		} else {
			/*
			 * index is populated with key cols, runtime scans rows
			 */
			// permutate rows
			for (unsigned iRow = 0; iRow < MAXTRANSFORM; iRow += this->interleaveStep) {

				// find where the transform is located in the evaluator store
				footprint_t *v = pRevEvaluator + iRow * tinyTree_t::TINYTREE_NEND;

				// apply the forward transform
				pTree->eval(v);

				// search the resulting footprint in the cache/index
				uint32_t ix = this->lookupImprint(v[pTree->root]);

				// add to the database is not there
				if (this->imprintIndex[ix] == 0 || (this->imprintVersion != NULL && this->imprintVersion[ix] != iVersion)) {
					this->imprintIndex[ix]           = this->addImprint(v[pTree->root]);
					if (this->imprintVersion)
						this->imprintVersion[ix] = iVersion;

					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// populate non-key fields
					pImprint->sid = sid;
					pImprint->tid = iRow;
				} else {
					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// test for similar. First imprint must be unique, others must have matching sid
					if (iRow == 0) {
						// signature already present, return found
						return pImprint->sid;
					} else if (pImprint->sid != sid) {
						ctx.fatal("\n{\"error\":\"index entry already in use\",\"where\":\"%s:%s:%d\",\"newsid\":\"%u\",\"newtid\":\"%u\",\"oldsid\":\"%u\",\"oldtid\":\"%u\",\"newname\":\"%s\",\"newname\":\"%s\"}\n",
							  __FUNCTION__, __FILE__, __LINE__, sid, iRow, pImprint->sid, pImprint->tid, this->signatures[pImprint->sid].name, this->signatures[sid].name);
					}
				}
			}
		}

		// return succeeded
		return 0;
	}

	/*
	 * Sid/Tid pair store
	 */

	/**
	 * @date 2021-07-08 21:35:58
	 *
	 * Perform sid/tid lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the sid/tid pair.
	 *
	 * @param id {uint32_t} - key value
	 * @param tid {uint32_t} - key value
	 * @return {number} offset into index
	 */
	inline uint32_t lookupPair(uint32_t id, uint32_t tid) {
		assert(this->numPair);
		ctx.cntHash++;

		// calculate starting position
		uint32_t crc32 = 0;

		crc32 = __builtin_ia32_crc32si(crc32, id);
		crc32 = __builtin_ia32_crc32si(crc32, tid);

		uint32_t ix   = crc32 % pairIndexSize;
		uint32_t bump = ix;
		if (bump == 0)
			bump = pairIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		for (;;) {
			ctx.cntCompare++;
			if (this->pairIndex[ix] == 0)
				return ix; // "not-found"

			if (this->pairs[this->pairIndex[ix]].equals(id, tid))
				return ix; // "found"

			// overflow, jump to next entry
			// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
			ix += bump;
			if (ix >= pairIndexSize)
				ix -= pairIndexSize;
		}
	}

	/**
 	 * Add a new sid/tid pair to the dataset
 	 *
	 * @param id {uint32_t} - key value
	 * @param tid {uint32_t} - key value
	 * @return {number} pairId
	 */
	inline uint32_t addPair(uint32_t id, uint32_t tid) {
		pair_t *pPair = this->pairs + this->numPair++;

		if (this->numPair > this->maxPair)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxPair\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxPair);

		pPair->id  = id;
		pPair->tid = tid;

		return (uint32_t) (pPair - this->pairs);
	}

	/*
	 * Member store
	 */

	/**
	 * Perform member lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the member.
	 *
	 * @param v {string} v - key value
	 * @return {number} offset into index
	 */
	inline uint32_t lookupMember(const char *name) {
		assert(this->numMember);
		ctx.cntHash++;

		// calculate starting position
		uint32_t        crc32  = 0;
		for (const char *pName = name; *pName; pName++)
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));

		uint32_t ix   = crc32 % memberIndexSize;
		uint32_t bump = ix;
		if (bump == 0)
			bump = memberIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		for (;;) {
			ctx.cntCompare++;
			if (this->memberIndex[ix] == 0)
				return ix; // "not-found"

			const member_t *pMember = this->members + this->memberIndex[ix];

			if (::strcmp(pMember->name, name) == 0)
				return ix; // "found"

			// overflow, jump to next entry
			// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
			ix += bump;
			if (ix >= memberIndexSize)
				ix -= memberIndexSize;
		}

	}

	/**
	 * Add a new member to the dataset
	 *
	 * @param v {string} name - key value
	 * @return {number} memberId
	 */
	inline uint32_t addMember(const char *name) {
		member_t *pMember = this->members + this->numMember++;

		if (this->numMember > this->maxMember)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxMember\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxMember);

		// clear before use
		::memset(pMember, 0, sizeof(*pMember));

		// only populate key fields
		strcpy(pMember->name, name);

		return (uint32_t) (pMember - this->members);
	}

	/*
	 * Pattern, First-stage store
	 */

	/**
	 * Perform patternFirst lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the patternFirst.
	 *
	 * @param v {uint32_t} sidQ - key value
	 * @param v {uint32_t} sidT - key value
	 * @param v {uint32_t} tidQT - key value
	 * @return {uint32_t} offset into index
	 */
	inline uint32_t lookupPatternFirst(uint32_t sidQ, uint32_t sidT, uint32_t tidSlotT) {
		assert(this->numPatternFirst);
		ctx.cntHash++;

		// split sidT into invert bit and unsigned parts 
		unsigned sidTj = sidT & IBIT ? 1 : 0;
		uint32_t sidTu = sidT & ~IBIT;

		// verify data fits in packed fields
		assert(sidQ < (1 << 20));
		assert(sidTu < (1 << 20));
		assert(tidSlotT < (1 << 19));

		// calculate starting position
		uint32_t crc32 = 0;
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(sidQ));
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(sidT));
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(tidSlotT));

		uint32_t ix   = crc32 % patternFirstIndexSize;
		uint32_t bump = ix;
		if (bump == 0)
			bump = patternFirstIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		for (;;) {
			ctx.cntCompare++;
			if (this->patternFirstIndex[ix] == 0)
				return ix; // "not-found"

			const patternFirst_t *pPatternFirst = this->patternsFirst + this->patternFirstIndex[ix];

			if (pPatternFirst->sidQ == sidQ && pPatternFirst->sidTu == sidTu && pPatternFirst->sidTj == sidTj && pPatternFirst->tidSlotT == tidSlotT)
				return ix; // "found"

			// overflow, jump to next entry
			// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
			ix += bump;
			if (ix >= patternFirstIndexSize)
				ix -= patternFirstIndexSize;
		}
	}


	/**
	 * Add a new patternFirst to the dataset
	 *
	 * @param v {uint32_t} sidQ - key value
	 * @param v {uint32_t} sidT - key value
	 * @param v {uint32_t} tidQT - key value
	 * @return {uint32_t} memberId
	 */
	inline uint32_t addPatternFirst(uint32_t sidQ, uint32_t sidT, uint32_t tidSlotT) {
		// split sidT into invert bit and unsigned parts 
		unsigned sidTj = sidT & IBIT ? 1 : 0;
		uint32_t sidTu = sidT & ~IBIT;

		patternFirst_t *pPatternFirst = this->patternsFirst + this->numPatternFirst++;

		if (this->numPatternFirst > this->maxPatternFirst)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxPatternFirst\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxPatternFirst);

		// clear before use
		::memset(pPatternFirst, 0, sizeof(*pPatternFirst));

		// verify data fits in packed fields
		assert(sidQ < (1 << 20));
		assert(sidTu < (1 << 20));
		assert(tidSlotT < (1 << 19));

		// only populate key fields
		pPatternFirst->sidQ     = sidQ;
		pPatternFirst->sidTj    = sidTj;
		pPatternFirst->sidTu    = sidTu;
		pPatternFirst->tidSlotT = tidSlotT;

		return (uint32_t) (pPatternFirst - this->patternsFirst);
	}

	/*
	 * Pattern, second-stage store
	 */

	/**
	 * Perform patternSecond lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the patternFirst.
	 *
	 * @param v {uint32_t} idFirst - key value
	 * @param v {uint32_t} sidF - key value
	 * @param v {uint32_t} sidFQ - key value
	 * @return {number} offset into index
	 */
	inline uint32_t lookupPatternSecond(uint32_t idFirst, uint32_t sidF, uint32_t tidSlotF) {
		assert(this->numPatternSecond);
		ctx.cntHash++;

		// verify data fits in packed fields
		assert(idFirst < (1 << 27));
		assert(sidF < (1 << 20));
		assert(tidSlotF < (1 << 19));

		// calculate starting position
		uint32_t crc32 = 0;
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(idFirst));
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(sidF));
		__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(tidSlotF));

		uint32_t ix   = crc32 % patternSecondIndexSize;
		uint32_t bump = ix;
		if (bump == 0)
			bump = patternSecondIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		for (;;) {
			ctx.cntCompare++;
			if (this->patternSecondIndex[ix] == 0)
				return ix; // "not-found"

			const patternSecond_t *pPatternSecond = this->patternsSecond + this->patternSecondIndex[ix];

			if (pPatternSecond->idFirst == idFirst && pPatternSecond->tidSlotF == tidSlotF && pPatternSecond->sidF == sidF)
				return ix; // "found"

			// overflow, jump to next entry
			// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
			ix += bump;
			if (ix >= patternSecondIndexSize)
				ix -= patternSecondIndexSize;
		}
	}

	/**
	 * Add a new patternSecond to the dataset
	 *
	 * @param v {string} name - key value
	 * @return {number} memberId
	 */
	inline uint32_t addPatternSecond(uint32_t idFirst, uint32_t sidF, uint32_t tidSlotF) {
		patternSecond_t *pPatternSecond = this->patternsSecond + this->numPatternSecond++;

		if (this->numPatternSecond > this->maxPatternSecond)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxPatternSecond\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxPatternSecond);

		// clear before use
		::memset(pPatternSecond, 0, sizeof(*pPatternSecond));

		// verify data fits in packed fields
		assert(idFirst < (1 << 27));
		assert(sidF < (1 << 20));
		assert(tidSlotF < (1 << 19));

		// only populate key fields
		pPatternSecond->idFirst  = idFirst;
		pPatternSecond->tidSlotF = tidSlotF;
		pPatternSecond->sidF     = sidF;

		return (uint32_t) (pPatternSecond - this->patternsSecond);
	}

	/*
	 * @date 2021-10-18 22:07:12
	 *
	 * Rebuild imprints and recreate imprint index.
	 */
	void rebuildImprint(void) {
		// start at first record
		this->numImprint = IDFIRST;
		
		// clear imprint index
		memset(this->imprintIndex, 0, this->imprintIndexSize * sizeof(*this->imprintIndex));

		if (this->numSignature <= 1)
			return; //nothing to do

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Rebuilding imprints\n", ctx.timeAsString());

		/*
		 * Create imprints for signature groups
		 */

		tinyTree_t tree(ctx);

		// reset ticker
		ctx.setupSpeed(this->numSignature);
		ctx.tick = 0;

		// create imprints for signature groups
		ctx.progress++; // skip reserved
		for (unsigned iSid = 1; iSid < this->numSignature; iSid++) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				if (perSecond == 0 || ctx.progress > ctx.progressHi) {
					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numImprint=%u(%.0f%%) | hash=%.3f",
						ctx.timeAsString(), ctx.progress, perSecond,
						this->numImprint, this->numImprint * 100.0 / this->maxImprint,
						(double) ctx.cntCompare / ctx.cntHash);
				} else {
					int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

					int etaH = eta / 3600;
					eta %= 3600;
					int etaM = eta / 60;
					eta %= 60;
					int etaS = eta;

					fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numImprint=%u(%.0f%%) | hash=%.3f",
						ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
						this->numImprint, this->numImprint * 100.0 / this->maxImprint,
						(double) ctx.cntCompare / ctx.cntHash);
				}

				ctx.tick = 0;
			}

			const signature_t *pSignature = this->signatures + iSid;

			// load signature
			tree.loadStringFast(pSignature->name);

			// add imprint
			unsigned ret = this->addImprintAssociative(&tree, this->fwdEvaluator, this->revEvaluator, iSid);
			assert(ret == 0);

			ctx.progress++;
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Imprints built. numImprint=%u(%.0f%%) | hash=%.3f\n",
				ctx.timeAsString(),
				this->numImprint, this->numImprint * 100.0 / this->maxImprint,
				(double) ctx.cntCompare / ctx.cntHash);
	}
	
	/**
	 * @date 2020-04-20 23:03:50
	 *
	 * Rebuild indices when settings changes makes them invalid
	 *
	 * @param {number} sections - set of sections to reindex
	 */
	void rebuildIndices(unsigned sections) {
		// anything to do
		sections &= ALLOCMASK_SIGNATUREINDEX |
			    ALLOCMASK_SWAPINDEX |
			    ALLOCMASK_IMPRINTINDEX |
			    ALLOCMASK_PAIRINDEX |
			    ALLOCMASK_MEMBERINDEX |
			    ALLOCMASK_PATTERNFIRSTINDEX |
			    ALLOCMASK_PATTERNSECONDINDEX;
		if (!sections)
			return;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Rebuilding indices [%s]\n", ctx.timeAsString(), this->sectionToText(sections).c_str());

		// reset ticker
		uint64_t numProgress = 0;
		if (sections & ALLOCMASK_SIGNATUREINDEX)
			numProgress += this->numSignature;
		if (sections & ALLOCMASK_SWAPINDEX)
			numProgress += this->numSwap;
		if (sections & ALLOCMASK_IMPRINTINDEX)
			numProgress += this->numImprint;
		if (sections & ALLOCMASK_PAIRINDEX)
			numProgress += this->numPair;
		if (sections & ALLOCMASK_MEMBERINDEX)
			numProgress += this->numMember;
		if (sections & ALLOCMASK_PATTERNFIRSTINDEX)
			numProgress += this->numPatternFirst;
		if (sections & ALLOCMASK_PATTERNSECONDINDEX)
			numProgress += this->numPatternSecond;
		ctx.setupSpeed(numProgress);
		ctx.tick = 0;

		/*
		 * Signatures
		 */

		if (sections & ALLOCMASK_SIGNATUREINDEX) {
			::memset(this->signatureIndex, 0, this->signatureIndexSize * sizeof(*this->signatureIndex));

			for (uint32_t iSid = 1; iSid < this->numSignature; iSid++) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | hash=%.3f", ctx.timeAsString(), ctx.progress, perSecond, (double) ctx.cntCompare / ctx.cntHash);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d  | hash=%.3f",
							ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
							(double) ctx.cntCompare / ctx.cntHash);
					}

					ctx.tick = 0;
				}

				const signature_t *pSignature = this->signatures + iSid;

				uint32_t ix = this->lookupSignature(pSignature->name);
				assert(this->signatureIndex[ix] == 0);
				this->signatureIndex[ix] = iSid;

				ctx.progress++;
			}
		}

		/*
		 * Swaps
		 */

		if (sections & ALLOCMASK_SWAPINDEX) {
			// clear
			::memset(this->swapIndex, 0, this->swapIndexSize * sizeof(*this->swapIndex));

			// rebuild
			for (uint32_t iSwap = 1; iSwap < this->numSwap; iSwap++) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | hash=%.3f", ctx.timeAsString(), ctx.progress, perSecond, (double) ctx.cntCompare / ctx.cntHash);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d  | hash=%.3f",
							ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
							(double) ctx.cntCompare / ctx.cntHash);
					}

					ctx.tick = 0;
				}

				const swap_t *pSwap = this->swaps + iSwap;

				uint32_t ix = this->lookupSwap(pSwap);
				assert(this->swapIndex[ix] == 0);
				this->swapIndex[ix] = iSwap;

				ctx.progress++;
			}
		}

		/*
		 * Imprints
		 */

		if (sections & ALLOCMASK_IMPRINTINDEX) {
			// clear
			::memset(this->imprintIndex, 0, this->imprintIndexSize * sizeof(*this->imprintIndex));

			// rebuild
			for (uint32_t iImprint = 1; iImprint < this->numImprint; iImprint++) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | hash=%.3f", ctx.timeAsString(), ctx.progress, perSecond, (double) ctx.cntCompare / ctx.cntHash);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d  | hash=%.3f",
							ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
							(double) ctx.cntCompare / ctx.cntHash);
					}

					ctx.tick = 0;
				}

				const imprint_t *pImprint = this->imprints + iImprint;

				uint32_t ix = this->lookupImprint(pImprint->footprint);
				assert(this->imprintIndex[ix] == 0);
				this->imprintIndex[ix] = iImprint;

				ctx.progress++;
			}
		}

		/*
		 * Sid/Tid pairs
		 */

		if (sections & ALLOCMASK_PAIRINDEX) {
			// clear
			::memset(this->pairIndex, 0, this->pairIndexSize * sizeof(*this->pairIndex));

			// rebuild
			for (uint32_t iPair = 1; iPair < this->numPair; iPair++) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | hash=%.3f", ctx.timeAsString(), ctx.progress, perSecond, (double) ctx.cntCompare / ctx.cntHash);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d  | hash=%.3f",
							ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
							(double) ctx.cntCompare / ctx.cntHash);
					}

					ctx.tick = 0;
				}

				const pair_t *pPair = this->pairs + iPair;

				uint32_t ix = this->lookupPair(pPair->id, pPair->tid);
				assert(this->pairIndex[ix] == 0);
				this->pairIndex[ix] = iPair;

				ctx.progress++;
			}
		}

		/*
		 * Members
		 */

		if (sections & ALLOCMASK_MEMBERINDEX) {
			// clear
			::memset(this->memberIndex, 0, this->memberIndexSize * sizeof(*this->memberIndex));

			// rebuild
			for (uint32_t iMember = 1; iMember < this->numMember; iMember++) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | hash=%.3f", ctx.timeAsString(), ctx.progress, perSecond, (double) ctx.cntCompare / ctx.cntHash);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d  | hash=%.3f",
							ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
							(double) ctx.cntCompare / ctx.cntHash);
					}

					ctx.tick = 0;
				}

				const member_t *pMember = this->members + iMember;

				uint32_t ix = this->lookupMember(pMember->name);
				assert(this->memberIndex[ix] == 0);
				this->memberIndex[ix] = iMember;

				ctx.progress++;
			}
		}

		/*
		 * PatternsFirst
		 */

		if (sections & ALLOCMASK_PATTERNFIRSTINDEX) {
			// clear
			::memset(this->patternFirstIndex, 0, this->patternFirstIndexSize * sizeof(*this->patternFirstIndex));

			// rebuild
			for (uint32_t iPatternfirst = 1; iPatternfirst < this->numPatternFirst; iPatternfirst++) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | hash=%.3f", ctx.timeAsString(), ctx.progress, perSecond, (double) ctx.cntCompare / ctx.cntHash);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d  | hash=%.3f",
							ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
							(double) ctx.cntCompare / ctx.cntHash);
					}

					ctx.tick = 0;
				}

				const patternFirst_t *pPatternFirst = this->patternsFirst + iPatternfirst;

				uint32_t ix = this->lookupPatternFirst(pPatternFirst->sidQ, pPatternFirst->sidTu ^ (pPatternFirst->sidTj ? IBIT : 0), pPatternFirst->tidSlotT); 
				assert(this->patternFirstIndex[ix] == 0);
				this->patternFirstIndex[ix] = iPatternfirst;

				ctx.progress++;
			}
		}

		/*
		 * PatternsSecond
		 */

		if (sections & ALLOCMASK_PATTERNSECONDINDEX) {
			// clear
			::memset(this->patternSecondIndex, 0, this->patternSecondIndexSize * sizeof(*this->patternSecondIndex));

			// rebuild
			for (uint32_t iPatternsecond = 1; iPatternsecond < this->numPatternSecond; iPatternsecond++) {
				if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
					int perSecond = ctx.updateSpeed();

					if (perSecond == 0 || ctx.progress > ctx.progressHi) {
						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | hash=%.3f", ctx.timeAsString(), ctx.progress, perSecond, (double) ctx.cntCompare / ctx.cntHash);
					} else {
						int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

						int etaH = eta / 3600;
						eta %= 3600;
						int etaM = eta / 60;
						eta %= 60;
						int etaS = eta;

						fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d  | hash=%.3f",
							ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
							(double) ctx.cntCompare / ctx.cntHash);
					}

					ctx.tick = 0;
				}

				const patternSecond_t *pPatternSecond = this->patternsSecond + iPatternsecond;

				uint32_t ix = this->lookupPatternSecond(pPatternSecond->idFirst, pPatternSecond->sidF, pPatternSecond->tidSlotF);
				assert(this->patternSecondIndex[ix] == 0);
				this->patternSecondIndex[ix] = iPatternsecond;

				ctx.progress++;
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Indices updated\n", ctx.timeAsString());
	}

	std::string sectionToText(unsigned sections) {
		std::string txt;
		
		if (sections & ALLOCMASK_TRANSFORM) {
			txt += "transform";
			sections &= ~ALLOCMASK_TRANSFORM;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_EVALUATOR) {
			txt += "evaluator";
			sections &= ~ALLOCMASK_EVALUATOR;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_SIGNATURE) {
			txt += "signature";
			sections &= ~ALLOCMASK_SIGNATURE;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_SIGNATUREINDEX) {
			txt += "signatureIndex";
			sections &= ~ALLOCMASK_SIGNATUREINDEX;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_SWAP) {
			txt += "swap";
			sections &= ~ALLOCMASK_SWAP;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_SWAPINDEX) {
			txt += "swapIndex";
			sections &= ~ALLOCMASK_SWAPINDEX;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_IMPRINT) {
			txt += "imprint";
			sections &= ~ALLOCMASK_IMPRINT;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_IMPRINTINDEX) {
			txt += "imprintIndex";
			sections &= ~ALLOCMASK_IMPRINTINDEX;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_PAIR) {
			txt += "pair";
			sections &= ~ALLOCMASK_PAIR;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_PAIRINDEX) {
			txt += "pairIndex";
			sections &= ~ALLOCMASK_PAIRINDEX;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_MEMBER) {
			txt += "member";
			sections &= ~ALLOCMASK_MEMBER;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_MEMBERINDEX) {
			txt += "memberIndex";
			sections &= ~ALLOCMASK_MEMBERINDEX;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_PATTERNFIRST) {
			txt += "patternFirst";
			sections &= ~ALLOCMASK_PATTERNFIRST;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_PATTERNFIRSTINDEX) {
			txt += "patternFirstIndex";
			sections &= ~ALLOCMASK_PATTERNFIRSTINDEX;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_PATTERNSECOND) {
			txt += "patternSecond";
			sections &= ~ALLOCMASK_PATTERNSECOND;
			if (sections)
				txt += '|';
		}
		if (sections & ALLOCMASK_PATTERNSECONDINDEX) {
			txt += "patternSecondIndex";
			sections &= ~ALLOCMASK_PATTERNSECONDINDEX;
			if (sections)
				txt += '|';
		}

		return txt;
	}

	/*
	 * @date 2021-07-24 10:49:25
	 *
	 * When swaps are involved, names and skins are a mess.
	 * This should finally plug the situation only it is highly expensive
	 *
	 * @date 2021-07-24 12:24:47
	 *
	 * Ugh with 'ecaab^!db2!!'
	 * The F component id `dbab^!'
	 *
	 */
	bool normaliseNameSkin(char *pName, char *pSkin, const signature_t *pSignature) {

		// load base tree
		tinyTree_t tree(ctx);
		tree.loadStringSafe(pName);

		// save safe name
		tree.saveString(tree.root, pName, NULL);

		// does signature have swap info
		if (pSignature->swapId == 0)
			return false;

		const swap_t *pSwap = this->swaps + pSignature->swapId;

		tinyTree_t testTree(ctx);

		bool anythingChanged = false;
		bool changed;

		do {
			changed = false;

			for (uint32_t iSwap = 0; iSwap < swap_t::MAXENTRY && pSwap->tids[iSwap]; iSwap++) {
				uint32_t tid = pSwap->tids[iSwap];

				// get the transform string
				const char *pTransformStr = this->fwdTransformNames[tid];

				// load transformed tree
				testTree.loadStringSafe(pName, pTransformStr);

				// test if better
				if (testTree.compare(testTree.root, &tree, tree.root) < 0) {
					// copy tree, including root, as new best candidate
					for (unsigned i = tinyTree_t::TINYTREE_NSTART; i <= testTree.root; i++)
						tree.N[i] = testTree.N[i];
					tree.root = testTree.root;

					// save better name
					tree.saveString(tree.root, pName, NULL);

					changed = true;
					anythingChanged = true;
				}
			}

			// TODO: Normalise skin
#if 0
			for (uint32_t iSwap = 0; iSwap < swap_t::MAXENTRY && pSwap->tids[iSwap]; iSwap++) {
				uint32_t tid = pSwap->tids[iSwap];

				// get the transform string
				const char *pTransformStr = pStore->fwdTransformNames[tid];

				// test if swap needed
				bool needSwap = false;

				for (unsigned i = 0; i < pSignature->numPlaceholder; i++) {
					if (sidSlots[tinyTree_t::TINYTREE_KSTART + i] > sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a']) {
						needSwap = true;
						break;
					}
					if (sidSlots[tinyTree_t::TINYTREE_KSTART + i] < sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a']) {
						needSwap = false;
						break;
					}
				}

				if (needSwap) {
					if (!displayed)
						printf(",\"level4\":[");
					else
						printf(",");
					printf("%.*s", pSignature->numPlaceholder, pStore->fwdTransformNames[tid]);
					displayed = true;

					uint32_t newSlots[MAXSLOTS];

					for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
						newSlots[i] = sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a'];

					for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
						sidSlots[tinyTree_t::TINYTREE_KSTART + i] = newSlots[i];

					changed = true;
				}
			}
#endif

		} while (changed);

		return anythingChanged;
	}

	/**
	 * @date 2020-03-12 19:36:56
	 *
	 * Encode dimensions as json object
	 */
	json_t *jsonInfo(json_t *jResult) {
		if (jResult == NULL)
			jResult = json_object();
		json_object_set_new_nocheck(jResult, "flags", json_integer(this->creationFlags));
		{
			char hex[10];
			sprintf(hex, "%08x", fileHeader.magic_sidCRC);
			json_object_set_new_nocheck(jResult, "sidCRC", json_string_nocheck(hex));
		}
		json_object_set_new_nocheck(jResult, "numTransform", json_integer(this->numTransform));
		json_object_set_new_nocheck(jResult, "transformIndexSize", json_integer(this->transformIndexSize));
		json_object_set_new_nocheck(jResult, "numEvaluator", json_integer(this->numEvaluator));
		json_object_set_new_nocheck(jResult, "numSignature", json_integer(this->numSignature));
		json_object_set_new_nocheck(jResult, "signatureIndexSize", json_integer(this->signatureIndexSize));
		json_object_set_new_nocheck(jResult, "numSwap", json_integer(this->numSwap));
		json_object_set_new_nocheck(jResult, "swapIndexSize", json_integer(this->swapIndexSize));
		json_object_set_new_nocheck(jResult, "interleave", json_integer(this->interleave));
		json_object_set_new_nocheck(jResult, "numImprint", json_integer(this->numImprint));
		json_object_set_new_nocheck(jResult, "imprintIndexSize", json_integer(this->imprintIndexSize));
		json_object_set_new_nocheck(jResult, "numPair", json_integer(this->numPair));
		json_object_set_new_nocheck(jResult, "pairIndexSize", json_integer(this->pairIndexSize));
		json_object_set_new_nocheck(jResult, "numMember", json_integer(this->numMember));
		json_object_set_new_nocheck(jResult, "memberIndexSize", json_integer(this->memberIndexSize));
		json_object_set_new_nocheck(jResult, "numPatternFirst", json_integer(this->numPatternFirst));
		json_object_set_new_nocheck(jResult, "patternFirstIndexSize", json_integer(this->patternFirstIndexSize));
		json_object_set_new_nocheck(jResult, "numPatternSecond", json_integer(this->numPatternSecond));
		json_object_set_new_nocheck(jResult, "patternSecondIndexSize", json_integer(this->patternSecondIndexSize));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader.offEnd));

		return jResult;
	}

};

#endif
