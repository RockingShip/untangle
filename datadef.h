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
 * As this is a reference implementation, `SIMD` instructions should be avoided.
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
	 * Compare twwo prints and determine if both are same
	 *
	 * @param {footprint_t} rhs - right hand side of comparison
	 * @return {boolean} `true` if same, `false` if different
	 */
	inline bool equals(const struct footprint_t &rhs) const {
		// NOTE: QUADPERFOOTPRINT tests
		if (this->bits[0] != rhs.bits[0]) return false;
		if (this->bits[1] != rhs.bits[1]) return false;
		if (this->bits[2] != rhs.bits[2]) return false;
		if (this->bits[3] != rhs.bits[3]) return false;
		if (this->bits[4] != rhs.bits[4]) return false;
		if (this->bits[5] != rhs.bits[5]) return false;
		if (this->bits[6] != rhs.bits[6]) return false;
		if (this->bits[7] != rhs.bits[7]) return false;

		return true;
	}

	/**
	 * @date 2020-03-15 20:29:35
	 *
	 * Calculate the crc of a footprint
	 *
	 * @return {number} - calculate crc
	 */
	inline uint32_t crc32(void) const {

		uint64_t crc64 = 0;
		// NOTE: QUADPERFOOTPRINT tests
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[0]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[1]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[2]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[3]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[4]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[5]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[6]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[7]));

		return (uint32_t) crc64;
	}

};

/*
 * @date 2020-03-15 19:16:31
 *
 * Footprint belonging to signature/transform
 */
struct imprint_t {
	footprint_t footprint; // footprint
	uint32_t sid;          // signature
	uint32_t tid;          // skin/transform
};

/*
 * @date 2020-03-22 17:43:03
 *
 * Footprint belonging to signature/transform
 */
struct signature_t {
	enum {
		/// @constant {number} (numnode*3+1+1/*root invert*/+1/*terminator*/) For 5n9 signatures (4n9 is default) that would be 18
		SIGNATURENAMELENGTH = (5 * 3 + 1 + 1 + 1),
	};

	enum {
		SIGFLAG_UNSAFE = 0, // It is unsafe to use the display name to reconstruct structures
		SIGFLAG_PROVIDES = 1, // this signature provides as an operand
		SIGFLAG_REQUIRED = 2, // this signature is used as an operand

		// @formatter: off
		SIGMASK_UNSAFE = 1 << SIGFLAG_UNSAFE,
		SIGMASK_PROVIDES = 1 << SIGFLAG_PROVIDES,
		SIGMASK_REQUIRED = 1 << SIGFLAG_REQUIRED,
		// @formatter: on
	};

	/// @var {number} member id of first member in signature group
	uint32_t firstMember;

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
	char name[SIGNATURENAMELENGTH];
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
		/// @constant {number} Maximum number of heads
		MAXHEAD = 6,
	};

	/// @var {number} Signature id to which signature group this member belongs
	uint32_t sid;

	/// @var {number} member id of `Q` component
	uint32_t Qmid;

	/// @var {number} Signature id of `Q` component
	uint32_t Qsid;

	/// @var {number} member id of `T` component
	uint32_t Tmid;

	/// @var {number} member id of `T` component
	uint32_t Tsid;

	/// @var {number} member id of `F` component
	uint32_t Fmid;

	/// @var {number} member id of `F` component
	uint32_t Fsid;

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
	char name[signature_t::SIGNATURENAMELENGTH];
};

#endif