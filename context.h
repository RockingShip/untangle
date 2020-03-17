#ifndef _CONTEXT_H
#define _CONTEXT_H

/*
 * @date 2020-03-12 13:16:47
 *
 * `context.h` collection of base types and utilities
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
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include "primedata.h"

/// @constant {number} IBIT - Which bit of the operand is reserved to flag that the result needs to be inverted
#define IBIT 0x80000000

/// @constant {number} MAXSLOTS - Total number of slots
#define MAXSLOTS 9

/// @constant {number} MAXTRANSFORM - Number of slot permutations
#define MAXTRANSFORM (1*2*3*4*5*6*7*8*9)

/**
 * Collection of utilities
 *
 * @typedef {object} FileHeader_t
 * @date 2020-03-12 13:19:24
 */
struct context_t {

	/*
	 * User specified program arguments and options
	 */

	/// @var {number} intentionally undocumented
	uint32_t opt_debug;
	/// @var {number} --verbose, What do you want to know
	unsigned opt_verbose;
	/// @var {number} --timer, interval timer for verbose updates
	unsigned opt_timer;

	/// @var {number} - async indication that a timer interrupt occurred
	uint32_t tick;
	/// @var {uint64_t} - total memory allocated by `myAlloc()`
	uint64_t totalAllocated;

	// statistics

	/// @var {uint64_t} - number of calls to baseTree::hash()
	uint64_t cntHash;
	/// @var {uint64_t} - number of compares in baseTree::hash() (collisions)
	uint64_t cntCompare;

	/*
	 * verbose levels
	 */
	// @formatter:off
	enum {
		VERBOSE_NONE       = 0,        // nothing
		VERBOSE_SUMMARY    = 1,        // summary after performing an action
		VERBOSE_ACTIONS    = 2,        // tell before performing an action
		VERBOSE_TICK       = 3,        // timed progress of actions (default)
		VERBOSE_VERBOSE    = 4,        // above average verbosity
		VERBOSE_INITIALIZE = 5,        // allocations, database connection and such
	};
	// @formatter:on

	/*
	 * tree/database constraints
	 */
	// @formatter:off
	enum {
		MAGICFLAG_PARANOID      = 0,                        // Force extra asserts when actually creating nodes
		MAGICFLAG_QNTF          = 1,                        // Force generation of QnTF
		MAGICFLAG_ROWINTERLEAVE = 2,                        // imprint index is row interleaved (otherwise column)

		MAGICMASK_PARANOID      = 1 << MAGICFLAG_PARANOID,
		MAGICMASK_QNTF          = 1 << MAGICFLAG_QNTF,
		MAGICMASK_ROWINTERLEAVE = 1 << MAGICFLAG_ROWINTERLEAVE,
	};
	// @formatter:on

	/**
	 * Constructor
	 */
	context_t() {
		// arguments and options
		opt_debug = 0;
		opt_verbose = VERBOSE_TICK;
		opt_timer = 1; // default is 1-second intervals

		// other values
		tick = 0;
		totalAllocated = 0;

		// statistics
		cntHash = 0;
		cntCompare = 0;
	}

	/**
	 * Construct a time themed prefix string for console logging
	 *
	 * @date 2020-03-12 13:37:12
	 */
	const char *timeAsString(void) {
		static char tstr[256];

		time_t t = ::time(0);
		struct tm *tm = ::localtime(&t);
		::strftime(tstr, sizeof(tstr), "%F %T", tm);

		return tstr;
	}

	/**
	 * Fatal error and exit
	 *
	 * @param format
	 * @param ...
	 * @date 2020-03-12 13:37:12
	 */
	void __attribute__((noreturn)) __attribute__ ((format (printf, 2, 3))) fatal(const char *format, ...) {
		va_list ap;
		va_start(ap, format);
		vfprintf(stderr, format, ap);
		::exit(1);
	}

	/**
	 * Allocate memory
	 *
	 * @param {string} name - Name associated to memory area
	 * @param {number} __nmemb - Number of elements
	 * @param {number} __size  - Size of element in bytes
	 * @return {void[]} Pointer to memory area or NULL if zero length was requested
	 * @date 2020-03-12 13:37:12
	 */
	void *myAlloc(const char *name, size_t __nmemb, size_t __size) {
		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "Allocating %s. %lu=%lu*%lu\n", name, __nmemb * __size, __nmemb, __size);

		if (__nmemb == 0 || __size == 0)
			return NULL;

		totalAllocated += __nmemb * __size;
		void *ret = ::calloc(__nmemb, __size);
		if (ret == 0)
			fatal("failed to allocate %ld bytes for \"%s\"\n", __nmemb * __size, name);

		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "memory +%p %s\n", ret, name);

		return ret;
	}

	/**
	 * Release memory
	 *
	 * @param {string} name - Name associated to memory area. Should match that of `myALloc()`
	 * @param {void[]} ptr - Pointer to memory area to be released
	 * @date 2020-03-12 13:37:12
	 */
	void myFree(const char *name, void *ptr) {
		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "memory -%p %s\n", ptr, name);

		::free(ptr);
	}

	/**
	 * Raise value to next prime.
	 *
	 * @param {number} n - number to raise
	 * @date 2020-03-15 19:21:50
	 */
	static uint32_t raisePrime(uint32_t n)
	{
		if (n == 0)
			return 0;

		// do not exceed 32 bits unsigned
		if (n > 4294000079 - n / 100)
			return 4294000079;

		// raise at least 1%
		n += n / 100;

		for (unsigned i = 0; ; i++) {
			if (n <= primeData[i])
				return primeData[i];
		}
		assert(0);
	}

	/**
	 * Raise value 1%.
	 *
	 * @param {number} n - number to raise
	 * @date 2020-03-15 19:42:19
	 */
	static uint32_t raiseProcent(uint32_t n) {
		if (n == 0)
			return 0; // zero is zero

		if (n > 4294000079 - n / 100)
			return 4294000079;

		// raise 1%
		if (n < 100)
			n++; // increment small numbers
		else
			n += n / 100; // raise 1%

		return n;
	}

	/**
	 * Display creation flags to stderr
	 *
	 * @date 2020-03-15 23:15:44
	 */
	void logFlags(uint32_t flags) {
		fprintf(stderr, "[%s] FLAGS [%x]:%s%s%s\n", this->timeAsString(),
		        flags,
		        (flags & context_t::MAGICMASK_PARANOID) ? " PARANOID" : "",
		        (flags & context_t::MAGICMASK_QNTF) ? " QNTF" : "",
		        (flags & context_t::MAGICMASK_ROWINTERLEAVE) ? " ROWINTERLEAVE" : ""
		);
	}

};

#endif