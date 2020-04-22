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

#include <assert.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/// @constant {number} IBIT - Which bit of the operand is reserved to flag that the result needs to be inverted
#define IBIT 0x80000000

/// @constant {number} MAXSLOTS - Total number of slots
#define MAXSLOTS 9

/// @constant {number} MAXTRANSFORM - Number of slot permutations
#define MAXTRANSFORM (1*2*3*4*5*6*7*8*9)

/**
 * @date 2020-03-12 13:19:24
 *
 * Collection of utilities
 *
 * @typedef {object} FileHeader_t
 */
struct context_t {

	/*
	 * verbose levels
	 */
	enum {
		// @formatter:off
		VERBOSE_NONE       = 0,        // nothing
		VERBOSE_WARNING    = 1,        // summary after performing an action
		VERBOSE_SUMMARY    = 2,        // summary after performing an action
		VERBOSE_ACTIONS    = 3,        // tell before performing an action
		VERBOSE_TICK       = 4,        // timed progress of actions (default)
		VERBOSE_VERBOSE    = 5,        // above average verbosity
		VERBOSE_INITIALIZE = 6,        // allocations, database connection and such
		// @formatter:on
	};

	/*
	 * tree/database constraints
	 */
	enum {
		// @formatter:off
		MAGICFLAG_PARANOID      = 0,    // Force extra asserts
		MAGICFLAG_PURE          = 1,    // Force `QTF->QnTF` rewriting
		MAGICFLAG_UNSAFE        = 2,    // Imprints for empty/unsafe groups

		MAGICMASK_PARANOID      = 1 << MAGICFLAG_PARANOID,
		MAGICMASK_PURE          = 1 << MAGICFLAG_PURE,
		MAGICMASK_UNSAFE        = 1 << MAGICFLAG_UNSAFE,
		// @formatter:on
	};

	/*
	 * @date 2020-03-19 19:51:15
	 *
	 * Debug settings. Usually flags you don't want to spend effort on adding extra program options
	 */
	enum {
		// @formatter:off
		// common flags go from low to high

		// un-common or internal flags go from high to low
		DEBUGFLAG_GENERATOR_TABS    = 30,    // Disable `generatorTree_t::callFoundTree()`. When generator hits a restart point invoke callback.
		DEBUGFLAG_METRICS_RATIO     = 31,    // Let `gensignature --metrics` generate ratio metrics

		DEBUGMASK_GENERATOR_TABS    = 1 << DEBUGFLAG_GENERATOR_TABS,
		DEBUGMASK_METRICS_RATIO     = 1 << DEBUGFLAG_METRICS_RATIO,
		// @formatter:on
	};

	/// @var {number} system flags
	uint32_t flags;

	/*
	 * User specified program arguments and options
	 */

	/// @var {number} intentionally undocumented
	unsigned opt_debug;
	/// @var {number} --timer, interval timer for verbose updates
	unsigned opt_timer;
	/// @var {number} --verbose, What do you want to know
	unsigned opt_verbose;

	/// @var {number} - async indication that a timer interrupt occurred. +1 for passing each restart point, +2 on timer event
	unsigned tick;
        /// @var {number} Indication that a restart point has passed
        unsigned restartTick;
	/// @var {uint64_t} - total memory allocated by `myAlloc()`
	uint64_t totalAllocated;

	// statistics

	/// @var {uint64_t} - number of calls to baseTree::hash()
	uint64_t cntHash;
	/// @var {uint64_t} - number of compares in baseTree::hash() (collisions)
	uint64_t cntCompare;

	/// @var {uint64_t} - Current position of progress tracker
	uint64_t progress;
	/// @var {uint64_t} - Upper limit of progress tracker
	uint64_t progressHi;
	/// @var {double} - feedback coefficient for average operations/second
	double progressCoef;
	/// @var {double} - Starting Coefficient. To dampen eta prediction when training
	double progressCoefStart;
	/// @var {double} - Target coefficient after training
	double progressCoefEnd;
	/// @var {double} - Coefficient training multiplier. Training is slide from large damping to less to catch initial spikes.
	double progressCoefMultiplier;
	/// @var {uint64_t} - progress during last interval
	uint64_t progressLast;
	/// @var {double} - progress speed
	double progressSpeed;

	/**
	 * Constructor
	 */
	context_t() {
		cntHash = 0;
		cntCompare = 0;
		flags = 0;
		opt_debug = 0;
		opt_timer = 1; // default is 1-second intervals
		opt_verbose = VERBOSE_TICK;
		progress = 0;
		progressCoef = 0;
		progressCoefEnd = 0.10; // dampen speed changes at Training end (low responsive)
		progressCoefMultiplier = 0.9072878562; //  #seconds as #th root of (end/start). set for 20 second training
		progressCoefStart = 0.70; // dampen speed changes at training start (high responsive)
		progressHi = 0;
		progressLast = 0;
		progressSpeed = 0;
		restartTick = 0;
		tick = 0;
		totalAllocated = 0;
	}

	/**
	 * @date 2020-03-12 13:37:12
	 *
	 * Construct a time themed prefix string for console logging
	 */
	const char *timeAsString(void) {
		static char tstr[256];

		time_t t = ::time(0);
		struct tm *tm = ::localtime(&t);
		::strftime(tstr, sizeof(tstr), "%F %T", tm);

		return tstr;
	}

	/**
	 * @date 2020-03-12 13:37:12
	 *
	 * Fatal error and exit
	 *
	 * @param format
	 * @param ...
	 */
	void __attribute__((noreturn)) __attribute__ ((format (printf, 2, 3))) fatal(const char *format, ...) {
		va_list ap;
		va_start(ap, format);
		vfprintf(stderr, format, ap);
		::exit(1);
	}

	/**
	 * @date 2020-03-12 13:37:12
	 *
	 * Allocate memory
	 *
	 * @param {string} name - Name associated to memory area
	 * @param {number} __nmemb - Number of elements
	 * @param {number} __size  - Size of element in bytes
	 * @return {void[]} Pointer to memory area or NULL if zero length was requested
	 */
	void *myAlloc(const char *name, size_t __nmemb, size_t __size) {
		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "Allocating %s. %lu=%lu*%lu\n", name, __nmemb * __size, __nmemb, __size);

		if (__nmemb == 0 || __size == 0)
			return NULL;

		totalAllocated += __nmemb * __size;

		/*
		 * @date 2020-04-16 20:06:57
		 *
		 * AVX2 needs 32byte alignment
		 */

		// round size up to nearest 32 bytes
		__size *= __nmemb;
		__size += 32;
		__size &= ~31ULL;

		void *ret = ::aligned_alloc(32, __size);
		if (ret == 0)
			fatal("failed to allocate %lu bytes for \"%s\"\n", __size, name);

		// clear area
		::memset(ret, 0, __size);

		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "memory +%p %s\n", ret, name);

		return ret;
	}

	/**
	 * @date 2020-03-12 13:37:12
	 *
	 * Release memory
	 *
	 * @param {string} name - Name associated to memory area. Should match that of `myALloc()`
	 * @param {void[]} ptr - Pointer to memory area to be released
	 */
	void myFree(const char *name, void *ptr) {
		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "memory -%p %s\n", ptr, name);

		::free(ptr);
	}

	/**
	 * @date 2020-03-25 02:29:23
	 *
	 * Simple test to determine if number is prime
	 *
	 * @param {uint64_t} n - number to test. 64 bits to catch weird overflows by caller
	 * @return {boolean} `true` if prime, `false` otherwise
	 */
	bool isPrime(uint64_t n) {
		if (n % 2 == 0)
			return false;

		for (unsigned i = 3; n / i >= i; i++) {
			if ((n % i) == 0)
				return false;
		}

		return true;
	}

	/**
	 * @date 2020-03-25 02:30:30
	 *
	 * Raise number to next prime
	 *
	 * @param {uint64_t} n - number to test
	 * @return {number} next highest prime, limited to 2^32-5
	 */
	unsigned nextPrime(uint64_t n) {
		// limit to highest possible
		if (n >= 4294967291)
			return 4294967291;
		if (n < 3)
			return 3;

		// If even then increment
		if (~n & 1)
			n++;

		// increment until prime found;
		for (;;) {
			if (isPrime(n))
				return n;
			n += 2;
		}
	}

	/**
	 * @date 2020-04-21 14:41:24
	 *
	 * Raise number with given percent.
	 * limit to largest possible prime
	 *
	 * @param {uint64_t} n - number to test
	 * @param {number} percent - percent to increase with
	 * @return {number} next highest prime, limited to 2^32-5
	 */
	unsigned raisePercent(uint64_t n, unsigned percent) {

		if (n >= 4294967291)
			return 4294967291; // largest possible prime
		if (n >= 4294967291 - (n / 100 * percent))
			return 4294967291; // overflow

		// increase with given percent
		return n + (n / 100 * percent);
	}

	/**
	 * @date 2020-04-21 10:42:11
	 *
	 * Convert system model flags to string
	 *
	 * @param {number} flags - mask to encode
	 * @param {string} pBuffer - optional buffer to store result
	 * @return {string} Textual description of flags.
	 */
	char *flagsToText(unsigned flags, char *pBuffer = NULL) {
		static char buffer[128];
		if (pBuffer == NULL)
			pBuffer = buffer;

		*pBuffer = 0;

		if (flags & MAGICMASK_PARANOID) {
			::strcpy(pBuffer, "PARANOID");
			flags &= ~MAGICMASK_PARANOID;
		}
		if (flags)
			::strcpy(pBuffer, "p");
		if (flags & MAGICMASK_PURE) {
			::strcpy(pBuffer, "PURE");
			flags &= ~MAGICMASK_PURE;
		}
		if (flags)
			::strcpy(pBuffer, "p");
		if (flags & MAGICMASK_UNSAFE) {
			::strcpy(pBuffer, "UNSAFE");
			flags &= ~MAGICMASK_UNSAFE;
		}

		return pBuffer;
	}

	/**
	 * @date 2020-03-24 00:15:48
	 *
	 * Setup progress with speed guesstimation
	 *
	 * @param {uint64_t} progressHi - Expected end condition
	 */
	void setupSpeed(uint64_t progressHi) {
		this->progress = 0;
		this->progressHi = progressHi;
		this->progressCoef = this->progressCoefStart;
		this->progressLast = 0;
		this->progressSpeed = 0;
	}

	/**
	 * @date 2020-03-24 00:13:19
	 *
	 * Update progress and return speed guesstimation.
	 * Principle is that of R/C circuits used in electronics
	 *
	 * @return {number} - expected increment per second
	 */
	unsigned updateSpeed(void) {
		// Test for first time
		if (progressLast == 0) {
			progressLast = progress;
			return 0;
		}

		// update speed
		if (progressSpeed == 0)
			progressSpeed = (int) (progress - progressLast); // first time
		else
			progressSpeed += ((int) (progress - progressLast) - progressSpeed) * progressCoef; // update

		// training
		progressCoef *= progressCoefMultiplier;
		if (progressCoefMultiplier > 1 && progressCoef > progressCoefEnd)
			progressCoef = progressCoefEnd; // end of training
		if (progressCoefMultiplier < 1 && progressCoef < progressCoefEnd)
			progressCoef = progressCoefEnd; // end of training

		int perInterval = progressSpeed;

		// NOTE: this is only called on a timer event, thus "opt_timer > 0"
		// if the timer interval is more than one second, scale speed accordingly
		int perSecond = perInterval / opt_timer;

		progressLast = progress;

		return perSecond;
	}

};

#endif