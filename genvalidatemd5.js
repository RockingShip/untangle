/*
 * genvalidatemd5.js
 *	Generate 1000 test cases for `validatemd5.h`
 *
 * Perform:
 *	echo -n -e '\xa7\x72\x70\x39\x7e\x51\x27\x33\xe7\xcb\xc5\xc2\xf7\xfa\xd6\x5c\x13\xc9\xcf\xf8\xbb\x52\xe0\x6c\x0a\x24\xf5\x55\xf8\xe8\x5e\xd7\xa8\xd0\xcc\xef\x2a\x55\x09\xae\xd3\xbc\x2c\x8f\x46\xcf\x7b\x07\x0a\x7b\xeb\xa4\x55\xb7\xd1' | md5sum -
 * Output:
 *	validate("a77270397e512733e7cbc5c2f7fad65c13c9cff8bb52e06c0a24f555f8e85ed7a8d0ccef2a5509aed3bc2c8f46cf7b070a7beba455b7d1 80b801000000000000","1e6ab2288ea73e234b42dfb7ec98b7ec");
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

'use strict';

const {exec} = require('child_process');

for (let iLoop = 0; iLoop < 1000; iLoop++) {

	/*
	 * Create 55 bytes randomness
	 */

	let keyData = new Uint8Array(55);

	for (let i = 0; i < keyData.length; i++)
		keyData[i] = Math.round(Math.random() * 255);

	/*
	 * extract K parts
	 */
	let kStr = "";
	for (let i = 0; i < 55; i++)
		kStr += "\\x" + (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

	/*
	 * Construct command
	 */

	let cmd = "echo -n -e '" + kStr + "' | md5sum -";

	console.log(cmd);

	/*
	 * Execute command, and display result
	 */

	exec(cmd, (error, stdout, stderr) => {
		if (error) {
			console.error("exec error: " + error);
			process.exit(1);
		}

		/*
		 * Stringify kStr, iStr, oStr
		 */

		let kStr = "";
		for (let i = 0; i < 55; i++)
			kStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

		// get first 128 bits (=16 bytes, =32chars)
		let oStr = stdout.substr(0, 32);

		// output and append internal md5sum header
		process.stdout.write("validate(\"" + kStr + " 80b801000000000000\",\"" + oStr + "\");\n");
	});

}