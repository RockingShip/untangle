/*
 * @date 2021-07-16 12:50:46
 *
 * `genport` is the shared logic behind `genexport`/`genimport`.
 * Converting the core data of the database to readable text and back.
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2021, xyzzy@rockingship.org
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
#include <unistd.h>

#include "database.h"
#include "dbtool.h"
#include "gentransform.h"
#include "genswap.h"
#include "gensignature.h"
#include "genmember.h"

/**
 * @date 2021-07-16 13:23:18
 *
 * Main program logic as application context
 * It is contained as an independent `struct` so it can be easily included into projects/code
 *
 * @typedef {object}
 */
struct genportContext_t : dbtool_t {

	/// @var {string} name of output database
	const char *arg_databaseName;

	/// @var {string} name of output database
	const char *arg_jsonName;

	/// @var {number} --depr, export depreciated members
	unsigned opt_depr;
	/// @var {number} --force, force overwriting of database if already exists
	unsigned opt_force;

	/// @var {database_t} - Database store to place results
	database_t  *pStore;

	genportContext_t(context_t &ctx) : dbtool_t(ctx) {
		arg_databaseName = NULL;
		arg_jsonName     = NULL;

		opt_depr         = 0;
		opt_force        = 0;

		pStore           = NULL;
	}

	uint32_t crc32Name(uint32_t crc32, const char *pName) {
		while (*pName) {
			__asm__ __volatile__ ("crc32b %1, %0" : "+r"(crc32) : "rm"(*pName));
			pName++;
		}
		return crc32;
	}

	/*
	 * @date 2021-07-17 20:51:23
	 *
	 * Calculate crc's
	 */
	uint32_t calcCRCsignatures(void) {
		/*
		 * Signatures
		 */

		uint32_t signatureCRC = 0;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			const signature_t *pSignature = pStore->signatures + iSid;

			signatureCRC = crc32Name(signatureCRC, pSignature->name);

			if (pSignature->flags & signature_t::SIGMASK_SAFE)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(signatureCRC) : "rm"('S'));
			if (pSignature->flags & signature_t::SIGMASK_PROVIDES)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(signatureCRC) : "rm"('P'));
			if (pSignature->flags & signature_t::SIGMASK_REQUIRED)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(signatureCRC) : "rm"('R'));
		}

		return signatureCRC;
	}

	/*
	 * @date 2021-07-17 23:09:29
	 *
	 * Calculate crc's
	 */
	uint32_t calcCRCswaps(void) {
		/*
		 * Swaps
 		 */

		uint32_t swapCRC = 0;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			const signature_t *pSignature = pStore->signatures + iSid;
			unsigned swapId = pSignature->swapId;

			if (swapId) {
				swapCRC = crc32Name(swapCRC, pSignature->name);

				const swap_t *pSwap = pStore->swaps + swapId;

				for (unsigned j = 0; j < pSwap->MAXENTRY; j++) {
					unsigned tid = pSwap->tids[j];
					if (tid) {
						assert(tid < pStore->numTransform);
						swapCRC = crc32Name(swapCRC, pStore->fwdTransformNames[tid]);
					}
				}
			}
		}

		return swapCRC;
	}

	uint32_t calcCRCmembers(void) {
		/*
		 * Members
		 */

		uint32_t memberCRC = 0;
		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			const member_t *pMember = pStore->members + iMid;

			if (pMember->flags & member_t::MEMMASK_DELETE)
				continue; // skip deleted
			if ((pMember->flags & member_t::MEMMASK_DEPR) && !opt_depr)
				continue; // skip depreciated

			memberCRC = crc32Name(memberCRC, pMember->name);

			assert(pMember->sid < pStore->numSignature);
			memberCRC = crc32Name(memberCRC, pStore->signatures[pMember->sid].name);
			assert(pMember->tid < pStore->numTransform);
			memberCRC = crc32Name(memberCRC, pStore->fwdTransformNames[pMember->tid]);

			assert(pMember->Qmt < pStore->numPair);
			pair_t *pPair = pStore->pairs + pMember->Qmt;
			assert(pPair->sidmid < pStore->numMember);
			memberCRC = crc32Name(memberCRC, pStore->members[pPair->sidmid].name);
			assert(pPair->tid < pStore->numTransform);
			memberCRC = crc32Name(memberCRC, pStore->fwdTransformNames[pPair->tid]);

			assert(pMember->Tmt < pStore->numPair);
			pPair = pStore->pairs + pMember->Tmt;
			assert(pPair->sidmid < pStore->numMember);
			memberCRC = crc32Name(memberCRC, pStore->members[pPair->sidmid].name);
			assert(pPair->tid < pStore->numTransform);
			memberCRC = crc32Name(memberCRC, pStore->fwdTransformNames[pPair->tid]);

			assert(pMember->Fmt < pStore->numPair);
			pPair = pStore->pairs + pMember->Fmt;
			assert(pPair->sidmid < pStore->numMember);
			memberCRC = crc32Name(memberCRC, pStore->members[pPair->sidmid].name);
			assert(pPair->tid < pStore->numTransform);
			memberCRC = crc32Name(memberCRC, pStore->fwdTransformNames[pPair->tid]);

			for (unsigned j = 0; j < pMember->MAXHEAD; j++) {
				unsigned mid = pMember->heads[j];
				if (mid) {
					assert(mid < pStore->numMember);
					memberCRC = crc32Name(memberCRC, pStore->members[mid].name);
				}
			}

			if (pMember->flags & member_t::MEMMASK_SAFE)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(memberCRC) : "rm"('S'));
#if 0
			/*
			 * @date 2021-07-18 14:33:44
			 * do not include component flag as it might change when removing depreciated from the collection
			 */
			if (pMember->flags & member_t::MEMMASK_COMP)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(memberCRC) : "rm"('C'));
#endif
			if (pMember->flags & member_t::MEMMASK_LOCKED)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(memberCRC) : "rm"('L'));
			if (pMember->flags & member_t::MEMMASK_DEPR)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(memberCRC) : "rm"('D'));
			if (pMember->flags & member_t::MEMMASK_DELETE)
				__asm__ __volatile__ ("crc32b %1, %0" : "+r"(memberCRC) : "rm"('X'));
		}

		return memberCRC;
	}

	/*
	 * @date 2021-07-18 12:06:46
	 *
	 * Write selection of header values
	 */
	void headersAsJson(FILE *f) {
		fprintf(f, "{\"%s\":%u\n", "maxSignature", pStore->numSignature);
		fprintf(f, ",\"%s\":%u\n", "signatureIndexSize", pStore->signatureIndexSize);
		fprintf(f, ",\"%s\":%u\n", "maxSwap", pStore->numSwap);
		fprintf(f, ",\"%s\":%u\n", "swapIndexSize", pStore->swapIndexSize);
		fprintf(f, ",\"%s\":%u\n", "interleave", pStore->interleave);
		fprintf(f, ",\"%s\":%u\n", "maxImprint", pStore->numImprint);
		fprintf(f, ",\"%s\":%u\n", "imprintIndexSize", pStore->imprintIndexSize);
		fprintf(f, ",\"%s\":%u\n", "maxPair", pStore->numPair);
		fprintf(f, ",\"%s\":%u\n", "pairIndexSize", pStore->pairIndexSize);
		fprintf(f, ",\"%s\":%u\n", "maxMember", pStore->numMember);
		fprintf(f, ",\"%s\":%u\n", "memberIndexSize", pStore->memberIndexSize);

	}

	/*
	 * @date 2021-07-18 12:07:46
	 * Write signatures
	 */
	void signaturesAsJson(FILE *f) {
		bool first = true;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			const signature_t *pSignature = pStore->signatures + iSid;

			if (first)
				fprintf(f, ",\"signatures\":[\n");
			else
				fprintf(f, ",");
			first = false;

			fprintf(f, "[\"%s\",\"", pSignature->name);

			if (pSignature->flags & signature_t::SIGMASK_SAFE)
				fprintf(f, "S");
			if (pSignature->flags & signature_t::SIGMASK_PROVIDES)
				fprintf(f, "P");
			if (pSignature->flags & signature_t::SIGMASK_REQUIRED)
				fprintf(f, "R");

			fprintf(f, "\"]\n");
		}

		fprintf(f, "]\n,\"%s\":%u\n", "signatureCRC", this->calcCRCsignatures());
	}

	/**
	 * @date 2020-04-21 18:56:28
	 *
	 * Read signatures from file
	 */
	void /*__attribute__((optimize("O0")))*/ signaturesFromJson(json_t *jInput, gensignatureContext_t &appSignature) {

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Loading signatures\n", ctx.timeAsString());

		json_t *jSignatures = json_object_get(jInput, "signatures");
		if (jSignatures == NULL)
			ctx.fatal("\n{\"error\":\"signature section not found\",\"where\":\"%s:%s:%d\"}\n",
				  __FUNCTION__, __FILE__, __LINE__);

		tinyTree_t tree(ctx);

		unsigned numSignatures = json_array_size(jSignatures);
		for (unsigned iSig = 0; iSig < numSignatures; iSig++) {
			json_t *jLine = json_array_get(jSignatures, iSig);
			const char *pName  = json_string_value(json_array_get(jLine, 0));
			const char *pFlags = json_string_value(json_array_get(jLine, 1));

			/*
			 * Analyse name
			 */

			unsigned numPlaceholder = 0, numEndpoint = 0, numBackRef = 0;
			unsigned beenThere      = 0;

			for (const char *p = pName; *p; p++) {
				if (::islower(*p)) {
					if (!(beenThere & (1 << (*p - 'a')))) {
						numPlaceholder++;
						beenThere |= 1 << (*p - 'a');
					}
					numEndpoint++;
				} else if (::isdigit(*p) && *p != '0') {
					numBackRef++;
				}
			}

			/*
			 * construct tree
			 */

			tree.loadStringFast(pName);

			/*
			 * construct signature
			 */

			uint32_t sid = pStore->addSignature(pName);
			signature_t *pSignature = pStore->signatures + sid;

			pSignature->size           = tree.count - tinyTree_t::TINYTREE_NSTART;
			pSignature->numPlaceholder = numPlaceholder;
			pSignature->numEndpoint    = numEndpoint;
			pSignature->numBackRef     = numBackRef;

			/*
			 * Examine flags
			 */

			while (*pFlags) {
				if (*pFlags == 'S')
					pSignature->flags |= signature_t::SIGMASK_SAFE;
				else if (*pFlags == 'P')
					pSignature->flags |= signature_t::SIGMASK_PROVIDES;
				else if (*pFlags == 'R')
					pSignature->flags |= signature_t::SIGMASK_REQUIRED;
				else
					ctx.fatal("\n{\"error\":\"unknown flag\",\"where\":\"%s:%s:%d\",\"name\":\"%s\"}\n", __FUNCTION__, __FILE__, __LINE__, pName);

				pFlags++;
			}

			/*
			 * add to index
			 */

			unsigned ix = pStore->lookupSignature(pName);
			assert(pStore->signatureIndex[ix] == 0);
			pStore->signatureIndex[ix] = sid;
		}

		/*
		 * Release storage
		 */
		json_array_clear(jSignatures);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			fprintf(stderr, "[%s] Read %u lines. numSignature=%u(%.0f%%) | hash=%.3f\n",
				ctx.timeAsString(),
				numSignatures,
				pStore->numSignature, pStore->numSignature * 100.0 / pStore->maxSignature,
				(double) ctx.cntCompare / ctx.cntHash);
		}

		/*
		 * Verify CRC
		 */
		uint32_t expectedCRC = json_integer_value(json_object_get(jInput, "signatureCRC"));
		uint32_t encounteredCRC = calcCRCsignatures();

		if (expectedCRC != encounteredCRC) {
			ctx.fatal("\n{\"error\":\"signature CRC failed\",\"where\":\"%s:%s:%d\",\"expected\":%u,\"encountered\":%u}\n", __FUNCTION__, __FILE__, __LINE__, expectedCRC, encounteredCRC);
		}
	}

	/*
	 * @date 2021-07-18 12:08:46
	 * Write swaps
	 */
	void swapsAsJson(FILE *f) {
		bool first = true;
		for (unsigned iSid = 1; iSid < pStore->numSignature; iSid++) {
			const signature_t *pSignature = pStore->signatures + iSid;
			unsigned swapId = pSignature->swapId;

			if (swapId) {
				if (first)
					fprintf(f, ",\"swaps\":[\n");
				else
					fprintf(f, ",");
				first = false;

				fprintf(f, "[\"%s\"", pSignature->name);

				const swap_t *pSwap = pStore->swaps + swapId;

				for (unsigned j = 0; j < pSwap->MAXENTRY; j++) {
					unsigned tid = pSwap->tids[j];
					if (tid)
						fprintf(f, ",\"%.*s\"", pSignature->numPlaceholder, pStore->fwdTransformNames[tid]);
				}

				fprintf(f, "]\n");
			}
		}

		fprintf(f, "]\n,\"%s\":%u\n", "swapCRC", this->calcCRCswaps());
	}

	/**
	 * @date 2021-07-16 22:30:14
	 *
	 * Read swaps from file
	 */
	void /*__attribute__((optimize("O0")))*/ swapsFromJson(json_t *jInput, genswapContext_t &appSwap) {

		/*
		 * Load candidates from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Loading swaps\n", ctx.timeAsString());

		json_t *jSwaps = json_object_get(jInput, "swaps");
		if (jSwaps == NULL)
			ctx.fatal("\n{\"error\":\"swap section not found\",\"where\":\"%s:%s:%d\"}\n",
				  __FUNCTION__, __FILE__, __LINE__);

		tinyTree_t tree(ctx);

		unsigned numSwaps = json_array_size(jSwaps);
		for (unsigned iSwap = 0; iSwap < numSwaps; iSwap++) {
			json_t *jLine = json_array_get(jSwaps, iSwap);
			const char *pName  = json_string_value(json_array_get(jLine, 0));

			/*
			 * lookup signature
			 */

			uint32_t ix = pStore->lookupSignature(pName);
			uint32_t sid = pStore->signatureIndex[ix];

			if (sid == 0)
				ctx.fatal("\n{\"error\":\"swap signature not found\",\"where\":\"%s:%s:%d\",\"signature\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			signature_t *pSignature = pStore->signatures + sid;

			/*
			 * Collect tids
			 */

			swap_t swap;
			unsigned numTid = 0;

			memset(&swap, 0, sizeof swap);

			for (unsigned iPos = 1; iPos < json_array_size(jLine); iPos++) {
				const char *pSkin = json_string_value(json_array_get(jLine, iPos));

				// lookup skin
				uint32_t tid = pStore->lookupFwdTransform(pSkin);
				if (tid == IBIT)
					ctx.fatal("\n{\"error\":\"swap tid not found\",\"where\":\"%s:%s:%d\",\"tid\":\"%s\"}\n",
						  __FUNCTION__, __FILE__, __LINE__, pSkin);

				assert(numTid < swap_t::MAXENTRY);
				swap.tids[numTid++] = tid;
			}

			if (numTid == 0)
				ctx.fatal("\n{\"error\":\"missing tids for swap\",\"where\":\"%s:%s:%d\",\"signature\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			// lookup/add swapId
			ix     = pStore->lookupSwap(&swap);
			unsigned swapId = pStore->swapIndex[ix];
			if (swapId == 0)
				pStore->swapIndex[ix] = swapId = pStore->addSwap(&swap);

			// add swapId to signature
			pSignature->swapId = swapId;
		}

		/*
		 * Release storage
		 */
		json_array_clear(jSwaps);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			fprintf(stderr, "[%s] Read %u lines. numSwaps=%u(%.0f%%) | hash=%.3f\n",
				ctx.timeAsString(),
				numSwaps,
				pStore->numSwap, pStore->numSwap * 100.0 / pStore->maxSwap,
				(double) ctx.cntCompare / ctx.cntHash);
		}

		/*
		 * Verify CRC
		 */
		uint32_t expectedCRC = json_integer_value(json_object_get(jInput, "swapCRC"));
		uint32_t encounteredCRC = calcCRCswaps();

		if (expectedCRC != encounteredCRC) {
			ctx.fatal("\n{\"error\":\"swap CRC failed\",\"where\":\"%s:%s:%d\",\"expected\":%u,\"encountered\":%u}\n", __FUNCTION__, __FILE__, __LINE__, expectedCRC, encounteredCRC);
		}
	}

	/*
	 * @date 2021-07-18 12:10:00
	 * Write members
	 */
	void membersAsJson(FILE *f) {
		bool first = true;
		for (unsigned iMid = 1; iMid < pStore->numMember; iMid++) {
			const member_t *pMember = pStore->members + iMid;

			if (pMember->flags &  member_t::MEMMASK_DELETE)
				continue; // skip deleted
			if ((pMember->flags & member_t::MEMMASK_DEPR) && !opt_depr)
				continue; // skip depreciated

			if (first)
				fprintf(f, ",\"members\":[\n");
			else
				fprintf(f, ",");
			first = false;

			fprintf(f, "[\"%s\",\"", pMember->name);

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

			fprintf(f, "\"]\n");
		}

		fprintf(f, "]\n,\"%s\":%u\n", "memberCRC", this->calcCRCmembers());
	}

	/**
	 * @date 2021-07-16 23:14:13
	 *
	 * Read members from file
	 */
	void /*__attribute__((optimize("O0")))*/ membersFromJson(json_t *jInput, genmemberContext_t &appMember) {

		/*
		 * Load members from file.
		 */

		if (ctx.opt_verbose >= ctx.VERBOSE_ACTIONS)
			fprintf(stderr, "[%s] Loading members\n", ctx.timeAsString());

		json_t *jMembers = json_object_get(jInput, "members");
		if (jMembers == NULL)
			ctx.fatal("\n{\"error\":\"members section not found\",\"where\":\"%s:%s:%d\"}\n",
				  __FUNCTION__, __FILE__, __LINE__);

		tinyTree_t tree(ctx);

		ctx.setupSpeed(pStore->maxMember);

		unsigned numMembers = json_array_size(jMembers);
		for (unsigned iMem = 0; iMem < numMembers; iMem++) {
			json_t *jLine = json_array_get(jMembers, iMem);
			const char *pName  = json_string_value(json_array_get(jLine, 0));
			const char *pFlags = json_string_value(json_array_get(jLine, 1));

			if (ctx.opt_verbose >= ctx.VERBOSE_TICK && ctx.tick) {
				int perSecond = ctx.updateSpeed();

				int eta = (int) ((ctx.progressHi - ctx.progress) / perSecond);

				int etaH = eta / 3600;
				eta %= 3600;
				int etaM = eta / 60;
				eta %= 60;
				int etaS = eta;

				fprintf(stderr, "\r\e[K[%s] %lu(%7d/s) %.5f%% eta=%d:%02d:%02d | numMember=%u(%.0f%%) | hash=%.3f %s",
					ctx.timeAsString(), ctx.progress, perSecond, ctx.progress * 100.0 / ctx.progressHi, etaH, etaM, etaS,
					pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
					(double) ctx.cntCompare / ctx.cntHash, pName);

				ctx.tick = 0;
			}

			/*
			 * construct member
			 */

			uint32_t mid = pStore->addMember(pName);
			member_t *pMember = pStore->members + mid;

			tree.loadStringFast(pName);
			pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &pMember->sid, &pMember->tid);

			if (pMember->sid == 0)
				ctx.fatal("\n{\"error\":\"member not matched\",\"where\":\"%s:%s:%d\",\"member\":\"%s\"}\n",
					  __FUNCTION__, __FILE__, __LINE__, pName);

			/*
			 * Examine flags
			 */

			while (*pFlags) {
				if (*pFlags == 'S')
					pMember->flags |= member_t::MEMMASK_SAFE;
				else if (*pFlags == 'C')
					pMember->flags |= member_t::MEMMASK_COMP;
				else if (*pFlags == 'L')
					pMember->flags |= member_t::MEMMASK_LOCKED;
				else if (*pFlags == 'D')
					pMember->flags |= member_t::MEMMASK_DEPR;
				else if (*pFlags == 'X')
					pMember->flags |= member_t::MEMMASK_DELETE;
				else
					ctx.fatal("\n{\"error\":\"unknown flag\",\"where\":\"%s:%s:%d\",\"name\":\"%s\"}\n", __FUNCTION__, __FILE__, __LINE__, pName);

				pFlags++;
			}

			/*
			 * add to index
			 */

			unsigned ix = pStore->lookupMember(pName);
			assert(pStore->memberIndex[ix] == 0);
			pStore->memberIndex[ix] = mid;

			/*
			 * Construct member
			 */
			unsigned savFlags = pMember->flags;
			bool ok = appMember.findHeadTail(pMember, tree);
			if (!ok)
				ctx.fatal("\n{\"error\":\"failed to reconstruct member\",\"where\":\"%s:%s:%d\",\"name\":\"%s\"}\n", __FUNCTION__, __FILE__, __LINE__, pName);
			if (pMember->flags != savFlags)
				ctx.fatal("\n{\"error\":\"flags changed after member reconstruction\",\"where\":\"%s:%s:%d\",\"name\":\"%s\",\"encountered\":%u,\"expected\":%u}\n", __FUNCTION__, __FILE__, __LINE__, pName, pMember->flags, savFlags);

			ctx.progress++;
		}
		if (ctx.opt_verbose >= ctx.VERBOSE_TICK)
			fprintf(stderr, "\r\e[K");

		/*
		 * Release storage
		 */
		json_array_clear(jMembers);

		if (ctx.opt_verbose >= ctx.VERBOSE_SUMMARY) {
			fprintf(stderr, "[%s] Read %u lines. numMembers=%u(%.0f%%) | hash=%.3f\n",
				ctx.timeAsString(),
				numMembers,
				pStore->numMember, pStore->numMember * 100.0 / pStore->maxMember,
				(double) ctx.cntCompare / ctx.cntHash);
		}

		/*
		 * compact, sort and reindex members
		 */

		appMember.finaliseMembers();

		/*
		 * Verify CRC
		 */
		uint32_t expectedCRC = json_integer_value(json_object_get(jInput, "memberCRC"));
		uint32_t encounteredCRC = calcCRCmembers();

		if (expectedCRC != encounteredCRC) {
			ctx.fatal("\n{\"error\":\"member CRC failed\",\"where\":\"%s:%s:%d\",\"expected\":%u,\"encountered\":%u}\n", __FUNCTION__, __FILE__, __LINE__, expectedCRC, encounteredCRC);
		}
	}

};
