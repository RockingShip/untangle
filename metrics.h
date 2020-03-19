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
	/// @var {number} - Valid when match MAXSLOTS
	unsigned maxSlots;

	/// @var {number} - Row interleave (distance between two adjacent row keys)
	unsigned interleaveFactor;
	/// @var {number} - How many row/columns need to be stored in database. This value is communicated with user.
	unsigned numStored;
	/// @var {number} - How many row/columns need to be permuted at runtime.
	unsigned numRuntime;

	// NOTE: (interleaveFactor == numStored) "store key columns" else "store key rows"

	/// @var {number} - Some indication of number of associative lookups per second
	unsigned speed;
	/// @var {number} - Some indication of runtime index storage in G bytes. (worse case)
	float storage;
} ;

static const metricsInterleave_t metricsInterleave[] = {
	{9, 120, 120,  3024, 8850,   6.896},
	{9, 720, 504,  720,  51840,  28.78},
	{9, 720, 720,  504,  90720,  41.095},
	{9, 120, 3024, 120,  362880, 172.420},
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
 * @param {number} interleave - The interleave value communicated with user
 * @return {metricsInterleave_t} Reference to match or NULL if not found
 */
const metricsInterleave_t * getMetricsInterleave(unsigned maxSlots, unsigned interleave) {

	// walk through list
	for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->maxSlots; pInterleave++) {
		if (pInterleave->maxSlots == maxSlots && pInterleave->numStored == interleave)
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
const char * getAllowedInterleaves(unsigned maxSlots) {
	static char sbuf[256];
	unsigned spos = 0;

	// walk through list
	for (const metricsInterleave_t *pInterleave = metricsInterleave; pInterleave->maxSlots; pInterleave++) {
		if (pInterleave->maxSlots == maxSlots) {
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
	/// @var {number} - Valid when match `MAXSLOTS`
	unsigned maxSlots;
	/// @var {number} - Valid when match `interleave` (higher values implies more imprints per signature)
	unsigned interleave;
	/// @var {number} - Valid when match `numNodes` (higher values implies more signatures)
	unsigned numNodes;

	/// @var {number} - Total number of imprints for settings
	uint32_t numImprints;
} ;

static const metricsImprint_t metricsImprint[] = {
	// placeholder values
	{9, 720,  1, 60},
	{9, 720,  2, 1859},
	{9, 720,  3, 114626},
	{9, 720,  4, 11167160},
	{9, 720,  5, 1035381442},
	//
	{0}
};

/**
 * @date 2020-03-17 17:37:50
 *
 * Get expected number of imprints for requested settings
 *
 * @param {number} maxSlots - Number of slots (call with MAXSLOTS)
 * @param {number} interleave - The interleave value communicated with user
 * @param {number} numNodes - signature size in number of nodes
 * @return {metricsImprint_t} Reference to match or NULL if not found
 */
unsigned getMaxImprints(unsigned maxSlots, unsigned interleave, unsigned numNodes) {
	// walk through list
	for (const metricsImprint_t *pImprint = metricsImprint; pImprint->maxSlots; pImprint++) {
		// test if found
		if (pImprint->maxSlots == maxSlots && pImprint->interleave == interleave && pImprint->numNodes == numNodes)
			return pImprint->numImprints; // found
	}
	// not found
	return 0;
}
