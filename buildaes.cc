#pragma GCC optimize ("O0") // optimize on demand

/*
 * buildaes.cc
 * 	Ancient code that creates the input database.
 * 	This was taken from some reference implementation.
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
#include "buildaes.h"

/// @var {baseTree_t*} global reference to tree
baseTree_t *gTree = NULL;
/// @var {json_t*} validation tests
json_t     *gTests; // validation tests

/*
 * @date 2021-05-17 16:57:03
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
#include "validateaes.h"

struct NODE {
	uint32_t id;

	NODE() { id = 0; }

	NODE(uint32_t id) {
		assert((id & ~IBIT) == 0 || ((id & ~IBIT) >= gTree->kstart && (id & ~IBIT) < gTree->ncount));
		this->id = id;
	}

	NODE(NODE Q, NODE T, NODE F) { this->id = gTree->normaliseNode(Q.id, T.id, F.id); }

	NODE operator|(const NODE &other) const { return NODE(this->id, IBIT, other.id); }

	NODE operator*(const NODE &other) const { return NODE(this->id, other.id, 0); }

	NODE operator^(const NODE &other) const { return NODE(this->id, other.id ^ IBIT, other.id); }
};

/*
 * Include S-box breakdown
 */
#include "buildaesbox.h"

/**
 * @date 2021-05-17 16:57:51
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct buildaesContext_t : context_t {

	/// @var {string} output metadata filename
	const char *arg_json;
	/// @var {string} output filename
	const char *arg_data;
	/// @var {number} header flags
	uint32_t   opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned   opt_force;
	/// @var {number} --split, split the tree into round
	unsigned   opt_split;
	/// @var {number} --opt_maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned   opt_maxnode;
	/// @var {NODE} variables referencing zero/false and nonZero/true
	NODE       vFalse, vTrue;

	buildaesContext_t() {
		arg_json    = NULL;
		arg_data    = NULL;
		opt_flags   = 0;
		opt_force   = 0;
		opt_split   = 0;
		opt_maxnode = DEFAULT_MAXNODE;
		vFalse.id = 0;
		vTrue.id  = IBIT;
	}

/*
	 * Split and Save intermediate tree
	 * The current round intermediates are stored as roots/entrypoints
	 * The new tree will find the intermediates as 'extended' keys
 */
	void splitTree(NODE *V, uint32_t vstart, int roundNr) {
		unsigned savNumRoots = gTree->numRoots;

		// output 32 round intermediates
		assert(gTree->numRoots >= 32);
		gTree->numRoots = 32;

		for (uint32_t i = vstart; i < vstart + 32; i++) {
			gTree->rootNames[i - vstart] = allNames[i]; // assign root name
			gTree->roots[i - vstart]     = V[i].id; // node id of intermediate
		}

		// save
		{
			// generate (hopefully) a unique tree id
			gTree->keysId = rand();

			char *filename;
			asprintf(&filename, arg_data, roundNr);

			gTree->saveFile(filename);

			free(filename);
		}

		// save metadata
		{
			json_t *jOutput = json_object();
			gTree->headerInfo(jOutput);

			char *filename;
			asprintf(&filename, arg_data, roundNr);

			FILE *f = fopen(filename, "w");
			if (!f)
				fatal("fopen(%s) returned: %m\n", filename);

			fprintf(f, "%s\n", json_dumps(jOutput, JSON_PRESERVE_ORDER | JSON_COMPACT));

			if (fclose(f))
				fatal("fclose(%s) returned: %m\n", arg_json);

			free(filename);
		}

		assert(!"TODO");
		
		// setup continuation tree
		gTree->rootsId  = gTree->keysId;
		gTree->keysId   = 0;
		gTree->estart   = NSTART; // first external/extended key
		gTree->nstart   = NSTART + 32; // inputs are keys + 32 round intermediates
		gTree->ncount   = gTree->nstart;
		gTree->numRoots = savNumRoots;
		// invalidate lookup cache
		++gTree->nodeIndexVersionNr;

		// setup intermediate keys for continuation
		for (uint32_t i = vstart; i < vstart + 32; i++) {
			V[i].id = NSTART + i - vstart;
			gTree->keyNames[V[i].id] = allNames[i];
		}

	}

	/*
	 * Build aes expression
	 * Ints are replaced by node_t wrappers in vectors.

	 * NOTE: disable optimisations or wait a day
	 */
	void __attribute__((optimize("O0"))) build(NODE *V) {
		//@formatter:off
//fprintf(stderr,"k070");
		// WORD k070 = 0x62 ^ sbox[k031] ^ k030^k010^k020^k000;
		V[k0700] = SBOX0(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k300] ^ V[k100] ^ V[k200] ^ V[k000] ^ vFalse;
		V[k0701] = SBOX1(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k301] ^ V[k101] ^ V[k201] ^ V[k001] ^ vTrue;
		V[k0702] = SBOX2(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k302] ^ V[k102] ^ V[k202] ^ V[k002] ^ vFalse;
		V[k0703] = SBOX3(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k303] ^ V[k103] ^ V[k203] ^ V[k003] ^ vFalse;
		V[k0704] = SBOX4(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k304] ^ V[k104] ^ V[k204] ^ V[k004] ^ vFalse;
		V[k0705] = SBOX5(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k305] ^ V[k105] ^ V[k205] ^ V[k005] ^ vTrue;
		V[k0706] = SBOX6(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k306] ^ V[k106] ^ V[k206] ^ V[k006] ^ vTrue;
		V[k0707] = SBOX7(V[k317], V[k316], V[k315], V[k314], V[k313], V[k312], V[k311], V[k310]) ^ V[k307] ^ V[k107] ^ V[k207] ^ V[k007] ^ vFalse;

//fprintf(stderr,".");
		// WORD k113 = 0x63 ^ sbox[k070] ^ k033^k013;
		V[k1130] = SBOX0(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k330] ^ V[k130] ^ vTrue;
		V[k1131] = SBOX1(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k331] ^ V[k131] ^ vTrue;
		V[k1132] = SBOX2(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k332] ^ V[k132] ^ vFalse;
		V[k1133] = SBOX3(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k333] ^ V[k133] ^ vFalse;
		V[k1134] = SBOX4(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k334] ^ V[k134] ^ vFalse;
		V[k1135] = SBOX5(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k335] ^ V[k135] ^ vTrue;
		V[k1136] = SBOX6(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k336] ^ V[k136] ^ vTrue;
		V[k1137] = SBOX7(V[k0707], V[k0706], V[k0705], V[k0704], V[k0703], V[k0702], V[k0701], V[k0700]) ^ V[k337] ^ V[k137] ^ vFalse;

//fprintf(stderr,".");
		// WORD k152 = 0x63 ^ sbox[k113] ^ k032^k022;
		V[k1520] = SBOX0(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k320] ^ V[k220] ^ vTrue;
		V[k1521] = SBOX1(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k321] ^ V[k221] ^ vTrue;
		V[k1522] = SBOX2(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k322] ^ V[k222] ^ vFalse;
		V[k1523] = SBOX3(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k323] ^ V[k223] ^ vFalse;
		V[k1524] = SBOX4(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k324] ^ V[k224] ^ vFalse;
		V[k1525] = SBOX5(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k325] ^ V[k225] ^ vTrue;
		V[k1526] = SBOX6(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k326] ^ V[k226] ^ vTrue;
		V[k1527] = SBOX7(V[k1137], V[k1136], V[k1135], V[k1134], V[k1133], V[k1132], V[k1131], V[k1130]) ^ V[k327] ^ V[k227] ^ vFalse;

//fprintf(stderr,".");
		// WORD k191 = 0x63 ^ sbox[k152] ^ k031;
		V[k1910] = SBOX0(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k310] ^ vTrue;
		V[k1911] = SBOX1(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k311] ^ vTrue;
		V[k1912] = SBOX2(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k312] ^ vFalse;
		V[k1913] = SBOX3(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k313] ^ vFalse;
		V[k1914] = SBOX4(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k314] ^ vFalse;
		V[k1915] = SBOX5(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k315] ^ vTrue;
		V[k1916] = SBOX6(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k316] ^ vTrue;
		V[k1917] = SBOX7(V[k1527], V[k1526], V[k1525], V[k1524], V[k1523], V[k1522], V[k1521], V[k1520]) ^ V[k317] ^ vFalse;

//fprintf(stderr,".");
		// WORD k230 = 0x73 ^ sbox[k191] ^ k070;
		V[k2300] = SBOX0(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0700] ^ vTrue;
		V[k2301] = SBOX1(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0701] ^ vTrue;
		V[k2302] = SBOX2(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0702] ^ vFalse;
		V[k2303] = SBOX3(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0703] ^ vFalse;
		V[k2304] = SBOX4(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0704] ^ vTrue;
		V[k2305] = SBOX5(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0705] ^ vTrue;
		V[k2306] = SBOX6(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0706] ^ vTrue;
		V[k2307] = SBOX7(V[k1917], V[k1916], V[k1915], V[k1914], V[k1913], V[k1912], V[k1911], V[k1910]) ^ V[k0707] ^ vFalse;

//fprintf(stderr,".");
		// WORD k273 = 0x63 ^ sbox[k230] ^ k113;
		V[k2730] = SBOX0(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1130] ^ vTrue;
		V[k2731] = SBOX1(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1131] ^ vTrue;
		V[k2732] = SBOX2(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1132] ^ vFalse;
		V[k2733] = SBOX3(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1133] ^ vFalse;
		V[k2734] = SBOX4(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1134] ^ vFalse;
		V[k2735] = SBOX5(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1135] ^ vTrue;
		V[k2736] = SBOX6(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1136] ^ vTrue;
		V[k2737] = SBOX7(V[k2307], V[k2306], V[k2305], V[k2304], V[k2303], V[k2302], V[k2301], V[k2300]) ^ V[k1137] ^ vFalse;

//fprintf(stderr,".");
		// WORD k312 = 0x63 ^ sbox[k273] ^ k152;
		V[k3120] = SBOX0(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1520] ^ vTrue;
		V[k3121] = SBOX1(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1521] ^ vTrue;
		V[k3122] = SBOX2(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1522] ^ vFalse;
		V[k3123] = SBOX3(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1523] ^ vFalse;
		V[k3124] = SBOX4(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1524] ^ vFalse;
		V[k3125] = SBOX5(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1525] ^ vTrue;
		V[k3126] = SBOX6(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1526] ^ vTrue;
		V[k3127] = SBOX7(V[k2737], V[k2736], V[k2735], V[k2734], V[k2733], V[k2732], V[k2731], V[k2730]) ^ V[k1527] ^ vFalse;

//fprintf(stderr,".");
		// WORD k351 = 0x63 ^ sbox[k312] ^ k191;
		V[k3510] = SBOX0(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1910] ^ vTrue;
		V[k3511] = SBOX1(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1911] ^ vTrue;
		V[k3512] = SBOX2(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1912] ^ vFalse;
		V[k3513] = SBOX3(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1913] ^ vFalse;
		V[k3514] = SBOX4(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1914] ^ vFalse;
		V[k3515] = SBOX5(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1915] ^ vTrue;
		V[k3516] = SBOX6(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1916] ^ vTrue;
		V[k3517] = SBOX7(V[k3127], V[k3126], V[k3125], V[k3124], V[k3123], V[k3122], V[k3121], V[k3120]) ^ V[k1917] ^ vFalse;

//fprintf(stderr,".");
		// WORD k390 = 0x78 ^ sbox[k351] ^ k230;
		V[k3900] = SBOX0(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2300] ^ vFalse;
		V[k3901] = SBOX1(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2301] ^ vFalse;
		V[k3902] = SBOX2(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2302] ^ vFalse;
		V[k3903] = SBOX3(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2303] ^ vTrue;
		V[k3904] = SBOX4(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2304] ^ vTrue;
		V[k3905] = SBOX5(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2305] ^ vTrue;
		V[k3906] = SBOX6(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2306] ^ vTrue;
		V[k3907] = SBOX7(V[k3517], V[k3516], V[k3515], V[k3514], V[k3513], V[k3512], V[k3511], V[k3510]) ^ V[k2307] ^ vFalse;

//fprintf(stderr,".");
		// WORD k433 = 0x63 ^ sbox[k390] ^ k273;
		V[k4330] = SBOX0(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2730] ^ vTrue;
		V[k4331] = SBOX1(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2731] ^ vTrue;
		V[k4332] = SBOX2(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2732] ^ vFalse;
		V[k4333] = SBOX3(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2733] ^ vFalse;
		V[k4334] = SBOX4(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2734] ^ vFalse;
		V[k4335] = SBOX5(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2735] ^ vTrue;
		V[k4336] = SBOX6(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2736] ^ vTrue;
		V[k4337] = SBOX7(V[k3907], V[k3906], V[k3905], V[k3904], V[k3903], V[k3902], V[k3901], V[k3900]) ^ V[k2737] ^ vFalse;

//fprintf(stderr,"k073");
		//	k073 = 0x63 ^ sbox[k030] ^ k033^k013^k023^k003;
		V[k0730] = SBOX0(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k330]^V[k130]^V[k230]^V[k030] ^ vTrue;
		V[k0731] = SBOX1(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k331]^V[k131]^V[k231]^V[k031] ^ vTrue;
		V[k0732] = SBOX2(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k332]^V[k132]^V[k232]^V[k032] ^ vFalse;
		V[k0733] = SBOX3(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k333]^V[k133]^V[k233]^V[k033] ^ vFalse;
		V[k0734] = SBOX4(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k334]^V[k134]^V[k234]^V[k034] ^ vFalse;
		V[k0735] = SBOX5(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k335]^V[k135]^V[k235]^V[k035] ^ vTrue;
		V[k0736] = SBOX6(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k336]^V[k136]^V[k236]^V[k036] ^ vTrue;
		V[k0737] = SBOX7(V[k307], V[k306], V[k305], V[k304], V[k303], V[k302], V[k301], V[k300]) ^ V[k337]^V[k137]^V[k237]^V[k037] ^ vFalse;

//fprintf(stderr,".");
		//	k112 = 0x63 ^ sbox[k073] ^ k032^k012;
		V[k1120] = SBOX0(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k320]^V[k120] ^ vTrue;
		V[k1121] = SBOX1(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k321]^V[k121] ^ vTrue;
		V[k1122] = SBOX2(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k322]^V[k122] ^ vFalse;
		V[k1123] = SBOX3(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k323]^V[k123] ^ vFalse;
		V[k1124] = SBOX4(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k324]^V[k124] ^ vFalse;
		V[k1125] = SBOX5(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k325]^V[k125] ^ vTrue;
		V[k1126] = SBOX6(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k326]^V[k126] ^ vTrue;
		V[k1127] = SBOX7(V[k0737], V[k0736], V[k0735], V[k0734], V[k0733], V[k0732], V[k0731], V[k0730]) ^ V[k327]^V[k127] ^ vFalse;

//fprintf(stderr,".");
		//	k151 = 0x63 ^ sbox[k112] ^ k031^k021;
		V[k1510] = SBOX0(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k310]^V[k210] ^ vTrue;
		V[k1511] = SBOX1(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k311]^V[k211] ^ vTrue;
		V[k1512] = SBOX2(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k312]^V[k212] ^ vFalse;
		V[k1513] = SBOX3(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k313]^V[k213] ^ vFalse;
		V[k1514] = SBOX4(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k314]^V[k214] ^ vFalse;
		V[k1515] = SBOX5(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k315]^V[k215] ^ vTrue;
		V[k1516] = SBOX6(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k316]^V[k216] ^ vTrue;
		V[k1517] = SBOX7(V[k1127], V[k1126], V[k1125], V[k1124], V[k1123], V[k1122], V[k1121], V[k1120]) ^ V[k317]^V[k217] ^ vFalse;

//fprintf(stderr,".");
		//	k190 = 0x6b ^ sbox[k151] ^ k030;
		V[k1900] = SBOX0(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k300] ^ vTrue;
		V[k1901] = SBOX1(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k301] ^ vTrue;
		V[k1902] = SBOX2(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k302] ^ vFalse;
		V[k1903] = SBOX3(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k303] ^ vTrue;
		V[k1904] = SBOX4(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k304] ^ vFalse;
		V[k1905] = SBOX5(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k305] ^ vTrue;
		V[k1906] = SBOX6(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k306] ^ vTrue;
		V[k1907] = SBOX7(V[k1517], V[k1516], V[k1515], V[k1514], V[k1513], V[k1512], V[k1511], V[k1510]) ^ V[k307] ^ vFalse;

//fprintf(stderr,".");
		//	k233 = 0x63 ^ sbox[k190] ^ k073;
		V[k2330] = SBOX0(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0730] ^ vTrue;
		V[k2331] = SBOX1(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0731] ^ vTrue;
		V[k2332] = SBOX2(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0732] ^ vFalse;
		V[k2333] = SBOX3(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0733] ^ vFalse;
		V[k2334] = SBOX4(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0734] ^ vFalse;
		V[k2335] = SBOX5(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0735] ^ vTrue;
		V[k2336] = SBOX6(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0736] ^ vTrue;
		V[k2337] = SBOX7(V[k1907], V[k1906], V[k1905], V[k1904], V[k1903], V[k1902], V[k1901], V[k1900]) ^ V[k0737] ^ vFalse;

//fprintf(stderr,".");
		//	k272 = 0x63 ^ sbox[k233] ^ k112;
		V[k2720] = SBOX0(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1120] ^ vTrue;
		V[k2721] = SBOX1(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1121] ^ vTrue;
		V[k2722] = SBOX2(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1122] ^ vFalse;
		V[k2723] = SBOX3(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1123] ^ vFalse;
		V[k2724] = SBOX4(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1124] ^ vFalse;
		V[k2725] = SBOX5(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1125] ^ vTrue;
		V[k2726] = SBOX6(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1126] ^ vTrue;
		V[k2727] = SBOX7(V[k2337], V[k2336], V[k2335], V[k2334], V[k2333], V[k2332], V[k2331], V[k2330]) ^ V[k1127] ^ vFalse;

//fprintf(stderr,".");
		//	k311 = 0x63 ^ sbox[k272] ^ k151;
		V[k3110] = SBOX0(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1510] ^ vTrue;
		V[k3111] = SBOX1(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1511] ^ vTrue;
		V[k3112] = SBOX2(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1512] ^ vFalse;
		V[k3113] = SBOX3(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1513] ^ vFalse;
		V[k3114] = SBOX4(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1514] ^ vFalse;
		V[k3115] = SBOX5(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1515] ^ vTrue;
		V[k3116] = SBOX6(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1516] ^ vTrue;
		V[k3117] = SBOX7(V[k2727], V[k2726], V[k2725], V[k2724], V[k2723], V[k2722], V[k2721], V[k2720]) ^ V[k1517] ^ vFalse;

//fprintf(stderr,".");
		//	k350 = 0xe3 ^ sbox[k311] ^ k190;
		V[k3500] = SBOX0(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1900] ^ vTrue;
		V[k3501] = SBOX1(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1901] ^ vTrue;
		V[k3502] = SBOX2(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1902] ^ vFalse;
		V[k3503] = SBOX3(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1903] ^ vFalse;
		V[k3504] = SBOX4(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1904] ^ vFalse;
		V[k3505] = SBOX5(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1905] ^ vTrue;
		V[k3506] = SBOX6(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1906] ^ vTrue;
		V[k3507] = SBOX7(V[k3117], V[k3116], V[k3115], V[k3114], V[k3113], V[k3112], V[k3111], V[k3110]) ^ V[k1907] ^ vTrue;

//fprintf(stderr,".");
		//	k393 = 0x63 ^ sbox[k350] ^ k233;
		V[k3930] = SBOX0(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2330] ^ vTrue;
		V[k3931] = SBOX1(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2331] ^ vTrue;
		V[k3932] = SBOX2(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2332] ^ vFalse;
		V[k3933] = SBOX3(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2333] ^ vFalse;
		V[k3934] = SBOX4(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2334] ^ vFalse;
		V[k3935] = SBOX5(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2335] ^ vTrue;
		V[k3936] = SBOX6(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2336] ^ vTrue;
		V[k3937] = SBOX7(V[k3507], V[k3506], V[k3505], V[k3504], V[k3503], V[k3502], V[k3501], V[k3500]) ^ V[k2337] ^ vFalse;

//fprintf(stderr,".");
		//	k432 = 0x63 ^ sbox[k393] ^ k272;
		V[k4320] = SBOX0(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2720] ^ vTrue;
		V[k4321] = SBOX1(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2721] ^ vTrue;
		V[k4322] = SBOX2(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2722] ^ vFalse;
		V[k4323] = SBOX3(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2723] ^ vFalse;
		V[k4324] = SBOX4(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2724] ^ vFalse;
		V[k4325] = SBOX5(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2725] ^ vTrue;
		V[k4326] = SBOX6(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2726] ^ vTrue;
		V[k4327] = SBOX7(V[k3937], V[k3936], V[k3935], V[k3934], V[k3933], V[k3932], V[k3931], V[k3930]) ^ V[k2727] ^ vFalse;

//fprintf(stderr,"k072");
		//	k072 = 0x63 ^ sbox[k033] ^ k032^k012^k022^k002;
		V[k0720] = SBOX0(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k320]^V[k120]^V[k220]^V[k020] ^ vTrue;
		V[k0721] = SBOX1(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k321]^V[k121]^V[k221]^V[k021] ^ vTrue;
		V[k0722] = SBOX2(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k322]^V[k122]^V[k222]^V[k022] ^ vFalse;
		V[k0723] = SBOX3(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k323]^V[k123]^V[k223]^V[k023] ^ vFalse;
		V[k0724] = SBOX4(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k324]^V[k124]^V[k224]^V[k024] ^ vFalse;
		V[k0725] = SBOX5(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k325]^V[k125]^V[k225]^V[k025] ^ vTrue;
		V[k0726] = SBOX6(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k326]^V[k126]^V[k226]^V[k026] ^ vTrue;
		V[k0727] = SBOX7(V[k337], V[k336], V[k335], V[k334], V[k333], V[k332], V[k331], V[k330]) ^ V[k327]^V[k127]^V[k227]^V[k027] ^ vFalse;

//fprintf(stderr,".");
		//	k111 = 0x63 ^ sbox[k072] ^ k031^k011;
		V[k1110] = SBOX0(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k310]^V[k110] ^ vTrue;
		V[k1111] = SBOX1(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k311]^V[k111] ^ vTrue;
		V[k1112] = SBOX2(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k312]^V[k112] ^ vFalse;
		V[k1113] = SBOX3(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k313]^V[k113] ^ vFalse;
		V[k1114] = SBOX4(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k314]^V[k114] ^ vFalse;
		V[k1115] = SBOX5(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k315]^V[k115] ^ vTrue;
		V[k1116] = SBOX6(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k316]^V[k116] ^ vTrue;
		V[k1117] = SBOX7(V[k0727], V[k0726], V[k0725], V[k0724], V[k0723], V[k0722], V[k0721], V[k0720]) ^ V[k317]^V[k117] ^ vFalse;

//fprintf(stderr,".");
		//	k150 = 0x67 ^ sbox[k111] ^ k030^k020;
		V[k1500] = SBOX0(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k300]^V[k200] ^ vTrue;
		V[k1501] = SBOX1(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k301]^V[k201] ^ vTrue;
		V[k1502] = SBOX2(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k302]^V[k202] ^ vTrue;
		V[k1503] = SBOX3(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k303]^V[k203] ^ vFalse;
		V[k1504] = SBOX4(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k304]^V[k204] ^ vFalse;
		V[k1505] = SBOX5(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k305]^V[k205] ^ vTrue;
		V[k1506] = SBOX6(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k306]^V[k206] ^ vTrue;
		V[k1507] = SBOX7(V[k1117], V[k1116], V[k1115], V[k1114], V[k1113], V[k1112], V[k1111], V[k1110]) ^ V[k307]^V[k207] ^ vFalse;

//fprintf(stderr,".");
		//	k193 = 0x63 ^ sbox[k150] ^ k033;
		V[k1930] = SBOX0(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k330] ^ vTrue;
		V[k1931] = SBOX1(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k331] ^ vTrue;
		V[k1932] = SBOX2(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k332] ^ vFalse;
		V[k1933] = SBOX3(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k333] ^ vFalse;
		V[k1934] = SBOX4(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k334] ^ vFalse;
		V[k1935] = SBOX5(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k335] ^ vTrue;
		V[k1936] = SBOX6(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k336] ^ vTrue;
		V[k1937] = SBOX7(V[k1507], V[k1506], V[k1505], V[k1504], V[k1503], V[k1502], V[k1501], V[k1500]) ^ V[k337] ^ vFalse;

//fprintf(stderr,".");
		//	k232 = 0x63 ^ sbox[k193] ^ k0727;
		V[k2320] = SBOX0(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0720] ^ vTrue;
		V[k2321] = SBOX1(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0721] ^ vTrue;
		V[k2322] = SBOX2(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0722] ^ vFalse;
		V[k2323] = SBOX3(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0723] ^ vFalse;
		V[k2324] = SBOX4(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0724] ^ vFalse;
		V[k2325] = SBOX5(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0725] ^ vTrue;
		V[k2326] = SBOX6(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0726] ^ vTrue;
		V[k2327] = SBOX7(V[k1937], V[k1936], V[k1935], V[k1934], V[k1933], V[k1932], V[k1931], V[k1930]) ^ V[k0727] ^ vFalse;

//fprintf(stderr,".");
		//	k271 = 0x63 ^ sbox[k232] ^ k111;
		V[k2710] = SBOX0(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1110] ^ vTrue;
		V[k2711] = SBOX1(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1111] ^ vTrue;
		V[k2712] = SBOX2(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1112] ^ vFalse;
		V[k2713] = SBOX3(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1113] ^ vFalse;
		V[k2714] = SBOX4(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1114] ^ vFalse;
		V[k2715] = SBOX5(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1115] ^ vTrue;
		V[k2716] = SBOX6(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1116] ^ vTrue;
		V[k2717] = SBOX7(V[k2327], V[k2326], V[k2325], V[k2324], V[k2323], V[k2322], V[k2321], V[k2320]) ^ V[k1117] ^ vFalse;

//fprintf(stderr,".");
		//	k310 = 0x23 ^ sbox[k271] ^ k150;
		V[k3100] = SBOX0(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1500] ^ vTrue;
		V[k3101] = SBOX1(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1501] ^ vTrue;
		V[k3102] = SBOX2(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1502] ^ vFalse;
		V[k3103] = SBOX3(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1503] ^ vFalse;
		V[k3104] = SBOX4(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1504] ^ vFalse;
		V[k3105] = SBOX5(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1505] ^ vTrue;
		V[k3106] = SBOX6(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1506] ^ vFalse;
		V[k3107] = SBOX7(V[k2717], V[k2716], V[k2715], V[k2714], V[k2713], V[k2712], V[k2711], V[k2710]) ^ V[k1507] ^ vFalse;

//fprintf(stderr,".");
		//	k353 = 0x63 ^ sbox[k310] ^ k193;
		V[k3530] = SBOX0(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1930] ^ vTrue;
		V[k3531] = SBOX1(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1931] ^ vTrue;
		V[k3532] = SBOX2(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1932] ^ vFalse;
		V[k3533] = SBOX3(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1933] ^ vFalse;
		V[k3534] = SBOX4(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1934] ^ vFalse;
		V[k3535] = SBOX5(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1935] ^ vTrue;
		V[k3536] = SBOX6(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1936] ^ vTrue;
		V[k3537] = SBOX7(V[k3107], V[k3106], V[k3105], V[k3104], V[k3103], V[k3102], V[k3101], V[k3100]) ^ V[k1937] ^ vFalse;

//fprintf(stderr,".");
		//	k392 = 0x63 ^ sbox[k353] ^ k232;
		V[k3920] = SBOX0(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2320] ^ vTrue;
		V[k3921] = SBOX1(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2321] ^ vTrue;
		V[k3922] = SBOX2(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2322] ^ vFalse;
		V[k3923] = SBOX3(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2323] ^ vFalse;
		V[k3924] = SBOX4(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2324] ^ vFalse;
		V[k3925] = SBOX5(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2325] ^ vTrue;
		V[k3926] = SBOX6(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2326] ^ vTrue;
		V[k3927] = SBOX7(V[k3537], V[k3536], V[k3535], V[k3534], V[k3533], V[k3532], V[k3531], V[k3530]) ^ V[k2327] ^ vFalse;

//fprintf(stderr,".");
		//	k431 = 0x63 ^ sbox[k392] ^ k271;
		V[k4310] = SBOX0(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2710] ^ vTrue;
		V[k4311] = SBOX1(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2711] ^ vTrue;
		V[k4312] = SBOX2(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2712] ^ vFalse;
		V[k4313] = SBOX3(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2713] ^ vFalse;
		V[k4314] = SBOX4(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2714] ^ vFalse;
		V[k4315] = SBOX5(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2715] ^ vTrue;
		V[k4316] = SBOX6(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2716] ^ vTrue;
		V[k4317] = SBOX7(V[k3927], V[k3926], V[k3925], V[k3924], V[k3923], V[k3922], V[k3921], V[k3920]) ^ V[k2717] ^ vFalse;

//fprintf(stderr,"k071");
		//	k071 = 0x63 ^ sbox[k032] ^ k031^k011^k021^k001;
		V[k0710] = SBOX0(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k310]^V[k110]^V[k210]^V[k010] ^ vTrue;
		V[k0711] = SBOX1(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k311]^V[k111]^V[k211]^V[k011] ^ vTrue;
		V[k0712] = SBOX2(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k312]^V[k112]^V[k212]^V[k012] ^ vFalse;
		V[k0713] = SBOX3(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k313]^V[k113]^V[k213]^V[k013] ^ vFalse;
		V[k0714] = SBOX4(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k314]^V[k114]^V[k214]^V[k014] ^ vFalse;
		V[k0715] = SBOX5(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k315]^V[k115]^V[k215]^V[k015] ^ vTrue;
		V[k0716] = SBOX6(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k316]^V[k116]^V[k216]^V[k016] ^ vTrue;
		V[k0717] = SBOX7(V[k327], V[k326], V[k325], V[k324], V[k323], V[k322], V[k321], V[k320]) ^ V[k317]^V[k117]^V[k217]^V[k017] ^ vFalse;

//fprintf(stderr,".");
		//	k110 = 0x61 ^ sbox[k071] ^ k030^k010;
		V[k1100] = SBOX0(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k300]^V[k100] ^ vTrue;
		V[k1101] = SBOX1(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k301]^V[k101] ^ vFalse;
		V[k1102] = SBOX2(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k302]^V[k102] ^ vFalse;
		V[k1103] = SBOX3(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k303]^V[k103] ^ vFalse;
		V[k1104] = SBOX4(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k304]^V[k104] ^ vFalse;
		V[k1105] = SBOX5(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k305]^V[k105] ^ vTrue;
		V[k1106] = SBOX6(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k306]^V[k106] ^ vTrue;
		V[k1107] = SBOX7(V[k0717], V[k0716], V[k0715], V[k0714], V[k0713], V[k0712], V[k0711], V[k0710]) ^ V[k307]^V[k107] ^ vFalse;

//fprintf(stderr,".");
		//	k153 = 0x63 ^ sbox[k110] ^ k033^k023;
		V[k1530] = SBOX0(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k330]^V[k230] ^ vTrue;
		V[k1531] = SBOX1(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k331]^V[k231] ^ vTrue;
		V[k1532] = SBOX2(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k332]^V[k232] ^ vFalse;
		V[k1533] = SBOX3(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k333]^V[k233] ^ vFalse;
		V[k1534] = SBOX4(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k334]^V[k234] ^ vFalse;
		V[k1535] = SBOX5(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k335]^V[k235] ^ vTrue;
		V[k1536] = SBOX6(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k336]^V[k236] ^ vTrue;
		V[k1537] = SBOX7(V[k1107], V[k1106], V[k1105], V[k1104], V[k1103], V[k1102], V[k1101], V[k1100]) ^ V[k337]^V[k237] ^ vFalse;

//fprintf(stderr,".");
		//	k192 = 0x63 ^ sbox[k153] ^ k032;
		V[k1920] = SBOX0(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k320] ^ vTrue;
		V[k1921] = SBOX1(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k321] ^ vTrue;
		V[k1922] = SBOX2(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k322] ^ vFalse;
		V[k1923] = SBOX3(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k323] ^ vFalse;
		V[k1924] = SBOX4(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k324] ^ vFalse;
		V[k1925] = SBOX5(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k325] ^ vTrue;
		V[k1926] = SBOX6(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k326] ^ vTrue;
		V[k1927] = SBOX7(V[k1537], V[k1536], V[k1535], V[k1534], V[k1533], V[k1532], V[k1531], V[k1530]) ^ V[k327] ^ vFalse;

//fprintf(stderr,".");
		//	k231 = 0x63 ^ sbox[k192] ^ k071;
		V[k2310] = SBOX0(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0710] ^ vTrue;
		V[k2311] = SBOX1(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0711] ^ vTrue;
		V[k2312] = SBOX2(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0712] ^ vFalse;
		V[k2313] = SBOX3(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0713] ^ vFalse;
		V[k2314] = SBOX4(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0714] ^ vFalse;
		V[k2315] = SBOX5(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0715] ^ vTrue;
		V[k2316] = SBOX6(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0716] ^ vTrue;
		V[k2317] = SBOX7(V[k1927], V[k1926], V[k1925], V[k1924], V[k1923], V[k1922], V[k1921], V[k1920]) ^ V[k0717] ^ vFalse;

//fprintf(stderr,".");
		//	k270 = 0x43 ^ sbox[k231] ^ k110;
		V[k2700] = SBOX0(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1100] ^ vTrue;
		V[k2701] = SBOX1(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1101] ^ vTrue;
		V[k2702] = SBOX2(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1102] ^ vFalse;
		V[k2703] = SBOX3(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1103] ^ vFalse;
		V[k2704] = SBOX4(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1104] ^ vFalse;
		V[k2705] = SBOX5(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1105] ^ vFalse;
		V[k2706] = SBOX6(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1106] ^ vTrue;
		V[k2707] = SBOX7(V[k2317], V[k2316], V[k2315], V[k2314], V[k2313], V[k2312], V[k2311], V[k2310]) ^ V[k1107] ^ vFalse;

//fprintf(stderr,".");
		//	k313 = 0x63 ^ sbox[k270] ^ k153;
		V[k3130] = SBOX0(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1530] ^ vTrue;
		V[k3131] = SBOX1(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1531] ^ vTrue;
		V[k3132] = SBOX2(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1532] ^ vFalse;
		V[k3133] = SBOX3(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1533] ^ vFalse;
		V[k3134] = SBOX4(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1534] ^ vFalse;
		V[k3135] = SBOX5(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1535] ^ vTrue;
		V[k3136] = SBOX6(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1536] ^ vTrue;
		V[k3137] = SBOX7(V[k2707], V[k2706], V[k2705], V[k2704], V[k2703], V[k2702], V[k2701], V[k2700]) ^ V[k1537] ^ vFalse;

//fprintf(stderr,".");
		//	k352 = 0x63 ^ sbox[k313] ^ k192;
		V[k3520] = SBOX0(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1920] ^ vTrue;
		V[k3521] = SBOX1(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1921] ^ vTrue;
		V[k3522] = SBOX2(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1922] ^ vFalse;
		V[k3523] = SBOX3(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1923] ^ vFalse;
		V[k3524] = SBOX4(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1924] ^ vFalse;
		V[k3525] = SBOX5(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1925] ^ vTrue;
		V[k3526] = SBOX6(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1926] ^ vTrue;
		V[k3527] = SBOX7(V[k3137], V[k3136], V[k3135], V[k3134], V[k3133], V[k3132], V[k3131], V[k3130]) ^ V[k1927] ^ vFalse;

//fprintf(stderr,".");
		//	k391 = 0x63 ^ sbox[k352] ^ k231;
		V[k3910] = SBOX0(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2310] ^ vTrue;
		V[k3911] = SBOX1(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2311] ^ vTrue;
		V[k3912] = SBOX2(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2312] ^ vFalse;
		V[k3913] = SBOX3(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2313] ^ vFalse;
		V[k3914] = SBOX4(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2314] ^ vFalse;
		V[k3915] = SBOX5(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2315] ^ vTrue;
		V[k3916] = SBOX6(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2316] ^ vTrue;
		V[k3917] = SBOX7(V[k3527], V[k3526], V[k3525], V[k3524], V[k3523], V[k3522], V[k3521], V[k3520]) ^ V[k2317] ^ vFalse;

//fprintf(stderr,".");
		//	k430 = 0x55 ^ sbox[k391] ^ k270;
		V[k4300] = SBOX0(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2700] ^ vTrue;
		V[k4301] = SBOX1(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2701] ^ vFalse;
		V[k4302] = SBOX2(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2702] ^ vTrue;
		V[k4303] = SBOX3(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2703] ^ vFalse;
		V[k4304] = SBOX4(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2704] ^ vTrue;
		V[k4305] = SBOX5(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2705] ^ vFalse;
		V[k4306] = SBOX6(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2706] ^ vTrue;
		V[k4307] = SBOX7(V[k3917], V[k3916], V[k3915], V[k3914], V[k3913], V[k3912], V[k3911], V[k3910]) ^ V[k2707] ^ vFalse;

//fprintf(stderr,"n000");
		// int n000 = zbox0[idata[ 0] ^ k000];
		V[v0000] = ZBOX0(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);
		V[v0001] = ZBOX1(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);
		V[v0002] = ZBOX2(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);
		V[v0003] = ZBOX3(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);
		V[v0004] = ZBOX4(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);
		V[v0005] = ZBOX5(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);
		V[v0006] = ZBOX6(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);
		V[v0007] = ZBOX7(V[i007]^V[k007], V[i006]^V[k006], V[i005]^V[k005], V[i004]^V[k004], V[i003]^V[k003], V[i002]^V[k002], V[i001]^V[k001], V[i000]^V[k000]);

		// int n010 = zbox0[idata[ 1] ^ k001];
		V[v0100] = ZBOX0(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);
		V[v0101] = ZBOX1(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);
		V[v0102] = ZBOX2(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);
		V[v0103] = ZBOX3(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);
		V[v0104] = ZBOX4(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);
		V[v0105] = ZBOX5(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);
		V[v0106] = ZBOX6(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);
		V[v0107] = ZBOX7(V[i017]^V[k017], V[i016]^V[k016], V[i015]^V[k015], V[i014]^V[k014], V[i013]^V[k013], V[i012]^V[k012], V[i011]^V[k011], V[i010]^V[k010]);

		// int n020 = zbox0[idata[ 2] ^ k002];
		V[v0200] = ZBOX0(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);
		V[v0201] = ZBOX1(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);
		V[v0202] = ZBOX2(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);
		V[v0203] = ZBOX3(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);
		V[v0204] = ZBOX4(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);
		V[v0205] = ZBOX5(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);
		V[v0206] = ZBOX6(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);
		V[v0207] = ZBOX7(V[i027]^V[k027], V[i026]^V[k026], V[i025]^V[k025], V[i024]^V[k024], V[i023]^V[k023], V[i022]^V[k022], V[i021]^V[k021], V[i020]^V[k020]);

		// int n030 = zbox0[idata[ 3] ^ k003];
		V[v0300] = ZBOX0(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);
		V[v0301] = ZBOX1(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);
		V[v0302] = ZBOX2(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);
		V[v0303] = ZBOX3(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);
		V[v0304] = ZBOX4(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);
		V[v0305] = ZBOX5(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);
		V[v0306] = ZBOX6(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);
		V[v0307] = ZBOX7(V[i037]^V[k037], V[i036]^V[k036], V[i035]^V[k035], V[i034]^V[k034], V[i033]^V[k033], V[i032]^V[k032], V[i031]^V[k031], V[i030]^V[k030]);

//fprintf(stderr,".");
		// int n001 = zbox0[idata[ 4] ^ k010];
		V[v0010] = ZBOX0(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);
		V[v0011] = ZBOX1(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);
		V[v0012] = ZBOX2(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);
		V[v0013] = ZBOX3(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);
		V[v0014] = ZBOX4(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);
		V[v0015] = ZBOX5(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);
		V[v0016] = ZBOX6(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);
		V[v0017] = ZBOX7(V[i107]^V[k107], V[i106]^V[k106], V[i105]^V[k105], V[i104]^V[k104], V[i103]^V[k103], V[i102]^V[k102], V[i101]^V[k101], V[i100]^V[k100]);

		// int n011 = zbox0[idata[ 5] ^ k011];
		V[v0110] = ZBOX0(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);
		V[v0111] = ZBOX1(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);
		V[v0112] = ZBOX2(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);
		V[v0113] = ZBOX3(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);
		V[v0114] = ZBOX4(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);
		V[v0115] = ZBOX5(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);
		V[v0116] = ZBOX6(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);
		V[v0117] = ZBOX7(V[i117]^V[k117], V[i116]^V[k116], V[i115]^V[k115], V[i114]^V[k114], V[i113]^V[k113], V[i112]^V[k112], V[i111]^V[k111], V[i110]^V[k110]);

		// int n021 = zbox0[idata[ 6] ^ k012];
		V[v0210] = ZBOX0(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);
		V[v0211] = ZBOX1(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);
		V[v0212] = ZBOX2(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);
		V[v0213] = ZBOX3(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);
		V[v0214] = ZBOX4(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);
		V[v0215] = ZBOX5(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);
		V[v0216] = ZBOX6(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);
		V[v0217] = ZBOX7(V[i127]^V[k127], V[i126]^V[k126], V[i125]^V[k125], V[i124]^V[k124], V[i123]^V[k123], V[i122]^V[k122], V[i121]^V[k121], V[i120]^V[k120]);

		// int n031 = zbox0[idata[ 7] ^ k013];
		V[v0310] = ZBOX0(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);
		V[v0311] = ZBOX1(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);
		V[v0312] = ZBOX2(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);
		V[v0313] = ZBOX3(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);
		V[v0314] = ZBOX4(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);
		V[v0315] = ZBOX5(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);
		V[v0316] = ZBOX6(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);
		V[v0317] = ZBOX7(V[i137]^V[k137], V[i136]^V[k136], V[i135]^V[k135], V[i134]^V[k134], V[i133]^V[k133], V[i132]^V[k132], V[i131]^V[k131], V[i130]^V[k130]);

//fprintf(stderr,".");
		// int n002 = zbox0[idata[ 8] ^ k020];
		V[v0020] = ZBOX0(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);
		V[v0021] = ZBOX1(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);
		V[v0022] = ZBOX2(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);
		V[v0023] = ZBOX3(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);
		V[v0024] = ZBOX4(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);
		V[v0025] = ZBOX5(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);
		V[v0026] = ZBOX6(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);
		V[v0027] = ZBOX7(V[i207]^V[k207], V[i206]^V[k206], V[i205]^V[k205], V[i204]^V[k204], V[i203]^V[k203], V[i202]^V[k202], V[i201]^V[k201], V[i200]^V[k200]);

		// int n012 = zbox0[idata[ 9] ^ k021];
		V[v0120] = ZBOX0(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);
		V[v0121] = ZBOX1(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);
		V[v0122] = ZBOX2(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);
		V[v0123] = ZBOX3(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);
		V[v0124] = ZBOX4(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);
		V[v0125] = ZBOX5(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);
		V[v0126] = ZBOX6(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);
		V[v0127] = ZBOX7(V[i217]^V[k217], V[i216]^V[k216], V[i215]^V[k215], V[i214]^V[k214], V[i213]^V[k213], V[i212]^V[k212], V[i211]^V[k211], V[i210]^V[k210]);

		// int n022 = zbox0[idata[10] ^ k022];
		V[v0220] = ZBOX0(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);
		V[v0221] = ZBOX1(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);
		V[v0222] = ZBOX2(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);
		V[v0223] = ZBOX3(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);
		V[v0224] = ZBOX4(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);
		V[v0225] = ZBOX5(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);
		V[v0226] = ZBOX6(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);
		V[v0227] = ZBOX7(V[i227]^V[k227], V[i226]^V[k226], V[i225]^V[k225], V[i224]^V[k224], V[i223]^V[k223], V[i222]^V[k222], V[i221]^V[k221], V[i220]^V[k220]);

		// int n032 = zbox0[idata[11] ^ k023];
		V[v0320] = ZBOX0(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);
		V[v0321] = ZBOX1(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);
		V[v0322] = ZBOX2(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);
		V[v0323] = ZBOX3(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);
		V[v0324] = ZBOX4(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);
		V[v0325] = ZBOX5(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);
		V[v0326] = ZBOX6(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);
		V[v0327] = ZBOX7(V[i237]^V[k237], V[i236]^V[k236], V[i235]^V[k235], V[i234]^V[k234], V[i233]^V[k233], V[i232]^V[k232], V[i231]^V[k231], V[i230]^V[k230]);

//fprintf(stderr,".");
		// int n003 = zbox0[idata[12] ^ k030];
		V[v0030] = ZBOX0(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);
		V[v0031] = ZBOX1(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);
		V[v0032] = ZBOX2(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);
		V[v0033] = ZBOX3(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);
		V[v0034] = ZBOX4(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);
		V[v0035] = ZBOX5(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);
		V[v0036] = ZBOX6(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);
		V[v0037] = ZBOX7(V[i307]^V[k307], V[i306]^V[k306], V[i305]^V[k305], V[i304]^V[k304], V[i303]^V[k303], V[i302]^V[k302], V[i301]^V[k301], V[i300]^V[k300]);

		// int n013 = zbox0[idata[13] ^ k031];
		V[v0130] = ZBOX0(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);
		V[v0131] = ZBOX1(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);
		V[v0132] = ZBOX2(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);
		V[v0133] = ZBOX3(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);
		V[v0134] = ZBOX4(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);
		V[v0135] = ZBOX5(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);
		V[v0136] = ZBOX6(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);
		V[v0137] = ZBOX7(V[i317]^V[k317], V[i316]^V[k316], V[i315]^V[k315], V[i314]^V[k314], V[i313]^V[k313], V[i312]^V[k312], V[i311]^V[k311], V[i310]^V[k310]);

		// int n023 = zbox0[idata[14] ^ k032];
		V[v0230] = ZBOX0(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);
		V[v0231] = ZBOX1(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);
		V[v0232] = ZBOX2(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);
		V[v0233] = ZBOX3(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);
		V[v0234] = ZBOX4(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);
		V[v0235] = ZBOX5(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);
		V[v0236] = ZBOX6(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);
		V[v0237] = ZBOX7(V[i327]^V[k327], V[i326]^V[k326], V[i325]^V[k325], V[i324]^V[k324], V[i323]^V[k323], V[i322]^V[k322], V[i321]^V[k321], V[i320]^V[k320]);

		// int n033 = zbox0[idata[15] ^ k033];
		V[v0330] = ZBOX0(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);
		V[v0331] = ZBOX1(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);
		V[v0332] = ZBOX2(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);
		V[v0333] = ZBOX3(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);
		V[v0334] = ZBOX4(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);
		V[v0335] = ZBOX5(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);
		V[v0336] = ZBOX6(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);
		V[v0337] = ZBOX7(V[i337]^V[k337], V[i336]^V[k336], V[i335]^V[k335], V[i334]^V[k334], V[i333]^V[k333], V[i332]^V[k332], V[i331]^V[k331], V[i330]^V[k330]);

		if (opt_split)
			splitTree(V, v0000, 0);

		NODE _b0,_b1,_b2,_b3,_b4,_b5,_b6,_b7;

#define setB4(A,B,C,D,W,X,Y,Z) \
_b0 = (V[A##0])^V[A##3]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##0]^V[X##0]^V[Y##0]^V[Z##0]^vTrue;\
_b1 = (V[A##1])^V[A##3]^V[A##5]^V[A##6]^V[A##7]^V[B##0]^V[B##3]^V[C##0]^V[C##1]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##5]^V[D##6]^V[D##7]^V[W##1]^V[X##1]^V[Y##1]^V[Z##1]^vTrue;\
_b2 = (V[A##2])^V[A##5]^V[B##0]^V[B##1]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##1]^V[C##2]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##6]^V[D##7]^V[W##2]^V[X##2]^V[Y##2]^V[Z##2];\
_b3 = (V[A##4])^V[A##5]^V[A##7]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##7]^V[W##3]^V[X##3]^V[Y##3]^V[Z##3];\
_b4 = (V[A##3])^V[A##5]^V[A##6]^V[B##0]^V[B##1]^V[B##2]^V[B##4]^V[B##5]^V[B##6]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[W##4]^V[X##4]^V[Y##4]^V[Z##4];\
_b5 = (V[A##0])^V[A##5]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[W##5]^V[X##5]^V[Y##5]^V[Z##5]^vTrue;\
_b6 = (V[A##1])^V[A##6]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[W##6]^V[X##6]^V[Y##6]^V[Z##6]^vTrue;\
_b7 = (V[A##2])^V[A##7]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##7]^V[X##7]^V[Y##7]^V[Z##7];
#define setB3(A,B,C,D,W,X,Y) \
_b0 = (V[A##0])^V[A##3]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##0]^V[X##0]^V[Y##0]^vTrue;\
_b1 = (V[A##1])^V[A##3]^V[A##5]^V[A##6]^V[A##7]^V[B##0]^V[B##3]^V[C##0]^V[C##1]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##5]^V[D##6]^V[D##7]^V[W##1]^V[X##1]^V[Y##1]^vTrue;\
_b2 = (V[A##2])^V[A##5]^V[B##0]^V[B##1]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##1]^V[C##2]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##6]^V[D##7]^V[W##2]^V[X##2]^V[Y##2];\
_b3 = (V[A##4])^V[A##5]^V[A##7]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##7]^V[W##3]^V[X##3]^V[Y##3];\
_b4 = (V[A##3])^V[A##5]^V[A##6]^V[B##0]^V[B##1]^V[B##2]^V[B##4]^V[B##5]^V[B##6]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[W##4]^V[X##4]^V[Y##4];\
_b5 = (V[A##0])^V[A##5]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[W##5]^V[X##5]^V[Y##5]^vTrue;\
_b6 = (V[A##1])^V[A##6]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[W##6]^V[X##6]^V[Y##6]^vTrue;\
_b7 = (V[A##2])^V[A##7]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##7]^V[X##7]^V[Y##7];
#define setB2(A,B,C,D,W,X) \
_b0 = (V[A##0])^V[A##3]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##0]^V[X##0]^vTrue;\
_b1 = (V[A##1])^V[A##3]^V[A##5]^V[A##6]^V[A##7]^V[B##0]^V[B##3]^V[C##0]^V[C##1]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##5]^V[D##6]^V[D##7]^V[W##1]^V[X##1]^vTrue;\
_b2 = (V[A##2])^V[A##5]^V[B##0]^V[B##1]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##1]^V[C##2]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##6]^V[D##7]^V[W##2]^V[X##2];\
_b3 = (V[A##4])^V[A##5]^V[A##7]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##7]^V[W##3]^V[X##3];\
_b4 = (V[A##3])^V[A##5]^V[A##6]^V[B##0]^V[B##1]^V[B##2]^V[B##4]^V[B##5]^V[B##6]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[W##4]^V[X##4];\
_b5 = (V[A##0])^V[A##5]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[W##5]^V[X##5]^vTrue;\
_b6 = (V[A##1])^V[A##6]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[W##6]^V[X##6]^vTrue;\
_b7 = (V[A##2])^V[A##7]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##7]^V[X##7];
#define setB1(A,B,C,D,W) \
_b0 = (V[A##0])^V[A##3]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##0]^vTrue;\
_b1 = (V[A##1])^V[A##3]^V[A##5]^V[A##6]^V[A##7]^V[B##0]^V[B##3]^V[C##0]^V[C##1]^V[C##5]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##5]^V[D##6]^V[D##7]^V[W##1]^vTrue;\
_b2 = (V[A##2])^V[A##5]^V[B##0]^V[B##1]^V[B##5]^V[B##6]^V[B##7]^V[C##0]^V[C##1]^V[C##2]^V[C##6]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##6]^V[D##7]^V[W##2];\
_b3 = (V[A##4])^V[A##5]^V[A##7]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##7]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##7]^V[W##3];\
_b4 = (V[A##3])^V[A##5]^V[A##6]^V[B##0]^V[B##1]^V[B##2]^V[B##4]^V[B##5]^V[B##6]^V[C##0]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[D##0]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[W##4];\
_b5 = (V[A##0])^V[A##5]^V[B##0]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[C##1]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[D##1]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[W##5]^vTrue;\
_b6 = (V[A##1])^V[A##6]^V[B##1]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[C##2]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[D##2]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[W##6]^vTrue;\
_b7 = (V[A##2])^V[A##7]^V[B##2]^V[B##3]^V[B##4]^V[B##5]^V[B##6]^V[C##3]^V[C##4]^V[C##5]^V[C##6]^V[C##7]^V[D##3]^V[D##4]^V[D##5]^V[D##6]^V[D##7]^V[W##7];

//fprintf(stderr,"v1000");
		// int v1000 = zbox0[mul3(v011)^mul2(v000)^mul1(v022)^mul1(v033)^k070^k030^k020^k010];
		setB4(v011,v000,v022,v033,k070,k30,k20,k10);
		V[v1000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1100 = zbox0[mul3(v022)^mul2(v011)^mul1(v000)^mul1(v033)^k071^k031^k021^k011];
		setB4(v022,v011,v000,v033,k071,k31,k21,k11);
		V[v1100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1200 = zbox0[mul3(v033)^mul2(v022)^mul1(v000)^mul1(v011)^k072^k032^k022^k012];
		setB4(v033,v022,v000,v011,k072,k32,k22,k12);
		V[v1200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1300 = zbox0[mul3(v000)^mul2(v033)^mul1(v011)^mul1(v022)^k073^k033^k023^k013];
		setB4(v000,v033,v011,v022,k073,k33,k23,k13);
		V[v1300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v1010 = zbox0[mul3(v012)^mul2(v001)^mul1(v023)^mul1(v030)^k070^k030^k020];
		setB3(v012,v001,v023,v030,k070,k30,k20);
		V[v1010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1110 = zbox0[mul3(v023)^mul2(v012)^mul1(v001)^mul1(v030)^k071^k031^k021];
		setB3(v023,v012,v001,v030,k071,k31,k21);
		V[v1110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1210 = zbox0[mul3(v030)^mul2(v023)^mul1(v001)^mul1(v012)^k072^k032^k022];
		setB3(v030,v023,v001,v012,k072,k32,k22);
		V[v1210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1310 = zbox0[mul3(v001)^mul2(v030)^mul1(v012)^mul1(v023)^k073^k033^k023];
		setB3(v001,v030,v012,v023,k073,k33,k23);
		V[v1310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v1020 = zbox0[mul3(v013)^mul2(v002)^mul1(v020)^mul1(v031)^k070^k030];
		setB2(v013,v002,v020,v031,k070,k30);
		V[v1020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1120 = zbox0[mul3(v020)^mul2(v013)^mul1(v002)^mul1(v031)^k071^k031];
		setB2(v020,v013,v002,v031,k071,k31);
		V[v1120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1220 = zbox0[mul3(v031)^mul2(v020)^mul1(v002)^mul1(v013)^k072^k032];
		setB2(v031,v020,v002,v013,k072,k32);
		V[v1220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1320 = zbox0[mul3(v002)^mul2(v031)^mul1(v013)^mul1(v020)^k073^k033];
		setB2(v002,v031,v013,v020,k073,k33);
		V[v1320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v1030 = zbox0[mul3(v010)^mul2(v003)^mul1(v021)^mul1(v032)^k070];
		setB1(v010,v003,v021,v032,k070);
		V[v1030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1130 = zbox0[mul3(v021)^mul2(v010)^mul1(v003)^mul1(v032)^k071];
		setB1(v021,v010,v003,v032,k071);
		V[v1130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1230 = zbox0[mul3(v032)^mul2(v021)^mul1(v003)^mul1(v010)^k072];
		setB1(v032,v021,v003,v010,k072);
		V[v1230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v1330 = zbox0[mul3(v003)^mul2(v032)^mul1(v010)^mul1(v021)^k073];
		setB1(v003,v032,v010,v021,k073);
		V[v1330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v1337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v1000, 1);

//fprintf(stderr,"v2000");
		// int v2000 = zbox0[mul3(v111)^mul2(v100)^mul1(v122)^mul1(v133)^k110^k070^k020];
		setB3(v111,v100,v122,v133,k110,k070,k20);
		V[v2000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2100 = zbox0[mul3(v122)^mul2(v111)^mul1(v100)^mul1(v133)^k111^k071^k021];
		setB3(v122,v111,v100,v133,k111,k071,k21);
		V[v2100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2200 = zbox0[mul3(v133)^mul2(v122)^mul1(v100)^mul1(v111)^k112^k072^k022];
		setB3(v133,v122,v100,v111,k112,k072,k22);
		V[v2200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2300 = zbox0[mul3(v100)^mul2(v133)^mul1(v111)^mul1(v122)^k113^k073^k023];
		setB3(v100,v133,v111,v122,k113,k073,k23);
		V[v2300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v2010 = zbox0[mul3(v112)^mul2(v101)^mul1(v123)^mul1(v130)^k110^k030];
		setB2(v112,v101,v123,v130,k110,k30);
		V[v2010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2110 = zbox0[mul3(v123)^mul2(v112)^mul1(v101)^mul1(v130)^k111^k031];
		setB2(v123,v112,v101,v130,k111,k31);
		V[v2110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2210 = zbox0[mul3(v130)^mul2(v123)^mul1(v101)^mul1(v112)^k112^k032];
		setB2(v130,v123,v101,v112,k112,k32);
		V[v2210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2310 = zbox0[mul3(v101)^mul2(v130)^mul1(v112)^mul1(v123)^k113^k033];
		setB2(v101,v130,v112,v123,k113,k33);
		V[v2310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v2020 = zbox0[mul3(v113)^mul2(v102)^mul1(v120)^mul1(v131)^k110^k070];
		setB2(v113,v102,v120,v131,k110,k070);
		V[v2020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2120 = zbox0[mul3(v120)^mul2(v113)^mul1(v102)^mul1(v131)^k111^k071];
		setB2(v120,v113,v102,v131,k111,k071);
		V[v2120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2220 = zbox0[mul3(v131)^mul2(v120)^mul1(v102)^mul1(v113)^k112^k072];
		setB2(v131,v120,v102,v113,k112,k072);
		V[v2220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2320 = zbox0[mul3(v102)^mul2(v131)^mul1(v113)^mul1(v120)^k113^k073];
		setB2(v102,v131,v113,v120,k113,k073);
		V[v2320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v2030 = zbox0[mul3(v110)^mul2(v103)^mul1(v121)^mul1(v132)^k110];
		setB1(v110,v103,v121,v132,k110);
		V[v2030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2130 = zbox0[mul3(v121)^mul2(v110)^mul1(v103)^mul1(v132)^k111];
		setB1(v121,v110,v103,v132,k111);
		V[v2130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2230 = zbox0[mul3(v132)^mul2(v121)^mul1(v103)^mul1(v110)^k112];
		setB1(v132,v121,v103,v110,k112);
		V[v2230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v2330 = zbox0[mul3(v103)^mul2(v132)^mul1(v110)^mul1(v121)^k113];
		setB1(v103,v132,v110,v121,k113);
		V[v2330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v2337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v2000, 2);

//fprintf(stderr,"v3000");
		// int v3000 = zbox0[mul3(v211)^mul2(v200)^mul1(v222)^mul1(v233)^k150^k110^k070^k030];
		setB4(v211,v200,v222,v233,k150,k110,k070,k30);
		V[v3000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3100 = zbox0[mul3(v222)^mul2(v211)^mul1(v200)^mul1(v233)^k151^k111^k071^k031];
		setB4(v222,v211,v200,v233,k151,k111,k071,k31);
		V[v3100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3200 = zbox0[mul3(v233)^mul2(v222)^mul1(v200)^mul1(v211)^k152^k112^k072^k032];
		setB4(v233,v222,v200,v211,k152,k112,k072,k32);
		V[v3200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3300 = zbox0[mul3(v200)^mul2(v233)^mul1(v211)^mul1(v222)^k153^k113^k073^k033];
		setB4(v200,v233,v211,v222,k153,k113,k073,k33);
		V[v3300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v3010 = zbox0[mul3(v212)^mul2(v201)^mul1(v223)^mul1(v230)^k150^k070];
		setB2(v212,v201,v223,v230,k150,k070);
		V[v3010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3110 = zbox0[mul3(v223)^mul2(v212)^mul1(v201)^mul1(v230)^k151^k071];
		setB2(v223,v212,v201,v230,k151,k071);
		V[v3110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3210 = zbox0[mul3(v230)^mul2(v223)^mul1(v201)^mul1(v212)^k152^k072];
		setB2(v230,v223,v201,v212,k152,k072);
		V[v3210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3310 = zbox0[mul3(v201)^mul2(v230)^mul1(v212)^mul1(v223)^k153^k073];
		setB2(v201,v230,v212,v223,k153,k073);
		V[v3310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v3020 = zbox0[mul3(v213)^mul2(v202)^mul1(v220)^mul1(v231)^k150^k110];
		setB2(v213,v202,v220,v231,k150,k110);
		V[v3020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3120 = zbox0[mul3(v220)^mul2(v213)^mul1(v202)^mul1(v231)^k151^k111];
		setB2(v220,v213,v202,v231,k151,k111);
		V[v3120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3220 = zbox0[mul3(v231)^mul2(v220)^mul1(v202)^mul1(v213)^k152^k112];
		setB2(v231,v220,v202,v213,k152,k112);
		V[v3220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3320 = zbox0[mul3(v202)^mul2(v231)^mul1(v213)^mul1(v220)^k153^k113];
		setB2(v202,v231,v213,v220,k153,k113);
		V[v3320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v3030 = zbox0[mul3(v210)^mul2(v203)^mul1(v221)^mul1(v232)^k150];
		setB1(v210,v203,v221,v232,k150);
		V[v3030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3130 = zbox0[mul3(v221)^mul2(v210)^mul1(v203)^mul1(v232)^k151];
		setB1(v221,v210,v203,v232,k151);
		V[v3130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3230 = zbox0[mul3(v232)^mul2(v221)^mul1(v203)^mul1(v210)^k152];
		setB1(v232,v221,v203,v210,k152);
		V[v3230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v3330 = zbox0[mul3(v203)^mul2(v232)^mul1(v210)^mul1(v221)^k153];
		setB1(v203,v232,v210,v221,k153);
		V[v3330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v3337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v3000, 3);

//fprintf(stderr,"v4000");
		// int v4000 = zbox0[mul3(v311)^mul2(v300)^mul1(v322)^mul1(v333)^k190^k150^k110^k070];
		setB4(v311,v300,v322,v333,k190,k150,k110,k070);
		V[v4000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4100 = zbox0[mul3(v322)^mul2(v311)^mul1(v300)^mul1(v333)^k191^k151^k111^k071];
		setB4(v322,v311,v300,v333,k191,k151,k111,k071);
		V[v4100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4200 = zbox0[mul3(v333)^mul2(v322)^mul1(v300)^mul1(v311)^k192^k152^k112^k072];
		setB4(v333,v322,v300,v311,k192,k152,k112,k072);
		V[v4200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4300 = zbox0[mul3(v300)^mul2(v333)^mul1(v311)^mul1(v322)^k193^k153^k113^k073];
		setB4(v300,v333,v311,v322,k193,k153,k113,k073);
		V[v4300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v4010 = zbox0[mul3(v312)^mul2(v301)^mul1(v323)^mul1(v330)^k190^k110];
		setB2(v312,v301,v323,v330,k190,k110);
		V[v4010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4110 = zbox0[mul3(v323)^mul2(v312)^mul1(v301)^mul1(v330)^k191^k111];
		setB2(v323,v312,v301,v330,k191,k111);
		V[v4110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4210 = zbox0[mul3(v330)^mul2(v323)^mul1(v301)^mul1(v312)^k192^k112];
		setB2(v330,v323,v301,v312,k192,k112);
		V[v4210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4310 = zbox0[mul3(v301)^mul2(v330)^mul1(v312)^mul1(v323)^k193^k113];
		setB2(v301,v330,v312,v323,k193,k113);
		V[v4310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v4020 = zbox0[mul3(v313)^mul2(v302)^mul1(v320)^mul1(v331)^k190^k150];
		setB2(v313,v302,v320,v331,k190,k150);
		V[v4020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4120 = zbox0[mul3(v320)^mul2(v313)^mul1(v302)^mul1(v331)^k191^k151];
		setB2(v320,v313,v302,v331,k191,k151);
		V[v4120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4220 = zbox0[mul3(v331)^mul2(v320)^mul1(v302)^mul1(v313)^k192^k152];
		setB2(v331,v320,v302,v313,k192,k152);
		V[v4220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4320 = zbox0[mul3(v302)^mul2(v331)^mul1(v313)^mul1(v320)^k193^k153];
		setB2(v302,v331,v313,v320,k193,k153);
		V[v4320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v4030 = zbox0[mul3(v310)^mul2(v303)^mul1(v321)^mul1(v332)^k190];
		setB1(v310,v303,v321,v332,k190);
		V[v4030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4130 = zbox0[mul3(v321)^mul2(v310)^mul1(v303)^mul1(v332)^k191];
		setB1(v321,v310,v303,v332,k191);
		V[v4130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4230 = zbox0[mul3(v332)^mul2(v321)^mul1(v303)^mul1(v310)^k192];
		setB1(v332,v321,v303,v310,k192);
		V[v4230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v4330 = zbox0[mul3(v303)^mul2(v332)^mul1(v310)^mul1(v321)^k193];
		setB1(v303,v332,v310,v321,k193);
		V[v4330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v4337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v4000, 4);

//fprintf(stderr,"v5000");
		// int v5000 = zbox0[mul3(v411)^mul2(v400)^mul1(v422)^mul1(v433)^k230^k190^k150^k110];
		setB4(v411,v400,v422,v433,k230,k190,k150,k110);
		V[v5000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5100 = zbox0[mul3(v422)^mul2(v411)^mul1(v400)^mul1(v433)^k231^k191^k151^k111];
		setB4(v422,v411,v400,v433,k231,k191,k151,k111);
		V[v5100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5200 = zbox0[mul3(v433)^mul2(v422)^mul1(v400)^mul1(v411)^k232^k192^k152^k112];
		setB4(v433,v422,v400,v411,k232,k192,k152,k112);
		V[v5200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5300 = zbox0[mul3(v400)^mul2(v433)^mul1(v411)^mul1(v422)^k233^k193^k153^k113];
		setB4(v400,v433,v411,v422,k233,k193,k153,k113);
		V[v5300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v5010 = zbox0[mul3(v412)^mul2(v401)^mul1(v423)^mul1(v430)^k230^k150];
		setB2(v412,v401,v423,v430,k230,k150);
		V[v5010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5110 = zbox0[mul3(v423)^mul2(v412)^mul1(v401)^mul1(v430)^k231^k151];
		setB2(v423,v412,v401,v430,k231,k151);
		V[v5110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5210 = zbox0[mul3(v430)^mul2(v423)^mul1(v401)^mul1(v412)^k232^k152];
		setB2(v430,v423,v401,v412,k232,k152);
		V[v5210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5310 = zbox0[mul3(v401)^mul2(v430)^mul1(v412)^mul1(v423)^k233^k153];
		setB2(v401,v430,v412,v423,k233,k153);
		V[v5310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v5020 = zbox0[mul3(v413)^mul2(v402)^mul1(v420)^mul1(v431)^k230^k190];
		setB2(v413,v402,v420,v431,k230,k190);
		V[v5020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5120 = zbox0[mul3(v420)^mul2(v413)^mul1(v402)^mul1(v431)^k231^k191];
		setB2(v420,v413,v402,v431,k231,k191);
		V[v5120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5220 = zbox0[mul3(v431)^mul2(v420)^mul1(v402)^mul1(v413)^k232^k192];
		setB2(v431,v420,v402,v413,k232,k192);
		V[v5220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5320 = zbox0[mul3(v402)^mul2(v431)^mul1(v413)^mul1(v420)^k233^k193];
		setB2(v402,v431,v413,v420,k233,k193);
		V[v5320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v5030 = zbox0[mul3(v410)^mul2(v403)^mul1(v421)^mul1(v432)^k230];
		setB1(v410,v403,v421,v432,k230);
		V[v5030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5130 = zbox0[mul3(v421)^mul2(v410)^mul1(v403)^mul1(v432)^k231];
		setB1(v421,v410,v403,v432,k231);
		V[v5130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5230 = zbox0[mul3(v432)^mul2(v421)^mul1(v403)^mul1(v410)^k232];
		setB1(v432,v421,v403,v410,k232);
		V[v5230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v5330 = zbox0[mul3(v403)^mul2(v432)^mul1(v410)^mul1(v421)^k233];
		setB1(v403,v432,v410,v421,k233);
		V[v5330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v5337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v5000, 5);

//fprintf(stderr,"v6000");
		// int v6000 = zbox0[mul3(v511)^mul2(v500)^mul1(v522)^mul1(v533)^k270^k230^k190^k150];
		setB4(v511,v500,v522,v533,k270,k230,k190,k150);
		V[v6000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6100 = zbox0[mul3(v522)^mul2(v511)^mul1(v500)^mul1(v533)^k271^k231^k191^k151];
		setB4(v522,v511,v500,v533,k271,k231,k191,k151);
		V[v6100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6200 = zbox0[mul3(v533)^mul2(v522)^mul1(v500)^mul1(v511)^k272^k232^k192^k152];
		setB4(v533,v522,v500,v511,k272,k232,k192,k152);
		V[v6200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6300 = zbox0[mul3(v500)^mul2(v533)^mul1(v511)^mul1(v522)^k273^k233^k193^k153];
		setB4(v500,v533,v511,v522,k273,k233,k193,k153);
		V[v6300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v6010 = zbox0[mul3(v512)^mul2(v501)^mul1(v523)^mul1(v530)^k270^k190];
		setB2(v512,v501,v523,v530,k270,k190);
		V[v6010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6110 = zbox0[mul3(v523)^mul2(v512)^mul1(v501)^mul1(v530)^k271^k191];
		setB2(v523,v512,v501,v530,k271,k191);
		V[v6110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6210 = zbox0[mul3(v530)^mul2(v523)^mul1(v501)^mul1(v512)^k272^k192];
		setB2(v530,v523,v501,v512,k272,k192);
		V[v6210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6310 = zbox0[mul3(v501)^mul2(v530)^mul1(v512)^mul1(v523)^k273^k193];
		setB2(v501,v530,v512,v523,k273,k193);
		V[v6310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v6020 = zbox0[mul3(v513)^mul2(v502)^mul1(v520)^mul1(v531)^k270^k230];
		setB2(v513,v502,v520,v531,k270,k230);
		V[v6020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6120 = zbox0[mul3(v520)^mul2(v513)^mul1(v502)^mul1(v531)^k271^k231];
		setB2(v520,v513,v502,v531,k271,k231);
		V[v6120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6220 = zbox0[mul3(v531)^mul2(v520)^mul1(v502)^mul1(v513)^k272^k232];
		setB2(v531,v520,v502,v513,k272,k232);
		V[v6220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6320 = zbox0[mul3(v502)^mul2(v531)^mul1(v513)^mul1(v520)^k273^k233];
		setB2(v502,v531,v513,v520,k273,k233);
		V[v6320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v6030 = zbox0[mul3(v510)^mul2(v503)^mul1(v521)^mul1(v532)^k270];
		setB1(v510,v503,v521,v532,k270);
		V[v6030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6130 = zbox0[mul3(v521)^mul2(v510)^mul1(v503)^mul1(v532)^k271];
		setB1(v521,v510,v503,v532,k271);
		V[v6130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6230 = zbox0[mul3(v532)^mul2(v521)^mul1(v503)^mul1(v510)^k272];
		setB1(v532,v521,v503,v510,k272);
		V[v6230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v6330 = zbox0[mul3(v503)^mul2(v532)^mul1(v510)^mul1(v521)^k273];
		setB1(v503,v532,v510,v521,k273);
		V[v6330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v6337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v6000, 6);

//fprintf(stderr,"v7000");
		// int v7000 = zbox0[mul3(v611)^mul2(v600)^mul1(v622)^mul1(v633)^k310^k270^k230^k190];
		setB4(v611,v600,v622,v633,k310,k270,k230,k190);
		V[v7000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7100 = zbox0[mul3(v622)^mul2(v611)^mul1(v600)^mul1(v633)^k311^k271^k231^k191];
		setB4(v622,v611,v600,v633,k311,k271,k231,k191);
		V[v7100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7200 = zbox0[mul3(v633)^mul2(v622)^mul1(v600)^mul1(v611)^k312^k272^k232^k192];
		setB4(v633,v622,v600,v611,k312,k272,k232,k192);
		V[v7200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7300 = zbox0[mul3(v600)^mul2(v633)^mul1(v611)^mul1(v622)^k313^k273^k233^k193];
		setB4(v600,v633,v611,v622,k313,k273,k233,k193);
		V[v7300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v7010 = zbox0[mul3(v612)^mul2(v601)^mul1(v623)^mul1(v630)^k310^k230];
		setB2(v612,v601,v623,v630,k310,k230);
		V[v7010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7110 = zbox0[mul3(v623)^mul2(v612)^mul1(v601)^mul1(v630)^k311^k231];
		setB2(v623,v612,v601,v630,k311,k231);
		V[v7110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7210 = zbox0[mul3(v630)^mul2(v623)^mul1(v601)^mul1(v612)^k312^k232];
		setB2(v630,v623,v601,v612,k312,k232);
		V[v7210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7310 = zbox0[mul3(v601)^mul2(v630)^mul1(v612)^mul1(v623)^k313^k233];
		setB2(v601,v630,v612,v623,k313,k233);
		V[v7310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v7020 = zbox0[mul3(v613)^mul2(v602)^mul1(v620)^mul1(v631)^k310^k270];
		setB2(v613,v602,v620,v631,k310,k270);
		V[v7020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7120 = zbox0[mul3(v620)^mul2(v613)^mul1(v602)^mul1(v631)^k311^k271];
		setB2(v620,v613,v602,v631,k311,k271);
		V[v7120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7220 = zbox0[mul3(v631)^mul2(v620)^mul1(v602)^mul1(v613)^k312^k272];
		setB2(v631,v620,v602,v613,k312,k272);
		V[v7220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7320 = zbox0[mul3(v602)^mul2(v631)^mul1(v613)^mul1(v620)^k313^k273];
		setB2(v602,v631,v613,v620,k313,k273);
		V[v7320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v7030 = zbox0[mul3(v610)^mul2(v603)^mul1(v621)^mul1(v632)^k310];
		setB1(v610,v603,v621,v632,k310);
		V[v7030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7130 = zbox0[mul3(v621)^mul2(v610)^mul1(v603)^mul1(v632)^k311];
		setB1(v621,v610,v603,v632,k311);
		V[v7130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7230 = zbox0[mul3(v632)^mul2(v621)^mul1(v603)^mul1(v610)^k312];
		setB1(v632,v621,v603,v610,k312);
		V[v7230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v7330 = zbox0[mul3(v603)^mul2(v632)^mul1(v610)^mul1(v621)^k313];
		setB1(v603,v632,v610,v621,k313);
		V[v7330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v7337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v7000, 7);

//fprintf(stderr,"v8000");
		// int v8000 = zbox0[mul3(v711)^mul2(v700)^mul1(v722)^mul1(v733)^k350^k310^k270^k230];
		setB4(v711,v700,v722,v733,k350,k310,k270,k230);
		V[v8000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8100 = zbox0[mul3(v722)^mul2(v711)^mul1(v700)^mul1(v733)^k351^k311^k271^k231];
		setB4(v722,v711,v700,v733,k351,k311,k271,k231);
		V[v8100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8200 = zbox0[mul3(v733)^mul2(v722)^mul1(v700)^mul1(v711)^k352^k312^k272^k232];
		setB4(v733,v722,v700,v711,k352,k312,k272,k232);
		V[v8200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8300 = zbox0[mul3(v700)^mul2(v733)^mul1(v711)^mul1(v722)^k353^k313^k273^k233];
		setB4(v700,v733,v711,v722,k353,k313,k273,k233);
		V[v8300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v8010 = zbox0[mul3(v712)^mul2(v701)^mul1(v723)^mul1(v730)^k350^k270];
		setB2(v712,v701,v723,v730,k350,k270);
		V[v8010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8110 = zbox0[mul3(v723)^mul2(v712)^mul1(v701)^mul1(v730)^k351^k271];
		setB2(v723,v712,v701,v730,k351,k271);
		V[v8110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8210 = zbox0[mul3(v730)^mul2(v723)^mul1(v701)^mul1(v712)^k352^k272];
		setB2(v730,v723,v701,v712,k352,k272);
		V[v8210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8310 = zbox0[mul3(v701)^mul2(v730)^mul1(v712)^mul1(v723)^k353^k273];
		setB2(v701,v730,v712,v723,k353,k273);
		V[v8310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v8020 = zbox0[mul3(v713)^mul2(v702)^mul1(v720)^mul1(v731)^k350^k310];
		setB2(v713,v702,v720,v731,k350,k310);
		V[v8020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8120 = zbox0[mul3(v720)^mul2(v713)^mul1(v702)^mul1(v731)^k351^k311];
		setB2(v720,v713,v702,v731,k351,k311);
		V[v8120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8220 = zbox0[mul3(v731)^mul2(v720)^mul1(v702)^mul1(v713)^k352^k312];
		setB2(v731,v720,v702,v713,k352,k312);
		V[v8220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8320 = zbox0[mul3(v702)^mul2(v731)^mul1(v713)^mul1(v720)^k353^k313];
		setB2(v702,v731,v713,v720,k353,k313);
		V[v8320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v8030 = zbox0[mul3(v710)^mul2(v703)^mul1(v721)^mul1(v732)^k350];
		setB1(v710,v703,v721,v732,k350);
		V[v8030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8130 = zbox0[mul3(v721)^mul2(v710)^mul1(v703)^mul1(v732)^k351];
		setB1(v721,v710,v703,v732,k351);
		V[v8130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8230 = zbox0[mul3(v732)^mul2(v721)^mul1(v703)^mul1(v710)^k352];
		setB1(v732,v721,v703,v710,k352);
		V[v8230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v8330 = zbox0[mul3(v703)^mul2(v732)^mul1(v710)^mul1(v721)^k353];
		setB1(v703,v732,v710,v721,k353);
		V[v8330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v8337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		if (opt_split)
			splitTree(V, v8000, 8);

//fprintf(stderr,"v9000");
		// int v9000 = zbox0[mul3(v811)^mul2(v800)^mul1(v822)^mul1(v833)^k390^k350^k310^k270];
		setB4(v811,v800,v822,v833,k390,k350,k310,k270);
		V[v9000] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9001] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9002] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9003] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9004] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9005] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9006] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9007] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9100 = zbox0[mul3(v822)^mul2(v811)^mul1(v800)^mul1(v833)^k391^k351^k311^k271];
		setB4(v822,v811,v800,v833,k391,k351,k311,k271);
		V[v9100] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9101] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9102] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9103] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9104] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9105] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9106] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9107] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9200 = zbox0[mul3(v833)^mul2(v822)^mul1(v800)^mul1(v811)^k392^k352^k312^k272];
		setB4(v833,v822,v800,v811,k392,k352,k312,k272);
		V[v9200] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9201] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9202] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9203] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9204] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9205] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9206] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9207] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9300 = zbox0[mul3(v800)^mul2(v833)^mul1(v811)^mul1(v822)^k393^k353^k313^k273];
		setB4(v800,v833,v811,v822,k393,k353,k313,k273);
		V[v9300] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9301] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9302] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9303] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9304] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9305] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9306] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9307] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v9010 = zbox0[mul3(v812)^mul2(v801)^mul1(v823)^mul1(v830)^k390^k310];
		setB2(v812,v801,v823,v830,k390,k310);
		V[v9010] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9011] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9012] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9013] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9014] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9015] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9016] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9017] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9110 = zbox0[mul3(v823)^mul2(v812)^mul1(v801)^mul1(v830)^k391^k311];
		setB2(v823,v812,v801,v830,k391,k311);
		V[v9110] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9111] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9112] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9113] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9114] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9115] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9116] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9117] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9210 = zbox0[mul3(v830)^mul2(v823)^mul1(v801)^mul1(v812)^k392^k312];
		setB2(v830,v823,v801,v812,k392,k312);
		V[v9210] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9211] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9212] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9213] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9214] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9215] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9216] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9217] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9310 = zbox0[mul3(v801)^mul2(v830)^mul1(v812)^mul1(v823)^k393^k313];
		setB2(v801,v830,v812,v823,k393,k313);
		V[v9310] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9311] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9312] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9313] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9314] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9315] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9316] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9317] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v9020 = zbox0[mul3(v813)^mul2(v802)^mul1(v820)^mul1(v831)^k390^k350];
		setB2(v813,v802,v820,v831,k390,k350);
		V[v9020] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9021] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9022] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9023] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9024] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9025] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9026] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9027] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9120 = zbox0[mul3(v820)^mul2(v813)^mul1(v802)^mul1(v831)^k391^k351];
		setB2(v820,v813,v802,v831,k391,k351);
		V[v9120] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9121] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9122] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9123] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9124] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9125] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9126] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9127] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9220 = zbox0[mul3(v831)^mul2(v820)^mul1(v802)^mul1(v813)^k392^k352];
		setB2(v831,v820,v802,v813,k392,k352);
		V[v9220] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9221] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9222] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9223] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9224] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9225] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9226] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9227] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9320 = zbox0[mul3(v802)^mul2(v831)^mul1(v813)^mul1(v820)^k393^k353];
		setB2(v802,v831,v813,v820,k393,k353);
		V[v9320] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9321] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9322] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9323] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9324] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9325] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9326] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9327] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
//fprintf(stderr,".");
		// int v9030 = zbox0[mul3(v810)^mul2(v803)^mul1(v821)^mul1(v832)^k390];
		setB1(v810,v803,v821,v832,k390);
		V[v9030] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9031] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9032] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9033] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9034] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9035] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9036] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9037] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9130 = zbox0[mul3(v821)^mul2(v810)^mul1(v803)^mul1(v832)^k391];
		setB1(v821,v810,v803,v832,k391);
		V[v9130] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9131] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9132] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9133] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9134] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9135] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9136] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9137] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9230 = zbox0[mul3(v832)^mul2(v821)^mul1(v803)^mul1(v810)^k392];
		setB1(v832,v821,v803,v810,k392);
		V[v9230] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9231] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9232] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9233] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9234] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9235] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9236] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9237] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

		// int v9330 = zbox0[mul3(v803)^mul2(v832)^mul1(v810)^mul1(v821)^k393];
		setB1(v803,v832,v810,v821,k393);
		V[v9330] = ZBOX0(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9331] = ZBOX1(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9332] = ZBOX2(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9333] = ZBOX3(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9334] = ZBOX4(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9335] = ZBOX5(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9336] = ZBOX6(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);
		V[v9337] = ZBOX7(_b7,_b6,_b5,_b4,_b3,_b2,_b1,_b0);

//fprintf(stderr,"o000");

		// odata[ 0]  = ((mul1(v900)>>0)&1)^v1^k430[0]^k390[0]^k350[0]^k310[0] ? 0x01 : 0x00;
		V[o000] = V[v9000]^V[v9004]^V[v9005]^V[v9006]^V[v9007]^V[k4300]^V[k3900]^V[k3500]^V[k3100]^vTrue;
		V[o001] = V[v9000]^V[v9001]^V[v9005]^V[v9006]^V[v9007]^V[k4301]^V[k3901]^V[k3501]^V[k3101]^vTrue;
		V[o002] = V[v9000]^V[v9001]^V[v9002]^V[v9006]^V[v9007]^V[k4302]^V[k3902]^V[k3502]^V[k3102];
		V[o003] = V[v9000]^V[v9001]^V[v9002]^V[v9003]^V[v9007]^V[k4303]^V[k3903]^V[k3503]^V[k3103];
		V[o004] = V[v9000]^V[v9001]^V[v9002]^V[v9003]^V[v9004]^V[k4304]^V[k3904]^V[k3504]^V[k3104];
		V[o005] = V[v9001]^V[v9002]^V[v9003]^V[v9004]^V[v9005]^V[k4305]^V[k3905]^V[k3505]^V[k3105]^vTrue;
		V[o006] = V[v9002]^V[v9003]^V[v9004]^V[v9005]^V[v9006]^V[k4306]^V[k3906]^V[k3506]^V[k3106]^vTrue;
		V[o007] = V[v9003]^V[v9004]^V[v9005]^V[v9006]^V[v9007]^V[k4307]^V[k3907]^V[k3507]^V[k3107];

		// odata[ 1]  = ((mul1(v911)>>0)&1)^v1^k431[0]^k391[0]^k351[0]^k311[0] ? 0x01 : 0x00;
		V[o010] = V[v9110]^V[v9114]^V[v9115]^V[v9116]^V[v9117]^V[k4310]^V[k3910]^V[k3510]^V[k3110]^vTrue;
		V[o011] = V[v9110]^V[v9111]^V[v9115]^V[v9116]^V[v9117]^V[k4311]^V[k3911]^V[k3511]^V[k3111]^vTrue;
		V[o012] = V[v9110]^V[v9111]^V[v9112]^V[v9116]^V[v9117]^V[k4312]^V[k3912]^V[k3512]^V[k3112];
		V[o013] = V[v9110]^V[v9111]^V[v9112]^V[v9113]^V[v9117]^V[k4313]^V[k3913]^V[k3513]^V[k3113];
		V[o014] = V[v9110]^V[v9111]^V[v9112]^V[v9113]^V[v9114]^V[k4314]^V[k3914]^V[k3514]^V[k3114];
		V[o015] = V[v9111]^V[v9112]^V[v9113]^V[v9114]^V[v9115]^V[k4315]^V[k3915]^V[k3515]^V[k3115]^vTrue;
		V[o016] = V[v9112]^V[v9113]^V[v9114]^V[v9115]^V[v9116]^V[k4316]^V[k3916]^V[k3516]^V[k3116]^vTrue;
		V[o017] = V[v9113]^V[v9114]^V[v9115]^V[v9116]^V[v9117]^V[k4317]^V[k3917]^V[k3517]^V[k3117];

		// odata[ 2]  = ((mul1(v922)>>0)&1)^v1^k431[0]^k391[0]^k351[0]^k311[0] ? 0x01 : 0x00;
		V[o020] = V[v9220]^V[v9224]^V[v9225]^V[v9226]^V[v9227]^V[k4320]^V[k3920]^V[k3520]^V[k3120]^vTrue;
		V[o021] = V[v9220]^V[v9221]^V[v9225]^V[v9226]^V[v9227]^V[k4321]^V[k3921]^V[k3521]^V[k3121]^vTrue;
		V[o022] = V[v9220]^V[v9221]^V[v9222]^V[v9226]^V[v9227]^V[k4322]^V[k3922]^V[k3522]^V[k3122];
		V[o023] = V[v9220]^V[v9221]^V[v9222]^V[v9223]^V[v9227]^V[k4323]^V[k3923]^V[k3523]^V[k3123];
		V[o024] = V[v9220]^V[v9221]^V[v9222]^V[v9223]^V[v9224]^V[k4324]^V[k3924]^V[k3524]^V[k3124];
		V[o025] = V[v9221]^V[v9222]^V[v9223]^V[v9224]^V[v9225]^V[k4325]^V[k3925]^V[k3525]^V[k3125]^vTrue;
		V[o026] = V[v9222]^V[v9223]^V[v9224]^V[v9225]^V[v9226]^V[k4326]^V[k3926]^V[k3526]^V[k3126]^vTrue;
		V[o027] = V[v9223]^V[v9224]^V[v9225]^V[v9226]^V[v9227]^V[k4327]^V[k3927]^V[k3527]^V[k3127];

		// odata[ 3]  = ((mul1(v933)>>0)&1)^v1^k431[0]^k391[0]^k351[0]^k311[0] ? 0x01 : 0x00;
		V[o030] = V[v9330]^V[v9334]^V[v9335]^V[v9336]^V[v9337]^V[k4330]^V[k3930]^V[k3530]^V[k3130]^vTrue;
		V[o031] = V[v9330]^V[v9331]^V[v9335]^V[v9336]^V[v9337]^V[k4331]^V[k3931]^V[k3531]^V[k3131]^vTrue;
		V[o032] = V[v9330]^V[v9331]^V[v9332]^V[v9336]^V[v9337]^V[k4332]^V[k3932]^V[k3532]^V[k3132];
		V[o033] = V[v9330]^V[v9331]^V[v9332]^V[v9333]^V[v9337]^V[k4333]^V[k3933]^V[k3533]^V[k3133];
		V[o034] = V[v9330]^V[v9331]^V[v9332]^V[v9333]^V[v9334]^V[k4334]^V[k3934]^V[k3534]^V[k3134];
		V[o035] = V[v9331]^V[v9332]^V[v9333]^V[v9334]^V[v9335]^V[k4335]^V[k3935]^V[k3535]^V[k3135]^vTrue;
		V[o036] = V[v9332]^V[v9333]^V[v9334]^V[v9335]^V[v9336]^V[k4336]^V[k3936]^V[k3536]^V[k3136]^vTrue;
		V[o037] = V[v9333]^V[v9334]^V[v9335]^V[v9336]^V[v9337]^V[k4337]^V[k3937]^V[k3537]^V[k3137];
//fprintf(stderr,".");
		// odata[ 4]  = ((mul1(v901)>>0)&1)^v1^k430[0]^k350[0] ? 0x01 : 0x00;
		V[o100] = V[v9010]^V[v9014]^V[v9015]^V[v9016]^V[v9017]^V[k4300]^V[k3500]^vTrue;
		V[o101] = V[v9010]^V[v9011]^V[v9015]^V[v9016]^V[v9017]^V[k4301]^V[k3501]^vTrue;
		V[o102] = V[v9010]^V[v9011]^V[v9012]^V[v9016]^V[v9017]^V[k4302]^V[k3502];
		V[o103] = V[v9010]^V[v9011]^V[v9012]^V[v9013]^V[v9017]^V[k4303]^V[k3503];
		V[o104] = V[v9010]^V[v9011]^V[v9012]^V[v9013]^V[v9014]^V[k4304]^V[k3504];
		V[o105] = V[v9011]^V[v9012]^V[v9013]^V[v9014]^V[v9015]^V[k4305]^V[k3505]^vTrue;
		V[o106] = V[v9012]^V[v9013]^V[v9014]^V[v9015]^V[v9016]^V[k4306]^V[k3506]^vTrue;
		V[o107] = V[v9013]^V[v9014]^V[v9015]^V[v9016]^V[v9017]^V[k4307]^V[k3507];

		// odata[ 5]  = ((mul1(v912)>>0)&1)^v1^k431[0]^k351[0] ? 0x01 : 0x00;
		V[o110] = V[v9120]^V[v9124]^V[v9125]^V[v9126]^V[v9127]^V[k4310]^V[k3510]^vTrue;
		V[o111] = V[v9120]^V[v9121]^V[v9125]^V[v9126]^V[v9127]^V[k4311]^V[k3511]^vTrue;
		V[o112] = V[v9120]^V[v9121]^V[v9122]^V[v9126]^V[v9127]^V[k4312]^V[k3512];
		V[o113] = V[v9120]^V[v9121]^V[v9122]^V[v9123]^V[v9127]^V[k4313]^V[k3513];
		V[o114] = V[v9120]^V[v9121]^V[v9122]^V[v9123]^V[v9124]^V[k4314]^V[k3514];
		V[o115] = V[v9121]^V[v9122]^V[v9123]^V[v9124]^V[v9125]^V[k4315]^V[k3515]^vTrue;
		V[o116] = V[v9122]^V[v9123]^V[v9124]^V[v9125]^V[v9126]^V[k4316]^V[k3516]^vTrue;
		V[o117] = V[v9123]^V[v9124]^V[v9125]^V[v9126]^V[v9127]^V[k4317]^V[k3517];

		// odata[ 6]  = ((mul1(v923)>>1)&1)^v1^k432[1]^k352[1] ? 0x02 : 0x00;
		V[o120] = V[v9230]^V[v9234]^V[v9235]^V[v9236]^V[v9237]^V[k4320]^V[k3520]^vTrue;
		V[o121] = V[v9230]^V[v9231]^V[v9235]^V[v9236]^V[v9237]^V[k4321]^V[k3521]^vTrue;
		V[o122] = V[v9230]^V[v9231]^V[v9232]^V[v9236]^V[v9237]^V[k4322]^V[k3522];
		V[o123] = V[v9230]^V[v9231]^V[v9232]^V[v9233]^V[v9237]^V[k4323]^V[k3523];
		V[o124] = V[v9230]^V[v9231]^V[v9232]^V[v9233]^V[v9234]^V[k4324]^V[k3524];
		V[o125] = V[v9231]^V[v9232]^V[v9233]^V[v9234]^V[v9235]^V[k4325]^V[k3525]^vTrue;
		V[o126] = V[v9232]^V[v9233]^V[v9234]^V[v9235]^V[v9236]^V[k4326]^V[k3526]^vTrue;
		V[o127] = V[v9233]^V[v9234]^V[v9235]^V[v9236]^V[v9237]^V[k4327]^V[k3527];

		// odata[ 7]  = ((mul1(v930)>>0)&1)^v1^k433[0]^k353[0] ? 0x01 : 0x00;
		V[o130] = V[v9300]^V[v9304]^V[v9305]^V[v9306]^V[v9307]^V[k4330]^V[k3530]^vTrue;
		V[o131] = V[v9300]^V[v9301]^V[v9305]^V[v9306]^V[v9307]^V[k4331]^V[k3531]^vTrue;
		V[o132] = V[v9300]^V[v9301]^V[v9302]^V[v9306]^V[v9307]^V[k4332]^V[k3532];
		V[o133] = V[v9300]^V[v9301]^V[v9302]^V[v9303]^V[v9307]^V[k4333]^V[k3533];
		V[o134] = V[v9300]^V[v9301]^V[v9302]^V[v9303]^V[v9304]^V[k4334]^V[k3534];
		V[o135] = V[v9301]^V[v9302]^V[v9303]^V[v9304]^V[v9305]^V[k4335]^V[k3535]^vTrue;
		V[o136] = V[v9302]^V[v9303]^V[v9304]^V[v9305]^V[v9306]^V[k4336]^V[k3536]^vTrue;
		V[o137] = V[v9303]^V[v9304]^V[v9305]^V[v9306]^V[v9307]^V[k4337]^V[k3537];
//fprintf(stderr,".");
		// odata[ 8]  = ((mul1(v902)>>0)&1)^v1^k430[0]^k390[0] ? 0x01 : 0x00;
		V[o200] = V[v9020]^V[v9024]^V[v9025]^V[v9026]^V[v9027]^V[k4300]^V[k3900]^vTrue;
		V[o201] = V[v9020]^V[v9021]^V[v9025]^V[v9026]^V[v9027]^V[k4301]^V[k3901]^vTrue;
		V[o202] = V[v9020]^V[v9021]^V[v9022]^V[v9026]^V[v9027]^V[k4302]^V[k3902];
		V[o203] = V[v9020]^V[v9021]^V[v9022]^V[v9023]^V[v9027]^V[k4303]^V[k3903];
		V[o204] = V[v9020]^V[v9021]^V[v9022]^V[v9023]^V[v9024]^V[k4304]^V[k3904];
		V[o205] = V[v9021]^V[v9022]^V[v9023]^V[v9024]^V[v9025]^V[k4305]^V[k3905]^vTrue;
		V[o206] = V[v9022]^V[v9023]^V[v9024]^V[v9025]^V[v9026]^V[k4306]^V[k3906]^vTrue;
		V[o207] = V[v9023]^V[v9024]^V[v9025]^V[v9026]^V[v9027]^V[k4307]^V[k3907];

		// odata[ 9]  = ((mul1(v913)>>0)&1)^v1^k431[0]^k391[0] ? 0x01 : 0x00;
		V[o210] = V[v9130]^V[v9134]^V[v9135]^V[v9136]^V[v9137]^V[k4310]^V[k3910]^vTrue;
		V[o211] = V[v9130]^V[v9131]^V[v9135]^V[v9136]^V[v9137]^V[k4311]^V[k3911]^vTrue;
		V[o212] = V[v9130]^V[v9131]^V[v9132]^V[v9136]^V[v9137]^V[k4312]^V[k3912];
		V[o213] = V[v9130]^V[v9131]^V[v9132]^V[v9133]^V[v9137]^V[k4313]^V[k3913];
		V[o214] = V[v9130]^V[v9131]^V[v9132]^V[v9133]^V[v9134]^V[k4314]^V[k3914];
		V[o215] = V[v9131]^V[v9132]^V[v9133]^V[v9134]^V[v9135]^V[k4315]^V[k3915]^vTrue;
		V[o216] = V[v9132]^V[v9133]^V[v9134]^V[v9135]^V[v9136]^V[k4316]^V[k3916]^vTrue;
		V[o217] = V[v9133]^V[v9134]^V[v9135]^V[v9136]^V[v9137]^V[k4317]^V[k3917];

		// odata[10]  = ((mul1(v920)>>0)&1)^v1^k432[0]^k392[0] ? 0x01 : 0x00;
		V[o220] = V[v9200]^V[v9204]^V[v9205]^V[v9206]^V[v9207]^V[k4320]^V[k3920]^vTrue;
		V[o221] = V[v9200]^V[v9201]^V[v9205]^V[v9206]^V[v9207]^V[k4321]^V[k3921]^vTrue;
		V[o222] = V[v9200]^V[v9201]^V[v9202]^V[v9206]^V[v9207]^V[k4322]^V[k3922];
		V[o223] = V[v9200]^V[v9201]^V[v9202]^V[v9203]^V[v9207]^V[k4323]^V[k3923];
		V[o224] = V[v9200]^V[v9201]^V[v9202]^V[v9203]^V[v9204]^V[k4324]^V[k3924];
		V[o225] = V[v9201]^V[v9202]^V[v9203]^V[v9204]^V[v9205]^V[k4325]^V[k3925]^vTrue;
		V[o226] = V[v9202]^V[v9203]^V[v9204]^V[v9205]^V[v9206]^V[k4326]^V[k3926]^vTrue;
		V[o227] = V[v9203]^V[v9204]^V[v9205]^V[v9206]^V[v9207]^V[k4327]^V[k3927];

		// odata[11]  = ((mul1(v931)>>0)&1)^v1^k433[0]^k393[0] ? 0x01 : 0x00;
		V[o230] = V[v9310]^V[v9314]^V[v9315]^V[v9316]^V[v9317]^V[k4330]^V[k3930]^vTrue;
		V[o231] = V[v9310]^V[v9311]^V[v9315]^V[v9316]^V[v9317]^V[k4331]^V[k3931]^vTrue;
		V[o232] = V[v9310]^V[v9311]^V[v9312]^V[v9316]^V[v9317]^V[k4332]^V[k3932];
		V[o233] = V[v9310]^V[v9311]^V[v9312]^V[v9313]^V[v9317]^V[k4333]^V[k3933];
		V[o234] = V[v9310]^V[v9311]^V[v9312]^V[v9313]^V[v9314]^V[k4334]^V[k3934];
		V[o235] = V[v9311]^V[v9312]^V[v9313]^V[v9314]^V[v9315]^V[k4335]^V[k3935]^vTrue;
		V[o236] = V[v9312]^V[v9313]^V[v9314]^V[v9315]^V[v9316]^V[k4336]^V[k3936]^vTrue;
		V[o237] = V[v9313]^V[v9314]^V[v9315]^V[v9316]^V[v9317]^V[k4337]^V[k3937];
//fprintf(stderr,".");
		// odata[12]  = ((mul1(v903)>>0)&1)^v1^k430[0] ? 0x01 : 0x00;
		V[o300] = V[v9030]^V[v9034]^V[v9035]^V[v9036]^V[v9037]^V[k4300]^vTrue;
		V[o301] = V[v9030]^V[v9031]^V[v9035]^V[v9036]^V[v9037]^V[k4301]^vTrue;
		V[o302] = V[v9030]^V[v9031]^V[v9032]^V[v9036]^V[v9037]^V[k4302];
		V[o303] = V[v9030]^V[v9031]^V[v9032]^V[v9033]^V[v9037]^V[k4303];
		V[o304] = V[v9030]^V[v9031]^V[v9032]^V[v9033]^V[v9034]^V[k4304];
		V[o305] = V[v9031]^V[v9032]^V[v9033]^V[v9034]^V[v9035]^V[k4305]^vTrue;
		V[o306] = V[v9032]^V[v9033]^V[v9034]^V[v9035]^V[v9036]^V[k4306]^vTrue;
		V[o307] = V[v9033]^V[v9034]^V[v9035]^V[v9036]^V[v9037]^V[k4307];

		// odata[13]  = ((mul1(v910)>>0)&1)^v1^k431[0] ? 0x01 : 0x00;
		V[o310] = V[v9100]^V[v9104]^V[v9105]^V[v9106]^V[v9107]^V[k4310]^vTrue;
		V[o311] = V[v9100]^V[v9101]^V[v9105]^V[v9106]^V[v9107]^V[k4311]^vTrue;
		V[o312] = V[v9100]^V[v9101]^V[v9102]^V[v9106]^V[v9107]^V[k4312];
		V[o313] = V[v9100]^V[v9101]^V[v9102]^V[v9103]^V[v9107]^V[k4313];
		V[o314] = V[v9100]^V[v9101]^V[v9102]^V[v9103]^V[v9104]^V[k4314];
		V[o315] = V[v9101]^V[v9102]^V[v9103]^V[v9104]^V[v9105]^V[k4315]^vTrue;
		V[o316] = V[v9102]^V[v9103]^V[v9104]^V[v9105]^V[v9106]^V[k4316]^vTrue;
		V[o317] = V[v9103]^V[v9104]^V[v9105]^V[v9106]^V[v9107]^V[k4317];

		// odata[14]  = ((mul1(v921)>>0)&1)^v1^k432[0] ? 0x01 : 0x00;
		V[o320] = V[v9210]^V[v9214]^V[v9215]^V[v9216]^V[v9217]^V[k4320]^vTrue;
		V[o321] = V[v9210]^V[v9211]^V[v9215]^V[v9216]^V[v9217]^V[k4321]^vTrue;
		V[o322] = V[v9210]^V[v9211]^V[v9212]^V[v9216]^V[v9217]^V[k4322];
		V[o323] = V[v9210]^V[v9211]^V[v9212]^V[v9213]^V[v9217]^V[k4323];
		V[o324] = V[v9210]^V[v9211]^V[v9212]^V[v9213]^V[v9214]^V[k4324];
		V[o325] = V[v9211]^V[v9212]^V[v9213]^V[v9214]^V[v9215]^V[k4325]^vTrue;
		V[o326] = V[v9212]^V[v9213]^V[v9214]^V[v9215]^V[v9216]^V[k4326]^vTrue;
		V[o327] = V[v9213]^V[v9214]^V[v9215]^V[v9216]^V[v9217]^V[k4327];

		// odata[15]  = ((mul1(v932)>>0)&1)^v1^k433[0] ? 0x01 : 0x00;
		V[o330] = V[v9320]^V[v9324]^V[v9325]^V[v9326]^V[v9327]^V[k4330]^vTrue;
		V[o331] = V[v9320]^V[v9321]^V[v9325]^V[v9326]^V[v9327]^V[k4331]^vTrue;
		V[o332] = V[v9320]^V[v9321]^V[v9322]^V[v9326]^V[v9327]^V[k4332];
		V[o333] = V[v9320]^V[v9321]^V[v9322]^V[v9323]^V[v9327]^V[k4333];
		V[o334] = V[v9320]^V[v9321]^V[v9322]^V[v9323]^V[v9324]^V[k4334];
		V[o335] = V[v9321]^V[v9322]^V[v9323]^V[v9324]^V[v9325]^V[k4335]^vTrue;
		V[o336] = V[v9322]^V[v9323]^V[v9324]^V[v9325]^V[v9326]^V[k4336]^vTrue;
		V[o337] = V[v9323]^V[v9324]^V[v9325]^V[v9326]^V[v9327]^V[k4337];
//fprintf(stderr,"\v");
		//@formatter:ov

		// setup root vames
		assert(gTree->numRoots == VSTART - OSTART);
		for (uint32_t iRoot = 0; iRoot < gTree->numRoots; iRoot++)
			gTree->rootNames[iRoot] = allNames[OSTART + iRoot];

	}
	
	void main(void) {
		/*
		 * Allocate the build tree containing the complete formula
		 */

		gTree = new baseTree_t(*this, KSTART, NSTART, VSTART - OSTART/*numRoots*/, opt_maxnode, opt_flags);

		// setup base key names
		for (unsigned i = 0; i < gTree->nstart; i++)
			gTree->keyNames[i] = allNames[i];

		// assign initial chain id
		gTree->rootsId = rand();

		// allocate and initialise placeholder/helper array
		// references to variables
		NODE *V = (NODE *) malloc(VLAST * sizeof V[0]);

		// set initial keys
		for (uint32_t iKey = 0; iKey < gTree->nstart; iKey++) {
			// key variable
			V[iKey].id = iKey;

			// key node
			gTree->N[iKey].Q = 0;
			gTree->N[iKey].T = 0;
			gTree->N[iKey].F = iKey;
		}

		// any de-reference of locations before `kstart` is considered triggering of undefined behaviour.
		// this could be intentional.
		for (uint32_t iKey = gTree->nstart; iKey < VLAST; iKey++)
			V[iKey].id = iKey; // mark as uninitialized

		// build. Uses gBuild
		build(V);

		/*
		 * Assign the roots/entrypoints.
		 */
		gTree->numRoots = VSTART - OSTART;
		for (unsigned i = OSTART; i < VSTART; i++)
			gTree->roots[i - OSTART] = V[i].id;

		/*
		 * Create tests as json object
		 */

		gTests = json_array();
		validateAll();

		/*
		 * Save the tree
		 */

		if (opt_split) {
			char *filename;
			asprintf(&filename, arg_data, 10);
			gTree->saveFile(filename);
			free(filename);
		} else {
			gTree->saveFile(arg_data);
		}

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
 * @global {buildaesContext_t} Application context
 */
buildaesContext_t app;

void usage(char *const *argv, bool verbose) {
	fprintf(stderr, "usage: %s <json> <data>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t   --maxnode=<number> [default=%d]\n", app.opt_maxnode);
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t   --split\n");
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
 * @date 2021-05-15 19:06:46
 *
 * Program main entry point
 * Process all user supplied arguments to construct a application context.
 * Activate application context.
 *
 * @param  {number} argc - number of arguments
 * @param  {string[]} argv - program arguments
 * @return {number} 0 on normal return, non-zero when attention is required
 */
int main(int argc, char * const * argv) {
	setlinebuf(stdout);

	for (;;) {
		int option_index = 0;
		enum {
			LO_HELP  = 1, LO_DEBUG, LO_TIMER, LO_FORCE, LO_MAXNODE, LO_SPLIT,
			LO_PARANOID, LO_NOPARANOID, LO_PURE, LO_NOPURE, LO_REWRITE, LO_NOREWRITE, LO_CASCADE, LO_NOCASCADE, LO_SHRINK, LO_NOSHRINK, LO_PIVOT3, LO_NOPIVOT3,
			LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",       1, 0, LO_DEBUG},
			{"force",       0, 0, LO_FORCE},
			{"help",       0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"quiet",      2, 0, LO_QUIET},
			{"split",       0, 0, LO_SPLIT},
			{"timer",       1, 0, LO_TIMER},
			{"verbose",    2, 0, LO_VERBOSE},
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
			{NULL,         0, 0, 0}
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

		*cp        = '\0';

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
			case LO_SPLIT:
				app.opt_split++;
			break;
		case LO_TIMER:
				app.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
			case LO_VERBOSE:
				app.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : app.opt_verbose + 1;
			break;
				//
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
