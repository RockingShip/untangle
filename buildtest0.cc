#pragma GCC optimize ("O0") // optimize on demand

/*
 * buildtest0.cc
 *	Test naming, alignment, offsets, evaluating, basics. For two bits.
 *
 *	!!! NOTE: test #8 is designed to throw an "undefined" error when validating
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

#include "context.h"
#include "basetree.h"

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

// used entry/root names
enum {
	kZero = 0, kError, // reserved
	k0, k1, k2, k3, // keys
	o0, o1, o2, // roots
	NSTART, // last

	KSTART = k0,
	OSTART = o0,
};

const char *allNames[] = {
	"0", "ERROR",
	"k0", "k1", "k2", "k3",
	"o0", "o1", "o2"
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

	NODE(NODE Q, NODE T, NODE F) { this->id = gTree->addNormaliseNode(Q.id, T.id, F.id); }

	NODE operator|(const NODE &other) const { return NODE(this->id, IBIT, other.id); }

	NODE operator*(const NODE &other) const { return NODE(this->id, other.id, 0); }

	NODE operator^(const NODE &other) const { return NODE(this->id, other.id ^ IBIT, other.id); }
};

/*
 * @date 2021-05-13 01:29:09
 *
 * Convert test to json entry
 */
void validate(const char *keyStr, const char *rootStr) {

	json_t *jTest = json_array();
	json_array_append_new(jTest, json_string_nocheck(keyStr));
	json_array_append_new(jTest, json_string_nocheck(rootStr));

	json_array_append_new(gTests, jTest);
}

/*
 * generate tests
 * NOTE: string character is a nibble representing the first 4-bits. Bits are read right-to-left. Thus: k0K1K2K3
 *
 * gTree->roots[0] =  gTree->N[k2] ? ! gTree->N[k1]     : gTree->N[k0] ;
 * gTree->roots[1] = (gTree->N[k2] ?   gTree->N[k0]     : gTree->N[k1] ) ^ IBIT;
 * gTree->roots[2] =  gTree->N[k3] ? ! 0                : gTree->roots[0]  ;
 */
void validateAll() {
	//         k3-k2-k1-k0   o2-o1-o0     o1           o0
	validate("05", "05"); // !(1?1:0)=0`   (1?!0:1)=1
	validate("02", "00"); // !(0?0:1)=0   (0?!1:0)=0

	validate("00", "02"); // !(0?0:0)=1   (0?!0:0)=0
	validate("01", "07"); // !(0:1:0)=1   (0?!0:1)=1 <
	validate("03", "05"); // !(0?1:1)=0   (0?!1:1)=1
	validate("04", "07"); // !(1?0:0)=1   (1?!0:0)=1

	validate("06", "02"); // !(1?0:1)=1   (1?!1:0)=0
	validate("07", "00"); // !(1?1:1)=0   (1?!1:1)=0

	// this one should trigger an undefined error on verification in combination with `--error`
	validate("08", "06"); // !(0?0:0)=1   (0?!0:0)=0
}

/**
 * @date 2021-05-10 13:23:43
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct buildtest0Context_t {

	/// @var {number} --error, create a node referencing `kError`
	unsigned opt_error;
	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;

	buildtest0Context_t() {
		opt_error   = 0;
		opt_flags   = 0;
		opt_force   = 0;
		opt_maxNode = DEFAULT_MAXNODE;
	}

	void main(const char *jsonFilename) {
		/*
		 * Allocate the build tree containing the complete formula
		 */

		gTree = new baseTree_t(ctx, KSTART, OSTART, OSTART, /*nstart=*/OSTART, /*numRoots=*/NSTART - OSTART, opt_maxNode, opt_flags);

		// setup entry names
		gTree->entryNames.resize(OSTART - KSTART);
		for (unsigned iEntry = 0; iEntry < OSTART - KSTART; iEntry++)
			gTree->entryNames[iEntry] = allNames[KSTART + iEntry];

		// setup root names
		gTree->numRoots = NSTART - OSTART;
		gTree->rootNames.resize(gTree->numRoots);
		for (unsigned iRoot = 0; iRoot < gTree->numRoots; iRoot++)
			gTree->rootNames[iRoot] = allNames[OSTART + iRoot];

		// setup nodes

		// gTree->roots[0] =  gTree->N[k2] ? ! gTree->N[k1]     : gTree->N[k0] ;
		// gTree->roots[1] = (gTree->N[k2] ?   gTree->N[k0]     : gTree->N[k1] ) ^ IBIT;
		// gTree->roots[2] =  gTree->N[k3] ? ! 0                : gTree->roots[0]  ;
		// because there is no operator overload available for the above

		gTree->N[gTree->ncount].Q = k2;
		gTree->N[gTree->ncount].T = k1 ^ IBIT;
		gTree->N[gTree->ncount].F = k0;
		gTree->roots[0] = gTree->ncount++; // o0 referenced once

//		uint32_t N1 = gTree->ncount;
		gTree->N[gTree->ncount].Q = k2;
		gTree->N[gTree->ncount].T = k0;
		gTree->N[gTree->ncount].F = k1;
		gTree->roots[1] = gTree->ncount++ ^ IBIT; // o1 referenced twice

//		uint32_t N2 = gTree->ncount;
		gTree->N[gTree->ncount].Q = k3;
		gTree->N[gTree->ncount].T = opt_error ? kError : IBIT;
		gTree->N[gTree->ncount].F = gTree->roots[0];
		gTree->roots[2] = gTree->ncount++; // o2 referenced once

		/*
		 * Create tests as json object
		 */

		gTests = json_array();
		validateAll();

		/*
		 * Create the meta json
		 */

		json_t *jOutput = json_object();

		// add tree meta
		gTree->summaryInfo(jOutput);
		// add names/history
		gTree->extraInfo(jOutput);

		// contents as multi-rooted
		json_object_set_new_nocheck(jOutput, "data", json_string_nocheck(gTree->saveString(0, NULL, true).c_str()));
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
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(jsonFilename));
			gTree->summaryInfo(jResult);
			printf("%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		delete gTree;
	}
};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {buildtest0Context_t} Application context
 */
buildtest0Context_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --error\n");
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxNode);
		fprintf(stderr, "\t-q --quiet\n");
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
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_FORCE, LO_MAXNODE, LO_ERROR,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",       1, 0, LO_DEBUG},
			{"error",       0, 0, LO_ERROR},
			{"force",       0, 0, LO_FORCE},
			{"help",        0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"quiet",       2, 0, LO_QUIET},
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
		case LO_ERROR:
			app.opt_error++;
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

	if (argc - optind >= 1) {
		jsonFilename = argv[optind++];
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
	}

	/*
	 * Main
	 */
	app.main(jsonFilename);

	return 0;
}
