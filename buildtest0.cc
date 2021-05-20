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

// used key/root names
enum {
	kZero = 0, kError, // reserved
	k0, k1, k2, k3, // keys
	o0, o1, o2, // roots
	OLAST, // last

	KSTART = k0,
	NSTART = o0,
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

	NODE(NODE Q, NODE T, NODE F) { this->id = gTree->normaliseNode(Q.id, T.id, F.id); }

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
 * o0 = k1 ? !k2 : k3
 * o1 = k1 ?  k2 : k3
 * o2 = k0 ?  ERROR : 0
 */
void validateAll() {
	//         k3-k2-k1-k0   o2-o1-o0     o1           o0
	validate("05", "01"); // (1?0:1)=0   (1?!0:1)=1
	validate("02", "00"); // (0?1:0)=0   (0?!1:0)=0

	validate("00", "00"); // (0?0:0)=0   (0?!0:0)=0
	validate("01", "03"); // (0:1:1)=1   (0?!0:1)=1
	validate("03", "03"); // (0?1:1)=1   (0?!1:1)=1
	validate("04", "01"); // (1?0:0)=0   (1?!0:0)=1
	validate("06", "02"); // (1?1:0)=1   (1?!1:0)=0
	validate("07", "02"); // (1?1:1)=1   (1?!1:1)=0

	// this one should trigger an undefined error on verification
	validate("08", "00");

	// this one should trigger an incorrect result
	validate("01", "00");
}

/**
 * @date 2021-05-10 13:23:43
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct buildtest0Context_t : context_t {

	/// @var {string} output metadata filename
	const char *arg_json;
	/// @var {string} output filename
	const char *arg_data;
	/// @var {number} header flags
	uint32_t   opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned   opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned   opt_maxnode;

	buildtest0Context_t() {
		arg_json    = NULL;
		arg_data    = NULL;
		opt_flags   = 0;
		opt_force   = 0;
		opt_maxnode = DEFAULT_MAXNODE;
	}

	void main(void) {
		/*
		 * Allocate the build tree containing the complete formula
		 */

		gTree = new baseTree_t(*this, KSTART, OSTART, NSTART, NSTART, NSTART/*numRoots*/, opt_maxnode, opt_flags);

		// setup key names
		for (unsigned iKey = 0; iKey < gTree->nstart; iKey++) {
			gTree->keyNames[iKey] = allNames[iKey];

			gTree->N[iKey].Q = 0;
			gTree->N[iKey].T = 0;
			gTree->N[iKey].F = iKey;
		}

		// setup root names
		for (unsigned iRoot = 0; iRoot < gTree->numRoots; iRoot++) {
			gTree->rootNames[iRoot] = allNames[iRoot];

			gTree->roots[iRoot] = iRoot;
		}

		// setup nodes

		// gTree->roots[0] = gTree->N[k2] ?   gTree->N[k1]     : gTree->N[k0] ;
		// gTree->roots[1] = gTree->N[k2] ? ! gTree->N[k1]     : gTree->N[k0] ;
		// gTree->roots[2] = gTree->N[k3] ? ! gTree->N[kError] : gTree->N[0]  ;
		// because there is no operator overload available for the above

		gTree->N[gTree->ncount].Q = k2;
		gTree->N[gTree->ncount].T = k1 ^ IBIT;
		gTree->N[gTree->ncount].F = k0;
		gTree->roots[o0 - OSTART] = gTree->ncount++;

		gTree->N[gTree->ncount].Q = k2;
		gTree->N[gTree->ncount].T = k1;
		gTree->N[gTree->ncount].F = k0;
		gTree->roots[o1 - OSTART] = gTree->ncount++;

		gTree->N[gTree->ncount].Q = k3;
		gTree->N[gTree->ncount].T = kError;
		gTree->N[gTree->ncount].F = kZero;
		gTree->roots[o2 - OSTART] = gTree->ncount++;

		/*
		 * Create tests as json object
		 */

		gTests = json_array();
		validateAll();

		/*
		 * Save the tree
		 */

		gTree->saveFile(arg_data);

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

		FILE *f = fopen(arg_json, "w");
		if (!f)
			fatal("fopen(%s) returned: %m\n", arg_json);

		fprintf(f, "%s\n", json_dumps(jOutput, JSON_PRESERVE_ORDER | JSON_COMPACT));

		if (fclose(f))
			fatal("fclose(%s) returned: %m\n", arg_json);

		/*
		 * Display json
		 */

		if (opt_verbose >= VERBOSE_SUMMARY) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(arg_data));
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
 * @global {buildtest0Context_t} Application context
 */
buildtest0Context_t app;

void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s <json> <data>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxnode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", app.opt_timer);
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --[no-]paranoid [default=%s]\n", app.opt_flags & app.MAGICMASK_PARANOID ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]pure [default=%s]\n", app.opt_flags & app.MAGICMASK_PURE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]rewrite [default=%s]\n", app.opt_flags & app.MAGICMASK_REWRITE ? "enabled" : "disabled");
		fprintf(stderr, "\t   --[no-]cascade [default=%s]\n", app.opt_flags & app.MAGICMASK_CASCADE ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]shrink [default=%s]\n", app.opt_flags &  app.MAGICMASK_SHRINK ? "enabled" : "disabled");
//		fprintf(stderr, "\t   --[no-]pivot3 [default=%s]\n", app.opt_flags &  app.MAGICMASK_PIVOT3 ? "enabled" : "disabled");
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
int main(int argc, char *const *argv) {
	setlinebuf(stdout);

	for (;;) {
		int option_index = 0;
		enum {
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_FORCE, LO_MAXNODE,
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

		char optstring[128], *cp;
		cp = optstring;

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
				app.opt_debug = (unsigned) strtoul(optarg, NULL, 8); // OCTAL!!
				break;
			case LO_FORCE:
				app.opt_force++;
				break;
			case LO_HELP:
				usage(argv, true);
				exit(0);
			case LO_MAXNODE:
				app.opt_maxnode = (unsigned) strtoul(optarg, NULL, 10);
				break;
			case LO_QUIET:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose - 1;
				break;
			case LO_TIMER:
				app.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
				break;
			case LO_VERBOSE:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose + 1;
				break;

			case LO_PARANOID:
				app.opt_flags |= app.MAGICMASK_PARANOID;
				break;
			case LO_NOPARANOID:
				app.opt_flags &= ~app.MAGICMASK_PARANOID;
				break;
			case LO_PURE:
				app.opt_flags |= app.MAGICMASK_PURE;
				break;
			case LO_NOPURE:
				app.opt_flags &= ~app.MAGICMASK_PURE;
				break;
			case LO_REWRITE:
				app.opt_flags |= app.MAGICMASK_REWRITE;
				break;
			case LO_NOREWRITE:
				app.opt_flags &= ~app.MAGICMASK_REWRITE;
				break;
			case LO_CASCADE:
				app.opt_flags |= app.MAGICMASK_CASCADE;
				break;
			case LO_NOCASCADE:
				app.opt_flags &= ~app.MAGICMASK_CASCADE;
				break;
//			case LO_SHRINK:
//				app.opt_flags |=  app.MAGICMASK_SHRINK;
//				break;
//			case LO_NOSHRINK:
//				app.opt_flags &=  ~app.MAGICMASK_SHRINK;
//				break;
//			case LO_PIVOT3:
//				app.opt_flags |=  app.MAGICMASK_PIVOT3;
//				break;
//			case LO_NOPIVOT3:
//				app.opt_flags &=  ~app.MAGICMASK_PIVOT3;
//				break;

			case '?':
				app.fatal("Try `%s --help' for more information.\n", argv[0]);
			default:
				app.fatal("getopt returned character code %d\n", c);
		}
	}

	if (argc - optind >= 2) {
		app.arg_json = argv[optind++];
		app.arg_data = argv[optind++];
	} else {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!app.opt_force) {
		struct stat sbuf;
		if (!stat(app.arg_json, &sbuf))
			app.fatal("%s already exists. Use --force to overwrite\n", app.arg_json);
		if (!stat(app.arg_data, &sbuf))
			app.fatal("%s already exists. Use --force to overwrite\n", app.arg_data);
	}

	/*
	 * Main
	 */
	app.main();

	return 0;
}
