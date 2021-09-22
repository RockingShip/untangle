#ifndef _METRICS_H
#define _METRICS_H

/*
 * @date 2020-03-17 14:06:32
 *
 * Presets and metrics
 *
 * @date 2020-03-25 01:00:45
 *
 * Imprints and ratio
 *
 * Ratios are used to scale the index in relation to amount of data.
 * The index is a hash table lookup with overflow on collision.
 * When the index has the same number of entries as the data then collisions are certain to happen.
 * The excess on index entries reduces the chance of collisions.
 * Using `crc32` as hash function produces a good evenly spread index starting point.
 * Index sizes must be prime. For speed, The code raises that to the next 1M boundary   and the code raises that to the
 *
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

/**
 * @date 2020-03-17 14:06:58
 *
 * Metrics for supported imprint index interleaving
 * `numStored` and `numRuntime` are worse case for footprints with 9 unique slots.
 *
 * @date 2020-03-25 03:09:01
 *
 * Note that speed/storage is based on worst-case 4n9 structures with 9 unique endpoints
 *
 * @date 2020-04-18 01:59:50
 *
 * Interleave can be used to set imprint index row/col.
 * And now, as it seems: windowing based on interleave.
 *
 * For the low sid's: Theoretical an signature group requiring 362880 imprints.
 * The advantage is that it has the fastest speed.
 * The generator has a throughput of 150M/sec
 * After an associative lookup 1.5M/s
 * The index needs tuning and is probably in a slow setting.
 *
 * Wise would be to put the index into the fastest mode first.
 * Instead of the theoretical of 362880, there are a large number of 4560 (at the moment 25%).
 * Then recreate the index based on the signatures excluding what has been found.
 * Set the index into the second fastest speed.
 *
 * Get frequency chart.
 * Look for the collection/setting which can detect relatively the most.
 * like sumWithThreshold(unsafe/empty candidates)) / 362880 (or any other interleave)
 *
 * Select those sids
 *
 * The intermediate databases can be kept small with many active entries.
 * That collection can be sliced in parallel with `--windowhi/--windowlo`.
 */
struct metricsInterleave_t {
	/*
	 * Key
	 */

	/// @var {number} - Valid when match MAXSLOTS
	unsigned numSlot;

	/// @var {number} - How many row/columns need to be stored in database. This value is communicated with user.
	unsigned numStored;

	/*
	 * non-Key
	 *
	 * @date 2020-04-12 15:11:00
	 *
	 * `numStored` and `numRuntime` is the division between storage and computation tradeoff.
	 * Their product should always be `numSlot`!
	 * `interleaveStep` determines if "rows" or "columns" are stored. This impacts CPU caching.
	 * if `interleaveStep` == `numStored` then "store key columns" else "store key rows"
	 */

	/// @var {number} - How many row/columns need to be permuted at runtime.
	unsigned numRuntime;

	/// @var {number} - Row interleave (distance between two adjacent row keys)
	unsigned interleaveStep;

	/// @var {number} - Some indication of number of associative lookups per second
	unsigned speed;

	/// @var {number} - Some indication of runtime index storage in G bytes. (worse case)
	float storage;

	/// @var {number} - Ignore when recalculating metrics. OR'd 2=selftest
	int noauto;
};

/*
 * NOTE: run `selftest` after changing this table.
 *
 * @date 2020-04-18 22:46:31
 *
 * Note: For writes `"numStored == interleaveStep"` is more cpu cache friendlier.
 *       For reads `"numStored != interleaveStep"` is more cpu cache friendlier.
 *
 * Note: There are two duplicates:
 *   `"numStored==362880, interleaveStep=362880"` vs. `"numStored==362880, interleaveStep=1"`
 *   `"numStored==1, interleaveStep=362880"` vs. `"numStored==1, interleaveStep=1"`
 *   The most cpu cache friendly alternate has been chosen.
 */
static const metricsInterleave_t metricsInterleave[] = {
	{9, 362880 /*9!*/, 1,            362880, 362880, 3447.331, 0}, // fastest R slowest W
	{9, 181440,        2 /*2!*/,     2,      362880, 3447.331, 0},
	{9, 60480,         6 /*3!*/,     6,      362880, 3447.331, 0},
	{9, 40320 /*8!*/,  9,            40320,  362880, 2298.240, 0},
	{9, 15120,         24 /*4!*/,    24,     362880, 861.876,  0},
	{9, 5040/*7!*/,    72,           5040,   362880, 287.330,  0},
	{9, 3024,          120 /*5!*/,   120,    362880, 172.420,  0},
	{9, 720 /*6!*/,    504,          720,    90720,  41.095,   0},
	{9, 504,           720/*6!*/,    720,    51840,  28.78,    0},
	{9, 120 /*5!*/,    3024,         120,    8850,   6.896,    0},
	// the following are too slow at reading to be practical
	{9, 72,            5040/*7!*/,   5040,   51840,  28.78,    2},
	{9, 24 /*4!*/,     15120,        24,     8850,   6.896,    2},
	{9, 9,             40320/*8!*/,  40320,  51840,  28.78,    2},
	{9, 6 /*3!*/,      60480,        6,      8850,   6.896,    2},
	{9, 2 /*2!*/,      181440,       2,      8850,   6.896,    2},
	{9, 1,             362880/*9!*/, 362880, 8850,   6.896,    2}, // slowest R fastest W
	//
	{0}
};

/**
 * @date 2020-03-17 15:33:02
 *
 * Return entry matching selection
 * Interleave can discriminate by number of entries stored in database.
 *
 * @param {number} maxSlots - Number of slots (call with MAXSLOTS)
 * @param {number} interleave - The interleave value communicated with user (numStored)
 * @return {metricsInterleave_t} Reference to match or NULL if not found
 */
const metricsInterleave_t *getMetricsInterleave(unsigned numSlot, unsigned interleave) {

	// walk through list
	for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {
		if (pInterleave->numSlot == numSlot && pInterleave->numStored == interleave)
			return pInterleave;
	}

	// not found
	return NULL;
}

/**
 * @date 2020-03-17 15:12:02
 *
 * Construct a list of valid interleaves
 *
 * @param {number} maxSlots - Number of slots (call with MAXSLOTS)
 * @return {string} Comma separated list of allowed interleaves
 */
const char *getAllowedInterleaves(unsigned numSlot) {
	static char sbuf[256];
	unsigned spos = 0;

	// walk through list
	for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlot; pInterleave++) {
		if (pInterleave->numSlot == numSlot) {
			// delimiter
			if (spos) {
				sbuf[spos++] = ',';
			}
			// interleave
			spos += sprintf(sbuf + spos, "%u", pInterleave->numStored);
		}
	}

	// terminate string
	sbuf[spos] = 0;

	return sbuf;
}

/**
 * @date 2020-03-17 14:06:58
 *
 * Metrics describing imprints.
 *
 * Imprints drive associative lookups of footprints and hog memory.
 * These metrics assist in speed/memory trade-offs
 */
struct metricsImprint_t {
	/*
	 * Key
	 */

	/// @var {number} - Valid when match `MAXSLOTS`
	unsigned numSlot;

	/// @var {number} - Valid when match `pure`
	unsigned pure;

	/// @var {number} - Valid when match `interleave` (higher values implies more imprints per signature)
	unsigned interleave;

	/// @var {number} - Valid when match `numNode` (higher values implies more signatures)
	unsigned numNode;

	/*
	 * non-Key
	 *
	 * @date 2020-04-12 15:25:33
	 *
	 * `speed`/`storage` are only used for visual hints.
	 * `speed` is based on random collection which changes per run.
	 * `speed` is tuned to an "AMD Ryzen 1950X"
	 *
	 * On the long run, the higher the interleave the faster.
	 * Values above 5040 can be dramatically faster which is exploited by `genmember`
	 *
	 */

	/// @var {number} - Total number of imprints for settings. Provided by `gensignature --metrics`
	unsigned numImprint;

	/// @var {double} - Estimated `database_t::lookupImprintAssociative()` in M/s. Provided by `selftest --metrics`
	double speed; // NOTE: based on random collection which changes per run.

	/// @var {double} - Estimated storage in Gb. Provided by `selftest --metrics`
	double storage;

	/// @var {number} - Ignore when recalculating metrics
	int noauto;
};

/*
 * @date 2020-03-23 14:06:19
 *   recalculating these metrics cost about 30 minutes
 * @date 2020-04-12 15:29:55
 *   updated
 * @date 2020-04-24 20:16:16
 *   added interleave=1
 */
static const metricsImprint_t metricsImprint[] = {
	{9, 1, 504,    0, 6,         102.166, 0.000,  0},
	{9, 1, 120,    0, 7,         78.162,  0.000,  0},
	{9, 1, 3024,   0, 7,         72.493,  0.000,  0},
	{9, 1, 720,    0, 8,         53.880,  0.000,  0},
	{9, 1, 504,    1, 67,        62.045,  0.000,  0},
	{9, 1, 120,    1, 107,       49.895,  0.000,  0},
	{9, 1, 3024,   1, 123,       55.162,  0.000,  0},
	{9, 1, 720,    1, 188,       47.703,  0.000,  0},
	{9, 1, 504,    2, 2176,      37.082,  0.000,  0},
	{9, 1, 120,    2, 3177,      32.906,  0.000,  0},
	{9, 1, 3024,   2, 6137,      36.216,  0.001,  0},
	{9, 1, 720,    2, 9863,      33.732,  0.001,  0},
	{9, 1, 120,    3, 126802,    17.617,  0.012,  0},
	{9, 1, 504,    3, 149379,    25.423,  0.014,  0},
	{9, 1, 3024,   3, 560824,    13.882,  0.052,  0},
	{9, 1, 720,    3, 647618,    12.226,  0.060,  0},
	{9, 1, 120,    4, 10424640,  8.558,   0.959,  0}, // <-- default
	{9, 1, 504,    4, 19338792,  10.006,  1.780,  0},
	{9, 1, 720,    4, 61887211,  7.396,   5.694,  0},
	{9, 1, 3024,   4, 87834669,  7.217,   8.083,  0},
	//
	{9, 0, 504,    0, 6,         93.006,  0.000,  0},
	{9, 0, 120,    0, 7,         72.315,  0.000,  0},
	{9, 0, 3024,   0, 7,         72.796,  0.000,  0},
	{9, 0, 720,    0, 8,         53.743,  0.000,  0},
	{9, 0, 504,    1, 108,       56.315,  0.000,  0},
	{9, 0, 120,    1, 177,       43.700,  0.000,  0},
	{9, 0, 3024,   1, 207,       50.036,  0.000,  0},
	{9, 0, 720,    1, 323,       44.937,  0.000,  0},
	{9, 0, 504,    2, 6327,      36.780,  0.001,  0},
	{9, 0, 120,    2, 8827,      29.721,  0.001,  0},
	{9, 0, 3024,   2, 18706,     33.313,  0.002,  0},
	{9, 0, 720,    2, 29743,     32.139,  0.003,  0},
	{9, 0, 120,    3, 591412,    11.892,  0.054,  0},
	{9, 0, 504,    3, 775199,    15.586,  0.071,  0},
	{9, 0, 3024,   3, 3052779,   10.341,  0.281,  0},
	{9, 0, 720,    3, 3283078,   9.359,   0.302,  0},
	{9, 0, 120,    4, 89019740,  7.756,   8.189,  0}, // <-- default
	{9, 0, 504,    4, 181859539, 7.046,   16.733, 0},
	{9, 0, 720,    4, 531756796, 3.989,   48.920, 1}, // unpractical
	//
	// special presets for `genmember` using 4n9 signatures
	{9, 1, 3024,   5, 8493341,   0,       0,      1},
	{9, 0, 15120,  5, 8493341,   0,       0,      1},
	{9, 0, 40320,  5, 40481281,  0,       0,      1}, // The high number of imprints is because row/col has a different spread which has less re-usability
	{9, 0, 60480,  5, 26043121,  0,       0,      1},
	{9, 0, 362880, 5, 118903681, 0,       0,      1},
	// special presets for `--interleave=1` with values taken from `metricsData[]`
	{9, 1, 1,      0, 3,         0,       0.000,  1},
	{9, 0, 1,      0, 3,         0,       0.000,  1},
	{9, 1, 1,      1, 7,         0,       0.000,  1},
	{9, 0, 1,      1, 9,         0,       0.000,  1},
	{9, 1, 1,      2, 49,        0,       0.000,  1},
	{9, 0, 1,      2, 110,       0,       0.000,  1},
	{9, 1, 1,      3, 1311,      0,       0.000,  1},
	{9, 0, 1,      3, 5666,      0,       0.000,  1},
	{9, 1, 1,      4, 96363,     0,       0.000,  1},
	{9, 0, 1,      4, 791647,    0,       0.000,  1},
	{9, 1, 1,      5, 57412551,  0,       0.000,  1},
	{0}
};

/**
 * @date 2020-03-22 19:03:33
 *
 * Get metrics for imprints
 *
 * @param {number} maxSlots - Number of slots (call with MAXSLOTS)
 * @param {number} pure - pure mode
 * @param {number} interleave - The interleave value communicated with user
 * @param {number} numNode - signature size in number of nodes
 * @return {metricsImprint_t} Reference to match or NULL if not found
 */
const metricsImprint_t *getMetricsImprint(unsigned numSlot, unsigned pure, unsigned interleave, unsigned numNode) {
	// pure is 0/1
	if (pure)
		pure = 1;

	// walk through list
	for (const metricsImprint_t *pMetrics = metricsImprint; pMetrics->numSlot; pMetrics++) {
		// test if found
		if (pMetrics->numSlot == numSlot && pMetrics->pure == pure && pMetrics->interleave == interleave && pMetrics->numNode == numNode)
			return pMetrics; // found
	}
	// not found
	return NULL;
}

/**
 * @date 2021-09-20 18:34:11
 *
 * Metrics describing generator loop overhead
 *
 * Primarily used for generator restart/windowing. 
 * Provided by `genrestartdata` and stored in `restartdata.h`.
 */

struct metricsRestart_t {
	/*
	 * Key
	 */

	/// @var {number} - MAXSLOTS
	unsigned numSlot;
	/// @var {number} - `numNode`
	unsigned numNode;
	/// @var {number} - `pure` mode
	unsigned pure;

	/// @var {number} - starting offset in `restartdata[]`
	uint32_t sectionOffset;
};

// Need to explicitly include the generated data
extern const uint64_t         __attribute__ ((weak)) restartData[];
extern const metricsRestart_t __attribute__ ((weak)) restartIndex[];

/*
 * @date 2021-09-20 01:26:59
 * 
 * Get metrics for restarting
 *
 * @param {number} maxSlots - Number of slots (call with MAXSLOTS)
 * @param {number} numNode - signature size in number of nodes
 * @param {number} pure - `--pure`
 * @param {number} cascade - `--cascade`
 * @return {metricsProgress_t} Reference to match or NULL if not found
 */
const metricsRestart_t *getMetricsRestart(unsigned numSlot, unsigned numNode, unsigned pure) {
	// ensure boolean values
	if (pure)
		pure = 1;

	// test if data available
	if (restartIndex == NULL)
		return NULL;
	
	// walk through list
	for (const metricsRestart_t *pMetrics = restartIndex; pMetrics->numSlot; pMetrics++) {
		// test if found
		if (pMetrics->numSlot == numSlot && pMetrics->numNode == numNode && pMetrics->pure == pure)
			return pMetrics; // found
	}
	// not found
	return NULL;
}

/**
 * @date 2020-03-20 00:42:42
 *
 * Metrics describing generator.
 *
 * Primarily used to calculate generator progress.
 * It also reflects effectiveness of normalisation levels 1+2 (numCandidate) and level 3 (numSignature).
 */
struct metricsGenerator_t {
	/*
	 * Key
	 */

	/// @var {number} - Valid when match MAXSLOTS
	unsigned numSlot;
	/// @var {number} - Valid when match `numNode` (higher values implies more signatures)
	unsigned numNode;
	/// @var {number} - `pure` mode
	unsigned pure;

	/*
	 * non-Key
	 *
	 * @date 2020-04-12 15:04:32
	 *
	 * `numCandidate`/`numProgress` indicates how many duplicates the generator creates.
	 * `numSignature`/`numCandidate` indicates how much redundancy a structure space has.
	 * `numMember`/`numSignature` indicates the average size of signature groups.
	 */

	/// @var {number} - Total number of `foundTrees()` called. Provided by `genrestartdata`
	uint64_t numProgress;

	/// @var {number} - Total candidate (unique `foundTrees()` calls). Provided by `genrestartdata --text`
	uint64_t numCandidate; // (including mandatory zero entry)

	/// @var {number} - Total signatures (unique footprints). Provided by `selftest --metrics`
	uint64_t numSignature; // (including mandatory zero entry)

	/// @var {number} - Numner of unique swaps
	uint64_t numSwap; // (including mandatory zero entry)

	/// @var {number} - Numner of unique hints
	uint64_t numHint; // (including mandatory zero entry)

	/// @var {number} - Total members (before compacting). Provided by `genmember`. Using `4n9` signature space
	uint64_t numPair; // (including mandatory zero entry)

	/// @var {number} - Total members (before compacting). Provided by `genmember`. Using `4n9` signature space, including depreciated
	uint64_t numMember; // (including mandatory zero entry)

	/// @var {number} - Total patternFirst. Provided by `genpattern`. Using `4n9` signature space
	uint64_t numPatternFirst; // (including mandatory zero entry)

	/// @var {number} - Total patternSecond. Provided by `genpattern`. Using `4n9` signature space
	uint64_t numPatternSecond; // (including mandatory zero entry)

	/// @var {number} - Ignore when recalculating metrics. OR'd 1=genrestartdata 2=selftest
	int noauto;
};

/*
 * @date 2021-07-20 09:44:51
 *   last updated
 *   NOTE: pure-v2 are signatures with pure components and mixed-toplevel
 */
static const metricsGenerator_t metricsGenerator[] = {
	{9, 0, 1, 0,             3,         3,        171, 225, 0,       3,        0, 0, 0},
	{9, 0, 0, 0,             3,         3,        171, 225, 5,       3,        0, 0, 0},
	{9, 1, 1, 4,             5,         7,        2,   6,   5,       7,        0, 0, 0},
	{9, 1, 0, 6,             7,         9,        2,   6,   5,       9,        0, 0, 0},
	{9, 2, 1, 180,           155,       49,       7,   14,  11,      108,      0, 0, 0},
	{9, 2, 0, 484,           425,       110,      7,   14,  44,      284,      0, 0, 0},
	{9, 3, 1, 19350,         15221,     1311,     35,  47,  171,     6937,     0, 0, 0},
	{9, 3, 0, 97696,         79835,     5666,     35,  47,  3327,    32246,    0, 0, 0},
	{9, 4, 1, 3849342,       2777493,   193171,   191, 225, 12647,   833486,   0, 0, 0},
	{9, 4, 0, 37144912,      28304991,  791647,   191, 225, 555494,  6570607,  0, 0, 0},
	{9, 5, 1, 1220219415,    809357847, 10233318, 0,   0,   1483834, 31827424, 0, 0, 2}, // NOTE: this is the extension to 4n9
	// below intended for members
	{9, 5, 0, 22715579984,   0,         0,        0,   0,   900252,  7506360,  0, 0, 2}, // NOTE: this is the extension to 4n9
	{9, 6, 1, 561428696882,  0,         0,        0,   0,   1483845, 31827834, 0, 0, 2}, // numProgress takes about 60 minutes
	{9, 6, 0, 1556055783374, 0,         0,        0,   0,   0,       0,        0, 0, 3}, // from historic metrics
	//
	{0}
};

/**
 * @date 2020-03-20 00:52:57
 *
 * Get metrics for invocation
 *
 * @param {number} maxSlots - Number of slots (call with MAXSLOTS)
 * @param {number} interleave - The interleave value communicated with user
 * @param {number} numNode - signature size in number of nodes
 * @return {metricsImprint_t} Reference to match or NULL if not found
 */
const metricsGenerator_t *getMetricsGenerator(unsigned numSlot, unsigned numNode, unsigned pure) {
	// pure is 0/1
	if (pure)
		pure = 1;

	// walk through list
	for (const metricsGenerator_t *pMetrics = metricsGenerator; pMetrics->numSlot; pMetrics++) {
		// test if found
		if (pMetrics->numSlot == numSlot && pMetrics->numNode == numNode && pMetrics->pure == pure)
			return pMetrics; // found
	}
	// not found
	return NULL;
}

/**
 * @date 2020-03-25 13:58:50
 *
 * Ratio statistics
 * Metrics were collected for all 4-node trees and ratio settings.
 * It shows: speed in associative lookups per second, required storage and the average number of cache hits per footprint lookup.
 *
 * The interleave influences CPU cache and how it might thrash it.
 * The cache hits influences how often 72 byte large structures get compared.
 *
 * These metrics are a side project and were a pain to get.
 * They were created to get an impression of the effects of settings and are once-only never again. (use `selftest --metrics=3`)
 *
 * Measurements were performed on random signature lookups with random skins.
 */

double ratioMetrics_pure[][8][3] = {
	// each triplet = [speed M/s, Storage Gb, avg. cache hits per lookup }
	//
	//      pure-i120                 pure-i504                pure-i720                pure-i3024
	{{6.273, 0.834, 2.19980}, {7.282,  1.548, 2.20292}, {5.429, 4.951, 2.22969}, {5.989, 7.029, 2.23827}}, // r=2.0
	{{6.482, 0.842, 1.98826}, {7.657,  1.563, 1.99072}, {5.616, 5.000, 2.01456}, {6.186, 7.099, 2.02260}}, // r=2.2
	{{6.717, 0.851, 1.83789}, {7.915,  1.579, 1.84022}, {5.808, 5.050, 1.86352}, {6.386, 7.169, 1.87256}}, // r=2.4
	{{6.920, 0.859, 1.72671}, {8.159,  1.594, 1.72928}, {5.981, 5.099, 1.74997}, {6.582, 7.240, 1.76004}}, // r=2.6
	{{6.804, 0.867, 1.64085}, {8.388,  1.610, 1.64357}, {6.160, 5.149, 1.66456}, {6.767, 7.310, 1.67317}}, // r=2.8
	{{7.286, 0.876, 1.57307}, {8.595,  1.625, 1.57605}, {6.318, 5.199, 1.59544}, {6.952, 7.380, 1.60419}}, // r=3.0
	{{7.410, 0.884, 1.51754}, {8.775,  1.641, 1.51986}, {6.463, 5.248, 1.53945}, {7.128, 7.451, 1.54795}}, // r=3.2
	{{7.664, 0.892, 1.47205}, {8.963,  1.656, 1.47469}, {6.608, 5.298, 1.49299}, {7.290, 7.521, 1.50156}}, // r=3.4
	{{7.808, 0.901, 1.43356}, {9.116,  1.672, 1.43613}, {6.725, 5.347, 1.45419}, {7.438, 7.591, 1.46225}}, // r=3.6
	{{7.922, 0.909, 1.40114}, {9.271,  1.687, 1.40350}, {6.843, 5.397, 1.42139}, {7.575, 7.661, 1.42874}}, // r=3.8
	{{8.049, 0.917, 1.37305}, {9.386,  1.702, 1.37499}, {6.944, 5.446, 1.39251}, {7.636, 7.732, 1.40023}}, // r=4.0
	{{8.160, 0.926, 1.34836}, {9.516,  1.718, 1.35068}, {7.041, 5.496, 1.36786}, {7.534, 7.802, 1.37574}}, // r=4.2
	{{8.277, 0.934, 1.32680}, {9.644,  1.733, 1.32891}, {7.146, 5.545, 1.34577}, {7.576, 7.872, 1.35339}}, // r=4.4
	{{8.317, 0.942, 1.30788}, {9.738,  1.749, 1.31003}, {7.232, 5.595, 1.32615}, {7.759, 7.943, 1.33406}}, // r=4.6
	{{8.374, 0.951, 1.29044}, {9.819,  1.764, 1.29306}, {7.295, 5.644, 1.30923}, {8.058, 8.013, 1.31674}}, // r=4.8
	{{8.492, 0.959, 1.27572}, {9.924,  1.780, 1.27809}, {7.364, 5.694, 1.29369}, {8.117, 8.083, 1.30103}}, // r=5.0 // <-- default
	{{8.555, 0.967, 1.26199}, {9.995,  1.795, 1.26405}, {7.421, 5.743, 1.27965}, {7.996, 8.153, 1.28761}}, // r=5.2
	{{8.631, 0.976, 1.24966}, {10.056, 1.811, 1.25178}, {7.475, 5.793, 1.26718}, {8.091, 8.224, 1.27408}}, // r=5.4
	{{8.672, 0.984, 1.23835}, {10.115, 1.826, 1.24093}, {7.517, 5.842, 1.25582}, {8.145, 8.294, 1.26343}}, // r=5.6
	{{8.773, 0.992, 1.22804}, {10.184, 1.842, 1.23040}, {7.562, 5.892, 1.24485}, {8.211, 8.364, 1.25297}}, // r=5.8
	{{8.826, 1.001, 1.21849}, {10.248, 1.857, 1.22058}, {7.607, 5.941, 1.23593}, {8.284, 8.435, 1.24294}}, // r=6.0
};

double ratioMetrics[][8][3] = {
	// each triplet = [speed M/s, Storage Gb, avg. cache hits per lookup }
	//
	//          i120                     i504                      i720
	{{5.617, 7.120, 2.25985}, {6.233, 14.551, 2.31815}, {4.150, 42.539, 2.55734}}, // r=2.0
	{{5.932, 7.192, 2.03981}, {6.675, 14.696, 2.09536}, {4.177, 42.964, 2.32632}}, // r=2.2
	{{5.997, 7.263, 1.88737}, {6.947, 14.842, 1.93758}, {4.142, 43.390, 2.15550}}, // r=2.4
	{{6.167, 7.334, 1.77228}, {7.162, 14.987, 1.81931}, {4.154, 43.815, 2.01615}}, // r=2.6
	{{6.431, 7.405, 1.68384}, {7.374, 15.133, 1.73038}, {4.160, 44.241, 1.91585}}, // r=2.8
	{{6.381, 7.476, 1.61325}, {7.550, 15.278, 1.65648}, {4.017, 44.666, 1.84370}}, // r=3.0
	{{6.854, 7.548, 1.55580}, {7.709, 15.424, 1.60000}, {4.087, 45.091, 1.78206}}, // r=3.2
	{{6.701, 7.619, 1.50961}, {7.837, 15.569, 1.54905}, {4.367, 45.517, 1.72809}}, // r=3.4
	{{6.999, 7.690, 1.47022}, {8.007, 15.715, 1.51026}, {4.404, 45.942, 1.67997}}, // r=3.6
	{{7.245, 7.761, 1.43604}, {8.134, 15.860, 1.47532}, {4.447, 46.368, 1.63405}}, // r=3.8
	{{7.348, 7.832, 1.40726}, {7.805, 16.006, 1.44362}, {4.274, 46.793, 1.58934}}, // r=4.0
	{{7.471, 7.904, 1.38213}, {7.874, 16.151, 1.41935}, {4.266, 47.218, 1.56701}}, // r=4.2
	{{7.239, 7.975, 1.35932}, {7.840, 16.297, 1.39619}, {4.621, 47.644, 1.55028}}, // r=4.4
	{{7.495, 8.046, 1.33970}, {7.903, 16.442, 1.37516}, {4.678, 48.069, 1.53416}}, // r=4.6
	{{7.744, 8.117, 1.32176}, {7.770, 16.588, 1.35627}, {4.689, 48.495, 1.51815}}, // r=4.8
	{{7.657, 8.189, 1.30637}, {8.009, 16.733, 1.34191}, {4.823, 48.920, 1.50416}}, // r=5.0 // <-- default
	{{7.609, 8.260, 1.29253}, {7.291, 16.879, 1.32796}, {4.302, 49.345, 1.49021}}, // r=5.2
	{{7.692, 8.331, 1.27904}, {7.198, 17.024, 1.31472}, {4.636, 49.771, 1.47606}}, // r=5.4
	{{7.893, 8.402, 1.26828}, {7.910, 17.170, 1.30138}, {4.752, 50.196, 1.46501}}, // r=5.6
	{{8.017, 8.473, 1.25734}, {6.945, 17.315, 1.28865}, {4.989, 50.621, 1.45427}}, // r=5.8
	{{7.773, 8.545, 1.24682}, {7.504, 17.461, 1.27884}, {4.891, 51.047, 1.44327}}, // r=6.0
};

#endif
