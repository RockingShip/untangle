#pragma GCC optimize ("O0") // usually here from within a debugger

/*
 * geval.cc
 *      Equivalent of `eval.cc` only for `groupTree_t`
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
#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <stdlib.h>
#include <unistd.h>

#include "context.h"
#include "grouptree.h"
#include "database.h"

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

/**
 * @date 2021-05-17 22:45:37
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int __attribute__ ((unused)) sig) {
	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

/**
 * @date 2021-05-13 15:30:14
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct gevalContext_t {

	enum {
		/// @constant {number} Size of footprint for `tinyTree_t` in terms of uint64_t
		QUADPERFOOTPRINT = ((1 << MAXSLOTS) / 64)
	};

	/// @var {string} name of database
	const char *opt_databaseName;
	/// @var {number} --datasize, Data vector size containing test patterns for CRC (units in uint64_t)
	unsigned   opt_dataSize;
	/// @var {number} header flags
	uint32_t   opt_flagsSet;
	/// @var {number} header flags
	uint32_t   opt_flagsClr;
	/// @var {number} --maxdepth, Maximum node expansion depth for `groupTree_t`.
	unsigned   opt_maxDepth;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned   opt_maxNode;
	/// @var {number} --normalise, display names as normalised with transforms
	unsigned   opt_normalise;
	/// @global {number} --seed=n, Random seed to generate evaluator test pattern
	unsigned   opt_seed;
	/// @var {number} --speed, Speed setting for `groupTree_t`.
	unsigned   opt_speed;

	/// @var {database_t} - Database store to place results
	database_t *pStore;

	gevalContext_t() {
		opt_databaseName = "untangle.db";
		opt_dataSize     = QUADPERFOOTPRINT; // compatible with `footprint_t::QUADPERFOOTPRINT`
		opt_flagsSet     = 0;
		opt_flagsClr     = 0;
		opt_maxDepth     = groupTree_t::DEFAULT_MAXDEPTH;
		opt_maxNode      = groupTree_t::DEFAULT_MAXNODE;
		opt_normalise    = 0;
		opt_seed         = 1;
		opt_speed        = groupTree_t::DEFAULT_SPEED;
		pStore           = NULL;
	}

	/**
	 * @date 2021-06-08 23:45:32
	 *
	 * Calculate the hash of a footprint.
	 *
	 * It doesn't really have to be crc,  as long as the result has some linear distribution over index.
	 * crc32 was chosen because it has a single assembler instruction on x86 platforms.
	 *
	 * Inspired by Mark Adler's software implementation of "crc32c.c -- compute CRC-32C using the Intel crc32 instruction"
	 *
	 * @return {number} - calculate crc
	 */
	uint32_t calccrc32(uint64_t *pData, unsigned numData) const {

		static uint32_t crc32c_table[8][256];

		if (crc32c_table[0][0] == 0) {
			/*
			 * Initialize table
			 */
			uint32_t n, crc, k;
			uint32_t poly = 0x82f63b78;

			for (n = 0; n < 256; n++) {
				crc = n;

				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);
				crc = (crc & 1) ? (crc >> 1) ^ poly : (crc >> 1);

				crc32c_table[0][n] = crc;
			}
			for (n = 0; n < 256; n++) {
				crc = crc32c_table[0][n];

				for (k = 1; k < 8; k++) {
					crc = crc32c_table[0][crc & 0xff] ^ (crc >> 8);
					crc32c_table[k][n] = crc;
				}
			}

		}

		/*
		 * Calculate crc
		 */
		uint64_t crc = 0;

		while (numData > 0) {

			crc ^= *pData++;

			crc = crc32c_table[7][crc & 0xff] ^
			      crc32c_table[6][(crc >> 8) & 0xff] ^
			      crc32c_table[5][(crc >> 16) & 0xff] ^
			      crc32c_table[4][(crc >> 24) & 0xff] ^
			      crc32c_table[3][(crc >> 32) & 0xff] ^
			      crc32c_table[2][(crc >> 40) & 0xff] ^
			      crc32c_table[1][(crc >> 48) & 0xff] ^
			      crc32c_table[0][crc >> 56];

			--numData;
		}

		return crc;
	}

	/**
	 * @date 2021-06-08 21:00:46
	 *
	 * Create/load tree based on arguments
	 */
	void handleArgument(const char *inputName) {

		/*
		 * Open input tree
		 */
		groupTree_t *pTree;

		if (strchr(inputName, '.') != NULL) {

			pTree = new groupTree_t(ctx, *pStore);

			/*
			 * Load from file
			 */
			if (pTree->loadFile(inputName)) {
				json_t *jError = json_object();
				json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to load"));
				json_object_set_new_nocheck(jError, "filename", json_string(inputName));
				ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
			}

			if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
				json_t *jResult = json_object();
				json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(inputName));
				pTree->headerInfo(jResult);
				pTree->extraInfo(jResult);
				fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
				json_delete(jResult);
			}

		} else {
			/*
			 * Load from string
			 */

			pTree = new groupTree_t(ctx, *pStore, inputName, opt_maxNode, /*flags=*/0);

			if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
				json_t *jResult = json_object();

				jResult = json_object();
				pTree->summaryInfo(jResult);
				fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
				json_delete(jResult);
			}
		}

		/*
		 * Record footprints for each node to maintain the results to compare trees
		 * Each bit is an independent test.
		 * For ease of calculation, number of tests = number of words per key/node
		 */

		// setup a data vector for evaluation
		footprint_t *pFootprint = (footprint_t *) ctx.myAlloc("pFootprint", pTree->ncount, sizeof(*pFootprint));

		/*
		 * Initialise data/footprint vector
		 */
		if (pTree->nstart - pTree->kstart <= MAXSLOTS) {
			/*
			 * If there are MAXSLOTS keys, then be `eval`/`tinyTree_t` compatible
			 */
			uint32_t kstart = pTree->kstart;
			uint32_t nstart = pTree->nstart;

			uint64_t *v = (uint64_t *) pFootprint;

			// set 64bit slice to zero
			memset(pFootprint, 0, kstart * sizeof(*pFootprint));

			// set footprint for 64bit slice
			assert(MAXSLOTS == 9);
			for (unsigned i = 0; i < (1 << MAXSLOTS); i++) {
				if (kstart + 0 < nstart)
					if (i & (1 << 0)) v[(i / 64) + (kstart + 0) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 1 < nstart)
					if (i & (1 << 1)) v[(i / 64) + (kstart + 1) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 2 < nstart)
					if (i & (1 << 2)) v[(i / 64) + (kstart + 2) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 3 < nstart)
					if (i & (1 << 3)) v[(i / 64) + (kstart + 3) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 4 < nstart)
					if (i & (1 << 4)) v[(i / 64) + (kstart + 4) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 5 < nstart)
					if (i & (1 << 5)) v[(i / 64) + (kstart + 5) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 6 < nstart)
					if (i & (1 << 6)) v[(i / 64) + (kstart + 6) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 7 < nstart)
					if (i & (1 << 7)) v[(i / 64) + (kstart + 7) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
				if (kstart + 8 < nstart)
					if (i & (1 << 8)) v[(i / 64) + (kstart + 8) * QUADPERFOOTPRINT] |= 1LL << (i % 64);
			}

		} else {
			srand(opt_seed);

			// fill rest with random patterns
			for (unsigned iEntry = pTree->kstart; iEntry < pTree->nstart; iEntry++) {
				uint64_t *v = (uint64_t *) (pFootprint + iEntry);

				// craptastic random fill
				for (unsigned i = 0; i < opt_dataSize; i++) {
					v[i] = (uint64_t) rand();
					v[i] = (v[i] << 16) ^ (uint64_t) rand();
					v[i] = (v[i] << 16) ^ (uint64_t) rand();
					v[i] = (v[i] << 16) ^ (uint64_t) rand();
				}
			}

			// erase v[0]
			for (unsigned i = 0; i < opt_dataSize; i++)
				pFootprint[0].bits[i] = 0;
		}

		/*
		 * @date 2021-11-03 00:40:18
		 * 
		 * Evaluate test vector (n9) stored in tree.
		 * 
		 * The tree can be accessed from algebra side (Q/T/F)
		 * as well as from the footprint side (2^9=512 bits)
		 * 1- Load QTF structure
		 * 2- Convert to footprint
		 * 3- Lookup with `lookupImprintAssociative()`
		 * 4- Replace with improeved most optimal replacement for most optimal storage
		 * - 4n9-pure (beval) 
		 * geval for groupTree removing structure from the equasion
		 * nodes are expressions of polarised sid/tid, where tid=0
		 * the reduced number of sids 
		 * the cross-poduct. 
		 * from one extreme to Cartesian products of primes
		 * 
		 *  Cartesian product (geval) using grupTree_t
		 *  where the nodes are not what you seek, but what you ask.
		 *  4n9-pure makes is the most minimal dataset covering whole 5n9 addressing space.
		 *  Average of 1.? members per signature group.
		 *  1.?^3 is the expected average of increase of storage.
		 *  
		 * 
		 * maximum storageMost optimal replacement function 
		 * 
		 */

		// load endpoints into placeholder slots
		// load signature into tree
		// apply tree on placeholders
		// `pFootprint` is a replacement for "number", it is a bit vector with the Q/T/F operator
		// the whole tree assumes `tid=0`

		// find group headers
		for (uint32_t iGroup = pTree->nstart; iGroup < pTree->ncount; iGroup++) {
			if (pTree->N[iGroup].gid != iGroup)
				continue; // not a group header

			// top-level components	
			uint32_t Q = 0, Ti = 0, Tu = 0, F = 0;

			// walk through group list in search of a `1n9` node
			for (uint32_t iNode = pTree->N[iGroup].next; iNode != iGroup; iNode = pTree->N[iNode].next) {
				groupNode_t *pNode = pTree->N + iNode;

				// catch `1n9`
				if (pNode->sid == pStore->SID_OR) {
					Q  = pNode->slots[0];
					Ti = IBIT;
					Tu = 0;
					F  = pNode->slots[1];
					break;
				} else if (pNode->sid == pStore->SID_GT) {
					Q  = pNode->slots[0];
					Ti = IBIT;
					Tu = pNode->slots[1];
					F  = 0;
					break;
				} else if (pNode->sid == pStore->SID_NE) {
					Q  = pNode->slots[0];
					Ti = IBIT;
					Tu = pNode->slots[1];
					F  = pNode->slots[1];
					break;
				} else if (pNode->sid == pStore->SID_AND) {
					Q  = pNode->slots[0];
					Ti = 0;
					Tu = pNode->slots[1];
					F  = 0;
					break;
				} else if (pNode->sid == pStore->SID_QNTF) {
					Q  = pNode->slots[0];
					Ti = IBIT;
					Tu = pNode->slots[1];
					F  = pNode->slots[2];
					break;
				} else if (pNode->sid == pStore->SID_QTF) {
					Q  = pNode->slots[0];
					Ti = 0;
					Tu = pNode->slots[1];
					F  = pNode->slots[2];
					break;
				}
			}

			// was anything found
			if (Q == 0) {
				fprintf(stderr, "\n{\"error\":\"group misses 1n9\",\"where\":\"%s:%s:%d\",\"gid\":%u}\n",
					__FUNCTION__, __FILE__, __LINE__, iGroup);
				break;
			}

			// determine if the operator is `QTF` or `QnTF`
			if (Ti) {
				// `QnTF` for each bit in the chunk, apply the operator `"Q ? !T : F"`
				for (unsigned j = 0; j < opt_dataSize; j++)
					pFootprint[iGroup].bits[j] = (pFootprint[Q].bits[j] & ~pFootprint[Tu].bits[j]) ^ (~pFootprint[Q].bits[j] & pFootprint[F].bits[j]);
			} else {
				// `QTF` for each bit in the chunk, apply the operator `"Q ? T : F"`
				for (unsigned j = 0; j < opt_dataSize; j++)
					pFootprint[iGroup].bits[j] = (pFootprint[Q].bits[j] & pFootprint[Tu].bits[j]) ^ (~pFootprint[Q].bits[j] & pFootprint[F].bits[j]);
			}
		}

		uint32_t firstcrc = 0;
		bool     differ   = false;

		for (unsigned iRoot = 0; iRoot < pTree->numRoots; iRoot++) {
			std::string name;
			std::string transform;

			const uint32_t Ri = pTree->roots[iRoot] & IBIT;
			const uint32_t Ru = pTree->roots[iRoot] & ~IBIT;

			// display root name
			printf("%s: ", pTree->rootNames[iRoot].c_str());

			// display footprint
			if (pTree->nstart - pTree->kstart <= MAXSLOTS) {
				// `eval` compatibility, display footprint
				if (Ri) {
					for (unsigned j = 0; j < opt_dataSize; j++)
						printf("%016lx ", pFootprint[Ru].bits[j] ^ ~0U);
				} else {
					for (unsigned j = 0; j < opt_dataSize; j++)
						printf("%016lx ", pFootprint[Ru].bits[j]);
				}
			}

			// display CRC
			unsigned crc32 = calccrc32(pFootprint[Ru].bits, opt_dataSize);
			// Inverted `T` is a concept not present in footprints. As a compromise, invert the result.
			if (Ri)
				crc32 ^= 0xffffffff;
			printf("{%08x} ", crc32);

			if (iRoot == 0)
				firstcrc = crc32;
			else if (firstcrc != crc32)
				differ = true;

			// display expression
			if (opt_normalise) {
				name = pTree->saveString(pTree->roots[iRoot], &transform);
				printf(": %s/%s", name.c_str(), transform.c_str());
			} else {
				name = pTree->saveString(pTree->roots[iRoot]);
				printf(": %s", name.c_str());
			}

			printf("\n");
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY && pTree->estart - pTree->ostart > 1) {
			if (differ)
				fprintf(stderr, "crc DIFFER\n");
			else
				fprintf(stderr, "crc same\n");

			if (differ)
				exit(1);
		}

		ctx.myFree("pFootprint", pFootprint);

		delete pTree;
	}

};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {gevalContext_t} Application context
 */
gevalContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <pattern> ...\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-D --database=<filename>   Database to query [default=%s]\n", app.opt_databaseName);
		fprintf(stderr, "\t   --explain\n");
		fprintf(stderr, "\t-n --normalise             Display pattern as: normalised/transform\n");
		fprintf(stderr, "\t-t --numtests=<seconds>    [default=%d]\n", app.opt_dataSize);
		fprintf(stderr, "\t   --maxdeph=<number>      Maximum node expansion depth [default=%d]\n", app.opt_maxDepth);
		fprintf(stderr, "\t   --maxnode=<number>      Maximum tree nodes [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet                 Say less\n");
		fprintf(stderr, "\t   --seed=n                Random seed to generate evaluator test pattern. [Default=%u]\n", app.opt_seed);
		fprintf(stderr, "\t   --speed=<number>        Speed setting [default=%d]\n", app.opt_speed);
		fprintf(stderr, "\t-v --verbose               Say more\n");
		fprintf(stderr, "\t   --timer=<seconds>       [default=%d]\n", ctx.opt_timer);

		fprintf(stderr, "\t   --[no-]paranoid [default=%s]\n", ctx.flags & ctx.MAGICMASK_PARANOID ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure [default=%s]\n", ctx.flags & ctx.MAGICMASK_PURE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]rewrite [default=%s]\n", ctx.flags & ctx.MAGICMASK_REWRITE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]cascade [default=%s]\n", ctx.flags & ctx.MAGICMASK_CASCADE ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]shrink [default=%s]\n", ctx.opt_flags &  ctx.MAGICMASK_SHRINK ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]pivot3 [default=%s]\n", ctx.opt_flags &  ctx.MAGICMASK_PIVOT3 ? "enabled" : "disabled");
	}
}

/**
 * @date 2021-05-13 15:28:31
 *
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context.
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 */
int main(int argc, char *argv[]) {
	setlinebuf(stdout);

	for (;;) {
		enum {
			LO_HELP     = 1, LO_DEBUG, LO_EXPLAIN, LO_MAXDEPTH, LO_MAXNODE, LO_SEED, LO_SPEED, LO_TIMER,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_DATABASE = 'D', LO_DATASIZE = 't', LO_NORMALISE = 'n', LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"database",    1, 0, LO_DATABASE},
			{"datasize",    1, 0, LO_DATASIZE},
			{"debug",       1, 0, LO_DEBUG},
			{"explain",     0, 0, LO_EXPLAIN},
			{"help",        0, 0, LO_HELP},
			{"maxdepth",    1, 0, LO_MAXDEPTH},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"normalise",   0, 0, LO_NORMALISE},
			{"quiet",       2, 0, LO_QUIET},
			{"seed",        1, 0, LO_SEED},
			{"speed",       1, 0, LO_SPEED},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",     2, 0, LO_VERBOSE},
			//
			{"paranoid",    0, 0, LO_PARANOID},
			{"no-paranoid", 0, 0, LO_NOPARANOID},
			{"pure",        0, 0, LO_PURE},
			{"no-pure",     0, 0, LO_NOPURE},
			{"rewrite",     0, 0, LO_REWRITE},
			{"no-rewrite",  0, 0, LO_NOREWRITE},
			{"cascade",     0, 0, LO_CASCADE},
			{"no-cascade",  0, 0, LO_NOCASCADE},
//			{"shrink",      0, 0, LO_SHRINK},
//			{"no-shrink",   0, 0, LO_NOSHRINK},
//			{"pivot3",      0, 0, LO_PIVOT3},
//			{"no-pivot3",   0, 0, LO_NOPIVOT3},
			//
			{NULL,          0, 0, 0}
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

		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_DATABASE:
			app.opt_databaseName = optarg;
			break;
		case LO_DATASIZE:
			app.opt_dataSize = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_DEBUG:
			ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
			break;
		case LO_EXPLAIN:
			ctx.opt_debug |= context_t::DEBUGMASK_EXPLAIN;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_MAXDEPTH:
			app.opt_maxDepth = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_MAXNODE:
			app.opt_maxNode = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_NORMALISE:
			app.opt_normalise++;
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_SEED:
			app.opt_seed = ::strtoul(optarg, NULL, 0);
			break;
		case LO_SPEED:
			app.opt_speed = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_TIMER:
			ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose + 1;
			break;

		case LO_PARANOID:
			app.opt_flagsSet |= ctx.MAGICMASK_PARANOID;
			app.opt_flagsClr &= ~ctx.MAGICMASK_PARANOID;
			break;
		case LO_NOPARANOID:
			app.opt_flagsSet &= ~ctx.MAGICMASK_PARANOID;
			app.opt_flagsClr |= ctx.MAGICMASK_PARANOID;
			break;
		case LO_PURE:
			app.opt_flagsSet |= ctx.MAGICMASK_PURE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_PURE;
			break;
		case LO_NOPURE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_PURE;
			app.opt_flagsClr |= ctx.MAGICMASK_PURE;
			break;
		case LO_REWRITE:
			app.opt_flagsSet |= ctx.MAGICMASK_REWRITE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_REWRITE;
			break;
		case LO_NOREWRITE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_REWRITE;
			app.opt_flagsClr |= ctx.MAGICMASK_REWRITE;
			break;
		case LO_CASCADE:
			app.opt_flagsSet |= ctx.MAGICMASK_CASCADE;
			app.opt_flagsClr &= ~ctx.MAGICMASK_CASCADE;
			break;
		case LO_NOCASCADE:
			app.opt_flagsSet &= ~ctx.MAGICMASK_CASCADE;
			app.opt_flagsClr |= ctx.MAGICMASK_CASCADE;
			break;
//			case LO_SHRINK:
//				app.opt_flags |=  ctx.MAGICMASK_SHRINK;
//				break;
//			case LO_NOSHRINK:
//				app.opt_flags &=  ~ctx.MAGICMASK_SHRINK;
//				break;
//			case LO_PIVOT3:
//				app.opt_flags |=  ctx.MAGICMASK_PIVOT3;
//				break;
//			case LO_NOPIVOT3:
//				app.opt_flags &=  ~ctx.MAGICMASK_PIVOT3;
//				break;


		case '?':
			ctx.fatal("Try `%s --help' for more information.\n", argv[0]);
		default:
			ctx.fatal("getopt returned character code %d\n", c);
		}
	}

	if (argc - optind < 1) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * Main
	 */

	// set random seed
	if (app.opt_seed)
		srand(app.opt_seed);
	else
		srand(clock());

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	// Open database
	database_t db(ctx);

	db.open(app.opt_databaseName);
	app.pStore = &db;

	// set flags
	ctx.flags = db.creationFlags;
	ctx.flags |= app.opt_flagsSet;
	ctx.flags &= ~app.opt_flagsClr;

	// display system flags when database was created
	if ((ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) || (ctx.flags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY))
		fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(ctx.flags).c_str());

	if (app.opt_maxNode < db.numSignature) {
		fprintf(stderr, "raising --maxnode to %u\n", db.numSignature);
		app.opt_maxNode = db.numSignature;
	}

	while (optind < argc) {
		app.handleArgument(argv[optind]);
		optind++;
	}

	return 0;
}
