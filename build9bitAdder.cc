//#pragma GCC optimize ("O0") // optimize on demand

/*
 * build9bitAdder.cc
 * 	4 bits adder with carry-in
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

#include <assert.h>
#include <ctype.h>
#include <getopt.h>
#include <jansson.h>
#include <stdint.h>
#include <sys/stat.h>

#include "basetree.h"

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

// used key/root names
enum {
	kZero = 0, kError, // reserved
	l0, l1, l2, l3, r0, r1, r2, r3, ci, // keys/inputs
	o0, o1, o2, o3, o4,  // roots/outputs
	NSTART, // last

	KSTART = l0,
	OSTART = o0,
};

const char *allNames[] = {
	"0", "ERROR",
	"l0", "l1", "l2", "l3", "r0", "r1", "r2", "r3", "ci",
	"o0", "o1", "o2", "o3", "o4",
};

/// @var {baseTree_t*} global reference to tree
baseTree_t *gTree = NULL;
/// @var {json_t*} validation tests
json_t     *gTests; // validation tests

struct NODE {
	uint32_t id;

	NODE() { id = 0; }

	NODE(uint32_t id) {
		assert(id == 0 || (id >= gTree->kstart && id < gTree->ncount));
		this->id = id;
	}

	NODE(NODE Q, NODE T, NODE F) { this->id = gTree->normaliseNode(Q.id, T.id, F.id); }

	NODE operator|(const NODE &other) const { return NODE(this->id, IBIT, other.id); }

	NODE operator*(const NODE &other) const { return NODE(this->id, other.id, 0); }

	NODE operator^(const NODE &other) const { return NODE(this->id, other.id ^ IBIT, other.id); }
};

/*
 * Build validation tests for the tree
 */
void validateAll(void) {
	static char keyStr[8], rootStr[8];

	for (unsigned inputs = 0; inputs < (1<<7); inputs++) {

		// the complete buffer string is read from right-to-left

		// first byte
		keyStr[0] = "0123456789abcdef"[(inputs >> 4) & 15];
		keyStr[1] = "0123456789abcdef"[(inputs >> 0) & 15];
		// second byte
		keyStr[2] = "0123456789abcdef"[(inputs >> 12) & 15];
		keyStr[3] = "0123456789abcdef"[(inputs >> 8) & 15];

		// calculate result
		unsigned outputs = 0;
		if (inputs & (1<<0)) outputs += 0x1; // l0
		if (inputs & (1<<1)) outputs += 0x2; // l1
		if (inputs & (1<<2)) outputs += 0x4; // l2
		if (inputs & (1<<3)) outputs += 0x8; // l3
		if (inputs & (1<<4)) outputs += 0x1; // r0
		if (inputs & (1<<5)) outputs += 0x2; // r1
		if (inputs & (1<<6)) outputs += 0x4; // r2
		if (inputs & (1<<7)) outputs += 0x8; // r3
		if (inputs & (1<<8)) outputs += 1; // carry-in

		// first byte
		rootStr[0] = "0123456789abcdef"[(outputs >> 4) & 15];
		rootStr[1] = "0123456789abcdef"[(outputs >> 0) & 15];
		// second byte
		rootStr[2] = "0123456789abcdef"[(outputs >> 12) & 15];
		rootStr[3] = "0123456789abcdef"[(outputs >> 8) & 15];

		json_t *jTest = json_array();
		json_array_append_new(jTest, json_string_nocheck(keyStr));
		json_array_append_new(jTest, json_string_nocheck(rootStr));

		json_array_append_new(gTests, jTest);
	}
}

/**
 * @date 2021-05-15 18:59:04
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */

struct build9bitAdderContext_t {

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;
	/// @var {number} --seed, randon number generator seed
	unsigned opt_seed;

	build9bitAdderContext_t() {
		opt_flags   = 0;
		opt_force   = 0;
		opt_maxNode = DEFAULT_MAXNODE;
		opt_seed    = 0x20171010;
	}

	/*
	 * Basic adder
	 * out = left ^ right ^ carryin
	 * carryout = carryin ? left | right : left & right
	 */
	void add(unsigned *carryout, unsigned *out, unsigned left, unsigned right, unsigned carryin) {
		/*
		 * Reminder:
		 *  [ 2] a ? ~0 : b                  "+" OR
		 *  [ 6] a ? ~b : 0                  ">" GT
		 *  [ 8] a ? ~b : b                  "^" XOR
		 *  [ 9] a ? ~b : c                  "!" QnTF
		 *  [16] a ?  b : 0                  "&" AND
		 *  [19] a ?  b : c                  "?" QTF
		 */

		unsigned leftORright = gTree->normaliseNode(left, IBIT, right);
		unsigned leftXORright = gTree->normaliseNode(left, right ^ IBIT, right);
		unsigned leftANDright = gTree->normaliseNode(left, right, 0);

		*out = gTree->normaliseNode(carryin, leftXORright ^ IBIT, leftXORright);
		*carryout = gTree->normaliseNode(carryin, leftORright, leftANDright);
	}

	/*
	 * Build the tree
	 */
	void build(void) {

		/*
		 * 	count = bits;
		 * 	count = (count & 0b1010101) + ( ((count & 0b0101010) >> 1)
		 * 	count = (count & 0b0110011) + ( ((count & 0b1001100) >> 2)
		 * 	count = (count & 0b0001111) + ( ((count & 0b1110000) >> 4)
		 */

		// count = bits;
		unsigned CI, R3, R2, R1, R0, L3, L2, L1, L0;

		L0 = gTree->kstart + 0;
		L1 = gTree->kstart + 1;
		L2 = gTree->kstart + 2;
		L3 = gTree->kstart + 3;
		R0 = gTree->kstart + 4;
		R1 = gTree->kstart + 5;
		R2 = gTree->kstart + 6;
		R3 = gTree->kstart + 7;
		CI = gTree->kstart + 8;

		// add
		unsigned C3, C2, C1, C0; // carries
		unsigned O3, O2, O1, O0; // outputs

		add(&C0, &O0, L0, R0, CI);
		add(&C1, &O1, L1, R1, C0);
		add(&C2, &O2, L2, R2, C1);
		add(&C3, &O3, L3, R3, C2);

		// store result
		gTree->roots[gTree->ostart + 0] = O0;
		gTree->roots[gTree->ostart + 1] = O1;
		gTree->roots[gTree->ostart + 2] = O2;
		gTree->roots[gTree->ostart + 3] = O3;
		gTree->roots[gTree->ostart + 4] = C3;
	}

	void main(const char *jsonFilename, const char *datFilename) {
		/*
		 * Allocate the build tree containing the complete formula
		 */

		gTree = new baseTree_t(ctx, KSTART, OSTART, NSTART/*estart*/, NSTART, NSTART/*numRoots*/, opt_maxNode, opt_flags);

		// setup key names
		for (unsigned iKey = 0; iKey < gTree->nstart; iKey++)
			gTree->keyNames[iKey] = allNames[iKey];

		// setup root names
		for (unsigned iRoot = 0; iRoot < gTree->numRoots; iRoot++) {
			gTree->rootNames[iRoot] = allNames[iRoot];

			gTree->roots[iRoot] = iRoot;
		}

		/*
		 * setup nodes
		 */
		build();

		/*
		 * Create tests as json object
		 */

		gTests = json_array();
		validateAll();

		/*
		 * Save the tree
		 */

		gTree->saveFile(datFilename);

		/*
		 * Create the meta json
		 */

		json_t *jOutput = json_object();

		// add tree meta
		gTree->headerInfo(jOutput);
		// add names/history
		gTree->extraInfo(jOutput);
		// add validations tests
		json_object_set_new_nocheck(jOutput, "tests", gTests);

		FILE *f = fopen(jsonFilename, "w");
		if (!f)
			ctx.fatal("fopen(%s) returned: %m\n", jsonFilename);

		fprintf(f, "%s\n", json_dumps(jOutput, JSON_PRESERVE_ORDER | JSON_COMPACT));

		if (fclose(f))
			ctx.fatal("fclose(%s) returned: %m\n", jsonFilename);

		/*
		 * Display json
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(datFilename));
			gTree->headerInfo(jResult);
			gTree->extraInfo(jResult);
			printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		delete gTree;
	}
};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {build9bitAdderContext_t} Application context
 */
build9bitAdderContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json> <output.dat>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t   --seed=<number> [default=%d]\n", app.opt_seed);
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --[no-]paranoid [default=%s]\n", app.opt_flags & ctx.MAGICMASK_PARANOID ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure [default=%s]\n", app.opt_flags & ctx.MAGICMASK_PURE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]rewrite [default=%s]\n", app.opt_flags & ctx.MAGICMASK_REWRITE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]cascade [default=%s]\n", app.opt_flags & ctx.MAGICMASK_CASCADE ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]shrink [default=%s]\n", app.opt_flags &  ctx.MAGICMASK_SHRINK ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]pivot3 [default=%s]\n", app.opt_flags &  ctx.MAGICMASK_PIVOT3 ? "enabled" : "disabled");
	}
}

/**
 * @date 2021-05-10 13:13:44
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
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_FORCE, LO_MAXNODE, LO_SEED,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",       1, 0, LO_DEBUG},
			{"force",       0, 0, LO_FORCE},
			{"help",        0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"quiet",       2, 0, LO_QUIET},
			{"seed",        1, 0, LO_SEED},
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
		char *cp          = optstring;
		int  option_index = 0;

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
		case LO_DEBUG:
			ctx.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
			break;
		case LO_FORCE:
			app.opt_force++;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_MAXNODE:
			app.opt_maxNode = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_SEED:
			app.opt_seed = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_TIMER:
			ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose + 1;
			break;

		case LO_PARANOID:
			app.opt_flags |= ctx.MAGICMASK_PARANOID;
			break;
		case LO_NOPARANOID:
			app.opt_flags &= ~ctx.MAGICMASK_PARANOID;
			break;
		case LO_PURE:
			app.opt_flags |= ctx.MAGICMASK_PURE;
			break;
		case LO_NOPURE:
			app.opt_flags &= ~ctx.MAGICMASK_PURE;
			break;
		case LO_REWRITE:
			app.opt_flags |= ctx.MAGICMASK_REWRITE;
			break;
		case LO_NOREWRITE:
			app.opt_flags &= ~ctx.MAGICMASK_REWRITE;
			break;
		case LO_CASCADE:
			app.opt_flags |= ctx.MAGICMASK_CASCADE;
			break;
		case LO_NOCASCADE:
			app.opt_flags &= ~ctx.MAGICMASK_CASCADE;
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

	char *jsonFilename;
	char *datFilename;

	if (argc - optind >= 2) {
		jsonFilename = argv[optind++];
		datFilename  = argv[optind++];
	} else {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!app.opt_force) {
		struct stat sbuf;
		if (!stat(jsonFilename, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", jsonFilename);
		if (!stat(datFilename, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", datFilename);
	}

	/*
	 * Main
	 */
	app.main(jsonFilename, datFilename);

	return 0;
}
