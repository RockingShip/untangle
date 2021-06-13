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
#define FILE_MAGIC        0x20200506

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
	uint32_t magic_sizeofHint;
	uint32_t magic_sizeofImprint;
	uint32_t magic_sizeofMember;
	uint32_t magic_sizeofPatternFirst;
	uint32_t magic_sizeofPatternSecond;
	uint32_t magic_sizeofGrow;

	// Associative index interleaving
	uint32_t interleave;
	uint32_t interleaveStep;

	// section sizes
	uint32_t numTransform;          // for both fwd/rev
	uint32_t transformIndexSize;    // for both fwd/rev
	uint32_t numSignature;
	uint32_t signatureIndexSize;
	uint32_t numSwap;
	uint32_t swapIndexSize;
	uint32_t numHint;
	uint32_t hintIndexSize;
	uint32_t numImprint;
	uint32_t imprintIndexSize;
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
	uint64_t offSignatures;
	uint64_t offSignatureIndex;
	uint64_t offSwaps;
	uint64_t offSwapIndex;
	uint64_t offHints;
	uint64_t offHintIndex;
	uint64_t offImprints;
	uint64_t offImprintIndex;
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
		ALLOCFLAG_SIGNATURE,
		ALLOCFLAG_SIGNATUREINDEX,
		ALLOCFLAG_SWAP,
		ALLOCFLAG_SWAPINDEX,
		ALLOCFLAG_HINT,
		ALLOCFLAG_HINTINDEX,
		ALLOCFLAG_IMPRINT,
		ALLOCFLAG_IMPRINTINDEX,
		ALLOCFLAG_MEMBER,
		ALLOCFLAG_MEMBERINDEX,

		// @formatter:off
		ALLOCMASK_TRANSFORM          = 1 << ALLOCFLAG_TRANSFORM,
		ALLOCMASK_SIGNATURE          = 1 << ALLOCFLAG_SIGNATURE,
		ALLOCMASK_SIGNATUREINDEX     = 1 << ALLOCFLAG_SIGNATUREINDEX,
		ALLOCMASK_SWAP               = 1 << ALLOCFLAG_SWAP,
		ALLOCMASK_SWAPINDEX          = 1 << ALLOCFLAG_SWAPINDEX,
		ALLOCMASK_HINT               = 1 << ALLOCFLAG_HINT,
		ALLOCMASK_HINTINDEX          = 1 << ALLOCFLAG_HINTINDEX,
		ALLOCMASK_IMPRINT            = 1 << ALLOCFLAG_IMPRINT,
		ALLOCMASK_IMPRINTINDEX       = 1 << ALLOCFLAG_IMPRINTINDEX,
		ALLOCMASK_MEMBER             = 1 << ALLOCFLAG_MEMBER,
		ALLOCMASK_MEMBERINDEX        = 1 << ALLOCFLAG_MEMBERINDEX,
		// @formatter:on
	};

	// I/O context
	context_t &ctx;

	// @formatter:off
	int                hndl;
	const uint8_t      *rawDatabase;                // base location of mmap segment
	fileHeader_t       fileHeader;                  // file header
	uint32_t           creationFlags;               // creation constraints
	uint32_t           allocFlags;                  // memory constraints
	// transforms
	uint32_t           numTransform;                // number of elements in collection
	uint32_t           maxTransform;                // maximum size of collection
	uint64_t           *fwdTransformData;           // forward transform (binary)
	uint64_t           *revTransformData;           // reverse transform (binary)
	transformName_t    *fwdTransformNames;          // forward transform (string)
	transformName_t    *revTransformNames;          // reverse transform (string)
	uint32_t           *revTransformIds;            // reverse transform (id)
	uint32_t           transformIndexSize;          // index size (must be prime)
	uint32_t           *fwdTransformNameIndex;      // fwdTransformNames index
	uint32_t           *revTransformNameIndex;      // revTransformNames index
	// signature store
	uint32_t           numSignature;                // number of signatures
	uint32_t           maxSignature;                // maximum size of collection
	signature_t        *signatures;                 // signature collection
	uint32_t           signatureIndexSize;          // index size (must be prime)
	uint32_t           *signatureIndex;             // index
	// swap store
	uint32_t           numSwap;                     // number of swaps
	uint32_t           maxSwap;                     // maximum size of collection
	swap_t             *swaps;                      // swap collection
	uint32_t           swapIndexSize;               // index size (must be prime)
	uint32_t           *swapIndex;                  // index
	// hint store
	uint32_t           numHint;                     // number of hints
	uint32_t           maxHint;                     // maximum size of collection
	hint_t             *hints;                      // hint collection
	uint32_t           hintIndexSize;               // index size (must be prime)
	uint32_t           *hintIndex;                  // index
	// imprint store
	uint32_t           interleave;                  // imprint interleave factor (display value)
	uint32_t           interleaveStep;              // imprint interleave factor (interleave distance)
	uint32_t           numImprint;                  // number of elements in collection
	uint32_t           maxImprint;                  // maximum size of collection
	imprint_t          *imprints;                   // imprint collection
	uint32_t           imprintIndexSize;            // index size (must be prime)
	uint32_t           *imprintIndex;               // index
	// member store
	uint32_t           numMember;                   // number of members
	uint32_t           maxMember;                   // maximum size of collection
	member_t           *members;                    // member collection
	uint32_t           memberIndexSize;             // index size (must be prime)
	uint32_t           *memberIndex;                // index
	// versioned memory
	uint32_t           iVersion;                    // version current incarnation
	uint32_t           *imprintVersion;             // versioned memory for `imprintIndex`
	uint32_t           *signatureVersion;           // versioned memory for `signatureIndex`
	// @formatter:on

	/**
	 * Constructor
	 */
	database_t(context_t &ctx) : ctx(ctx) {
		// copy user flags+debug settings
		creationFlags = 0;

		hndl        = 0;
		rawDatabase = NULL;
		::memset(&fileHeader, 0, sizeof(fileHeader));
		allocFlags = 0;

		// transform store
		numTransform          = 0;
		maxTransform          = 0;
		transformIndexSize    = 0;
		fwdTransformData      = revTransformData      = NULL;
		fwdTransformNames     = revTransformNames     = NULL;
		fwdTransformNameIndex = revTransformNameIndex = NULL;
		revTransformIds       = NULL;

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

		// hint store
		numHint       = 0;
		maxHint       = 0;
		hints         = NULL;
		hintIndexSize = 0;
		hintIndex     = NULL;

		// imprint store
		interleave       = 1;
		interleaveStep   = 1;
		numImprint       = 0;
		maxImprint       = 0;
		imprints         = NULL;
		imprintIndexSize = 0;
		imprintIndex     = NULL;

		// member store
		numMember       = 0;
		maxMember       = 0;
		members         = NULL;
		memberIndexSize = 0;
		memberIndex     = NULL;

		// versioned memory
		iVersion         = 0;
		imprintVersion   = NULL;
		signatureVersion = NULL;
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
		if (allocFlags & ALLOCMASK_SIGNATURE)
			ctx.myFree("database_t::signatures", signatures);
		if (allocFlags & ALLOCMASK_SIGNATUREINDEX)
			ctx.myFree("database_t::signatureIndex", signatureIndex);
		if (allocFlags & ALLOCMASK_SWAP)
			ctx.myFree("database_t::swaps", swaps);
		if (allocFlags & ALLOCMASK_SWAPINDEX)
			ctx.myFree("database_t::swapIndex", swapIndex);
		if (allocFlags & ALLOCMASK_HINT)
			ctx.myFree("database_t::hints", hints);
		if (allocFlags & ALLOCMASK_HINTINDEX)
			ctx.myFree("database_t::hintIndex", hintIndex);
		if (allocFlags & ALLOCMASK_IMPRINT)
			ctx.myFree("database_t::imprints", imprints);
		if (allocFlags & ALLOCMASK_IMPRINTINDEX)
			ctx.myFree("database_t::imprintIndex", imprintIndex);
		if (allocFlags & ALLOCMASK_MEMBER)
			ctx.myFree("database_t::members", members);
		if (allocFlags & ALLOCMASK_MEMBERINDEX)
			ctx.myFree("database_t::memberIndex", memberIndex);

		// release versioned memory
		disableVersioned();

		/*
		 * Release resources
		 */
		if (hndl) {
			/*
			 * Database was opened with `mmap()`
			 */
			if (::munmap((void *) rawDatabase, fileHeader.offEnd))
				ctx.fatal("\n{\"error\":\"munmap()\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", __FUNCTION__, __FILE__, __LINE__);
			if (::close(hndl))
				ctx.fatal("\n{\"error\":\"close()\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", __FUNCTION__, __FILE__, __LINE__);
		} else if (rawDatabase) {
			/*
			 * Database was loaded with `read()`
			 */
			ctx.myFree("database_t::rawDatabase", (void *) rawDatabase);
		}
	}

	/**
	 * @date 2020-04-17 00:54:09
	 *
	 * Enable versioned memory for selected indices
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
	 * Inherit read-only sections from an source database.
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

		// signature store
		if (inheritSections & (ALLOCMASK_SIGNATURE | ALLOCMASK_SIGNATUREINDEX)) {
			if (pFrom->numSignature == 0)
				ctx.fatal("\n{\"error\":\"Missing signature section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_SIGNATURE) {
				assert(~allocFlags & ALLOCMASK_SIGNATURE);
				this->maxSignature = pFrom->maxSignature;
				this->numSignature = pFrom->numSignature;
				this->signatures   = pFrom->signatures;
			}

			if (inheritSections & ALLOCMASK_SIGNATUREINDEX) {
				assert(~allocFlags & ALLOCMASK_SIGNATUREINDEX);
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
				assert(~allocFlags & ALLOCMASK_SWAP);
				this->maxSwap = pFrom->maxSwap;
				this->numSwap = pFrom->numSwap;
				this->swaps   = pFrom->swaps;
			}

			if (inheritSections & ALLOCMASK_SWAPINDEX) {
				assert(~allocFlags & ALLOCMASK_SWAPINDEX);
				this->swapIndexSize = pFrom->swapIndexSize;
				this->swapIndex     = pFrom->swapIndex;
			}
		}

		// hint store
		if (inheritSections & (ALLOCMASK_HINT | ALLOCMASK_HINTINDEX)) {
			if (pFrom->numHint == 0)
				ctx.fatal("\n{\"error\":\"Missing hint section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_HINT) {
				assert(~allocFlags & ALLOCMASK_HINT);
				this->maxHint = pFrom->maxHint;
				this->numHint = pFrom->numHint;
				this->hints   = pFrom->hints;
			}

			if (inheritSections & ALLOCMASK_HINTINDEX) {
				assert(~allocFlags & ALLOCMASK_HINTINDEX);
				this->hintIndexSize = pFrom->hintIndexSize;
				this->hintIndex     = pFrom->hintIndex;
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
				assert(~allocFlags & ALLOCMASK_IMPRINT);
				this->maxImprint = pFrom->maxImprint;
				this->numImprint = pFrom->numImprint;
				this->imprints   = pFrom->imprints;
			}

			if (inheritSections & ALLOCMASK_IMPRINTINDEX) {
				assert(~allocFlags & ALLOCMASK_IMPRINTINDEX);
				this->imprintIndexSize = pFrom->imprintIndexSize;
				this->imprintIndex     = pFrom->imprintIndex;
			}
		}

		// member store
		if (inheritSections & (ALLOCMASK_MEMBER | ALLOCMASK_MEMBERINDEX)) {
			if (pFrom->numMember == 0)
				ctx.fatal("\n{\"error\":\"Missing member section\",\"where\":\"%s:%s:%d\",\"database\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			if (inheritSections & ALLOCMASK_MEMBER) {
				assert(~allocFlags & ALLOCMASK_MEMBER);
				this->maxMember = pFrom->maxMember;
				this->numMember = pFrom->numMember;
				this->members   = pFrom->members;
			}

			if (inheritSections & ALLOCMASK_MEMBERINDEX) {
				assert(~allocFlags & ALLOCMASK_MEMBERINDEX);
				this->memberIndexSize = pFrom->memberIndexSize;
				this->memberIndex     = pFrom->memberIndex;
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
		if (maxTransform && (~excludeSections & ALLOCMASK_TRANSFORM)) {
			memUsage += maxTransform * sizeof(*this->fwdTransformData);
			memUsage += maxTransform * sizeof(*this->revTransformData);
			memUsage += maxTransform * sizeof(*this->fwdTransformNames);
			memUsage += maxTransform * sizeof(*this->revTransformNames);
			memUsage += maxTransform * sizeof(*this->revTransformIds);
			memUsage += transformIndexSize * sizeof(*fwdTransformNameIndex);
			memUsage += transformIndexSize * sizeof(*revTransformNameIndex);
		}

		// signature store
		if (maxSignature && (~excludeSections & ALLOCMASK_SIGNATURE))
			memUsage += maxSignature * sizeof(*signatures); // increase with 5%
		if (signatureIndexSize && (~excludeSections & ALLOCMASK_SIGNATUREINDEX))
			memUsage += signatureIndexSize * sizeof(*signatureIndex);

		// swap store
		if (maxSwap && (~excludeSections & ALLOCMASK_SWAP))
			memUsage += maxSwap * sizeof(*swaps); // increase with 5%
		if (swapIndexSize && (~excludeSections & ALLOCMASK_SWAPINDEX))
			memUsage += swapIndexSize * sizeof(*swapIndex);

		// hint store
		if (maxHint && (~excludeSections & ALLOCMASK_HINT))
			memUsage += maxHint * sizeof(*hints); // increase with 5%
		if (hintIndexSize && (~excludeSections & ALLOCMASK_HINTINDEX))
			memUsage += hintIndexSize * sizeof(*hintIndex);

		// imprint store
		if (maxImprint && (~excludeSections & ALLOCMASK_IMPRINT))
			memUsage += maxImprint * sizeof(*imprints); // increase with 5%
		if (imprintIndexSize && (~excludeSections & ALLOCMASK_IMPRINTINDEX))
			memUsage += imprintIndexSize * sizeof(*imprintIndex);

		// member store
		if (maxMember && (~excludeSections & ALLOCMASK_MEMBER))
			memUsage += maxMember * sizeof(*members); // increase with 5%
		if (memberIndexSize && (~excludeSections & ALLOCMASK_MEMBERINDEX))
			memUsage += memberIndexSize * sizeof(*memberIndex);

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

		// transform store
		if (maxTransform && (~excludeSections & ALLOCMASK_TRANSFORM)) {
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

		// signature store
		if (maxSignature && (~excludeSections & ALLOCMASK_SIGNATURE)) {
			// increase with 5%
			maxSignature = maxSignature;
			numSignature = 1; // do not start at 1
			signatures   = (signature_t *) ctx.myAlloc("database_t::signatures", maxSignature, sizeof(*signatures));
			allocFlags |= ALLOCMASK_SIGNATURE;
		}
		if (signatureIndexSize && (~excludeSections & ALLOCMASK_SIGNATUREINDEX)) {
			assert(ctx.isPrime(signatureIndexSize));
			signatureIndex = (uint32_t *) ctx.myAlloc("database_t::signatureIndex", signatureIndexSize, sizeof(*signatureIndex));
			allocFlags |= ALLOCMASK_SIGNATUREINDEX;
		}

		// swap store
		if (maxSwap && (~excludeSections & ALLOCMASK_SWAP)) {
			// increase with 5%
			maxSwap = maxSwap;
			numSwap = 1; // do not start at 1
			swaps   = (swap_t *) ctx.myAlloc("database_t::swaps", maxSwap, sizeof(*swaps));
			allocFlags |= ALLOCMASK_SWAP;
		}
		if (swapIndexSize && (~excludeSections & ALLOCMASK_SWAPINDEX)) {
			assert(ctx.isPrime(swapIndexSize));
			swapIndex = (uint32_t *) ctx.myAlloc("database_t::swapIndex", swapIndexSize, sizeof(*swapIndex));
			allocFlags |= ALLOCMASK_SWAPINDEX;
		}

		// hint store
		if (maxHint && (~excludeSections & ALLOCMASK_HINT)) {
			// increase with 5%
			maxHint = maxHint;
			numHint = 1; // do not start at 1
			hints   = (hint_t *) ctx.myAlloc("database_t::hints", maxHint, sizeof(*hints));
			allocFlags |= ALLOCMASK_HINT;
		}
		if (hintIndexSize && (~excludeSections & ALLOCMASK_HINTINDEX)) {
			assert(ctx.isPrime(hintIndexSize));
			hintIndex = (uint32_t *) ctx.myAlloc("database_t::hintIndex", hintIndexSize, sizeof(*hintIndex));
			allocFlags |= ALLOCMASK_HINTINDEX;
		}

		// imprint store
		if (maxImprint && (~excludeSections & ALLOCMASK_IMPRINT)) {
			assert(interleave && interleaveStep);
			// increase with 5%
			maxImprint = maxImprint;
			numImprint = 1; // do not start at 1
			imprints   = (imprint_t *) ctx.myAlloc("database_t::imprints", maxImprint, sizeof(*imprints));
			allocFlags |= ALLOCMASK_IMPRINT;
		}
		if (imprintIndexSize && (~excludeSections & ALLOCMASK_IMPRINTINDEX)) {
			assert(ctx.isPrime(imprintIndexSize));
			imprintIndex = (uint32_t *) ctx.myAlloc("database_t::imprintIndex", imprintIndexSize, sizeof(*imprintIndex));
			allocFlags |= ALLOCMASK_IMPRINTINDEX;
		}

		// member store
		if (maxMember && (~excludeSections & ALLOCMASK_MEMBER)) {
			// increase with 5%
			maxMember = maxMember;
			numMember = 1; // do not start at 1
			members   = (member_t *) ctx.myAlloc("database_t::members", maxMember, sizeof(*members));
			allocFlags |= ALLOCMASK_MEMBER;
		}
		if (memberIndexSize && (~excludeSections & ALLOCMASK_MEMBERINDEX)) {
			assert(ctx.isPrime(memberIndexSize));
			memberIndex = (uint32_t *) ctx.myAlloc("database_t::memberIndex", memberIndexSize, sizeof(*memberIndex));
			allocFlags |= ALLOCMASK_MEMBERINDEX;
		}

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
	void open(const char *fileName, unsigned writable) {

		/*
		 * Open file
		 */
		hndl = ::open(fileName, O_RDONLY);
		if (hndl == -1)
			ctx.fatal("\n{\"error\":\"fopen('%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);

		struct stat sbuf;
		if (::fstat(hndl, &sbuf))
			ctx.fatal("\n{\"error\":\"fstat('%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);

#if defined(HAVE_MMAP)
		/*
		 * Load using mmap()
		 */
		void *pMemory;

		if (writable) {
			pMemory = ::mmap(NULL, (size_t) sbuf.st_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_NORESERVE, hndl, 0);
			if (pMemory == MAP_FAILED)
				ctx.fatal("\n{\"error\":\"mmap(PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_NORESERVE,'%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);
		} else {
			pMemory = ::mmap(NULL, (size_t) sbuf.st_size, PROT_READ, MAP_SHARED | MAP_NORESERVE, hndl, 0);
			if (pMemory == MAP_FAILED)
				ctx.fatal("\n{\"error\":\"mmap(PROT_READ,MAP_SHARED|MAP_NORESERVE,'%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);
		}

		// set memory usage preferances
		if (::madvise(pMemory, (size_t) sbuf.st_size, MADV_RANDOM))
			ctx.fatal("\n{\"error\":\"madvise(MADV_RANDOM,'%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);
		if (::madvise(pMemory, (size_t) sbuf.st_size, MADV_DONTDUMP))
			ctx.fatal("\n{\"error\":\"madvise(MADV_DONTDUMP,'%s')\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n", fileName, __FUNCTION__, __FILE__, __LINE__);

		rawDatabase = (const uint8_t *) pMemory;
#else
		/*
		 * Load using read()
		 */

		/*
		 * Allocate storage
		 */
		rawDatabase = (uint8_t *) ctx.myAlloc("database_t::rawDatabase", 1, (size_t) sbuf.st_size);

		ctx.progressHi = (uint64_t) sbuf.st_size;
		ctx.progress = 0;

		readData(hndl, (uint8_t *) rawDatabase, (size_t) sbuf.st_size);

		/*
		 * Close
		 */
		::close(hndl);
		hndl = 0;
#endif

		::memcpy(&fileHeader, rawDatabase, sizeof(fileHeader));
		if (fileHeader.magic != FILE_MAGIC)
			ctx.fatal("\n{\"error\":\"db version mismatch\",\"where\":\"%s:%s:%d\",\"encountered\":\"%08x\",\"expected\":\"%08x\"}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic, FILE_MAGIC);
		if (fileHeader.magic_maxSlots != MAXSLOTS)
			ctx.fatal("\n{\"error\":\"db magic_maxslots\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_maxSlots, MAXSLOTS);
		if (fileHeader.offEnd != (uint64_t) sbuf.st_size)
			ctx.fatal("\n{\"error\":\"db size mismatch\",\"where\":\"%s:%s:%d\",\"encountered\":\"%lu\",\"expected\":\"%lu\"}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.offEnd, sbuf.st_size);
		if (fileHeader.magic_sizeofSignature != sizeof(signature_t))
			ctx.fatal("\n{\"error\":\"db magic_sizeofSignature\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofSignature, (unsigned) sizeof(signature_t));
		if (fileHeader.magic_sizeofSwap != sizeof(swap_t))
			ctx.fatal("\n{\"error\":\"db magic_sizeofSwap\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofSwap, (unsigned) sizeof(swap_t));
		if (fileHeader.magic_sizeofHint != sizeof(hint_t))
			ctx.fatal("\n{\"error\":\"db magic_sizeofHint\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofHint, (unsigned) sizeof(hint_t));
		if (fileHeader.magic_sizeofImprint != sizeof(imprint_t))
			ctx.fatal("\n{\"error\":\"db magic_sizeofImprint\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofImprint, (unsigned) sizeof(imprint_t));
		if (fileHeader.magic_sizeofMember != sizeof(member_t))
			ctx.fatal("\n{\"error\":\"db magic_sizeofMember\",\"where\":\"%s:%s:%d\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, fileHeader.magic_sizeofMember, (unsigned) sizeof(member_t));

		creationFlags = fileHeader.magic_flags;

		/*
		 * map sections to starting positions in data
		 */

		// transforms
		maxTransform          = numTransform = fileHeader.numTransform;
		fwdTransformData      = (uint64_t *) (rawDatabase + fileHeader.offFwdTransforms);
		revTransformData      = (uint64_t *) (rawDatabase + fileHeader.offRevTransforms);
		fwdTransformNames     = (transformName_t *) (rawDatabase + fileHeader.offFwdTransformNames);
		revTransformNames     = (transformName_t *) (rawDatabase + fileHeader.offRevTransformNames);
		revTransformIds       = (uint32_t *) (rawDatabase + fileHeader.offRevTransformIds);
		transformIndexSize    = fileHeader.transformIndexSize;
		fwdTransformNameIndex = (uint32_t *) (rawDatabase + fileHeader.offFwdTransformNameIndex);
		revTransformNameIndex = (uint32_t *) (rawDatabase + fileHeader.offRevTransformNameIndex);

		// signatures
		maxSignature       = numSignature = fileHeader.numSignature;
		signatures         = (signature_t *) (rawDatabase + fileHeader.offSignatures);
		signatureIndexSize = fileHeader.signatureIndexSize;
		signatureIndex     = (uint32_t *) (rawDatabase + fileHeader.offSignatureIndex);

		// swap
		maxSwap       = numSwap = fileHeader.numSwap;
		swaps         = (swap_t *) (rawDatabase + fileHeader.offSwaps);
		swapIndexSize = fileHeader.swapIndexSize;
		swapIndex     = (uint32_t *) (rawDatabase + fileHeader.offSwapIndex);

		// hint
		maxHint       = numHint       = fileHeader.numHint;
		hints         = (hint_t *) (rawDatabase + fileHeader.offHints);
		hintIndexSize = fileHeader.hintIndexSize;
		hintIndex     = (uint32_t *) (rawDatabase + fileHeader.offHintIndex);

		// imprints
		interleave       = fileHeader.interleave;
		interleaveStep   = fileHeader.interleaveStep;
		maxImprint       = numImprint = fileHeader.numImprint;
		imprints         = (imprint_t *) (rawDatabase + fileHeader.offImprints);
		imprintIndexSize = fileHeader.imprintIndexSize;
		imprintIndex     = (uint32_t *) (rawDatabase + fileHeader.offImprintIndex);

		// members
		maxMember       = numMember = fileHeader.numMember;
		members         = (member_t *) (rawDatabase + fileHeader.offMember);
		memberIndexSize = fileHeader.memberIndexSize;
		memberIndex     = (uint32_t *) (rawDatabase + fileHeader.offMemberIndex);
	};

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
		 * Quick cvalculate file size
		 */
		ctx.progressHi = align32(sizeof(fileHeader));
		ctx.progressHi += align32(sizeof(*this->fwdTransformData) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->revTransformData) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->fwdTransformNames) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->revTransformNames) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->revTransformIds) * this->numTransform);
		ctx.progressHi += align32(sizeof(*this->fwdTransformNameIndex * this->transformIndexSize));
		ctx.progressHi += align32(sizeof(*this->revTransformNameIndex * this->transformIndexSize));
		ctx.progressHi += align32(sizeof(*this->signatures) * this->numSignature);
		ctx.progressHi += align32(sizeof(*this->signatureIndex) * this->signatureIndexSize);
		ctx.progressHi += align32(sizeof(*this->swaps) * this->numSwap);
		ctx.progressHi += align32(sizeof(*this->swapIndex) * this->swapIndexSize);
		ctx.progressHi += align32(sizeof(*this->hints) * this->numHint);
		ctx.progressHi += align32(sizeof(*this->hintIndex) * this->hintIndexSize);
		ctx.progressHi += align32(sizeof(*this->imprints) * this->numImprint);
		ctx.progressHi += align32(sizeof(*this->imprintIndex) * this->imprintIndexSize);
		ctx.progressHi += align32(sizeof(*this->members) * this->numMember);
		ctx.progressHi += align32(sizeof(*this->memberIndex) * this->memberIndexSize);
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

		flen += writeData(outf, &fileHeader, align32(sizeof(fileHeader)), fileName);

		/*
		 * write transforms
		 */
		if (this->numTransform) {
			fileHeader.numTransform = this->numTransform;

			// write forward/reverse transforms
			fileHeader.offFwdTransforms = flen;
			flen += writeData(outf, this->fwdTransformData, align32(sizeof(*this->fwdTransformData) * this->numTransform), fileName);
			fileHeader.offRevTransforms = flen;
			flen += writeData(outf, this->revTransformData, align32(sizeof(*this->revTransformData) * this->numTransform), fileName);

			// write forward/reverse names
			fileHeader.offFwdTransformNames = flen;
			flen += writeData(outf, this->fwdTransformNames, align32(sizeof(*this->fwdTransformNames) * this->numTransform), fileName);
			fileHeader.offRevTransformNames = flen;
			flen += writeData(outf, this->revTransformNames, align32(sizeof(*this->revTransformNames) * this->numTransform), fileName);

			// write inverted skins
			fileHeader.offRevTransformIds = flen;
			flen += writeData(outf, this->revTransformIds, align32(sizeof(*this->revTransformIds) * this->numTransform), fileName);

			// write index
			if (transformIndexSize) {
				fileHeader.transformIndexSize = this->transformIndexSize;

				// write index
				fileHeader.offFwdTransformNameIndex = flen;
				flen += writeData(outf, this->fwdTransformNameIndex, align32(sizeof(*this->fwdTransformNameIndex) * this->transformIndexSize), fileName);
				fileHeader.offRevTransformNameIndex = flen;
				flen += writeData(outf, this->revTransformNameIndex, align32(sizeof(*this->revTransformNameIndex) * this->transformIndexSize), fileName);
			}
		}

		/*
		 * write signatures
		 */
		if (this->numSignature) {
			// first entry must be zero
			signature_t zero;
			::memset(&zero, 0, sizeof(zero));
			assert(::memcmp(this->signatures, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numSignature  = this->numSignature;
			fileHeader.offSignatures = flen;
			flen += writeData(outf, this->signatures, align32(sizeof(*this->signatures) * this->numSignature), fileName);
			if (this->signatureIndexSize) {
				// Index
				fileHeader.signatureIndexSize = this->signatureIndexSize;
				fileHeader.offSignatureIndex  = flen;
				flen += writeData(outf, this->signatureIndex, align32(sizeof(*this->signatureIndex) * this->signatureIndexSize), fileName);
			}
		}

		/*
		 * write swaps
		 */
		if (this->numSwap) {
			// first entry must be zero
			swap_t zero;
			::memset(&zero, 0, sizeof(zero));
			assert(::memcmp(this->swaps, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numSwap  = this->numSwap;
			fileHeader.offSwaps = flen;
			flen += writeData(outf, this->swaps, align32(sizeof(*this->swaps) * this->numSwap), fileName);
			if (this->swapIndexSize) {
				// Index
				fileHeader.swapIndexSize = this->swapIndexSize;
				fileHeader.offSwapIndex  = flen;
				flen += writeData(outf, this->swapIndex, align32(sizeof(*this->swapIndex) * this->swapIndexSize), fileName);
			}
		}

		/*
		 * write hints
		 */
		if (this->numHint) {
			// first entry must be zero
			hint_t zero;
			::memset(&zero, 0, sizeof(zero));
			assert(::memcmp(this->hints, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numHint  = this->numHint;
			fileHeader.offHints = flen;
			flen += writeData(outf, this->hints, align32(sizeof(*this->hints) * this->numHint), fileName);
			if (this->hintIndexSize) {
				// Index
				fileHeader.hintIndexSize = this->hintIndexSize;
				fileHeader.offHintIndex  = flen;
				flen += writeData(outf, this->hintIndex, align32(sizeof(*this->hintIndex) * this->hintIndexSize), fileName);
			}
		}

		/*
		 * write imprints
		 */
		if (this->numImprint) {
			fileHeader.interleave     = interleave;
			fileHeader.interleaveStep = interleaveStep;

			// first entry must be zero
			imprint_t zero;
			::memset(&zero, 0, sizeof(zero));
			assert(::memcmp(this->imprints, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numImprint  = this->numImprint;
			fileHeader.offImprints = flen;
			flen += writeData(outf, this->imprints, align32(sizeof(*this->imprints) * this->numImprint), fileName);
			if (this->imprintIndexSize) {
				// Index
				fileHeader.imprintIndexSize = this->imprintIndexSize;
				fileHeader.offImprintIndex  = flen;
				flen += writeData(outf, this->imprintIndex, align32(sizeof(*this->imprintIndex) * this->imprintIndexSize), fileName);
			}
		}

		/*
		 * write members
		 */
		if (this->numMember) {
			// first entry must be zero
			member_t zero;
			::memset(&zero, 0, sizeof(zero));
			assert(::memcmp(this->members, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numMember = this->numMember;
			fileHeader.offMember = flen;
			flen += writeData(outf, this->members, align32(sizeof(*this->members) * this->numMember), fileName);
			if (this->memberIndexSize) {
				// Index
				fileHeader.memberIndexSize = this->memberIndexSize;
				fileHeader.offMemberIndex  = flen;
				flen += writeData(outf, this->memberIndex, align32(sizeof(*this->memberIndex) * this->memberIndexSize), fileName);
			}
		}

		/*
		 * Rewrite header and close
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[Kclosing");

		fileHeader.magic                 = FILE_MAGIC;
		fileHeader.magic_flags           = ctx.flags;
		fileHeader.magic_maxSlots        = MAXSLOTS;
		fileHeader.magic_sizeofSignature = sizeof(signature_t);
		fileHeader.magic_sizeofSwap      = sizeof(swap_t);
		fileHeader.magic_sizeofHint      = sizeof(hint_t);
		fileHeader.magic_sizeofImprint   = sizeof(imprint_t);
		fileHeader.magic_sizeofMember    = sizeof(member_t);
		fileHeader.offEnd                = flen;

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
	uint64_t writeData(FILE *outf, const void *data, size_t dataLength, const char *fileName) {

		// write in chunks of 1024*1024 bytes

		size_t written = 0;
		while (dataLength > 0) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				fprintf(stderr, "\r\e[K%.5f%%", ctx.progress * 100.0 / ctx.progressHi);
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
		 * Quad align
		 */
		dataLength = 8U - (written & 7U);
		if (dataLength > 0) {
			uint8_t zero8[8] = {0, 0, 0, 0, 0, 0, 0, 0};

			fwrite(zero8, dataLength, 1, outf);
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
	inline unsigned lookupTransform(const char *pName, uint32_t *pIndex) {
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
		else if (~pos & IBIT)
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
	inline unsigned lookupTransformName(const char *pName, const char *pSkin, uint32_t *pIndex) {
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
		else if (~pos & IBIT)
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
	inline unsigned lookupTransformSlot(const char *pName, const char *pSkin, uint32_t *pIndex) {
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
		else if (~pos & IBIT)
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
	inline unsigned lookupFwdTransform(const char *pName) {
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
	inline unsigned lookupRevTransform(const char *pName) {
		return lookupTransform(pName, this->revTransformNameIndex);
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
	inline unsigned lookupSignature(const char *name) {
		ctx.cntHash++;

		// calculate starting position
		unsigned crc32 = 0;

		for (const char *pName = name; *pName; pName++)
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));

		unsigned ix   = crc32 % signatureIndexSize;
		unsigned bump = ix;
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
	inline unsigned addSignature(const char *name) {
		signature_t *pSignature = this->signatures + this->numSignature++;

		if (this->numSignature > this->maxSignature)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxSignature\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxSignature);

		// clear before use
		::memset(pSignature, 0, sizeof(*pSignature));

		// only populate key fields
		strcpy(pSignature->name, name);

		return (unsigned) (pSignature - this->signatures);
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
	inline unsigned lookupSwap(const swap_t *pSwap) {
		ctx.cntHash++;

		// calculate starting position
		unsigned crc32 = 0;

		for (unsigned j = 0; j < swap_t::MAXENTRY; j++)
			crc32 = __builtin_ia32_crc32si(crc32, pSwap->tids[j]);

		unsigned ix   = crc32 % swapIndexSize;
		unsigned bump = ix;
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
	inline unsigned addSwap(swap_t *pSwap) {
		unsigned swapId = this->numSwap++;

		if (this->numSwap > this->maxSwap)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxSwap\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxSwap);

		::memcpy(&this->swaps[swapId], pSwap, sizeof(*pSwap));

		return swapId;
	}

	/*
	 * Hint store
	 */

	/**
	 * @date 2020-04-19 20:52:11
	 *
	 * Perform hint lookup
	 *
	 * Lookup key in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the hint.
	 *
	 * @param v {hint_t} v - key value
	 * @return {number} offset into index
	 */
	inline unsigned lookupHint(const hint_t *pHint) {
		ctx.cntHash++;

		// calculate starting position
		unsigned crc32 = 0;

		for (unsigned j = 0; j < hint_t::MAXENTRY; j++)
			crc32 = __builtin_ia32_crc32si(crc32, pHint->numStored[j]);

		unsigned ix   = crc32 % hintIndexSize;
		unsigned bump = ix;
		if (bump == 0)
			bump = hintIndexSize - 1; // may never be zero
		if (bump > 2147000041)
			bump = 2147000041; // may never exceed last 32bit prime

		for (;;) {
			ctx.cntCompare++;
			if (this->hintIndex[ix] == 0)
				return ix; // "not-found"

			if (this->hints[this->hintIndex[ix]].equals(*pHint))
				return ix; // "found"

			// overflow, jump to next entry
			// if `ix` and `bump` are both 31 bit values, then the addition will never overflow
			ix += bump;
			if (ix >= hintIndexSize)
				ix -= hintIndexSize;
		}
	}

	/**
 	 * Add a new hint to the dataset
 	 *
	 * @param v {hint_t} v - key value
	 * @return {number} hintId
	 */
	inline unsigned addHint(hint_t *pHint) {
		unsigned hintId = this->numHint++;

		if (this->numHint > this->maxHint)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxHint\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxHint);

		::memcpy(&this->hints[hintId], pHint, sizeof(*pHint));

		return hintId;
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
	inline unsigned lookupImprint(const footprint_t &v) const {

		ctx.cntHash++;

		// starting position
		unsigned crc = v.crc32();

		unsigned ix = crc % imprintIndexSize;

		// increment when overflowing
		unsigned bump = ix;
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
	inline unsigned addImprint(const footprint_t &v) {
		imprint_t *pImprint = this->imprints + this->numImprint++;

		if (this->numImprint > this->maxImprint)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxImprint\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxImprint);

		// only populate key fields
		pImprint->footprint = v;

		return (unsigned) (pImprint - this->imprints);
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
	inline bool lookupImprintAssociative(const tinyTree_t *pTree, footprint_t *pFwdEvaluator, footprint_t *pRevEvaluator, unsigned *sid, unsigned *tid) {
		/*
		 * According to `performSelfTestInterleave` the following is true:
	         *   fwdTransform[row + col] == fwdTransform[row][fwdTransform[col]]
	         *   revTransform[row][fwdTransform[row + col]] == fwdTransform[col]
		 */

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
				unsigned ix = this->lookupImprint(v[pTree->root]);

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
				unsigned ix = this->lookupImprint(v[pTree->root]);

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
	 * Typically code looks like:
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
	inline unsigned addImprintAssociative(const tinyTree_t *pTree, footprint_t *pFwdEvaluator, footprint_t *pRevEvaluator, unsigned sid) {
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
				unsigned ix = this->lookupImprint(v[pTree->root]);

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
				unsigned ix = this->lookupImprint(v[pTree->root]);

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
	inline unsigned lookupMember(const char *name) {
		ctx.cntHash++;

		// calculate starting position
		unsigned        crc32  = 0;
		for (const char *pName = name; *pName; pName++)
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));

		unsigned ix   = crc32 % memberIndexSize;
		unsigned bump = ix;
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
	inline unsigned addMember(const char *name) {
		member_t *pMember = this->members + this->numMember++;

		if (this->numMember > this->maxMember)
			ctx.fatal("\n{\"error\":\"storage full\",\"where\":\"%s:%s:%d\",\"maxMember\":%u}\n", __FUNCTION__, __FILE__, __LINE__, this->maxMember);

		// clear before use
		::memset(pMember, 0, sizeof(*pMember));

		// only populate key fields
		strcpy(pMember->name, name);

		return (unsigned) (pMember - this->members);
	}

	/**
	 * @date 2020-04-20 23:03:50
	 *
	 * Rebuild indices when settings changes makes them invalid
	 *
	 * @param {number} sections - set of sections to reindex
	 */
	void rebuildIndices(unsigned sections) {
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Rebuilding indices\n", ctx.timeAsString());

		// reset ticker
		uint64_t numProgress = 0;
		if (sections & ALLOCMASK_SIGNATUREINDEX)
			numProgress += this->numSignature;
		if (sections & ALLOCMASK_SWAPINDEX)
			numProgress += this->numSwap;
		if (sections & ALLOCMASK_HINTINDEX)
			numProgress += this->numHint;
		if (sections & ALLOCMASK_IMPRINTINDEX)
			numProgress += this->numImprint;
		if (sections & ALLOCMASK_MEMBERINDEX)
			numProgress += this->numMember;
		ctx.setupSpeed(numProgress);
		ctx.tick = 0;

		/*
		 * Signatures
		 */

		if (sections & ALLOCMASK_SIGNATUREINDEX) {
			::memset(this->signatureIndex, 0, this->signatureIndexSize * sizeof(*this->signatureIndex));

			for (unsigned iSid = 1; iSid < this->numSignature; iSid++) {
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

				unsigned ix = this->lookupSignature(pSignature->name);
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
			for (unsigned iSwap = 1; iSwap < this->numSwap; iSwap++) {
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

				unsigned ix = this->lookupSwap(pSwap);
				assert(this->swapIndex[ix] == 0);
				this->swapIndex[ix] = iSwap;

				ctx.progress++;
			}
		}

		/*
		 * Hints
		 */

		if (sections & ALLOCMASK_HINTINDEX) {
			// clear
			::memset(this->hintIndex, 0, this->hintIndexSize * sizeof(*this->hintIndex));

			// rebuild
			for (unsigned iHint = 1; iHint < this->numHint; iHint++) {
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

				const hint_t *pHint = this->hints + iHint;

				unsigned ix = this->lookupHint(pHint);
				assert(this->hintIndex[ix] == 0);
				this->hintIndex[ix] = iHint;

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
			for (unsigned iImprint = 1; iImprint < this->numImprint; iImprint++) {
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

				unsigned ix = this->lookupImprint(pImprint->footprint);
				assert(this->imprintIndex[ix] == 0);
				this->imprintIndex[ix] = iImprint;

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
			for (unsigned iMember = 1; iMember < this->numMember; iMember++) {
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

				unsigned ix = this->lookupMember(pMember->name);
				assert(this->memberIndex[ix] == 0);
				this->memberIndex[ix] = iMember;

				ctx.progress++;
			}
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] Indices built\n", ctx.timeAsString());
	}

	char *sectionToText(unsigned sections, char *pBuffer = NULL) {
		static char buffer[512];
		if (pBuffer == NULL)
			pBuffer = buffer;

		*pBuffer = 0;

		if (sections & ALLOCMASK_TRANSFORM) {
			::strcat(pBuffer, "transform");
			sections &= ~ALLOCMASK_TRANSFORM;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_SIGNATURE) {
			::strcat(pBuffer, "signature");
			sections &= ~ALLOCMASK_SIGNATURE;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_SIGNATUREINDEX) {
			::strcat(pBuffer, "signatureIndex");
			sections &= ~ALLOCMASK_SIGNATUREINDEX;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_SWAP) {
			::strcat(pBuffer, "swap");
			sections &= ~ALLOCMASK_SWAP;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_SWAPINDEX) {
			::strcat(pBuffer, "swapIndex");
			sections &= ~ALLOCMASK_SWAPINDEX;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_HINT) {
			::strcat(pBuffer, "hint");
			sections &= ~ALLOCMASK_HINT;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_HINTINDEX) {
			::strcat(pBuffer, "hintIndex");
			sections &= ~ALLOCMASK_HINTINDEX;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_IMPRINT) {
			::strcat(pBuffer, "imprint");
			sections &= ~ALLOCMASK_IMPRINT;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_IMPRINTINDEX) {
			::strcat(pBuffer, "imprintIndex");
			sections &= ~ALLOCMASK_IMPRINTINDEX;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_MEMBER) {
			::strcat(pBuffer, "member");
			sections &= ~ALLOCMASK_MEMBER;
			if (sections)
				::strcat(pBuffer, "|");
		}
		if (sections & ALLOCMASK_MEMBERINDEX) {
			::strcat(pBuffer, "memberIndex");
			sections &= ~ALLOCMASK_MEMBERINDEX;
		}

		return pBuffer;
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
		json_object_set_new_nocheck(jResult, "numTransform", json_integer(this->numTransform));
		json_object_set_new_nocheck(jResult, "transformIndexSize", json_integer(this->transformIndexSize));
		json_object_set_new_nocheck(jResult, "numSignature", json_integer(this->numSignature));
		json_object_set_new_nocheck(jResult, "signatureIndexSize", json_integer(this->signatureIndexSize));
		json_object_set_new_nocheck(jResult, "numSwap", json_integer(this->numSwap));
		json_object_set_new_nocheck(jResult, "swapIndexSize", json_integer(this->swapIndexSize));
		json_object_set_new_nocheck(jResult, "numHint", json_integer(this->numHint));
		json_object_set_new_nocheck(jResult, "hintIndexSize", json_integer(this->hintIndexSize));
		json_object_set_new_nocheck(jResult, "interleave", json_integer(this->interleave));
		json_object_set_new_nocheck(jResult, "numImprint", json_integer(this->numImprint));
		json_object_set_new_nocheck(jResult, "imprintIndexSize", json_integer(this->imprintIndexSize));
		json_object_set_new_nocheck(jResult, "numMember", json_integer(this->numMember));
		json_object_set_new_nocheck(jResult, "memberIndexSize", json_integer(this->memberIndexSize));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader.offEnd));

		return jResult;
	}

};

#endif
