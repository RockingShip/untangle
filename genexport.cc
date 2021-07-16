//#pragma GCC optimize ("O0") // optimize on demand

/*
 * genexport.cc
 *      Export the main sections of the database to text files that can be imported by `genimport`.
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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <stdlib.h>
#include <unistd.h>

#include "context.h"
#include "basetree.h"
#include "database.h"

/*
 * Resource context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} Application context
 */
context_t ctx;

/**
 * @date 2021-05-17 22:45:37
 *
 * Signal handlers
 *
 * Bump interval timer
 *
 * @param {number} sig - signal (ignored)
 */
void sigalrmHandler(int __attribute__ ((unused)) sig) {
	if (ctx.opt_timer) {
		ctx.tick++;
		alarm(ctx.opt_timer);
	}
}

/**
 * @date 2021-05-13 15:30:14
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 */
struct genexportContext_t {

	enum {
		/// @constant {number} Size of footprint for `tinyTree_t` in terms of uint64_t
		QUADPERFOOTPRINT = ((1 << MAXSLOTS) / 64)
	};

	/// @var {string} name of database
	const char *arg_databaseName;
	/// @var {string} name of metadata
	const char *arg_jsonName;
	/// @var {string} name of signature section
	const char *arg_signatureName;
	/// @var {string} name of swap section
	const char *arg_swapName;
	/// @var {string} name of member section
	const char *arg_memberName;

	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;

	/// @var {database_t} - Database store to place results
	database_t *pStore;

	genexportContext_t() {
		arg_databaseName  = NULL;
		arg_jsonName      = NULL;
		arg_signatureName = NULL;
		arg_swapName      = NULL;
		arg_memberName    = NULL;

		opt_force = 0;

		pStore = NULL;
	}

	uint32_t crc32Name(uint32_t crc32, const char *pName) {
		while (*pName) {
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));
			pName++;
		}
		return crc32;
	}

	/**
	 * @date 2021-06-08 21:01:18
	 *
	 * What `eval` does
	 */
	int main(void) {

		FILE     *f;
		json_t   *jOutput = json_object();
		uint32_t crc32;

		/*
		 * Write selection of header values
		 */
		json_object_set_new_nocheck(jOutput, "interleave", json_integer(pStore->interleave));

		/*
		 * Write signatures
		 */

		f = fopen(arg_signatureName, "w");
		if (!f)
			ctx.fatal("fopen(%s) returned: %m\n", arg_signatureName);

		crc32 = 0;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			const signature_t *pSignature = pStore->signatures + iSid;

			fprintf(f, "%s\t", pSignature->name);

			if (pSignature->flags & signature_t::SIGMASK_SAFE)
				fprintf(f, "S");
			if (pSignature->flags & signature_t::SIGMASK_PROVIDES)
				fprintf(f, "P");
			if (pSignature->flags & signature_t::SIGMASK_REQUIRED)
				fprintf(f, "R");

			fprintf(f, "\n");

			/*
			 * Update CRC
			 */

			assert(sizeof(pSignature->flags) == 1);

			crc32 = crc32Name(crc32, pSignature->name);

			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(pSignature->flags));

		}

		// close
		if (fclose(f)) {
			unlink(arg_signatureName);
			ctx.fatal("[fclose(%s,\"w\") returned: %m]\n", arg_signatureName);
		}

		json_t *jSignature = json_object();
		json_object_set_new_nocheck(jSignature, "filename", json_string(arg_signatureName));
		json_object_set_new_nocheck(jSignature, "count", json_integer(pStore->numSignature - 1));
		json_object_set_new_nocheck(jSignature, "crc", json_integer(crc32));
		json_object_set_new_nocheck(jOutput, "signature", jSignature);

		/*
		 * Write swaps
		 */

		f = fopen(arg_swapName, "w");
		if (!f)
			ctx.fatal("fopen(%s) returned: %m\n", arg_swapName);

		crc32 = 0;
		for (unsigned iSwap = 1; iSwap < pStore->numSwap; iSwap++) {
			const swap_t *pSwap = pStore->swaps + iSwap;

			for (unsigned j = 0; j < pSwap->MAXENTRY; j++) {
				if (pSwap->tids[j]) {
					if (j)
						fprintf(f, "\t");
					fprintf(f, "%s", pStore->fwdTransformNames[pSwap->tids[j]]);

					// update crc
					crc32 = crc32Name(crc32, pStore->fwdTransformNames[pSwap->tids[j]]);
				}
			}
			fprintf(f, "\n");
		}

		// close
		if (fclose(f)) {
			unlink(arg_swapName);
			ctx.fatal("[fclose(%s,\"w\") returned: %m]\n", arg_swapName);
		}

		json_t *jSwap = json_object();
		json_object_set_new_nocheck(jSwap, "filename", json_string(arg_swapName));
		json_object_set_new_nocheck(jSwap, "count", json_integer(pStore->numSwap - 1));
		json_object_set_new_nocheck(jSwap, "crc", json_integer(crc32));
		json_object_set_new_nocheck(jOutput, "swap", jSwap);

		/*
		 * Write members
		 */

		f = fopen(arg_memberName, "w");
		if (!f)
			ctx.fatal("fopen(%s) returned: %m\n", arg_memberName);

		crc32 = 0;
		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			const member_t *pMember = pStore->members + iMid;

			fprintf(f, "%s\t", pMember->name);

			if (pMember->flags & member_t::MEMMASK_SAFE)
				fprintf(f, "S");
			if (pMember->flags & member_t::MEMMASK_COMP)
				fprintf(f, "C");
			if (pMember->flags & member_t::MEMMASK_LOCKED)
				fprintf(f, "L");
			if (pMember->flags & member_t::MEMMASK_DEPR)
				fprintf(f, "D");
			if (pMember->flags & member_t::MEMMASK_DELETE)
				fprintf(f, "X");

			fprintf(f, "\n");

			/*
			 * Update CRC
			 */

			assert(sizeof(pMember->flags) == 1);

			__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(pMember->sid));
			__asm__ __volatile__ ("crc32l %1, %0" : "+r"(crc32) : "rm"(pMember->tid));

			crc32 = crc32Name(crc32, pMember->name);

			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(pMember->flags));

		}

		// close
		if (fclose(f)) {
			unlink(arg_memberName);
			ctx.fatal("[fclose(%s,\"w\") returned: %m]\n", arg_memberName);
		}

		json_t *jMember = json_object();
		json_object_set_new_nocheck(jMember, "filename", json_string(arg_signatureName));
		json_object_set_new_nocheck(jMember, "count", json_integer(pStore->numSignature - 1));
		json_object_set_new_nocheck(jMember, "crc", json_integer(crc32));
		json_object_set_new_nocheck(jOutput, "member", jMember);

		/*
		 * Write JSON
		 */

		f = fopen(arg_jsonName, "w");
		if (!f)
			ctx.fatal("fopen(%s) returned: %m\n", arg_jsonName);

		fprintf(f, "%s\n", json_dumps(jOutput, JSON_PRESERVE_ORDER | JSON_COMPACT));

		if (fclose(f))
			ctx.fatal("fclose(%s) returned: %m\n", arg_jsonName);

		return 0;
	}

};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {genexportContext_t} Application context
 */
genexportContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <database.db> <export.json> <signature.lst> <swap.lst> <member.lst>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t   --force\n");
		fprintf(stderr, "\t-q --quiet\n");
		fprintf(stderr, "\t-v --verbose\n");
		fprintf(stderr, "\t   --timer=<seconds> [default=%d]\n", ctx.opt_timer);
	}
}

/**
 * @date 2021-05-13 15:28:31
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
			LO_HELP     = 1, LO_DEBUG, LO_FORCE, LO_TIMER, LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",   1, 0, LO_DEBUG},
			{"force",   0, 0, LO_FORCE},
			{"help",    0, 0, LO_HELP},
			{"quiet",   2, 0, LO_QUIET},
			{"timer",   1, 0, LO_TIMER},
			{"verbose", 2, 0, LO_VERBOSE},
			{NULL,      0, 0, 0}
		};

		char optstring[64];
		char *cp                            = optstring;
		int  option_index                   = 0;

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
		case LO_QUIET:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose - 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = (unsigned) strtoul(optarg, NULL, 10);
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? (unsigned) strtoul(optarg, NULL, 10) : ctx.opt_verbose + 1;
			break;

		case '?':
			ctx.fatal("Try `%s --help' for more information.\n", argv[0]);
		default:
			ctx.fatal("getopt returned character code %d\n", c);
		}
	}

	if (argc - optind >= 1)
		app.arg_databaseName = argv[optind++];
	if (argc - optind >= 1)
		app.arg_jsonName = argv[optind++];
	if (argc - optind >= 1)
		app.arg_signatureName = argv[optind++];
	if (argc - optind >= 1)
		app.arg_swapName = argv[optind++];
	if (argc - optind >= 1)
		app.arg_memberName = argv[optind++];

	if (app.arg_memberName == NULL) {
		usage(argv, false);
		exit(1);
	}

		/*
	 * None of the outputs may exist
	 */
	if (!app.opt_force) {
		struct stat sbuf;
		if (!stat(app.arg_jsonName, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", app.arg_jsonName);
		if (!stat(app.arg_signatureName, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", app.arg_signatureName);
		if (!stat(app.arg_swapName, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", app.arg_swapName);
		if (!stat(app.arg_memberName, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", app.arg_memberName);
	}

	/*
	 * Main
	 */

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

		// Open database
	database_t db(ctx);

	db.open(app.arg_databaseName);

	app.pStore   = &db;

	// display system flags when database was created
	if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] DB FLAGS [%s]\n", ctx.timeAsString(), ctx.flagsToText(db.creationFlags));

	return app.main();
}
