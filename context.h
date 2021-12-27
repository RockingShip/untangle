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
#include <cstring>
#include <string>
#include <time.h>
#include <unistd.h>

/**
 * Which bit of node/key/root ID's is reserved to flag that the result needs to be inverted
 *
 * @constant {number} IBIT
 */
#define IBIT 0x80000000

/**
 * Number of slots/keys for `tinyTree_t` structures.
 *
 * @constant {number} MAXSLOTS
 */
#define MAXSLOTS 9

/**
 * Number of MAXSLOTS key permutations
 *
 * @constant {number} MAXTRANSFORM - Number of slot permutations
 */
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
		MAGICFLAG_PURE          = 1,    // Force `QTF->QnTF` rewriting, smaller collection of candidates
		MAGICFLAG_AINF          = 3,    // add-if-not-found, signatures/imprints contain false duplicates
		MAGICFLAG_CASCADE       = 4,    // Enable level-3 normalisation: cascaded OR/NE/AND
		MAGICFLAG_REWRITE       = 5,    // Enable level-4 normalisation: Database lookup/rewrite

		MAGICMASK_PARANOID      = 1 << MAGICFLAG_PARANOID,
		MAGICMASK_PURE          = 1 << MAGICFLAG_PURE,
		MAGICMASK_AINF          = 1 << MAGICFLAG_AINF,
		MAGICMASK_CASCADE       = 1 << MAGICFLAG_CASCADE,
		MAGICMASK_REWRITE       = 1 << MAGICFLAG_REWRITE,
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
		DEBUGFLAG_COMPARE           = 0,	// Display the intermediate steps in `baseTree_t::compare()`
		DEBUGFLAG_REWRITE           = 1,	// Display the intermediate steps in `baseTree_t::rewriteNode()`
		DEBUGFLAG_EXPLAIN           = 2,	// Display the intermediate steps in `baseTree_t::addNormaliseNode()`
		
		DEBUGFLAG_CARTESIAN         = 3,	// Display the raw Cartesian products.
		DEBUGFLAG_GROUP             = 4,	// Display final nodes of group creation
		DEBUGFLAG_GROUPNODE         = 5,	// Display creation of `groupTree_t` nodes
		
		DEBUGFLAG_GROUPEXPR         = 6,	// Display group as string after creation

		// un-common or internal flags go from high to low
		DEBUGFLAG_GENERATOR_TABS    = 30,	// Disable `generatorTree_t::callFoundTree()`. When generator hits a restart point invoke callback.
		DEBUGFLAG_METRICS_RATIO     = 31,	// Let `selftest --metrics` generate ratio metrics

		DEBUGMASK_COMPARE           = 1 << DEBUGFLAG_COMPARE,
		DEBUGMASK_REWRITE           = 1 << DEBUGFLAG_REWRITE,
		DEBUGMASK_EXPLAIN           = 1 << DEBUGFLAG_EXPLAIN,
		DEBUGMASK_CARTESIAN         = 1 << DEBUGFLAG_CARTESIAN,
		DEBUGMASK_GROUP             = 1 << DEBUGFLAG_GROUP,
		DEBUGMASK_GROUPNODE         = 1 << DEBUGFLAG_GROUPNODE,
		DEBUGMASK_GROUPEXPR         = 1 << DEBUGFLAG_GROUPEXPR,
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

	/// @var {number} - async indication that a timer interrupt occurred.
	unsigned tick;
        /// @var {number} Indication that a restart point has passed
        unsigned restartTick;

	// statistics

	/// @var {uint64_t} - total memory allocated by `myAlloc()`
	uint64_t totalAllocated;

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
		flags = 0;
		// user arguments
		opt_debug = 0;
		opt_timer = 1; // default is 1-second intervals
		opt_verbose = VERBOSE_TICK;
		// timed progress
		restartTick = 0;
		tick = 0;
		// statistics
		totalAllocated = 0;
		cntHash = 0;
		cntCompare = 0;
		progress = 0;
		progressHi = 0;
		progressCoef = 0;
		progressCoefStart = 0.70; // dampen speed changes at training start (high responsive)
		progressCoefEnd = 0.10; // dampen speed changes at Training end (low responsive)
		progressCoefMultiplier = 0.9072878562; //  #seconds as #th root of (end/start). set for 20 second training
		progressLast = 0;
		progressSpeed = 0;
	}

	/*
	 * @date 2021-05-14 22:15:31
	 *
	 * Display creation flags
	 */
	void logFlags(uint32_t flags) {
		fprintf(stderr, "[%s] FLAGS [%x]:%s%s%s%s%s\n", timeAsString(),
			flags,
			(flags & context_t::MAGICMASK_PARANOID) ? " PARANOID" : "",
			(flags & context_t::MAGICMASK_PURE) ? " PURE" : "",
			(flags & context_t::MAGICMASK_AINF) ? " AINF" : "",
			(flags & context_t::MAGICMASK_CASCADE) ? " CASCADE" : "",
			(flags & context_t::MAGICMASK_REWRITE) ? " REWRITE" : ""
		);
	}

	/**
	 * @date 2020-03-12 13:37:12
	 *
	 * Construct a time themed prefix string for console logging
	 */
	const char *timeAsString(void) {
		static char tstr[64];

		if (false) {
			// relative
			static time_t t0;
			if (!t0)
				t0 = ::time(0);

			time_t t = ::time(0) - t0;

			unsigned s = t % 60;
			t /= 60;
			unsigned m = t % 60;
			t /= 60;
			sprintf(tstr, "%02d:%02u:%02u", (unsigned) t, m, s);

		} else {
			// absolute
			time_t    t   = ::time(0);
			struct tm *tm = ::localtime(&t);
			::strftime(tstr, sizeof(tstr), "%F %T", tm);
		}

		return tstr;
	}

	/**
	 * @date 2020-03-12 13:37:12
	 *
	 * Fatal error and exit
	 *
	 * @date 2020-05-05 16:35:58
	 *
	 * Always to stdout to break `--text`.
	 * Also to stderr if stdout is redirected
	 *
	 * @param format
	 * @param ...
	 */
	void __attribute__((noreturn)) __attribute__ ((format (printf, 2, 3))) fatal(const char *format, ...) {
		::va_list ap;
		::va_start(ap, format);

		if (::isatty(1)) {
			// to stdout
			::vfprintf(stdout, format, ap);
		} else {
			// using varargs twice will crash
			char *p;
			::vasprintf(&p, format, ap);
			::fputs(p, stdout);
			::fputs(p, stderr);
			::free(p);
		}

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

		// calculate and round size up to nearest 32 bytes
		__size *= __nmemb;
		__size += 32;
		__size &= ~31ULL;

		totalAllocated += __size;

		/*
		 * @date 2020-04-16 20:06:57
		 *
		 * AVX2 needs 32byte alignment
		 */

		void *ret = ::aligned_alloc(32, __size);
		if (ret == 0)
			fatal("failed to allocate %lu bytes for \"%s\"\n", __size, name);

		// clear area
		memset(ret, 0, __size);

		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "memory +%p %s\n", ret, name);

		return ret;
	}

	/**
	 * @date 2020-03-12 13:37:12
	 *
	 * Release memory
	 *
	 * @param {string} name - Name associated to memory area. Should match that of `myAlloc()`
	 * @param {void[]} ptr - Pointer to memory area to be released
	 */
	void myFree(const char *name, void *ptr) {
		if (opt_verbose >= VERBOSE_INITIALIZE)
			fprintf(stderr, "memory -%p %s\n", ptr, name);

		::free(ptr);
	}

        /*
         * Prime numbers
         */

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
		if (n == 0)
			return 0; // zero ois zero
		if (n < 3)
			return 3;

		// If even then increment
		if (!(n & 1))
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
	 * limit to largest possible unsigned prime
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
		if (n < 4294967291 / 1000)
			return (unsigned) (n + (n * percent / 100)); // better rounding
		else
			return (unsigned) (n + (n / 100 * percent)); // overflow protected
	}

	/**
	 * @date 2020-04-27 12:04:08
	 *
	 * Double to unsigned and limit to largest signed prime - 1. (2147483647-1)
	 * This is to allow a prime index size which is larger yet still 31 bits.
	 * 31 bits is limit because of IBIT.
	 *
	 * @param {uint64_t} n - number to test
	 * @return {number}
	 */
	unsigned dToMax(double d) {

		if (d >= 2147483646)
			return 2147483646; // largest signed prime - 1

		return (unsigned) d;
	}
	/**
	 * @date 2020-04-21 10:42:11
	 *
	 * Convert system model flags to string
	 *
	 * @param {number} flags - mask to encode
	 * @return {string} Textual description of flags.
	 */
	std::string flagsToText(unsigned flags) {
		std::string txt;

		if (flags & MAGICMASK_PARANOID) {
			txt += "PARANOID";
			flags &= ~MAGICMASK_PARANOID;
			if (flags)
				txt += '|';
		}
		if (flags & MAGICMASK_PURE) {
			txt += "PURE";
			flags &= ~MAGICMASK_PURE;
			if (flags)
				txt += '|';
		}
		if (flags & MAGICMASK_AINF) {
			txt += "AINF";
			flags &= ~MAGICMASK_AINF;
			if (flags)
				txt += '|';
		}
		if (flags & MAGICMASK_CASCADE) {
			txt += "CASCADE";
			flags &= ~MAGICMASK_CASCADE;
			if (flags)
				txt += '|';
		}
		if (flags & MAGICMASK_REWRITE) {
			txt += "REWRITE";
			flags &= ~MAGICMASK_REWRITE;
			if (flags)
				txt += '|';
		}

		return txt;
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
		this->tick = 0;
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
			return 1; // avoid division by zero
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
		if (perSecond == 0)
			perSecond = 1; // avoid division by zero

		progressLast = progress;

		return perSecond;
	}

};

#endif
