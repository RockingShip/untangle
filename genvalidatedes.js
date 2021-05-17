/*
 * genvalidatedes.js
 *	Generate 1000 test cases for `validatedes.h`
 *
 * Perform:
 *	echo -n -e '\x29\xcd\x7b\x70\xba\xe6\xea\x87' | openssl enc -des -e -K b17118840b83dc0c -iv 0 -nosalt | od -t x1 -A none
 * Output:
 *	result-> validate("b17118840b83dc0c 29cd7b70bae6ea87","c6c40cb34e905d7a");
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
	 * Create 2x 64 bits randomness
	 */

	let keyData = new Uint8Array(2 * 8);

	for (let i = 0; i < keyData.length; i++)
		keyData[i] = Math.round(Math.random() * 255);

	/*
	 * extract K and I parts
	 */
	let kStr = "";
	for (let i = 0; i < 8; i++)
		kStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

	let iStr = "";
	for (let i = 8; i < 16; i++)
		iStr += "\\x" + (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

	/*
	 * Construct command
	 */

	let cmd = "echo -n -e '" + iStr + "' | openssl enc -des -e -K " + kStr + " -iv 0 -nosalt | od -t x1 -A none";

	// console.log(cmd);

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
		for (let i = 0; i < 8; i++)
			kStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

		let iStr = "";
		for (let i = 8; i < 16; i++)
			iStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

		let oStr = stdout.replace(/\s/g, ""); // remove whitespace
		// get first 64 bits (=8 bytes, =16chars)
		oStr = oStr.substr(0, 16);

		process.stdout.write("validate(\"" + kStr + " " + iStr + "\",\"" + oStr + "\");\n");
	});

}