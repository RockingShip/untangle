//#pragma GCC optimize ("O0") // optimize on demand

/*
 * buildmd5.cc
 * 	Ancient code that creates the input database.
 * 	This was taken from a reference implementation in the corresponding
 * 	wikipedia page. Ints were replaced by node_t wrappers in vectors.
 *
 * Creating/loading and storing database files and their sections.
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
#include "buildmd5.h"

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

/// @var {baseTree_t*} global reference to tree
baseTree_t *gTree = NULL;
/// @var {json_t*} validation tests
json_t     *gTests; // validation tests

/*
 * @date 2021-05-16 23:03:02
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
 * Include generated tests
 */

#include "validatemd5.h"

struct NODE {
	uint32_t id;

	NODE() { id = 0; }

	NODE(uint32_t id) {
		assert((id & ~IBIT) == 0 || ((id & ~IBIT) >= gTree->kstart && (id & ~IBIT) < gTree->ncount));
		this->id = id;
	}

	NODE(NODE Q, NODE T, NODE F) {
		this->id = gTree->addNormaliseNode(Q.id, T.id, F.id);

		context_t &ctx = gTree->ctx;

		ctx.progress++;

		if (ctx.tick && ctx.opt_verbose >= ctx.VERBOSE_TICK) {
			int perSecond = ctx.updateSpeed();

			int eta  = (int) ((ctx.progressHi - ctx.progress) / perSecond);
			int etaH = eta / 3600;
			eta %= 3600;
			int etaM = eta / 60;
			eta %= 60;
			int etaS = eta;

			fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% %3d:%02d:%02d ncount=%d",
				ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS, gTree->ncount);
		}
	}

	NODE operator|(const NODE &other) const { return NODE(this->id, IBIT, other.id); }

	NODE operator*(const NODE &other) const { return NODE(this->id, other.id, 0); }

	NODE operator^(const NODE &other) const { return NODE(this->id, other.id ^ IBIT, other.id); }
};

/**
 * @date 2021-05-16 23:10:57
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct buildmd5Context_t {

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;
	/// @var {NODE} variables referencing zero/false and nonZero/true
	NODE     vFalse, vTrue;

	buildmd5Context_t() {
		opt_flags   = 0;
		opt_force   = 0;
		opt_maxNode = DEFAULT_MAXNODE;
		vFalse.id = 0;
		vTrue.id  = IBIT;
	}

	void addC3(NODE *V, int Q, int L, unsigned int R) {
		NODE ovf = 0;

		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[L + i];
			if ((R >> i) & 1) {
				V[Q + i] = l ^ ovf ^ vTrue;
				ovf = ovf | l;
			} else {
				V[Q + i] = l ^ ovf;
				ovf = ovf * l;
			}
		}
	}

	void toN(NODE *V, int L, unsigned int VAL) {
		for (unsigned i = 0; i < 32; i++) {
			V[L + i] = ((VAL >> i) & 1) ? vTrue : 0;
		}
	}

	void F1(NODE *V, int Q, int A, int B, int C, int D, int K, unsigned int VAL, int R) {
		if ((opt_flags & ctx.MAGICMASK_CASCADE) && ctx.opt_verbose >= ctx.VERBOSE_TICK)
			printf("[%s] F1 %s\n", ctx.timeAsString(), gTree->entryNames[K].c_str());

		NODE W[32];
		NODE ovf = 0;

		for (unsigned i = 0; i < 32; i++)
			W[i] = (V[D + i] ^ (V[C + i] * (V[B + i] ^ V[D + i])));

		// add
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[A + i];
			NODE r = W[i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add K
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[K + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add VAL
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			if ((VAL >> i) & 1) {
				V[Q + i] = l ^ ovf ^ vTrue;
				ovf = ovf | l;
			} else {
				V[Q + i] = l ^ ovf;
				ovf = ovf * l;
			}
		}

		// W = Q
		for (unsigned i = 0; i < 32; i++)
			W[i] = V[Q + i];

		// rotate R
		for (unsigned i = 0; i < 32; i++) {
			V[Q + (i + R) % 32] = W[i];
		}

		// add C
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[C + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}
	}

	void F2(NODE *V, int Q, int A, int B, int C, int D, int K, unsigned int VAL, int R) {
		if ((opt_flags & ctx.MAGICMASK_CASCADE) && ctx.opt_verbose >= ctx.VERBOSE_TICK)
			printf("[%s] F2 %s\n", ctx.timeAsString(), gTree->entryNames[K].c_str());

		NODE W[32];
		NODE ovf = 0;

		for (unsigned i = 0; i < 32; i++)
			W[i] = (V[D + i] ^ (V[B + i] * (V[C + i] ^ V[D + i])));

		// add
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[A + i];
			NODE r = W[i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add K
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[K + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add VAL
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			if ((VAL >> i) & 1) {
				V[Q + i] = l ^ ovf ^ vTrue;
				ovf = ovf | l;
			} else {
				V[Q + i] = l ^ ovf;
				ovf = ovf * l;
			}
		}

		// W = Q
		for (unsigned i = 0; i < 32; i++)
			W[i] = V[Q + i];

		// rotate R
		for (unsigned i = 0; i < 32; i++) {
			V[Q + (i + R) % 32] = W[i];
		}

		// add C
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[C + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}
	}

	void F3(NODE *V, int Q, int A, int B, int C, int D, int K, unsigned int VAL, int R) {
		if ((opt_flags & ctx.MAGICMASK_CASCADE) && ctx.opt_verbose >= ctx.VERBOSE_TICK)
			printf("[%s] F3 %s\n", ctx.timeAsString(), gTree->entryNames[K].c_str());

		NODE W[32];
		NODE ovf = 0;

		for (unsigned i = 0; i < 32; i++)
			W[i] = V[B + i] ^ V[C + i] ^ V[D + i];

		// add
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[A + i];
			NODE r = W[i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add K
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[K + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add VAL
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			if ((VAL >> i) & 1) {
				V[Q + i] = l ^ ovf ^ vTrue;
				ovf = ovf | l;
			} else {
				V[Q + i] = l ^ ovf;
				ovf = ovf * l;
			}
		}

		// W = Q
		for (unsigned i = 0; i < 32; i++)
			W[i] = V[Q + i];

		// rotate R
		for (unsigned i = 0; i < 32; i++) {
			V[Q + (i + R) % 32] = W[i];
		}

		// add C
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[C + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}
	}

	void F4(NODE *V, int Q, int A, int B, int C, int D, int K, unsigned int VAL, int R) {
		if ((opt_flags & ctx.MAGICMASK_CASCADE) && ctx.opt_verbose >= ctx.VERBOSE_TICK)
			printf("[%s] F4 %s\n", ctx.timeAsString(), gTree->entryNames[K].c_str());

		NODE W[32];
		NODE ovf = 0;

		for (unsigned i = 0; i < 32; i++)
			W[i] = V[B + i] ^ (V[C + i] | (V[D + i] ^ vTrue));

		// add
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[A + i];
			NODE r = W[i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add K
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[K + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}

		// add VAL
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			if ((VAL >> i) & 1) {
				V[Q + i] = l ^ ovf ^ vTrue;
				ovf = ovf | l;
			} else {
				V[Q + i] = l ^ ovf;
				ovf = ovf * l;
			}
		}

		// W = Q
		for (unsigned i = 0; i < 32; i++)
			W[i] = V[Q + i];

		// rotate R
		for (unsigned i = 0; i < 32; i++) {
			V[Q + (i + R) % 32] = W[i];
		}

		// add C
		ovf = 0;
		for (unsigned i = 0; i < 32; i++) {
			NODE l = V[Q + i];
			NODE r = V[C + i];
			V[Q + i] = l ^ r ^ ovf;
			ovf = NODE(ovf, l | r, l * r);
		}
	}

	void __attribute__((optimize("O0"))) build(NODE *V) {
		toN(V, ax00, 0x67452301);
		toN(V, dx00, 0x10325476);
		toN(V, cx00, 0x98badcfe);
		toN(V, bx00, 0xefcdab89);

		//@formatter:off
		F1(V,a000,ax00,cx00,bx00,dx00,k000,0xd76aa478, 7);
		F1(V,d000,dx00,bx00,a000,cx00,k100,0xe8c7b756,12);
		F1(V,c000,cx00,a000,d000,bx00,k200,0x242070db,17);
		F1(V,b000,bx00,d000,c000,a000,k300,0xc1bdceee,22);
		F1(V,a100,a000,c000,b000,d000,k400,0xf57c0faf, 7);
		F1(V,d100,d000,b000,a100,c000,k500,0x4787c62a,12);
		F1(V,c100,c000,a100,d100,b000,k600,0xa8304613,17);
		F1(V,b100,b000,d100,c100,a100,k700,0xfd469501,22);
		F1(V,a200,a100,c100,b100,d100,k800,0x698098d8, 7);
		F1(V,d200,d100,b100,a200,c100,k900,0x8b44f7af,12);
		F1(V,c200,c100,a200,d200,b100,ka00,0xffff5bb1,17);
		F1(V,b200,b100,d200,c200,a200,kb00,0x895cd7be,22);
		F1(V,a300,a200,c200,b200,d200,kc00,0x6b901122, 7);
		F1(V,d300,d200,b200,a300,c200,kd00,0xfd987193,12);
		F1(V,c300,c200,a300,d300,b200,ke00,0xa679438e,17);
		F1(V,b300,b200,d300,c300,a300,kf00,0x49b40821,22);

		F2(V,a400,a300,d300,b300,c300,k100,0xf61e2562, 5);
		F2(V,d400,d300,c300,a400,b300,k600,0xc040b340, 9);
		F2(V,c400,c300,b300,d400,a400,kb00,0x265e5a51,14);
		F2(V,b400,b300,a400,c400,d400,k000,0xe9b6c7aa,20);
		F2(V,a500,a400,d400,b400,c400,k500,0xd62f105d, 5);
		F2(V,d500,d400,c400,a500,b400,ka00,0x02441453, 9);
		F2(V,c500,c400,b400,d500,a500,kf00,0xd8a1e681,14);
		F2(V,b500,b400,a500,c500,d500,k400,0xe7d3fbc8,20);
		F2(V,a600,a500,d500,b500,c500,k900,0x21e1cde6, 5);
		F2(V,d600,d500,c500,a600,b500,ke00,0xc33707d6, 9);
		F2(V,c600,c500,b500,d600,a600,k300,0xF4d50d87,14);
		F2(V,b600,b500,a600,c600,d600,k800,0x455a14ed,20);
		F2(V,a700,a600,d600,b600,c600,kd00,0xa9e3e905, 5);
		F2(V,d700,d600,c600,a700,b600,k200,0xfcefa3f8, 9);
		F2(V,c700,c600,b600,d700,a700,k700,0x676f02d9,14);
		F2(V,b700,b600,a700,c700,d700,kc00,0x8d2a4c8a,20);

		F3(V,a800,a700,c700,b700,d700,k500,0xfffa3942, 4);
		F3(V,d800,d700,b700,a800,c700,k800,0x8771f681,11);
		F3(V,c800,c700,a800,d800,b700,kb00,0x6d9d6122,16);
		F3(V,b800,b700,d800,c800,a800,ke00,0xfde5380c,23);
		F3(V,a900,a800,c800,b800,d800,k100,0xa4beea44, 4);
		F3(V,d900,d800,b800,a900,c800,k400,0x4bdecfa9,11);
		F3(V,c900,c800,a900,d900,b800,k700,0xf6bb4b60,16);
		F3(V,b900,b800,d900,c900,a900,ka00,0xbebfbc70,23);
		F3(V,aa00,a900,c900,b900,d900,kd00,0x289b7ec6, 4);
		F3(V,da00,d900,b900,aa00,c900,k000,0xeaa127fa,11);
		F3(V,ca00,c900,aa00,da00,b900,k300,0xd4ef3085,16);
		F3(V,ba00,b900,da00,ca00,aa00,k600,0x04881d05,23);
		F3(V,ab00,aa00,ca00,ba00,da00,k900,0xd9d4d039, 4);
		F3(V,db00,da00,ba00,ab00,ca00,kc00,0xe6db99e5,11);
		F3(V,cb00,ca00,ab00,db00,ba00,kf00,0x1fa27cf8,16);
		F3(V,bb00,ba00,db00,cb00,ab00,k200,0xc4ac5665,23);

		F4(V,ac00,ab00,cb00,bb00,db00,k000,0xF4292244, 6);
		F4(V,dc00,db00,bb00,ac00,cb00,k700,0x432aff97,10);
		F4(V,cc00,cb00,ac00,dc00,bb00,ke00,0xab9423a7,15);
		F4(V,bc00,bb00,dc00,cc00,ac00,k500,0xfc93a039,21);
		F4(V,ad00,ac00,cc00,bc00,dc00,kc00,0x655b59c3, 6);
		F4(V,dd00,dc00,bc00,ad00,cc00,k300,0x8f0ccc92,10);
		F4(V,cd00,cc00,ad00,dd00,bc00,ka00,0xffefF47d,15);
		F4(V,bd00,bc00,dd00,cd00,ad00,k100,0x85845dd1,21);
		F4(V,ae00,ad00,cd00,bd00,dd00,k800,0x6fa87e4f, 6);
		F4(V,de00,dd00,bd00,ae00,cd00,kf00,0xfe2ce6e0,10);
		F4(V,ce00,cd00,ae00,de00,bd00,k600,0xa3014314,15);
		F4(V,be00,bd00,de00,ce00,ae00,kd00,0x4e0811a1,21);
		F4(V,af00,ae00,ce00,be00,de00,k400,0xf7537e82, 6);
		F4(V,df00,de00,be00,af00,ce00,kb00,0xbd3af235,10);
		F4(V,cf00,ce00,af00,df00,be00,k200,0x2ad7d2bb,15);
		F4(V,bf00,be00,df00,cf00,af00,k900,0xeb86d391,21);
		//@formatter:on

		addC3(V, o00, af00, 0x67452301);
		addC3(V, oc0, df00, 0x10325476);
		addC3(V, o80, cf00, 0x98badcfe);
		addC3(V, o40, bf00, 0xefcdab89);
	}

	void main(const char *jsonFilename) {
		/*
		 * There are a real long OR/XOR/AND chains
		 */
		if (!(opt_flags & ctx.MAGICMASK_CASCADE))
			fprintf(stderr, "WARNING: optimisation `--cascade` not specified\n");

		/*
		 * allocate and initialise placeholder/helper array references to variables
		 */
		NODE *V = (NODE *) malloc(ELAST * sizeof V[0]);

		/*
		 * Allocate the build tree containing the complete formula
		 */
		// basic keys
		gTree = new baseTree_t(ctx, KSTART, OSTART, ESTART, ESTART/*NSTART*/, ESTART/*numRoots*/, opt_maxNode, opt_flags);

		// setup entry names
		for (unsigned iEntry = 0; iEntry < gTree->nstart; iEntry++) {
			// key name
			gTree->entryNames[iEntry] = allNames[iEntry];

			// key variable
			V[iEntry].id = iEntry;
		}

		// setup root names
		for (unsigned iRoot = 0; iRoot < gTree->numRoots; iRoot++) {
			// key name
			gTree->rootNames[iRoot] = allNames[iRoot];

			// root result
			gTree->roots[iRoot] = iRoot;
		}

		// build. Uses gBuild
		build(V);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * Assign the roots/entrypoints.
		 */
		gTree->numRoots = gTree->estart;
		for (unsigned iRoot = 0; iRoot < gTree->estart; iRoot++)
			gTree->roots[iRoot] = V[iRoot].id;

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
		json_object_set_new_nocheck(jOutput, "raw", json_string_nocheck(gTree->saveString(0, NULL, true).c_str()));
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
 * @global {buildmd5Context_t} Application context
 */
buildmd5Context_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json> <output.dat>\n", argv[0]);
	if (verbose) {
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
 * @date 2021-05-16 23:31:57
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
