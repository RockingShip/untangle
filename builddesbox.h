/*
 * builddesbox.h
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

NODE __attribute__((optimize("O0"))) box_0_9(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in5, 0);
	NODE _01 = NODE(in2, 0, in5 ^ IBIT);
	NODE _02 = NODE(in2, in5, in5 ^ IBIT);
	NODE _03 = NODE(in2, in5, IBIT);
	NODE _04 = NODE(in2, 0, in5);
	NODE _05 = NODE(in4, in5, _01);
	NODE _06 = NODE(in4, _03, _04);
	NODE _07 = NODE(in4, _00, _02 ^ IBIT);
	NODE _08 = NODE(in4, in5, in5 ^ IBIT);
	NODE _09 = NODE(in4, in5, in2);
	NODE _10 = NODE(in4, in5, _02 ^ IBIT);
	NODE _11 = NODE(in4, _02, _02 ^ IBIT);
	NODE _12 = NODE(in1, _05, _09 ^ IBIT);
	NODE _13 = NODE(in1, _06, _06 ^ IBIT);
	NODE _14 = NODE(in1, _07, _10 ^ IBIT);
	NODE _15 = NODE(in1, _08, _11);
	NODE _16 = NODE(in3, _12, _14 ^ IBIT);
	NODE _17 = NODE(in3, _13, _15 ^ IBIT);
	NODE _18 = NODE(in0, _16, _17);
	return _18;
}

NODE __attribute__((optimize("O0"))) box_0_17(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in5, 0);
	NODE _01 = NODE(in2, 0, in5 ^ IBIT);
	NODE _02 = NODE(in2, in5, in5 ^ IBIT);
	NODE _03 = NODE(in2, in5, IBIT);
	NODE _04 = NODE(in2, 0, in5);
	NODE _07 = NODE(in4, _00, _02 ^ IBIT);
	NODE _09 = NODE(in4, in5, in2);
	NODE _19 = NODE(in4, _03, in2);
	NODE _20 = NODE(in4, _01, in2 ^ IBIT);
	NODE _21 = NODE(in4, in5, _03 ^ IBIT);
	NODE _22 = NODE(in4, _00, _01);
	NODE _23 = NODE(in4, _04, _03);
	NODE _24 = NODE(in4, _02, 0);
	NODE _25 = NODE(in1, _19, _22);
	NODE _26 = NODE(in1, _09, _23 ^ IBIT);
	NODE _27 = NODE(in1, _20, _24 ^ IBIT);
	NODE _28 = NODE(in1, _21, _07 ^ IBIT);
	NODE _29 = NODE(in3, _25, _27);
	NODE _30 = NODE(in3, _26, _28 ^ IBIT);
	NODE _31 = NODE(in0, _29, _30 ^ IBIT);
	return _31;
}

NODE __attribute__((optimize("O0"))) box_0_23(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in2, 0, in5 ^ IBIT);
	NODE _02 = NODE(in2, in5, in5 ^ IBIT);
	NODE _03 = NODE(in2, in5, IBIT);
	NODE _04 = NODE(in2, 0, in5);
	NODE _11 = NODE(in4, _02, _02 ^ IBIT);
	NODE _32 = NODE(in4, in2, in5 ^ IBIT);
	NODE _33 = NODE(in4, _02, _04);
	NODE _34 = NODE(in4, _01, in5);
	NODE _35 = NODE(in4, 0, _02 ^ IBIT);
	NODE _36 = NODE(in4, _03, _04 ^ IBIT);
	NODE _37 = NODE(in4, _01, _04);
	NODE _38 = NODE(in1, _32, _34);
	NODE _39 = NODE(in1, _11, _35 ^ IBIT);
	NODE _40 = NODE(in1, _33, _36);
	NODE _41 = NODE(in1, in2, _37);
	NODE _42 = NODE(in3, _38, _40);
	NODE _43 = NODE(in3, _39, _41);
	NODE _44 = NODE(in0, _42, _43);
	return _44 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_0_31(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in5, 0);
	NODE _02 = NODE(in2, in5, in5 ^ IBIT);
	NODE _04 = NODE(in2, 0, in5);
	NODE _11 = NODE(in4, _02, _02 ^ IBIT);
	NODE _45 = NODE(in4, _02, _00 ^ IBIT);
	NODE _46 = NODE(in4, _04, _00 ^ IBIT);
	NODE _47 = NODE(in4, in2, _04 ^ IBIT);
	NODE _48 = NODE(in4, in2, _00);
	NODE _49 = NODE(in4, _02, IBIT);
	NODE _50 = NODE(in1, _45, _48);
	NODE _51 = NODE(in1, _45, _49 ^ IBIT);
	NODE _52 = NODE(in1, _46, _11);
	NODE _53 = NODE(in1, _47, _49 ^ IBIT);
	NODE _54 = NODE(in3, _50, _52);
	NODE _55 = NODE(in3, _51, _53 ^ IBIT);
	NODE _56 = NODE(in0, _54, _55 ^ IBIT);
	return _56;
}

NODE __attribute__((optimize("O0"))) box_1_2(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, 0, in3 ^ IBIT);
	NODE _01 = NODE(in2, in3, IBIT);
	NODE _02 = NODE(in2, in3, 0);
	NODE _03 = NODE(in2, in3, in3 ^ IBIT);
	NODE _04 = NODE(in2, 0, in3);
	NODE _05 = NODE(in1, _00, in2);
	NODE _06 = NODE(in1, _04, _00 ^ IBIT);
	NODE _07 = NODE(in1, _01, _03 ^ IBIT);
	NODE _08 = NODE(in1, in2, in3 ^ IBIT);
	NODE _09 = NODE(in1, _03, _03 ^ IBIT);
	NODE _10 = NODE(in1, _02, _03 ^ IBIT);
	NODE _11 = NODE(in1, _04, _04 ^ IBIT);
	NODE _12 = NODE(in5, _05, _05 ^ IBIT);
	NODE _13 = NODE(in5, _06, _09);
	NODE _14 = NODE(in5, _07, _10);
	NODE _15 = NODE(in5, _08, _11);
	NODE _16 = NODE(in4, _12, _14);
	NODE _17 = NODE(in4, _13, _15);
	NODE _18 = NODE(in0, _16, _17);
	return _18;
}

NODE __attribute__((optimize("O0"))) box_1_13(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _03 = NODE(in2, in3, in3 ^ IBIT);
	NODE _09 = NODE(in1, _03, _03 ^ IBIT);
	NODE _19 = NODE(in1, in3, in3 ^ IBIT);
	NODE _20 = NODE(in1, _03, in2 ^ IBIT);
	NODE _21 = NODE(in1, _03, in2);
	NODE _22 = NODE(in1, in3, _03 ^ IBIT);
	NODE _23 = NODE(in1, in3, in2);
	NODE _24 = NODE(in5, _19, _20);
	NODE _25 = NODE(in5, _09, _22 ^ IBIT);
	NODE _26 = NODE(in5, _20, _20 ^ IBIT);
	NODE _27 = NODE(in5, _21, _23 ^ IBIT);
	NODE _28 = NODE(in4, _24, _26);
	NODE _29 = NODE(in4, _25, _27 ^ IBIT);
	NODE _30 = NODE(in0, _28, _29 ^ IBIT);
	return _30;
}

NODE __attribute__((optimize("O0"))) box_1_18(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _03 = NODE(in2, in3, in3 ^ IBIT);
	NODE _19 = NODE(in1, in3, in3 ^ IBIT);
	NODE _31 = NODE(in1, in2, _03 ^ IBIT);
	NODE _32 = NODE(in1, in2, in2 ^ IBIT);
	NODE _33 = NODE(in1, in2, in3);
	NODE _34 = NODE(in5, _31, _33);
	NODE _35 = NODE(in5, _32, _33 ^ IBIT);
	NODE _36 = NODE(in5, _33, _19);
	NODE _37 = NODE(in5, _19, _03);
	NODE _38 = NODE(in4, _34, _36 ^ IBIT);
	NODE _39 = NODE(in4, _35, _37);
	NODE _40 = NODE(in0, _38, _39);
	return _40;
}

NODE __attribute__((optimize("O0"))) box_1_28(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in2, in3, IBIT);
	NODE _02 = NODE(in2, in3, 0);
	NODE _03 = NODE(in2, in3, in3 ^ IBIT);
	NODE _08 = NODE(in1, in2, in3 ^ IBIT);
	NODE _19 = NODE(in1, in3, in3 ^ IBIT);
	NODE _41 = NODE(in1, _02, _01);
	NODE _42 = NODE(in1, in3, _03);
	NODE _43 = NODE(in5, _41, _19 ^ IBIT);
	NODE _44 = NODE(in5, _41, _08 ^ IBIT);
	NODE _45 = NODE(in5, _42, _08 ^ IBIT);
	NODE _46 = NODE(in4, _43, _45 ^ IBIT);
	NODE _47 = NODE(in4, _44, _45 ^ IBIT);
	NODE _48 = NODE(in0, _46, _47 ^ IBIT);
	return _48 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_2_6(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in1, 0, in4);
	NODE _01 = NODE(in1, in4, in4 ^ IBIT);
	NODE _02 = NODE(in1, 0, in4 ^ IBIT);
	NODE _03 = NODE(in5, _00, _01);
	NODE _04 = NODE(in1, in5, in5 ^ IBIT);
	NODE _05 = NODE(in5, _01, _00);
	NODE _06 = NODE(in5, _01, _01 ^ IBIT);
	NODE _07 = NODE(in5, _02, _01);
	NODE _08 = NODE(in5, _01, _00 ^ IBIT);
	NODE _09 = NODE(in3, _03, _07 ^ IBIT);
	NODE _10 = NODE(in3, _04, _06);
	NODE _11 = NODE(in3, _05, _08);
	NODE _12 = NODE(in3, _06, _04);
	NODE _13 = NODE(in2, _09, _11);
	NODE _14 = NODE(in2, _10, _12 ^ IBIT);
	NODE _15 = NODE(in0, _13, _14);
	return _15;
}

NODE __attribute__((optimize("O0"))) box_2_16(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in1, in4, in4 ^ IBIT);
	NODE _02 = NODE(in1, 0, in4 ^ IBIT);
	NODE _04 = NODE(in1, in5, in5 ^ IBIT);
	NODE _06 = NODE(in5, _01, _01 ^ IBIT);
	NODE _16 = NODE(in1, in4, 0);
	NODE _17 = NODE(in5, in4, in1);
	NODE _18 = NODE(in5, in4, _01 ^ IBIT);
	NODE _19 = NODE(in5, in1, _02 ^ IBIT);
	NODE _20 = NODE(in5, in1, _16 ^ IBIT);
	NODE _21 = NODE(in3, _17, _06);
	NODE _22 = NODE(in3, _18, _04 ^ IBIT);
	NODE _23 = NODE(in3, _19, _20 ^ IBIT);
	NODE _24 = NODE(in2, _21, _22 ^ IBIT);
	NODE _25 = NODE(in2, _21, _23 ^ IBIT);
	NODE _26 = NODE(in0, _24, _25 ^ IBIT);
	return _26;
}

NODE __attribute__((optimize("O0"))) box_2_24(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in1, in4, in4 ^ IBIT);
	NODE _06 = NODE(in5, _01, _01 ^ IBIT);
	NODE _16 = NODE(in1, in4, 0);
	NODE _27 = NODE(in1, in4, IBIT);
	NODE _28 = NODE(in5, _01, _27 ^ IBIT);
	NODE _29 = NODE(in4, in5, in5 ^ IBIT);
	NODE _30 = NODE(in5, in4, _16 ^ IBIT);
	NODE _31 = NODE(in5, in1, _27 ^ IBIT);
	NODE _32 = NODE(in3, _28, _28 ^ IBIT);
	NODE _33 = NODE(in3, _06, _31 ^ IBIT);
	NODE _34 = NODE(in3, _29, _29 ^ IBIT);
	NODE _35 = NODE(in3, _30, _01);
	NODE _36 = NODE(in2, _32, _34 ^ IBIT);
	NODE _37 = NODE(in2, _33, _35 ^ IBIT);
	NODE _38 = NODE(in0, _36, _37 ^ IBIT);
	return _38;
}

NODE __attribute__((optimize("O0"))) box_2_30(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in1, 0, in4);
	NODE _01 = NODE(in1, in4, in4 ^ IBIT);
	NODE _02 = NODE(in1, 0, in4 ^ IBIT);
	NODE _05 = NODE(in5, _01, _00);
	NODE _16 = NODE(in1, in4, 0);
	NODE _18 = NODE(in5, in4, _01 ^ IBIT);
	NODE _27 = NODE(in1, in4, IBIT);
	NODE _29 = NODE(in4, in5, in5 ^ IBIT);
	NODE _31 = NODE(in5, in1, _27 ^ IBIT);
	NODE _39 = NODE(in5, _16, in4);
	NODE _40 = NODE(in5, _01, _16 ^ IBIT);
	NODE _41 = NODE(in5, _01, _02 ^ IBIT);
	NODE _42 = NODE(in3, _05, _29);
	NODE _43 = NODE(in3, _39, _40 ^ IBIT);
	NODE _44 = NODE(in3, _31, _18);
	NODE _45 = NODE(in3, _39, _41 ^ IBIT);
	NODE _46 = NODE(in2, _42, _44 ^ IBIT);
	NODE _47 = NODE(in2, _43, _45 ^ IBIT);
	NODE _48 = NODE(in0, _46, _47);
	return _48 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_3_1(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in0, in1, IBIT);
	NODE _01 = NODE(in0, in1, in1 ^ IBIT);
	NODE _02 = NODE(in0, 0, in1);
	NODE _03 = NODE(in3, in0, _02);
	NODE _04 = NODE(in3, _00, _02 ^ IBIT);
	NODE _05 = NODE(in3, _01, _01 ^ IBIT);
	NODE _06 = NODE(in3, _01, _02 ^ IBIT);
	NODE _07 = NODE(in3, _01, _00);
	NODE _08 = NODE(in1, in3, in3 ^ IBIT);
	NODE _09 = NODE(in3, in0, _01 ^ IBIT);
	NODE _10 = NODE(in3, in0, _00);
	NODE _11 = NODE(in2, _03, _05 ^ IBIT);
	NODE _12 = NODE(in2, _04, _06 ^ IBIT);
	NODE _13 = NODE(in4, _11, _12);
	NODE _14 = NODE(in2, _07, _09);
	NODE _15 = NODE(in2, _08, _10 ^ IBIT);
	NODE _16 = NODE(in4, _14, _15);
	NODE _17 = NODE(in5, _13, _16);
	return _17 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_3_10(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in0, in1, IBIT);
	NODE _01 = NODE(in0, in1, in1 ^ IBIT);
	NODE _02 = NODE(in0, 0, in1);
	NODE _03 = NODE(in3, in0, _02);
	NODE _04 = NODE(in3, _00, _02 ^ IBIT);
	NODE _05 = NODE(in3, _01, _01 ^ IBIT);
	NODE _06 = NODE(in3, _01, _02 ^ IBIT);
	NODE _07 = NODE(in3, _01, _00);
	NODE _08 = NODE(in1, in3, in3 ^ IBIT);
	NODE _09 = NODE(in3, in0, _01 ^ IBIT);
	NODE _10 = NODE(in3, in0, _00);
	NODE _11 = NODE(in2, _03, _05 ^ IBIT);
	NODE _12 = NODE(in2, _04, _06 ^ IBIT);
	NODE _13 = NODE(in4, _11, _12);
	NODE _14 = NODE(in2, _07, _09);
	NODE _15 = NODE(in2, _08, _10 ^ IBIT);
	NODE _16 = NODE(in4, _14, _15);
	NODE _18 = NODE(in5, _16, _13 ^ IBIT);
	return _18;
}

NODE __attribute__((optimize("O0"))) box_3_20(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in0, in1, in1 ^ IBIT);
	NODE _05 = NODE(in3, _01, _01 ^ IBIT);
	NODE _08 = NODE(in1, in3, in3 ^ IBIT);
	NODE _19 = NODE(in0, in1, 0);
	NODE _20 = NODE(in0, 0, in1 ^ IBIT);
	NODE _21 = NODE(in3, _19, _01);
	NODE _22 = NODE(in3, _19, _20);
	NODE _23 = NODE(in3, _19, in0 ^ IBIT);
	NODE _24 = NODE(in3, _20, in0);
	NODE _25 = NODE(in3, _01, in0 ^ IBIT);
	NODE _26 = NODE(in3, _20, _01);
	NODE _27 = NODE(in2, _21, _22 ^ IBIT);
	NODE _28 = NODE(in2, _05, _23 ^ IBIT);
	NODE _29 = NODE(in4, _27, _28 ^ IBIT);
	NODE _30 = NODE(in2, _24, _08 ^ IBIT);
	NODE _31 = NODE(in2, _25, _26 ^ IBIT);
	NODE _32 = NODE(in4, _30, _31);
	NODE _33 = NODE(in5, _29, _32 ^ IBIT);
	return _33;
}

NODE __attribute__((optimize("O0"))) box_3_26(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in0, in1, in1 ^ IBIT);
	NODE _05 = NODE(in3, _01, _01 ^ IBIT);
	NODE _08 = NODE(in1, in3, in3 ^ IBIT);
	NODE _19 = NODE(in0, in1, 0);
	NODE _20 = NODE(in0, 0, in1 ^ IBIT);
	NODE _21 = NODE(in3, _19, _01);
	NODE _22 = NODE(in3, _19, _20);
	NODE _23 = NODE(in3, _19, in0 ^ IBIT);
	NODE _24 = NODE(in3, _20, in0);
	NODE _25 = NODE(in3, _01, in0 ^ IBIT);
	NODE _26 = NODE(in3, _20, _01);
	NODE _27 = NODE(in2, _21, _22 ^ IBIT);
	NODE _28 = NODE(in2, _05, _23 ^ IBIT);
	NODE _29 = NODE(in4, _27, _28 ^ IBIT);
	NODE _30 = NODE(in2, _24, _08 ^ IBIT);
	NODE _31 = NODE(in2, _25, _26 ^ IBIT);
	NODE _32 = NODE(in4, _30, _31);
	NODE _34 = NODE(in5, _32, _29);
	return _34 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_4_3(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in4, in5, in5 ^ IBIT);
	NODE _01 = NODE(in4, 0, in5);
	NODE _02 = NODE(in4, in5, IBIT);
	NODE _03 = NODE(in4, in5, 0);
	NODE _04 = NODE(in3, in5, _02 ^ IBIT);
	NODE _05 = NODE(in3, _02, _02 ^ IBIT);
	NODE _06 = NODE(in3, in4, _01 ^ IBIT);
	NODE _07 = NODE(in3, _03, _00);
	NODE _08 = NODE(in3, _00, _00 ^ IBIT);
	NODE _09 = NODE(in3, _02, _00);
	NODE _10 = NODE(in3, _01, _01 ^ IBIT);
	NODE _11 = NODE(in3, _02, _03 ^ IBIT);
	NODE _12 = NODE(in1, _04, _08);
	NODE _13 = NODE(in1, _05, _09 ^ IBIT);
	NODE _14 = NODE(in1, _06, _10 ^ IBIT);
	NODE _15 = NODE(in1, _07, _11);
	NODE _16 = NODE(in2, _12, _14);
	NODE _17 = NODE(in2, _13, _15);
	NODE _18 = NODE(in0, _16, _17 ^ IBIT);
	return _18;
}

NODE __attribute__((optimize("O0"))) box_4_8(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in4, in5, in5 ^ IBIT);
	NODE _01 = NODE(in4, 0, in5);
	NODE _02 = NODE(in4, in5, IBIT);
	NODE _03 = NODE(in4, in5, 0);
	NODE _06 = NODE(in3, in4, _01 ^ IBIT);
	NODE _19 = NODE(in4, 0, in5 ^ IBIT);
	NODE _20 = NODE(in3, _02, _01 ^ IBIT);
	NODE _21 = NODE(in3, _03, _00 ^ IBIT);
	NODE _22 = NODE(in3, _00, _01 ^ IBIT);
	NODE _23 = NODE(in3, _19, _19 ^ IBIT);
	NODE _24 = NODE(in3, in4, _02);
	NODE _25 = NODE(in3, _00, in5 ^ IBIT);
	NODE _26 = NODE(in3, _03, _19 ^ IBIT);
	NODE _27 = NODE(in1, _20, _06 ^ IBIT);
	NODE _28 = NODE(in1, _21, _24);
	NODE _29 = NODE(in1, _22, _25 ^ IBIT);
	NODE _30 = NODE(in1, _23, _26 ^ IBIT);
	NODE _31 = NODE(in2, _27, _29 ^ IBIT);
	NODE _32 = NODE(in2, _28, _30);
	NODE _33 = NODE(in0, _31, _32);
	return _33 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_4_14(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in4, in5, in5 ^ IBIT);
	NODE _02 = NODE(in4, in5, IBIT);
	NODE _04 = NODE(in3, in5, _02 ^ IBIT);
	NODE _08 = NODE(in3, _00, _00 ^ IBIT);
	NODE _19 = NODE(in4, 0, in5 ^ IBIT);
	NODE _34 = NODE(in3, _00, _19);
	NODE _35 = NODE(in3, _00, in4);
	NODE _36 = NODE(in3, _00, _02 ^ IBIT);
	NODE _37 = NODE(in3, in5, _19);
	NODE _38 = NODE(in1, _08, _35 ^ IBIT);
	NODE _39 = NODE(in1, _34, _36 ^ IBIT);
	NODE _40 = NODE(in1, _04, _37 ^ IBIT);
	NODE _41 = NODE(in1, _00, _08 ^ IBIT);
	NODE _42 = NODE(in2, _38, _40);
	NODE _43 = NODE(in2, _39, _41 ^ IBIT);
	NODE _44 = NODE(in0, _42, _43 ^ IBIT);
	return _44 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_4_25(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in4, in5, in5 ^ IBIT);
	NODE _01 = NODE(in4, 0, in5);
	NODE _02 = NODE(in4, in5, IBIT);
	NODE _03 = NODE(in4, in5, 0);
	NODE _45 = NODE(in3, in4, _03 ^ IBIT);
	NODE _46 = NODE(in3, _00, _01);
	NODE _47 = NODE(in3, 0, _02);
	NODE _48 = NODE(in3, in4, _00 ^ IBIT);
	NODE _49 = NODE(in3, in5, _01);
	NODE _50 = NODE(in3, _01, _02);
	NODE _51 = NODE(in1, _45, _48 ^ IBIT);
	NODE _52 = NODE(in1, _46, _49 ^ IBIT);
	NODE _53 = NODE(in1, _47, _48);
	NODE _54 = NODE(in1, in3, _50);
	NODE _55 = NODE(in2, _51, _53);
	NODE _56 = NODE(in2, _52, _54);
	NODE _57 = NODE(in0, _55, _56);
	return _57;
}

NODE __attribute__((optimize("O0"))) box_5_4(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in4, IBIT);
	NODE _01 = NODE(in2, in4, 0);
	NODE _02 = NODE(in2, in4, in4 ^ IBIT);
	NODE _03 = NODE(in2, 0, in4 ^ IBIT);
	NODE _04 = NODE(in1, in2, _03 ^ IBIT);
	NODE _05 = NODE(in1, _00, _00 ^ IBIT);
	NODE _06 = NODE(in1, in2, _00);
	NODE _07 = NODE(in1, _01, _00);
	NODE _08 = NODE(in1, _02, in4);
	NODE _09 = NODE(in5, _04, _08 ^ IBIT);
	NODE _10 = NODE(in5, _05, _07);
	NODE _11 = NODE(in5, _06, _07 ^ IBIT);
	NODE _12 = NODE(in5, _07, _08 ^ IBIT);
	NODE _13 = NODE(in3, _09, _11 ^ IBIT);
	NODE _14 = NODE(in3, _10, _12);
	NODE _15 = NODE(in0, _13, _14);
	return _15;
}

NODE __attribute__((optimize("O0"))) box_5_11(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in2, in4, 0);
	NODE _02 = NODE(in2, in4, in4 ^ IBIT);
	NODE _03 = NODE(in2, 0, in4 ^ IBIT);
	NODE _16 = NODE(in2, 0, in4);
	NODE _17 = NODE(in1, _16, _16 ^ IBIT);
	NODE _18 = NODE(in1, _02, _16 ^ IBIT);
	NODE _19 = NODE(in1, _01, _16 ^ IBIT);
	NODE _20 = NODE(in1, _02, _01 ^ IBIT);
	NODE _21 = NODE(in1, _03, _02 ^ IBIT);
	NODE _22 = NODE(in1, in2, _01);
	NODE _23 = NODE(in5, _17, _21 ^ IBIT);
	NODE _24 = NODE(in5, _18, _20 ^ IBIT);
	NODE _25 = NODE(in5, _19, _21 ^ IBIT);
	NODE _26 = NODE(in5, _20, _22);
	NODE _27 = NODE(in3, _23, _25 ^ IBIT);
	NODE _28 = NODE(in3, _24, _26 ^ IBIT);
	NODE _29 = NODE(in0, _27, _28 ^ IBIT);
	return _29;
}

NODE __attribute__((optimize("O0"))) box_5_19(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in4, IBIT);
	NODE _02 = NODE(in2, in4, in4 ^ IBIT);
	NODE _03 = NODE(in2, 0, in4 ^ IBIT);
	NODE _05 = NODE(in1, _00, _00 ^ IBIT);
	NODE _16 = NODE(in2, 0, in4);
	NODE _17 = NODE(in1, _16, _16 ^ IBIT);
	NODE _30 = NODE(in1, in4, in2 ^ IBIT);
	NODE _31 = NODE(in1, _03, _03 ^ IBIT);
	NODE _32 = NODE(in1, in4, _02 ^ IBIT);
	NODE _33 = NODE(in1, _16, _03 ^ IBIT);
	NODE _34 = NODE(in5, _05, _05 ^ IBIT);
	NODE _35 = NODE(in5, _30, _17);
	NODE _36 = NODE(in5, _31, _33 ^ IBIT);
	NODE _37 = NODE(in3, _34, _36);
	NODE _38 = NODE(in3, _35, _32 ^ IBIT);
	NODE _39 = NODE(in0, _37, _38 ^ IBIT);
	return _39;
}

NODE __attribute__((optimize("O0"))) box_5_29(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in2, in4, 0);
	NODE _02 = NODE(in2, in4, in4 ^ IBIT);
	NODE _03 = NODE(in2, 0, in4 ^ IBIT);
	NODE _16 = NODE(in2, 0, in4);
	NODE _20 = NODE(in1, _02, _01 ^ IBIT);
	NODE _31 = NODE(in1, _03, _03 ^ IBIT);
	NODE _32 = NODE(in1, in4, _02 ^ IBIT);
	NODE _40 = NODE(in1, in2, _02 ^ IBIT);
	NODE _41 = NODE(in1, _02, _02 ^ IBIT);
	NODE _42 = NODE(in1, _02, _16);
	NODE _43 = NODE(in5, _32, _20);
	NODE _44 = NODE(in5, _40, _02);
	NODE _45 = NODE(in5, _41, _42 ^ IBIT);
	NODE _46 = NODE(in5, _31, _31 ^ IBIT);
	NODE _47 = NODE(in3, _43, _45 ^ IBIT);
	NODE _48 = NODE(in3, _44, _46 ^ IBIT);
	NODE _49 = NODE(in0, _47, _48 ^ IBIT);
	return _49;
}

NODE __attribute__((optimize("O0"))) box_6_0(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in0, in5, 0);
	NODE _01 = NODE(in0, 0, in5 ^ IBIT);
	NODE _02 = NODE(in0, in5, in5 ^ IBIT);
	NODE _03 = NODE(in0, 0, in5);
	NODE _04 = NODE(in1, _00, _00 ^ IBIT);
	NODE _05 = NODE(in1, _01, _03);
	NODE _06 = NODE(in2, _04, _05 ^ IBIT);
	NODE _07 = NODE(in1, _02, _02 ^ IBIT);
	NODE _08 = NODE(in1, _01, _01 ^ IBIT);
	NODE _09 = NODE(in2, _07, _08 ^ IBIT);
	NODE _10 = NODE(in1, in0, _03 ^ IBIT);
	NODE _11 = NODE(in1, in5, _02 ^ IBIT);
	NODE _12 = NODE(in2, _10, _11);
	NODE _13 = NODE(in1, _00, in5 ^ IBIT);
	NODE _14 = NODE(in1, _01, _03 ^ IBIT);
	NODE _15 = NODE(in2, _13, _14 ^ IBIT);
	NODE _16 = NODE(in3, _06, _12 ^ IBIT);
	NODE _17 = NODE(in3, _09, _15 ^ IBIT);
	NODE _18 = NODE(in4, _16, _17 ^ IBIT);
	return _18;
}

NODE __attribute__((optimize("O0"))) box_6_7(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _01 = NODE(in0, 0, in5 ^ IBIT);
	NODE _02 = NODE(in0, in5, in5 ^ IBIT);
	NODE _07 = NODE(in1, _02, _02 ^ IBIT);
	NODE _19 = NODE(in1, _01, _02);
	NODE _20 = NODE(in2, _19, _07);
	NODE _21 = NODE(in1, in0, _02 ^ IBIT);
	NODE _22 = NODE(in1, in0, _01);
	NODE _23 = NODE(in2, _21, _22 ^ IBIT);
	NODE _24 = NODE(in1, _01, _02 ^ IBIT);
	NODE _25 = NODE(in2, _02, _24);
	NODE _26 = NODE(in3, _20, _25 ^ IBIT);
	NODE _27 = NODE(in3, _23, _20);
	NODE _28 = NODE(in4, _26, _27);
	return _28;
}

NODE __attribute__((optimize("O0"))) box_6_12(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in0, in5, 0);
	NODE _01 = NODE(in0, 0, in5 ^ IBIT);
	NODE _02 = NODE(in0, in5, in5 ^ IBIT);
	NODE _03 = NODE(in0, 0, in5);
	NODE _29 = NODE(in0, in5, IBIT);
	NODE _30 = NODE(in1, in5, _03 ^ IBIT);
	NODE _31 = NODE(in1, _29, _00 ^ IBIT);
	NODE _32 = NODE(in2, _30, _31);
	NODE _33 = NODE(in1, _01, _29 ^ IBIT);
	NODE _34 = NODE(in1, _02, _00 ^ IBIT);
	NODE _35 = NODE(in2, _33, _34 ^ IBIT);
	NODE _36 = NODE(in1, _03, _29);
	NODE _37 = NODE(in2, _34, _36);
	NODE _38 = NODE(in3, _32, _37 ^ IBIT);
	NODE _39 = NODE(in3, _35, _37);
	NODE _40 = NODE(in4, _38, _39);
	return _40;
}

NODE __attribute__((optimize("O0"))) box_6_22(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in0, in5, 0);
	NODE _01 = NODE(in0, 0, in5 ^ IBIT);
	NODE _02 = NODE(in0, in5, in5 ^ IBIT);
	NODE _04 = NODE(in1, _00, _00 ^ IBIT);
	NODE _07 = NODE(in1, _02, _02 ^ IBIT);
	NODE _29 = NODE(in0, in5, IBIT);
	NODE _41 = NODE(in1, _02, _01);
	NODE _42 = NODE(in1, _02, in0 ^ IBIT);
	NODE _43 = NODE(in2, _41, _42 ^ IBIT);
	NODE _44 = NODE(in1, _02, in0);
	NODE _45 = NODE(in1, _29, _00);
	NODE _46 = NODE(in2, _44, _45 ^ IBIT);
	NODE _47 = NODE(in1, _29, _29 ^ IBIT);
	NODE _48 = NODE(in2, _47, _07 ^ IBIT);
	NODE _49 = NODE(in1, in5, _01);
	NODE _50 = NODE(in2, _49, _04 ^ IBIT);
	NODE _51 = NODE(in3, _43, _48 ^ IBIT);
	NODE _52 = NODE(in3, _46, _50);
	NODE _53 = NODE(in4, _51, _52 ^ IBIT);
	return _53 ^ IBIT;
}

NODE __attribute__((optimize("O0"))) box_7_5(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in3, in3 ^ IBIT);
	NODE _01 = NODE(in1, _00, in2);
	NODE _02 = NODE(in1, in2, in3 ^ IBIT);
	NODE _03 = NODE(in4, _01, _02 ^ IBIT);
	NODE _04 = NODE(in1, in3, in3 ^ IBIT);
	NODE _05 = NODE(in1, _00, in2 ^ IBIT);
	NODE _06 = NODE(in4, _04, _05 ^ IBIT);
	NODE _07 = NODE(in1, _00, _00 ^ IBIT);
	NODE _08 = NODE(in4, _01, _07 ^ IBIT);
	NODE _09 = NODE(in2, in3, IBIT);
	NODE _10 = NODE(in1, in2, _09);
	NODE _11 = NODE(in1, _00, _09);
	NODE _12 = NODE(in4, _10, _11 ^ IBIT);
	NODE _13 = NODE(in0, _03, _08 ^ IBIT);
	NODE _14 = NODE(in0, _06, _12 ^ IBIT);
	NODE _15 = NODE(in5, _13, _14);
	return _15;
}

NODE __attribute__((optimize("O0"))) box_7_15(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in3, in3 ^ IBIT);
	NODE _04 = NODE(in1, in3, in3 ^ IBIT);
	NODE _07 = NODE(in1, _00, _00 ^ IBIT);
	NODE _16 = NODE(in2, in3, 0);
	NODE _17 = NODE(in2, 0, in3);
	NODE _18 = NODE(in1, _16, _00 ^ IBIT);
	NODE _19 = NODE(in1, _17, in2);
	NODE _20 = NODE(in4, _18, _19 ^ IBIT);
	NODE _21 = NODE(in1, in3, _00 ^ IBIT);
	NODE _22 = NODE(in4, _21, _07);
	NODE _23 = NODE(in2, 0, in3 ^ IBIT);
	NODE _24 = NODE(in1, _23, in3);
	NODE _25 = NODE(in4, _24, _19 ^ IBIT);
	NODE _26 = NODE(in1, in2, in2 ^ IBIT);
	NODE _27 = NODE(in4, _04, _26 ^ IBIT);
	NODE _28 = NODE(in0, _20, _25 ^ IBIT);
	NODE _29 = NODE(in0, _22, _27 ^ IBIT);
	NODE _30 = NODE(in5, _28, _29 ^ IBIT);
	return _30;
}

NODE __attribute__((optimize("O0"))) box_7_21(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in3, in3 ^ IBIT);
	NODE _01 = NODE(in1, _00, in2);
	NODE _02 = NODE(in1, in2, in3 ^ IBIT);
	NODE _03 = NODE(in4, _01, _02 ^ IBIT);
	NODE _07 = NODE(in1, _00, _00 ^ IBIT);
	NODE _08 = NODE(in4, _01, _07 ^ IBIT);
	NODE _09 = NODE(in2, in3, IBIT);
	NODE _10 = NODE(in1, in2, _09);
	NODE _13 = NODE(in0, _03, _08 ^ IBIT);
	NODE _16 = NODE(in2, in3, 0);
	NODE _31 = NODE(in1, _16, IBIT);
	NODE _32 = NODE(in4, _10, _31 ^ IBIT);
	NODE _33 = NODE(in1, in2, in3);
	NODE _34 = NODE(in1, _16, _09);
	NODE _35 = NODE(in4, _33, _34 ^ IBIT);
	NODE _36 = NODE(in0, _32, _35 ^ IBIT);
	NODE _37 = NODE(in5, _36, _13 ^ IBIT);
	return _37;
}

NODE __attribute__((optimize("O0"))) box_7_27(NODE in5, NODE in4, NODE in3, NODE in2, NODE in1, NODE in0) {
	NODE _00 = NODE(in2, in3, in3 ^ IBIT);
	NODE _02 = NODE(in1, in2, in3 ^ IBIT);
	NODE _04 = NODE(in1, in3, in3 ^ IBIT);
	NODE _07 = NODE(in1, _00, _00 ^ IBIT);
	NODE _09 = NODE(in2, in3, IBIT);
	NODE _10 = NODE(in1, in2, _09);
	NODE _21 = NODE(in1, in3, _00 ^ IBIT);
	NODE _38 = NODE(in4, _04, _07 ^ IBIT);
	NODE _39 = NODE(in4, _10, _10 ^ IBIT);
	NODE _40 = NODE(in4, _21, _02);
	NODE _41 = NODE(in0, _38, _40);
	NODE _42 = NODE(in0, _39, _40 ^ IBIT);
	NODE _43 = NODE(in5, _41, _42);
	return _43 ^ IBIT;
}
