# pragma GCC optimize ("O3") // optimize on demand

/*
 * @date 2021-06-17 17:58:17
 *
 * Find prime structures for signatures.
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
#include <errno.h>
#include <getopt.h>
#include <jansson.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include "config.h"
#include "database.h"
#include "dbtool.h"
#include "generator.h"
#include "metrics.h"
#include "restartdata.h"
#include "tinytree.h"

/**
 * @date 2020-03-14 11:10:15
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genprimeContext_t : dbtool_t {

	enum {
		/// @constant {number} - `--text` modes
		OPTTEXT_WON = 1,
		OPTTEXT_COMPARE = 2,
		OPTTEXT_BRIEF = 3,
		OPTTEXT_VERBOSE = 4,

	};

	/*
	 * User specified program arguments and options
	 */

	/// @var {string} name of input database
	const char *arg_inputDatabase;
	/// @var {number} Tree size in nodes to be generated for this invocation;
	unsigned arg_numNodes;
	/// @var {string} name of output database
	const char *arg_outputDatabase;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;
	/// @var {number} Invoke generator for new candidates
	unsigned opt_generate;
	/// @var {string} name of file containing primes
	const char *opt_load;
	/// @var {number} save level-1 indices (hintIndex, signatureIndex, ImprintIndex) and level-2 index (imprints)
	unsigned opt_saveIndex;
	/// @var {number} Sid range upper bound
	unsigned opt_sidHi;
	/// @var {number} Sid range lower bound
	unsigned opt_sidLo;
	/// @var {number} task Id. First task=1
	unsigned opt_taskId;
	/// @var {number} Number of tasks / last task
	unsigned opt_taskLast;
	/// @var {number} --text, textual output instead of binary database
	unsigned opt_text;
	/// @var {number} generator upper bound
	uint64_t opt_windowHi;
	/// @var {number} generator lower bound
	uint64_t opt_windowLo;

	/// @var {footprint_t[]} - Evaluator for forward transforms
	footprint_t *pEvalFwd;
	/// @var {footprint_t[]} - Evaluator for reverse transforms
	footprint_t *pEvalRev;
	/// @var {uint16_t} - prime structure scores fpr comparison
	uint16_t *pPrimeScores;
	/// @var {tinyTree_t} - prime structure trees for comparison
	tinyTree_t **pPrimeTrees;
	/// @var {database_t} - Database store to place results
	database_t *pStore;


	/// @var {number} - THE generator
	generatorTree_t generator;
	/// @var {number} - Number of prime signatures found
	unsigned numPrime;
	/// @var {number} `foundTree()` duplicate by name
	unsigned skipDuplicate;
	/// @var {number} `foundTree()` too large for signature
	unsigned skipScore;

	/**
	 * Constructor
	 */
	genprimeContext_t(context_t &ctx) : dbtool_t(ctx), generator(ctx) {
		// arguments and options
		arg_inputDatabase = NULL;
		arg_numNodes = 0;
		arg_outputDatabase = NULL;
		opt_force = 0;
		opt_generate = 1;
		opt_saveIndex = 1;
		opt_taskId = 0;
		opt_taskLast = 0;
		opt_load = NULL;
		opt_sidHi = 0;
		opt_sidLo = 0;
		opt_text = 0;
		opt_windowHi = 0;
		opt_windowLo = 0;

		pEvalFwd = NULL;
		pEvalRev = NULL;
		pPrimeScores = NULL;
		pPrimeTrees = NULL;
		pStore = NULL;


		numPrime = 0;
		skipDuplicate = 0;
		skipScore = 0;
	}

	/**
	 * @date 2021-06-18 00:09:37
	 *
	 * Break tree into smaller components and test they are all prime
	 */
	bool testHeadTail(const tinyTree_t &treeR, const char *pNameR) {

		assert(!(treeR.root & IBIT));

		/*
		 * @date 2020-03-29 23:16:43
		 *
		 * Reserved root entries
		 *
		 * `"N[0] = 0?!0:0"` // zero value, zero QnTF operator, zero reference
		 * `"N[a] = 0?!0:a"` // self reference
		 */
		if (treeR.root == 0 || treeR.root == tinyTree_t::TINYTREE_KSTART) {
			return true;
		}

		/*
		 * Single node trees are always prime
		 */
		if (treeR.root == tinyTree_t::TINYTREE_NSTART)
			return true;

		assert(treeR.root > tinyTree_t::TINYTREE_NSTART);

		tinyTree_t tree(ctx);
		tinyTree_t tree2(ctx);

		/*
		 * @date 2021-06-18 00:11:37
		 * Check all nodes/tails, except root because that is candidate
		 */
		for (unsigned iTail = tinyTree_t::TINYTREE_NSTART; iTail < treeR.root; iTail++) {
			// prepare tree
			tree.N[iTail] = treeR.N[iTail];
			tree.root = iTail;
			tree.count = iTail + 1;

			// lookup head
			unsigned sid = 0;
			unsigned tid = 0;
			pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);

			if (sid == 0)
				return false;

			signature_t *pSig = pStore->signatures + sid;
			if (pSig->prime[0] == 0)
				return false;

			// remove skin of tree
			char skin[MAXSLOTS + 1];
			char name[tinyTree_t::TINYTREE_NAMELEN + 1];
			tree.encode(tree.root, name, skin);

			/*
			 * @date 2021-06-18 21:29:50
			 *
			 * NOTE/WARNING the extracted component may have non-normalised dyadic ordering
			 * because in the context of the original trees, the endpoints were locked by the now removed node
			 */
			tree2.decodeSafe(name);
			// structure is now okay
			tree2.encode(tree2.root, name, skin);
			// endpoints are now okay

			// does it match
			if (strcmp(name, pSig->prime) != 0)
				return false; // no
		}

		/*
		 * @date 2020-04-01 22:30:09
		 *
		 * check all heads
		 */
		{
			// replace `hot` node with placeholder
			for (unsigned iHead = tinyTree_t::TINYTREE_NSTART; iHead < treeR.root; iHead++) {
				unsigned select = 1 << treeR.root | 1 << 0; // selected nodes to extract nodes
				unsigned nextPlaceholderPlaceholder = tinyTree_t::TINYTREE_KSTART;
				uint32_t what[tinyTree_t::TINYTREE_NEND];
				what[0] = 0; // replacement for zero

				// scan tree for needed nodes, ignoring `hot` node
				for (unsigned k = treeR.root; k >= tinyTree_t::TINYTREE_NSTART; k--) {
					if (k != iHead && (select & (1 << k))) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q = pNode->Q;
						const unsigned To = pNode->T & ~IBIT;
						const unsigned F = pNode->F;

						if (Q >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << Q;
						if (To >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << To;
						if (F >= tinyTree_t::TINYTREE_NSTART)
							select |= 1 << F;
					}
				}

				// prepare for extraction
				tree.clearTree();
				// remove `hot` node from selection
				select &= ~(1 << iHead);

				/*
				 * Extract head.
				 * Replacing references by placeholders changes dyadic ordering.
				 * `what[hot]` is not a reference but a placeholder
				 */
				for (unsigned k = tinyTree_t::TINYTREE_NSTART; k <= treeR.root; k++) {
					if (k != iHead && select & (1 << k)) {
						const tinyNode_t *pNode = treeR.N + k;
						const unsigned Q = pNode->Q;
						const unsigned To = pNode->T & ~IBIT;
						const unsigned Ti = pNode->T & IBIT;
						const unsigned F = pNode->F;

						// assign placeholder to endpoint or `hot`
						if (!(select & (1 << Q))) {
							what[Q] = nextPlaceholderPlaceholder++;
							select |= 1 << Q;
						}
						if (!(select & (1 << To))) {
							what[To] = nextPlaceholderPlaceholder++;
							select |= 1 << To;
						}
						if (!(select & (1 << F))) {
							what[F] = nextPlaceholderPlaceholder++;
							select |= 1 << F;
						}

						// mark replacement of old node
						what[k] = tree.count;
						select |= 1 << k;

						/*
						 * Reminder:
						 *  [ 2] a ? ~0 : b                  "+" OR
						 *  [ 6] a ? ~b : 0                  ">" GT
						 *  [ 8] a ? ~b : b                  "^" XOR
						 *  [ 9] a ? ~b : c                  "!" QnTF
						 *  [16] a ?  b : 0                  "&" AND
						 *  [19] a ?  b : c                  "?" QTF
						 */

						// perform dyadic ordering
						if (To == 0 && Ti && tree.compare(what[Q], tree, what[F]) > 0) {
							// reorder OR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (To == F && tree.compare(what[Q], tree, what[F]) > 0) {
							// reorder XOR
							tree.N[tree.count].Q = what[F];
							tree.N[tree.count].T = what[Q] ^ IBIT;
							tree.N[tree.count].F = what[Q];
						} else if (F == 0 && !Ti && tree.compare(what[Q], tree, what[To]) > 0) {
							// reorder AND
							tree.N[tree.count].Q = what[To];
							tree.N[tree.count].T = what[Q];
							tree.N[tree.count].F = 0;
						} else {
							// default
							tree.N[tree.count].Q = what[Q];
							tree.N[tree.count].T = what[To] ^ Ti;
							tree.N[tree.count].F = what[F];
						}

						tree.count++;
					}
				}

				// set root
				tree.root = tree.count - 1;

				/*
				 * Extracted tree
				 */

				// lookup head
				unsigned sid = 0;
				unsigned tid = 0;
				pStore->lookupImprintAssociative(&tree, pEvalFwd, pEvalRev, &sid, &tid);

				if (sid == 0) {
					/*
					 * this happens in 6n9 space where the current head is in 5n9 space, and outside the collection of sids
					 */
					return false;
				}

				signature_t *pSig = pStore->signatures + sid;
				if (pSig->prime[0] == 0) {
					/*
					 * No prime found
					 * That means that if/when there will be a prime, it will be larger than the head and never match
					 * Smart compare like with `baseTree_t` might be a thing, but thats too complicated for now
					 */
					return false;
				}

				// remove skin of tree
				char skin[MAXSLOTS + 1];
				char name[tinyTree_t::TINYTREE_NAMELEN + 1];
				tree.encode(tree.root, name, skin);

				// NOTE/WARNING the extracted component may have non-normalised dyadic ordering, see note above
				tree2.decodeSafe(name);
				// structure is now okay
				tree2.encode(tree2.root, name, skin);
				// endpoints are now okay

				// does it match
				if (strcmp(name, pSig->prime) == 0)
					continue; // match

				// no match
				return false;
			}
		}

		return true;
	}

	/**
	 * @date 2021-06-17 18:39:26
	 *
	 * Test if candidate is a prime structurecan be a signature group member and add when possible
	 *
	 * @date 2020-04-02 11:41:44
	 *
	 * for `signature_t`, only use `flags`, `size` and `firstMember`.
	 *
	 * @date 2020-04-15 11:02:46
	 *
	 * For now, collect members only based on size instead of `compareMember()`.
	 * Member properties still need to be discovered to make strategic decisions.
	 * Collecting members is too expensive to ask questions on missing members later.
	 *
	 * @param {generatorTree_t} treeR - candidate tree
	 * @param {string} pNameR - Tree name/notation
	 * @param {number} numPlaceholder - number of unique endpoints/placeholders in tree
	 * @param {number} numEndpoint - number of non-zero endpoints in tree
	 * @param {number} numBackRef - number of back-references
	 * @return {boolean} return `true` to continue with recursion (this should be always the case except for `genrestartdata`)
	 */
	bool foundTreePrime(const generatorTree_t &treeR, const char *pNameR, unsigned numPlaceholder, unsigned numEndpoint, unsigned numBackRef) {

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
			int perSecond = ctx.updateSpeed();

			if (perSecond == 0 || ctx.progress > ctx.progressHi) {
				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) | numPrime=%u | skipDuplicate=%u skipScore=%u | hash=%.3f",
					ctx.timeAsString(), ctx.progress, perSecond,
					numPrime,
					skipDuplicate, skipScore,
					(double) ctx.cntCompare / ctx.cntHash);
			} else {
				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numPrime=%u | skipDuplicate=%u skipScore=%u | hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond, (ctx.progress - treeR.windowLo) * 100.0 / (ctx.progressHi - treeR.windowLo), etaH, etaM, etaS,
					numPrime,
					skipDuplicate, skipScore,
					(double) ctx.cntCompare / ctx.cntHash, pNameR);
			}

			if (ctx.restartTick) {
				// passed a restart point
				fprintf(stderr, "\n");
				ctx.restartTick = 0;
			}

			ctx.tick = 0;
		}

		/*
		 * Find the matching signature group. It's layout only so ignore transformId.
		 */

		unsigned sid = 0;
		unsigned tid = 0;
		pStore->lookupImprintAssociative(&treeR, pEvalFwd, pEvalRev, &sid, &tid);

//		printf("@@ %d @ %s @\n", sid, pStore->signatures[sid].prime);
//		return true;

		if (sid == 0)
			return true; // not found

		signature_t *pSignature = pStore->signatures + sid;
		unsigned cmp = 0;
		unsigned scoreR = treeR.calcScoreName(pNameR);

		/*
		 * early-reject
		 */

		if (pSignature->prime[0]) {
			/*
			 * @date 2021-06-20 19:06:44
			 * Just like primes with component dependency chains, members can be larger than signatures
			 * Larger candidates will always be rejected, so reject now before doing expensive testing
			 * Grouping can be either by node size or score
			 */

			if (scoreR > pPrimeScores[sid])
				cmp = '*'; // reject
			if (scoreR == pPrimeScores[sid]) {
				cmp = treeR.compare(treeR.root, *pPrimeTrees[sid], pPrimeTrees[sid]->root);

				if (cmp > 0) {
					// candidate is worse than signature, reject
					skipScore++; // count rejected primes
					cmp = '-';
				} else if (cmp == 0){
					// its a duplicate, reject
					skipDuplicate++; // count rejected primes
					cmp = '=';
				}
			}

		} else {
			/*
			 * @date 2021-06-20 19:15:49
			 * unsafe groups are a collection of everything that matches.
			 * however, keep the difference less than 2 nodes, primarily to protect 5n9 against populating <= 3n9
			 */
			if (treeR.count - tinyTree_t::TINYTREE_NSTART > pSignature->size + 2u)
				cmp = '*'; // reject
		}

		if (cmp) {
			if (opt_text == OPTTEXT_COMPARE)
				printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", ctx.progress, sid, cmp, pNameR, treeR.count - tinyTree_t::TINYTREE_NSTART, numPlaceholder, numEndpoint, numBackRef);
			return true;
		}

		/*
		 * Verify if candidate member is acceptable
		 */

		bool isPrime = testHeadTail(treeR, pNameR);

		if (!isPrime) {
			// candidate not prime. Reject
			cmp = '<';
		} else if (pSignature->prime[0] == 0) {
			// signature has no prime. Accept
			cmp = '>';
		} else if (scoreR > pPrimeScores[sid]) {
			// candidate is worse than signature, reject
			skipScore++; // count rejected primes
			cmp = '-';
		} else if (scoreR < pPrimeScores[sid]) {
			// candidate is better than signature, accept
			cmp = '+';
		} else {
			cmp = treeR.compare(treeR.root, *pPrimeTrees[sid], pPrimeTrees[sid]->root);

			if (cmp < 0) {
				// candidate is better than signature, accept
				cmp = '+';
			} else if (cmp > 0) {
				// candidate is worse than signature, reject
				skipScore++; // count rejected primes
				cmp = '-';
			} else {
				// its a duplicate, reject
				skipDuplicate++; // count rejected primes
				cmp = '=';
			}
		}

		if (opt_text == OPTTEXT_COMPARE)
			printf("%lu\t%u\t%c\t%s\t%u\t%u\t%u\t%u\n", ctx.progress, sid, cmp, pNameR, treeR.count - tinyTree_t::TINYTREE_NSTART, numPlaceholder, numEndpoint, numBackRef);

		if (cmp == '<' || cmp == '-' || cmp == '=')
			return true;  // lost challenge

		// won challenge
		if (opt_text == OPTTEXT_WON)
			printf("%s\n", pNameR);

		if (pSignature->prime[0] == 0) {
			// New prime
			numPrime++;
		}

		strcpy(pSignature->prime, pNameR);
		pPrimeTrees[sid]->decodeFast(pNameR);
		pPrimeScores[sid] = scoreR;
		return true;
	}

	/**
	 * @date 2020-03-22 01:00:05
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add primes to signatures to the database
	 */
	void /*__attribute__((optimize("O0")))*/ primesFromFile(void) {

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Reading primes from file\n", ctx.timeAsString());

		FILE *f = fopen(this->opt_load, "r");
		if (f == NULL)
			ctx.fatal("\n{\"error\":\"fopen('%s') failed\",\"where\":\"%s:%s:%d\",\"return\":\"%m\"}\n",
				  this->opt_load, __FUNCTION__, __FILE__, __LINE__);

		// apply settings for `--window`
		generator.windowLo = this->opt_windowLo;
		generator.windowHi = this->opt_windowHi;

		// reset ticker
		ctx.setupSpeed(0);
		ctx.tick = 0;
		skipDuplicate = skipScore = 0;

		char name[64];
		unsigned numPlaceholder, numEndpoint, numBackRef;

		// <name> [ <numPlaceholder> <numEndpoint> <numBackRef> ]
		for (;;) {
			static char line[512];

			if (::fgets(line, sizeof(line), f) == 0)
				break; // end-of-input

			name[0] = 0;
			int ret = ::sscanf(line, "%s %u %u %u\n", name, &numPlaceholder, &numEndpoint, &numBackRef);

			// calculate values
			unsigned newPlaceholder = 0, newEndpoint = 0, newBackRef = 0;
			unsigned beenThere = 0;
			for (const char *p = name; *p; p++) {
				if (::islower(*p)) {
					if (!(beenThere & (1 << (*p - 'a')))) {
						newPlaceholder++;
						beenThere |= 1 << (*p - 'a');
					}
					newEndpoint++;
				} else if (::isdigit(*p) && *p != '0') {
					newBackRef++;
				}
			}

			if (ret != 1 && ret != 4)
				ctx.fatal("\n{\"error\":\"bad/empty line\",\"where\":\"%s:%s:%d\",\"linenr\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress);
			if (ret == 4 && (numPlaceholder != newPlaceholder || numEndpoint != newEndpoint || numBackRef != newBackRef))
				ctx.fatal("\n{\"error\":\"line has incorrect values\",\"where\":\"%s:%s:%d\",\"linenr\":%lu}\n",
					  __FUNCTION__, __FILE__, __LINE__, ctx.progress);

			// test if line is within progress range
			// NOTE: first line has `progress==0`
			if ((generator.windowLo && ctx.progress < generator.windowLo) || (generator.windowHi && ctx.progress >= generator.windowHi)) {
				ctx.progress++;
				continue;
			}

			/*
			 * construct tree
			 */
			generator.decodeFast(name);

			/*
			 * call `foundTreePrime()`
			 */

			if (!foundTreePrime(generator, name, newPlaceholder, newEndpoint, newBackRef))
				break;

			ctx.progress++;
		}

		fclose(f);

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "[%s] Read %lu primes. numSignature=%u(%.0f%%) numPrime=%u | skipDuplicate=%u skipScore=%u\n",
				ctx.timeAsString(),
				ctx.progress,
				pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				numPrime,
				skipDuplicate, skipScore);
	}

	/**
	 * @date 2020-03-22 01:00:05
	 *
	 * Main entrypoint
	 *
	 * Create generator for given dataset and add newly unique signatures to the database
	 *
	 * @param {database_t} pStore - memory based database
	 */
	void /*__attribute__((optimize("O0")))*/ primesFromGenerator(void) {

		/*
		 * Apply window/task setting on generator
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
			if (this->opt_taskId || this->opt_taskLast) {
				if (this->opt_windowHi)
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%lu-%lu\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_windowLo, this->opt_windowHi);
				else
					fprintf(stderr, "[%s] INFO: task=%u,%u window=%lu-last\n", ctx.timeAsString(), this->opt_taskId, this->opt_taskLast, this->opt_windowLo);
			} else if (this->opt_windowLo || this->opt_windowHi) {
				if (this->opt_windowHi)
					fprintf(stderr, "[%s] INFO: window=%lu-%lu\n", ctx.timeAsString(), this->opt_windowLo, this->opt_windowHi);
				else
					fprintf(stderr, "[%s] INFO: window=%lu-last\n", ctx.timeAsString(), this->opt_windowLo);
			}
		}

		// apply settings for `--window`
		generator.windowLo = this->opt_windowLo;
		generator.windowHi = this->opt_windowHi;

		// apply restart data for > `4n9`
		unsigned ofs = 0;
		if (this->arg_numNodes > 4 && this->arg_numNodes < tinyTree_t::TINYTREE_MAXNODES)
			ofs = restartIndex[this->arg_numNodes][(ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0];
		if (ofs)
			generator.pRestartData = restartData + ofs;

		// reset progress
		if (generator.windowHi) {
			ctx.setupSpeed(generator.windowHi);
		} else {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, arg_numNodes);
			ctx.setupSpeed(pMetrics ? pMetrics->numProgress : 0);
		}
		ctx.tick = 0;
		skipDuplicate = skipScore = 0;

		/*
		 * Generate candidates
		 */
		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Generating candidates for %un%u%s\n", ctx.timeAsString(), arg_numNodes, MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE ? "-pure" : "");

		if (arg_numNodes == 0) {
			generator.root = 0; // "0"
			foundTreePrime(generator, "0", 0, 0, 0);
			generator.root = 1; // "a"
			foundTreePrime(generator, "a", 1, 1, 0);
		} else {
			unsigned endpointsLeft = arg_numNodes * 2 + 1;

			generator.initialiseGenerator(ctx.flags & context_t::MAGICMASK_PURE);
			generator.clearGenerator();
			generator.generateTrees(arg_numNodes, endpointsLeft, 0, 0, this, static_cast<generatorTree_t::generateTreeCallback_t>(&genprimeContext_t::foundTreePrime));
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		if (ctx.progress != ctx.progressHi && this->opt_windowLo == 0 && this->opt_windowHi == 0) {
			// can only test if windowing is disabled
			printf("{\"error\":\"progressHi failed\",\"where\":\"%s:%s:%d\",\"encountered\":%lu,\"expected\":%lu,\"numNode\":%u}\n",
			       __FUNCTION__, __FILE__, __LINE__, ctx.progress, ctx.progressHi, arg_numNodes);
		}

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] numSlot=%u pure=%u numNode=%u numCandidate=%lu numSignature=%u(%.0f%%) numPrime=%u | skipDuplicate=%u skipScore=%u\n",
				ctx.timeAsString(), MAXSLOTS, (ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0, arg_numNodes, ctx.progress,
				pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				numPrime,
				skipDuplicate, skipScore);
	}

};

/*
 * I/O context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {context_t} I/O context
 */
context_t ctx;

/*
 * Application context.
 * Needs to be global to be accessible by signal handlers.
 *
 * @global {genprimeContext_t} Application context
 */
genprimeContext_t app(ctx);

/**
 * @date 2020-03-11 23:06:35
 *
 * Signal handler
 *
 * Delete partially created database unless explicitly requested
 *
 * @param {number} sig - signal (ignored)
 */
void sigintHandler(int __attribute__ ((unused)) sig) {
	if (app.arg_outputDatabase) {
		remove(app.arg_outputDatabase);
	}
	exit(1);
}

/**
 * @date 2020-03-11 23:06:35
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
 * @date 2020-03-14 11:17:04
 *
 * Program usage. Keep this directly above `main()`
 *
 * @param {string[]} argv - program arguments
 * @param {boolean} verbose - set to true for option descriptions
 * @param {userArguments_t} args - argument context
 */
void usage(char *argv[], bool verbose) {
	fprintf(stderr, "usage: %s <input.db> <numnode> [<output.db>]\n", argv[0]);

	if (verbose) {
		fprintf(stderr, "\n");
		fprintf(stderr, "\t   --force                         Force overwriting of database if already exists\n");
		fprintf(stderr, "\t   --[no-]generate                 Invoke generator for new candidates [default=%s]\n", app.opt_generate ? "enabled" : "disabled");
		fprintf(stderr, "\t-h --help                          This list\n");
		fprintf(stderr, "\t   --imprintindexsize=<number>     Size of imprint index [default=%u]\n", app.opt_imprintIndexSize);
		fprintf(stderr, "\t   --interleave=<number>           Imprint index interleave [default=%u]\n", app.opt_interleave);
		fprintf(stderr, "\t   --load=<file>                   Read candidates from file instead of generating [default=%s]\n", app.opt_load ? app.opt_load : "");
		fprintf(stderr, "\t   --maximprint=<number>           Maximum number of imprints [default=%u]\n", app.opt_maxImprint);
		fprintf(stderr, "\t   --maxmember=<number>            Maximum number of members [default=%u]\n", app.opt_maxMember);
		fprintf(stderr, "\t   --memberindexsize=<number>      Size of member index [default=%u]\n", app.opt_memberIndexSize);
		fprintf(stderr, "\t   --[no-]paranoid                 Enable expensive assertions [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PARANOID) ? "enabled" : "disabled");
		fprintf(stderr, "\t   --prepare                       Prepare dataset for empty/unsafe groups\n");
		fprintf(stderr, "\t   --[no-]pure                     QTF->QnTF rewriting [default=%s]\n", (ctx.flags & context_t::MAGICMASK_PURE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-q --quiet                         Say less\n");
		fprintf(stderr, "\t   --ratio=<number>                Index/data ratio [default=%.1f]\n", app.opt_ratio);
		fprintf(stderr, "\t   --[no-]saveindex                Save with indices [default=%s]\n", app.opt_saveIndex ? "enabled" : "disabled");
		fprintf(stderr, "\t   --sid=[<low>,]<high>            Sid range upper bound  [default=%u,%u]\n", app.opt_sidLo, app.opt_sidHi);
		fprintf(stderr, "\t   --signatureindexsize=<number>   Size of signature index [default=%u]\n", app.opt_signatureIndexSize);
		fprintf(stderr, "\t   --task=sge                      Get task settings from SGE environment\n");
		fprintf(stderr, "\t   --task=<id>,<last>              Task id/number of tasks. [default=%u,%u]\n", app.opt_taskId, app.opt_taskLast);
		fprintf(stderr, "\t   --text                          Textual output instead of binary database\n");
		fprintf(stderr, "\t   --timer=<seconds>               Interval timer for verbose updates [default=%u]\n", ctx.opt_timer);
		fprintf(stderr, "\t   --[no-]unsafe                   Reindex imprints based on empty/unsafe signature groups [default=%s]\n", (ctx.flags & context_t::MAGICMASK_UNSAFE) ? "enabled" : "disabled");
		fprintf(stderr, "\t-v --truncate                      Truncate on database overflow\n");
		fprintf(stderr, "\t-v --verbose                       Say more\n");
		fprintf(stderr, "\t   --window=[<low>,]<high>         Upper end restart window [default=%lu,%lu]\n", app.opt_windowLo, app.opt_windowHi);
	}
}

/**
 * @date 2020-03-14 11:19:40
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

	/*
	 *  Process program options
	 */
	for (;;) {
		// Long option shortcuts
		enum {
			// long-only opts
			LO_DEBUG = 1,
			LO_FORCE,
			LO_GENERATE,
			LO_IMPRINTINDEXSIZE,
			LO_INTERLEAVE,
			LO_LOAD,
			LO_MAXIMPRINT,
			LO_MAXMEMBER,
			LO_MEMBERINDEXSIZE,
			LO_NOGENERATE,
			LO_NOPARANOID,
			LO_NOPURE,
			LO_NOSAVEINDEX,
			LO_NOUNSAFE,
			LO_PARANOID,
			LO_PURE,
			LO_RATIO,
			LO_SAVEINDEX,
			LO_SID,
			LO_SIGNATUREINDEXSIZE,
			LO_TASK,
			LO_TEXT,
			LO_TIMER,
			LO_TRUNCATE,
			LO_UNSAFE,
			LO_WINDOW,
			// short opts
			LO_HELP = 'h',
			LO_QUIET = 'q',
			LO_VERBOSE = 'v',
		};

		// long option descriptions
		static struct option long_options[] = {
			/* name, has_arg, flag, val */
			{"debug",              1, 0, LO_DEBUG},
			{"force",              0, 0, LO_FORCE},
			{"generate",           0, 0, LO_GENERATE},
			{"help",               0, 0, LO_HELP},
			{"imprintindexsize",   1, 0, LO_IMPRINTINDEXSIZE},
			{"interleave",         1, 0, LO_INTERLEAVE},
			{"load",               1, 0, LO_LOAD},
			{"maximprint",         1, 0, LO_MAXIMPRINT},
			{"maxmember",          1, 0, LO_MAXMEMBER},
			{"memberindexsize",    1, 0, LO_MEMBERINDEXSIZE},
			{"no-generate",        0, 0, LO_NOGENERATE},
			{"no-paranoid",        0, 0, LO_NOPARANOID},
			{"no-pure",            0, 0, LO_NOPURE},
			{"no-saveindex",       0, 0, LO_NOSAVEINDEX},
			{"no-unsafe",          0, 0, LO_NOUNSAFE},
			{"paranoid",           0, 0, LO_PARANOID},
			{"pure",               0, 0, LO_PURE},
			{"quiet",              2, 0, LO_QUIET},
			{"ratio",              1, 0, LO_RATIO},
			{"saveindex",          0, 0, LO_SAVEINDEX},
			{"sid",                1, 0, LO_SID},
			{"signatureindexsize", 1, 0, LO_SIGNATUREINDEXSIZE},
			{"task",               1, 0, LO_TASK},
			{"text",               2, 0, LO_TEXT},
			{"timer",              1, 0, LO_TIMER},
			{"truncate",           0, 0, LO_TRUNCATE},
			{"unsafe",             0, 0, LO_UNSAFE},
			{"verbose",            2, 0, LO_VERBOSE},
			{"window",             1, 0, LO_WINDOW},
			//
			{NULL,                 0, 0, 0}
		};

		char optstring[64];
		char *cp = optstring;
		int option_index = 0;

		/* construct optarg */
		for (int i = 0; long_options[i].name; i++) {
			if (isalpha(long_options[i].val)) {
				*cp++ = (char) long_options[i].val;

				if (long_options[i].has_arg != 0)
					*cp++ = ':';
				if (long_options[i].has_arg == 2)
					*cp++ = ':';
			}
		}

		*cp = '\0';

		// parse long options
		int c = getopt_long(argc, argv, optstring, long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case LO_DEBUG:
			ctx.opt_debug = ::strtoul(optarg, NULL, 0);
			break;
		case LO_FORCE:
			app.opt_force++;
			break;
		case LO_GENERATE:
			app.opt_generate++;
			break;
		case LO_HELP:
			usage(argv, true);
			exit(0);
		case LO_IMPRINTINDEXSIZE:
			app.opt_imprintIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_INTERLEAVE:
			app.opt_interleave = ::strtoul(optarg, NULL, 0);
			if (!getMetricsInterleave(MAXSLOTS, app.opt_interleave))
				ctx.fatal("--interleave must be one of [%s]\n", getAllowedInterleaves(MAXSLOTS));
			break;
		case LO_LOAD:
			app.opt_load = optarg;
			break;
		case LO_MAXIMPRINT:
			app.opt_maxImprint = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MAXMEMBER:
			app.opt_maxMember = ctx.dToMax(::strtod(optarg, NULL));
			break;
		case LO_MEMBERINDEXSIZE:
			app.opt_memberIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_NOGENERATE:
			app.opt_generate = 0;
			break;
		case LO_NOPARANOID:
			ctx.flags &= ~context_t::MAGICMASK_PARANOID;
			break;
		case LO_NOPURE:
			ctx.flags &= ~context_t::MAGICMASK_PURE;
			break;
		case LO_NOUNSAFE:
			ctx.flags &= ~context_t::MAGICMASK_UNSAFE;
			break;
		case LO_PARANOID:
			ctx.flags |= context_t::MAGICMASK_PARANOID;
			break;
		case LO_PURE:
			ctx.flags |= context_t::MAGICMASK_PURE;
			break;
		case LO_QUIET:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose - 1;
			break;
		case LO_RATIO:
			app.opt_ratio = strtof(optarg, NULL);
			break;
		case LO_NOSAVEINDEX:
			app.opt_saveIndex = 0;
			break;
		case LO_SAVEINDEX:
			app.opt_saveIndex = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_saveIndex + 1;
			break;
		case LO_SID: {
			unsigned m, n;

			int ret = sscanf(optarg, "%u,%u", &m, &n);
			if (ret == 2) {
				app.opt_sidLo = m;
				app.opt_sidHi = n;
			} else if (ret == 1) {
				app.opt_sidHi = m;
			} else {
				usage(argv, true);
				exit(1);
			}

			break;
		}
		case LO_SIGNATUREINDEXSIZE:
			app.opt_signatureIndexSize = ctx.nextPrime(::strtod(optarg, NULL));
			break;
		case LO_TASK:
			if (::strcmp(optarg, "sge") == 0) {
				const char *p;

				p = getenv("SGE_TASK_ID");
				app.opt_taskId = p ? atoi(p) : 0;
				if (app.opt_taskId < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_ID\n");
					exit(0);
				}

				p = getenv("SGE_TASK_LAST");
				app.opt_taskLast = p ? atoi(p) : 0;
				if (app.opt_taskLast < 1) {
					fprintf(stderr, "Missing environment SGE_TASK_LAST\n");
					exit(0);
				}

				if (app.opt_taskId < 1 || app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "sge id/last out of bounds: %u,%u\n", app.opt_taskId, app.opt_taskLast);
					exit(1);
				}

				// set ticker interval to 60 seconds
				ctx.opt_timer = 60;
			} else {
				if (sscanf(optarg, "%u,%u", &app.opt_taskId, &app.opt_taskLast) != 2) {
					usage(argv, true);
					exit(1);
				}
				if (app.opt_taskId == 0 || app.opt_taskLast == 0) {
					fprintf(stderr, "Task id/last must be non-zero\n");
					exit(1);
				}
				if (app.opt_taskId > app.opt_taskLast) {
					fprintf(stderr, "Task id exceeds last\n");
					exit(1);
				}
			}
			break;
		case LO_TEXT:
			app.opt_text = optarg ? ::strtoul(optarg, NULL, 0) : app.opt_text + 1;
			break;
		case LO_TIMER:
			ctx.opt_timer = ::strtoul(optarg, NULL, 0);
			break;
		case LO_UNSAFE:
			ctx.flags |= context_t::MAGICMASK_UNSAFE;
			break;
		case LO_VERBOSE:
			ctx.opt_verbose = optarg ? ::strtoul(optarg, NULL, 0) : ctx.opt_verbose + 1;
			break;
		case LO_WINDOW: {
			uint64_t m, n;

			int ret = sscanf(optarg, "%lu,%lu", &m, &n);
			if (ret == 2) {
				app.opt_windowLo = m;
				app.opt_windowHi = n;
			} else if (ret == 1) {
				app.opt_windowHi = m;
			} else {
				usage(argv, true);
				exit(1);
			}

			break;
		}

		case '?':
			fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
			exit(1);
		default:
			fprintf(stderr, "getopt_long() returned character code %d\n", c);
			exit(1);
		}
	}

	/*
	 * Program arguments
	 */
	if (argc - optind >= 1)
		app.arg_inputDatabase = argv[optind++];

	if (argc - optind >= 1) {
		char *endptr;

		errno = 0; // To distinguish success/failure after call
		app.arg_numNodes = ::strtoul(argv[optind++], &endptr, 0);

		// strip trailing spaces
		while (*endptr && isspace(*endptr))
			endptr++;

		// test for error
		if (errno != 0 || *endptr != '\0')
			app.arg_inputDatabase = NULL;
	}

	if (argc - optind >= 1)
		app.arg_outputDatabase = argv[optind++];

	if (app.arg_inputDatabase == NULL) {
		usage(argv, false);
		exit(1);
	}

	/*
	 * `--task` post-processing
	 */
	if (app.opt_taskId || app.opt_taskLast) {
		const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & context_t::MAGICMASK_PURE, app.arg_numNodes);
		if (!pMetrics)
			ctx.fatal("no preset for --task\n");

		// split progress into chunks
		uint64_t taskSize = pMetrics->numProgress / app.opt_taskLast;
		if (taskSize == 0)
			taskSize = 1;
		app.opt_windowLo = taskSize * (app.opt_taskId - 1);
		app.opt_windowHi = taskSize * app.opt_taskId;

		// last task is open ended in case metrics are off
		if (app.opt_taskId == app.opt_taskLast)
			app.opt_windowHi = 0;
	}
	if (app.opt_windowHi && app.opt_windowLo >= app.opt_windowHi) {
		fprintf(stderr, "--window low exceeds high\n");
		exit(1);
	}

	if (app.opt_windowLo || app.opt_windowHi) {
		if (app.arg_numNodes > tinyTree_t::TINYTREE_MAXNODES || restartIndex[app.arg_numNodes][(ctx.flags & context_t::MAGICMASK_PURE) ? 1 : 0] == 0) {
			fprintf(stderr, "No restart data for --window\n");
			exit(1);
		}
	}

	/*
	 * None of the outputs may exist
	 */

	if (app.arg_outputDatabase && !app.opt_force) {
		struct stat sbuf;

		if (!stat(app.arg_outputDatabase, &sbuf)) {
			fprintf(stderr, "%s already exists. Use --force to overwrite\n", app.arg_outputDatabase);
			exit(1);
		}
	}

	if (app.opt_load) {
		struct stat sbuf;

		if (stat(app.opt_load, &sbuf)) {
			fprintf(stderr, "%s does not exist\n", app.opt_load);
			exit(1);
		}
	}

	if (app.opt_text && isatty(1)) {
		fprintf(stderr, "stdout not redirected\n");
		exit(1);
	}

	// register timer handler
	if (ctx.opt_timer) {
		signal(SIGALRM, sigalrmHandler);
		::alarm(ctx.opt_timer);
	}

	/*
	 * Open input and create output database
	 */

	// Open input
	database_t db(ctx);

	// test readOnly mode
	app.readOnlyMode = (app.arg_outputDatabase == NULL && app.opt_text != app.OPTTEXT_BRIEF && app.opt_text != app.OPTTEXT_VERBOSE);

	db.open(app.arg_inputDatabase, !app.readOnlyMode);

	// display system flags when database was created
	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		char dbText[128], ctxText[128];

		ctx.flagsToText(db.creationFlags, dbText);
		ctx.flagsToText(ctx.flags, ctxText);

		if (db.creationFlags != ctx.flags)
			fprintf(stderr, "[%s] WARNING: Database/system flags differ: database=[%s] current=[%s]\n", ctx.timeAsString(), dbText, ctxText);
		else if (db.creationFlags && ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
			fprintf(stderr, "[%s] FLAGS [%s]\n", ctx.timeAsString(), dbText);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE)
		fprintf(stderr, "[%s] %s\n", ctx.timeAsString(), json_dumps(db.jsonInfo(NULL), JSON_PRESERVE_ORDER | JSON_COMPACT));

	/*
	 * @date 2020-04-21 00:16:34
	 *
	 * create output
	 *
	 * Transforms, signature, hint and imprint data never change and can be inherited
	 * Members can be inherited when nothing is added (missing output database)
	 *
	 * Sections can be inherited if their data or index settings remain unchanged
	 *
	 * NOTE: Signature data must be writable when `firstMember` changes (output database present)
	 */

	database_t store(ctx);

	// will be using `lookupSignature()` and `lookupImprintAssociative()`
	app.inheritSections &= ~(database_t::ALLOCMASK_SIGNATURE);
	// signature indices are used read-only, remove from inherit if sections are empty
	if (!db.signatureIndexSize)
		app.inheritSections &= ~database_t::ALLOCMASK_SIGNATUREINDEX;
	// will require local copy of signatures
	app.rebuildSections |= database_t::ALLOCMASK_SIGNATURE;

	// input database will always have a minimal node size of 4.
	unsigned minNodes = app.arg_numNodes > 4 ? app.arg_numNodes : 4;

	// inherit signature size (section is not going to grow)
	if (!app.readOnlyMode)
		app.opt_maxSignature = db.numSignature;

	// assign sizes to output sections
	app.sizeDatabaseSections(store, db, minNodes);

	/*
	 * Finalise allocations and create database
	 */

	// allocate evaluators
	app.pEvalFwd = (footprint_t *) ctx.myAlloc("genprimeContext_t::pEvalFwd", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalFwd));
	app.pEvalRev = (footprint_t *) ctx.myAlloc("genprimeContext_t::pEvalRev", tinyTree_t::TINYTREE_NEND * MAXTRANSFORM, sizeof(*app.pEvalRev));

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		// Assuming with database allocations included
		size_t allocated = ctx.totalAllocated + store.estimateMemoryUsage(app.inheritSections);

		struct sysinfo info;
		if (sysinfo(&info) == 0) {
			double percent = 100.0 * allocated / info.freeram;
			if (percent > 80)
				fprintf(stderr, "WARNING: using %.1f%% of free memory minus cache\n", percent);
		}
	}

	// actual create
	store.create(app.inheritSections);
	app.pStore = &store;

	if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS && (!(app.rebuildSections & ~app.inheritSections))) {
		struct sysinfo info;
		if (sysinfo(&info) != 0)
			info.freeram = 0;

		fprintf(stderr, "[%s] Allocated %.3fG memory. freeMemory=%.3fG.\n", ctx.timeAsString(), ctx.totalAllocated / 1e9, info.freeram / 1e9);
	}

#if 0
	tinyTree_t a(ctx);
	tinyTree_t b(ctx);
	a.decodeFast("ab^ac!");
	b.decodeFast("ab^bc!");
	uint64_t slotsL=0, slotsR =0;
	unsigned nextId = 0;
	bool similar = a.similar(a.root, b, b.root, slotsL, slotsR, nextId);
	if (!similar)
		similar = !similar;
#endif

	// initialize evaluator early using input database
	tinyTree_t tree(ctx);
	tree.initialiseVector(ctx, app.pEvalFwd, MAXTRANSFORM, db.fwdTransformData);
	tree.initialiseVector(ctx, app.pEvalRev, MAXTRANSFORM, db.revTransformData);

	/*
	 * Inherit/copy sections
	 */

	app.populateDatabaseSections(store, db);

	/*
	 * Rebuild sections
	 */

	// todo: move this to `populateDatabaseSections()`
	// data sections cannot be automatically rebuilt
	assert((app.rebuildSections & (database_t::ALLOCMASK_SWAP | database_t::ALLOCMASK_HINT | database_t::ALLOCMASK_MEMBER)) == 0);

	if (app.rebuildSections & database_t::ALLOCMASK_SIGNATURE) {
		store.numSignature = db.numSignature;
		::memcpy(store.signatures, db.signatures, store.numSignature * sizeof(*store.signatures));
	}
	if (app.rebuildSections)
		store.rebuildIndices(app.rebuildSections);

	/*
	 * count empty/unsafe
	 */

	app.numPrime = 0;
	for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
		if (store.signatures[iSid].prime[0])
			app.numPrime++;
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY)
		fprintf(stderr, "[%s] numImprint=%u(%.0f%%) numPrime=%u\n",
			ctx.timeAsString(),
			store.numImprint, store.numImprint * 100.0 / store.maxImprint,
			app.numPrime);

	/*
	 * Allocate storage for prime structures and their scoring
	 */
	app.pPrimeScores = (uint16_t *) ctx.myAlloc("genprimeContext_t::pPrimeScores", store.numSignature, sizeof(*app.pPrimeScores));
	app.pPrimeTrees = (tinyTree_t **) ctx.myAlloc("genprimeContext_t::pPrimeScores", store.numSignature, sizeof(*app.pPrimeTrees));

	for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
		app.pPrimeScores[iSid] = 0;
		app.pPrimeTrees[iSid] = new tinyTree_t(ctx);
	}

	/*
	 * Where to look for new candidates
	 */

	if (app.opt_load)
		app.primesFromFile();
	if (app.opt_generate) {
		if (app.arg_numNodes == 1) {
			// also include "0" and "a"
			app.arg_numNodes = 0;
			app.primesFromGenerator();
			app.arg_numNodes = 1;
		}

//		app.arg_numNodes = 3;
//		app.primesFromGenerator();
//		app.arg_numNodes = 4;
		app.primesFromGenerator();
	}

	/*
	 * re-order and re-index members
	 */

	if (!app.readOnlyMode) {
		if (app.opt_text == app.OPTTEXT_BRIEF) {
			/*
			 * Display members of complete dataset
			 *
			 * <memberName> <numPlaceholder>
			 */
			for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
				const signature_t *pSignature = store.signatures + iSid;

				if (pSignature->prime[0])
					printf("%s\n", pSignature->prime);
			}
		}

		if (app.opt_text == app.OPTTEXT_VERBOSE) {
			/*
			 * Display full members, grouped by signature
			 */
			for (unsigned iSid = 1; iSid < store.numSignature; iSid++) {
				const signature_t *pSignature = store.signatures + iSid;

				if (pSignature->prime[0])
					printf("%u\t%s\t%s\n", iSid, pSignature->name, pSignature->prime);
			}
		}
	}

	/*
	 * Save the database
	 */

	if (app.arg_outputDatabase) {
		if (!app.opt_saveIndex) {
			store.signatureIndexSize = 0;
			store.hintIndexSize = 0;
			store.imprintIndexSize = 0;
			store.numImprint = 0;
			store.interleave = 0;
			store.interleaveStep = 0;
		}

		// unexpected termination should unlink the outputs
		signal(SIGINT, sigintHandler);
		signal(SIGHUP, sigintHandler);

		store.save(app.arg_outputDatabase);
	}

	if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
		json_t *jResult = json_object();
		json_object_set_new_nocheck(jResult, "done", json_string_nocheck(argv[0]));
		if (app.opt_taskLast) {
			json_object_set_new_nocheck(jResult, "taskId", json_integer(app.opt_taskId));
			json_object_set_new_nocheck(jResult, "taskLast", json_integer(app.opt_taskLast));
		}
		if (app.opt_windowLo || app.opt_windowHi) {
			json_object_set_new_nocheck(jResult, "windowLo", json_integer(app.opt_windowLo));
			json_object_set_new_nocheck(jResult, "windowHi", json_integer(app.opt_windowHi));
		}
		if (app.arg_outputDatabase)
			json_object_set_new_nocheck(jResult, "filename", json_string_nocheck(app.arg_outputDatabase));
		store.jsonInfo(jResult);
		fprintf(stderr, "%s\n", json_dumps(jResult, JSON_PRESERVE_ORDER | JSON_COMPACT));
	}

	return 0;
}
