#ifndef _DATADEF_H
#define _DATADEF_H

/*
 * @date 2020-03-15 20:20:35
 *
 * Collection of data definitions used to store footprints, signatures, patterns and more
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
#include "context.h"
#include <immintrin.h>

/*
 * Fixed length transform name
 *
 * @typedef {char[]} fixed size string containing transform
 */
typedef char transformName_t[MAXSLOTS + 1];

/**
 * @date 2020-03-06 23:23:32
 *
 * struct representing a 512 bit vector, each bit representing the outcome of the unified operator for every possible state 9 variables can take
 * The vector is split into a collection of 64bit wide words.
 *
 * Test vectors are also used to compare equality of two trees
 *
 * @typedef {number[]}
 */
struct footprint_t {
	enum {
		/// @constant {number} Size of footprint in terms of uint64_t
		QUADPERFOOTPRINT = ((1 << MAXSLOTS) / 64)
	};

	uint64_t bits[QUADPERFOOTPRINT]; // = 512/64 = 8 = QUADPERFOOTPRINT

	/**
	 * @date 2020-03-15 20:26:04
	 *
	 * Compare two prints and determine if both are same
	 *
	 * @param {footprint_t} rhs - right hand side of comparison
	 * @return {boolean} `true` if same, `false` if different
	 */
	inline bool equals(const struct footprint_t &rhs) const {

		/*
		 * @date 2020-04-14 21:22:52
		 * Update to SIMD
		 */

#if 0 && defined(__AVX2__)

		const __m256i *L = (const __m256i *) this->bits;
		const __m256i *R = (const __m256i *) rhs.bits;

		if (_mm256_movemask_epi8(_mm256_cmpeq_epi32(L[0], R[0])) != 0xffffffff) return false;
		if (_mm256_movemask_epi8(_mm256_cmpeq_epi32(L[1], R[1])) != 0xffffffff) return false;

#elif defined(__SSE2__)

		const __m128i *L = (const __m128i *) this->bits;
		const __m128i *R = (const __m128i *) rhs.bits;

		if (_mm_movemask_epi8(_mm_cmpeq_epi32(L[0], R[0])) != 0xffff) return false;
		if (_mm_movemask_epi8(_mm_cmpeq_epi32(L[1], R[1])) != 0xffff) return false;
		if (_mm_movemask_epi8(_mm_cmpeq_epi32(L[2], R[2])) != 0xffff) return false;
		if (_mm_movemask_epi8(_mm_cmpeq_epi32(L[3], R[3])) != 0xffff) return false;

#else
#warning non-assembler implementation

		// NOTE: QUADPERFOOTPRINT tests
		if (this->bits[0] != rhs.bits[0]) return false;
		if (this->bits[1] != rhs.bits[1]) return false;
		if (this->bits[2] != rhs.bits[2]) return false;
		if (this->bits[3] != rhs.bits[3]) return false;
		if (this->bits[4] != rhs.bits[4]) return false;
		if (this->bits[5] != rhs.bits[5]) return false;
		if (this->bits[6] != rhs.bits[6]) return false;
		if (this->bits[7] != rhs.bits[7]) return false;
#endif

		return true;
	}

	/**
	 * @date 2020-03-15 20:29:35
	 *
	 * Calculate the hash of a footprint
	 *
	 * It doesn't really have to be crc,  as long as the result has some linear distribution over index.
	 * crc32 was chosen because it has a single assembler instruction on x86 platforms.
	 *
	 * @return {number} - calculate crc
	 */
	inline unsigned crc32(void) const {

		// NOTE: QUADPERFOOTPRINT tests
#if defined(__SSE4_2__)
		uint32_t crc32 = 0;
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[0]);
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[1]);
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[2]);
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[3]);
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[4]);
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[5]);
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[6]);
		crc32 = __builtin_ia32_crc32di(crc32, this->bits[7]);
		return crc32;
#else
		uint64_t crc64 = 0;
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[0]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[1]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[2]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[3]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[4]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[5]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[6]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[7]));
		return crc64;
#endif

	}

};

/*
 * @date 2020-03-22 17:43:03
 *
 * Footprint belonging to signature/transform
 *
 * @date 2020-04-23 09:46:17
 *
 * Unsafe signatures/members will trigger level-4 normalisation because one of the components is unsafe.
 *
 */
struct signature_t {
	enum {
		/// @constant {number} (numnode*3+1+1/*root invert*/) For 5n9 signatures (4n9 is default) that would be 18
		SIGNATURENAMELENGTH = (6/*TINYTREE_MAXNODES*/ * 3 + 1 + 1),
		/// @constant {number} Number of slot folding entryes
		MAXFOLDS            = 36,
	};

	enum {
		SIGFLAG_SAFE     = 0, // It is safe to use the display name to reconstruct structures
		SIGFLAG_PROVIDES = 1, // this signature provides as an operand
		SIGFLAG_REQUIRED = 2, // this signature is used as an operand
		SIGFLAG_OPTIONAL = 3, // this signature is optional and may become empty during compacting

		// @formatter: off
		SIGMASK_SAFE     = 1 << SIGFLAG_SAFE,
		SIGMASK_PROVIDES = 1 << SIGFLAG_PROVIDES,
		SIGMASK_REQUIRED = 1 << SIGFLAG_REQUIRED,
		SIGMASK_OPTIONAL = 1 << SIGFLAG_OPTIONAL,
		// @formatter: on
	};

	/// @var {number} member id of first member in signature group
	uint32_t firstMember;

	/*
	 * the following are 16-bit values and align better here
	 */

	/// @var {number} index of `hints`
	uint16_t hintId;

	/// @var {number} index of `swaps`
	uint16_t swapId;

	/*
	 * the following are 8-bit values and align better if placed last
	 */

	/// @var {number} flags
	uint8_t flags;

	/// @var {number} size of tree in nodes
	uint8_t size;

	/// @var {number} number of unique endpoints
	uint8_t numPlaceholder;

	/// @var {number} number of endpoints
	uint8_t numEndpoint;

	/// @var {number} number of back-references
	uint8_t numBackRef;

	/// @var {string} Display name of signature. With space for inverted root and terminator
	char name[SIGNATURENAMELENGTH + 1];

	/*
 	 * Lookup algorithm:
	 * Entry offset for first occurrence [0, 1, 3, 6, 10, 15, 21, 28] (geometric series)
	 * Add to offset for second occurrence [0, 1, 2, 3, 4, 5, 6, 7, 8]
	 */

	struct fold_t {
		/// @var {number} Signature id after slots folded
		uint32_t sid;

		/// @var {number} Transform id to rebuild slots when folded
		uint32_t tid;
	};

	/// @var {number} results of slot folding
	fold_t folds[MAXFOLDS];

	/// @var {number} results of slot set to 0
	fold_t foldsZ[MAXSLOTS];

	/// @var {number} results of slot set to ~0
	fold_t foldsNZ[MAXSLOTS];
};

/*
 * @date 2020-05-04 13:14:11
 *
 * Swap instructions for level-5 normalisation.
 * Apply, compare and swap stored tids to normalise endpoints
 */
struct swap_t {

	enum {
		/// @constant {number} Maximum number of entries
		MAXENTRY = 10,
	};

	uint32_t tids[MAXENTRY];

	/**
	 * @date 2020-05-04 13:16:40
	 *
	 * Compare two swaps and determine if both are same
	 *
	 * @param {swap_t} rhs - right hand side of comparison
	 * @return {boolean} `true` if same, `false` if different
	 */
	inline bool equals(const struct swap_t &rhs) const {

		for (unsigned j = 0; j < MAXENTRY; j++) {
			if (this->tids[j] != rhs.tids[j])
				return false;
		}

		return true;
	}

};

/*
 * @date 2020-03-15 19:16:31
 *
 * Footprint belonging to signature/transform
 *
 * @date 2020-04-14 23:29:59
 *
 * Implementing SSE2/AVX2 requires alignment. Extra space of alignment reduces the maximum number of available imprints.
 */
struct imprint_t {
	footprint_t footprint; // footprint
	uint32_t sid;          // signature
	uint32_t tid;          // skin/transform
	uint32_t filler16[2];  // need 16 byte alignment for SSE
#if 0 && defined(__AVX2__)
	#warning AVX2 will give zero speed improvement and waste memory due to alignment
	uint32_t filler32[4];  // need 32 byte alignment
#endif
};

/*
 * @date 2021-07-08 21:13:03
 *
 * sid/tid or mid/tid pair for `member_t` heads and tails.
 */
struct pair_t {

	uint32_t id;
	uint32_t tid;

	/**
	 * @date 2021-07-08 21:17:14
	 *
	 * Compare two records and determine if both are same
	 *
	 * @param {pair_t} rhs - right hand side of comparison
	 * @return {boolean} `true` if same, `false` if different
	 */
	inline bool equals(uint32_t id, uint32_t tid) const {
		return this->id == id && this->tid == tid;
	}

};

/*
 * @date 2020-03-29 22:07:22
 *
 * Members of signature groups.
 *
 * Contains references to components and signatures
 */
struct member_t {

	enum {
		/// @constant {number} Maximum number of heads. bump when assertion indicates.
		MAXHEAD = 5,
	};

	enum {
		MEMFLAG_SAFE   = 0,  // Member is safe
		MEMFLAG_COMP   = 1,  // Member is a component
		MEMFLAG_LOCKED = 2,  // Member is required, may not be depreciated or deleted
		MEMFLAG_DEPR   = 3,  // Member is depreciated and should be ignored, used by `rewritedata[]` to force-rewrite into a better alternative
		MEMFLAG_DELETE = 4,  // Member is no-existent because one of the Q/T/F components is depreciated

		// @formatter: off
		MEMMASK_SAFE   = 1 << MEMFLAG_SAFE,
		MEMMASK_COMP   = 1 << MEMFLAG_COMP,
		MEMMASK_LOCKED = 1 << MEMFLAG_LOCKED,
		MEMMASK_DEPR   = 1 << MEMFLAG_DEPR,
		MEMMASK_DELETE = 1 << MEMFLAG_DELETE,
		// @formatter: on
	};

	/// @var {number} Signature id to which signature group this member belongs
	uint32_t sid;

	/// @var {number} Transform relative to signature
	uint32_t tid;

	/// @var {number} member id of `Q` component and transform
	uint32_t Qmt;

	/// @var {number} member id of `T` component and transform
	uint32_t Tmt;

	/// @var {number} member id of `F` component and transform
	uint32_t Fmt;

	/// @var {number} member id of next member in signature group
	uint32_t nextMember;

	/// @var {number} member id of heads
	uint32_t heads[MAXHEAD];

	/*
	 * the following are 8-bit values and align better if placed last
	 */

	/// @var {number} flags
	uint8_t flags;

	/// @var {number} size of tree in nodes
	uint8_t size;

	/// @var {number} number of unique endpoints
	uint8_t numPlaceholder;

	/// @var {number} number of endpoints
	uint8_t numEndpoint;

	/// @var {number} number of back-references
	uint8_t numBackRef;

	/// @var {string} Notation/name of signature. With space for inverted root and terminator
	char name[signature_t::SIGNATURENAMELENGTH + 1];
};

/*
 * @date 2021-10-17 21:50:15
 *
 * Patterns
 *
 * Contains sid/tid values for slot based patterns
 */

// NOTE: 33173240 entries [0.3G data + 0.2G index, ratio=1.5]
struct __attribute__((__packed__)) patternFirst_t {
        unsigned sidQ     :20; // key - sidQ
	unsigned sidTj    : 1; // key - actually sidTi but with LSB (`j` and not `i` to accentuate LSB instead of MSB)
	unsigned sidTu    :20; // key - sidT
	unsigned tidSlotT :19; // key - add T to reassembly slots (row)
        // total 60 bits (8 bytes)
};

// NOTE: 1175043464 entries [17.8G data + 7.1G index, ratio=1.5]
struct __attribute__((__packed__)) patternSecond_t {
	unsigned idFirst    :27; // key - reference to `patternFirst_t`
	unsigned sidF       :20; // key - id of sidF
	unsigned tidSlotF   :19; // key - add F to reassembly slots (row)
        // 64bit align
	unsigned sidR       :20; // data - sidR
	unsigned tidExtract :19; // data - extract R from reassembly slots (col)
	unsigned power      : 3; // data - difference `pattern.size - signature.size`
        // total 108 bits (14 bytes)
};

#endif
