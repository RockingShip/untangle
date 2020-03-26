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
		VERBOSE_SUMMARY    = 1,        // summary after performing an action
		VERBOSE_ACTIONS    = 2,        // tell before performing an action
		VERBOSE_TICK       = 3,        // timed progress of actions (default)
		VERBOSE_VERBOSE    = 4,        // above average verbosity
		VERBOSE_INITIALIZE = 5,        // allocations, database connection and such
		// @formatter:on
	};

	/*
	 * tree/database constraints
	 */
	enum {
		// @formatter:off
		MAGICFLAG_PARANOID      = 0,    // Force extra asserts
		MAGICFLAG_QNTF          = 1,    // Force generation of QnTF

		MAGICMASK_PARANOID      = 1 << MAGICFLAG_PARANOID,
		MAGICMASK_QNTF          = 1 << MAGICFLAG_QNTF,
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

		// un-common flags go from high to low
		DEBUGFLAG_GEN_RATIO     = 31,    // Let `gensignature --metrics`

		DEBUGMASK_GEN_RATIO     = 1 << DEBUGFLAG_GEN_RATIO,
		// @formatter:on
	};

	/*
	 * User specified program arguments and options
	 */

	/// @var {number} intentionally undocumented
	uint32_t opt_debug;
	/// @var {number} program flags
	uint32_t opt_flags;
	/// @var {number} --timer, interval timer for verbose updates
	unsigned opt_timer;
	/// @var {number} --verbose, What do you want to know
	unsigned opt_verbose;

	/// @var {number} - async indication that a timer interrupt occurred
	uint32_t tick;
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
		opt_debug = 0;
		opt_flags = 0;
		opt_timer = 1; // default is 1-second intervals
		opt_verbose = VERBOSE_TICK;
		tick = 0;
		totalAllocated = 0;
		cntHash = 0;
		cntCompare = 0;
		progress = 0;
		progressHi = 0;
		progressCoef = 0;
		progressCoefStart = 0.70; // dampen speed changes at training start (high responsive)
		progressCoefEnd   = 0.10; // dampen speed changes at Training end (low responsive)
		progressCoefMultiplier = 0.9072878562; //  #seconds as #th root of (end/start). set for 20 second training
		progressLast = 0;
		progressSpeed = 0;
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
		void *ret = ::calloc(__nmemb, __size);
		if (ret == 0)
			fatal("failed to allocate %ld bytes for \"%s\"\n", __nmemb * __size, name);

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

		for (uint32_t i = 3; n / i >= i; i++) {
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
	 * @return {uint32_t} next highest prime, limited to 2^32-5
	 */
	uint32_t nextPrime(uint64_t n) {
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
	 * @date 2020-03-15 23:15:44
	 *
	 * Display creation flags to stderr
	 */
	void logFlags(uint32_t flags) {
		fprintf(stderr, "[%s] FLAGS [%x]:%s%s\n", this->timeAsString(),
		        flags,
		        (flags & context_t::MAGICMASK_PARANOID) ? " PARANOID" : "",
		        (flags & context_t::MAGICMASK_QNTF) ? " QNTF" : ""
		);
	}

	/**
	 * @date 2020-03-24 00:15:48
	 *
	 * Setup progress with speed guesstimation
	 *
	 * @param {uint64_t} progressHi - Expected end condition
	 */
	void setupSpeed(uint64_t progressHi) {
		this->progress      = 0;
		this->progressHi    = progressHi;
		this->progressCoef  = this->progressCoefStart;
		this->progressLast  = 0;
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
	uint32_t updateSpeed(void) {
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
		if (!perInterval)
			perInterval = 1; // avoid zero

		// NOTE: this is only called on a timer event, thus "opt_timer > 0"
		// if the timer interval is more than one second, scale speed accordingly
		int perSecond = perInterval / opt_timer;
		if (!perSecond)
			perSecond = 1; // avoid zero

		progressLast = progress;

		return perSecond;
	}

};

#endif