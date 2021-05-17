/*
 * genvalidatespongent.js
 *	Generate 1000 test cases for `validatespongent.h`
 *
 * Perform:
 *	echo -n -e '\xe4\xf6\x24\xe2\x33\x9a\x7f\x0a\x4b\xbc\xe6' | ./spongent -
 * Output:
 *	validate("e4f624e2339a7f0a4bbce6","BF66C0A41733BC2C842A9C");
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

	let keyData = new Uint8Array(11);

	for (let i = 0; i < keyData.length; i++)
		keyData[i] = Math.round(Math.random() * 255);

	/*
	 * extract K parts
	 */
	let kStr = "";
	for (let i = 0; i < 11; i++)
		kStr += "\\x" + (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

	/*
	 * Construct command
	 */

	let cmd = "echo -n -e '" + kStr + "' | ./spongent -";

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
		for (let i = 0; i < 11; i++)
			kStr += (keyData[i] >> 4).toString(16) + (keyData[i] & 15).toString(16);

		// get first 88 bits (=11 bytes, =22chars)
		let oStr = stdout.substr(0, 22);

		// output and append internal md5sum header
		process.stdout.write("validate(\"" + kStr + "\",\"" + oStr + "\");\n");
	});

}