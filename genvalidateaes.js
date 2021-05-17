/*
 * genvalidateaes.js
 *	Generate 1000 test cases for `validateaes.h`
 *
 * Perform:
 *	echo -n -e '\x37\xc7\x6c\xeb\xfd\xde\xc0\x90\x86\x06\x0d\xa6\x2f\xd0\xfc\x10' | openssl enc -aes128 -e -K 88c261421719c96fd71652f5d38923c3 -iv 0 -nosalt | od -t x1 -A none
 * Output:
 *	validate("88c261421719c96fd71652f5d38923c3 37c76cebfddec09086060da62fd0fc10","f93094899ba8c856044ecfdd1a02a521");
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
	 * Create 2x 128 bits randomness
	 */

	let keyData = new Uint8Array(2 * 16);

	for (let i = 0; i < keyData.length; i++)
		keyData[i] = Math.round(Math.random() * 255);

	/*
	 * extract K and I parts
	 */
	let kStr = "";
	for (let i = 0; i < 16; i++)
		kStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

	let iStr = "";
	for (let i = 16; i < 32; i++)
		iStr += "\\x" + (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

	/*
	 * Construct command
	 */

	let cmd = "echo -n -e '" + iStr + "' | openssl enc -aes128 -e -K " + kStr + " -iv 0 -nosalt | od -t x1 -A none";

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
		for (let i = 0; i < 16; i++)
			kStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

		let iStr = "";
		for (let i = 16; i < 32; i++)
			iStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

		let oStr = stdout.replace(/\s/g, ""); // remove whitespace
		// get first 128 bits (=16 bytes, =32chars)
		oStr = oStr.substr(0, 32);

		process.stdout.write("validate(\"" + kStr + " " + iStr + "\",\"" + oStr + "\");\n");
	});

}