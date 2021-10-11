//#pragma GCC optimize ("O0") // optimize on demand

/*
 * genrewritedata.cc
 * 	Program to create peephole rewrite for `baseTree_t::normaliseNode()`
 * 	This program may not depend on `baseTree_t`
 *
 * 	Address space is simple "abc!def!ghi!!" (with all QTF/QTnF combos)
 * 	There are to flavours, non-destructive and destructive.
 * 	The first rewrites only the top-level QTF operator, the latter will also rewrite operands
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2017-2021, xyzzy@rockingship.org
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

#include <ctype.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "database.h"
#include "generator.h"
#include "rewritedata.h"

struct genrewritedataContext_t {

	struct print_t {
		char        name[tinyTree_t::TINYTREE_NAMELEN + 1];
		footprint_t footprint;
		uint32_t    size;
		uint32_t    score;
	};

	/// @var {context_t} I/O context
	context_t &ctx;

	uint32_t opt_flags;
	uint32_t opt_first;

	uint32_t iVersion;

	// print store
	uint32_t numPrints;            // number of elements in collection
	uint32_t maxPrints;            // maximum size of collection
	print_t  *prints;              // imprint collection
	uint32_t printIndexSize;       // index size (must be prime)
	uint32_t *printIndex;          // index

	uint32_t *normVersion;
	uint32_t *normMap;

	uint32_t maxDataTree;
	uint32_t numDataTree;
	uint64_t *gDataTree;

	uint32_t maxData;
	uint32_t numData;
	uint32_t *gData;                // state data
	uint8_t  *gDataLength;          // size of block/chunk
	char     **gDataLabel;              // name of block/chunk
	uint32_t *gDataOwner;           // owner of the entry, marked by `progress`
	uint32_t gCntFound;
	uint32_t gCntTree;
	uint32_t gCntNode[5];

	genrewritedataContext_t(context_t &ctx)
	/*
	 * initialize fields using initializer lists
	 */
		:
		ctx(ctx),
		opt_flags(0),
		opt_first(1<<5), // assuming tinyTree_t nodeID's fit in 5 bits, so start after that
		iVersion(1),
		// imprint store
		numPrints(0), // do not start at 0
		maxPrints(1000000),
		prints((print_t *) ctx.myAlloc("prints", maxPrints, sizeof(*this->prints))),
		printIndexSize(ctx.nextPrime(1000000)),
		printIndex((uint32_t *) ctx.myAlloc("printIndex", printIndexSize, sizeof(*this->printIndex))),
		//
		normVersion((uint32_t *) ctx.myAlloc("normVersion", tinyTree_t::TINYTREE_NEND, sizeof(*this->normVersion))),
		normMap((uint32_t *) ctx.myAlloc("normMap", tinyTree_t::TINYTREE_NEND, sizeof(*this->normMap))),
		//
		maxDataTree(1000000),
		numDataTree(0),
		gDataTree((uint64_t *) ctx.myAlloc("gDataTree", maxData, sizeof(*gDataTree))),
		//
		maxData(2100000),
		numData(0),
		gData((uint32_t *) ctx.myAlloc("gData", maxData, sizeof(*gData))),
		gDataLength((uint8_t *) ctx.myAlloc("gDataLength", maxData, sizeof(*gDataLength))),
		gDataLabel((char **) ctx.myAlloc("gDataLabel", maxData, sizeof(*gDataLabel))),
		gDataOwner((uint32_t *) ctx.myAlloc("gDataOwner", maxData, sizeof(*gDataOwner)))

	/*
	 * test all allocations succeeded
	 */
	{
		gCntFound = 0;
		gCntTree  = 0;
		gCntNode[0] = gCntNode[1] = gCntNode[2] = gCntNode[3] = gCntNode[4] = 0;
	}

#if 0
	inline uint32_t lookupPrint(const footprint_t &v) {

		context_t::cntHash++;

		// starting position
		uint64_t crc64 = 0;
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[0]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[1]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[2]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[3]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[4]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[5]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[6]));
		__asm__ __volatile__ ("crc32q %1, %0" : "+r"(crc64) : "rm"(v.bits[7]));

		uint32_t ix   = (uint32_t) (crc64 % printIndexSize);
		uint32_t bump = ix;
		if (bump == 0)
			bump = printIndexSize - 1;
		if (bump > 2147000041)
			bump = 2147000041;

		for (;;) {
			context_t::cntCompare++;
			if (this->printIndex[ix] == 0)
				return ix;

			const print_t *pPrint = this->prints + this->printIndex[ix];

			if (pPrint->footprint.equals(v))
				return ix;

			ix += bump;
			if (ix >= printIndexSize)
				ix -= printIndexSize;
		}
	}

	inline uint32_t addPrint(const footprint_t &v) {
		print_t *pPrint = this->prints + this->numPrints++;
		if (this->numPrints >= this->maxPrints)
			context_t::fatal("\n[%s %s:%u storage full %d]\n", __FUNCTION__, __FILE__, __LINE__, this->maxPrints);

		// only populate key fields
		pPrint->footprint = v;
		pPrint->size      = 0;
		pPrint->score     = 0;

		return (uint32_t) (pPrint - this->prints);
	}

	void collect(void) {
		tinyTree_t tree(genrewritedataArgs.opt_flags);

		if (context_t::opt_verbose >= VERBOSE_ACTIONS)
			fprintf(stderr, "\r\e[K[%s] Collect [progress(speed) eta]\n", timeAsString());

		/*
		 * generate all slot-relative combinations
		 * NOTE: allow triple-zero and triple-self
		 */

		progress      = 0;
		progressHi    = 940140; // 198072;
		progressCoef  = progressCoefMin;
		progressLast  = 0;
		progressSpeed = 0;
		tick          = 0;

		numPrints = 1; // first entry is reserved

		//@formatter:off
		for (uint32_t Q1 = 1; Q1 < NSTART+0; Q1++)
		for (uint32_t Tu1 = 0; Tu1 < NSTART+0; Tu1++)
		for (uint32_t Ti1 = 0; Ti1 < 2; Ti1++)
		for (uint32_t F1 = 0; F1 < NSTART+0; F1++) {
		//@formatter:on

			// validate
			if (Q1 != Tu1 || Q1 != F1 || Ti1) {
				if (Q1 == 0) continue;
				if (Q1 == Tu1) continue;             // Q/T collapse
				if (Q1 == F1) continue;              // Q/F collapse
				if (Tu1 == F1 && Ti1 == 0) continue; // T/F collapse
				if (Tu1 == 0 && Ti1 == 0) continue;  // Q?0:F -> F?!Q:0
				if (Tu1 == 0 && F1 == 0) continue;   // Q?!0:0 -> Q
			}

			// check adjacent endpoints
			uint32_t nextSlot1 = 0;
			if (Q1 < NSTART && Q1 > nextSlot1) {
				if (Q1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}
			if (Tu1 < NSTART && Tu1 > nextSlot1) {
				if (Tu1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}
			if (F1 < NSTART && F1 > nextSlot1) {
				if (F1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}

			uint32_t nextNode1 = NSTART;
			uint32_t tlQ;
			if (Q1 == Tu1) {
				tlQ = Q1;
			} else {
				tlQ = nextNode1++;
				tree.N[tlQ].Q = Q1;
				tree.N[tlQ].T = Tu1 ^ (Ti1 ? IBIT : 0);
				tree.N[tlQ].F = F1;
			}

			//@formatter:off
			for (uint32_t Q2 = 0; Q2 < nextNode1; Q2++)
			for (uint32_t Tu2 = 0; Tu2 < nextNode1; Tu2++)
			for (uint32_t Ti2 = 0; Ti2 < 2; Ti2++)
			for (uint32_t F2 = 0; F2  < nextNode1; F2++) {
			//@formatter:on

				// validate
				if (Q2 != Tu2 || Q2 != F2 || Ti2) {
					if (Q2 == 0 && (Tu2 || Ti2 || F2)) continue;
					if (Q2 == Tu2) continue;             // Q/T collapse
					if (Q2 == F2) continue;              // Q/F collapse
					if (Tu2 == F2 && Ti2 == 0) continue; // T/F collapse
					if (Tu2 == 0 && Ti2 == 0) continue;  // Q?0:F -> F?!Q:0
					if (Tu2 == 0 && F2 == 0) continue;   // Q?!0:0 -> Q
					// assume runtime detects duplicates
					if (Q2 == Q1 && Tu2 == Tu1 && Ti2 == Ti1 && F2 == F1) continue;
				}


				// check adjacent endpoints
				uint32_t nextSlot2 = nextSlot1;
				if (Q2 < NSTART && Q2 > nextSlot2) {
					if (Q2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}
				if (Tu2 < NSTART && Tu2 > nextSlot2) {
					if (Tu2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}
				if (F2 < NSTART && F2 > nextSlot2) {
					if (F2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}

				uint32_t nextNode2 = nextNode1;
				uint32_t tlTu;
				if (Q2 == Tu2) {
					tlTu = Q2;
				} else {
					tlTu = nextNode2++;
					tree.N[tlTu].Q = Q2;
					tree.N[tlTu].T = Tu2 ^ (Ti2 ? IBIT : 0);
					tree.N[tlTu].F = F2;
				}

				//@formatter:off
				for (uint32_t Q3 = 0; Q3 < nextNode2; Q3++)
				for (uint32_t Tu3 = 0; Tu3  < nextNode2; Tu3++)
				for (uint32_t Ti3 = 0; Ti3 < 2; Ti3++)
				for (uint32_t F3 = 0; F3  < nextNode2; F3++) {
				//@formatter:on

					// validate
					if (Q3 != Tu3 || Q3 != F3 || Ti3) {
						if (Q3 == 0 && (Tu3 || Ti3 || F3)) continue;
						if (Q3 == Tu3) continue;             // Q/T collapse
						if (Q3 == F3) continue;              // Q/F collapse
						if (Tu3 == F3 && Ti3 == 0) continue; // T/F collapse
						if (Tu3 == 0 && Ti3 == 0) continue;  // Q?0:F -> F?!Q:0
						if (Tu3 == 0 && F3 == 0) continue;   // Q?!0:0 -> Q
						// assume runtime detects duplicates
						if (Q3 == Q1 && Tu3 == Tu1 && Ti3 == Ti1 && F3 == F1) continue;
						if (Q3 == Q2 && Tu3 == Tu2 && Ti3 == Ti2 && F3 == F2) continue;
					}

					// check adjacent endpoints
					uint32_t nextSlot3 = nextSlot2;
					if (Q3 < NSTART && Q3 > nextSlot3) {
						if (Q3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}
					if (Tu3 < NSTART && Tu3 > nextSlot3) {
						if (Tu3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}
					if (F3 < NSTART && F3 > nextSlot3) {
						if (F3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}

					uint32_t nextNode3 = nextNode2;
					uint32_t tlF;
					if (Q3 == Tu3) {
						tlF = Q3;
					} else {
						tlF = nextNode3++;
						tree.N[tlF].Q = Q3;
						tree.N[tlF].T = Tu3 ^ (Ti3 ? IBIT : 0);
						tree.N[tlF].F = F3;
					}

					for (uint32_t tlTi = 0; tlTi < 2; tlTi++) {
						if (tick && context_t::opt_verbose >= VERBOSE_TICK) {
							if (progressSpeed != 0)
								progressSpeed += ((int) (progress - progressLast) - progressSpeed) * progressCoef; // update
							else
								progressSpeed = (int) (progress - progressLast); // first time

							progressCoef *= progressCoefIncrement;
							if (progressCoef > progressCoefMax)
								progressCoef  = progressCoefMax;

							int perInterval = progressSpeed;
							if (!perInterval) perInterval = 1;
							int perSecond = perInterval / opt_timer;
							if (!perSecond) perSecond = 1;
							int eta = (int) ((progressHi - progress) / perSecond);

							int etaH = eta / 3600;
							eta %= 3600;
							int etaM = eta / 60;
							eta %= 60;
							int etaS = eta;

							progressLast = progress;
							tick         = 0;

							fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d %d",
								timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS, numPrints);
						}
						progress++;

						// validate
						if (tlQ != tlTu || tlQ != tlF || tlTi) {
							if (tlQ == 0 && (tlTu || tlTi || tlF)) continue;
							if (tlQ == tlTu) continue;              // Q/T collapse
							if (tlQ == tlF) continue;               // Q/F collapse
							if (tlTu == tlF && tlTi == 0) continue; // T/F collapse
							if (tlTu == 0 && tlTi == 0) continue;   // Q?0:F -> F?!Q:0
							if (tlTu == 0 && tlF == 0) continue;    // Q?!0:0 -> Q
						}

						// construct a tree
						tree.N[nextNode3].Q = tlQ;
						tree.N[nextNode3].T = tlTu ^ (tlTi ? IBIT : 0);
						tree.N[nextNode3].F = tlF;

						tree.root  = nextNode3;
						tree.count = nextNode3 + 1;

						/*
 						 * Determine score (less is better)
 						 *  numNodes << 8  | numEndpoint << 4 | numQTF
 						 */
						uint32-t score = 0;

						score = tree.count - tree.TINYTREE_NSTART;
						...

						/*
						 * Add footprint if not found
						 */

						tree.eval(evaluatorRow->evalData64);

						footprint_t *pFoot = evaluatorRow->evalData64 + tree.root;

						// start with footprint
						uint32_t ix = lookupPrint(*pFoot);
						if (printIndex[ix] == 0) {
							// new
							printIndex[ix] = addPrint(*pFoot);

							print_t *pPrint = prints + printIndex[ix];
							strcpy(pPrint->name, tree.saveString(tree.root));
							pPrint->size  = tree.count - NSTART;
							pPrint->score = tree.calcSecondaryScore(tree.root);
						} else {
							// update
							print_t *pPrint = prints + printIndex[ix];

							uint64_t score = tree.calcSecondaryScore(tree.root);
							if (score > pPrint->score) {
								strcpy(pPrint->name, tree.saveString(tree.root));
								pPrint->size  = tree.count - NSTART;
								pPrint->score = tree.calcSecondaryScore(tree.root);

							}
						}
					}
				}
			}
		}
		if (context_t::opt_verbose >= VERBOSE_SUMMARY)
			fprintf(stderr, "\r\e[K[%s] numPrints=%d\n", timeAsString(), numPrints);
		if (progress != progressHi)
			fprintf(stderr, "[progressHi=%ld]\n", progress);

	}

	void generate() {
		tinyTree_t tree(store.flags);
		tinyTree_t testTree(genrewritedataArgs.opt_flags);

		if (context_t::opt_verbose >= VERBOSE_ACTIONS)
			fprintf(stderr, "\r\e[K[%s] Find sources [progress(speed) eta cntFound cntShrink cntNode0 cntNode1 cntNode2 cntNode3 cntNode4]\n", timeAsString());

		// setup first block
		numData = genrewritedataArgs.opt_first;
		gDataLength[numData] = 2;
		gDataLabel[numData]  = strdup("start");
		numData += 2;

		numDataTree = 1;

		/*
		 * generate all slot-relative combinations
		 * NOTE: allow triple-zero and triple-self
		 */

		progress      = 0;
		progressHi    = 940140; // 198072;
		progressCoef  = progressCoefMin;
		progressLast  = 0;
		progressSpeed = 0;
		tick          = 0;

		//@formatter:off
		for (uint32_t Q1 = 1; Q1 < NSTART+0; Q1++)
		for (uint32_t Tu1 = 0; Tu1 < NSTART+0; Tu1++)
		for (uint32_t Ti1 = 0; Ti1 < 2; Ti1++)
		for (uint32_t F1 = 0; F1 < NSTART+0; F1++) {
		//@formatter:on

			// validate
			if (Q1 != Tu1 || Q1 != F1 || Ti1) {
				if (Q1 == 0) continue;
				if (Q1 == Tu1) continue;             // Q/T collapse
				if (Q1 == F1) continue;              // Q/F collapse
				if (Tu1 == F1 && Ti1 == 0) continue; // T/F collapse
				if (Tu1 == 0 && Ti1 == 0) continue;  // Q?0:F -> F?!Q:0
				if (Tu1 == 0 && F1 == 0) continue;   // Q?!0:0 -> Q
			}

			// check adjacent endpoints
			uint32_t nextSlot1 = 0;
			if (Q1 < NSTART && Q1 > nextSlot1) {
				if (Q1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}
			if (Tu1 < NSTART && Tu1 > nextSlot1) {
				if (Tu1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}
			if (F1 < NSTART && F1 > nextSlot1) {
				if (F1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}

			uint32_t nextNode1 = NSTART;
			uint32_t tlQ;
			if (Q1 == Tu1) {
				tlQ = Q1;
			} else {
				tlQ = nextNode1++;
				tree.N[tlQ].Q = Q1;
				tree.N[tlQ].T = Tu1 ^ (Ti1 ? IBIT : 0);
				tree.N[tlQ].F = F1;
			}

			//@formatter:off
			for (uint32_t Q2 = 0; Q2 < nextNode1; Q2++)
			for (uint32_t Tu2 = 0; Tu2 < nextNode1; Tu2++)
			for (uint32_t Ti2 = 0; Ti2 < 2; Ti2++)
			for (uint32_t F2 = 0; F2  < nextNode1; F2++) {
			//@formatter:on

				// validate
				if (Q2 != Tu2 || Q2 != F2 || Ti2) {
					if (Q2 == 0 && (Tu2 || Ti2 || F2)) continue;
					if (Q2 == Tu2) continue;             // Q/T collapse
					if (Q2 == F2) continue;              // Q/F collapse
					if (Tu2 == F2 && Ti2 == 0) continue; // T/F collapse
					if (Tu2 == 0 && Ti2 == 0) continue;  // Q?0:F -> F?!Q:0
					if (Tu2 == 0 && F2 == 0) continue;   // Q?!0:0 -> Q
					// assume runtime detects duplicates
					if (Q2 == Q1 && Tu2 == Tu1 && Ti2 == Ti1 && F2 == F1) continue;
				}


				// check adjacent endpoints
				uint32_t nextSlot2 = nextSlot1;
				if (Q2 < NSTART && Q2 > nextSlot2) {
					if (Q2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}
				if (Tu2 < NSTART && Tu2 > nextSlot2) {
					if (Tu2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}
				if (F2 < NSTART && F2 > nextSlot2) {
					if (F2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}

				uint32_t nextNode2 = nextNode1;
				uint32_t tlTu;
				if (Q2 == Tu2) {
					tlTu = Q2;
				} else {
					tlTu = nextNode2++;
					tree.N[tlTu].Q = Q2;
					tree.N[tlTu].T = Tu2 ^ (Ti2 ? IBIT : 0);
					tree.N[tlTu].F = F2;
				}

				//@formatter:off
				for (uint32_t Q3 = 0; Q3 < nextNode2; Q3++)
				for (uint32_t Tu3 = 0; Tu3  < nextNode2; Tu3++)
				for (uint32_t Ti3 = 0; Ti3 < 2; Ti3++)
				for (uint32_t F3 = 0; F3  < nextNode2; F3++) {
				//@formatter:on

					// validate
					if (Q3 != Tu3 || Q3 != F3 || Ti3) {
						if (Q3 == 0 && (Tu3 || Ti3 || F3)) continue;
						if (Q3 == Tu3) continue;             // Q/T collapse
						if (Q3 == F3) continue;              // Q/F collapse
						if (Tu3 == F3 && Ti3 == 0) continue; // T/F collapse
						if (Tu3 == 0 && Ti3 == 0) continue;  // Q?0:F -> F?!Q:0
						if (Tu3 == 0 && F3 == 0) continue;   // Q?!0:0 -> Q
						// assume runtime detects duplicates
						if (Q3 == Q1 && Tu3 == Tu1 && Ti3 == Ti1 && F3 == F1) continue;
						if (Q3 == Q2 && Tu3 == Tu2 && Ti3 == Ti2 && F3 == F2) continue;
					}

					// check adjacent endpoints
					uint32_t nextSlot3 = nextSlot2;
					if (Q3 < NSTART && Q3 > nextSlot3) {
						if (Q3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}
					if (Tu3 < NSTART && Tu3 > nextSlot3) {
						if (Tu3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}
					if (F3 < NSTART && F3 > nextSlot3) {
						if (F3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}

					uint32_t nextNode3 = nextNode2;
					uint32_t tlF;
					if (Q3 == Tu3) {
						tlF = Q3;
					} else {
						tlF = nextNode3++;
						tree.N[tlF].Q = Q3;
						tree.N[tlF].T = Tu3 ^ (Ti3 ? IBIT : 0);
						tree.N[tlF].F = F3;
					}

					for (uint32_t tlTi = 0; tlTi < 2; tlTi++) {
						if (tick && context_t::opt_verbose >= VERBOSE_TICK) {
							if (progressSpeed != 0)
								progressSpeed += ((int) (progress - progressLast) - progressSpeed) * progressCoef; // update
							else
								progressSpeed = (int) (progress - progressLast); // first time

							progressCoef *= progressCoefIncrement;
							if (progressCoef > progressCoefMax)
								progressCoef  = progressCoefMax;

							int perInterval = progressSpeed;
							if (!perInterval) perInterval = 1;
							int perSecond = perInterval / opt_timer;
							if (!perSecond) perSecond = 1;
							int eta = (int) ((progressHi - progress) / perSecond);

							int etaH = eta / 3600;
							eta %= 3600;
							int etaM = eta / 60;
							eta %= 60;
							int etaS = eta;

							progressLast = progress;
							tick         = 0;

							fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d %d %d %d(%.2f%%) %d(%.2f%%) %d(%.2f%%) %d(%.2f%%) %d(%.2f%%)",
								timeAsString(), progress, perSecond, progress * 100.0 / progressHi, etaH, etaM, etaS,
								gCntFound, gCntTree,
								gCntNode[0], gCntNode[0] * 100.0 / gCntFound,
								gCntNode[1], gCntNode[1] * 100.0 / gCntFound,
								gCntNode[2], gCntNode[2] * 100.0 / gCntFound,
								gCntNode[3], gCntNode[3] * 100.0 / gCntFound,
								gCntNode[4], gCntNode[4] * 100.0 / gCntFound);
						}
						progress++;

						// construct a tree
						tree.N[nextNode3].Q = tlQ;
						tree.N[nextNode3].T = tlTu ^ (tlTi ? IBIT : 0);
						tree.N[nextNode3].F = tlF;

						tree.root  = nextNode3;
						tree.count = nextNode3 + 1;

						foundTree(tree);
					}
				}
			}
		}
		if (context_t::opt_verbose >= VERBOSE_SUMMARY)
			fprintf(stderr, "\r\e[K[%s] numData=%d numDataTree=%d\n",
				timeAsString(),
				numData, numDataTree);

		fprintf(stderr, "\r\e[K[%s] cntFound=%d cntTree=%d cntNode0=%d(%.2f%%) cntNode1=%d(%.2f%%) cntNode2=%d(%.2f%%) cntNode3=%d(%.2f%%) cntNode4=%d(%.2f%%)\n",
			timeAsString(),
			gCntFound, gCntTree,
			gCntNode[0], gCntNode[0] * 100.0 / gCntFound,
			gCntNode[1], gCntNode[1] * 100.0 / gCntFound,
			gCntNode[2], gCntNode[2] * 100.0 / gCntFound,
			gCntNode[3], gCntNode[3] * 100.0 / gCntFound,
			gCntNode[4], gCntNode[4] * 100.0 / gCntFound);
		if (progress != progressHi)
			fprintf(stderr, "[progressHi=%ld]\n", progress);

		fprintf(stderr, "numData=%d\n", numData);

	}
#endif

	uint32_t foundNode(uint32_t pos, const tinyTree_t &tree, uint32_t nid, uint32_t *slots, uint32_t &nextSlot, char *label, uint32_t &lenLabel, uint32_t &lenBlock) {

		const unsigned NSTART     = tinyTree_t::TINYTREE_NSTART;

		if (nid < NSTART || normVersion[nid] == iVersion) {

			/*
			 * id. Marked by two consecutive id's as if T=Q
			 *
			 */

			// make relative
			if (normVersion[nid] != iVersion) {
				slots[nextSlot]  = nid;
				normVersion[nid] = iVersion;
				normMap[nid]     = nextSlot++;
			}
			nid = normMap[nid];
			assert(nid < lenBlock);

			// allocate block
			if (pos == numData) {
				numData += lenBlock;
				gDataLength[pos] = lenBlock;
				gDataLabel[pos]  = strdup(label);
				gDataOwner[pos]  = ctx.progress;
			}
			assert(gDataLength[pos] == lenBlock);
			label[lenLabel++] = "0abcdefghijkl..."[nid];
			// add state
			if (!gData[pos + nid]) {
				gData[pos + nid]      = numData;
				gDataOwner[pos + nid] = ctx.progress;
			}
			pos = gData[pos + nid];
			lenBlock++;

			/*
			 * Second id
			 */

			// allocate block
			if (pos == numData) {
				numData += lenBlock;
				gDataLength[pos] = lenBlock;
				gDataLabel[pos]  = strdup(label);
				gDataOwner[pos]  = ctx.progress;
			}
			assert(gDataLength[pos] == lenBlock);
			label[lenLabel++] = '.';

			// add state
			if (!gData[pos + nid]) {
				gData[pos + nid]      = numData;
				gDataOwner[pos + nid] = ctx.progress;
			}
			pos = gData[pos + nid];
			lenBlock++;

		} else {
			const tinyNode_t *pNode = tree.N + nid;
			uint32_t         Q      = pNode->Q;
			uint32_t         Tu     = pNode->T & ~IBIT;
			uint32_t         Ti     = (pNode->T & IBIT) ? 1 : 0;
			uint32_t         F      = pNode->F;

			/*
			 * Q
			 */
			// make relative
			if (normVersion[Q] != iVersion) {
				slots[nextSlot] = Q;
				normVersion[Q]  = iVersion;
				normMap[Q]      = nextSlot++;
			}
			Q = normMap[Q];
			assert(Q < lenBlock);

			// allocate block
			if (pos == numData) {
				numData += lenBlock;
				gDataLength[pos] = lenBlock;
				gDataLabel[pos]  = strdup(label);
				gDataOwner[pos]  = ctx.progress;
			}
			assert(gDataLength[pos] == lenBlock);
			label[lenLabel++] = "0abcdefghijkl..."[Q];
			// add state
			if (!gData[pos + Q]) {
				gData[pos + Q]      = numData;
				gDataOwner[pos + Q] = ctx.progress;
			}
			pos = gData[pos + Q];
			lenBlock++;

			/*
			 * Tu
			 */
			// make relative
			if (normVersion[Tu] != iVersion) {
				slots[nextSlot] = Tu;
				normVersion[Tu] = iVersion;
				normMap[Tu]     = nextSlot++;
			}
			Tu = normMap[Tu];
			assert(Tu < lenBlock);

			// allocate block
			if (pos == numData) {
				numData += lenBlock;
				gDataLength[pos] = lenBlock;
				gDataLabel[pos]  = strdup(label);
				gDataOwner[pos]  = ctx.progress;
			}
			assert(gDataLength[pos] == lenBlock);
			label[lenLabel++] = "0abcdefghijkl..."[Tu];
			// add state
			if (!gData[pos + Tu]) {
				gData[pos + Tu]      = numData;
				gDataOwner[pos + Tu] = ctx.progress;
			}
			pos = gData[pos + Tu];
			lenBlock++;

			/*
			 * F
			 */
			// make relative
			if (normVersion[F] != iVersion) {
				slots[nextSlot] = F;
				normVersion[F]  = iVersion;
				normMap[F]      = nextSlot++;
			}
			F = normMap[F];
			assert(F < lenBlock);

			// allocate block
			if (pos == numData) {
				numData += lenBlock;
				gDataLength[pos] = lenBlock;
				gDataLabel[pos]  = strdup(label);
				gDataOwner[pos]  = ctx.progress;
			}
			assert(gDataLength[pos] == lenBlock);
			label[lenLabel++] = "0abcdefghijkl..."[F];
			// add state
			if (!gData[pos + F]) {
				gData[pos + F]      = numData;
				gDataOwner[pos + F] = ctx.progress;
			}
			pos = gData[pos + F];
			lenBlock++;

			/*
			 * Ti
			 */
			// allocate block
			if (pos == numData) {
				numData += 2;
				gDataLength[pos] = 2;
				gDataLabel[pos]  = strdup(label);
				gDataOwner[pos]  = ctx.progress;
			}
			assert(gDataLength[pos] == 2);
			label[lenLabel++] = "?!.............."[Ti];
			// add state
			if (!gData[pos + Ti]) {
				gData[pos + Ti]      = numData;
				gDataOwner[pos + Ti] = ctx.progress;
			}
			pos = gData[pos + Ti];

			/*
			 * node-id
			 * NOTE: this would be the only entry in the index-block/chunk. Allocate slot but not a block.
			 */

			// make relative
			if (normVersion[nid] != iVersion) {
				slots[nextSlot]  = nid;
				normVersion[nid] = iVersion;
				normMap[nid]     = nextSlot++;
				lenBlock++; // increase blocksize because of extra slot
			}
		}
		return pos;
	}

	void main(void) {
		/*
		 * Create evaluator vector for 4n9.
		 */

		footprint_t *pEval = (footprint_t *) ctx.myAlloc("pEval", tinyTree_t::TINYTREE_NEND, sizeof(*pEval));

		// set 64bit slice to zero
		for (unsigned i = 0; i < tinyTree_t::TINYTREE_NEND; i++)
			for (unsigned j = 0; j<footprint_t::QUADPERFOOTPRINT; j++)
				pEval[i].bits[j] = 0;

		// set footprint for 64bit slice
		assert(MAXSLOTS == 9);
		assert(tinyTree_t::TINYTREE_KSTART == 1);
		for (unsigned i = 0; i < (1 << MAXSLOTS); i++) {
			// v[0+(i/64)] should be 0
			if (i & (1 << 0)) pEval[tinyTree_t::TINYTREE_KSTART + 0].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 1)) pEval[tinyTree_t::TINYTREE_KSTART + 1].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 2)) pEval[tinyTree_t::TINYTREE_KSTART + 2].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 3)) pEval[tinyTree_t::TINYTREE_KSTART + 3].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 4)) pEval[tinyTree_t::TINYTREE_KSTART + 4].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 5)) pEval[tinyTree_t::TINYTREE_KSTART + 5].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 6)) pEval[tinyTree_t::TINYTREE_KSTART + 6].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 7)) pEval[tinyTree_t::TINYTREE_KSTART + 7].bits[i / 64] |= 1LL << (i % 64);
			if (i & (1 << 8)) pEval[tinyTree_t::TINYTREE_KSTART + 8].bits[i / 64] |= 1LL << (i % 64);
		}

		/*
		 * Create trees.
		 * NOTE: `baseTree_t` are not allowed because they depend on data this program generates
		 */
		tinyTree_t tree(ctx);
		tinyTree_t testTree(ctx);

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "\r\e[K[%s] Find sources [progress(speed) eta cntFound cntShrink cntNode0 cntNode1 cntNode2 cntNode3 cntNode4]\n", ctx.timeAsString());

		// setup first block
		numData = opt_first;
		gDataLength[numData] = 2;
		gDataLabel[numData]  = strdup("start");
		numData += 2;

		numDataTree = 1;

		/*
		 * generate all slot-relative combinations
		 * NOTE: allow triple-zero and triple-self
		 *
		 * Construction is old (not ancient) code.
		 * Keeping it for posterity and as an independent implementation to optionally compare with the generator.
		 */

		ctx.setupSpeed(940140); // 198072;

		const unsigned NSTART     = tinyTree_t::TINYTREE_NSTART;
		const unsigned NAMELENGTH = tinyTree_t::TINYTREE_NAMELEN;

		//@formatter:off
		for (uint32_t Q1 = 1; Q1 < NSTART+0; Q1++)
		for (uint32_t Tu1 = 0; Tu1 < NSTART+0; Tu1++)
		for (uint32_t Ti1 = 0; Ti1 < 2; Ti1++)
		for (uint32_t F1 = 0; F1 < NSTART+0; F1++) {
		//@formatter:on

			tree.clearTree();
			
			// validate
			if (Q1 != Tu1 || Q1 != F1 || Ti1) {
				if (Q1 == 0) continue;
				if (Q1 == Tu1) continue;             // Q/T collapse
				if (Q1 == F1) continue;              // Q/F collapse
				if (Tu1 == F1 && Ti1 == 0) continue; // T/F collapse
				if (Tu1 == 0 && Ti1 == 0) continue;  // Q?0:F -> F?!Q:0
				if (Tu1 == 0 && F1 == 0) continue;   // Q?!0:0 -> Q
			}

			// check adjacent endpoints
			uint32_t nextSlot1 = 0;
			if (Q1 < NSTART && Q1 > nextSlot1) {
				if (Q1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}
			if (Tu1 < NSTART && Tu1 > nextSlot1) {
				if (Tu1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}
			if (F1 < NSTART && F1 > nextSlot1) {
				if (F1 != nextSlot1 + 1) continue;
				nextSlot1++;
			}

			uint32_t nextNode1 = NSTART;
			uint32_t tlQ;
			if (Q1 == Tu1) {
				tlQ = Q1;
			} else {
				tlQ = nextNode1++;
				tree.N[tlQ].Q = Q1;
				tree.N[tlQ].T = Tu1 ^ (Ti1 ? IBIT : 0);
				tree.N[tlQ].F = F1;
			}

			//@formatter:off
			for (uint32_t Q2 = 0; Q2 < nextNode1; Q2++)
			for (uint32_t Tu2 = 0; Tu2 < nextNode1; Tu2++)
			for (uint32_t Ti2 = 0; Ti2 < 2; Ti2++)
			for (uint32_t F2 = 0; F2  < nextNode1; F2++) {
			//@formatter:on

				// validate
				if (Q2 != Tu2 || Q2 != F2 || Ti2) {
					if (Q2 == 0 && (Tu2 || Ti2 || F2)) continue;
					if (Q2 == Tu2) continue;             // Q/T collapse
					if (Q2 == F2) continue;              // Q/F collapse
					if (Tu2 == F2 && Ti2 == 0) continue; // T/F collapse
					if (Tu2 == 0 && Ti2 == 0) continue;  // Q?0:F -> F?!Q:0
					if (Tu2 == 0 && F2 == 0) continue;   // Q?!0:0 -> Q
					// assume runtime detects duplicates
					if (Q2 == Q1 && Tu2 == Tu1 && Ti2 == Ti1 && F2 == F1) continue;
				}


				// check adjacent endpoints
				uint32_t nextSlot2 = nextSlot1;
				if (Q2 < NSTART && Q2 > nextSlot2) {
					if (Q2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}
				if (Tu2 < NSTART && Tu2 > nextSlot2) {
					if (Tu2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}
				if (F2 < NSTART && F2 > nextSlot2) {
					if (F2 != nextSlot2 + 1) continue;
					nextSlot2++;
				}

				uint32_t nextNode2 = nextNode1;
				uint32_t tlTu;
				if (Q2 == Tu2) {
					tlTu = Q2;
				} else {
					tlTu = nextNode2++;
					tree.N[tlTu].Q = Q2;
					tree.N[tlTu].T = Tu2 ^ (Ti2 ? IBIT : 0);
					tree.N[tlTu].F = F2;
				}

				//@formatter:off
				for (uint32_t Q3 = 0; Q3 < nextNode2; Q3++)
				for (uint32_t Tu3 = 0; Tu3  < nextNode2; Tu3++)
				for (uint32_t Ti3 = 0; Ti3 < 2; Ti3++)
				for (uint32_t F3 = 0; F3  < nextNode2; F3++) {
				//@formatter:on

					// validate
					if (Q3 != Tu3 || Q3 != F3 || Ti3) {
						if (Q3 == 0 && (Tu3 || Ti3 || F3)) continue;
						if (Q3 == Tu3) continue;             // Q/T collapse
						if (Q3 == F3) continue;              // Q/F collapse
						if (Tu3 == F3 && Ti3 == 0) continue; // T/F collapse
						if (Tu3 == 0 && Ti3 == 0) continue;  // Q?0:F -> F?!Q:0
						if (Tu3 == 0 && F3 == 0) continue;   // Q?!0:0 -> Q
						// assume runtime detects duplicates
						if (Q3 == Q1 && Tu3 == Tu1 && Ti3 == Ti1 && F3 == F1) continue;
						if (Q3 == Q2 && Tu3 == Tu2 && Ti3 == Ti2 && F3 == F2) continue;
					}

					// check adjacent endpoints
					uint32_t nextSlot3 = nextSlot2;
					if (Q3 < NSTART && Q3 > nextSlot3) {
						if (Q3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}
					if (Tu3 < NSTART && Tu3 > nextSlot3) {
						if (Tu3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}
					if (F3 < NSTART && F3 > nextSlot3) {
						if (F3 != nextSlot3 + 1) continue;
						nextSlot3++;
					}

					uint32_t nextNode3 = nextNode2;
					uint32_t tlF;
					if (Q3 == Tu3) {
						tlF = Q3;
					} else {
						tlF = nextNode3++;
						tree.N[tlF].Q = Q3;
						tree.N[tlF].T = Tu3 ^ (Ti3 ? IBIT : 0);
						tree.N[tlF].F = F3;
					}

					for (uint32_t tlTi = 0; tlTi < 2; tlTi++) {
						ctx.progress++;

						if (ctx.tick && ctx.opt_verbose >= ctx.VERBOSE_TICK) {
							int perSecond = ctx.updateSpeed();

							int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

							int etaH = eta / 3600;
							eta %= 3600;
							int etaM = eta / 60;
							eta %= 60;
							int etaS = eta;

							fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d %d %d %d(%.2f%%) %d(%.2f%%) %d(%.2f%%) %d(%.2f%%) %d(%.2f%%)",
								ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
								gCntFound, gCntTree,
								gCntNode[0], gCntNode[0] * 100.0 / gCntFound,
								gCntNode[1], gCntNode[1] * 100.0 / gCntFound,
								gCntNode[2], gCntNode[2] * 100.0 / gCntFound,
								gCntNode[3], gCntNode[3] * 100.0 / gCntFound,
								gCntNode[4], gCntNode[4] * 100.0 / gCntFound);

							ctx.tick = 0;
						}

						// construct a tree
						tree.count = nextNode3;
						tree.root  = tree.addNormaliseNode(tlQ, tlTu ^ (tlTi ? IBIT : 0), tlF);

						/*
						 * @date 2021-06-12 00:00:57
						 *
						 * At this point, the tree contains a valid pattern
						 * With all the 9 slots and 3 heads available,
						 * try all combinations for a better top-level-node
						 * `--rewrite` will enable (multi-node) rewriting to best candidate found.
						 */

						// add to state table
						char     label[5 * 5 + 1];
						uint32_t slots[1 + MAXSLOTS + 3];
						uint32_t dataPos  = opt_first;
						uint32_t nextSlot = 0;
						uint32_t lenLabel = 0;
						uint32_t lenBlock = 2; // first block must be able to store "0a"

						memset(label, 0, sizeof(label));
						++iVersion;
						assert(iVersion != 0);

						// preset zero
						slots[nextSlot] = 0;
						normVersion[0]  = iVersion;
						normMap[0]      = nextSlot++;

						dataPos = foundNode(dataPos, tree, tlQ, slots, nextSlot, label, lenLabel, lenBlock);
						dataPos = foundNode(dataPos, tree, tlTu, slots, nextSlot, label, lenLabel, lenBlock);
						dataPos = foundNode(dataPos, tree, tlF, slots, nextSlot, label, lenLabel, lenBlock);

						if (numData >= this->maxData)
							ctx.fatal("\n[%s %s:%u storage full %d]\n", __FUNCTION__, __FILE__, __LINE__, this->maxData);

						/*
						 * capture name, origScore, footprint and top-level Q/T/F
						 */

						char origName[NAMELENGTH + 1];
						char skin[31];
						tree.saveString(tree.root, origName, skin);

//						printf("@@ %s\n", origName);

						uint32_t origScore = tree.calcScoreName(origName);

						tree.eval(pEval);
						footprint_t origFoot = pEval[tree.root];

						uint32_t origData = tlTi << 12 | normMap[tlQ] << 8 | normMap[tlTu] << 4 | normMap[tlF];

						/*
						 * Find highest scoring target
						 * Candidates are all permutations of 3-out-of-N slots
						 */

						tinyTree_t bestTree(ctx);
						char       bestName[NAMELENGTH + 1] = {0};
						uint32_t   bestScore                = 0;
						uint32_t   bestSize                 = 0;
						uint32_t   bestData                 = 0;

						{
							// copy tree
							testTree.clearTree();
							testTree.N[NSTART + 0] = tree.N[NSTART + 0];
							testTree.N[NSTART + 1] = tree.N[NSTART + 1];
							testTree.N[NSTART + 2] = tree.N[NSTART + 2];
							testTree.N[NSTART + 3] = tree.N[NSTART + 3];
							testTree.root  = tree.root;
							testTree.count = tree.count;


							//@formatter:off
							for (uint32_t testQ = 0; testQ < nextNode3; testQ++)
							for (uint32_t testTu = 0; testTu < nextNode3; testTu++)
							for (uint32_t testTi = 0; testTi < 2; testTi++)
							for (uint32_t testF = 0; testF < nextNode3; testF++) {
							//@formatter:on

								if (testQ < NSTART && testQ > nextSlot3) continue;
								if (testTu < NSTART && testTu > nextSlot3) continue;
								if (testF < NSTART && testF > nextSlot3) continue;

								// validate
								if (testQ != testTu || testQ != testF || testTi) {
									if (testQ == 0 && (testTu || testTi || testF)) continue;
									if (testQ == testTu) continue;             // Q/T collapse
									if (testQ == testF) continue;              // Q/F collapse
									if (testTu == testF && testTi == 0) continue; // T/F collapse
									if (testTu == 0 && testTi == 0) continue;  // Q?0:F -> F?!Q:0
									if (testTu == 0 && testF == 0) continue;   // Q?!0:0 -> Q
								}

								/*
								 * Load into a test tree
								 */

								testTree.N[NSTART + 0] = tree.N[NSTART + 0];
								testTree.N[NSTART + 1] = tree.N[NSTART + 1];
								testTree.N[NSTART + 2] = tree.N[NSTART + 2];
								testTree.count = tree.count;
								testTree.root = testTree.addNormaliseNode(testQ, testTu ^ (testTi ? IBIT : 0), testF);

								// create a data word before tree changes (references will change when tree shrinks)
								uint32_t testData = (testTi ? 1 : 0) << 12 | normMap[testQ] << 8 | normMap[testTu] << 4 | normMap[testF];

								// reload tree for optimal name and origScore
								const char *pName = testTree.saveString(testTree.root);
								testTree.loadStringFast(pName);

								// test if footprint match
								testTree.eval(pEval);
								if (!origFoot.equals(pEval[testTree.root]))
									continue;

								// determine if better target
								uint64_t testScore = testTree.calcScoreName(pName);
								if (bestName[0] == 0 || testScore < bestScore || (testScore == bestScore && testTree.compare(testTree.root, &bestTree, bestTree.root) < 0)) {
									// rember best candidate
									for (uint32_t iNode=NSTART; iNode<tree.count; iNode++)
										bestTree.N[iNode] = tree.N[iNode];
									bestTree.root = tree.root;
									testTree.saveString(testTree.root, bestName, NULL);
									bestScore = testScore;
									bestSize  = testTree.count - NSTART;

									if (testTree.root < NSTART) {
										bestData = REWRITEMASK_COLLAPSE | normMap[testTree.root]; // collapse
									} else {
										bestData = testData;
										if (bestData == origData)
											bestData |= REWRITEMASK_FOUND; // mark no rewrite required

										// merge power for statistics
										bestData |= (tree.count - testTree.count) << REWRITEFLAG_POWER;
									}
								}
							}
							assert(bestName[0]);
						}

#ifdef ENABLE_REWRITE_DESTRUCTIVE
						/*
						 * OPTIONAL: Test if there is a rewrite that shrinks but is destructive
						 */
						{
							// start with footprint
							uint32_t ix     = lookupPrint(origFoot);
							uint32_t iPrint = printIndex[ix];

							if (iPrint) {
								print_t *pPrint = prints + iPrint;
								if (pPrint->size < bestSize) {
									strcpy(bestName, pPrint->name);
									bestScore = pPrint->origScore;
									bestSize = pPrint->size;

									// encode tree, max 3 nodes
									assert(pPrint->size > 0 && pPrint->size <= 3);

									testTree.loadStringFast(pPrint->name);

									uint64_t treedata = 0;

									for (uint32_t i=testTree.root; i>=NSTART; i--) {
										const tinyNode_t *pNode = testTree.N + i;
										uint32_t Q  = pNode->Q;
										uint32_t Tu = pNode->T & ~IBIT;
										uint32_t Ti = pNode->T & IBIT;
										uint32_t F  = pNode->F;

										// encode the runtime endpoints
										if (Q < NSTART)
											Q =  normMap[Q];
										if (Tu < NSTART)
											Tu =  normMap[Tu];
										if (F < NSTART)
											F =  normMap[F];

										treedata = treedata << 16 | (Ti?1:0) << 12 | Q << 8 | Tu << 4 | F << 0;
									}

									bestData = REWRITEMASK_TREE | numDataTree; // encode tree id

									gDataTree[numDataTree++] = treedata; // save 64bits data

									if (this->numDataTree >= this->maxDataTree)
										context_t::fatal("\n[%s %s:%u gDataTree full %d]\n", __FUNCTION__, __FILE__, __LINE__, this->maxDataTree);
								}
							}
						}
#endif

						/*
						 * update counters
						 */
						gCntFound++;


						if (bestData & REWRITEMASK_TREE) {
							gCntTree++;
							gCntNode[bestSize]++;
						} else if (!(bestData & (1 << 31))) {
							gCntNode[bestSize]++;
						}

						/*
						 * Note, last Ti is not an index but an offset
						 */

						// allocate block
						if (dataPos == numData) {
							numData += 2;
							gDataLength[dataPos] = 2;
						}
						assert(gDataLength[dataPos] == 2);

						// add state
						// Collisions might exist (like "a0b!000?a0b!?"), as long as their rewrite are the same
						assert(gData[dataPos + tlTi] == 0 || gData[dataPos + tlTi] == bestData);

						// save rewrite data
						gData[dataPos + tlTi]      = bestData;
						gDataOwner[dataPos + tlTi] = ctx.progress;

						// save name
						label[lenLabel] = tlTi ? '!' : '?';
						asprintf(&gDataLabel[dataPos + tlTi],
							 "%s %c%c%c%c%c%c%c%c%c%c%c%c%c (%x) -> %s (%x)",
							 label,
							 "0abcdefghiQTF..."[Q1],
							 "0abcdefghiQTF..."[Tu1],
							 "0abcdefghiQTF..."[F1],
							 "?!.............."[Ti1],
							 "0abcdefghiQTF..."[Q2],
							 "0abcdefghiQTF..."[Tu2],
							 "0abcdefghiQTF..."[F2],
							 "?!.............."[Ti2],
							 "0abcdefghiQTF..."[Q3],
							 "0abcdefghiQTF..."[Tu3],
							 "0abcdefghiQTF..."[F3],
							 "?!.............."[Ti3],
							 "?!.............."[tlTi],
							 origScore,
							 bestName, bestScore
						);
					}
				}
			}
		}
		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "\r\e[K[%s] numData=%d numDataTree=%d\n",
				ctx.timeAsString(),
				numData, numDataTree);

		fprintf(stderr, "\r\e[K[%s] cntFound=%d cntTree=%d cntNode0=%d(%.2f%%) cntNode1=%d(%.2f%%) cntNode2=%d(%.2f%%) cntNode3=%d(%.2f%%) cntNode4=%d(%.2f%%)\n",
			ctx.timeAsString(),
			gCntFound, gCntTree,
			gCntNode[0], gCntNode[0] * 100.0 / gCntFound,
			gCntNode[1], gCntNode[1] * 100.0 / gCntFound,
			gCntNode[2], gCntNode[2] * 100.0 / gCntFound,
			gCntNode[3], gCntNode[3] * 100.0 / gCntFound,
			gCntNode[4], gCntNode[4] * 100.0 / gCntFound);
		if (ctx.progress != ctx.progressHi)
			fprintf(stderr, "[progressHi=%ld]\n", ctx.progress);

		fprintf(stderr, "numData=%d\n", numData);

		/*
		 * 64bit tree rewrite data
		 */
		printf("const uint32_t rewriteDataFirst = %d;\n", opt_first);
		printf("const uint32_t rewriteData[] = {\n");
		for (unsigned i = 0; i < numData; i++) {
			printf("/*%06x,%06x*/ 0x%08x,", i, gDataOwner[i], gData[i]);
			if (gDataLabel[i])
				printf(" // %s", gDataLabel[i]);
			printf("\n");
		}
		printf("\n};\n");

#ifdef ENABLE_REWRITE_DESTRUCTIVE
		/*
		 * OPTIONAL: 64bit tree construction data
		 */
		{
			printf("const uint64_t rewriteTree[] = {\n");
			printf("/*%06x*/ 0, // reserved\n", 0);
			for (int i=1; i<numDataTree; i++) {
				printf("/*%06x*/ 0x%lxLL,\n", i, gDataTree[i]);
			}
			printf("\n};\n");
		}
#else
			printf("const uint64_t rewriteTree[] = {0};\n");
#endif
	}

};

/*
 * I/O context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} I/O context
 */
context_t ctx;

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {genrestartdataContext_t} Application context
 */
genrewritedataContext_t app(ctx);

/**
 * @date 2020-03-18 18:09:31
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int __attribute__ ((unused)) sig) {
	(void) sig; // trick compiler t see parameter is used

	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --first=<number> [default=%d]\n", app.opt_first);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose\n");
	}
}


int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	if (isatty(1)) {
		fprintf(stderr, "stdout is a tty\n");
		exit(1);
	}

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_FIRST,
			LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",   1, 0, LO_DEBUG},
			{"first",   1, 0, LO_FIRST},
			{"help",    0, 0, LO_HELP},
			{"quiet",   2, 0, LO_QUIET},
			{"timer",   1, 0, LO_TIMER},
			{"verbose", 2, 0, LO_VERBOSE},

			{NULL,      0, 0, 0}
		};

		char optstring[64];
		char *cp                            = optstring;
		int  option_index                   = 0;

		for (int i = 0; long_options[i].name; i++) {
			if (isalpha(long_options[i].val)) {
				*cp++ = (char) long_options[i].val;

				if (long_options[i].has_arg)
					*cp++ = ':';
				if (long_options[i].has_arg == 2)
					*cp++ = ':';
			}
		}

		*cp = '\0';

		// parse long options
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_DEBUG:
			ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
			break;
		case LO_FIRST:
			app.opt_first = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_TIMER:
			ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose + 1;
			break;

		case '?':
			ctx.fatal("Try `%s --help' for more information.\n", argv[0]);
		default:
			ctx.fatal("getopt returned character code %d\n", c);
		}
	}

	/*
	 * Setup
	 */

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(1);
	}

	/*
	 * Run the main
	 */

	printf("// generated by %s\n", argv[0]);
	printf("#include <stdint.h>\n");

#ifdef ENABLE_REWRITE_DESTRUCTIVE
	ctx.collect();
#endif
	app.main();

	return 0;
}
