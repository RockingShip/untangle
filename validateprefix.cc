//#pragma GCC optimize ("O0") // optimize on demand

/*
 * validateprefix.cc
 * 	Brute force test if all prefixes for `baseTree_t::saveString()` are correct
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

char *encodePrefix(char *pName, unsigned value) {

	// creating is right-to-left. Storage to reverse
	char stack[10], *pStack = stack;

	// push terminator
	*pStack++ = 0;

	// process the value
	do {
		*pStack++ = 'A' + (value % 26);
		value /= 26;
	} while (value);

	// append, including trailing zero
	do {
	} while ((*pName++ = *--pStack));

	return pName - 1; // return location of trailing zero
}

int decodeNode(const char *pName) {

	if (isdigit(*pName))
		return *pName - '0';

	unsigned value = 0;

	while (isupper(*pName))
		value = value * 26 + *pName++ - 'A';

	if (!isdigit(*pName))
		return -1;

	return (value + 1) * 10 + *pName - '0';
}

int decodeSlot(const char *pName) {

	if (islower(*pName))
		return *pName - 'a';

	unsigned value = 0;

	while (isupper(*pName))
		value = value * 26 + *pName++ - 'A';

	if (!islower(*pName))
		return -1;

	return (value + 1) * 26 + *pName - 'a';
}

int main(int argc, char *const *argv) {
	setlinebuf(stdout);

	static char name[32];

	for (int i = 10; i < 10000000; i++) {

		// base prefix
		char *pName = encodePrefix(name, (i - 10) / 10);

		// append slot
		*pName++ = '0' + (i % 10);
		*pName   = 0;

		int value = decodeNode(name);

		if (i != value) {
			fprintf(stderr, "prefix failed for %d. name=%s value=%d\n", i, name, value);
			exit(1);
		}
	}

	printf("decodeNode(\"Z9\")=%d\n", decodeNode("Z9"));
	printf("decodeNode(\"ZZ9\")=%d\n", decodeNode("ZZ9"));
	printf("decodeNode(\"ZZZ9\")=%d\n", decodeNode("ZZZ9"));
	printf("decodeNode(\"ZZZZ9\")=%d\n", decodeNode("ZZZZ9"));

	printf("decodeSlot(\"Za\")=%d\n", decodeSlot("Za"));
	printf("decodeSlot(\"ZZa\")=%d\n", decodeSlot("ZZa"));
	printf("decodeSlot(\"ZZZa\")=%d\n", decodeSlot("ZZZa"));
	printf("decodeSlot(\"ZZZZa\")=%d\n", decodeSlot("ZZZZa"));

	return 0;
}