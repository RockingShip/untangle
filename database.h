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
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "context.h"

#include "config.h"

#if defined(ENABLE_JANSSON)
#include "jansson.h"
#endif

/// @constant {number} FILE_MAGIC - Database version. Update this when either the file header or one of the structures change
#define FILE_MAGIC        0x20200312

/*
 *  All components contributing and using the database should share the same dimensions
 */

/// @constant {number} IBIT - Which bit of the operand is reserved to flag that the result needs to be inverted
#define IBIT 0x80000000
/// @constant {number} MAXSLOTS - Transform/skin size
#define MAXSLOTS 9

/**
 * The database file header
 *
 * @typedef {object} FileHeader_t
 * @date 2020-03-12 15:09:50
 */
struct FileHeader_t {
	// environment metrics
	uint32_t magic;                  // magic+version
	uint32_t magic_flags;            // conditions it was created
	uint32_t magic_maxSlots;
	uint32_t magic_sizeofSignature;
	uint32_t magic_sizeofPatternFirst;
	uint32_t magic_sizeofPatternSecond;
	uint32_t magic_sizeofGrow;

	// optional metrics.
	uint32_t interleaveFactor;

	// section sizes
	uint32_t numTransforms;         // for both fwd/rev
	uint32_t transformIndexSize;    // for both fwd/rev
	uint32_t numSignatures;
	uint32_t signatureIndexSize;
	uint32_t numImprints;
	uint32_t imprintIndexSize;
	uint32_t numPatternFirst;
	uint32_t patternFirstIndexSize;
	uint32_t numPatternSecond;
	uint32_t patternSecondIndexSize;
	uint32_t numGrows;
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
	uint64_t offImprints;
	uint64_t offImprintIndex;
	uint64_t offPatternFirst;
	uint64_t offPatternFirstIndex;
	uint64_t offPatternSecond;
	uint64_t offPatternSecondIndex;
	uint64_t offGrows;
	uint64_t offGrowIndex;

	uint64_t offEnd;
};

/// @typedef {char[]} fixed size string containing transform
typedef char transformName_t[MAXSLOTS + 1];

/**
 * The *DATABASE*
 *
 * @date 2020-03-12 15:17:55
 */
struct database_t {

	// I/O context
	context_t &ctx;

	// @formatter:off
	int                hndl;
	const uint8_t      *rawDatabase;            // base location of mmap segment
	const FileHeader_t *dbHeader;               // segment header
	uint32_t           flags;                   // creation constraints
	uint32_t           allocFlags;              // memory constraints
	// transforms
	uint32_t           numTransform;            // number of transforms (9!)
	uint32_t           transformIndexSize;      // index size (must be prime)
	uint64_t           *fwdTransformData;       // forward transform (binary)
	uint64_t           *revTransformData;       // reverse transform (binary)
	transformName_t    *fwdTransformNames;      // forward transform (string)
	transformName_t    *revTransformNames;      // reverse transform (string)
	uint32_t           *revTransformIds;          // inverted skins
	uint32_t           *fwdTransformNameIndex;  // fwdTransformNames index
	uint32_t           *revTransformNameIndex;  // revTransformNames index
	// statistics
	uint64_t           progressHi;
	uint64_t           progress;
	// @formatter:on

	/**
	 * Constructor
	 */
	database_t(context_t &ctx) : ctx(ctx) {
		hndl = 0;
		rawDatabase = NULL;
		dbHeader = NULL;
		flags = 0;
		allocFlags = 0;

		numTransform = 0;
		transformIndexSize = 0;
		fwdTransformData = revTransformData = NULL;
		fwdTransformNames = revTransformNames = NULL;
		fwdTransformNameIndex = revTransformNameIndex = NULL;
		revTransformIds = NULL;

		progressHi = 0;
		progress = 0;
	};

	/**
	 * Runtime flags to indicate which sections were allocated. If not then they are read-only mmapped.
	 *
	 * @date 2020-03-12 15:19:50
	 */
	enum {
		ALLOCFLAG_TRANSFORM = 0,
		ALLOCFLAG_TRANSFORMINDEX,

		// @formatter:off
		ALLOCMASK_TRANSFORM      = 1 << ALLOCFLAG_TRANSFORM,
		ALLOCMASK_TRANSFORMINDEX = 1 << ALLOCFLAG_TRANSFORMINDEX,
		// @formatter:on
	};

	/**
	 * Create read-write database as memory store
	 *
         * @param {context_t} ctx - I/O context 
         * @param {databaseArguments_t} userArguments - creation arguments
	 * @date 2020-03-12 16:05:37
	 */
	void create(void) {
		if (numTransform) {
			fwdTransformData = (uint64_t *) ctx.myAlloc("database_t::fwdTransformData", numTransform, sizeof(*this->fwdTransformData));
			revTransformData = (uint64_t *) ctx.myAlloc("database_t::revTransformData", numTransform, sizeof(*this->revTransformData));
			fwdTransformNames = (transformName_t *) ctx.myAlloc("database_t::fwdTransformNames", numTransform, sizeof(*this->fwdTransformNames));
			revTransformNames = (transformName_t *) ctx.myAlloc("database_t::revTransformNames", numTransform, sizeof(*this->revTransformNames));
			revTransformIds = (uint32_t *) ctx.myAlloc("database_t::revTransformIds", numTransform, sizeof(*this->revTransformIds));
			allocFlags |= ALLOCMASK_TRANSFORM;
			if (transformIndexSize) {
				fwdTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::fwdTransformNameIndex", transformIndexSize, sizeof(*fwdTransformNameIndex));
				revTransformNameIndex = (uint32_t *) ctx.myAlloc("database_t::revTransformNameIndex", transformIndexSize, sizeof(*revTransformNameIndex));
				allocFlags |= ALLOCMASK_TRANSFORMINDEX;
			}
		}
	};


	/**
	 * Create read-only database mmapped onto file
	 *
         * @param {string} fileName - database filename
         * @param {boolean} shared - `false` to `read()`, `true` to `mmap()`
	 * @date 2020-03-12 16:07:44
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

			progressHi = (uint64_t) sbuf.st_size;
			progress = 0;

			readData(hndl, (uint8_t *) rawDatabase, (size_t) sbuf.st_size);

			/*
			 * Close
			 */
			::close(hndl);
			hndl = 0;
		}

		dbHeader = (FileHeader_t *) rawDatabase;
		if (dbHeader->magic != FILE_MAGIC)
			ctx.fatal("db version missmatch. Encountered %08x, Expected %08x, \n", dbHeader->magic, FILE_MAGIC);
		if (dbHeader->magic_maxSlots != MAXSLOTS)
			ctx.fatal("db magic_maxslots. Encountered %d, Expected %d\n", dbHeader->magic_maxSlots, MAXSLOTS);
		if (dbHeader->offEnd != (uint64_t) sbuf.st_size)
			ctx.fatal("db size missmatch. Encountered %lu, Expected %lu\n", dbHeader->offEnd, (uint64_t) sbuf.st_size);

		flags = dbHeader->magic_flags;

		/*
		 * map sections to starting positions in data
		 */

		// transforms
		numTransform = dbHeader->numTransforms;
		transformIndexSize = dbHeader->transformIndexSize;
		fwdTransformData = (uint64_t *) (rawDatabase + dbHeader->offFwdTransforms);
		revTransformData = (uint64_t *) (rawDatabase + dbHeader->offRevTransforms);
		fwdTransformNames = (transformName_t *) (rawDatabase + dbHeader->offFwdTransformNames);
		revTransformNames = (transformName_t *) (rawDatabase + dbHeader->offRevTransformNames);
		revTransformIds = (uint32_t *) (rawDatabase + dbHeader->offRevTransformIds);
		fwdTransformNameIndex = (uint32_t *) (rawDatabase + dbHeader->offFwdTransformNameIndex);
		revTransformNameIndex = (uint32_t *) (rawDatabase + dbHeader->offRevTransformNameIndex);
	};

	/*
	 * Release system resources
	 * @date 2020-03-12 15:57:37
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
		}
		if (allocFlags & ALLOCMASK_TRANSFORMINDEX) {
			ctx.myFree("database_t::fwdTransformNameIndex", fwdTransformNameIndex);
			ctx.myFree("database_t::revTransformNameIndex", revTransformNameIndex);
		}

		/*
		 * Release resources
		 */
		if (hndl) {
			/*
			 * Database was opened with `mmap()`
			 */
			if (::munmap((void *) rawDatabase, dbHeader->offEnd))
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
	 * Write database to file
	 *
	 * @param {string} fileName - File to write to
	 * @date 2020-03-12 16:04:30
	 */
	void save(const char *fileName) {

		// activate a local header
		static FileHeader_t fileHeader;
		dbHeader = &fileHeader;

		memset(&fileHeader, 0, sizeof(fileHeader));

		/*
		 * Quick cvalculate file size
		 */
		progressHi = sizeof(fileHeader);
		progressHi += sizeof(*this->fwdTransformData) * this->numTransform;
		progressHi += sizeof(*this->revTransformData) * this->numTransform;
		progressHi += sizeof(*this->fwdTransformNames) * this->numTransform;
		progressHi += sizeof(*this->revTransformNames) * this->numTransform;
		progressHi += sizeof(*this->revTransformIds) * this->numTransform;
		progressHi += sizeof(*this->fwdTransformNameIndex * this->transformIndexSize);
		progressHi += sizeof(*this->revTransformNameIndex * this->transformIndexSize);
		progress = 0;
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
		** Write empty header (overwritten later)
		*/
		fwrite(&fileHeader, sizeof(fileHeader), 1, outf);
		uint64_t flen = sizeof(fileHeader);

		/*
		 * write transforms
		 */
		if (this->numTransform) {
			fileHeader.numTransforms = this->numTransform;

			// write forward/reverse transforms
			fileHeader.offFwdTransforms = flen;
			flen += writeData(outf, this->fwdTransformData, sizeof(*this->fwdTransformData) * this->numTransform);
			fileHeader.offRevTransforms = flen;
			flen += writeData(outf, this->revTransformData, sizeof(*this->revTransformData) * this->numTransform);

			// write forward/reverse names
			fileHeader.offFwdTransformNames = flen;
			flen += writeData(outf, this->fwdTransformNames, sizeof(*this->fwdTransformNames) * this->numTransform);
			fileHeader.offRevTransformNames = flen;
			flen += writeData(outf, this->revTransformNames, sizeof(*this->revTransformNames) * this->numTransform);

			// write inverted skins
			fileHeader.offRevTransformIds = flen;
			flen += writeData(outf, this->revTransformIds, sizeof(*this->revTransformIds) * this->numTransform);

			// write index
			if (transformIndexSize) {
				// write index
				fileHeader.offFwdTransformNameIndex = flen;
				flen += writeData(outf, this->fwdTransformNameIndex, sizeof(*this->fwdTransformNameIndex) * this->transformIndexSize);
				fileHeader.offRevTransformNameIndex = flen;
				flen += writeData(outf, this->revTransformNameIndex, sizeof(*this->revTransformNameIndex) * this->transformIndexSize);
			}
		}

		/*
		 * Rewrite header and close
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[Kclosing");

		fileHeader.magic = FILE_MAGIC;
		fileHeader.magic_flags = this->flags;
		fileHeader.magic_maxSlots = MAXSLOTS;
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
	 * Read data from database file
	 * 
	 * @param {number} hndl - OS file handle 
	 * @param {void[]} data - Buffer to read to
	 * @param {number} dataLength = how much to read
	 * @date 2020-03-12 15:54:57
	 * @return {number} total number of bytes read
	 */
	uint64_t readData(int hndl, void *data, size_t dataLength) {

		// read in chunks of 1024*1024 bytes
		uint64_t sumRead = 0;
		while (dataLength > 0) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				fprintf(stderr, "\r\e[K%.5f%%", progress * 100.0 / progressHi);
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
			this->progress += sliceLength;
			sumRead += sliceLength;
		}

		return sumRead;
	}

	/**
	 * Write data to database file
	 * 
	 * @param {number} hndl - OS file handle 
	 * @param {void[]} data - Buffer to read to
	 * @param {number} dataLength = how much to write
	 * @date 2020-03-12 15:56:46
	 * @return {number} total number of bytes written
	 */
	uint64_t writeData(FILE *outf, const void *data, size_t dataLength) {

		// write in chunks of 1024*1024 bytes

		size_t written = 0;
		while (dataLength > 0) {
			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				fprintf(stderr, "\r\e[K%.5f%%", progress * 100.0 / progressHi);
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
			this->progress += sliceLength;
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
	 * @date 2020-03-12 10:28:05
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
	 * Lookup a forward transform name and return its matching enumeration id.
 	 *
	 * @param {string} pName - Transform name
 	 * @return {uint32_t} - Transform enumeration id or `IBIT` if "not-found"
 	 * @date 2020-03-13 14:20:29
 	 */
	inline uint32_t lookupFwdTransform(const char *pName) {
		return lookupTransform(pName, this->fwdTransformNameIndex);
	}

	/**
	 * Lookup a reverse transform name and return its matching enumeration id.
  	 *
 	 * @param {string} pName - Transform name
  	 * @return {uint32_t} - Transform enumeration id or `IBIT` if "not-found"
  	 * @date 2020-03-13 14:20:29
  	 */
	inline uint32_t lookupRevTransform(const char *pName) {
		return lookupTransform(pName, this->revTransformNameIndex);
	}

	/*
	 * @date 2020-03-12 19:36:56
	 *
	 * Transform store
	 */
#if defined(ENABLE_JANSSON)
	static json_t *headerInfo(json_t *jResult, const FileHeader_t *header) {
		if (jResult == NULL)
			jResult = json_object();
		json_object_set_new_nocheck(jResult, "flags", json_integer(header->magic_flags));
		json_object_set_new_nocheck(jResult, "maxSlots", json_integer(header->magic_maxSlots));
		json_object_set_new_nocheck(jResult, "numTransforms", json_integer(header->numTransforms));
		json_object_set_new_nocheck(jResult, "transformIndexSize", json_integer(header->transformIndexSize));
		json_object_set_new_nocheck(jResult, "size", json_integer(header->offEnd));

		return jResult;
	}
#endif

};

#endif
