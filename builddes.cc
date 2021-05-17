#pragma GCC optimize ("O0") // optimize on demand

/*
 * builddes.cc
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
#include "builddes.h"

/// @var {baseTree_t*} global reference to tree
baseTree_t *gTree = NULL;
/// @var {json_t*} validation tests
json_t     *gTests; // validation tests

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
 * Include generated tests
 */
#include "validatedes.h"

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
#include "builddesbox.h"

/**
 * @date 2021-05-10 13:23:43
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct builddesContext_t : context_t {

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

	builddesContext_t() {
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
	 * Build des expression
	 * Ints are replaced by node_t wrappers in vectors.

	 * NOTE: disable optimisations or wait a day
	 */
	void __attribute__((optimize("O0"))) build(NODE *V) {
		//@formatter:off
		V[v0009] = V[i74] ^ box_0_9 (V[i37]^V[k27],V[i47]^V[k67],V[i57]^V[k74],V[i67]^V[k46],V[i77]^V[k65],V[i01]^V[k16]);
		V[v0017] = V[i72] ^ box_0_17(V[i37]^V[k27],V[i47]^V[k67],V[i57]^V[k74],V[i67]^V[k46],V[i77]^V[k65],V[i01]^V[k16]);
		V[v0023] = V[i12] ^ box_0_23(V[i37]^V[k27],V[i47]^V[k67],V[i57]^V[k74],V[i67]^V[k46],V[i77]^V[k65],V[i01]^V[k16]);
		V[v0031] = V[i10] ^ box_0_31(V[i37]^V[k27],V[i47]^V[k67],V[i57]^V[k74],V[i67]^V[k46],V[i77]^V[k65],V[i01]^V[k16]);
		V[v0006] = V[i26] ^ box_2_6 (V[i35]^V[k76],V[i45]^V[k54],V[i55]^V[k37],V[i65]^V[k36],V[i75]^V[k45],V[i07]^V[k05]);
		V[v0016] = V[i04] ^ box_2_16(V[i35]^V[k76],V[i45]^V[k54],V[i55]^V[k37],V[i65]^V[k36],V[i75]^V[k45],V[i07]^V[k05]);
		V[v0024] = V[i02] ^ box_2_24(V[i35]^V[k76],V[i45]^V[k54],V[i55]^V[k37],V[i65]^V[k36],V[i75]^V[k45],V[i07]^V[k05]);
		V[v0030] = V[i20] ^ box_2_30(V[i35]^V[k76],V[i45]^V[k54],V[i55]^V[k37],V[i65]^V[k36],V[i75]^V[k45],V[i07]^V[k05]);
		V[v0003] = V[i56] ^ box_4_3 (V[i33]^V[k04],V[i43]^V[k43],V[i53]^V[k62],V[i63]^V[k41],V[i73]^V[k34],V[i05]^V[k22]);
		V[v0008] = V[i06] ^ box_4_8 (V[i33]^V[k04],V[i43]^V[k43],V[i53]^V[k62],V[i63]^V[k41],V[i73]^V[k34],V[i05]^V[k22]);
		V[v0014] = V[i24] ^ box_4_14(V[i33]^V[k04],V[i43]^V[k43],V[i53]^V[k62],V[i63]^V[k41],V[i73]^V[k34],V[i05]^V[k22]);
		V[v0025] = V[i70] ^ box_4_25(V[i33]^V[k04],V[i43]^V[k43],V[i53]^V[k62],V[i63]^V[k41],V[i73]^V[k34],V[i05]^V[k22]);
		V[v0000] = V[i00] ^ box_6_0 (V[i31]^V[k24],V[i41]^V[k11],V[i51]^V[k71],V[i61]^V[k42],V[i71]^V[k23],V[i03]^V[k73]);
		V[v0007] = V[i16] ^ box_6_7 (V[i31]^V[k24],V[i41]^V[k11],V[i51]^V[k71],V[i61]^V[k42],V[i71]^V[k23],V[i03]^V[k73]);
		V[v0012] = V[i44] ^ box_6_12(V[i31]^V[k24],V[i41]^V[k11],V[i51]^V[k71],V[i61]^V[k42],V[i71]^V[k23],V[i03]^V[k73]);
		V[v0022] = V[i22] ^ box_6_22(V[i31]^V[k24],V[i41]^V[k11],V[i51]^V[k71],V[i61]^V[k42],V[i71]^V[k23],V[i03]^V[k73]);
		V[v0002] = V[i66] ^ box_1_2 (V[i75]^V[k56],V[i07]^V[k25],V[i17]^V[k17],V[i27]^V[k06],V[i37]^V[k77],V[i47]^V[k47]);
		V[v0013] = V[i34] ^ box_1_13(V[i75]^V[k56],V[i07]^V[k25],V[i17]^V[k17],V[i27]^V[k06],V[i37]^V[k77],V[i47]^V[k47]);
		V[v0018] = V[i62] ^ box_1_18(V[i75]^V[k56],V[i07]^V[k25],V[i17]^V[k17],V[i27]^V[k06],V[i37]^V[k77],V[i47]^V[k47]);
		V[v0028] = V[i40] ^ box_1_28(V[i75]^V[k56],V[i07]^V[k25],V[i17]^V[k17],V[i27]^V[k06],V[i37]^V[k77],V[i47]^V[k47]);
		V[v0001] = V[i76] ^ box_3_1 (V[i73]^V[k57],V[i05]^V[k26],V[i15]^V[k35],V[i25]^V[k44],V[i35]^V[k07],V[i45]^V[k75]);
		V[v0010] = V[i64] ^ box_3_10(V[i73]^V[k57],V[i05]^V[k26],V[i15]^V[k35],V[i25]^V[k44],V[i35]^V[k07],V[i45]^V[k75]);
		V[v0020] = V[i42] ^ box_3_20(V[i73]^V[k57],V[i05]^V[k26],V[i15]^V[k35],V[i25]^V[k44],V[i35]^V[k07],V[i45]^V[k75]);
		V[v0026] = V[i60] ^ box_3_26(V[i73]^V[k57],V[i05]^V[k26],V[i15]^V[k35],V[i25]^V[k44],V[i35]^V[k07],V[i45]^V[k75]);
		V[v0004] = V[i46] ^ box_5_4 (V[i71]^V[k33],V[i03]^V[k21],V[i13]^V[k63],V[i23]^V[k03],V[i33]^V[k32],V[i43]^V[k51]);
		V[v0011] = V[i54] ^ box_5_11(V[i71]^V[k33],V[i03]^V[k21],V[i13]^V[k63],V[i23]^V[k03],V[i33]^V[k32],V[i43]^V[k51]);
		V[v0019] = V[i52] ^ box_5_19(V[i71]^V[k33],V[i03]^V[k21],V[i13]^V[k63],V[i23]^V[k03],V[i33]^V[k32],V[i43]^V[k51]);
		V[v0029] = V[i30] ^ box_5_29(V[i71]^V[k33],V[i03]^V[k21],V[i13]^V[k63],V[i23]^V[k03],V[i33]^V[k32],V[i43]^V[k51]);
		V[v0005] = V[i36] ^ box_7_5 (V[i77]^V[k31],V[i01]^V[k61],V[i11]^V[k72],V[i21]^V[k13],V[i31]^V[k12],V[i41]^V[k53]);
		V[v0015] = V[i14] ^ box_7_15(V[i77]^V[k31],V[i01]^V[k61],V[i11]^V[k72],V[i21]^V[k13],V[i31]^V[k12],V[i41]^V[k53]);
		V[v0021] = V[i32] ^ box_7_21(V[i77]^V[k31],V[i01]^V[k61],V[i11]^V[k72],V[i21]^V[k13],V[i31]^V[k12],V[i41]^V[k53]);
		V[v0027] = V[i50] ^ box_7_27(V[i77]^V[k31],V[i01]^V[k61],V[i11]^V[k72],V[i21]^V[k13],V[i31]^V[k12],V[i41]^V[k53]);

		if (opt_split)
			splitTree(V, v0000, 0);

		V[v0109] = V[i75] ^ box_0_9 (V[v0005]^V[k17],V[v0004]^V[k57],V[v0003]^V[k64],V[v0002]^V[k36],V[v0001]^V[k55],V[v0000]^V[k06]);
		V[v0117] = V[i73] ^ box_0_17(V[v0005]^V[k17],V[v0004]^V[k57],V[v0003]^V[k64],V[v0002]^V[k36],V[v0001]^V[k55],V[v0000]^V[k06]);
		V[v0123] = V[i13] ^ box_0_23(V[v0005]^V[k17],V[v0004]^V[k57],V[v0003]^V[k64],V[v0002]^V[k36],V[v0001]^V[k55],V[v0000]^V[k06]);
		V[v0131] = V[i11] ^ box_0_31(V[v0005]^V[k17],V[v0004]^V[k57],V[v0003]^V[k64],V[v0002]^V[k36],V[v0001]^V[k55],V[v0000]^V[k06]);
		V[v0106] = V[i27] ^ box_2_6 (V[v0013]^V[k66],V[v0012]^V[k44],V[v0011]^V[k27],V[v0010]^V[k26],V[v0009]^V[k35],V[v0008]^V[k74]);
		V[v0116] = V[i05] ^ box_2_16(V[v0013]^V[k66],V[v0012]^V[k44],V[v0011]^V[k27],V[v0010]^V[k26],V[v0009]^V[k35],V[v0008]^V[k74]);
		V[v0124] = V[i03] ^ box_2_24(V[v0013]^V[k66],V[v0012]^V[k44],V[v0011]^V[k27],V[v0010]^V[k26],V[v0009]^V[k35],V[v0008]^V[k74]);
		V[v0130] = V[i21] ^ box_2_30(V[v0013]^V[k66],V[v0012]^V[k44],V[v0011]^V[k27],V[v0010]^V[k26],V[v0009]^V[k35],V[v0008]^V[k74]);
		V[v0103] = V[i57] ^ box_4_3 (V[v0021]^V[k71],V[v0020]^V[k33],V[v0019]^V[k52],V[v0018]^V[k31],V[v0017]^V[k24],V[v0016]^V[k12]);
		V[v0108] = V[i07] ^ box_4_8 (V[v0021]^V[k71],V[v0020]^V[k33],V[v0019]^V[k52],V[v0018]^V[k31],V[v0017]^V[k24],V[v0016]^V[k12]);
		V[v0114] = V[i25] ^ box_4_14(V[v0021]^V[k71],V[v0020]^V[k33],V[v0019]^V[k52],V[v0018]^V[k31],V[v0017]^V[k24],V[v0016]^V[k12]);
		V[v0125] = V[i71] ^ box_4_25(V[v0021]^V[k71],V[v0020]^V[k33],V[v0019]^V[k52],V[v0018]^V[k31],V[v0017]^V[k24],V[v0016]^V[k12]);
		V[v0100] = V[i01] ^ box_6_0 (V[v0029]^V[k14],V[v0028]^V[k01],V[v0027]^V[k61],V[v0026]^V[k32],V[v0025]^V[k13],V[v0024]^V[k63]);
		V[v0107] = V[i17] ^ box_6_7 (V[v0029]^V[k14],V[v0028]^V[k01],V[v0027]^V[k61],V[v0026]^V[k32],V[v0025]^V[k13],V[v0024]^V[k63]);
		V[v0112] = V[i45] ^ box_6_12(V[v0029]^V[k14],V[v0028]^V[k01],V[v0027]^V[k61],V[v0026]^V[k32],V[v0025]^V[k13],V[v0024]^V[k63]);
		V[v0122] = V[i23] ^ box_6_22(V[v0029]^V[k14],V[v0028]^V[k01],V[v0027]^V[k61],V[v0026]^V[k32],V[v0025]^V[k13],V[v0024]^V[k63]);
		V[v0102] = V[i67] ^ box_1_2 (V[v0009]^V[k46],V[v0008]^V[k15],V[v0007]^V[k07],V[v0006]^V[k75],V[v0005]^V[k67],V[v0004]^V[k37]);
		V[v0113] = V[i35] ^ box_1_13(V[v0009]^V[k46],V[v0008]^V[k15],V[v0007]^V[k07],V[v0006]^V[k75],V[v0005]^V[k67],V[v0004]^V[k37]);
		V[v0118] = V[i63] ^ box_1_18(V[v0009]^V[k46],V[v0008]^V[k15],V[v0007]^V[k07],V[v0006]^V[k75],V[v0005]^V[k67],V[v0004]^V[k37]);
		V[v0128] = V[i41] ^ box_1_28(V[v0009]^V[k46],V[v0008]^V[k15],V[v0007]^V[k07],V[v0006]^V[k75],V[v0005]^V[k67],V[v0004]^V[k37]);
		V[v0101] = V[i77] ^ box_3_1 (V[v0017]^V[k47],V[v0016]^V[k16],V[v0015]^V[k25],V[v0014]^V[k77],V[v0013]^V[k76],V[v0012]^V[k65]);
		V[v0110] = V[i65] ^ box_3_10(V[v0017]^V[k47],V[v0016]^V[k16],V[v0015]^V[k25],V[v0014]^V[k77],V[v0013]^V[k76],V[v0012]^V[k65]);
		V[v0120] = V[i43] ^ box_3_20(V[v0017]^V[k47],V[v0016]^V[k16],V[v0015]^V[k25],V[v0014]^V[k77],V[v0013]^V[k76],V[v0012]^V[k65]);
		V[v0126] = V[i61] ^ box_3_26(V[v0017]^V[k47],V[v0016]^V[k16],V[v0015]^V[k25],V[v0014]^V[k77],V[v0013]^V[k76],V[v0012]^V[k65]);
		V[v0104] = V[i47] ^ box_5_4 (V[v0025]^V[k23],V[v0024]^V[k11],V[v0023]^V[k53],V[v0022]^V[k34],V[v0021]^V[k22],V[v0020]^V[k41]);
		V[v0111] = V[i55] ^ box_5_11(V[v0025]^V[k23],V[v0024]^V[k11],V[v0023]^V[k53],V[v0022]^V[k34],V[v0021]^V[k22],V[v0020]^V[k41]);
		V[v0119] = V[i53] ^ box_5_19(V[v0025]^V[k23],V[v0024]^V[k11],V[v0023]^V[k53],V[v0022]^V[k34],V[v0021]^V[k22],V[v0020]^V[k41]);
		V[v0129] = V[i31] ^ box_5_29(V[v0025]^V[k23],V[v0024]^V[k11],V[v0023]^V[k53],V[v0022]^V[k34],V[v0021]^V[k22],V[v0020]^V[k41]);
		V[v0105] = V[i37] ^ box_7_5 (V[v0001]^V[k21],V[v0000]^V[k51],V[v0031]^V[k62],V[v0030]^V[k03],V[v0029]^V[k02],V[v0028]^V[k43]);
		V[v0115] = V[i15] ^ box_7_15(V[v0001]^V[k21],V[v0000]^V[k51],V[v0031]^V[k62],V[v0030]^V[k03],V[v0029]^V[k02],V[v0028]^V[k43]);
		V[v0121] = V[i33] ^ box_7_21(V[v0001]^V[k21],V[v0000]^V[k51],V[v0031]^V[k62],V[v0030]^V[k03],V[v0029]^V[k02],V[v0028]^V[k43]);
		V[v0127] = V[i51] ^ box_7_27(V[v0001]^V[k21],V[v0000]^V[k51],V[v0031]^V[k62],V[v0030]^V[k03],V[v0029]^V[k02],V[v0028]^V[k43]);

		if (opt_split)
			splitTree(V, v0100, 1);

		V[v0209] = V[v0009] ^ box_0_9 (V[v0105]^V[k76],V[v0104]^V[k37],V[v0103]^V[k44],V[v0102]^V[k16],V[v0101]^V[k35],V[v0100]^V[k65]);
		V[v0217] = V[v0017] ^ box_0_17(V[v0105]^V[k76],V[v0104]^V[k37],V[v0103]^V[k44],V[v0102]^V[k16],V[v0101]^V[k35],V[v0100]^V[k65]);
		V[v0223] = V[v0023] ^ box_0_23(V[v0105]^V[k76],V[v0104]^V[k37],V[v0103]^V[k44],V[v0102]^V[k16],V[v0101]^V[k35],V[v0100]^V[k65]);
		V[v0231] = V[v0031] ^ box_0_31(V[v0105]^V[k76],V[v0104]^V[k37],V[v0103]^V[k44],V[v0102]^V[k16],V[v0101]^V[k35],V[v0100]^V[k65]);
		V[v0206] = V[v0006] ^ box_2_6 (V[v0113]^V[k46],V[v0112]^V[k67],V[v0111]^V[k07],V[v0110]^V[k06],V[v0109]^V[k15],V[v0108]^V[k54]);
		V[v0216] = V[v0016] ^ box_2_16(V[v0113]^V[k46],V[v0112]^V[k67],V[v0111]^V[k07],V[v0110]^V[k06],V[v0109]^V[k15],V[v0108]^V[k54]);
		V[v0224] = V[v0024] ^ box_2_24(V[v0113]^V[k46],V[v0112]^V[k67],V[v0111]^V[k07],V[v0110]^V[k06],V[v0109]^V[k15],V[v0108]^V[k54]);
		V[v0230] = V[v0030] ^ box_2_30(V[v0113]^V[k46],V[v0112]^V[k67],V[v0111]^V[k07],V[v0110]^V[k06],V[v0109]^V[k15],V[v0108]^V[k54]);
		V[v0203] = V[v0003] ^ box_4_3 (V[v0121]^V[k51],V[v0120]^V[k13],V[v0119]^V[k32],V[v0118]^V[k11],V[v0117]^V[k04],V[v0116]^V[k73]);
		V[v0208] = V[v0008] ^ box_4_8 (V[v0121]^V[k51],V[v0120]^V[k13],V[v0119]^V[k32],V[v0118]^V[k11],V[v0117]^V[k04],V[v0116]^V[k73]);
		V[v0214] = V[v0014] ^ box_4_14(V[v0121]^V[k51],V[v0120]^V[k13],V[v0119]^V[k32],V[v0118]^V[k11],V[v0117]^V[k04],V[v0116]^V[k73]);
		V[v0225] = V[v0025] ^ box_4_25(V[v0121]^V[k51],V[v0120]^V[k13],V[v0119]^V[k32],V[v0118]^V[k11],V[v0117]^V[k04],V[v0116]^V[k73]);
		V[v0200] = V[v0000] ^ box_6_0 (V[v0129]^V[k71],V[v0128]^V[k62],V[v0127]^V[k41],V[v0126]^V[k12],V[v0125]^V[k34],V[v0124]^V[k43]);
		V[v0207] = V[v0007] ^ box_6_7 (V[v0129]^V[k71],V[v0128]^V[k62],V[v0127]^V[k41],V[v0126]^V[k12],V[v0125]^V[k34],V[v0124]^V[k43]);
		V[v0212] = V[v0012] ^ box_6_12(V[v0129]^V[k71],V[v0128]^V[k62],V[v0127]^V[k41],V[v0126]^V[k12],V[v0125]^V[k34],V[v0124]^V[k43]);
		V[v0222] = V[v0022] ^ box_6_22(V[v0129]^V[k71],V[v0128]^V[k62],V[v0127]^V[k41],V[v0126]^V[k12],V[v0125]^V[k34],V[v0124]^V[k43]);
		V[v0202] = V[v0002] ^ box_1_2 (V[v0109]^V[k26],V[v0108]^V[k74],V[v0107]^V[k66],V[v0106]^V[k55],V[v0105]^V[k47],V[v0104]^V[k17]);
		V[v0213] = V[v0013] ^ box_1_13(V[v0109]^V[k26],V[v0108]^V[k74],V[v0107]^V[k66],V[v0106]^V[k55],V[v0105]^V[k47],V[v0104]^V[k17]);
		V[v0218] = V[v0018] ^ box_1_18(V[v0109]^V[k26],V[v0108]^V[k74],V[v0107]^V[k66],V[v0106]^V[k55],V[v0105]^V[k47],V[v0104]^V[k17]);
		V[v0228] = V[v0028] ^ box_1_28(V[v0109]^V[k26],V[v0108]^V[k74],V[v0107]^V[k66],V[v0106]^V[k55],V[v0105]^V[k47],V[v0104]^V[k17]);
		V[v0201] = V[v0001] ^ box_3_1 (V[v0117]^V[k27],V[v0116]^V[k75],V[v0115]^V[k05],V[v0114]^V[k57],V[v0113]^V[k56],V[v0112]^V[k45]);
		V[v0210] = V[v0010] ^ box_3_10(V[v0117]^V[k27],V[v0116]^V[k75],V[v0115]^V[k05],V[v0114]^V[k57],V[v0113]^V[k56],V[v0112]^V[k45]);
		V[v0220] = V[v0020] ^ box_3_20(V[v0117]^V[k27],V[v0116]^V[k75],V[v0115]^V[k05],V[v0114]^V[k57],V[v0113]^V[k56],V[v0112]^V[k45]);
		V[v0226] = V[v0026] ^ box_3_26(V[v0117]^V[k27],V[v0116]^V[k75],V[v0115]^V[k05],V[v0114]^V[k57],V[v0113]^V[k56],V[v0112]^V[k45]);
		V[v0204] = V[v0004] ^ box_5_4 (V[v0125]^V[k03],V[v0124]^V[k72],V[v0123]^V[k33],V[v0122]^V[k14],V[v0121]^V[k02],V[v0120]^V[k21]);
		V[v0211] = V[v0011] ^ box_5_11(V[v0125]^V[k03],V[v0124]^V[k72],V[v0123]^V[k33],V[v0122]^V[k14],V[v0121]^V[k02],V[v0120]^V[k21]);
		V[v0219] = V[v0019] ^ box_5_19(V[v0125]^V[k03],V[v0124]^V[k72],V[v0123]^V[k33],V[v0122]^V[k14],V[v0121]^V[k02],V[v0120]^V[k21]);
		V[v0229] = V[v0029] ^ box_5_29(V[v0125]^V[k03],V[v0124]^V[k72],V[v0123]^V[k33],V[v0122]^V[k14],V[v0121]^V[k02],V[v0120]^V[k21]);
		V[v0205] = V[v0005] ^ box_7_5 (V[v0101]^V[k01],V[v0100]^V[k31],V[v0131]^V[k42],V[v0130]^V[k24],V[v0129]^V[k63],V[v0128]^V[k23]);
		V[v0215] = V[v0015] ^ box_7_15(V[v0101]^V[k01],V[v0100]^V[k31],V[v0131]^V[k42],V[v0130]^V[k24],V[v0129]^V[k63],V[v0128]^V[k23]);
		V[v0221] = V[v0021] ^ box_7_21(V[v0101]^V[k01],V[v0100]^V[k31],V[v0131]^V[k42],V[v0130]^V[k24],V[v0129]^V[k63],V[v0128]^V[k23]);
		V[v0227] = V[v0027] ^ box_7_27(V[v0101]^V[k01],V[v0100]^V[k31],V[v0131]^V[k42],V[v0130]^V[k24],V[v0129]^V[k63],V[v0128]^V[k23]);

		if (opt_split)
			splitTree(V, v0200, 2);

		V[v0309] = V[v0109] ^ box_0_9 (V[v0205]^V[k56],V[v0204]^V[k17],V[v0203]^V[k67],V[v0202]^V[k75],V[v0201]^V[k15],V[v0200]^V[k45]);
		V[v0317] = V[v0117] ^ box_0_17(V[v0205]^V[k56],V[v0204]^V[k17],V[v0203]^V[k67],V[v0202]^V[k75],V[v0201]^V[k15],V[v0200]^V[k45]);
		V[v0323] = V[v0123] ^ box_0_23(V[v0205]^V[k56],V[v0204]^V[k17],V[v0203]^V[k67],V[v0202]^V[k75],V[v0201]^V[k15],V[v0200]^V[k45]);
		V[v0331] = V[v0131] ^ box_0_31(V[v0205]^V[k56],V[v0204]^V[k17],V[v0203]^V[k67],V[v0202]^V[k75],V[v0201]^V[k15],V[v0200]^V[k45]);
		V[v0306] = V[v0106] ^ box_2_6 (V[v0213]^V[k26],V[v0212]^V[k47],V[v0211]^V[k66],V[v0210]^V[k65],V[v0209]^V[k74],V[v0208]^V[k77]);
		V[v0316] = V[v0116] ^ box_2_16(V[v0213]^V[k26],V[v0212]^V[k47],V[v0211]^V[k66],V[v0210]^V[k65],V[v0209]^V[k74],V[v0208]^V[k77]);
		V[v0324] = V[v0124] ^ box_2_24(V[v0213]^V[k26],V[v0212]^V[k47],V[v0211]^V[k66],V[v0210]^V[k65],V[v0209]^V[k74],V[v0208]^V[k77]);
		V[v0330] = V[v0130] ^ box_2_30(V[v0213]^V[k26],V[v0212]^V[k47],V[v0211]^V[k66],V[v0210]^V[k65],V[v0209]^V[k74],V[v0208]^V[k77]);
		V[v0303] = V[v0103] ^ box_4_3 (V[v0221]^V[k31],V[v0220]^V[k34],V[v0219]^V[k12],V[v0218]^V[k72],V[v0217]^V[k61],V[v0216]^V[k53]);
		V[v0308] = V[v0108] ^ box_4_8 (V[v0221]^V[k31],V[v0220]^V[k34],V[v0219]^V[k12],V[v0218]^V[k72],V[v0217]^V[k61],V[v0216]^V[k53]);
		V[v0314] = V[v0114] ^ box_4_14(V[v0221]^V[k31],V[v0220]^V[k34],V[v0219]^V[k12],V[v0218]^V[k72],V[v0217]^V[k61],V[v0216]^V[k53]);
		V[v0325] = V[v0125] ^ box_4_25(V[v0221]^V[k31],V[v0220]^V[k34],V[v0219]^V[k12],V[v0218]^V[k72],V[v0217]^V[k61],V[v0216]^V[k53]);
		V[v0300] = V[v0100] ^ box_6_0 (V[v0229]^V[k51],V[v0228]^V[k42],V[v0227]^V[k21],V[v0226]^V[k73],V[v0225]^V[k14],V[v0224]^V[k23]);
		V[v0307] = V[v0107] ^ box_6_7 (V[v0229]^V[k51],V[v0228]^V[k42],V[v0227]^V[k21],V[v0226]^V[k73],V[v0225]^V[k14],V[v0224]^V[k23]);
		V[v0312] = V[v0112] ^ box_6_12(V[v0229]^V[k51],V[v0228]^V[k42],V[v0227]^V[k21],V[v0226]^V[k73],V[v0225]^V[k14],V[v0224]^V[k23]);
		V[v0322] = V[v0122] ^ box_6_22(V[v0229]^V[k51],V[v0228]^V[k42],V[v0227]^V[k21],V[v0226]^V[k73],V[v0225]^V[k14],V[v0224]^V[k23]);
		V[v0302] = V[v0102] ^ box_1_2 (V[v0209]^V[k06],V[v0208]^V[k54],V[v0207]^V[k46],V[v0206]^V[k35],V[v0205]^V[k27],V[v0204]^V[k76]);
		V[v0313] = V[v0113] ^ box_1_13(V[v0209]^V[k06],V[v0208]^V[k54],V[v0207]^V[k46],V[v0206]^V[k35],V[v0205]^V[k27],V[v0204]^V[k76]);
		V[v0318] = V[v0118] ^ box_1_18(V[v0209]^V[k06],V[v0208]^V[k54],V[v0207]^V[k46],V[v0206]^V[k35],V[v0205]^V[k27],V[v0204]^V[k76]);
		V[v0328] = V[v0128] ^ box_1_28(V[v0209]^V[k06],V[v0208]^V[k54],V[v0207]^V[k46],V[v0206]^V[k35],V[v0205]^V[k27],V[v0204]^V[k76]);
		V[v0301] = V[v0101] ^ box_3_1 (V[v0217]^V[k07],V[v0216]^V[k55],V[v0215]^V[k64],V[v0214]^V[k37],V[v0213]^V[k36],V[v0212]^V[k25]);
		V[v0310] = V[v0110] ^ box_3_10(V[v0217]^V[k07],V[v0216]^V[k55],V[v0215]^V[k64],V[v0214]^V[k37],V[v0213]^V[k36],V[v0212]^V[k25]);
		V[v0320] = V[v0120] ^ box_3_20(V[v0217]^V[k07],V[v0216]^V[k55],V[v0215]^V[k64],V[v0214]^V[k37],V[v0213]^V[k36],V[v0212]^V[k25]);
		V[v0326] = V[v0126] ^ box_3_26(V[v0217]^V[k07],V[v0216]^V[k55],V[v0215]^V[k64],V[v0214]^V[k37],V[v0213]^V[k36],V[v0212]^V[k25]);
		V[v0304] = V[v0104] ^ box_5_4 (V[v0225]^V[k24],V[v0224]^V[k52],V[v0223]^V[k13],V[v0222]^V[k71],V[v0221]^V[k63],V[v0220]^V[k01]);
		V[v0311] = V[v0111] ^ box_5_11(V[v0225]^V[k24],V[v0224]^V[k52],V[v0223]^V[k13],V[v0222]^V[k71],V[v0221]^V[k63],V[v0220]^V[k01]);
		V[v0319] = V[v0119] ^ box_5_19(V[v0225]^V[k24],V[v0224]^V[k52],V[v0223]^V[k13],V[v0222]^V[k71],V[v0221]^V[k63],V[v0220]^V[k01]);
		V[v0329] = V[v0129] ^ box_5_29(V[v0225]^V[k24],V[v0224]^V[k52],V[v0223]^V[k13],V[v0222]^V[k71],V[v0221]^V[k63],V[v0220]^V[k01]);
		V[v0305] = V[v0105] ^ box_7_5 (V[v0201]^V[k62],V[v0200]^V[k11],V[v0231]^V[k22],V[v0230]^V[k04],V[v0229]^V[k43],V[v0228]^V[k03]);
		V[v0315] = V[v0115] ^ box_7_15(V[v0201]^V[k62],V[v0200]^V[k11],V[v0231]^V[k22],V[v0230]^V[k04],V[v0229]^V[k43],V[v0228]^V[k03]);
		V[v0321] = V[v0121] ^ box_7_21(V[v0201]^V[k62],V[v0200]^V[k11],V[v0231]^V[k22],V[v0230]^V[k04],V[v0229]^V[k43],V[v0228]^V[k03]);
		V[v0327] = V[v0127] ^ box_7_27(V[v0201]^V[k62],V[v0200]^V[k11],V[v0231]^V[k22],V[v0230]^V[k04],V[v0229]^V[k43],V[v0228]^V[k03]);

		if (opt_split)
			splitTree(V, v0300, 3);

		V[v0409] = V[v0209] ^ box_0_9 (V[v0305]^V[k36],V[v0304]^V[k76],V[v0303]^V[k47],V[v0302]^V[k55],V[v0301]^V[k74],V[v0300]^V[k25]);
		V[v0417] = V[v0217] ^ box_0_17(V[v0305]^V[k36],V[v0304]^V[k76],V[v0303]^V[k47],V[v0302]^V[k55],V[v0301]^V[k74],V[v0300]^V[k25]);
		V[v0423] = V[v0223] ^ box_0_23(V[v0305]^V[k36],V[v0304]^V[k76],V[v0303]^V[k47],V[v0302]^V[k55],V[v0301]^V[k74],V[v0300]^V[k25]);
		V[v0431] = V[v0231] ^ box_0_31(V[v0305]^V[k36],V[v0304]^V[k76],V[v0303]^V[k47],V[v0302]^V[k55],V[v0301]^V[k74],V[v0300]^V[k25]);
		V[v0406] = V[v0206] ^ box_2_6 (V[v0313]^V[k06],V[v0312]^V[k27],V[v0311]^V[k46],V[v0310]^V[k45],V[v0309]^V[k54],V[v0308]^V[k57]);
		V[v0416] = V[v0216] ^ box_2_16(V[v0313]^V[k06],V[v0312]^V[k27],V[v0311]^V[k46],V[v0310]^V[k45],V[v0309]^V[k54],V[v0308]^V[k57]);
		V[v0424] = V[v0224] ^ box_2_24(V[v0313]^V[k06],V[v0312]^V[k27],V[v0311]^V[k46],V[v0310]^V[k45],V[v0309]^V[k54],V[v0308]^V[k57]);
		V[v0430] = V[v0230] ^ box_2_30(V[v0313]^V[k06],V[v0312]^V[k27],V[v0311]^V[k46],V[v0310]^V[k45],V[v0309]^V[k54],V[v0308]^V[k57]);
		V[v0403] = V[v0203] ^ box_4_3 (V[v0321]^V[k11],V[v0320]^V[k14],V[v0319]^V[k73],V[v0318]^V[k52],V[v0317]^V[k41],V[v0316]^V[k33]);
		V[v0408] = V[v0208] ^ box_4_8 (V[v0321]^V[k11],V[v0320]^V[k14],V[v0319]^V[k73],V[v0318]^V[k52],V[v0317]^V[k41],V[v0316]^V[k33]);
		V[v0414] = V[v0214] ^ box_4_14(V[v0321]^V[k11],V[v0320]^V[k14],V[v0319]^V[k73],V[v0318]^V[k52],V[v0317]^V[k41],V[v0316]^V[k33]);
		V[v0425] = V[v0225] ^ box_4_25(V[v0321]^V[k11],V[v0320]^V[k14],V[v0319]^V[k73],V[v0318]^V[k52],V[v0317]^V[k41],V[v0316]^V[k33]);
		V[v0400] = V[v0200] ^ box_6_0 (V[v0329]^V[k31],V[v0328]^V[k22],V[v0327]^V[k01],V[v0326]^V[k53],V[v0325]^V[k71],V[v0324]^V[k03]);
		V[v0407] = V[v0207] ^ box_6_7 (V[v0329]^V[k31],V[v0328]^V[k22],V[v0327]^V[k01],V[v0326]^V[k53],V[v0325]^V[k71],V[v0324]^V[k03]);
		V[v0412] = V[v0212] ^ box_6_12(V[v0329]^V[k31],V[v0328]^V[k22],V[v0327]^V[k01],V[v0326]^V[k53],V[v0325]^V[k71],V[v0324]^V[k03]);
		V[v0422] = V[v0222] ^ box_6_22(V[v0329]^V[k31],V[v0328]^V[k22],V[v0327]^V[k01],V[v0326]^V[k53],V[v0325]^V[k71],V[v0324]^V[k03]);
		V[v0402] = V[v0202] ^ box_1_2 (V[v0309]^V[k65],V[v0308]^V[k77],V[v0307]^V[k26],V[v0306]^V[k15],V[v0305]^V[k07],V[v0304]^V[k56]);
		V[v0413] = V[v0213] ^ box_1_13(V[v0309]^V[k65],V[v0308]^V[k77],V[v0307]^V[k26],V[v0306]^V[k15],V[v0305]^V[k07],V[v0304]^V[k56]);
		V[v0418] = V[v0218] ^ box_1_18(V[v0309]^V[k65],V[v0308]^V[k77],V[v0307]^V[k26],V[v0306]^V[k15],V[v0305]^V[k07],V[v0304]^V[k56]);
		V[v0428] = V[v0228] ^ box_1_28(V[v0309]^V[k65],V[v0308]^V[k77],V[v0307]^V[k26],V[v0306]^V[k15],V[v0305]^V[k07],V[v0304]^V[k56]);
		V[v0401] = V[v0201] ^ box_3_1 (V[v0317]^V[k66],V[v0316]^V[k35],V[v0315]^V[k44],V[v0314]^V[k17],V[v0313]^V[k16],V[v0312]^V[k05]);
		V[v0410] = V[v0210] ^ box_3_10(V[v0317]^V[k66],V[v0316]^V[k35],V[v0315]^V[k44],V[v0314]^V[k17],V[v0313]^V[k16],V[v0312]^V[k05]);
		V[v0420] = V[v0220] ^ box_3_20(V[v0317]^V[k66],V[v0316]^V[k35],V[v0315]^V[k44],V[v0314]^V[k17],V[v0313]^V[k16],V[v0312]^V[k05]);
		V[v0426] = V[v0226] ^ box_3_26(V[v0317]^V[k66],V[v0316]^V[k35],V[v0315]^V[k44],V[v0314]^V[k17],V[v0313]^V[k16],V[v0312]^V[k05]);
		V[v0404] = V[v0204] ^ box_5_4 (V[v0325]^V[k04],V[v0324]^V[k32],V[v0323]^V[k34],V[v0322]^V[k51],V[v0321]^V[k43],V[v0320]^V[k62]);
		V[v0411] = V[v0211] ^ box_5_11(V[v0325]^V[k04],V[v0324]^V[k32],V[v0323]^V[k34],V[v0322]^V[k51],V[v0321]^V[k43],V[v0320]^V[k62]);
		V[v0419] = V[v0219] ^ box_5_19(V[v0325]^V[k04],V[v0324]^V[k32],V[v0323]^V[k34],V[v0322]^V[k51],V[v0321]^V[k43],V[v0320]^V[k62]);
		V[v0429] = V[v0229] ^ box_5_29(V[v0325]^V[k04],V[v0324]^V[k32],V[v0323]^V[k34],V[v0322]^V[k51],V[v0321]^V[k43],V[v0320]^V[k62]);
		V[v0405] = V[v0205] ^ box_7_5 (V[v0301]^V[k42],V[v0300]^V[k72],V[v0331]^V[k02],V[v0330]^V[k61],V[v0329]^V[k23],V[v0328]^V[k24]);
		V[v0415] = V[v0215] ^ box_7_15(V[v0301]^V[k42],V[v0300]^V[k72],V[v0331]^V[k02],V[v0330]^V[k61],V[v0329]^V[k23],V[v0328]^V[k24]);
		V[v0421] = V[v0221] ^ box_7_21(V[v0301]^V[k42],V[v0300]^V[k72],V[v0331]^V[k02],V[v0330]^V[k61],V[v0329]^V[k23],V[v0328]^V[k24]);
		V[v0427] = V[v0227] ^ box_7_27(V[v0301]^V[k42],V[v0300]^V[k72],V[v0331]^V[k02],V[v0330]^V[k61],V[v0329]^V[k23],V[v0328]^V[k24]);

		if (opt_split)
			splitTree(V, v0400, 4);

		V[v0509] = V[v0309] ^ box_0_9 (V[v0405]^V[k16],V[v0404]^V[k56],V[v0403]^V[k27],V[v0402]^V[k35],V[v0401]^V[k54],V[v0400]^V[k05]);
		V[v0517] = V[v0317] ^ box_0_17(V[v0405]^V[k16],V[v0404]^V[k56],V[v0403]^V[k27],V[v0402]^V[k35],V[v0401]^V[k54],V[v0400]^V[k05]);
		V[v0523] = V[v0323] ^ box_0_23(V[v0405]^V[k16],V[v0404]^V[k56],V[v0403]^V[k27],V[v0402]^V[k35],V[v0401]^V[k54],V[v0400]^V[k05]);
		V[v0531] = V[v0331] ^ box_0_31(V[v0405]^V[k16],V[v0404]^V[k56],V[v0403]^V[k27],V[v0402]^V[k35],V[v0401]^V[k54],V[v0400]^V[k05]);
		V[v0506] = V[v0306] ^ box_2_6 (V[v0413]^V[k65],V[v0412]^V[k07],V[v0411]^V[k26],V[v0410]^V[k25],V[v0409]^V[k77],V[v0408]^V[k37]);
		V[v0516] = V[v0316] ^ box_2_16(V[v0413]^V[k65],V[v0412]^V[k07],V[v0411]^V[k26],V[v0410]^V[k25],V[v0409]^V[k77],V[v0408]^V[k37]);
		V[v0524] = V[v0324] ^ box_2_24(V[v0413]^V[k65],V[v0412]^V[k07],V[v0411]^V[k26],V[v0410]^V[k25],V[v0409]^V[k77],V[v0408]^V[k37]);
		V[v0530] = V[v0330] ^ box_2_30(V[v0413]^V[k65],V[v0412]^V[k07],V[v0411]^V[k26],V[v0410]^V[k25],V[v0409]^V[k77],V[v0408]^V[k37]);
		V[v0503] = V[v0303] ^ box_4_3 (V[v0421]^V[k72],V[v0420]^V[k71],V[v0419]^V[k53],V[v0418]^V[k32],V[v0417]^V[k21],V[v0416]^V[k13]);
		V[v0508] = V[v0308] ^ box_4_8 (V[v0421]^V[k72],V[v0420]^V[k71],V[v0419]^V[k53],V[v0418]^V[k32],V[v0417]^V[k21],V[v0416]^V[k13]);
		V[v0514] = V[v0314] ^ box_4_14(V[v0421]^V[k72],V[v0420]^V[k71],V[v0419]^V[k53],V[v0418]^V[k32],V[v0417]^V[k21],V[v0416]^V[k13]);
		V[v0525] = V[v0325] ^ box_4_25(V[v0421]^V[k72],V[v0420]^V[k71],V[v0419]^V[k53],V[v0418]^V[k32],V[v0417]^V[k21],V[v0416]^V[k13]);
		V[v0500] = V[v0300] ^ box_6_0 (V[v0429]^V[k11],V[v0428]^V[k02],V[v0427]^V[k62],V[v0426]^V[k33],V[v0425]^V[k51],V[v0424]^V[k24]);
		V[v0507] = V[v0307] ^ box_6_7 (V[v0429]^V[k11],V[v0428]^V[k02],V[v0427]^V[k62],V[v0426]^V[k33],V[v0425]^V[k51],V[v0424]^V[k24]);
		V[v0512] = V[v0312] ^ box_6_12(V[v0429]^V[k11],V[v0428]^V[k02],V[v0427]^V[k62],V[v0426]^V[k33],V[v0425]^V[k51],V[v0424]^V[k24]);
		V[v0522] = V[v0322] ^ box_6_22(V[v0429]^V[k11],V[v0428]^V[k02],V[v0427]^V[k62],V[v0426]^V[k33],V[v0425]^V[k51],V[v0424]^V[k24]);
		V[v0502] = V[v0302] ^ box_1_2 (V[v0409]^V[k45],V[v0408]^V[k57],V[v0407]^V[k06],V[v0406]^V[k74],V[v0405]^V[k66],V[v0404]^V[k36]);
		V[v0513] = V[v0313] ^ box_1_13(V[v0409]^V[k45],V[v0408]^V[k57],V[v0407]^V[k06],V[v0406]^V[k74],V[v0405]^V[k66],V[v0404]^V[k36]);
		V[v0518] = V[v0318] ^ box_1_18(V[v0409]^V[k45],V[v0408]^V[k57],V[v0407]^V[k06],V[v0406]^V[k74],V[v0405]^V[k66],V[v0404]^V[k36]);
		V[v0528] = V[v0328] ^ box_1_28(V[v0409]^V[k45],V[v0408]^V[k57],V[v0407]^V[k06],V[v0406]^V[k74],V[v0405]^V[k66],V[v0404]^V[k36]);
		V[v0501] = V[v0301] ^ box_3_1 (V[v0417]^V[k46],V[v0416]^V[k15],V[v0415]^V[k67],V[v0414]^V[k76],V[v0413]^V[k75],V[v0412]^V[k64]);
		V[v0510] = V[v0310] ^ box_3_10(V[v0417]^V[k46],V[v0416]^V[k15],V[v0415]^V[k67],V[v0414]^V[k76],V[v0413]^V[k75],V[v0412]^V[k64]);
		V[v0520] = V[v0320] ^ box_3_20(V[v0417]^V[k46],V[v0416]^V[k15],V[v0415]^V[k67],V[v0414]^V[k76],V[v0413]^V[k75],V[v0412]^V[k64]);
		V[v0526] = V[v0326] ^ box_3_26(V[v0417]^V[k46],V[v0416]^V[k15],V[v0415]^V[k67],V[v0414]^V[k76],V[v0413]^V[k75],V[v0412]^V[k64]);
		V[v0504] = V[v0304] ^ box_5_4 (V[v0425]^V[k61],V[v0424]^V[k12],V[v0423]^V[k14],V[v0422]^V[k31],V[v0421]^V[k23],V[v0420]^V[k42]);
		V[v0511] = V[v0311] ^ box_5_11(V[v0425]^V[k61],V[v0424]^V[k12],V[v0423]^V[k14],V[v0422]^V[k31],V[v0421]^V[k23],V[v0420]^V[k42]);
		V[v0519] = V[v0319] ^ box_5_19(V[v0425]^V[k61],V[v0424]^V[k12],V[v0423]^V[k14],V[v0422]^V[k31],V[v0421]^V[k23],V[v0420]^V[k42]);
		V[v0529] = V[v0329] ^ box_5_29(V[v0425]^V[k61],V[v0424]^V[k12],V[v0423]^V[k14],V[v0422]^V[k31],V[v0421]^V[k23],V[v0420]^V[k42]);
		V[v0505] = V[v0305] ^ box_7_5 (V[v0401]^V[k22],V[v0400]^V[k52],V[v0431]^V[k63],V[v0430]^V[k41],V[v0429]^V[k03],V[v0428]^V[k04]);
		V[v0515] = V[v0315] ^ box_7_15(V[v0401]^V[k22],V[v0400]^V[k52],V[v0431]^V[k63],V[v0430]^V[k41],V[v0429]^V[k03],V[v0428]^V[k04]);
		V[v0521] = V[v0321] ^ box_7_21(V[v0401]^V[k22],V[v0400]^V[k52],V[v0431]^V[k63],V[v0430]^V[k41],V[v0429]^V[k03],V[v0428]^V[k04]);
		V[v0527] = V[v0327] ^ box_7_27(V[v0401]^V[k22],V[v0400]^V[k52],V[v0431]^V[k63],V[v0430]^V[k41],V[v0429]^V[k03],V[v0428]^V[k04]);

		if (opt_split)
			splitTree(V, v0500, 5);

		V[v0609] = V[v0409] ^ box_0_9 (V[v0505]^V[k75],V[v0504]^V[k36],V[v0503]^V[k07],V[v0502]^V[k15],V[v0501]^V[k77],V[v0500]^V[k64]);
		V[v0617] = V[v0417] ^ box_0_17(V[v0505]^V[k75],V[v0504]^V[k36],V[v0503]^V[k07],V[v0502]^V[k15],V[v0501]^V[k77],V[v0500]^V[k64]);
		V[v0623] = V[v0423] ^ box_0_23(V[v0505]^V[k75],V[v0504]^V[k36],V[v0503]^V[k07],V[v0502]^V[k15],V[v0501]^V[k77],V[v0500]^V[k64]);
		V[v0631] = V[v0431] ^ box_0_31(V[v0505]^V[k75],V[v0504]^V[k36],V[v0503]^V[k07],V[v0502]^V[k15],V[v0501]^V[k77],V[v0500]^V[k64]);
		V[v0606] = V[v0406] ^ box_2_6 (V[v0513]^V[k45],V[v0512]^V[k66],V[v0511]^V[k06],V[v0510]^V[k05],V[v0509]^V[k57],V[v0508]^V[k17]);
		V[v0616] = V[v0416] ^ box_2_16(V[v0513]^V[k45],V[v0512]^V[k66],V[v0511]^V[k06],V[v0510]^V[k05],V[v0509]^V[k57],V[v0508]^V[k17]);
		V[v0624] = V[v0424] ^ box_2_24(V[v0513]^V[k45],V[v0512]^V[k66],V[v0511]^V[k06],V[v0510]^V[k05],V[v0509]^V[k57],V[v0508]^V[k17]);
		V[v0630] = V[v0430] ^ box_2_30(V[v0513]^V[k45],V[v0512]^V[k66],V[v0511]^V[k06],V[v0510]^V[k05],V[v0509]^V[k57],V[v0508]^V[k17]);
		V[v0603] = V[v0403] ^ box_4_3 (V[v0521]^V[k52],V[v0520]^V[k51],V[v0519]^V[k33],V[v0518]^V[k12],V[v0517]^V[k01],V[v0516]^V[k34]);
		V[v0608] = V[v0408] ^ box_4_8 (V[v0521]^V[k52],V[v0520]^V[k51],V[v0519]^V[k33],V[v0518]^V[k12],V[v0517]^V[k01],V[v0516]^V[k34]);
		V[v0614] = V[v0414] ^ box_4_14(V[v0521]^V[k52],V[v0520]^V[k51],V[v0519]^V[k33],V[v0518]^V[k12],V[v0517]^V[k01],V[v0516]^V[k34]);
		V[v0625] = V[v0425] ^ box_4_25(V[v0521]^V[k52],V[v0520]^V[k51],V[v0519]^V[k33],V[v0518]^V[k12],V[v0517]^V[k01],V[v0516]^V[k34]);
		V[v0600] = V[v0400] ^ box_6_0 (V[v0529]^V[k72],V[v0528]^V[k63],V[v0527]^V[k42],V[v0526]^V[k13],V[v0525]^V[k31],V[v0524]^V[k04]);
		V[v0607] = V[v0407] ^ box_6_7 (V[v0529]^V[k72],V[v0528]^V[k63],V[v0527]^V[k42],V[v0526]^V[k13],V[v0525]^V[k31],V[v0524]^V[k04]);
		V[v0612] = V[v0412] ^ box_6_12(V[v0529]^V[k72],V[v0528]^V[k63],V[v0527]^V[k42],V[v0526]^V[k13],V[v0525]^V[k31],V[v0524]^V[k04]);
		V[v0622] = V[v0422] ^ box_6_22(V[v0529]^V[k72],V[v0528]^V[k63],V[v0527]^V[k42],V[v0526]^V[k13],V[v0525]^V[k31],V[v0524]^V[k04]);
		V[v0602] = V[v0402] ^ box_1_2 (V[v0509]^V[k25],V[v0508]^V[k37],V[v0507]^V[k65],V[v0506]^V[k54],V[v0505]^V[k46],V[v0504]^V[k16]);
		V[v0613] = V[v0413] ^ box_1_13(V[v0509]^V[k25],V[v0508]^V[k37],V[v0507]^V[k65],V[v0506]^V[k54],V[v0505]^V[k46],V[v0504]^V[k16]);
		V[v0618] = V[v0418] ^ box_1_18(V[v0509]^V[k25],V[v0508]^V[k37],V[v0507]^V[k65],V[v0506]^V[k54],V[v0505]^V[k46],V[v0504]^V[k16]);
		V[v0628] = V[v0428] ^ box_1_28(V[v0509]^V[k25],V[v0508]^V[k37],V[v0507]^V[k65],V[v0506]^V[k54],V[v0505]^V[k46],V[v0504]^V[k16]);
		V[v0601] = V[v0401] ^ box_3_1 (V[v0517]^V[k26],V[v0516]^V[k74],V[v0515]^V[k47],V[v0514]^V[k56],V[v0513]^V[k55],V[v0512]^V[k44]);
		V[v0610] = V[v0410] ^ box_3_10(V[v0517]^V[k26],V[v0516]^V[k74],V[v0515]^V[k47],V[v0514]^V[k56],V[v0513]^V[k55],V[v0512]^V[k44]);
		V[v0620] = V[v0420] ^ box_3_20(V[v0517]^V[k26],V[v0516]^V[k74],V[v0515]^V[k47],V[v0514]^V[k56],V[v0513]^V[k55],V[v0512]^V[k44]);
		V[v0626] = V[v0426] ^ box_3_26(V[v0517]^V[k26],V[v0516]^V[k74],V[v0515]^V[k47],V[v0514]^V[k56],V[v0513]^V[k55],V[v0512]^V[k44]);
		V[v0604] = V[v0404] ^ box_5_4 (V[v0525]^V[k41],V[v0524]^V[k73],V[v0523]^V[k71],V[v0522]^V[k11],V[v0521]^V[k03],V[v0520]^V[k22]);
		V[v0611] = V[v0411] ^ box_5_11(V[v0525]^V[k41],V[v0524]^V[k73],V[v0523]^V[k71],V[v0522]^V[k11],V[v0521]^V[k03],V[v0520]^V[k22]);
		V[v0619] = V[v0419] ^ box_5_19(V[v0525]^V[k41],V[v0524]^V[k73],V[v0523]^V[k71],V[v0522]^V[k11],V[v0521]^V[k03],V[v0520]^V[k22]);
		V[v0629] = V[v0429] ^ box_5_29(V[v0525]^V[k41],V[v0524]^V[k73],V[v0523]^V[k71],V[v0522]^V[k11],V[v0521]^V[k03],V[v0520]^V[k22]);
		V[v0605] = V[v0405] ^ box_7_5 (V[v0501]^V[k02],V[v0500]^V[k32],V[v0531]^V[k43],V[v0530]^V[k21],V[v0529]^V[k24],V[v0528]^V[k61]);
		V[v0615] = V[v0415] ^ box_7_15(V[v0501]^V[k02],V[v0500]^V[k32],V[v0531]^V[k43],V[v0530]^V[k21],V[v0529]^V[k24],V[v0528]^V[k61]);
		V[v0621] = V[v0421] ^ box_7_21(V[v0501]^V[k02],V[v0500]^V[k32],V[v0531]^V[k43],V[v0530]^V[k21],V[v0529]^V[k24],V[v0528]^V[k61]);
		V[v0627] = V[v0427] ^ box_7_27(V[v0501]^V[k02],V[v0500]^V[k32],V[v0531]^V[k43],V[v0530]^V[k21],V[v0529]^V[k24],V[v0528]^V[k61]);

		if (opt_split)
			splitTree(V, v0600, 6);

		V[v0709] = V[v0509] ^ box_0_9 (V[v0605]^V[k55],V[v0604]^V[k16],V[v0603]^V[k66],V[v0602]^V[k74],V[v0601]^V[k57],V[v0600]^V[k44]);
		V[v0717] = V[v0517] ^ box_0_17(V[v0605]^V[k55],V[v0604]^V[k16],V[v0603]^V[k66],V[v0602]^V[k74],V[v0601]^V[k57],V[v0600]^V[k44]);
		V[v0723] = V[v0523] ^ box_0_23(V[v0605]^V[k55],V[v0604]^V[k16],V[v0603]^V[k66],V[v0602]^V[k74],V[v0601]^V[k57],V[v0600]^V[k44]);
		V[v0731] = V[v0531] ^ box_0_31(V[v0605]^V[k55],V[v0604]^V[k16],V[v0603]^V[k66],V[v0602]^V[k74],V[v0601]^V[k57],V[v0600]^V[k44]);
		V[v0706] = V[v0506] ^ box_2_6 (V[v0613]^V[k25],V[v0612]^V[k46],V[v0611]^V[k65],V[v0610]^V[k64],V[v0609]^V[k37],V[v0608]^V[k76]);
		V[v0716] = V[v0516] ^ box_2_16(V[v0613]^V[k25],V[v0612]^V[k46],V[v0611]^V[k65],V[v0610]^V[k64],V[v0609]^V[k37],V[v0608]^V[k76]);
		V[v0724] = V[v0524] ^ box_2_24(V[v0613]^V[k25],V[v0612]^V[k46],V[v0611]^V[k65],V[v0610]^V[k64],V[v0609]^V[k37],V[v0608]^V[k76]);
		V[v0730] = V[v0530] ^ box_2_30(V[v0613]^V[k25],V[v0612]^V[k46],V[v0611]^V[k65],V[v0610]^V[k64],V[v0609]^V[k37],V[v0608]^V[k76]);
		V[v0703] = V[v0503] ^ box_4_3 (V[v0621]^V[k32],V[v0620]^V[k31],V[v0619]^V[k13],V[v0618]^V[k73],V[v0617]^V[k62],V[v0616]^V[k14]);
		V[v0708] = V[v0508] ^ box_4_8 (V[v0621]^V[k32],V[v0620]^V[k31],V[v0619]^V[k13],V[v0618]^V[k73],V[v0617]^V[k62],V[v0616]^V[k14]);
		V[v0714] = V[v0514] ^ box_4_14(V[v0621]^V[k32],V[v0620]^V[k31],V[v0619]^V[k13],V[v0618]^V[k73],V[v0617]^V[k62],V[v0616]^V[k14]);
		V[v0725] = V[v0525] ^ box_4_25(V[v0621]^V[k32],V[v0620]^V[k31],V[v0619]^V[k13],V[v0618]^V[k73],V[v0617]^V[k62],V[v0616]^V[k14]);
		V[v0700] = V[v0500] ^ box_6_0 (V[v0629]^V[k52],V[v0628]^V[k43],V[v0627]^V[k22],V[v0626]^V[k34],V[v0625]^V[k11],V[v0624]^V[k61]);
		V[v0707] = V[v0507] ^ box_6_7 (V[v0629]^V[k52],V[v0628]^V[k43],V[v0627]^V[k22],V[v0626]^V[k34],V[v0625]^V[k11],V[v0624]^V[k61]);
		V[v0712] = V[v0512] ^ box_6_12(V[v0629]^V[k52],V[v0628]^V[k43],V[v0627]^V[k22],V[v0626]^V[k34],V[v0625]^V[k11],V[v0624]^V[k61]);
		V[v0722] = V[v0522] ^ box_6_22(V[v0629]^V[k52],V[v0628]^V[k43],V[v0627]^V[k22],V[v0626]^V[k34],V[v0625]^V[k11],V[v0624]^V[k61]);
		V[v0702] = V[v0502] ^ box_1_2 (V[v0609]^V[k05],V[v0608]^V[k17],V[v0607]^V[k45],V[v0606]^V[k77],V[v0605]^V[k26],V[v0604]^V[k75]);
		V[v0713] = V[v0513] ^ box_1_13(V[v0609]^V[k05],V[v0608]^V[k17],V[v0607]^V[k45],V[v0606]^V[k77],V[v0605]^V[k26],V[v0604]^V[k75]);
		V[v0718] = V[v0518] ^ box_1_18(V[v0609]^V[k05],V[v0608]^V[k17],V[v0607]^V[k45],V[v0606]^V[k77],V[v0605]^V[k26],V[v0604]^V[k75]);
		V[v0728] = V[v0528] ^ box_1_28(V[v0609]^V[k05],V[v0608]^V[k17],V[v0607]^V[k45],V[v0606]^V[k77],V[v0605]^V[k26],V[v0604]^V[k75]);
		V[v0701] = V[v0501] ^ box_3_1 (V[v0617]^V[k06],V[v0616]^V[k54],V[v0615]^V[k27],V[v0614]^V[k36],V[v0613]^V[k35],V[v0612]^V[k67]);
		V[v0710] = V[v0510] ^ box_3_10(V[v0617]^V[k06],V[v0616]^V[k54],V[v0615]^V[k27],V[v0614]^V[k36],V[v0613]^V[k35],V[v0612]^V[k67]);
		V[v0720] = V[v0520] ^ box_3_20(V[v0617]^V[k06],V[v0616]^V[k54],V[v0615]^V[k27],V[v0614]^V[k36],V[v0613]^V[k35],V[v0612]^V[k67]);
		V[v0726] = V[v0526] ^ box_3_26(V[v0617]^V[k06],V[v0616]^V[k54],V[v0615]^V[k27],V[v0614]^V[k36],V[v0613]^V[k35],V[v0612]^V[k67]);
		V[v0704] = V[v0504] ^ box_5_4 (V[v0625]^V[k21],V[v0624]^V[k53],V[v0623]^V[k51],V[v0622]^V[k72],V[v0621]^V[k24],V[v0620]^V[k02]);
		V[v0711] = V[v0511] ^ box_5_11(V[v0625]^V[k21],V[v0624]^V[k53],V[v0623]^V[k51],V[v0622]^V[k72],V[v0621]^V[k24],V[v0620]^V[k02]);
		V[v0719] = V[v0519] ^ box_5_19(V[v0625]^V[k21],V[v0624]^V[k53],V[v0623]^V[k51],V[v0622]^V[k72],V[v0621]^V[k24],V[v0620]^V[k02]);
		V[v0729] = V[v0529] ^ box_5_29(V[v0625]^V[k21],V[v0624]^V[k53],V[v0623]^V[k51],V[v0622]^V[k72],V[v0621]^V[k24],V[v0620]^V[k02]);
		V[v0705] = V[v0505] ^ box_7_5 (V[v0601]^V[k63],V[v0600]^V[k12],V[v0631]^V[k23],V[v0630]^V[k01],V[v0629]^V[k04],V[v0628]^V[k41]);
		V[v0715] = V[v0515] ^ box_7_15(V[v0601]^V[k63],V[v0600]^V[k12],V[v0631]^V[k23],V[v0630]^V[k01],V[v0629]^V[k04],V[v0628]^V[k41]);
		V[v0721] = V[v0521] ^ box_7_21(V[v0601]^V[k63],V[v0600]^V[k12],V[v0631]^V[k23],V[v0630]^V[k01],V[v0629]^V[k04],V[v0628]^V[k41]);
		V[v0727] = V[v0527] ^ box_7_27(V[v0601]^V[k63],V[v0600]^V[k12],V[v0631]^V[k23],V[v0630]^V[k01],V[v0629]^V[k04],V[v0628]^V[k41]);

		if (opt_split)
			splitTree(V, v0700, 7);

		V[v0809] = V[v0609] ^ box_0_9 (V[v0705]^V[k45],V[v0704]^V[k06],V[v0703]^V[k56],V[v0702]^V[k64],V[v0701]^V[k47],V[v0700]^V[k77]);
		V[v0817] = V[v0617] ^ box_0_17(V[v0705]^V[k45],V[v0704]^V[k06],V[v0703]^V[k56],V[v0702]^V[k64],V[v0701]^V[k47],V[v0700]^V[k77]);
		V[v0823] = V[v0623] ^ box_0_23(V[v0705]^V[k45],V[v0704]^V[k06],V[v0703]^V[k56],V[v0702]^V[k64],V[v0701]^V[k47],V[v0700]^V[k77]);
		V[v0831] = V[v0631] ^ box_0_31(V[v0705]^V[k45],V[v0704]^V[k06],V[v0703]^V[k56],V[v0702]^V[k64],V[v0701]^V[k47],V[v0700]^V[k77]);
		V[v0806] = V[v0606] ^ box_2_6 (V[v0713]^V[k15],V[v0712]^V[k36],V[v0711]^V[k55],V[v0710]^V[k54],V[v0709]^V[k27],V[v0708]^V[k66]);
		V[v0816] = V[v0616] ^ box_2_16(V[v0713]^V[k15],V[v0712]^V[k36],V[v0711]^V[k55],V[v0710]^V[k54],V[v0709]^V[k27],V[v0708]^V[k66]);
		V[v0824] = V[v0624] ^ box_2_24(V[v0713]^V[k15],V[v0712]^V[k36],V[v0711]^V[k55],V[v0710]^V[k54],V[v0709]^V[k27],V[v0708]^V[k66]);
		V[v0830] = V[v0630] ^ box_2_30(V[v0713]^V[k15],V[v0712]^V[k36],V[v0711]^V[k55],V[v0710]^V[k54],V[v0709]^V[k27],V[v0708]^V[k66]);
		V[v0803] = V[v0603] ^ box_4_3 (V[v0721]^V[k22],V[v0720]^V[k21],V[v0719]^V[k03],V[v0718]^V[k63],V[v0717]^V[k52],V[v0716]^V[k04]);
		V[v0808] = V[v0608] ^ box_4_8 (V[v0721]^V[k22],V[v0720]^V[k21],V[v0719]^V[k03],V[v0718]^V[k63],V[v0717]^V[k52],V[v0716]^V[k04]);
		V[v0814] = V[v0614] ^ box_4_14(V[v0721]^V[k22],V[v0720]^V[k21],V[v0719]^V[k03],V[v0718]^V[k63],V[v0717]^V[k52],V[v0716]^V[k04]);
		V[v0825] = V[v0625] ^ box_4_25(V[v0721]^V[k22],V[v0720]^V[k21],V[v0719]^V[k03],V[v0718]^V[k63],V[v0717]^V[k52],V[v0716]^V[k04]);
		V[v0800] = V[v0600] ^ box_6_0 (V[v0729]^V[k42],V[v0728]^V[k33],V[v0727]^V[k12],V[v0726]^V[k24],V[v0725]^V[k01],V[v0724]^V[k51]);
		V[v0807] = V[v0607] ^ box_6_7 (V[v0729]^V[k42],V[v0728]^V[k33],V[v0727]^V[k12],V[v0726]^V[k24],V[v0725]^V[k01],V[v0724]^V[k51]);
		V[v0812] = V[v0612] ^ box_6_12(V[v0729]^V[k42],V[v0728]^V[k33],V[v0727]^V[k12],V[v0726]^V[k24],V[v0725]^V[k01],V[v0724]^V[k51]);
		V[v0822] = V[v0622] ^ box_6_22(V[v0729]^V[k42],V[v0728]^V[k33],V[v0727]^V[k12],V[v0726]^V[k24],V[v0725]^V[k01],V[v0724]^V[k51]);
		V[v0802] = V[v0602] ^ box_1_2 (V[v0709]^V[k74],V[v0708]^V[k07],V[v0707]^V[k35],V[v0706]^V[k67],V[v0705]^V[k16],V[v0704]^V[k65]);
		V[v0813] = V[v0613] ^ box_1_13(V[v0709]^V[k74],V[v0708]^V[k07],V[v0707]^V[k35],V[v0706]^V[k67],V[v0705]^V[k16],V[v0704]^V[k65]);
		V[v0818] = V[v0618] ^ box_1_18(V[v0709]^V[k74],V[v0708]^V[k07],V[v0707]^V[k35],V[v0706]^V[k67],V[v0705]^V[k16],V[v0704]^V[k65]);
		V[v0828] = V[v0628] ^ box_1_28(V[v0709]^V[k74],V[v0708]^V[k07],V[v0707]^V[k35],V[v0706]^V[k67],V[v0705]^V[k16],V[v0704]^V[k65]);
		V[v0801] = V[v0601] ^ box_3_1 (V[v0717]^V[k75],V[v0716]^V[k44],V[v0715]^V[k17],V[v0714]^V[k26],V[v0713]^V[k25],V[v0712]^V[k57]);
		V[v0810] = V[v0610] ^ box_3_10(V[v0717]^V[k75],V[v0716]^V[k44],V[v0715]^V[k17],V[v0714]^V[k26],V[v0713]^V[k25],V[v0712]^V[k57]);
		V[v0820] = V[v0620] ^ box_3_20(V[v0717]^V[k75],V[v0716]^V[k44],V[v0715]^V[k17],V[v0714]^V[k26],V[v0713]^V[k25],V[v0712]^V[k57]);
		V[v0826] = V[v0626] ^ box_3_26(V[v0717]^V[k75],V[v0716]^V[k44],V[v0715]^V[k17],V[v0714]^V[k26],V[v0713]^V[k25],V[v0712]^V[k57]);
		V[v0804] = V[v0604] ^ box_5_4 (V[v0725]^V[k11],V[v0724]^V[k43],V[v0723]^V[k41],V[v0722]^V[k62],V[v0721]^V[k14],V[v0720]^V[k73]);
		V[v0811] = V[v0611] ^ box_5_11(V[v0725]^V[k11],V[v0724]^V[k43],V[v0723]^V[k41],V[v0722]^V[k62],V[v0721]^V[k14],V[v0720]^V[k73]);
		V[v0819] = V[v0619] ^ box_5_19(V[v0725]^V[k11],V[v0724]^V[k43],V[v0723]^V[k41],V[v0722]^V[k62],V[v0721]^V[k14],V[v0720]^V[k73]);
		V[v0829] = V[v0629] ^ box_5_29(V[v0725]^V[k11],V[v0724]^V[k43],V[v0723]^V[k41],V[v0722]^V[k62],V[v0721]^V[k14],V[v0720]^V[k73]);
		V[v0805] = V[v0605] ^ box_7_5 (V[v0701]^V[k53],V[v0700]^V[k02],V[v0731]^V[k13],V[v0730]^V[k72],V[v0729]^V[k71],V[v0728]^V[k31]);
		V[v0815] = V[v0615] ^ box_7_15(V[v0701]^V[k53],V[v0700]^V[k02],V[v0731]^V[k13],V[v0730]^V[k72],V[v0729]^V[k71],V[v0728]^V[k31]);
		V[v0821] = V[v0621] ^ box_7_21(V[v0701]^V[k53],V[v0700]^V[k02],V[v0731]^V[k13],V[v0730]^V[k72],V[v0729]^V[k71],V[v0728]^V[k31]);
		V[v0827] = V[v0627] ^ box_7_27(V[v0701]^V[k53],V[v0700]^V[k02],V[v0731]^V[k13],V[v0730]^V[k72],V[v0729]^V[k71],V[v0728]^V[k31]);

		if (opt_split)
			splitTree(V, v0800, 8);

		V[v0909] = V[v0709] ^ box_0_9 (V[v0805]^V[k25],V[v0804]^V[k65],V[v0803]^V[k36],V[v0802]^V[k44],V[v0801]^V[k27],V[v0800]^V[k57]);
		V[v0917] = V[v0717] ^ box_0_17(V[v0805]^V[k25],V[v0804]^V[k65],V[v0803]^V[k36],V[v0802]^V[k44],V[v0801]^V[k27],V[v0800]^V[k57]);
		V[v0923] = V[v0723] ^ box_0_23(V[v0805]^V[k25],V[v0804]^V[k65],V[v0803]^V[k36],V[v0802]^V[k44],V[v0801]^V[k27],V[v0800]^V[k57]);
		V[v0931] = V[v0731] ^ box_0_31(V[v0805]^V[k25],V[v0804]^V[k65],V[v0803]^V[k36],V[v0802]^V[k44],V[v0801]^V[k27],V[v0800]^V[k57]);
		V[v0906] = V[v0706] ^ box_2_6 (V[v0813]^V[k74],V[v0812]^V[k16],V[v0811]^V[k35],V[v0810]^V[k77],V[v0809]^V[k07],V[v0808]^V[k46]);
		V[v0916] = V[v0716] ^ box_2_16(V[v0813]^V[k74],V[v0812]^V[k16],V[v0811]^V[k35],V[v0810]^V[k77],V[v0809]^V[k07],V[v0808]^V[k46]);
		V[v0924] = V[v0724] ^ box_2_24(V[v0813]^V[k74],V[v0812]^V[k16],V[v0811]^V[k35],V[v0810]^V[k77],V[v0809]^V[k07],V[v0808]^V[k46]);
		V[v0930] = V[v0730] ^ box_2_30(V[v0813]^V[k74],V[v0812]^V[k16],V[v0811]^V[k35],V[v0810]^V[k77],V[v0809]^V[k07],V[v0808]^V[k46]);
		V[v0903] = V[v0703] ^ box_4_3 (V[v0821]^V[k02],V[v0820]^V[k01],V[v0819]^V[k24],V[v0818]^V[k43],V[v0817]^V[k32],V[v0816]^V[k61]);
		V[v0908] = V[v0708] ^ box_4_8 (V[v0821]^V[k02],V[v0820]^V[k01],V[v0819]^V[k24],V[v0818]^V[k43],V[v0817]^V[k32],V[v0816]^V[k61]);
		V[v0914] = V[v0714] ^ box_4_14(V[v0821]^V[k02],V[v0820]^V[k01],V[v0819]^V[k24],V[v0818]^V[k43],V[v0817]^V[k32],V[v0816]^V[k61]);
		V[v0925] = V[v0725] ^ box_4_25(V[v0821]^V[k02],V[v0820]^V[k01],V[v0819]^V[k24],V[v0818]^V[k43],V[v0817]^V[k32],V[v0816]^V[k61]);
		V[v0900] = V[v0700] ^ box_6_0 (V[v0829]^V[k22],V[v0828]^V[k13],V[v0827]^V[k73],V[v0826]^V[k04],V[v0825]^V[k62],V[v0824]^V[k31]);
		V[v0907] = V[v0707] ^ box_6_7 (V[v0829]^V[k22],V[v0828]^V[k13],V[v0827]^V[k73],V[v0826]^V[k04],V[v0825]^V[k62],V[v0824]^V[k31]);
		V[v0912] = V[v0712] ^ box_6_12(V[v0829]^V[k22],V[v0828]^V[k13],V[v0827]^V[k73],V[v0826]^V[k04],V[v0825]^V[k62],V[v0824]^V[k31]);
		V[v0922] = V[v0722] ^ box_6_22(V[v0829]^V[k22],V[v0828]^V[k13],V[v0827]^V[k73],V[v0826]^V[k04],V[v0825]^V[k62],V[v0824]^V[k31]);
		V[v0902] = V[v0702] ^ box_1_2 (V[v0809]^V[k54],V[v0808]^V[k66],V[v0807]^V[k15],V[v0806]^V[k47],V[v0805]^V[k75],V[v0804]^V[k45]);
		V[v0913] = V[v0713] ^ box_1_13(V[v0809]^V[k54],V[v0808]^V[k66],V[v0807]^V[k15],V[v0806]^V[k47],V[v0805]^V[k75],V[v0804]^V[k45]);
		V[v0918] = V[v0718] ^ box_1_18(V[v0809]^V[k54],V[v0808]^V[k66],V[v0807]^V[k15],V[v0806]^V[k47],V[v0805]^V[k75],V[v0804]^V[k45]);
		V[v0928] = V[v0728] ^ box_1_28(V[v0809]^V[k54],V[v0808]^V[k66],V[v0807]^V[k15],V[v0806]^V[k47],V[v0805]^V[k75],V[v0804]^V[k45]);
		V[v0901] = V[v0701] ^ box_3_1 (V[v0817]^V[k55],V[v0816]^V[k67],V[v0815]^V[k76],V[v0814]^V[k06],V[v0813]^V[k05],V[v0812]^V[k37]);
		V[v0910] = V[v0710] ^ box_3_10(V[v0817]^V[k55],V[v0816]^V[k67],V[v0815]^V[k76],V[v0814]^V[k06],V[v0813]^V[k05],V[v0812]^V[k37]);
		V[v0920] = V[v0720] ^ box_3_20(V[v0817]^V[k55],V[v0816]^V[k67],V[v0815]^V[k76],V[v0814]^V[k06],V[v0813]^V[k05],V[v0812]^V[k37]);
		V[v0926] = V[v0726] ^ box_3_26(V[v0817]^V[k55],V[v0816]^V[k67],V[v0815]^V[k76],V[v0814]^V[k06],V[v0813]^V[k05],V[v0812]^V[k37]);
		V[v0904] = V[v0704] ^ box_5_4 (V[v0825]^V[k72],V[v0824]^V[k23],V[v0823]^V[k21],V[v0822]^V[k42],V[v0821]^V[k71],V[v0820]^V[k53]);
		V[v0911] = V[v0711] ^ box_5_11(V[v0825]^V[k72],V[v0824]^V[k23],V[v0823]^V[k21],V[v0822]^V[k42],V[v0821]^V[k71],V[v0820]^V[k53]);
		V[v0919] = V[v0719] ^ box_5_19(V[v0825]^V[k72],V[v0824]^V[k23],V[v0823]^V[k21],V[v0822]^V[k42],V[v0821]^V[k71],V[v0820]^V[k53]);
		V[v0929] = V[v0729] ^ box_5_29(V[v0825]^V[k72],V[v0824]^V[k23],V[v0823]^V[k21],V[v0822]^V[k42],V[v0821]^V[k71],V[v0820]^V[k53]);
		V[v0905] = V[v0705] ^ box_7_5 (V[v0801]^V[k33],V[v0800]^V[k63],V[v0831]^V[k34],V[v0830]^V[k52],V[v0829]^V[k51],V[v0828]^V[k11]);
		V[v0915] = V[v0715] ^ box_7_15(V[v0801]^V[k33],V[v0800]^V[k63],V[v0831]^V[k34],V[v0830]^V[k52],V[v0829]^V[k51],V[v0828]^V[k11]);
		V[v0921] = V[v0721] ^ box_7_21(V[v0801]^V[k33],V[v0800]^V[k63],V[v0831]^V[k34],V[v0830]^V[k52],V[v0829]^V[k51],V[v0828]^V[k11]);
		V[v0927] = V[v0727] ^ box_7_27(V[v0801]^V[k33],V[v0800]^V[k63],V[v0831]^V[k34],V[v0830]^V[k52],V[v0829]^V[k51],V[v0828]^V[k11]);

		if (opt_split)
			splitTree(V, v0900, 9);

		V[v1009] = V[v0809] ^ box_0_9 (V[v0905]^V[k05],V[v0904]^V[k45],V[v0903]^V[k16],V[v0902]^V[k67],V[v0901]^V[k07],V[v0900]^V[k37]);
		V[v1017] = V[v0817] ^ box_0_17(V[v0905]^V[k05],V[v0904]^V[k45],V[v0903]^V[k16],V[v0902]^V[k67],V[v0901]^V[k07],V[v0900]^V[k37]);
		V[v1023] = V[v0823] ^ box_0_23(V[v0905]^V[k05],V[v0904]^V[k45],V[v0903]^V[k16],V[v0902]^V[k67],V[v0901]^V[k07],V[v0900]^V[k37]);
		V[v1031] = V[v0831] ^ box_0_31(V[v0905]^V[k05],V[v0904]^V[k45],V[v0903]^V[k16],V[v0902]^V[k67],V[v0901]^V[k07],V[v0900]^V[k37]);
		V[v1006] = V[v0806] ^ box_2_6 (V[v0913]^V[k54],V[v0912]^V[k75],V[v0911]^V[k15],V[v0910]^V[k57],V[v0909]^V[k66],V[v0908]^V[k26]);
		V[v1016] = V[v0816] ^ box_2_16(V[v0913]^V[k54],V[v0912]^V[k75],V[v0911]^V[k15],V[v0910]^V[k57],V[v0909]^V[k66],V[v0908]^V[k26]);
		V[v1024] = V[v0824] ^ box_2_24(V[v0913]^V[k54],V[v0912]^V[k75],V[v0911]^V[k15],V[v0910]^V[k57],V[v0909]^V[k66],V[v0908]^V[k26]);
		V[v1030] = V[v0830] ^ box_2_30(V[v0913]^V[k54],V[v0912]^V[k75],V[v0911]^V[k15],V[v0910]^V[k57],V[v0909]^V[k66],V[v0908]^V[k26]);
		V[v1003] = V[v0803] ^ box_4_3 (V[v0921]^V[k63],V[v0920]^V[k62],V[v0919]^V[k04],V[v0918]^V[k23],V[v0917]^V[k12],V[v0916]^V[k41]);
		V[v1008] = V[v0808] ^ box_4_8 (V[v0921]^V[k63],V[v0920]^V[k62],V[v0919]^V[k04],V[v0918]^V[k23],V[v0917]^V[k12],V[v0916]^V[k41]);
		V[v1014] = V[v0814] ^ box_4_14(V[v0921]^V[k63],V[v0920]^V[k62],V[v0919]^V[k04],V[v0918]^V[k23],V[v0917]^V[k12],V[v0916]^V[k41]);
		V[v1025] = V[v0825] ^ box_4_25(V[v0921]^V[k63],V[v0920]^V[k62],V[v0919]^V[k04],V[v0918]^V[k23],V[v0917]^V[k12],V[v0916]^V[k41]);
		V[v1000] = V[v0800] ^ box_6_0 (V[v0929]^V[k02],V[v0928]^V[k34],V[v0927]^V[k53],V[v0926]^V[k61],V[v0925]^V[k42],V[v0924]^V[k11]);
		V[v1007] = V[v0807] ^ box_6_7 (V[v0929]^V[k02],V[v0928]^V[k34],V[v0927]^V[k53],V[v0926]^V[k61],V[v0925]^V[k42],V[v0924]^V[k11]);
		V[v1012] = V[v0812] ^ box_6_12(V[v0929]^V[k02],V[v0928]^V[k34],V[v0927]^V[k53],V[v0926]^V[k61],V[v0925]^V[k42],V[v0924]^V[k11]);
		V[v1022] = V[v0822] ^ box_6_22(V[v0929]^V[k02],V[v0928]^V[k34],V[v0927]^V[k53],V[v0926]^V[k61],V[v0925]^V[k42],V[v0924]^V[k11]);
		V[v1002] = V[v0802] ^ box_1_2 (V[v0909]^V[k77],V[v0908]^V[k46],V[v0907]^V[k74],V[v0906]^V[k27],V[v0905]^V[k55],V[v0904]^V[k25]);
		V[v1013] = V[v0813] ^ box_1_13(V[v0909]^V[k77],V[v0908]^V[k46],V[v0907]^V[k74],V[v0906]^V[k27],V[v0905]^V[k55],V[v0904]^V[k25]);
		V[v1018] = V[v0818] ^ box_1_18(V[v0909]^V[k77],V[v0908]^V[k46],V[v0907]^V[k74],V[v0906]^V[k27],V[v0905]^V[k55],V[v0904]^V[k25]);
		V[v1028] = V[v0828] ^ box_1_28(V[v0909]^V[k77],V[v0908]^V[k46],V[v0907]^V[k74],V[v0906]^V[k27],V[v0905]^V[k55],V[v0904]^V[k25]);
		V[v1001] = V[v0801] ^ box_3_1 (V[v0917]^V[k35],V[v0916]^V[k47],V[v0915]^V[k56],V[v0914]^V[k65],V[v0913]^V[k64],V[v0912]^V[k17]);
		V[v1010] = V[v0810] ^ box_3_10(V[v0917]^V[k35],V[v0916]^V[k47],V[v0915]^V[k56],V[v0914]^V[k65],V[v0913]^V[k64],V[v0912]^V[k17]);
		V[v1020] = V[v0820] ^ box_3_20(V[v0917]^V[k35],V[v0916]^V[k47],V[v0915]^V[k56],V[v0914]^V[k65],V[v0913]^V[k64],V[v0912]^V[k17]);
		V[v1026] = V[v0826] ^ box_3_26(V[v0917]^V[k35],V[v0916]^V[k47],V[v0915]^V[k56],V[v0914]^V[k65],V[v0913]^V[k64],V[v0912]^V[k17]);
		V[v1004] = V[v0804] ^ box_5_4 (V[v0925]^V[k52],V[v0924]^V[k03],V[v0923]^V[k01],V[v0922]^V[k22],V[v0921]^V[k51],V[v0920]^V[k33]);
		V[v1011] = V[v0811] ^ box_5_11(V[v0925]^V[k52],V[v0924]^V[k03],V[v0923]^V[k01],V[v0922]^V[k22],V[v0921]^V[k51],V[v0920]^V[k33]);
		V[v1019] = V[v0819] ^ box_5_19(V[v0925]^V[k52],V[v0924]^V[k03],V[v0923]^V[k01],V[v0922]^V[k22],V[v0921]^V[k51],V[v0920]^V[k33]);
		V[v1029] = V[v0829] ^ box_5_29(V[v0925]^V[k52],V[v0924]^V[k03],V[v0923]^V[k01],V[v0922]^V[k22],V[v0921]^V[k51],V[v0920]^V[k33]);
		V[v1005] = V[v0805] ^ box_7_5 (V[v0901]^V[k13],V[v0900]^V[k43],V[v0931]^V[k14],V[v0930]^V[k32],V[v0929]^V[k31],V[v0928]^V[k72]);
		V[v1015] = V[v0815] ^ box_7_15(V[v0901]^V[k13],V[v0900]^V[k43],V[v0931]^V[k14],V[v0930]^V[k32],V[v0929]^V[k31],V[v0928]^V[k72]);
		V[v1021] = V[v0821] ^ box_7_21(V[v0901]^V[k13],V[v0900]^V[k43],V[v0931]^V[k14],V[v0930]^V[k32],V[v0929]^V[k31],V[v0928]^V[k72]);
		V[v1027] = V[v0827] ^ box_7_27(V[v0901]^V[k13],V[v0900]^V[k43],V[v0931]^V[k14],V[v0930]^V[k32],V[v0929]^V[k31],V[v0928]^V[k72]);

		if (opt_split)
			splitTree(V, v1000, 10);

		V[v1109] = V[v0909] ^ box_0_9 (V[v1005]^V[k64],V[v1004]^V[k25],V[v1003]^V[k75],V[v1002]^V[k47],V[v1001]^V[k66],V[v1000]^V[k17]);
		V[v1117] = V[v0917] ^ box_0_17(V[v1005]^V[k64],V[v1004]^V[k25],V[v1003]^V[k75],V[v1002]^V[k47],V[v1001]^V[k66],V[v1000]^V[k17]);
		V[v1123] = V[v0923] ^ box_0_23(V[v1005]^V[k64],V[v1004]^V[k25],V[v1003]^V[k75],V[v1002]^V[k47],V[v1001]^V[k66],V[v1000]^V[k17]);
		V[v1131] = V[v0931] ^ box_0_31(V[v1005]^V[k64],V[v1004]^V[k25],V[v1003]^V[k75],V[v1002]^V[k47],V[v1001]^V[k66],V[v1000]^V[k17]);
		V[v1106] = V[v0906] ^ box_2_6 (V[v1013]^V[k77],V[v1012]^V[k55],V[v1011]^V[k74],V[v1010]^V[k37],V[v1009]^V[k46],V[v1008]^V[k06]);
		V[v1116] = V[v0916] ^ box_2_16(V[v1013]^V[k77],V[v1012]^V[k55],V[v1011]^V[k74],V[v1010]^V[k37],V[v1009]^V[k46],V[v1008]^V[k06]);
		V[v1124] = V[v0924] ^ box_2_24(V[v1013]^V[k77],V[v1012]^V[k55],V[v1011]^V[k74],V[v1010]^V[k37],V[v1009]^V[k46],V[v1008]^V[k06]);
		V[v1130] = V[v0930] ^ box_2_30(V[v1013]^V[k77],V[v1012]^V[k55],V[v1011]^V[k74],V[v1010]^V[k37],V[v1009]^V[k46],V[v1008]^V[k06]);
		V[v1103] = V[v0903] ^ box_4_3 (V[v1021]^V[k43],V[v1020]^V[k42],V[v1019]^V[k61],V[v1018]^V[k03],V[v1017]^V[k73],V[v1016]^V[k21]);
		V[v1108] = V[v0908] ^ box_4_8 (V[v1021]^V[k43],V[v1020]^V[k42],V[v1019]^V[k61],V[v1018]^V[k03],V[v1017]^V[k73],V[v1016]^V[k21]);
		V[v1114] = V[v0914] ^ box_4_14(V[v1021]^V[k43],V[v1020]^V[k42],V[v1019]^V[k61],V[v1018]^V[k03],V[v1017]^V[k73],V[v1016]^V[k21]);
		V[v1125] = V[v0925] ^ box_4_25(V[v1021]^V[k43],V[v1020]^V[k42],V[v1019]^V[k61],V[v1018]^V[k03],V[v1017]^V[k73],V[v1016]^V[k21]);
		V[v1100] = V[v0900] ^ box_6_0 (V[v1029]^V[k63],V[v1028]^V[k14],V[v1027]^V[k33],V[v1026]^V[k41],V[v1025]^V[k22],V[v1024]^V[k72]);
		V[v1107] = V[v0907] ^ box_6_7 (V[v1029]^V[k63],V[v1028]^V[k14],V[v1027]^V[k33],V[v1026]^V[k41],V[v1025]^V[k22],V[v1024]^V[k72]);
		V[v1112] = V[v0912] ^ box_6_12(V[v1029]^V[k63],V[v1028]^V[k14],V[v1027]^V[k33],V[v1026]^V[k41],V[v1025]^V[k22],V[v1024]^V[k72]);
		V[v1122] = V[v0922] ^ box_6_22(V[v1029]^V[k63],V[v1028]^V[k14],V[v1027]^V[k33],V[v1026]^V[k41],V[v1025]^V[k22],V[v1024]^V[k72]);
		V[v1102] = V[v0902] ^ box_1_2 (V[v1009]^V[k57],V[v1008]^V[k26],V[v1007]^V[k54],V[v1006]^V[k07],V[v1005]^V[k35],V[v1004]^V[k05]);
		V[v1113] = V[v0913] ^ box_1_13(V[v1009]^V[k57],V[v1008]^V[k26],V[v1007]^V[k54],V[v1006]^V[k07],V[v1005]^V[k35],V[v1004]^V[k05]);
		V[v1118] = V[v0918] ^ box_1_18(V[v1009]^V[k57],V[v1008]^V[k26],V[v1007]^V[k54],V[v1006]^V[k07],V[v1005]^V[k35],V[v1004]^V[k05]);
		V[v1128] = V[v0928] ^ box_1_28(V[v1009]^V[k57],V[v1008]^V[k26],V[v1007]^V[k54],V[v1006]^V[k07],V[v1005]^V[k35],V[v1004]^V[k05]);
		V[v1101] = V[v0901] ^ box_3_1 (V[v1017]^V[k15],V[v1016]^V[k27],V[v1015]^V[k36],V[v1014]^V[k45],V[v1013]^V[k44],V[v1012]^V[k76]);
		V[v1110] = V[v0910] ^ box_3_10(V[v1017]^V[k15],V[v1016]^V[k27],V[v1015]^V[k36],V[v1014]^V[k45],V[v1013]^V[k44],V[v1012]^V[k76]);
		V[v1120] = V[v0920] ^ box_3_20(V[v1017]^V[k15],V[v1016]^V[k27],V[v1015]^V[k36],V[v1014]^V[k45],V[v1013]^V[k44],V[v1012]^V[k76]);
		V[v1126] = V[v0926] ^ box_3_26(V[v1017]^V[k15],V[v1016]^V[k27],V[v1015]^V[k36],V[v1014]^V[k45],V[v1013]^V[k44],V[v1012]^V[k76]);
		V[v1104] = V[v0904] ^ box_5_4 (V[v1025]^V[k32],V[v1024]^V[k24],V[v1023]^V[k62],V[v1022]^V[k02],V[v1021]^V[k31],V[v1020]^V[k13]);
		V[v1111] = V[v0911] ^ box_5_11(V[v1025]^V[k32],V[v1024]^V[k24],V[v1023]^V[k62],V[v1022]^V[k02],V[v1021]^V[k31],V[v1020]^V[k13]);
		V[v1119] = V[v0919] ^ box_5_19(V[v1025]^V[k32],V[v1024]^V[k24],V[v1023]^V[k62],V[v1022]^V[k02],V[v1021]^V[k31],V[v1020]^V[k13]);
		V[v1129] = V[v0929] ^ box_5_29(V[v1025]^V[k32],V[v1024]^V[k24],V[v1023]^V[k62],V[v1022]^V[k02],V[v1021]^V[k31],V[v1020]^V[k13]);
		V[v1105] = V[v0905] ^ box_7_5 (V[v1001]^V[k34],V[v1000]^V[k23],V[v1031]^V[k71],V[v1030]^V[k12],V[v1029]^V[k11],V[v1028]^V[k52]);
		V[v1115] = V[v0915] ^ box_7_15(V[v1001]^V[k34],V[v1000]^V[k23],V[v1031]^V[k71],V[v1030]^V[k12],V[v1029]^V[k11],V[v1028]^V[k52]);
		V[v1121] = V[v0921] ^ box_7_21(V[v1001]^V[k34],V[v1000]^V[k23],V[v1031]^V[k71],V[v1030]^V[k12],V[v1029]^V[k11],V[v1028]^V[k52]);
		V[v1127] = V[v0927] ^ box_7_27(V[v1001]^V[k34],V[v1000]^V[k23],V[v1031]^V[k71],V[v1030]^V[k12],V[v1029]^V[k11],V[v1028]^V[k52]);

		if (opt_split)
			splitTree(V, v1100, 11);

		V[v1209] = V[v1009] ^ box_0_9 (V[v1105]^V[k44],V[v1104]^V[k05],V[v1103]^V[k55],V[v1102]^V[k27],V[v1101]^V[k46],V[v1100]^V[k76]);
		V[v1217] = V[v1017] ^ box_0_17(V[v1105]^V[k44],V[v1104]^V[k05],V[v1103]^V[k55],V[v1102]^V[k27],V[v1101]^V[k46],V[v1100]^V[k76]);
		V[v1223] = V[v1023] ^ box_0_23(V[v1105]^V[k44],V[v1104]^V[k05],V[v1103]^V[k55],V[v1102]^V[k27],V[v1101]^V[k46],V[v1100]^V[k76]);
		V[v1231] = V[v1031] ^ box_0_31(V[v1105]^V[k44],V[v1104]^V[k05],V[v1103]^V[k55],V[v1102]^V[k27],V[v1101]^V[k46],V[v1100]^V[k76]);
		V[v1206] = V[v1006] ^ box_2_6 (V[v1113]^V[k57],V[v1112]^V[k35],V[v1111]^V[k54],V[v1110]^V[k17],V[v1109]^V[k26],V[v1108]^V[k65]);
		V[v1216] = V[v1016] ^ box_2_16(V[v1113]^V[k57],V[v1112]^V[k35],V[v1111]^V[k54],V[v1110]^V[k17],V[v1109]^V[k26],V[v1108]^V[k65]);
		V[v1224] = V[v1024] ^ box_2_24(V[v1113]^V[k57],V[v1112]^V[k35],V[v1111]^V[k54],V[v1110]^V[k17],V[v1109]^V[k26],V[v1108]^V[k65]);
		V[v1230] = V[v1030] ^ box_2_30(V[v1113]^V[k57],V[v1112]^V[k35],V[v1111]^V[k54],V[v1110]^V[k17],V[v1109]^V[k26],V[v1108]^V[k65]);
		V[v1203] = V[v1003] ^ box_4_3 (V[v1121]^V[k23],V[v1120]^V[k22],V[v1119]^V[k41],V[v1118]^V[k24],V[v1117]^V[k53],V[v1116]^V[k01]);
		V[v1208] = V[v1008] ^ box_4_8 (V[v1121]^V[k23],V[v1120]^V[k22],V[v1119]^V[k41],V[v1118]^V[k24],V[v1117]^V[k53],V[v1116]^V[k01]);
		V[v1214] = V[v1014] ^ box_4_14(V[v1121]^V[k23],V[v1120]^V[k22],V[v1119]^V[k41],V[v1118]^V[k24],V[v1117]^V[k53],V[v1116]^V[k01]);
		V[v1225] = V[v1025] ^ box_4_25(V[v1121]^V[k23],V[v1120]^V[k22],V[v1119]^V[k41],V[v1118]^V[k24],V[v1117]^V[k53],V[v1116]^V[k01]);
		V[v1200] = V[v1000] ^ box_6_0 (V[v1129]^V[k43],V[v1128]^V[k71],V[v1127]^V[k13],V[v1126]^V[k21],V[v1125]^V[k02],V[v1124]^V[k52]);
		V[v1207] = V[v1007] ^ box_6_7 (V[v1129]^V[k43],V[v1128]^V[k71],V[v1127]^V[k13],V[v1126]^V[k21],V[v1125]^V[k02],V[v1124]^V[k52]);
		V[v1212] = V[v1012] ^ box_6_12(V[v1129]^V[k43],V[v1128]^V[k71],V[v1127]^V[k13],V[v1126]^V[k21],V[v1125]^V[k02],V[v1124]^V[k52]);
		V[v1222] = V[v1022] ^ box_6_22(V[v1129]^V[k43],V[v1128]^V[k71],V[v1127]^V[k13],V[v1126]^V[k21],V[v1125]^V[k02],V[v1124]^V[k52]);
		V[v1202] = V[v1002] ^ box_1_2 (V[v1109]^V[k37],V[v1108]^V[k06],V[v1107]^V[k77],V[v1106]^V[k66],V[v1105]^V[k15],V[v1104]^V[k64]);
		V[v1213] = V[v1013] ^ box_1_13(V[v1109]^V[k37],V[v1108]^V[k06],V[v1107]^V[k77],V[v1106]^V[k66],V[v1105]^V[k15],V[v1104]^V[k64]);
		V[v1218] = V[v1018] ^ box_1_18(V[v1109]^V[k37],V[v1108]^V[k06],V[v1107]^V[k77],V[v1106]^V[k66],V[v1105]^V[k15],V[v1104]^V[k64]);
		V[v1228] = V[v1028] ^ box_1_28(V[v1109]^V[k37],V[v1108]^V[k06],V[v1107]^V[k77],V[v1106]^V[k66],V[v1105]^V[k15],V[v1104]^V[k64]);
		V[v1201] = V[v1001] ^ box_3_1 (V[v1117]^V[k74],V[v1116]^V[k07],V[v1115]^V[k16],V[v1114]^V[k25],V[v1113]^V[k67],V[v1112]^V[k56]);
		V[v1210] = V[v1010] ^ box_3_10(V[v1117]^V[k74],V[v1116]^V[k07],V[v1115]^V[k16],V[v1114]^V[k25],V[v1113]^V[k67],V[v1112]^V[k56]);
		V[v1220] = V[v1020] ^ box_3_20(V[v1117]^V[k74],V[v1116]^V[k07],V[v1115]^V[k16],V[v1114]^V[k25],V[v1113]^V[k67],V[v1112]^V[k56]);
		V[v1226] = V[v1026] ^ box_3_26(V[v1117]^V[k74],V[v1116]^V[k07],V[v1115]^V[k16],V[v1114]^V[k25],V[v1113]^V[k67],V[v1112]^V[k56]);
		V[v1204] = V[v1004] ^ box_5_4 (V[v1125]^V[k12],V[v1124]^V[k04],V[v1123]^V[k42],V[v1122]^V[k63],V[v1121]^V[k11],V[v1120]^V[k34]);
		V[v1211] = V[v1011] ^ box_5_11(V[v1125]^V[k12],V[v1124]^V[k04],V[v1123]^V[k42],V[v1122]^V[k63],V[v1121]^V[k11],V[v1120]^V[k34]);
		V[v1219] = V[v1019] ^ box_5_19(V[v1125]^V[k12],V[v1124]^V[k04],V[v1123]^V[k42],V[v1122]^V[k63],V[v1121]^V[k11],V[v1120]^V[k34]);
		V[v1229] = V[v1029] ^ box_5_29(V[v1125]^V[k12],V[v1124]^V[k04],V[v1123]^V[k42],V[v1122]^V[k63],V[v1121]^V[k11],V[v1120]^V[k34]);
		V[v1205] = V[v1005] ^ box_7_5 (V[v1101]^V[k14],V[v1100]^V[k03],V[v1131]^V[k51],V[v1130]^V[k73],V[v1129]^V[k72],V[v1128]^V[k32]);
		V[v1215] = V[v1015] ^ box_7_15(V[v1101]^V[k14],V[v1100]^V[k03],V[v1131]^V[k51],V[v1130]^V[k73],V[v1129]^V[k72],V[v1128]^V[k32]);
		V[v1221] = V[v1021] ^ box_7_21(V[v1101]^V[k14],V[v1100]^V[k03],V[v1131]^V[k51],V[v1130]^V[k73],V[v1129]^V[k72],V[v1128]^V[k32]);
		V[v1227] = V[v1027] ^ box_7_27(V[v1101]^V[k14],V[v1100]^V[k03],V[v1131]^V[k51],V[v1130]^V[k73],V[v1129]^V[k72],V[v1128]^V[k32]);

		if (opt_split)
			splitTree(V, v1200, 12);

		V[v1309] = V[v1109] ^ box_0_9 (V[v1205]^V[k67],V[v1204]^V[k64],V[v1203]^V[k35],V[v1202]^V[k07],V[v1201]^V[k26],V[v1200]^V[k56]);
		V[v1317] = V[v1117] ^ box_0_17(V[v1205]^V[k67],V[v1204]^V[k64],V[v1203]^V[k35],V[v1202]^V[k07],V[v1201]^V[k26],V[v1200]^V[k56]);
		V[v1323] = V[v1123] ^ box_0_23(V[v1205]^V[k67],V[v1204]^V[k64],V[v1203]^V[k35],V[v1202]^V[k07],V[v1201]^V[k26],V[v1200]^V[k56]);
		V[v1331] = V[v1131] ^ box_0_31(V[v1205]^V[k67],V[v1204]^V[k64],V[v1203]^V[k35],V[v1202]^V[k07],V[v1201]^V[k26],V[v1200]^V[k56]);
		V[v1306] = V[v1106] ^ box_2_6 (V[v1213]^V[k37],V[v1212]^V[k15],V[v1211]^V[k77],V[v1210]^V[k76],V[v1209]^V[k06],V[v1208]^V[k45]);
		V[v1316] = V[v1116] ^ box_2_16(V[v1213]^V[k37],V[v1212]^V[k15],V[v1211]^V[k77],V[v1210]^V[k76],V[v1209]^V[k06],V[v1208]^V[k45]);
		V[v1324] = V[v1124] ^ box_2_24(V[v1213]^V[k37],V[v1212]^V[k15],V[v1211]^V[k77],V[v1210]^V[k76],V[v1209]^V[k06],V[v1208]^V[k45]);
		V[v1330] = V[v1130] ^ box_2_30(V[v1213]^V[k37],V[v1212]^V[k15],V[v1211]^V[k77],V[v1210]^V[k76],V[v1209]^V[k06],V[v1208]^V[k45]);
		V[v1303] = V[v1103] ^ box_4_3 (V[v1221]^V[k03],V[v1220]^V[k02],V[v1219]^V[k21],V[v1218]^V[k04],V[v1217]^V[k33],V[v1216]^V[k62]);
		V[v1308] = V[v1108] ^ box_4_8 (V[v1221]^V[k03],V[v1220]^V[k02],V[v1219]^V[k21],V[v1218]^V[k04],V[v1217]^V[k33],V[v1216]^V[k62]);
		V[v1314] = V[v1114] ^ box_4_14(V[v1221]^V[k03],V[v1220]^V[k02],V[v1219]^V[k21],V[v1218]^V[k04],V[v1217]^V[k33],V[v1216]^V[k62]);
		V[v1325] = V[v1125] ^ box_4_25(V[v1221]^V[k03],V[v1220]^V[k02],V[v1219]^V[k21],V[v1218]^V[k04],V[v1217]^V[k33],V[v1216]^V[k62]);
		V[v1300] = V[v1100] ^ box_6_0 (V[v1229]^V[k23],V[v1228]^V[k51],V[v1227]^V[k34],V[v1226]^V[k01],V[v1225]^V[k63],V[v1224]^V[k32]);
		V[v1307] = V[v1107] ^ box_6_7 (V[v1229]^V[k23],V[v1228]^V[k51],V[v1227]^V[k34],V[v1226]^V[k01],V[v1225]^V[k63],V[v1224]^V[k32]);
		V[v1312] = V[v1112] ^ box_6_12(V[v1229]^V[k23],V[v1228]^V[k51],V[v1227]^V[k34],V[v1226]^V[k01],V[v1225]^V[k63],V[v1224]^V[k32]);
		V[v1322] = V[v1122] ^ box_6_22(V[v1229]^V[k23],V[v1228]^V[k51],V[v1227]^V[k34],V[v1226]^V[k01],V[v1225]^V[k63],V[v1224]^V[k32]);
		V[v1302] = V[v1102] ^ box_1_2 (V[v1209]^V[k17],V[v1208]^V[k65],V[v1207]^V[k57],V[v1206]^V[k46],V[v1205]^V[k74],V[v1204]^V[k44]);
		V[v1313] = V[v1113] ^ box_1_13(V[v1209]^V[k17],V[v1208]^V[k65],V[v1207]^V[k57],V[v1206]^V[k46],V[v1205]^V[k74],V[v1204]^V[k44]);
		V[v1318] = V[v1118] ^ box_1_18(V[v1209]^V[k17],V[v1208]^V[k65],V[v1207]^V[k57],V[v1206]^V[k46],V[v1205]^V[k74],V[v1204]^V[k44]);
		V[v1328] = V[v1128] ^ box_1_28(V[v1209]^V[k17],V[v1208]^V[k65],V[v1207]^V[k57],V[v1206]^V[k46],V[v1205]^V[k74],V[v1204]^V[k44]);
		V[v1301] = V[v1101] ^ box_3_1 (V[v1217]^V[k54],V[v1216]^V[k66],V[v1215]^V[k75],V[v1214]^V[k05],V[v1213]^V[k47],V[v1212]^V[k36]);
		V[v1310] = V[v1110] ^ box_3_10(V[v1217]^V[k54],V[v1216]^V[k66],V[v1215]^V[k75],V[v1214]^V[k05],V[v1213]^V[k47],V[v1212]^V[k36]);
		V[v1320] = V[v1120] ^ box_3_20(V[v1217]^V[k54],V[v1216]^V[k66],V[v1215]^V[k75],V[v1214]^V[k05],V[v1213]^V[k47],V[v1212]^V[k36]);
		V[v1326] = V[v1126] ^ box_3_26(V[v1217]^V[k54],V[v1216]^V[k66],V[v1215]^V[k75],V[v1214]^V[k05],V[v1213]^V[k47],V[v1212]^V[k36]);
		V[v1304] = V[v1104] ^ box_5_4 (V[v1225]^V[k73],V[v1224]^V[k61],V[v1223]^V[k22],V[v1222]^V[k43],V[v1221]^V[k72],V[v1220]^V[k14]);
		V[v1311] = V[v1111] ^ box_5_11(V[v1225]^V[k73],V[v1224]^V[k61],V[v1223]^V[k22],V[v1222]^V[k43],V[v1221]^V[k72],V[v1220]^V[k14]);
		V[v1319] = V[v1119] ^ box_5_19(V[v1225]^V[k73],V[v1224]^V[k61],V[v1223]^V[k22],V[v1222]^V[k43],V[v1221]^V[k72],V[v1220]^V[k14]);
		V[v1329] = V[v1129] ^ box_5_29(V[v1225]^V[k73],V[v1224]^V[k61],V[v1223]^V[k22],V[v1222]^V[k43],V[v1221]^V[k72],V[v1220]^V[k14]);
		V[v1305] = V[v1105] ^ box_7_5 (V[v1201]^V[k71],V[v1200]^V[k24],V[v1231]^V[k31],V[v1230]^V[k53],V[v1229]^V[k52],V[v1228]^V[k12]);
		V[v1315] = V[v1115] ^ box_7_15(V[v1201]^V[k71],V[v1200]^V[k24],V[v1231]^V[k31],V[v1230]^V[k53],V[v1229]^V[k52],V[v1228]^V[k12]);
		V[v1321] = V[v1121] ^ box_7_21(V[v1201]^V[k71],V[v1200]^V[k24],V[v1231]^V[k31],V[v1230]^V[k53],V[v1229]^V[k52],V[v1228]^V[k12]);
		V[v1327] = V[v1127] ^ box_7_27(V[v1201]^V[k71],V[v1200]^V[k24],V[v1231]^V[k31],V[v1230]^V[k53],V[v1229]^V[k52],V[v1228]^V[k12]);

		if (opt_split)
			splitTree(V, v1300, 13);

		V[v1409] = V[v1209] ^ box_0_9 (V[v1305]^V[k47],V[v1304]^V[k44],V[v1303]^V[k15],V[v1302]^V[k66],V[v1301]^V[k06],V[v1300]^V[k36]);
		V[v1417] = V[v1217] ^ box_0_17(V[v1305]^V[k47],V[v1304]^V[k44],V[v1303]^V[k15],V[v1302]^V[k66],V[v1301]^V[k06],V[v1300]^V[k36]);
		V[v1423] = V[v1223] ^ box_0_23(V[v1305]^V[k47],V[v1304]^V[k44],V[v1303]^V[k15],V[v1302]^V[k66],V[v1301]^V[k06],V[v1300]^V[k36]);
		V[v1431] = V[v1231] ^ box_0_31(V[v1305]^V[k47],V[v1304]^V[k44],V[v1303]^V[k15],V[v1302]^V[k66],V[v1301]^V[k06],V[v1300]^V[k36]);
		V[v1406] = V[v1206] ^ box_2_6 (V[v1313]^V[k17],V[v1312]^V[k74],V[v1311]^V[k57],V[v1310]^V[k56],V[v1309]^V[k65],V[v1308]^V[k25]);
		V[v1416] = V[v1216] ^ box_2_16(V[v1313]^V[k17],V[v1312]^V[k74],V[v1311]^V[k57],V[v1310]^V[k56],V[v1309]^V[k65],V[v1308]^V[k25]);
		V[v1424] = V[v1224] ^ box_2_24(V[v1313]^V[k17],V[v1312]^V[k74],V[v1311]^V[k57],V[v1310]^V[k56],V[v1309]^V[k65],V[v1308]^V[k25]);
		V[v1430] = V[v1230] ^ box_2_30(V[v1313]^V[k17],V[v1312]^V[k74],V[v1311]^V[k57],V[v1310]^V[k56],V[v1309]^V[k65],V[v1308]^V[k25]);
		V[v1403] = V[v1203] ^ box_4_3 (V[v1321]^V[k24],V[v1320]^V[k63],V[v1319]^V[k01],V[v1318]^V[k61],V[v1317]^V[k13],V[v1316]^V[k42]);
		V[v1408] = V[v1208] ^ box_4_8 (V[v1321]^V[k24],V[v1320]^V[k63],V[v1319]^V[k01],V[v1318]^V[k61],V[v1317]^V[k13],V[v1316]^V[k42]);
		V[v1414] = V[v1214] ^ box_4_14(V[v1321]^V[k24],V[v1320]^V[k63],V[v1319]^V[k01],V[v1318]^V[k61],V[v1317]^V[k13],V[v1316]^V[k42]);
		V[v1425] = V[v1225] ^ box_4_25(V[v1321]^V[k24],V[v1320]^V[k63],V[v1319]^V[k01],V[v1318]^V[k61],V[v1317]^V[k13],V[v1316]^V[k42]);
		V[v1400] = V[v1200] ^ box_6_0 (V[v1329]^V[k03],V[v1328]^V[k31],V[v1327]^V[k14],V[v1326]^V[k62],V[v1325]^V[k43],V[v1324]^V[k12]);
		V[v1407] = V[v1207] ^ box_6_7 (V[v1329]^V[k03],V[v1328]^V[k31],V[v1327]^V[k14],V[v1326]^V[k62],V[v1325]^V[k43],V[v1324]^V[k12]);
		V[v1412] = V[v1212] ^ box_6_12(V[v1329]^V[k03],V[v1328]^V[k31],V[v1327]^V[k14],V[v1326]^V[k62],V[v1325]^V[k43],V[v1324]^V[k12]);
		V[v1422] = V[v1222] ^ box_6_22(V[v1329]^V[k03],V[v1328]^V[k31],V[v1327]^V[k14],V[v1326]^V[k62],V[v1325]^V[k43],V[v1324]^V[k12]);
		V[v1402] = V[v1202] ^ box_1_2 (V[v1309]^V[k76],V[v1308]^V[k45],V[v1307]^V[k37],V[v1306]^V[k26],V[v1305]^V[k54],V[v1304]^V[k67]);
		V[v1413] = V[v1213] ^ box_1_13(V[v1309]^V[k76],V[v1308]^V[k45],V[v1307]^V[k37],V[v1306]^V[k26],V[v1305]^V[k54],V[v1304]^V[k67]);
		V[v1418] = V[v1218] ^ box_1_18(V[v1309]^V[k76],V[v1308]^V[k45],V[v1307]^V[k37],V[v1306]^V[k26],V[v1305]^V[k54],V[v1304]^V[k67]);
		V[v1428] = V[v1228] ^ box_1_28(V[v1309]^V[k76],V[v1308]^V[k45],V[v1307]^V[k37],V[v1306]^V[k26],V[v1305]^V[k54],V[v1304]^V[k67]);
		V[v1401] = V[v1201] ^ box_3_1 (V[v1317]^V[k77],V[v1316]^V[k46],V[v1315]^V[k55],V[v1314]^V[k64],V[v1313]^V[k27],V[v1312]^V[k16]);
		V[v1410] = V[v1210] ^ box_3_10(V[v1317]^V[k77],V[v1316]^V[k46],V[v1315]^V[k55],V[v1314]^V[k64],V[v1313]^V[k27],V[v1312]^V[k16]);
		V[v1420] = V[v1220] ^ box_3_20(V[v1317]^V[k77],V[v1316]^V[k46],V[v1315]^V[k55],V[v1314]^V[k64],V[v1313]^V[k27],V[v1312]^V[k16]);
		V[v1426] = V[v1226] ^ box_3_26(V[v1317]^V[k77],V[v1316]^V[k46],V[v1315]^V[k55],V[v1314]^V[k64],V[v1313]^V[k27],V[v1312]^V[k16]);
		V[v1404] = V[v1204] ^ box_5_4 (V[v1325]^V[k53],V[v1324]^V[k41],V[v1323]^V[k02],V[v1322]^V[k23],V[v1321]^V[k52],V[v1320]^V[k71]);
		V[v1411] = V[v1211] ^ box_5_11(V[v1325]^V[k53],V[v1324]^V[k41],V[v1323]^V[k02],V[v1322]^V[k23],V[v1321]^V[k52],V[v1320]^V[k71]);
		V[v1419] = V[v1219] ^ box_5_19(V[v1325]^V[k53],V[v1324]^V[k41],V[v1323]^V[k02],V[v1322]^V[k23],V[v1321]^V[k52],V[v1320]^V[k71]);
		V[v1429] = V[v1229] ^ box_5_29(V[v1325]^V[k53],V[v1324]^V[k41],V[v1323]^V[k02],V[v1322]^V[k23],V[v1321]^V[k52],V[v1320]^V[k71]);
		V[v1405] = V[v1205] ^ box_7_5 (V[v1301]^V[k51],V[v1300]^V[k04],V[v1331]^V[k11],V[v1330]^V[k33],V[v1329]^V[k32],V[v1328]^V[k73]);
		V[v1415] = V[v1215] ^ box_7_15(V[v1301]^V[k51],V[v1300]^V[k04],V[v1331]^V[k11],V[v1330]^V[k33],V[v1329]^V[k32],V[v1328]^V[k73]);
		V[v1421] = V[v1221] ^ box_7_21(V[v1301]^V[k51],V[v1300]^V[k04],V[v1331]^V[k11],V[v1330]^V[k33],V[v1329]^V[k32],V[v1328]^V[k73]);
		V[v1427] = V[v1227] ^ box_7_27(V[v1301]^V[k51],V[v1300]^V[k04],V[v1331]^V[k11],V[v1330]^V[k33],V[v1329]^V[k32],V[v1328]^V[k73]);

		if (opt_split)
			splitTree(V, v1400, 14);

		V[v1509] = V[v1309] ^ box_0_9 (V[v1405]^V[k37],V[v1404]^V[k77],V[v1403]^V[k05],V[v1402]^V[k56],V[v1401]^V[k75],V[v1400]^V[k26]);
		V[v1517] = V[v1317] ^ box_0_17(V[v1405]^V[k37],V[v1404]^V[k77],V[v1403]^V[k05],V[v1402]^V[k56],V[v1401]^V[k75],V[v1400]^V[k26]);
		V[v1523] = V[v1323] ^ box_0_23(V[v1405]^V[k37],V[v1404]^V[k77],V[v1403]^V[k05],V[v1402]^V[k56],V[v1401]^V[k75],V[v1400]^V[k26]);
		V[v1531] = V[v1331] ^ box_0_31(V[v1405]^V[k37],V[v1404]^V[k77],V[v1403]^V[k05],V[v1402]^V[k56],V[v1401]^V[k75],V[v1400]^V[k26]);
		V[v1506] = V[v1306] ^ box_2_6 (V[v1413]^V[k07],V[v1412]^V[k64],V[v1411]^V[k47],V[v1410]^V[k46],V[v1409]^V[k55],V[v1408]^V[k15]);
		V[v1516] = V[v1316] ^ box_2_16(V[v1413]^V[k07],V[v1412]^V[k64],V[v1411]^V[k47],V[v1410]^V[k46],V[v1409]^V[k55],V[v1408]^V[k15]);
		V[v1524] = V[v1324] ^ box_2_24(V[v1413]^V[k07],V[v1412]^V[k64],V[v1411]^V[k47],V[v1410]^V[k46],V[v1409]^V[k55],V[v1408]^V[k15]);
		V[v1530] = V[v1330] ^ box_2_30(V[v1413]^V[k07],V[v1412]^V[k64],V[v1411]^V[k47],V[v1410]^V[k46],V[v1409]^V[k55],V[v1408]^V[k15]);
		V[v1503] = V[v1303] ^ box_4_3 (V[v1421]^V[k14],V[v1420]^V[k53],V[v1419]^V[k72],V[v1418]^V[k51],V[v1417]^V[k03],V[v1416]^V[k32]);
		V[v1508] = V[v1308] ^ box_4_8 (V[v1421]^V[k14],V[v1420]^V[k53],V[v1419]^V[k72],V[v1418]^V[k51],V[v1417]^V[k03],V[v1416]^V[k32]);
		V[v1514] = V[v1314] ^ box_4_14(V[v1421]^V[k14],V[v1420]^V[k53],V[v1419]^V[k72],V[v1418]^V[k51],V[v1417]^V[k03],V[v1416]^V[k32]);
		V[v1525] = V[v1325] ^ box_4_25(V[v1421]^V[k14],V[v1420]^V[k53],V[v1419]^V[k72],V[v1418]^V[k51],V[v1417]^V[k03],V[v1416]^V[k32]);
		V[v1500] = V[v1300] ^ box_6_0 (V[v1429]^V[k34],V[v1428]^V[k21],V[v1427]^V[k04],V[v1426]^V[k52],V[v1425]^V[k33],V[v1424]^V[k02]);
		V[v1507] = V[v1307] ^ box_6_7 (V[v1429]^V[k34],V[v1428]^V[k21],V[v1427]^V[k04],V[v1426]^V[k52],V[v1425]^V[k33],V[v1424]^V[k02]);
		V[v1512] = V[v1312] ^ box_6_12(V[v1429]^V[k34],V[v1428]^V[k21],V[v1427]^V[k04],V[v1426]^V[k52],V[v1425]^V[k33],V[v1424]^V[k02]);
		V[v1522] = V[v1322] ^ box_6_22(V[v1429]^V[k34],V[v1428]^V[k21],V[v1427]^V[k04],V[v1426]^V[k52],V[v1425]^V[k33],V[v1424]^V[k02]);
		V[v1502] = V[v1302] ^ box_1_2 (V[v1409]^V[k66],V[v1408]^V[k35],V[v1407]^V[k27],V[v1406]^V[k16],V[v1405]^V[k44],V[v1404]^V[k57]);
		V[v1513] = V[v1313] ^ box_1_13(V[v1409]^V[k66],V[v1408]^V[k35],V[v1407]^V[k27],V[v1406]^V[k16],V[v1405]^V[k44],V[v1404]^V[k57]);
		V[v1518] = V[v1318] ^ box_1_18(V[v1409]^V[k66],V[v1408]^V[k35],V[v1407]^V[k27],V[v1406]^V[k16],V[v1405]^V[k44],V[v1404]^V[k57]);
		V[v1528] = V[v1328] ^ box_1_28(V[v1409]^V[k66],V[v1408]^V[k35],V[v1407]^V[k27],V[v1406]^V[k16],V[v1405]^V[k44],V[v1404]^V[k57]);
		V[v1501] = V[v1301] ^ box_3_1 (V[v1417]^V[k67],V[v1416]^V[k36],V[v1415]^V[k45],V[v1414]^V[k54],V[v1413]^V[k17],V[v1412]^V[k06]);
		V[v1510] = V[v1310] ^ box_3_10(V[v1417]^V[k67],V[v1416]^V[k36],V[v1415]^V[k45],V[v1414]^V[k54],V[v1413]^V[k17],V[v1412]^V[k06]);
		V[v1520] = V[v1320] ^ box_3_20(V[v1417]^V[k67],V[v1416]^V[k36],V[v1415]^V[k45],V[v1414]^V[k54],V[v1413]^V[k17],V[v1412]^V[k06]);
		V[v1526] = V[v1326] ^ box_3_26(V[v1417]^V[k67],V[v1416]^V[k36],V[v1415]^V[k45],V[v1414]^V[k54],V[v1413]^V[k17],V[v1412]^V[k06]);
		V[v1504] = V[v1304] ^ box_5_4 (V[v1425]^V[k43],V[v1424]^V[k31],V[v1423]^V[k73],V[v1422]^V[k13],V[v1421]^V[k42],V[v1420]^V[k61]);
		V[v1511] = V[v1311] ^ box_5_11(V[v1425]^V[k43],V[v1424]^V[k31],V[v1423]^V[k73],V[v1422]^V[k13],V[v1421]^V[k42],V[v1420]^V[k61]);
		V[v1519] = V[v1319] ^ box_5_19(V[v1425]^V[k43],V[v1424]^V[k31],V[v1423]^V[k73],V[v1422]^V[k13],V[v1421]^V[k42],V[v1420]^V[k61]);
		V[v1529] = V[v1329] ^ box_5_29(V[v1425]^V[k43],V[v1424]^V[k31],V[v1423]^V[k73],V[v1422]^V[k13],V[v1421]^V[k42],V[v1420]^V[k61]);
		V[v1505] = V[v1305] ^ box_7_5 (V[v1401]^V[k41],V[v1400]^V[k71],V[v1431]^V[k01],V[v1430]^V[k23],V[v1429]^V[k22],V[v1428]^V[k63]);
		V[v1515] = V[v1315] ^ box_7_15(V[v1401]^V[k41],V[v1400]^V[k71],V[v1431]^V[k01],V[v1430]^V[k23],V[v1429]^V[k22],V[v1428]^V[k63]);
		V[v1521] = V[v1321] ^ box_7_21(V[v1401]^V[k41],V[v1400]^V[k71],V[v1431]^V[k01],V[v1430]^V[k23],V[v1429]^V[k22],V[v1428]^V[k63]);
		V[v1527] = V[v1327] ^ box_7_27(V[v1401]^V[k41],V[v1400]^V[k71],V[v1431]^V[k01],V[v1430]^V[k23],V[v1429]^V[k22],V[v1428]^V[k63]);
		//@formatter:on

		V[o01] = V[v1400];
		V[o77] = V[v1401];
		V[o67] = V[v1402];
		V[o57] = V[v1403];
		V[o47] = V[v1404];
		V[o37] = V[v1405];
		V[o27] = V[v1406];
		V[o17] = V[v1407];
		V[o07] = V[v1408];
		V[o75] = V[v1409];
		V[o65] = V[v1410];
		V[o55] = V[v1411];
		V[o45] = V[v1412];
		V[o35] = V[v1413];
		V[o25] = V[v1414];
		V[o15] = V[v1415];
		V[o05] = V[v1416];
		V[o73] = V[v1417];
		V[o63] = V[v1418];
		V[o53] = V[v1419];
		V[o43] = V[v1420];
		V[o33] = V[v1421];
		V[o23] = V[v1422];
		V[o13] = V[v1423];
		V[o03] = V[v1424];
		V[o71] = V[v1425];
		V[o61] = V[v1426];
		V[o51] = V[v1427];
		V[o41] = V[v1428];
		V[o31] = V[v1429];
		V[o21] = V[v1430];
		V[o11] = V[v1431];
		V[o00] = V[v1500];
		V[o76] = V[v1501];
		V[o66] = V[v1502];
		V[o56] = V[v1503];
		V[o46] = V[v1504];
		V[o36] = V[v1505];
		V[o26] = V[v1506];
		V[o16] = V[v1507];
		V[o06] = V[v1508];
		V[o74] = V[v1509];
		V[o64] = V[v1510];
		V[o54] = V[v1511];
		V[o44] = V[v1512];
		V[o34] = V[v1513];
		V[o24] = V[v1514];
		V[o14] = V[v1515];
		V[o04] = V[v1516];
		V[o72] = V[v1517];
		V[o62] = V[v1518];
		V[o52] = V[v1519];
		V[o42] = V[v1520];
		V[o32] = V[v1521];
		V[o22] = V[v1522];
		V[o12] = V[v1523];
		V[o02] = V[v1524];
		V[o70] = V[v1525];
		V[o60] = V[v1526];
		V[o50] = V[v1527];
		V[o40] = V[v1528];
		V[o30] = V[v1529];
		V[o20] = V[v1530];
		V[o10] = V[v1531];

		// setup root names
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
			asprintf(&filename, arg_data, 15);
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
 * @global {builddesContext_t} Application context
 */
builddesContext_t app;

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
int main(int argc, char *const *argv) {
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
			{"help",        0, 0, LO_HELP},
			{"maxnode",     1, 0, LO_MAXNODE},
			{"quiet",       2, 0, LO_QUIET},
			{"split",       0, 0, LO_SPLIT},
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
