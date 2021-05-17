/*
 * buildspongent.h
 * 	Ancient code that creates the input database.
 * 	Names of keys, roots/entrypoints and intermediates
 * 	Note: nodeId #1 is used for "un-initialised" error
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
enum {
	kZero, // reference value
	kError, // un-initialised marker

	// input keys
	k00, k01, k02, k03, k04, k05, k06, k07,
	k10, k11, k12, k13, k14, k15, k16, k17,
	k20, k21, k22, k23, k24, k25, k26, k27,
	k30, k31, k32, k33, k34, k35, k36, k37,
	k40, k41, k42, k43, k44, k45, k46, k47,
	k50, k51, k52, k53, k54, k55, k56, k57,
	k60, k61, k62, k63, k64, k65, k66, k67,
	k70, k71, k72, k73, k74, k75, k76, k77,
	k80, k81, k82, k83, k84, k85, k86, k87,
	k90, k91, k92, k93, k94, k95, k96, k97,
	ka0, ka1, ka2, ka3, ka4, ka5, ka6, ka7,

	/*
	 * NOTE: NSTART of the main tree starts here, the following are offsets for `T[]`
	 */

	// output roots/entrypoints
	o00, o01, o02, o03, o04, o05, o06, o07,
	o10, o11, o12, o13, o14, o15, o16, o17,
	o20, o21, o22, o23, o24, o25, o26, o27,
	o30, o31, o32, o33, o34, o35, o36, o37,
	o40, o41, o42, o43, o44, o45, o46, o47,
	o50, o51, o52, o53, o54, o55, o56, o57,
	o60, o61, o62, o63, o64, o65, o66, o67,
	o70, o71, o72, o73, o74, o75, o76, o77,
	o80, o81, o82, o83, o84, o85, o86, o87,
	o90, o91, o92, o93, o94, o95, o96, o97,
	oa0, oa1, oa2, oa3, oa4, oa5, oa6, oa7,

	VSTART,

	/*
	 * For this version on spongert, the buffer is 11 bytes long and 11 permutation in both the absorbing and squeezing areas. The starting buffer of each permutation is tagged a P point
	 */

//_NSTART = _PSTART+1+(8*11*11*2 * 15/*permutation split into 15 chunks*/),
//_NSTART = _PSTART+1+(8*11*11*2 * 45/*permutation split into 45 chunks*/),
	VLAST = VSTART +
		   1 +//reserved for main entrypoint
		   (8 * 11 * // 88 bits wide
		    11 * 2 * // 44 rounds
		    45) + // 45 permutations per round
		   7, // alignment

	KSTART = k00, // first key
	NSTART = o00, // first node
	OSTART = o00, // first output

};

const char* allNames[] = {
	"ZERO", // reference value
	"ERROR", // un-initialised marker

	// input keys
	"k00", "k01", "k02", "k03", "k04", "k05", "k06", "k07",
	"k10", "k11", "k12", "k13", "k14", "k15", "k16", "k17",
	"k20", "k21", "k22", "k23", "k24", "k25", "k26", "k27",
	"k30", "k31", "k32", "k33", "k34", "k35", "k36", "k37",
	"k40", "k41", "k42", "k43", "k44", "k45", "k46", "k47",
	"k50", "k51", "k52", "k53", "k54", "k55", "k56", "k57",
	"k60", "k61", "k62", "k63", "k64", "k65", "k66", "k67",
	"k70", "k71", "k72", "k73", "k74", "k75", "k76", "k77",
	"k80", "k81", "k82", "k83", "k84", "k85", "k86", "k87",
	"k90", "k91", "k92", "k93", "k94", "k95", "k96", "k97",
	"ka0", "ka1", "ka2", "ka3", "ka4", "ka5", "ka6", "ka7",

	// output roots
	"o00", "o01", "o02", "o03", "o04", "o05", "o06", "o07",
	"o10", "o11", "o12", "o13", "o14", "o15", "o16", "o17",
	"o20", "o21", "o22", "o23", "o24", "o25", "o26", "o27",
	"o30", "o31", "o32", "o33", "o34", "o35", "o36", "o37",
	"o40", "o41", "o42", "o43", "o44", "o45", "o46", "o47",
	"o50", "o51", "o52", "o53", "o54", "o55", "o56", "o57",
	"o60", "o61", "o62", "o63", "o64", "o65", "o66", "o67",
	"o70", "o71", "o72", "o73", "o74", "o75", "o76", "o77",
	"o80", "o81", "o82", "o83", "o84", "o85", "o86", "o87",
	"o90", "o91", "o92", "o93", "o94", "o95", "o96", "o97",
	"oa0", "oa1", "oa2", "oa3", "oa4", "oa5", "oa6", "oa7",
};
