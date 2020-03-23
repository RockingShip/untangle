#ifndef _METRICS_H
#define _METRICS_H

/*
 * @date 2020-03-17 14:06:32
 *
 * Presets and metrics
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

/**
 * @date 2020-03-17 14:06:58
 *
 * Metrics for supported imprint index interleaving
 * `numStored` and `numRuntime` are worse case for footprints with 9 unique slots.
 */
struct metricsInterleave_t {
	/*
	 * Key
	 */

	/// @var {number} - Valid when match MAXSLOTS
	unsigned numSlots;

	/// @var {number} - How many row/columns need to be stored in database. This value is communicated with user.
	unsigned numStored;

	/*
	 * non-Key
	 */

	/// @var {number} - How many row/columns need to be permuted at runtime.
	unsigned numRuntime;

	/// @var {number} - Row interleave (distance between two adjacent row keys)
	unsigned interleaveStep;

	// NOTE: if (interleaveStep == numStored) then "store key columns" else "store key rows"

	/// @var {number} - Some indication of number of associative lookups per second
	unsigned speed;

	/// @var {number} - Some indication of runtime index storage in G bytes. (worse case)
	float storage;

	int zero; // to screen align data
} ;

static const metricsInterleave_t metricsInterleave[] = {
	{9, 120,  3024, 120, 8850,   6.896,   0}, // runtime slowest
	{9, 504,  720,  720, 51840,  28.78,   0},
	{9, 720,  504,  720, 90720,  41.095,  0},
	{9, 3024, 120,  120, 362880, 172.420, 0}, // runtime fastest
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
const metricsInterleave_t * getMetricsInterleave(unsigned numSlots, unsigned interleave) {

	// walk through list
	for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlots; pInterleave++) {
		if (pInterleave->numSlots == numSlots && pInterleave->numStored == interleave)
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
const char * getAllowedInterleaves(unsigned numSlots) {
	static char sbuf[256];
	unsigned spos = 0;

	// walk through list
	for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->numSlots; pInterleave++) {
		if (pInterleave->numSlots == numSlots) {
			// delimiter
			if (spos) {
				sbuf[spos++] = ',';
			}
			// interleave
			spos += sprintf(sbuf+spos,"%d", pInterleave->numStored);
		}
	}

	// terminate string
	sbuf[spos] = 0;

	return sbuf;
}

/**
 * @date 2020-03-17 14:06:58
 *
 * Metrics describing imprints
 */
struct metricsImprint_t {
	/*
	 * Key
	 */

	/// @var {number} - Valid when match `MAXSLOTS`
	unsigned numSlots;

	/// @var {number} - Valid when match `qntf`
	unsigned qntf;

	/// @var {number} - Valid when match `interleave` (higher values implies more imprints per signature)
	unsigned interleave;

	/// @var {number} - Valid when match `numNodes` (higher values implies more signatures)
	unsigned numNodes;

	/*
	 * non-Key
	 */

	/// @var {number} - Total number of imprints for settings
	uint32_t numImprints;

	int zero; // to screen align data
} ;

static const metricsImprint_t metricsImprint[] = {
	{9, 1, 504,  0, 6,         0},
	{9, 1, 120,  0, 7,         0},
	{9, 1, 3024, 0, 7,         0},
	{9, 1, 720,  0, 8,         0},
	{9, 1, 504,  1, 62,        0},
	{9, 1, 120,  1, 101,       0},
	{9, 1, 3024, 1, 117,       0},
	{9, 1, 720,  1, 181,       0},
	{9, 1, 504,  2, 2177,      0},
	{9, 1, 120,  2, 3177,      0},
	{9, 1, 3024, 2, 6138,      0},
	{9, 1, 720,  2, 9863,      0},
	{9, 1, 120,  3, 126802,    0},
	{9, 1, 504,  3, 149494,    0},
	{9, 1, 3024, 3, 561057,    0},
	{9, 1, 720,  3, 647618,    0},
	{9, 1, 120,  4, 10425180,  0},
	{9, 1, 504,  4, 19346575,  0},
	{9, 1, 720,  4, 61887091,  0},
	{9, 1, 3024, 4, 87860469,  0},
	//
	{9, 0, 504,  0, 6,         0},
	{9, 0, 120,  0, 7,         0},
	{9, 0, 3024, 0, 7,         0},
	{9, 0, 720,  0, 8,         0},
	{9, 0, 504,  1, 103,       0},
	{9, 0, 120,  1, 171,       0},
	{9, 0, 3024, 1, 201,       0},
	{9, 0, 720,  1, 316,       0},
	{9, 0, 504,  2, 6327,      0},
	{9, 0, 120,  2, 8827,      0},
	{9, 0, 3024, 2, 18706,     0},
	{9, 0, 720,  2, 29743,     0},
	{9, 0, 120,  3, 591412,    0},
	{9, 0, 504,  3, 775393,    0},
	{9, 0, 3024, 3, 3053157,   0},
	{9, 0, 720,  3, 3283078,   0},
	{9, 0, 120,  4, 89007120,  0}, //  8G memory
	{9, 0, 504,  4, 181883642, 0}, // 15G memory
	{9, 0, 720,  4, 531740476, 0}, // 45G memory
	{9, 0, 3024, 4, 0,         0}, // too large
	//
	{0}
};

/**
 * @date 2020-03-22 19:03:33
 *
 * Get metrics for imprints
 *
 * @param {number} maxSlots - Number of slots (call with MAXSLOTS)
 * @param {number} qntf - `QnTF-only` mode
 * @param {number} interleave - The interleave value communicated with user
 * @param {number} numNodes - signature size in number of nodes
 * @return {metricsImprint_t} Reference to match or NULL if not found
 */
const metricsImprint_t * getMetricsImprint(unsigned numSlots, unsigned qntf, unsigned interleave, unsigned numNodes) {
	// qntf is 0/1
	if (qntf)
		qntf = 1;

	// walk through list
	for (const metricsImprint_t *pMetrics = metricsImprint; pMetrics->numSlots; pMetrics++) {
		// test if found
		if (pMetrics->numSlots == numSlots && pMetrics->qntf == qntf && pMetrics->interleave == interleave && pMetrics->numNodes == numNodes)
			return pMetrics; // found
	}
	// not found
	return NULL;
}

/**
 * @date 2020-03-20 00:42:42
 *
 * Metrics describing generator
 */
struct metricsGenerator_t {
	/*
	 * Key
	 */

	/// @var {number} - Valid when match MAXSLOTS
	unsigned numSlots;
	/// @var {number} - `QnTF` mode
	unsigned qntf;
	/// @var {number} - Valid when match `numNodes` (higher values implies more signatures)
	unsigned numNodes;

	/*
	 * non-Key
	 */

	/// @var {number} - Total number of `foundTrees()` called
	uint64_t numProgress;

	/// @var {number} - Total number of `foundTrees()` called
	uint64_t numSignatures;

	int zero; // to screen align data
};

static const metricsGenerator_t metricsGenerator[] = {
	{9, 1, 0, 0,             3,      0},
	{9, 0, 0, 0,             3,      0},
	{9, 1, 1, 4,             5,      0},
	{9, 0, 1, 6,             7,      0},
	{9, 1, 2, 180LL,         49,     0},
	{9, 0, 2, 464LL,         110,    0},
	{9, 1, 3, 21645LL,       1311,   0},
	{9, 0, 3, 103240LL,      5666,   0},
	{9, 1, 4, 4807294LL,     96363,  0},
	{9, 0, 4, 43544252LL,    791647, 0},
	{9, 1, 5, 1682420716LL,  0,      0},
	{9, 0, 5, 29289824236LL, 0,      0},
	//
	//	{9, 1, 6, 41814086105LL, 0},
	//	{9, 0, 6, 1556055783374LL, 0},
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
 * @param {number} numNodes - signature size in number of nodes
 * @return {metricsImprint_t} Reference to match or NULL if not found
 */
const metricsGenerator_t * getMetricsGenerator(unsigned numSlots, unsigned qntf, unsigned numNodes) {
	// qntf is 0/1
	if (qntf)
		qntf = 1;

	// walk through list
	for (const metricsGenerator_t *pMetrics = metricsGenerator; pMetrics->numSlots; pMetrics++) {
		// test if found
		if (pMetrics->numSlots == numSlots && pMetrics->qntf == qntf && pMetrics->numNodes == numNodes)
			return pMetrics; // found
	}
	// not found
	return NULL;
}

#endif
