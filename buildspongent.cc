//#pragma GCC optimize ("O0") // optimize on demand

/*
 * buildspongent.cc
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
#include "buildspongent.h"

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
 * @date 2021-05-17 14:49:57
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
#include "validatespongent.h"

struct NODE {
	uint32_t id;

	NODE() { id = 0; }

	NODE(uint32_t id) {
		assert((id & ~IBIT) == 0 || ((id & ~IBIT) >= gTree->kstart && (id & ~IBIT) < gTree->ncount));
		this->id = id;
	}

	NODE(NODE Q, NODE T, NODE F) { this->id = gTree->addNormaliseNode(Q.id, T.id, F.id); }

	NODE operator|(const NODE &other) const { return NODE(this->id, IBIT, other.id); }

	NODE operator*(const NODE &other) const { return NODE(this->id, other.id, 0); }

	NODE operator^(const NODE &other) const { return NODE(this->id, other.id ^ IBIT, other.id); }
};

#include "buildspongentbox.h"

/**
 * @date 2021-05-17 14:53:57
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct buildspongentContext_t {

	/// @var {number} header flags
	uint32_t opt_flags;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;
	/// @var {number} --maxnode, Maximum number of nodes for `baseTree_t`.
	unsigned opt_maxNode;

	buildspongentContext_t() {
		opt_flags   = 0;
		opt_force   = 0;
		opt_maxNode = DEFAULT_MAXNODE;
	}

	void __attribute__((optimize("O0"))) Permute(NODE value[11][8], NODE *V, uint32_t kstart, uint32_t ostart) {


		static int IV[]     = {0x05, 0x0a, 0x14, 0x29, 0x13, 0x27, 0x0f, 0x1e, 0x3d, 0x3a, 0x34, 0x28, 0x11, 0x23, 0x07, 0x0e, 0x1c, 0x39, 0x32, 0x24, 0x09, 0x12, 0x25, 0x0b, 0x16, 0x2d, 0x1b, 0x37, 0x2e, 0x1d, 0x3b, 0x36, 0x2c, 0x19, 0x33, 0x26, 0x0d, 0x1a, 0x35, 0x2a, 0x15, 0x2b, 0x17, 0x2f, 0x1f};
		static int INV_IV[] = {0xa0, 0x50, 0x28, 0x94, 0xc8, 0xe4, 0xf0, 0x78, 0xbc, 0x5c, 0x2c, 0x14, 0x88, 0xc4, 0xe0, 0x70, 0x38, 0x9c, 0x4c, 0x24, 0x90, 0x48, 0xa4, 0xd0, 0x68, 0xb4, 0xd8, 0xec, 0x74, 0xb8, 0xdc, 0x6c, 0x34, 0x98, 0xcc, 0x64, 0xb0, 0x58, 0xac, 0x54, 0xa8, 0xd4, 0xe8, 0xf4, 0xf8};

		NODE tmp[11][8];

		if (kstart) {
			value[0][0] = value[0][0] ^ V[kstart + 8 * 0 + 0];
			value[0][1] = value[0][1] ^ V[kstart + 8 * 0 + 1];
			value[0][2] = value[0][2] ^ V[kstart + 8 * 0 + 2];
			value[0][3] = value[0][3] ^ V[kstart + 8 * 0 + 3];
			value[0][4] = value[0][4] ^ V[kstart + 8 * 0 + 4];
			value[0][5] = value[0][5] ^ V[kstart + 8 * 0 + 5];
			value[0][6] = value[0][6] ^ V[kstart + 8 * 0 + 6];
			value[0][7] = value[0][7] ^ V[kstart + 8 * 0 + 7];
		}

		for (int i = 0; i < 45; i++) {
			//@formatter:off

#define A 10

			// Add counter values
			value[0][0] = value[0][0] ^ (IV[i]&0x01?IBIT:0);
			value[0][1] = value[0][1] ^ (IV[i]&0x02?IBIT:0);
			value[0][2] = value[0][2] ^ (IV[i]&0x04?IBIT:0);
			value[0][3] = value[0][3] ^ (IV[i]&0x08?IBIT:0);
			value[0][4] = value[0][4] ^ (IV[i]&0x10?IBIT:0);
			value[0][5] = value[0][5] ^ (IV[i]&0x20?IBIT:0);
			value[0][6] = value[0][6] ^ (IV[i]&0x40?IBIT:0);
			value[0][7] = value[0][7] ^ (IV[i]&0x80?IBIT:0);
			value[A][0] = value[A][0] ^ (INV_IV[i]&0x01?IBIT:0);
			value[A][1] = value[A][1] ^ (INV_IV[i]&0x02?IBIT:0);
			value[A][2] = value[A][2] ^ (INV_IV[i]&0x04?IBIT:0);
			value[A][3] = value[A][3] ^ (INV_IV[i]&0x08?IBIT:0);
			value[A][4] = value[A][4] ^ (INV_IV[i]&0x10?IBIT:0);
			value[A][5] = value[A][5] ^ (INV_IV[i]&0x20?IBIT:0);
			value[A][6] = value[A][6] ^ (INV_IV[i]&0x40?IBIT:0);
			value[A][7] = value[A][7] ^ (INV_IV[i]&0x80?IBIT:0);

			tmp[0][7] = box_4(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[0][6] = box_0(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[0][5] = box_4(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[0][4] = box_0(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[0][3] = box_4(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[0][2] = box_0(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[0][1] = box_4(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[0][0] = box_0(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[1][7] = box_4(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);
			tmp[1][6] = box_0(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);
			tmp[1][5] = box_4(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[1][4] = box_0(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[1][3] = box_4(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[1][2] = box_0(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[1][1] = box_4(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[1][0] = box_0(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[2][7] = box_5(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[2][6] = box_1(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[2][5] = box_4(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[2][4] = box_0(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[2][3] = box_4(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[2][2] = box_0(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[2][1] = box_4(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[2][0] = box_0(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[3][7] = box_5(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[3][6] = box_1(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[3][5] = box_5(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[3][4] = box_1(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[3][3] = box_5(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[3][2] = box_1(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[3][1] = box_5(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[3][0] = box_1(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[4][7] = box_5(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[4][6] = box_1(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[4][5] = box_5(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);
			tmp[4][4] = box_1(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);
			tmp[4][3] = box_5(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[4][2] = box_1(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[4][1] = box_5(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[4][0] = box_1(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[5][7] = box_6(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[5][6] = box_2(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[5][5] = box_6(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[5][4] = box_2(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[5][3] = box_5(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[5][2] = box_1(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[5][1] = box_5(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[5][0] = box_1(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[6][7] = box_6(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[6][6] = box_2(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[6][5] = box_6(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[6][4] = box_2(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[6][3] = box_6(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[6][2] = box_2(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[6][1] = box_6(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[6][0] = box_2(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[7][7] = box_6(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[7][6] = box_2(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[7][5] = box_6(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[7][4] = box_2(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[7][3] = box_6(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);
			tmp[7][2] = box_2(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);
			tmp[7][1] = box_6(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[7][0] = box_2(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[8][7] = box_7(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[8][6] = box_3(value[2][7], value[2][6], value[2][5], value[2][4], value[2][3], value[2][2], value[2][1], value[2][0]);
			tmp[8][5] = box_7(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[8][4] = box_3(value[1][7], value[1][6], value[1][5], value[1][4], value[1][3], value[1][2], value[1][1], value[1][0]);
			tmp[8][3] = box_7(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[8][2] = box_3(value[0][7], value[0][6], value[0][5], value[0][4], value[0][3], value[0][2], value[0][1], value[0][0]);
			tmp[8][1] = box_6(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[8][0] = box_2(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[9][7] = box_7(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[9][6] = box_3(value[6][7], value[6][6], value[6][5], value[6][4], value[6][3], value[6][2], value[6][1], value[6][0]);
			tmp[9][5] = box_7(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[9][4] = box_3(value[5][7], value[5][6], value[5][5], value[5][4], value[5][3], value[5][2], value[5][1], value[5][0]);
			tmp[9][3] = box_7(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[9][2] = box_3(value[4][7], value[4][6], value[4][5], value[4][4], value[4][3], value[4][2], value[4][1], value[4][0]);
			tmp[9][1] = box_7(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[9][0] = box_3(value[3][7], value[3][6], value[3][5], value[3][4], value[3][3], value[3][2], value[3][1], value[3][0]);
			tmp[A][7] = box_7(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[A][6] = box_3(value[A][7], value[A][6], value[A][5], value[A][4], value[A][3], value[A][2], value[A][1], value[A][0]);
			tmp[A][5] = box_7(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[A][4] = box_3(value[9][7], value[9][6], value[9][5], value[9][4], value[9][3], value[9][2], value[9][1], value[9][0]);
			tmp[A][3] = box_7(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[A][2] = box_3(value[8][7], value[8][6], value[8][5], value[8][4], value[8][3], value[8][2], value[8][1], value[8][0]);
			tmp[A][1] = box_7(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);
			tmp[A][0] = box_3(value[7][7], value[7][6], value[7][5], value[7][4], value[7][3], value[7][2], value[7][1], value[7][0]);

			value[0][0] = tmp[0][0]; value[0][1] = tmp[0][1]; value[0][2] = tmp[0][2]; value[0][3] = tmp[0][3]; value[0][4] = tmp[0][4]; value[0][5] = tmp[0][5]; value[0][6] = tmp[0][6]; value[0][7] = tmp[0][7];
			value[1][0] = tmp[1][0]; value[1][1] = tmp[1][1]; value[1][2] = tmp[1][2]; value[1][3] = tmp[1][3]; value[1][4] = tmp[1][4]; value[1][5] = tmp[1][5]; value[1][6] = tmp[1][6]; value[1][7] = tmp[1][7];
			value[2][0] = tmp[2][0]; value[2][1] = tmp[2][1]; value[2][2] = tmp[2][2]; value[2][3] = tmp[2][3]; value[2][4] = tmp[2][4]; value[2][5] = tmp[2][5]; value[2][6] = tmp[2][6]; value[2][7] = tmp[2][7];
			value[3][0] = tmp[3][0]; value[3][1] = tmp[3][1]; value[3][2] = tmp[3][2]; value[3][3] = tmp[3][3]; value[3][4] = tmp[3][4]; value[3][5] = tmp[3][5]; value[3][6] = tmp[3][6]; value[3][7] = tmp[3][7];
			value[4][0] = tmp[4][0]; value[4][1] = tmp[4][1]; value[4][2] = tmp[4][2]; value[4][3] = tmp[4][3]; value[4][4] = tmp[4][4]; value[4][5] = tmp[4][5]; value[4][6] = tmp[4][6]; value[4][7] = tmp[4][7];
			value[5][0] = tmp[5][0]; value[5][1] = tmp[5][1]; value[5][2] = tmp[5][2]; value[5][3] = tmp[5][3]; value[5][4] = tmp[5][4]; value[5][5] = tmp[5][5]; value[5][6] = tmp[5][6]; value[5][7] = tmp[5][7];
			value[6][0] = tmp[6][0]; value[6][1] = tmp[6][1]; value[6][2] = tmp[6][2]; value[6][3] = tmp[6][3]; value[6][4] = tmp[6][4]; value[6][5] = tmp[6][5]; value[6][6] = tmp[6][6]; value[6][7] = tmp[6][7];
			value[7][0] = tmp[7][0]; value[7][1] = tmp[7][1]; value[7][2] = tmp[7][2]; value[7][3] = tmp[7][3]; value[7][4] = tmp[7][4]; value[7][5] = tmp[7][5]; value[7][6] = tmp[7][6]; value[7][7] = tmp[7][7];
			value[8][0] = tmp[8][0]; value[8][1] = tmp[8][1]; value[8][2] = tmp[8][2]; value[8][3] = tmp[8][3]; value[8][4] = tmp[8][4]; value[8][5] = tmp[8][5]; value[8][6] = tmp[8][6]; value[8][7] = tmp[8][7];
			value[9][0] = tmp[9][0]; value[9][1] = tmp[9][1]; value[9][2] = tmp[9][2]; value[9][3] = tmp[9][3]; value[9][4] = tmp[9][4]; value[9][5] = tmp[9][5]; value[9][6] = tmp[9][6]; value[9][7] = tmp[9][7];
			value[A][0] = tmp[A][0]; value[A][1] = tmp[A][1]; value[A][2] = tmp[A][2]; value[A][3] = tmp[A][3]; value[A][4] = tmp[A][4]; value[A][5] = tmp[A][5]; value[A][6] = tmp[A][6]; value[A][7] = tmp[A][7];

#undef A

			//@formatter:on
		}

		if (ostart) {
			V[ostart + 0] = value[0][0];
			V[ostart + 1] = value[0][1];
			V[ostart + 2] = value[0][2];
			V[ostart + 3] = value[0][3];
			V[ostart + 4] = value[0][4];
			V[ostart + 5] = value[0][5];
			V[ostart + 6] = value[0][6];
			V[ostart + 7] = value[0][7];
		}

//printf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n", value[0].toInt(), value[1].toInt(),value[2].toInt(),value[3].toInt(),value[4].toInt(),value[5].toInt(),value[6].toInt(),value[7].toInt(),value[8].toInt(),value[9].toInt(),value[A].toInt());
//9ecd2099c3037de47b9af6
//59a34c4d698a6f4bbbdd73
//9133d0084d4f8809027f6b
//d9853021226170a7e6a14f
//3ff32475b8250a8327e806
//a1f42a8ce238ba973808b4
//e8d673048201d1e011f28c
//87d48e86da3d3877ddb5d5
//25d24b92db70d49d91d7ed
//d121478fae456a3aa2c0b6
//be5d2abc66997a4c3f2025
//b4bdb368d46ce054c9d076
//5e03095e9550b2832ee951
//fc82a833438701ed547bef
//093405eb532bfd4ea93acd
//bebf7844494787851b6e7d
//3ce8039a4bca0acf975f9f
//1cc581d512468bceb1dde8
//08d7ef55e2a1f144c76cb7
//5b68bd59f4d7bcc4a6294d
//a70bc136298ddf6c7d898d
//b1e0be49e901cd423c1fdb

	}

	/*
	 * Build spongent expression
	 * Ints are replaced by node_t wrappers in vectors.
	 */
	void __attribute__((optimize("O0"))) build(NODE *V) {
		NODE value[11][8];

		// first _PSTART is main entrypoint

		Permute(value, V, KSTART + 8 * 0, 0);
		Permute(value, V, KSTART + 8 * 1, 0);
		Permute(value, V, KSTART + 8 * 2, 0);
		Permute(value, V, KSTART + 8 * 3, 0);
		Permute(value, V, KSTART + 8 * 4, 0);
		Permute(value, V, KSTART + 8 * 5, 0);
		Permute(value, V, KSTART + 8 * 6, 0);
		Permute(value, V, KSTART + 8 * 7, 0);
		Permute(value, V, KSTART + 8 * 8, 0);
		Permute(value, V, KSTART + 8 * 9, 0);
		Permute(value, V, KSTART + 8 * 10, 0);

		value[0][7] = value[0][7] ^ IBIT;

		Permute(value, V, 0, OSTART + 8 * 0);
		Permute(value, V, 0, OSTART + 8 * 1);
		Permute(value, V, 0, OSTART + 8 * 2);
		Permute(value, V, 0, OSTART + 8 * 3);
		Permute(value, V, 0, OSTART + 8 * 4);
		Permute(value, V, 0, OSTART + 8 * 5);
		Permute(value, V, 0, OSTART + 8 * 6);
		Permute(value, V, 0, OSTART + 8 * 7);
		Permute(value, V, 0, OSTART + 8 * 8);
		Permute(value, V, 0, OSTART + 8 * 9);
		Permute(value, V, 0, OSTART + 8 * 10);
	}


	void main(const char *jsonFilename) {
		/*
		 * allocate and initialise placeholder/helper array references to variables
		 * NOTE: use NSTART because this is the last intermediate as gTree->nstart might point to ESTART)
		 */
		NODE *V = (NODE *) malloc(VLAST * sizeof V[0]);

		/*
		 * Allocate the build tree containing the complete formula
		 */
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
 * @global {buildspongentContext_t} Application context
 */
buildspongentContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json>\n", argv[0]);
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
