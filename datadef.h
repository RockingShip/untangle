#ifndef _DATADEF_H
#define _DATADEF_H

/*
 * Collection of data definitions used to store footprints, signatures, patterns and more
 *
 @date 2020-03-15 20:20:35 */

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
 * struct representing a 512 bit vector, each bit representing the outcome of the unified operator for every possible state 9 variables can take
 * The vector is split into a collection of 64bit wide words.
 *
 * Test vectors are also used to compare equality of two trees
 *
 * As this is a reference implementation, `SIMD` instructions should be avoided.
 *
 * @typedef {number[]}
 * @date 2020-03-06 23:23:32
 */
struct footprint_t {
	enum {
		/// @constant {number} Size of footprint in terms of uint64_t
		QUADPERFOOTPRINT = ((1 << MAXSLOTS) / 64)
	};

	uint64_t bits[QUADPERFOOTPRINT]; // = 512/64 = 8 = QUADPERFOOTPRINT

	/**
	 * Compare twwo prints and determine if both are same
	 *
	 * @param {footprint_t} rhs - right hand side of comparison
	 * @return {boolean} `true` if same, `false` if different
	 * @date 2020-03-15 20:26:04
	 */
	inline bool equals(const struct footprint_t &rhs) const {
		if (this->bits[0] != rhs.bits[0]) return false;
		if (this->bits[1] != rhs.bits[1]) return false;
		if (this->bits[2] != rhs.bits[2]) return false;
		if (this->bits[3] != rhs.bits[3]) return false;
		if (this->bits[4] != rhs.bits[4]) return false;
		if (this->bits[5] != rhs.bits[5]) return false;
		if (this->bits[6] != rhs.bits[6]) return false;
		if (this->bits[7] != rhs.bits[7]) return false;
		if (this->bits[8] != rhs.bits[8]) return false;

		return true;
	}

	/**
	 * Calculate the crc of a footprint
	 *
	 * @return {number} - calculate crc
	 * @date 2020-03-15 20:29:35
	 */
	inline uint32_t crc32(void) const {

		uint64_t crc64 = 0;
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[0]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[1]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[2]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[3]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[4]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[5]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[6]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[7]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(this->bits[8]));

		return (uint32_t) crc64;
	}

};

/*
 * Footprint belonging to signature/transform
 *
 * @date 2020-03-15 19:16:31
 */
struct imprint_t {
	footprint_t footprint; // footprint
	uint32_t    sid;       // signature
	uint32_t    tid;       // skin/transform
};

#endif