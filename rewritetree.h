#ifndef _REWRITETREE_H
#define _REWRITETREE_H

/*
 * @date 2021-10-07 23:38:53
 *
 * Structural rewrites based on pre-calculated structural analysis.
 * Requires the database for fixed lookups and rewrite templates.
 * The lookup pattern is the "abc!def!ghi!!" category. 
 * 
 * Placeholder skeleton with a virtual function declaration until all dependencies in place. 
 */

/*
 *	This file is part of Untangle, Information in fractal structures.
 *	Copyright (C) 2017-2020, xyzzy@rockingship.org
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

#include <stdint.h>
#include "database.h"
#include "basetree.h"

struct rewriteTree_t : baseTree_t {

	/// @var {database_t} database for signature/member lookups
	database_t &db;

	/**
	  * @date 2021-10-07 23:56:48
	  *
	  * Constructor
	  *
	  * @param {context_t} ctx - I/O context
	  * @param {database_t} db - Database for signature/member lookups
	  */
	rewriteTree_t(context_t &ctx, database_t &db) : baseTree_t(ctx), db(db) {
	}

	/**
	  * @date 2021-10-08 14:08:22
	  *
	  * Constructor
	  *
	  * @param {context_t} ctx - I/O context
	  * @param {database_t} db - Database for signature/member lookups
	  */
	rewriteTree_t(context_t &ctx, database_t &db, uint32_t kstart, uint32_t ostart, uint32_t estart, uint32_t nstart, uint32_t numRoots, uint32_t maxNodes, uint32_t flags) :
		baseTree_t(ctx, kstart, ostart, estart, nstart, numRoots, maxNodes, flags),
		db(db) {
	}


	/**
	 * @date 2021-10-08 00:00:19
	 *
	 * Apply structural rewriting.
	 * Lookup with index template "abc!def!ghi!!"
	 * Multiple results are all dry-run for their score.
	 * The winner is used as construction template.
	 * If a structural rewrite or collapse occurs,
	 *   the Q/T/F arguments hold the values for the final top-level node.
	 * Rewriting can create orphans and larger trees.
	 * 
	 * WARNING: A side-effect of the dry-run is that it will break the assertion `nodeId >= ncount`. 
	 * 
	 * return:
	 * Q/T/F are the `addBasicNode()` arguments.
	 * Collapse are returned as: `Q=T=F=id`. This can be tested with `if (T == F) {}`.
	 * returns true if Q/T/F changed.
	 * 
	 * WARNING: Always use the returned Q/T/F, even when function returns false.
	 * 
	 * @param {number*} Q - writable reference to `Q`
	 * @param {number*} T - writable reference to `T`
	 * @param {number*} F - writable reference to `F`
	 * @param {unsigned*} pFailCount - `non-null`: dry-run and increment on each missing node. `null`: apply changes, 
	 * @param {number} depth - Recursion depth
	 * @return {number} `true` if a rewrite was triggered
	 */
	virtual bool rewriteQTF(uint32_t *Q, uint32_t *T, uint32_t *F, unsigned *pFailCount, unsigned depth) {
		// save arguments to test return value
		uint32_t savQ = *Q, savT = *T, savF = *F;
		
		/*
		 * Level-3 normalisation: single node rewrites
		 * simulate what `genrewritedata()` does:
		 *   Populate slots, perform member lookup, if not found/depreciated perform signature lookup
		 */
		char     level3name[signature_t::SIGNATURENAMELENGTH + 1]; // name used to index `rewritedata[]`
		uint32_t level3mid = 0; // exact member match if non-zero
		uint32_t level3sid = 0; // signature match if non-zero (sid/mid are mutual exclusive)
		uint32_t sidSlots[tinyTree_t::TINYTREE_NEND];

		{
			static unsigned iVersion;
			static uint32_t buildVersion[100000000];
			static uint32_t buildSlot[100000000];

			unsigned nextSlotId   = tinyTree_t::TINYTREE_KSTART;

			tinyTree_t tree(ctx);
			unsigned   nextNodeId = tinyTree_t::TINYTREE_NSTART;

			++iVersion;
			assert(iVersion != 0);

			// setup zero
			buildVersion[0] = iVersion;
			buildSlot[0]    = 0;

			// raw slots as loaded to index `rewriteData[]`
			uint32_t rwSlots[tinyTree_t::TINYTREE_NEND]; // reverse endpoint index (effectively, KSTART-NSTART being `slots[]`)

			/*
			 * Construct Q component
			 */

			unsigned tlQ;

			if (*Q < this->nstart) {
				if (buildVersion[*Q] != iVersion) {
					buildVersion[*Q]       = iVersion;
					buildSlot[*Q]          = nextSlotId;
					rwSlots[nextSlotId++] = *Q;
				}
				tlQ = buildSlot[*Q];
			} else {
				rwSlots[nextNodeId] = *Q;
				tlQ = nextNodeId++;
				baseNode_t *pQ = this->N + *Q;

				if (buildVersion[pQ->Q] != iVersion) {
					buildVersion[pQ->Q]   = iVersion;
					buildSlot[pQ->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pQ->Q;
				}
				tree.N[tlQ].Q = buildSlot[pQ->Q];

				if (buildVersion[pQ->T & ~IBIT] != iVersion) {
					buildVersion[pQ->T & ~IBIT] = iVersion;
					buildSlot[pQ->T & ~IBIT]    = nextSlotId;
					rwSlots[nextSlotId++]       = pQ->T & ~IBIT;
				}
				tree.N[tlQ].T = buildSlot[pQ->T & ~IBIT] ^ (pQ->T & IBIT);

				if (buildVersion[pQ->F] != iVersion) {
					buildVersion[pQ->F]   = iVersion;
					buildSlot[pQ->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pQ->F;
				}
				tree.N[tlQ].F = buildSlot[pQ->F];

				// add node for back link
				buildVersion[*Q] = iVersion;
				buildSlot[*Q]    = tlQ;
			}

			/*
			 * Construct T component
			 */

			uint32_t Ti = *T & IBIT;
			uint32_t Tu = *T & ~IBIT;
			unsigned tlT;

			if (Tu < this->nstart) {
				if (buildVersion[Tu] != iVersion) {
					buildVersion[Tu]      = iVersion;
					buildSlot[Tu]         = nextSlotId;
					rwSlots[nextSlotId++] = Tu;
				}
				tlT = buildSlot[Tu];
			} else {
				/*
				 * @date 2021-07-06 00:38:52
				 * Tu is a reference (10)
				 * It sets stored in `slots[10]`
				 * so that later, when revering `testTree`, `N[10]` will refer to Tu
				 * making the return `T` correct.
				 */
				rwSlots[nextNodeId] = Tu;
				tlT = nextNodeId++;
				baseNode_t *pT = this->N + Tu;

				if (buildVersion[pT->Q] != iVersion) {
					buildVersion[pT->Q]   = iVersion;
					buildSlot[pT->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pT->Q;
				}
				tree.N[tlT].Q = buildSlot[pT->Q];

				if (buildVersion[pT->T & ~IBIT] != iVersion) {
					buildVersion[pT->T & ~IBIT] = iVersion;
					buildSlot[pT->T & ~IBIT]    = nextSlotId;
					rwSlots[nextSlotId++]       = pT->T & ~IBIT;
				}
				tree.N[tlT].T = buildSlot[pT->T & ~IBIT] ^ (pT->T & IBIT);

				if (buildVersion[pT->F] != iVersion) {
					buildVersion[pT->F]   = iVersion;
					buildSlot[pT->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pT->F;
				}
				tree.N[tlT].F = buildSlot[pT->F];

				// add node for back link
				buildVersion[Tu] = iVersion;
				buildSlot[Tu]    = tlT;
			}

			/*
			 * Construct F component
			 */

			unsigned tlF;

			if (*F < this->nstart) {
				if (buildVersion[*F] != iVersion) {
					buildVersion[*F]       = iVersion;
					buildSlot[*F]          = nextSlotId;
					rwSlots[nextSlotId++] = *F;
				}
				tlF = buildSlot[*F];
			} else {
				rwSlots[nextNodeId] = *F;
				tlF = nextNodeId++;
				baseNode_t *pF = this->N + *F;

				if (buildVersion[pF->Q] != iVersion) {
					buildVersion[pF->Q]   = iVersion;
					buildSlot[pF->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pF->Q;
				}
				tree.N[tlF].Q = buildSlot[pF->Q];

				if (buildVersion[pF->T & ~IBIT] != iVersion) {
					buildVersion[pF->T & ~IBIT] = iVersion;
					buildSlot[pF->T & ~IBIT]    = nextSlotId;
					rwSlots[nextSlotId++]       = pF->T & ~IBIT;
				}
				tree.N[tlF].T = buildSlot[pF->T & ~IBIT] ^ (pF->T & IBIT);

				if (buildVersion[pF->F] != iVersion) {
					buildVersion[pF->F]   = iVersion;
					buildSlot[pF->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pF->F & ~IBIT;
				}
				tree.N[tlF].F = buildSlot[pF->F];
			}

			/*
			 * Construct top-level
			 */
			tree.root           = nextNodeId;
			tree.count          = nextNodeId + 1;
			tree.N[tree.root].Q = tlQ;
			tree.N[tree.root].T = tlT ^ Ti;
			tree.N[tree.root].F = tlF;

			/*
			 * @date 2021-07-14 22:38:41
			 * Normalize to sanitize the name for lookups
			 */

			tree.saveString(tree.root, level3name, NULL);
			tree.loadStringSafe(level3name);

			/*
			 * The tree has a different endpoint allocation.
			 * The `rewriteData[]` index scans from left-to-right, otherwise it's (the default) depth-first
			 * Convert to depth-first, because that is how members are indexed,
			 * then apply the reverse transform of the skin to update the slots.
			 */

			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) {
				printf(",   \"level3\":{\"rwslots\"");
				for (unsigned i = tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++) {
					if (i == tinyTree_t::TINYTREE_KSTART)
						printf(":[%u", rwSlots[i]);
					else
						printf(",%u", rwSlots[i]);
				}
				printf("]");
			}

			/*
			 * Determine difference between left-to-right and depth-first
			 * and convert `rawSlots[]` to `slots[]` accordingly
			 */
			char skin[MAXSLOTS + 1];
			tree.saveString(tree.root, level3name, skin);

			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"name\":\"%s/%s\"", level3name, skin);


			/*
			 * Lookup signature
			 */
			uint32_t tid;

			// lookup the tree used by the detector
			db.lookupImprintAssociative(&tree, db.fwdEvaluator, db.revEvaluator, &level3sid, &tid);
			assert(level3sid);

			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"sid\":\"%u:%s\"", level3sid, db.signatures[level3sid].name);

			/*
			 * Lookup member
			 */

			uint32_t ix = db.lookupMember(level3name);
			level3mid = db.memberIndex[ix];
			member_t *pMember = db.members + level3mid;

			if (level3mid == 0 || (pMember->flags & member_t::MEMMASK_DEPR)) {
				level3mid = 0;
			} else {
				// use capitals to visually accentuate presence
				if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN)
					printf(",\"MID\":\"%u:%s/%u:%.*s\"",
					       level3mid, pMember->name,
					       pMember->tid, db.signatures[pMember->sid].numPlaceholder, db.revTransformNames[pMember->tid]);
			}

			/*
			 * Translate slots relative to `rwSlots[]`
			 */
			for (unsigned i = tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++)
				sidSlots[i] = rwSlots[i];
			for (unsigned i     = tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++)
				sidSlots[i] = rwSlots[tinyTree_t::TINYTREE_KSTART + db.fwdTransformNames[tid][i - tinyTree_t::TINYTREE_KSTART] - 'a'];

			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) {
				printf(",\"sidslots\"");
				for (unsigned i = tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++) {
					if (i == tinyTree_t::TINYTREE_KSTART)
						printf(":[%u", sidSlots[i]);
					else
						printf(",%u", sidSlots[i]);
				}
				printf("]");
				printf("}");
			}
		}

		/*
		 * Level-4 signature operand swapping
		 */
		{
			bool displayed = false;

			signature_t *pSignature = db.signatures + level3sid;
			if (pSignature->swapId) {
				swap_t *pSwap = db.swaps + pSignature->swapId;

				bool changed;
				do {
					changed = false;

					for (unsigned iSwap = 0; iSwap < swap_t::MAXENTRY && pSwap->tids[iSwap]; iSwap++) {
						unsigned tid = pSwap->tids[iSwap];

						// get the transform string
						const char *pTransformStr = db.fwdTransformNames[tid];

						// test if swap needed
						bool needSwap = false;

						for (unsigned i = 0; i < pSignature->numPlaceholder; i++) {
							if (sidSlots[tinyTree_t::TINYTREE_KSTART + i] > sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a']) {
								needSwap = true;
								break;
							}
							if (sidSlots[tinyTree_t::TINYTREE_KSTART + i] < sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a']) {
								needSwap = false;
								break;
							}
						}

						if (needSwap) {
							if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) {
								if (!displayed)
									printf(",   \"level4\":[");
								else
									printf(",");
								printf("%.*s", pSignature->numPlaceholder, db.fwdTransformNames[tid]);
							}
							displayed = true;

							uint32_t newSlots[MAXSLOTS];

							for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
								newSlots[i] = sidSlots[tinyTree_t::TINYTREE_KSTART + pTransformStr[i] - 'a'];

							for (unsigned i = 0; i < pSignature->numPlaceholder; i++)
								sidSlots[tinyTree_t::TINYTREE_KSTART + i] = newSlots[i];

							changed = true;
						}
					}
				} while (changed);
			}

			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) {
				if (displayed)
					printf("]");
			}
		}

		/*
		 * Level-5 normalisation: single node rewrites
		 */
		uint32_t level5mid = 0;
		{
			uint32_t bestCount = 0;

			if (level3mid != 0) {
				level5mid = level3mid;
			} else {
				/*
				 * @date 2021-07-12 23:15:58
				 * The best scoring members are the first on the list.
				 * Test how many nodes need to be created to store the runtime components
				 * This includes the top-level node that she current call is creating.
				 */
				if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",\"probe\":[");
				for (unsigned iMid = db.signatures[level3sid].firstMember; iMid; iMid = db.members[iMid].nextMember) {
					member_t *pMember = db.members + iMid;

					// depreciated are at the end of the list
					if (pMember->flags & member_t::MEMMASK_DEPR)
						break;

					uint32_t failCount = 0;
					this->rewriteString(pMember->name, db.revTransformNames[pMember->tid], sidSlots + tinyTree_t::TINYTREE_KSTART, Q, T, F, &failCount, depth + 1);
					if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) {
						if (level5mid != 0)
							printf(",");
						printf("{\"name\":\"%u:%s/%u:%.*s\",\"miss\":%u}", iMid, pMember->name,
						       pMember->tid, db.signatures[pMember->sid].numPlaceholder, db.revTransformNames[pMember->tid],
						       failCount);
					}

					if (level5mid == 0 || failCount < bestCount) {
						level5mid = iMid;
						bestCount = failCount;

						if (bestCount == 0) {
							break; // its already there
						} else if (bestCount == 1)
							break; // everything present except top-level
					}
				}
				if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf("]");
			}
			assert(level5mid);

			// apply best
			member_t *pMember = db.members + level5mid;

			if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"level5\":{\"member\":\"%u:%s/%u:%.*s\"}", level5mid, pMember->name, pMember->tid, db.signatures[pMember->sid].numPlaceholder, db.revTransformNames[pMember->tid]);
		}

		/*
		 * apply found member
		 */
		this->rewriteString(db.members[level5mid].name, db.revTransformNames[db.members[level5mid].tid], sidSlots + tinyTree_t::TINYTREE_KSTART, Q, T, F, NULL, 0);

		if (ctx.opt_debug & context_t::DEBUGMASK_EXPLAIN) printf(",   \"qtf\":[%u,%s%u,%u]}", *Q, (*T & IBIT) ? "~" : "", (*T & ~IBIT), *F);

		/*
		 * @date 2021-08-11 22:38:55
		 * Sometimes a rerun may result in a different tree.
		 * This is because normalisation adapts to what is already found in the tree.
		 *
		 * Some used samples
		 * ./bexplain 'cd^agd1!eh^!a2gdgcd!^c!!!' 'cd^agd1!eh^!a2gdgcd!^c!!!'
		 * ./bexplain 'ef^eg!gg2^^eg!ab^c1dacab!^!^^1aabccd^a7>!+2!^B2ac!ccdB3!^ac!>!^^2!C6C1B5^1g>C8!^1C5c>C6d1!^ggef+^eD5>!5caB1C6!C6!!^93^4gB0^^9B0!>!^^'
		 */

		return *Q != savQ || *T != savT || *F != savF;
	}

	/*
	 * @date 2021-07-08 04:49:14
	 *
	 * Expand and create a structure name with transform.
	 * Fast version specifically for `tinyTree_t` structures.
	 *
	 * @date 2021-08-14 12:08:06
	 * Need to call `explainNormaliseNode()` instead if `explainBasicNode()`/`explainOrderedNode()`.
	 * because this processes user input which is unnormalised.
	 * eg. "aab^cd^!" is passed and tested against member "abcd^!" which goes sour when `b` is "cd^".
	 * As a lightweight hack, expand `explainOrderedNode()` that it also soes some basic folding
	 *
	 * @date 2021-10-09 21:45:33
	 * Stricter/simpler version as names lack transform  
	 * 
	 * @param {string} name - structure name
	 * @param {string} skin - structure skin
	 * @param {number[]} slots - structure run-time slots
	 * @param {number*} Q - writable reference to top-level `Q`
	 * @param {number*} T - writable reference to top-level `T`
	 * @param {number*} F - writable reference to top-level `F`
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @param {number} depth - Recursion depth
	 */
	void rewriteString(const char name[], const char skin[], const uint32_t slot[], uint32_t *tlQ, uint32_t *tlT, uint32_t *tlF, unsigned *pFailCount, unsigned depth) {

		// state storage for postfix notation
		uint32_t stack[tinyTree_t::TINYTREE_MAXSTACK]; // there are 3 operands per per opcode
		int      stackPos = 0;
		uint32_t beenThere[tinyTree_t::TINYTREE_NEND]; // track id's of display operators.
		unsigned nextNode = tinyTree_t::TINYTREE_NSTART; // next visual node

		// walk through the notation until end or until placeholder/skin separator
		for (const char *pCh = name; *pCh; pCh++) {

			if (isalnum(*pCh) && stackPos >= tinyTree_t::TINYTREE_MAXSTACK)
				assert(!"DERR_OVERFLOW");
			if (islower(*pCh) && !islower(skin[*pCh - 'a']))
				assert(!"DERR_PLACEHOLDER");

			switch (*pCh) {
			case '0':
				stack[stackPos++] = 0;
				break;
			case 'a':
				stack[stackPos++] = slot[skin[0] - 'a'];
				break;
			case 'b':
				stack[stackPos++] = slot[skin[1] - 'a'];
				break;
			case 'c':
				stack[stackPos++] = slot[skin[2] - 'a'];
				break;
			case 'd':
				stack[stackPos++] = slot[skin[3] - 'a'];
				break;
			case 'e':
				stack[stackPos++] = slot[skin[4] - 'a'];
				break;
			case 'f':
				stack[stackPos++] = slot[skin[5] - 'a'];
				break;
			case 'g':
				stack[stackPos++] = slot[skin[6] - 'a'];
				break;
			case 'h':
				stack[stackPos++] = slot[skin[7] - 'a'];
				break;
			case 'i':
				stack[stackPos++] = slot[skin[8] - 'a'];
				break;
			case '1':
				stack[stackPos++] = beenThere[nextNode - ('1' - '0')];
				break;
			case '2':
				stack[stackPos++] = beenThere[nextNode - ('2' - '0')];
				break;
			case '3':
				stack[stackPos++] = beenThere[nextNode - ('3' - '0')];
				break;
			case '4':
				stack[stackPos++] = beenThere[nextNode - ('4' - '0')];
				break;
			case '5':
				stack[stackPos++] = beenThere[nextNode - ('5' - '0')];
				break;
			case '6':
				stack[stackPos++] = beenThere[nextNode - ('6' - '0')];
				break;
			case '7':
				stack[stackPos++] = beenThere[nextNode - ('7' - '0')];
				break;
			case '8':
				stack[stackPos++] = beenThere[nextNode - ('8' - '0')];
				break;
			case '9':
				stack[stackPos++] = beenThere[nextNode - ('9' - '0')];
				break;

			case '>': {
				// GT
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid;
				if (L >= this->ncount)
					nid = L; // propagate failed
				else if (R >= this->ncount)
					nid = R; // propagate failed
				else if (pCh[1] != 0)
					nid = this->addNormaliseNode(L, R ^ IBIT, 0, pFailCount, depth);
				else {
					*tlQ = L;
					*tlT = R ^ IBIT;
					*tlF = 0;
					return;
				}

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '+': {
				// OR
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid;
				if (L >= this->ncount)
					nid = L; // propagate failed
				else if (R >= this->ncount)
					nid = R; // propagate failed
				else if (pCh[1] != 0)
					nid = this->addNormaliseNode(L, IBIT, R, pFailCount, depth);
				else {
					*tlQ = L;
					*tlT = IBIT;
					*tlF = R;
					return;
				}

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '^': {
				// XOR/NE
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				//pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid;
				if (L >= this->ncount)
					nid = L; // propagate failed
				else if (R >= this->ncount)
					nid = R; // propagate failed
				else if (pCh[1] != 0)
					nid = this->addNormaliseNode(L, R ^ IBIT, R, pFailCount, depth);
				else {
					*tlQ = L;
					*tlT = R ^ IBIT;
					*tlF = R;
					return;
				}

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '!': {
				// QnTF
				if (stackPos < 3)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid;
				if (Q >= this->ncount)
					nid = Q; // propagate failed
				else if (T >= this->ncount)
					nid = T; // propagate failed
				else if (F >= this->ncount)
					nid = F; // propagate failed
				else if (pCh[1] != 0)
					nid = this->addNormaliseNode(Q, T ^ IBIT, F, pFailCount, depth);
				else {
					*tlQ = Q;
					*tlT = T ^ IBIT;
					*tlF = F;
					return;
				}

				// push
				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '&': {
				// AND
				if (stackPos < 2)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned R = stack[--stackPos]; // right hand side
				unsigned L = stack[--stackPos]; // left hand side

				// create operator
				unsigned nid;
				if (L >= this->ncount)
					nid = L; // propagate failed
				else if (R >= this->ncount)
					nid = R; // propagate failed
				else if (pCh[1] != 0)
					nid = this->addNormaliseNode(L, R, 0, pFailCount, depth);
				else {
					*tlQ = L;
					*tlT = R;
					*tlF = 0;
					return;
				}

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '?': {
				// QTF
				if (stackPos < 3)
					assert(!"DERR_UNDERFLOW");

				// pop operands
				unsigned F = stack[--stackPos];
				unsigned T = stack[--stackPos];
				unsigned Q = stack[--stackPos];

				// create operator
				unsigned nid;
				if (Q >= this->ncount)
					nid = Q; // propagate failed
				else if (T >= this->ncount)
					nid = T; // propagate failed
				else if (F >= this->ncount)
					nid = F; // propagate failed
				else if (pCh[1] != 0)
					nid = this->addNormaliseNode(Q, T, F, pFailCount, depth);
				else {
					*tlQ = Q;
					*tlT = T;
					*tlF = F;
					return;
				}

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			default:
				assert(!"DERR_UNDERFLOW");
			}
		}

		/*
		 * Name is an endpoint
		 */
		assert(stackPos == 1);
		assert(name[1] == 0);
		
		*tlQ = *tlT = *tlF = stack[--stackPos];
	}

};

#endif
