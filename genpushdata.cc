//#pragma GCC optimize ("O0") // optimize on demand

/*
 * @date 2020-03-18 10:50:29
 *
 * `genpushdata` generates template data for `generator.h`
 *
 * The list will start with all `QnTF` templates followed by all `QTF` templates and terminated by zero
 *
 * The starting position of the list is found through the index:
 *   starting point = `"pushIndex[<section>][numNode][numPlaceholder]`
 *
 * Where `<section>` is one of:
 *   `PUSH_QTF`, `PUSH_QTP`, `PUSH_QPF`, `PUSH_QPP`, `PUSH_PTF`, `PUSH_PTP`, `PUSH_PPF`
 *
 * Templates are encoded as `"nextNumPlaceholders << 16 | TIBIT << 15 | Q << 10 | T << 5 | F << 0"`
 * QTF are positioned to match the same positions as on the runtime stack.
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

enum {
	/// @constant {number} MAXSLOTS - Total number of slots
	MAXSLOTS = 9,

	/// @constant {number} MAXNODES - Number of nodes. should match tinyTree_t::TINYTREE_MAXNODES
	MAXNODES = 10,

	/// @constant {number} KSTART - Start of endpoints. should match tinyTree_t::TINYTREE_KSTART
	KSTART = 1,

	/// @constant {number} NSTART - Start of nodes. should match tinyTree_t::TINYTREE_NSTART
	NSTART = (KSTART + MAXSLOTS),

	/// @constant {number} NEND - Highest node id
	NEND = (NSTART + MAXNODES),

	/// @constant {number} PUSH_TIBIT - template Bitmask to indicate inverted `T`
	PUSH_TIBIT = 0x8000,

	/// @constant {number} PUSH_QTF_MASK - mask to isolate QTF fields
	PUSH_QTF_MASK = 0b11111,

	/// @constant {number} PUSH_POS_Q - template starting bit position for `Q`
	PUSH_POS_Q = 5 * 2,

	/// @constant {number} PUSH_POS_T - template starting bit position for `T`
	PUSH_POS_T = 5 * 1,

	/// @constant {number} PUSH_POS_F - template starting bit position for `F`
	PUSH_POS_F = 5 * 0,

	/// @constant {number} PUSH_POS_F - template starting bit position for `newNumPlaceholders`
	PUSH_POS_NUMPLACEHOLDER = 16,
};

// section starting offsets
enum {
	PUSH_QTF = 0,
	PUSH_QTP = 1,
	PUSH_QPF = 2,
	PUSH_QPP = 3,
	PUSH_PTF = 4,
	PUSH_PTP = 5,
	PUSH_PPF = 6,
};

/// @global {number} - async indication that a timer interrupt occurred
unsigned tick;

// index tables pointing to start of data
uint32_t pushIndex[MAXNODES * MAXSLOTS * 7];

/**
 * Test if a `Q,T,F` combo would flow through normalisation unchanged
 *
 * Test level-1 normalisation excluding dyadic ordering
 *
 * @param {number} Q
 * @param {number} T
 * @param {number} F
 * @return {boolean} `true` it is would pass, `false` otherwise
 */
bool testNormalised(uint32_t Q, uint32_t T, uint32_t F) {

	// level-1
	if (Q == (T & ~PUSH_TIBIT))
		return false;  // Q?Q:F or Q?~Q:F
	if (Q == F)
		return false; // Q?T:Q
	if (T == F)
		return false; // Q?F:F
	if (Q == 0)
		return false; // 0?X:Y
	if (T == 0)
		return false; // Q?0:F -> F?~Q:0
	if (T == PUSH_TIBIT && F == 0)
		return false; // Q?~0:0

	// level-2 (simple)
	if (F == (T & ~PUSH_TIBIT) && Q > F)
		return false; // XOR "Q?~F:F"
	if (F == 0 && !(T & PUSH_TIBIT) && Q > (T & ~PUSH_TIBIT))
		return false; // AND "Q?T:0"
	if ((T & ~PUSH_TIBIT) == 0 && Q > F)
		return false; // OR "Q?~0:F"

	return 1;
}

/**
 * @date 2020-03-18 10:52:57
 *
 * Wildcard values represent node-references that are popped from the stack during runtime.
 * Zero means no wildcard, otherwise it must be a value greater than NSTART
 *
 * @return {number} Number of data entries created
 */
uint32_t generateData(void) {

	/*
	 * start data with an empty list
	 * index entries containing zero indicate invalid `numPlaceholder/numNode`
	 */

	printf("const uint32_t pushData[] = { 0,\n\n");
	uint32_t numData = 1;

	/*
	 * Run in multiple rounds, each round is a 3-bit mask, each bit indicating which operands are wildcards
	 * Do not include all bits set because that implies at runtime all operands were popped from stack with optimized handling
	 */
	for (unsigned iWildcard = 0; iWildcard < 0b111; iWildcard++) {

		// @formatter:off
		for (unsigned numNode=0; numNode < MAXNODES; numNode++)
		for (unsigned numPlaceholder=0; numPlaceholder < MAXSLOTS; numPlaceholder++) {
		// @formatter:on

			unsigned col = 0;

			// Index position
			unsigned ix = (iWildcard * MAXNODES + numNode) * MAXSLOTS + numPlaceholder;

			// test for proper section starts
			if (numPlaceholder == 0 && numNode == 0) {
				switch (iWildcard) {
					case 0b000:
						assert(PUSH_QTF * (MAXNODES * MAXSLOTS) == ix);
						break;
					case 0b001:
						assert(PUSH_QTP * (MAXNODES * MAXSLOTS) == ix);
						break;
					case 0b010:
						assert(PUSH_QPF * (MAXNODES * MAXSLOTS) == ix);
						break;
					case 0b011:
						assert(PUSH_QPP * (MAXNODES * MAXSLOTS) == ix);
						break;
					case 0b100:
						assert(PUSH_PTF * (MAXNODES * MAXSLOTS) == ix);
						break;
					case 0b101:
						assert(PUSH_PTP * (MAXNODES * MAXSLOTS) == ix);
						break;
					case 0b110:
						assert(PUSH_PPF * (MAXNODES * MAXSLOTS) == ix);
						break;
				}
			}

			// save starting position in data
			pushIndex[ix] = numData;

			printf("// %x: wildcard=%d numNode=%d numPlaceholder=%d\n", numData, iWildcard, numPlaceholder, numNode);

			/*
			 * Iterate through all possible `Q,T,F` possibilities
			 * First all the `QnTF` (Ti=1), then all the `QTF` (Ti=0)
			 *
			 * This to allow early bailout of list handling in `QnTF` mode.
			 */

			// @formatter:off
			for (int Ti = 1; Ti >= 0; Ti--)
			for (unsigned Q = 0; Q < NSTART + numNode; Q++)
			for (unsigned To = 0; To < NSTART + numNode; To++)
			for (unsigned F = 0; F < NSTART + numNode; F++) {
			// @formatter:on

				unsigned newNumPlaceholder = numPlaceholder;

				/*
				 * Test if some placeholders are wildcards.
				 * Wildcards get runtime replaced by popped values from the stack
				 * The replacement values must be higher than the end-loop condition
				 */

				if (iWildcard & 0b100) {
					Q = 0x7d; // assign unique value and break loop after finishing code block
				} else if (newNumPlaceholder < MAXSLOTS) {
					// Q must be a previously existing placeholder
					if (Q > KSTART + newNumPlaceholder && Q < NSTART)
						continue; // placeholder not created yet
					// bump placeholder if using for the first time
					if (Q == KSTART + newNumPlaceholder)
						newNumPlaceholder++;

					// verify that fielded does not overflow
					assert(!(Q & ~PUSH_QTF_MASK));
				} else {
					continue; // skip if exceeds maximum
				}

				if (iWildcard & 0b010) {
					To = 0x7e; // assign unique value and break loop after finishing code block
				} else if (newNumPlaceholder < MAXSLOTS) {
					// T must be a previously existing placeholder
					if (To > KSTART + newNumPlaceholder && To < NSTART)
						continue; // placeholder not created yet
					// bump placeholder if using for the first time
					if (To == KSTART + newNumPlaceholder)
						newNumPlaceholder++;

					// verify that fielded does not overflow
					assert(!(To & ~PUSH_QTF_MASK));
				} else {
					continue; // skip if exceeds maximum
				}

				if (iWildcard & 0b001) {
					F = 0x7f; // assign unique value and break loop after finishing code block
				} else if (newNumPlaceholder < MAXSLOTS) {
					// F must be a previously existing placeholder
					if (F > KSTART + newNumPlaceholder && F < NSTART)
						continue; // placeholder not created yet
					// bump placeholder if using for the first time
					if (F == KSTART + newNumPlaceholder)
						newNumPlaceholder++;

					// verify that fielded does not overflow
					assert(!(F & ~PUSH_QTF_MASK));
				} else {
					continue; // skip if exceeds maximum
				}

				/*
				 * Write output to data
				 */
				if (Ti == 1 && testNormalised(Q, To ^ PUSH_TIBIT, F)) {

					// `">NSTART"` flags a wildcard
					uint32_t outQ = (Q > NEND) ? 0 : Q;
					uint32_t outT = (To > NEND) ? 0 : To;
					uint32_t outF = (F > NEND) ? 0 : F;

					printf("0x%05x,", newNumPlaceholder << PUSH_POS_NUMPLACEHOLDER | PUSH_TIBIT | outQ << PUSH_POS_Q | outT << PUSH_POS_T | outF << PUSH_POS_F); // inverted T
					numData++;

					if (col % 9 == 8)
						printf("\n");
					else
						printf(" ");
					col++;
				}
				if (Ti == 0 && testNormalised(Q, To, F)) {

					// `">NSTART"` flags a wildcard
					uint32_t outQ = (Q > NEND) ? 0 : Q;
					uint32_t outT = (To > NEND) ? 0 : To;
					uint32_t outF = (F > NEND) ? 0 : F;

					printf("0x%05x,", newNumPlaceholder << PUSH_POS_NUMPLACEHOLDER | 0 | outQ << PUSH_POS_Q | outT << PUSH_POS_T | outF << PUSH_POS_F); // non-inverted T
					numData++;

					if (col % 9 == 8)
						printf("\n");
					else
						printf(" ");
					col++;
				}
			}

			// assert that a list was created
			assert (pushIndex[ix] != numData);

			// end of list
			printf("0,\n");
			numData++;
		}

		// bump data index
	}

	printf("};\n\n");
	return numData;
}

/**
 * @date 2020-03-18 13:49:33
 *
 * Generate/display the index
 */
void generateIndex(void) {

	printf("const uint32_t pushIndex[7][%d][%d] = { \n", MAXNODES, MAXSLOTS);

	/*
	 * Generate index
	 */
	for (unsigned iWildcard = 0; iWildcard < 0b111; iWildcard++) {

		printf("{ // wildcard=%u\n", iWildcard);

		for (unsigned numNode = 0; numNode < MAXNODES; numNode++) {
			printf("\t{ ");
			for (unsigned numPlaceholder = 0; numPlaceholder < MAXSLOTS; numPlaceholder++) {

				// Index position
				unsigned ix = (iWildcard * MAXNODES + numNode) * MAXSLOTS + numPlaceholder;

				printf("0x%05x,", pushIndex[ix]);
			}

			printf(" },\n");
		}
		printf("},\n");
	}

	// end of list
	printf("};\n\n");
}

/**
 * @date 2020-03-18 10:51:04
 *
 * Construct a time themed prefix string for console logging
 */
const char *timeAsString(void) {
	static char tstr[256];

	time_t t = time(0);
	struct tm *tm = localtime(&t);
	strftime(tstr, sizeof(tstr), "%F %T", tm);

	return tstr;
}

/**
 * @date 2020-03-18 10:51:11
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int sig) {
	(void) sig; // trick compiler t see parameter is used

	tick++;
	alarm(1);
}

/**
 * @date   2020-03-14 18:12:59
 *
 * Program main entry point
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 */
int main(int argc, char *const *argv) {
	setlinebuf(stdout);

	/*
	 * Test if output is redirected
	 */
	if (isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

	/*
	 * register timer handler
	 */
	signal(SIGALRM, sigalrmHandler);
	::alarm(1);

	/*
	 * Create data and output
	 */

	printf("// generated by %s on \"%s\"\n", argv[0], timeAsString());
	printf("\n");
	printf("#ifndef _PUSHDATA_H\n");
	printf("#define _PUSHDATA_H\n");
	printf("\n");
	printf("#include <stdint.h>\n");
	printf("\n");
	printf("// Index is encoded as: \"pushIndex[SECTION][numNode][numPlaceholder]\"\n");

	printf("\n\n");

	printf("enum {\n");
	printf("\t// Maximum number of placeholders\n\tPUSH_MAXPLACEHOLDERS=%d,\n", MAXSLOTS);
	printf("\t// Maximum number of nodes\n\tPUSH_MAXNODES=%d,\n", MAXNODES);
	printf("\t// Should match `tinyTree_t::TINYTREE_KSTART\n\tPUSH_KSTART=%d,\n", KSTART);
	printf("\t// Should match `tinyTree_t::TINYTREE_NSTART\n\tPUSH_NSTART=%d,\n", NSTART);

	printf("\t// Sections\n\t");
	printf("PUSH_QTF=%d, ", PUSH_QTF);
	printf("PUSH_QTP=%d, ", PUSH_QTP);
	printf("PUSH_QPF=%d, ", PUSH_QPF);
	printf("PUSH_QPP=%d, ", PUSH_QPP);
	printf("PUSH_PTF=%d, ", PUSH_PTF);
	printf("PUSH_PTP=%d, ", PUSH_PTP);
	printf("PUSH_PPF=%d,\n", PUSH_PPF);

	printf("\t// Bit offsets in template\n\t");
	printf("PUSH_POS_NUMPLACEHOLDER=%d, ", PUSH_POS_NUMPLACEHOLDER);
	printf("PUSH_POS_Q=%d, ", PUSH_POS_Q);
	printf("PUSH_POS_T=%d, ", PUSH_POS_T);
	printf("PUSH_POS_F=%d,\n", PUSH_POS_F);

	printf("\t// Mask to indicate T inverted\n\tPUSH_TIBIT=0x%x,\n", PUSH_TIBIT);

	printf("};\n");

	printf("\n\n");

	uint32_t numData = generateData();
	generateIndex();

	printf("\n\n");

	printf("#endif\n");

	// status
	fprintf(stderr, "[%s] Generated %d data entries\n", timeAsString(), numData);

	return 0;
}

