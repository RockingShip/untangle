//#pragma GCC optimize ("O0") // optimize on demand

/*
 * ksave.cc
 *      Export a baseTree_t` file as textual json file (or C code)
 *      Store the data nodes as 'data' tag.
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

#include <string>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <stdlib.h>
#include <unistd.h>

#include "context.h"
#include "basetree.h"

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
struct ksaveContext_t {

	/// @var {number} --code, Output as C code
	unsigned opt_code;
	/// @var {number} --force, force overwriting of outputs if already exists
	unsigned opt_force;

	/// @var {baseTree_t*} input tree
	baseTree_t *pInputTree;

	ksaveContext_t() {
		opt_code  = 0;
		opt_force = 0;
	}

	/**
	 * @date 2021-05-20 23:15:36
	 *
	 * Main entrypoint
	 */
	int main(const char *outputFilename, const char *inputFilename) {

		/*
		 * Open input tree
		 */
		baseTree_t *pTree = new baseTree_t(ctx);

		if (pTree->loadFile(inputFilename)) {
			json_t *jError = json_object();
			json_object_set_new_nocheck(jError, "error", json_string_nocheck("failed to load"));
			json_object_set_new_nocheck(jError, "filename", json_string(inputFilename));
			ctx.fatal("%s\n", json_dumps(jError, JSON_PRESERVE_ORDER | JSON_COMPACT));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
			json_t *jResult = json_object();
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(inputFilename));
			pTree->headerInfo(jResult);
			pTree->extraInfo(jResult);
			fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
			json_delete(jResult);
		}

		/*
		 * Save the tree
		 */
		if (!opt_code) {
			json_t *jOutput = json_object();

			// add tree meta
			pTree->headerInfo(jOutput);
			// add names/history
			pTree->extraInfo(jOutput);

			// roots
			json_t *jData = json_object();

			for (unsigned iRoot = 0; iRoot < pTree->numRoots; iRoot++) {
				if (pTree->roots[iRoot] != iRoot) {
					// export root
					std::string expr = pTree->saveString(pTree->roots[iRoot]);
					// save
					json_object_set_new_nocheck(jData, pTree->rootNames[iRoot].c_str(), json_string_nocheck(expr.c_str()));
				}
			}

			// add data as strings
			json_object_set_new_nocheck(jOutput, "data", jData);

			// system
			if (pTree->system) {
				std::string expr = pTree->saveString(pTree->system);
				json_object_set_new_nocheck(jOutput, "system", json_string_nocheck(expr.c_str()));
			}

			FILE *f = fopen(outputFilename, "w");
			if (!f)
				ctx.fatal("fopen(%s) returned: %m\n", outputFilename);

			fprintf(f, "%s\n", json_dumps(jOutput, JSON_PRESERVE_ORDER | JSON_COMPACT));

			if (fclose(f))
				ctx.fatal("fclose(%s) returned: %m\n", outputFilename);

			return 0;
		}

		FILE *f = fopen(outputFilename, "w");
		if (!f)
			ctx.fatal("fopen(%s) returned: %m\n", outputFilename);

		/*
		 * Save the tree as C code
		 */
		fprintf(f, "({\n");
		fprintf(f, "unsigned\n");

		fprintf(f, "kstart=%d,\n", pTree->kstart);
		fprintf(f, "ostart=%d,\n", pTree->ostart);
		fprintf(f, "estart=%d,\n", pTree->estart);
		fprintf(f, "nstart=%d,\n", pTree->nstart);
		fprintf(f, "ncount=%d,\n", pTree->ncount);
		fprintf(f, "numRoots=%d,\n", pTree->numRoots);

		/*
		 * Perform a node reference count
		 */
		uint32_t *pRootRef = pTree->allocMap();

		for (uint32_t iNode = 0; iNode < pTree->ncount; iNode++)
			pRootRef[iNode] = 0;

		for (uint32_t iRoot = 0; iRoot < pTree->numRoots; iRoot++)
			pRootRef[pTree->roots[iRoot] & ~IBIT]++;

		pRootRef[pTree->system & ~IBIT]++;

		fprintf(f, "N[]=");
		for (uint32_t iKey = 0; iKey < pTree->kstart; iKey++)
			fprintf(f, "%c%d", (iKey ? ',' : '{'), iKey);
		fprintf(f, ",\n");

		for (uint32_t iKey = pTree->kstart; iKey < pTree->nstart; iKey++) {
			fprintf(f, "%s,", pTree->keyNames[iKey].c_str());
		}
		fprintf(f, "\n");

		for (uint32_t iNode = pTree->nstart; iNode < pTree->ncount; iNode++) {
			// write labels
			if (pRootRef[iNode]) {
				fprintf(f, "// ");
				// scan roots
				for (uint32_t iRoot = 0; iRoot < pTree->numRoots; iRoot++) {
					int32_t R = pTree->roots[iRoot];

					if ((R & ~IBIT) == iNode) {
						fprintf(f, "%s", pTree->rootNames[iRoot].c_str());
						if (R & IBIT)
							fprintf(f, "~");
						fprintf(f, ":");
					}
				}
				// system
				if ((pTree->system & ~IBIT) == iNode) {
					fprintf(f, "system");
					if (pTree->system & IBIT)
						fprintf(f, "~");
					fprintf(f, ":");
				}
				fprintf(f, "\n");
			}

			const baseNode_t *pNode = pTree->N + iNode;
			const uint32_t   Q      = pNode->Q;
			const uint32_t   Tu     = pNode->T & ~IBIT;
			const uint32_t   Ti     = pNode->T & IBIT;
			const uint32_t   F      = pNode->F;

			if (Ti) {
				fprintf(f, "/*%d*/N[%d]?!N[%d]:N[%d],\n", iNode, Q, Tu, F);
			} else {
				fprintf(f, "/*%d*/N[%d]?N[%d]:N[%d],\n", iNode, Q, Tu, F);
			}
		}
		fprintf(f, "}");

		// roots
		for (unsigned iRoot = 0; iRoot < pTree->numRoots; iRoot++) {
			uint32_t R = pTree->roots[iRoot];

			if (R != iRoot) {
				fprintf(f, ",\n");

				if (R & IBIT) {
					fprintf(f, "%s=N[%d]^0x80000000", pTree->rootNames[iRoot].c_str(), R & ~IBIT);
				} else {
					fprintf(f, "%s=N[%d]", pTree->rootNames[iRoot].c_str(), R);
				}
			}
		}

		// system
		if (pTree->system) {
			fprintf(f, ",\n");
			fprintf(f, "system=N[%d]", pTree->system);
		}

		fprintf(f, "\n})\n");

		pTree->freeMap(pRootRef);
		delete pTree;

		if (fclose(f))
			ctx.fatal("fclose(%s) returned: %m\n", outputFilename);

		return 0;
	}


};

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {ksaveContext_t} Application context
 */
ksaveContext_t app;

void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <output.json> <input.dat>\n", argv[0]);
	if (verbose) {
		fprintf(stderr, "\t-c --code\n");
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
			LO_HELP = 1, LO_DEBUG, LO_TIMER, LO_FORCE,
			LO_CODE = 'c', LO_QUIET = 'q', LO_VERBOSE = 'v'
		};

		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"code",    0, 0, LO_CODE},
			{"debug",   1, 0, LO_DEBUG},
			{"force",   0, 0, LO_FORCE},
			{"help",    0, 0, LO_HELP},
			{"quiet",   2, 0, LO_QUIET},
			{"timer",   1, 0, LO_TIMER},
			{"verbose", 2, 0, LO_VERBOSE},
			//
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
		case LO_CODE:
			app.opt_code++;
			break;
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

	char *outputFilename;
	char *inputFilename;

	if (argc - optind >= 2) {
		outputFilename = argv[optind++];
		inputFilename  = argv[optind++];
	} else {
		usage(argv, false);
		exit(1);
	}

	/*
	 * None of the outputs may exist
	 */
	if (!app.opt_force) {
		struct stat sbuf;
		if (!stat(outputFilename, &sbuf))
			ctx.fatal("%s already exists. Use --force to overwrite\n", outputFilename);
	}

	/*
	 * Main
	 */

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	return app.main(outputFilename, inputFilename);
}
