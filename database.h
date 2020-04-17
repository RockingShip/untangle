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

#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "datadef.h"
#include "tinytree.h"
#include "config.h"

#if defined(ENABLE_JANSSON)
#include "jansson.h"
#endif

/// @constant {number} FILE_MAGIC - Database version. Update this when either the file header or one of the structures change
#define FILE_MAGIC        0x20200402

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
	uint32_t magic_sizeofImprint;
	uint32_t magic_sizeofSignature;
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
	uint32_t numImprint;
	uint32_t imprintIndexSize;
	uint32_t numSignature;
	uint32_t signatureIndexSize;
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
	uint64_t offImprints;
	uint64_t offImprintIndex;
	uint64_t offSignatures;
	uint64_t offSignatureIndex;
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
	// imprint store
	uint32_t           interleave;                  // imprint interleave factor (display value)
	uint32_t           interleaveStep;              // imprint interleave factor (interleave distance)
	uint32_t           numImprint;                  // number of elements in collection
	uint32_t           maxImprint;                  // maximum size of collection
	imprint_t          *imprints;                   // imprint collection
	uint32_t           imprintIndexSize;            // index size (must be prime)
	uint32_t           *imprintIndex;               // index
	// signature store
	uint32_t           numSignature;                // number of signatures
	uint32_t           maxSignature;                // maximum size of collection
	signature_t        *signatures;                 // signature collection
	uint32_t           signatureIndexSize;          // index size (must be prime)
	uint32_t           *signatureIndex;             // index
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

		hndl = 0;
		rawDatabase = NULL;
		::memset(&fileHeader, 0, sizeof(fileHeader));
		allocFlags = 0;

		// transform store
		numTransform = 0;
		maxTransform = 0;
		transformIndexSize = 0;
		fwdTransformData = revTransformData = NULL;
		fwdTransformNames = revTransformNames = NULL;
		fwdTransformNameIndex = revTransformNameIndex = NULL;
		revTransformIds = NULL;

		// imprint store
		interleave = 0;
		interleaveStep = 0;
		numImprint = 0;
		maxImprint = 0;
		imprints = NULL;
		imprintIndexSize = 0;
		imprintIndex = NULL;

		// signature store
		numSignature = 0;
		maxSignature = 0;
		signatures = NULL;
		signatureIndexSize = 0;
		signatureIndex = NULL;

		// member store
		numMember = 0;
		maxMember = 0;
		members = NULL;
		memberIndexSize = 0;
		memberIndex = NULL;

		// versioned memory
		iVersion = 0;
		imprintVersion = NULL;
		signatureVersion = NULL;
	};

	/**
	 * @date 2020-03-12 15:19:50
	 *
	 * Runtime flags to indicate which sections were allocated. If not then they are read-only mmapped.
	 */
	enum {
		ALLOCFLAG_TRANSFORM = 0,
		ALLOCFLAG_IMPRINT,
		ALLOCFLAG_SIGNATURE,
		ALLOCFLAG_MEMBER,

		// @formatter:off
		ALLOCMASK_TRANSFORM          = 1 << ALLOCFLAG_TRANSFORM,
		ALLOCMASK_IMPRINT            = 1 << ALLOCFLAG_IMPRINT,
		ALLOCMASK_SIGNATURE          = 1 << ALLOCFLAG_SIGNATURE,
		ALLOCMASK_MEMBER             = 1 << ALLOCFLAG_MEMBER,
		// @formatter:on
	};

	/**
	 * @date 2020-03-12 16:05:37
	 *
	 * Create read-write database as memory store
	 *
         * @param {context_t} ctx - I/O context 
         * @param {databaseArguments_t} userArguments - creation arguments
	 */
	void create(void) {

		// transform store
		if (maxTransform) {
			assert(maxTransform == MAXTRANSFORM);
			fwdTransformData = (uint64_t *) ctx.myAlloc("database_t::fwdTransformData", maxTransform, sizeof(*this->fwdTransformData));
			revTransformData = (uint64_t *) ctx.myAlloc("database_t::revTransformData", maxTransform, sizeof(*this->revTransformData));
			fwdTransformNames = (transformName_t *) ctx.myAlloc("database_t::fwdTransformNames", maxTransform, sizeof(*this->fwdTransformNames));
			revTransformNames = (transformName_t *) ctx.myAlloc("database_t::revTransformNames", maxTransform, sizeof(*this->revTransformNames));
			revTransformIds = (uint32_t *) ctx.myAlloc("database_t::revTransformIds", maxTransform, sizeof(*this->revTransformIds));
			fwdTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::fwdTransformNameIndex", transformIndexSize, sizeof(*fwdTransformNameIndex));
			revTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::revTransformNameIndex", transformIndexSize, sizeof(*revTransformNameIndex));
			allocFlags |= ALLOCMASK_TRANSFORM;
		}

		// imprint store
		if (maxImprint) {
			// increase with 5%
			if (maxImprint < UINT32_MAX - maxImprint / 20)
				maxImprint += maxImprint / 20;

			assert(interleave && interleaveStep);
			assert(ctx.isPrime(imprintIndexSize));
			numImprint = 1; // do not start at 1
			imprints = (imprint_t *) ctx.myAlloc("database_t::imprints", maxImprint, sizeof(*imprints));
			imprintIndex = (uint32_t *) ctx.myAlloc("database_t::imprintIndex", imprintIndexSize, sizeof(*imprintIndex));
			allocFlags |= ALLOCMASK_IMPRINT;
		}

		// signature store
		if (maxSignature) {
			// increase with 5%
			if (maxSignature < UINT32_MAX - maxSignature / 20)
				maxSignature += maxSignature / 20;

			assert(ctx.isPrime(signatureIndexSize));
			numSignature = 1; // do not start at 1
			signatures = (signature_t *) ctx.myAlloc("database_t::signatures", maxSignature, sizeof(*signatures));
			signatureIndex = (uint32_t *) ctx.myAlloc("database_t::signatureIndex", signatureIndexSize, sizeof(*signatureIndex));
			allocFlags |= ALLOCMASK_SIGNATURE;
		}

		// member store
		if (maxMember) {
			// increase with 5%
			if (maxMember < UINT32_MAX - maxMember / 20)
				maxMember += maxMember / 20;

			assert(ctx.isPrime(memberIndexSize));
			numMember = 1; // do not start at 1
			members = (member_t *) ctx.myAlloc("database_t::members", maxMember, sizeof(*members));
			memberIndex = (uint32_t *) ctx.myAlloc("database_t::memberIndex", memberIndexSize, sizeof(*memberIndex));
			allocFlags |= ALLOCMASK_MEMBER;
		}

	};


	/**
	 * @date 2020-03-15 22:25:41
	 *
	 * Inherit read-only sections from an source database.
	 *
	 * NOTE: call after calling `create()`
	 *
	 * @param {database_t} pDatabase - Database to inherit from
	 * @param {string} pName - Name of database
	 * @param {number} sections - set of sections to inherit
	 */
	void inheritSections(const database_t *pDatabase, const char *pName, uint32_t sections) {

		// transform store
		if (sections & database_t::ALLOCMASK_TRANSFORM) {
			if (pDatabase->numTransform == 0) {
				printf("{\"error\":\"Missing transform section\",\"where\":\"%s\",\"database\":\"%s\"}\n",
				       __FUNCTION__, pName);
				exit(1);
			}

			assert(maxTransform == 0);
			maxTransform = pDatabase->maxTransform;
			numTransform = pDatabase->numTransform;

			fwdTransformData = pDatabase->fwdTransformData;
			revTransformData = pDatabase->revTransformData;
			fwdTransformNames = pDatabase->fwdTransformNames;
			revTransformNames = pDatabase->revTransformNames;
			revTransformIds = pDatabase->revTransformIds;

			assert(transformIndexSize == 0);
			transformIndexSize = pDatabase->transformIndexSize;

			fwdTransformNameIndex = pDatabase->fwdTransformNameIndex;
			revTransformNameIndex = pDatabase->revTransformNameIndex;
		}

		// imprint store
		if (sections & database_t::ALLOCMASK_IMPRINT) {
			if (pDatabase->numImprint == 0) {
				printf("{\"error\":\"Missing imprint section\",\"where\":\"%s\",\"database\":\"%s\"}\n",
				       __FUNCTION__, pName);
				exit(1);
			}

			this->interleave = pDatabase->interleave;
			this->interleaveStep = pDatabase->interleaveStep;

			assert(maxImprint == 0);
			this->maxImprint = pDatabase->maxImprint;
			this->numImprint = pDatabase->numImprint;
			this->imprints = pDatabase->imprints;

			this->imprintIndexSize = pDatabase->imprintIndexSize;
			this->imprintIndex = pDatabase->imprintIndex;
		}

		// signature store
		if (sections & database_t::ALLOCMASK_SIGNATURE) {
			if (pDatabase->numSignature == 0) {
				printf("{\"error\":\"Missing signature section\",\"where\":\"%s\",\"database\":\"%s\"}\n",
				       __FUNCTION__, pName);
				exit(1);
			}

			assert(maxSignature == 0);
			this->maxSignature = pDatabase->maxSignature;
			this->numSignature = pDatabase->numSignature;
			this->signatures = pDatabase->signatures;

			this->signatureIndexSize = pDatabase->signatureIndexSize;
			this->signatureIndex = pDatabase->signatureIndex;
		}

		// member store
		if (sections & database_t::ALLOCMASK_MEMBER) {
			if (pDatabase->numMember == 0) {
				printf("{\"error\":\"Missing member section\",\"where\":\"%s\",\"database\":\"%s\"}\n",
				       __FUNCTION__, pName);
				exit(1);
			}

			assert(maxMember == 0);
			this->maxMember = pDatabase->maxMember;
			this->numMember = pDatabase->numMember;
			this->members = pDatabase->members;

			this->memberIndexSize = pDatabase->memberIndexSize;
			this->memberIndex = pDatabase->memberIndex;
		}
	}

	/**
	 * @date 2020-03-12 16:07:44
	 *
	 * Create read-only database mmapped onto file
	 *
         * @param {string} fileName - database filename
         * @param {boolean} shared - `false` to `read()`, `true` to `mmap()`
	 */
	void open(const char *fileName, bool shared) {

		/*
		 * Open file
		 */
		hndl = ::open(fileName, O_RDONLY);
		if (hndl == -1)
			ctx.fatal("fopen(\"%s\",\"r\") returned: %m\n", fileName);

		struct stat sbuf;
		if (::fstat(hndl, &sbuf))
			ctx.fatal("fstat(\"%s\") returned: %m\n", fileName);

		if (shared) {
			/*
			 * Load using mmap()
			 */
			void *pMemory = ::mmap(NULL, (size_t) sbuf.st_size, PROT_READ, MAP_SHARED | MAP_NORESERVE, hndl, 0);
			if (pMemory == MAP_FAILED)
				ctx.fatal("mmap(PROT_READ, MAP_SHARED|MAP_NORESERVE,%s) returned: %m\n", fileName);

			// set memory usage preferances
			if (::madvise(pMemory, (size_t) sbuf.st_size, MADV_RANDOM))
				ctx.fatal("madvise(MADV_RANDOM) returned: %m\n");
			if (::madvise(pMemory, (size_t) sbuf.st_size, MADV_DONTDUMP))
				ctx.fatal("madvise(MADV_DONTDUMP) returned: %m\n");

			rawDatabase = (const uint8_t *) pMemory;
		} else {
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
		}

		::memcpy(&fileHeader, rawDatabase, sizeof(fileHeader));
		if (fileHeader.magic != FILE_MAGIC)
			ctx.fatal("db version missmatch. Encountered %08x, Expected %08x, \n", fileHeader.magic, FILE_MAGIC);
		if (fileHeader.magic_maxSlots != MAXSLOTS)
			ctx.fatal("db magic_maxslots. Encountered %d, Expected %d\n", fileHeader.magic_maxSlots, MAXSLOTS);
		if (fileHeader.offEnd != (uint64_t) sbuf.st_size)
			ctx.fatal("db size missmatch. Encountered %lu, Expected %lu\n", fileHeader.offEnd, (uint64_t) sbuf.st_size);
		if (fileHeader.magic_sizeofImprint != sizeof(imprint_t))
			ctx.fatal("db magic_sizeofImprint. Encountered %d, Expected %ld\n", fileHeader.magic_sizeofImprint, sizeof(imprint_t));
		if (fileHeader.magic_sizeofSignature != sizeof(signature_t))
			ctx.fatal("db magic_sizeofSignature. Encountered %d, Expected %ld\n", fileHeader.magic_sizeofSignature, sizeof(signature_t));
		if (fileHeader.magic_sizeofMember != sizeof(member_t))
			ctx.fatal("db magic_sizeofMember. Encountered %d, Expected %ld\n", fileHeader.magic_sizeofMember, sizeof(member_t));

		creationFlags = fileHeader.magic_flags;

		/*
		 * map sections to starting positions in data
		 */

		// transforms
		maxTransform = numTransform = fileHeader.numTransform;
		fwdTransformData = (uint64_t *) (rawDatabase + fileHeader.offFwdTransforms);
		revTransformData = (uint64_t *) (rawDatabase + fileHeader.offRevTransforms);
		fwdTransformNames = (transformName_t *) (rawDatabase + fileHeader.offFwdTransformNames);
		revTransformNames = (transformName_t *) (rawDatabase + fileHeader.offRevTransformNames);
		revTransformIds = (uint32_t *) (rawDatabase + fileHeader.offRevTransformIds);
		transformIndexSize = fileHeader.transformIndexSize;
		fwdTransformNameIndex = (uint32_t *) (rawDatabase + fileHeader.offFwdTransformNameIndex);
		revTransformNameIndex = (uint32_t *) (rawDatabase + fileHeader.offRevTransformNameIndex);

		// imprints
		interleave = fileHeader.interleave;
		interleaveStep = fileHeader.interleaveStep;
		maxImprint = numImprint = fileHeader.numImprint;
		imprints = (imprint_t *) (rawDatabase + fileHeader.offImprints);
		imprintIndexSize = fileHeader.imprintIndexSize;
		imprintIndex = (uint32_t *) (rawDatabase + fileHeader.offImprintIndex);

		// signatures
		maxSignature = numSignature = fileHeader.numSignature;
		signatures = (signature_t *) (rawDatabase + fileHeader.offSignatures);
		signatureIndexSize = fileHeader.signatureIndexSize;
		signatureIndex = (uint32_t *) (rawDatabase + fileHeader.offSignatureIndex);

		// members
		maxMember = numMember = fileHeader.numMember;
		members = (member_t *) (rawDatabase + fileHeader.offMember);
		memberIndexSize = fileHeader.memberIndexSize;
		memberIndex = (uint32_t *) (rawDatabase + fileHeader.offMemberIndex);
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
		if (allocFlags & ALLOCMASK_IMPRINT) {
			ctx.myFree("database_t::imprints", imprints);
			ctx.myFree("database_t::imprintIndex", imprintIndex);
			if (imprintVersion)
				ctx.myFree("database_t::imprintVersion", imprintVersion);
		}
		if (allocFlags & ALLOCMASK_SIGNATURE) {
			ctx.myFree("database_t::signatures", signatures);
			ctx.myFree("database_t::signatureIndex", signatureIndex);
			if (signatureVersion)
				ctx.myFree("database_t::signatureVersion", signatureVersion);
		}
		if (allocFlags & ALLOCMASK_MEMBER) {
			ctx.myFree("database_t::members", members);
			ctx.myFree("database_t::memberIndex", memberIndex);
		}

		/*
		 * Release resources
		 */
		if (hndl) {
			/*
			 * Database was opened with `mmap()`
			 */
			if (::munmap((void *) rawDatabase, fileHeader.offEnd))
				ctx.fatal("munmap() returned: %m\n");
			if (::close(hndl))
				ctx.fatal("close() returned: %m\n");
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
	inline void enabledVersioned(void) {

		// allocate version indices
		if (allocFlags & ALLOCMASK_IMPRINT)
			imprintVersion = (uint32_t *) ctx.myAlloc("database_t::imprintVersion", imprintIndexSize, sizeof(*imprintVersion));
		if (allocFlags & ALLOCMASK_SIGNATURE)
			signatureVersion = (uint32_t *) ctx.myAlloc("database_t::signatureVersion", signatureIndexSize, sizeof(*signatureVersion));

		// clear versioned memory
		iVersion = 0;
		InvalidateVersioned();
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
				::memset(imprintVersion, 0, (sizeof(*imprintVersion) * imprintIndexSize));
			if (signatureVersion)
				::memset(signatureVersion, 0, (sizeof(*signatureVersion) * signatureIndexSize));
		}

		// bump version number.
		iVersion++;
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
		ctx.progressHi += align32(sizeof(*this->imprints) * this->numImprint);
		ctx.progressHi += align32(sizeof(*this->imprintIndex) * this->imprintIndexSize);
		ctx.progressHi += align32(sizeof(*this->signatures) * this->numSignature);
		ctx.progressHi += align32(sizeof(*this->signatureIndex) * this->signatureIndexSize);
		ctx.progressHi += align32(sizeof(*this->members) * this->numMember);
		ctx.progressHi += align32(sizeof(*this->memberIndex) * this->memberIndexSize);
		ctx.progress = 0;
		ctx.tick = 0;

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Writing %s\n", ctx.timeAsString(), fileName);

		/*
		 * Open output file
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[Kopening");

		FILE *outf = fopen(fileName, "w");
		if (!outf)
			ctx.fatal("Failed to open %s: %m\n", fileName);

		/*
		 * Write empty header (overwritten later)
		 */
		uint64_t flen = 0;

		flen += writeData(outf, &fileHeader, align32(sizeof(fileHeader)));

		/*
		 * write transforms
		 */
		if (this->numTransform) {
			fileHeader.numTransform = this->numTransform;

			// write forward/reverse transforms
			fileHeader.offFwdTransforms = flen;
			flen += writeData(outf, this->fwdTransformData, align32(sizeof(*this->fwdTransformData) * this->numTransform));
			fileHeader.offRevTransforms = flen;
			flen += writeData(outf, this->revTransformData, align32(sizeof(*this->revTransformData) * this->numTransform));

			// write forward/reverse names
			fileHeader.offFwdTransformNames = flen;
			flen += writeData(outf, this->fwdTransformNames, align32(sizeof(*this->fwdTransformNames) * this->numTransform));
			fileHeader.offRevTransformNames = flen;
			flen += writeData(outf, this->revTransformNames, align32(sizeof(*this->revTransformNames) * this->numTransform));

			// write inverted skins
			fileHeader.offRevTransformIds = flen;
			flen += writeData(outf, this->revTransformIds, align32(sizeof(*this->revTransformIds) * this->numTransform));

			// write index
			if (transformIndexSize) {
				fileHeader.transformIndexSize = this->transformIndexSize;

				// write index
				fileHeader.offFwdTransformNameIndex = flen;
				flen += writeData(outf, this->fwdTransformNameIndex, align32(sizeof(*this->fwdTransformNameIndex) * this->transformIndexSize));
				fileHeader.offRevTransformNameIndex = flen;
				flen += writeData(outf, this->revTransformNameIndex, align32(sizeof(*this->revTransformNameIndex) * this->transformIndexSize));
			}
		}

		/*
		 * write imprints
		 */
		if (this->numImprint) {
			fileHeader.interleave = interleave;
			fileHeader.interleaveStep = interleaveStep;

			// first entry must be zero
			imprint_t zero;
			::memset(&zero, 0, sizeof(zero));
			assert(::memcmp(this->imprints, &zero, sizeof(zero)) == 0);

			// collection
			fileHeader.numImprint = this->numImprint;
			fileHeader.offImprints = flen;
			flen += writeData(outf, this->imprints, align32(sizeof(*this->imprints) * this->numImprint));
			if (this->imprintIndexSize) {
				// Index
				fileHeader.imprintIndexSize = this->imprintIndexSize;
				fileHeader.offImprintIndex = flen;
				flen += writeData(outf, this->imprintIndex, align32(sizeof(*this->imprintIndex) * this->imprintIndexSize));
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
			fileHeader.numSignature = this->numSignature;
			fileHeader.offSignatures = flen;
			flen += writeData(outf, this->signatures, align32(sizeof(*this->signatures) * this->numSignature));
			if (this->signatureIndexSize) {
				// Index
				fileHeader.signatureIndexSize = this->signatureIndexSize;
				fileHeader.offSignatureIndex = flen;
				flen += writeData(outf, this->signatureIndex, align32(sizeof(*this->signatureIndex) * this->signatureIndexSize));
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
			flen += writeData(outf, this->members, align32(sizeof(*this->members) * this->numMember));
			if (this->memberIndexSize) {
				// Index
				fileHeader.memberIndexSize = this->memberIndexSize;
				fileHeader.offMemberIndex = flen;
				flen += writeData(outf, this->memberIndex, align32(sizeof(*this->memberIndex) * this->memberIndexSize));
			}
		}

		/*
		 * Rewrite header and close
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[Kclosing");

		fileHeader.magic = FILE_MAGIC;
		fileHeader.magic_flags = ctx.flags;
		fileHeader.magic_maxSlots = MAXSLOTS;
		fileHeader.magic_sizeofImprint = sizeof(imprint_t);
		fileHeader.magic_sizeofSignature = sizeof(signature_t);
		fileHeader.magic_sizeofMember = sizeof(member_t);
		fileHeader.offEnd = flen;

		// rewrite header
		fseek(outf, 0, SEEK_SET);
		fwrite(&fileHeader, sizeof(fileHeader), 1, outf);

		// test for errors, most likely disk-full
		if (feof(outf) || ferror(outf)) {
			::remove(fileName);
			ctx.fatal("[ferror(%s,\"w\") returned: %m]\n", fileName);
		}

		// close
		if (fclose(outf)) {
			::remove(fileName);
			ctx.fatal("[fclose(%s,\"w\") returned: %m]\n", fileName);
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
	 * @param {number} dataLength = how much to read
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
			if ((uint64_t) ::read(hndl, data, sliceLength) != sliceLength)
				ctx.fatal("[Failed to read %lu bytes: %m]\n", sliceLength);

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
	 * @param {number} dataLength = how much to write
	 * @return {number} total number of bytes written
	 */
	uint64_t writeData(FILE *outf, const void *data, size_t dataLength) {

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
			if (fwrite(data, sliceLength, 1, outf) != 1)
				ctx.fatal("[Failed to write %lu bytes: %m]\n", sliceLength);

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
	 * @return {uint32_t} - Transform enumeration id or `IBIT` if "not-found"
	 */
	inline uint32_t lookupTransform(const char *pName, uint32_t *pIndex) {
		assert(pIndex);

		// starting position in index
		uint32_t pos = MAXSLOTS + 1;

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
 	 * @return {uint32_t} - Transform enumeration id or `IBIT` if "not-found"
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
  	 * @return {uint32_t} - Transform enumeration id or `IBIT` if "not-found"
  	 */
	inline uint32_t lookupRevTransform(const char *pName) {
		return lookupTransform(pName, this->revTransformNameIndex);
	}

	/*
	 * Imprint store
	 */

	/**
	 * @date 2020-03-15 20:07:14
	 *
	 * Lookup value in index using a hash array with overflow.
	 * Returns the offset within the index.
	 * If contents of index is 0, then not found, otherwise it the index where to find the data in `pImprint`.
	 *
	 * @param v {footprint_t} v - value to index
	 * @return {number} offset into index
	 */
	inline uint32_t lookupImprint(const footprint_t &v) const {

		ctx.cntHash++;

		// starting position
		uint32_t crc = v.crc32();

		uint32_t ix = crc % imprintIndexSize;

		// increment when overflowing
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
	 * @param v {footprint_t} v - value to index
	 * @return {number} imprint id which should be stored in the index.
	 */
	inline uint32_t addImprint(const footprint_t &v) {
		imprint_t *pImprint = this->imprints + this->numImprint++;

		if (this->numImprint > this->maxImprint)
			ctx.fatal("\n[%s %s:%u storage full %d]\n", __FUNCTION__, __FILE__, __LINE__, this->maxImprint);

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
	 * @param {uint32_t} sid - found structure id
	 * @param {uint32_t} tid - found transform id. what was queried can be reconstructed as `"sid/tid"`
	 * @return {boolean} - `true` if found, `false` if not.
	 */
	inline bool lookupImprintAssociative(const tinyTree_t *pTree, footprint_t *pFwdEvaluator, footprint_t *pRevEvaluator, uint32_t *sid, uint32_t *tid) {
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
			for (uint32_t iRow = 0; iRow < MAXTRANSFORM; iRow += this->interleaveStep) {

				// find where the evaluator for the key is located in the evaluator store
				footprint_t *v = pRevEvaluator + iRow * tinyTree_t::TINYTREE_NEND;

				// apply the reverse transform
				pTree->eval(v);

				// search the resulting footprint in the cache/index
				uint32_t ix = this->lookupImprint(v[pTree->root]);

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
			 */
			footprint_t *v = pFwdEvaluator;

			// permutate all colums
			for (unsigned iCol = 0; iCol < interleaveStep; iCol++) {

				// apply the tree to the store
				pTree->eval(v);

				// search the resulting footprint in the cache/index
				uint32_t ix = this->lookupImprint(v[pTree->root]);

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
	 * @param {tinyTree_t} pTree - Tree containg expression
	 * @param {footprint_t[]} pFwdEvaluator - Evaluator with forward transforms (modified)
	 * @param {footprint_t[]} RevEvaluator - Evaluator with reverse transforms (modified)
	 * @param {uint32_t} sid - structure id to attach to imprints.
	 */
	inline void addImprintAssociative(const tinyTree_t *pTree, footprint_t *pFwdEvaluator, footprint_t *pRevEvaluator, uint32_t sid) {
		/*
		 * According to `performSelfTestInterleave` the following is true:
	         *   fwdTransform[row + col] == fwdTransform[row][fwdTransform[col]]
	         *   revTransform[row][fwdTransform[row + col]] == fwdTransform[col]
		 */
		if (this->interleave == this->interleaveStep) {
			/*
			 * index is populated with key rows, runtime scans cols
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
					this->imprintIndex[ix] = this->addImprint(v[pTree->root]);
					if (this->imprintVersion)
						this->imprintVersion[ix] = iVersion;

					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// populate non-key fields
					pImprint->sid = sid;
					pImprint->tid = iCol;
				} else {
					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// test for similar. First imprint must be unique, others must have matching sid
					if (iCol == 0 || pImprint->sid != sid) {
						printf("{\"error\":\"index entry already in use\",\"where\":\"%s\",\"newsid\":\"%d\",\"newtid\":\"%d\",\"oldsid\":\"%d\",\"oldtid\":\"%d\",\"newname\":\"%s\",\"newname\":\"%s\"}\n",
						       __FUNCTION__, sid, iCol, pImprint->sid, pImprint->tid, this->signatures[pImprint->sid].name, this->signatures[sid].name);
						exit(1);
					}
				}

				v += tinyTree_t::TINYTREE_NEND;
			}
		} else {
			/*
			 * index is populated with key cols, runtime scans rows
			 */
			// permutate rows
			for (uint32_t iRow = 0; iRow < MAXTRANSFORM; iRow += this->interleaveStep) {

				// find where the transform is located in the evaluator store
				footprint_t *v = pRevEvaluator + iRow * tinyTree_t::TINYTREE_NEND;

				// apply the forward transform
				pTree->eval(v);

				// search the resulting footprint in the cache/index
				uint32_t ix = this->lookupImprint(v[pTree->root]);

				// add to the database is not there
				if (this->imprintIndex[ix] == 0 || (this->imprintVersion != NULL && this->imprintVersion[ix] != iVersion)) {
					this->imprintIndex[ix] = this->addImprint(v[pTree->root]);
					if (this->imprintVersion)
						this->imprintVersion[ix] = iVersion;

					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// populate non-key fields
					pImprint->sid = sid;
					pImprint->tid = iRow;
				} else {
					imprint_t *pImprint = this->imprints + this->imprintIndex[ix];
					// test for similar. First imprint must be unique, others must have matching sid
					if (iRow == 0 || pImprint->sid != sid) {
						printf("{\"error\":\"index entry already in use\",\"where\":\"%s\",\"newsid\":\"%d\",\"newtid\":\"%d\",\"oldsid\":\"%d\",\"oldtid\":\"%d\",\"newname\":\"%s\",\"newname\":\"%s\"}\n",
						       __FUNCTION__, sid, iRow, pImprint->sid, pImprint->tid, this->signatures[pImprint->sid].name, this->signatures[sid].name);
						exit(1);
					}
				}
			}
		}
	}

	/*
	 * Signature store
	 */

	inline uint32_t lookupSignature(const char *name) {
		ctx.cntHash++;

		// calculate starting position
		uint32_t crc32 = 0;
		for (const char *pName = name; *pName; pName++)
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));

		uint32_t ix = crc32 % signatureIndexSize;
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

	inline uint32_t addSignature(const char *name) {
		signature_t *pSignature = this->signatures + this->numSignature++;

		if (this->numSignature > this->maxSignature)
			ctx.fatal("\n[%s %s:%u storage full %d]\n", __FUNCTION__, __FILE__, __LINE__, this->maxSignature);

		// clear before use
		::memset(pSignature, 0, sizeof(*pSignature));

		// only populate key fields
		strcpy(pSignature->name, name);

		return (uint32_t) (pSignature - this->signatures);
	}

	/*
	 * Member store
	 */

	inline uint32_t lookupMember(const char *name) {
		ctx.cntHash++;

		// calculate starting position
		uint32_t crc32 = 0;
		for (const char *pName = name; *pName; pName++)
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));

		uint32_t ix = crc32 % memberIndexSize;
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

	inline uint32_t addMember(const char *name) {
		member_t *pMember = this->members + this->numMember++;

		if (this->numMember > this->maxMember)
			ctx.fatal("\n[%s %s:%u storage full %d]\n", __FUNCTION__, __FILE__, __LINE__, this->maxMember);

		// clear before use
		::memset(pMember, 0, sizeof(*pMember));

		// only populate key fields
		strcpy(pMember->name, name);

		return (uint32_t) (pMember - this->members);
	}


#if defined(ENABLE_JANSSON)

	/**
	 * @date 2020-03-12 19:36:56
	 *
	 * Encode dimensions as json object
	 */
	json_t *jsonInfo(json_t *jResult) {
		if (jResult == NULL)
			jResult = json_object();
		json_object_set_new_nocheck(jResult, "flags", json_integer(this->creationFlags));
		json_object_set_new_nocheck(jResult, "interleave", json_integer(this->interleave));
		json_object_set_new_nocheck(jResult, "numTransform", json_integer(this->numTransform));
		json_object_set_new_nocheck(jResult, "transformIndexSize", json_integer(this->transformIndexSize));
		json_object_set_new_nocheck(jResult, "numImprint", json_integer(this->numImprint));
		json_object_set_new_nocheck(jResult, "imprintIndexSize", json_integer(this->imprintIndexSize));
		json_object_set_new_nocheck(jResult, "numSignature", json_integer(this->numSignature));
		json_object_set_new_nocheck(jResult, "signatureIndexSize", json_integer(this->signatureIndexSize));
		json_object_set_new_nocheck(jResult, "numMember", json_integer(this->numMember));
		json_object_set_new_nocheck(jResult, "memberIndexSize", json_integer(this->memberIndexSize));
		json_object_set_new_nocheck(jResult, "size", json_integer(fileHeader.offEnd));

		return jResult;
	}

#endif

};

#endif
