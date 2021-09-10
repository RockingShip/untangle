#ifndef _BASEEXPLAIN_H
#define _BASEEXPLAIN_H

#include <stdint.h>
#include "database.h"
#include "basetree.h"

struct baseExplain_t {

	/// @var {context_t} I/O context
	context_t &ctx;

	bool track;

	/// @var {database_t} database for signature/member lookups
	database_t *pStore;

	/**
	  * @date 2021-08-18 21:53:34
	  *
	  * Constructor
	  *
	  * @param {context_t} ctx - I/O context
	  */
	baseExplain_t(context_t &ctx) : ctx(ctx) {
		track = true;
		pStore = NULL;
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
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {string} name - structure name
	 * @param {string} skin - structure skin
	 * @param {number[]} slots - structure run-time slots
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainStringFast(unsigned depth, uint32_t expectId, baseTree_t *pTree, const char name[], const char skin[], const uint32_t slot[], unsigned *pFailCount) {

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
				if (L >= pTree->ncount)
					nid = L; // propagate failed
				else if (R >= pTree->ncount)
					nid = R; // propagate failed
				else
					nid = pTree->addOrderNode(L, R ^ IBIT, 0, expectId, pFailCount, depth);

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
				if (L >= pTree->ncount)
					nid = L; // propagate failed
				else if (R >= pTree->ncount)
					nid = R; // propagate failed
				else
					nid = pTree->addOrderNode(L, 0 ^ IBIT, R, expectId, pFailCount, depth);

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
				if (L >= pTree->ncount)
					nid = L; // propagate failed
				else if (R >= pTree->ncount)
					nid = R; // propagate failed
				else
					nid = pTree->addOrderNode(L, R ^ IBIT, R, expectId, pFailCount, depth);

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
				if (Q >= pTree->ncount)
					nid = Q; // propagate failed
				else if (T >= pTree->ncount)
					nid = T; // propagate failed
				else if (F >= pTree->ncount)
					nid = F; // propagate failed
				else
					nid = pTree->addOrderNode(Q, T ^ IBIT, F, expectId, pFailCount, depth);

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
				if (L >= pTree->ncount)
					nid = L; // propagate failed
				else if (R >= pTree->ncount)
					nid = R; // propagate failed
				else
					nid = pTree->addOrderNode(L, R, 0, expectId, pFailCount, depth);

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
				if (Q >= pTree->ncount)
					nid = Q; // propagate failed
				else if (T >= pTree->ncount)
					nid = T; // propagate failed
				else if (F >= pTree->ncount)
					nid = F; // propagate failed
				else
					nid = pTree->addOrderNode(Q, T, F, expectId, pFailCount, depth);

				stack[stackPos++]     = nid; // push
				beenThere[nextNode++] = nid; // save actual index for back references
				break;
			}
			case '~': {
				// NOT
				if (stackPos < 1)
					assert(!"DERR_UNDERFLOW");

				// invert top-of-stack
				stack[stackPos - 1] ^= IBIT;
				break;
			}

			case '/':
				// separator between placeholder/skin
				while (pCh[1])
					pCh++;
				break;
			case ' ':
				// skip spaces
				break;
			default:
				assert(!"DERR_UNDERFLOW");
			}
		}

		if (stackPos != 1)
			assert(!"DERR_UNDERFLOW");

		// store result into root
		return stack[stackPos - 1];
	}

	/*
	 * @date 2021-07-05 19:23:46
	 *
	 * Normalise Q/T/F and add to tree
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {number} Q - component
	 * @param {number} T - component
	 * @param {number} F - component
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainNormaliseNode(unsigned depth, uint32_t expectId, baseTree_t *pTree, uint32_t Q, uint32_t T, uint32_t F, unsigned *pFailCount) {

		if (track) printf("%*s{\"Q\":%s%u,\"T\":%s%u,\"F\":%s%u",
				   depth, "",
				   (Q & IBIT) ? "~" : "", (Q & ~IBIT),
				   (T & IBIT) ? "~" : "", (T & ~IBIT),
				   (F & IBIT) ? "~" : "", (F & ~IBIT));

		assert(++depth < 80);

		assert ((Q & ~IBIT) < pTree->ncount);
		assert ((T & ~IBIT) < pTree->ncount);
		assert ((F & ~IBIT) < pTree->ncount);

		/*
		 * Level-1 normalisation: invert propagation
		 *
		 * !a ?  b :  c  ->  a ? c : b
		 *  0 ?  b :  c  ->  c
		 *  a ?  b : !c  ->  !(a ? !b : c)
		 */
		uint32_t ibit = 0; // needed for return values
		{
			bool changed = false;

			if (Q & IBIT) {
				// "!Q?T:F" -> "Q?F:T"
				uint32_t savT = T;
				T = F;
				F = savT;
				Q ^= IBIT;
				changed = true;
			}
			if (Q == 0) {
				// "0?T:F" -> "F"
				if (track) printf(",   \"level1\":\"F\",\"N\":%s%u}", (F & IBIT) ? "~" : "", (F & ~IBIT));
				return F;
			}

			if (F & IBIT) {
				// "Q?T:!F" -> "!(Q?!T:F)"
				F ^= IBIT;
				T ^= IBIT;
				ibit ^= IBIT;
				changed = true;
			}

			if (changed) {
				if (track) printf(",   \"level1\":{\"Q\":%u,\"T\":%s%u,\"F\":%u}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			}
		}

		/*
		 * Level-2 normalisation: function grouping
		 * NOTE: this is also embedded in level-3, it is included for visual completeness
		 */
		{
			bool changed = false;

			if (T & IBIT) {

				if (T == IBIT) {
					if (F == Q || F == 0) {
						// SELF
						// "Q?!0:Q" [1] -> "Q?!0:0" [0] -> Q
						if (track) printf(",   \"level2\":\"Q\",\"N\":%s%u}", ibit ? "~" : "", Q);
						return Q ^ ibit;
					} else {
						// OR
						// "Q?!0:F" [2]
					}
				} else if ((T & ~IBIT) == Q) {
					if (F == Q || F == 0) {
						// ZERO
						// "Q?!Q:Q" [4] -> "Q?!Q:0" [3] -> "0"
						if (track) printf(",   \"level2\":\"0\",\"N\":%s%u}", ibit ? "~" : "", 0);
						return 0 ^ ibit;
					} else {
						// LESS-THAN
						// "Q?!Q:F" [5] -> "F?!Q:F" -> "F?!Q:0"
						Q = F;
						F = 0;
						changed = true;
					}
				} else {
					if (F == 0) {
						// GREATER-THAN
						// "Q?!T:Q" [7] -> "Q?!T:0" [6]
					} else if (F == Q) {
						// GREATER-THAN
						// "Q?!T:Q" [7] -> "Q?!T:0" [6]
						F = 0;
						changed = true;
					} else if ((T & ~IBIT) == F) {
						// NOT-EQUAL
						// "Q?!F:F" [8]
					} else {
						// QnTF (new unified operator)
						// "Q?!T:F" [9]
					}
				}

			} else {

				if (T == 0) {
					if (F == Q || F == 0) {
						// ZERO
						// "Q?0:Q" [11] -> "Q?0:0" [10] -> "0"
						if (track) printf(",   \"level2\":\"0\",\"N\":%s%u}", ibit ? "~" : "", 0);
						return 0 ^ ibit;
					} else {
						// LESS-THAN
						// "Q?0:F" [12] -> "F?!Q:0" [6]
						T = Q ^ IBIT;
						Q = F;
						F = 0;
						changed = true;
					}

				} else if (T == Q) {
					if (F == Q || F == 0) {
						// SELF
						// "Q?Q:Q" [14] -> Q?Q:0" [13] -> "Q"
						if (track) printf(",   \"level2\":\"Q\",\"N\":%s%u}", ibit ? "~" : "", Q);
						return Q ^ ibit;
					} else {
						// OR
						// "Q?Q:F" [15] -> "Q?!0:F" [2]
						T = 0 ^ IBIT;
						changed = true;
					}
				} else {
					if (F == 0) {
						// AND
						// "Q?T:Q" [17] -> "Q?T:0" [16]
					} else if (F == Q) {
						// AND
						// "Q?T:Q" [17] -> "Q?T:0" [16]
						F = 0;
						changed = true;
					} else if (T == F) {
						// SELF
						// "Q?F:F" [18] -> "F"
						if (track) printf(",   \"level2\":\"F\",\"N\":%s%u}", ibit ? "~" : "", F);
						return F ^ ibit;
					} else {
						// QTF (old unified operator)
						// "Q?T:F" [19]
					}
				}
			}

			if (changed) {
				if (track) printf(",   \"level2\":{\"Q\":%u,\"T\":%s%u,\"F\":%u}", Q, (T & IBIT) ? "~" : "", (T & ~IBIT), F);
			}
		}

		/*
		 * test if node is available
		 *
		 * @date 2021-08-11 22:49:54
		 * This is intended as a fast-path, only it might be that the found node has been depreciated from a different context
		 * `testBasicNode()` now handles the end-condition
		 *
		 * @date 2021-08-13 00:22:55
		 * Now with ordered nodes, the written database should be fully ordered, making this test valid again
		 */
		{
			uint32_t ix = pTree->lookupNode(Q, T, F);
			if (pTree->nodeIndex[ix] != 0) {
				if (track) printf(",   \"lookup\":%s%u}}", ibit ? "~" : "", pTree->nodeIndex[ix]);
				return pTree->nodeIndex[ix] ^ ibit;
			}
		}

		/*
		 * Level-3 normalisation: single node rewrites
		 * simulate what `genrewritedata()` does:
		 *   Populate slots, perform member lookup, if not found/depreciated perform signature lookup
		 */
		char level3name[signature_t::SIGNATURENAMELENGTH + 1]; // name used to index `rewritedata[]`
		uint32_t level3mid = 0; // exact member match if non-zero
		uint32_t level3sid = 0; // signature match if non-zero (sid/mid are mutual exclusive)
		uint32_t sidSlots[tinyTree_t::TINYTREE_NEND];

		{
			static unsigned iVersion;
			static uint32_t buildVersion[100000000];
			static uint32_t buildSlot[100000000];

			unsigned nextSlotId = tinyTree_t::TINYTREE_KSTART;

			tinyTree_t tree(ctx);
			unsigned nextNodeId = tinyTree_t::TINYTREE_NSTART;

			++iVersion;
			assert(iVersion != 0);

			// setup zero
			buildVersion[0] = iVersion;
			buildSlot[0] = 0;

			// raw slots as loaded to index `rewriteData[]`
			uint32_t rwSlots[tinyTree_t::TINYTREE_NEND]; // reverse endpoint index (effectively, KSTART-NSTART being `slots[]`)

			/*
			 * Construct Q component
			 */

			unsigned tlQ;

			if (Q < pTree->nstart) {
				if (buildVersion[Q] != iVersion) {
					buildVersion[Q]       = iVersion;
					buildSlot[Q]          = nextSlotId;
					rwSlots[nextSlotId++] = Q;
				}
				tlQ = buildSlot[Q];
			} else {
				rwSlots[nextNodeId] = Q;
				tlQ = nextNodeId++;
				baseNode_t *pQ = pTree->N + Q;

				if (buildVersion[pQ->Q] != iVersion) {
					buildVersion[pQ->Q] = iVersion;
					buildSlot[pQ->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pQ->Q;
				}
				tree.N[tlQ].Q = buildSlot[pQ->Q];

				if (buildVersion[pQ->T & ~IBIT] != iVersion) {
					buildVersion[pQ->T & ~IBIT] = iVersion;
					buildSlot[pQ->T & ~IBIT] = nextSlotId;
					rwSlots[nextSlotId++]    = pQ->T & ~IBIT;
				}
				tree.N[tlQ].T = buildSlot[pQ->T & ~IBIT] ^ (pQ->T & IBIT);

				if (buildVersion[pQ->F] != iVersion) {
					buildVersion[pQ->F] = iVersion;
					buildSlot[pQ->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pQ->F;
				}
				tree.N[tlQ].F = buildSlot[pQ->F];

				// add node for back link
				buildVersion[Q] = iVersion;
				buildSlot[Q] = tlQ;
			}

			/*
			 * Construct T component
			 */

			uint32_t Ti = T & IBIT;
			uint32_t Tu = T & ~IBIT;
			unsigned tlT;

			if (Tu < pTree->nstart) {
				if (buildVersion[Tu] != iVersion) {
					buildVersion[Tu]  = iVersion;
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
				baseNode_t *pT = pTree->N + Tu;

				if (buildVersion[pT->Q] != iVersion) {
					buildVersion[pT->Q] = iVersion;
					buildSlot[pT->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pT->Q;
				}
				tree.N[tlT].Q = buildSlot[pT->Q];

				if (buildVersion[pT->T & ~IBIT] != iVersion) {
					buildVersion[pT->T & ~IBIT] = iVersion;
					buildSlot[pT->T & ~IBIT] = nextSlotId;
					rwSlots[nextSlotId++]    = pT->T & ~IBIT;
				}
				tree.N[tlT].T = buildSlot[pT->T & ~IBIT] ^ (pT->T & IBIT);

				if (buildVersion[pT->F] != iVersion) {
					buildVersion[pT->F] = iVersion;
					buildSlot[pT->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pT->F;
				}
				tree.N[tlT].F = buildSlot[pT->F];

				// add node for back link
				buildVersion[Tu] = iVersion;
				buildSlot[Tu] = tlT;
			}

			/*
			 * Construct F component
			 */

			unsigned tlF;

			if (F < pTree->nstart) {
				if (buildVersion[F] != iVersion) {
					buildVersion[F] = iVersion;
					buildSlot[F]          = nextSlotId;
					rwSlots[nextSlotId++] = F;
				}
				tlF = buildSlot[F];
			} else {
				rwSlots[nextNodeId] = F;
				tlF = nextNodeId++;
				baseNode_t *pF = pTree->N + F;

				if (buildVersion[pF->Q] != iVersion) {
					buildVersion[pF->Q] = iVersion;
					buildSlot[pF->Q]      = nextSlotId;
					rwSlots[nextSlotId++] = pF->Q;
				}
				tree.N[tlF].Q = buildSlot[pF->Q];

				if (buildVersion[pF->T & ~IBIT] != iVersion) {
					buildVersion[pF->T & ~IBIT] = iVersion;
					buildSlot[pF->T & ~IBIT] = nextSlotId;
					rwSlots[nextSlotId++]    = pF->T & ~IBIT;
				}
				tree.N[tlF].T = buildSlot[pF->T & ~IBIT] ^ (pF->T & IBIT);

				if (buildVersion[pF->F] != iVersion) {
					buildVersion[pF->F] = iVersion;
					buildSlot[pF->F]      = nextSlotId;
					rwSlots[nextSlotId++] = pF->F & ~IBIT;
				}
				tree.N[tlF].F = buildSlot[pF->F];
			}

			/*
			 * Construct top-level
			 */
			tree.root = nextNodeId;
			tree.count = nextNodeId+1;
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

			if (track) {
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

			if (track) printf(",\"name\":\"%s/%s\"", level3name, skin);


			/*
			 * Lookup signature
			 */
			uint32_t tid;

			// lookup the tree used by the detector
			pStore->lookupImprintAssociative(&tree, pStore->fwdEvaluator, pStore->revEvaluator, &level3sid, &tid);
			assert(level3sid);

			if (track) printf(",\"sid\":\"%u:%s\"",
					   level3sid, pStore->signatures[level3sid].name);

			/*
			 * Lookup member
			 */

			uint32_t ix = pStore->lookupMember(level3name);
			level3mid = pStore->memberIndex[ix];
			member_t *pMember = pStore->members + level3mid;

			if (level3mid == 0 || (pMember->flags & member_t::MEMMASK_DEPR)) {
				level3mid = 0;
			} else {
				// use capitals to visually accentuate presence
				if (track) printf(",\"MID\":\"%u:%s/%u:%.*s\"",
						   level3mid, pMember->name,
						   pMember->tid, pStore->signatures[pMember->sid].numPlaceholder, pStore->revTransformNames[pMember->tid]);
			}

			/*
			 * Translate slots relative to `rwSlots[]`
			 */
			for (unsigned i = tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++)
				sidSlots[i] = rwSlots[i];
			for (unsigned i = tinyTree_t::TINYTREE_KSTART; i < nextSlotId; i++)
				sidSlots[i] = rwSlots[tinyTree_t::TINYTREE_KSTART + pStore->fwdTransformNames[tid][i - tinyTree_t::TINYTREE_KSTART] - 'a'];

			if (track) {
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

			signature_t *pSignature = pStore->signatures + level3sid;
			if (pSignature->swapId) {
				swap_t *pSwap = pStore->swaps + pSignature->swapId;

				bool changed;
				do {
					changed = false;

					for (unsigned iSwap = 0; iSwap < swap_t::MAXENTRY && pSwap->tids[iSwap]; iSwap++) {
						unsigned tid = pSwap->tids[iSwap];

						// get the transform string
						const char *pTransformStr = pStore->fwdTransformNames[tid];

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
							if (track) {
								if (!displayed)
									printf(",   \"level4\":[");
								else
									printf(",");
								printf("%.*s", pSignature->numPlaceholder, pStore->fwdTransformNames[tid]);
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
				} while(changed);
			}

			if (track) {
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
				if (track) printf(",\"probe\":[");
				for (unsigned iMid = pStore->signatures[level3sid].firstMember; iMid; iMid = pStore->members[iMid].nextMember) {
					member_t *pMember = pStore->members + iMid;

					// depreciated are at the end of the list
					if (pMember->flags & member_t::MEMMASK_DEPR)
						break;

					uint32_t failCount = 0;
					explainStringFast(depth + 1, expectId, pTree, pMember->name, pStore->revTransformNames[pMember->tid], sidSlots + tinyTree_t::TINYTREE_KSTART, &failCount);
					if (track) {
						if (level5mid != 0)
							printf(",");
						printf("{\"name\":\"%u:%s/%u:%.*s\",\"miss\":%u}", iMid, pMember->name,
						       pMember->tid, pStore->signatures[pMember->sid].numPlaceholder, pStore->revTransformNames[pMember->tid],
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
				if (track) printf("]");
			}
			assert(level5mid);

			// apply best
			member_t *pMember = pStore->members + level5mid;

			if (track) printf(",   \"level5\":{\"member\":\"%u:%s/%u:%.*s\"}",
					   level5mid, pMember->name,
					   pMember->tid, pStore->signatures[pMember->sid].numPlaceholder, pStore->revTransformNames[pMember->tid]);
		}

		/*
		 * apply found member
		 */
		uint32_t ret = explainStringFast(depth + 1, expectId, pTree, pStore->members[level5mid].name, pStore->revTransformNames[pStore->members[level5mid].tid], sidSlots + tinyTree_t::TINYTREE_KSTART, NULL);

		if (track) printf(",   \"N\":%s%u}", ibit ? "~" : "", ret);

		/*
		 * @date 2021-08-11 22:38:55
		 * Sometimes a rerun may result in a different tree.
		 * This is because normalisation adapts to what is already found in the tree.
		 *
		 * Some used samples
		 * ./bexplain 'cd^agd1!eh^!a2gdgcd!^c!!!' 'cd^agd1!eh^!a2gdgcd!^c!!!'
		 * ./bexplain 'ef^eg!gg2^^eg!ab^c1dacab!^!^^1aabccd^a7>!+2!^B2ac!ccdB3!^ac!>!^^2!C6C1B5^1g>C8!^1C5c>C6d1!^ggef+^eD5>!5caB1C6!C6!!^93^4gB0^^9B0!>!^^'
		 */

		return ret ^ ibit;
	}

	/*
	 * @date 2021-07-05 19:19:25
	 *
	 * Expand and create a structure name with transform.
	 * Fast version specifically for user input
	 *
	 * @param {number} depth - Recursion depth
	 * @param {number} expectId - Recursion end condition, the node id to be added
	 * @param {baseTree_t*} pTree - Tree containing nodes
	 * @param {string} name - structure name
	 * @param {string} skin - structure skin
	 * @param {number[]} slots - structure run-time slots
	 * @param {unsigned*} pFailCount - null: apply changed, non-null: stay silent and count missing nodes (when nondryRun==true)
	 * @return {number} newly created nodeId
	 */
	uint32_t explainStringSafe(unsigned depth, baseTree_t *pTree, const char *pPattern, const char *pTransform) {

		// modify if transform is present
		uint32_t *transformList = NULL;
		if (pTransform && *pTransform)
			transformList = pTree->decodeTransform(ctx, pTree->kstart, pTree->nstart, pTransform);

		/*
		 * init
		 */

		uint32_t stackPos = 0;
		uint32_t nextNode = pTree->nstart;
		uint32_t *pStack  = pTree->allocMap();
		uint32_t *pMap    = pTree->allocMap();
		uint32_t nid;

		/*
		 * Load string
		 */
		for (const char *pattern = pPattern; *pattern; pattern++) {

			switch (*pattern) {
			case '0': //
				pStack[stackPos++] = 0;
				break;

				// @formatter:off
			case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8':
			case '9':
				// @formatter:on
			{
				/*
				 * Push back-reference
				 */
				uint32_t v = nextNode - (*pattern - '0');

				if (v < pTree->nstart || v >= nextNode)
					ctx.fatal("[node out of range: %d]\n", v);
				if (stackPos >= pTree->ncount)
					ctx.fatal("[stack overflow]\n");

				pStack[stackPos++] = pMap[v];

				break;
			}

				// @formatter:off
			case 'a': case 'b': case 'c': case 'd':
			case 'e': case 'f': case 'g': case 'h':
			case 'i': case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o': case 'p':
			case 'q': case 'r': case 's': case 't':
			case 'u': case 'v': case 'w': case 'x':
			case 'y': case 'z':
				// @formatter:on
			{
				/*
				 * Push endpoint
				 */
				uint32_t v = pTree->kstart + (*pattern - 'a');

				if (v < pTree->kstart || v >= pTree->nstart)
					ctx.fatal("[endpoint out of range: %d]\n", v);
				if (stackPos >= pTree->ncount)
					ctx.fatal("[stack overflow]\n");

				if (transformList)
					pStack[stackPos++] = transformList[v];
				else
					pStack[stackPos++] = v;
				break;

			}

				// @formatter:off
			case 'A': case 'B': case 'C': case 'D':
			case 'E': case 'F': case 'G': case 'H':
			case 'I': case 'J': case 'K': case 'L':
			case 'M': case 'N': case 'O': case 'P':
			case 'Q': case 'R': case 'S': case 'T':
			case 'U': case 'V': case 'W': case 'X':
			case 'Y': case 'Z':
				// @formatter:on
			{
				/*
				 * Prefix
				 */
				uint32_t v = 0;
				while (isupper(*pattern))
					v = v * 26 + *pattern++ - 'A';

				if (isdigit(*pattern)) {
					/*
					 * prefixed back-reference
					 */
					v = nextNode - (v * 10 + *pattern - '0');

					if (v < pTree->nstart || v >= nextNode)
						ctx.fatal("[node out of range: %d]\n", v);
					if (stackPos >= pTree->ncount)
						ctx.fatal("[stack overflow]\n");

					pStack[stackPos++] = pMap[v];
				} else if (islower(*pattern)) {
					/*
					 * prefixed endpoint
					 */
					v = pTree->kstart + (v * 26 + *pattern - 'a');

					if (v < pTree->kstart || v >= pTree->nstart)
						ctx.fatal("[endpoint out of range: %d]\n", v);
					if (stackPos >= pTree->ncount)
						ctx.fatal("[stack overflow]\n");

					if (transformList)
						pStack[stackPos++] = transformList[v];
					else
						pStack[stackPos++] = v;
				} else {
					ctx.fatal("[bad token '%c']\n", *pattern);
				}
				break;
			}

			case '>': {
				// GT
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				uint32_t R = pStack[--stackPos];
				uint32_t L = pStack[--stackPos];

				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, R ^ IBIT, 0, NULL);
				if (track) printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '+': {
				// OR
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t R = pStack[--stackPos]; // right hand side
				uint32_t L = pStack[--stackPos]; // left hand side

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, IBIT, R, NULL);
				if (track) printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '^': {
				// XOR/NE
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t R = pStack[--stackPos]; // right hand side
				uint32_t L = pStack[--stackPos]; // left hand side

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, R ^ IBIT, R, NULL);
				if (track) printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '!': {
				// QnTF
				if (stackPos < 3)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t F = pStack[--stackPos];
				uint32_t T = pStack[--stackPos];
				uint32_t Q = pStack[--stackPos];

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, Q, T ^ IBIT, F, NULL);
				if (track) printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '&': {
				// AND
				if (stackPos < 2)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t R = pStack[--stackPos]; // right hand side
				uint32_t L = pStack[--stackPos]; // left hand side

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, L, R, 0, NULL);
				if (track) printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '?': {
				// QTF
				if (stackPos < 3)
					ctx.fatal("[stack underflow]\n");

				// pop operands
				uint32_t F = pStack[--stackPos];
				uint32_t T = pStack[--stackPos];
				uint32_t Q = pStack[--stackPos];

				// create operator
				nid = explainNormaliseNode(depth, pTree->ncount, pTree, Q, T, F, NULL);
				if (track) printf("\n");

				pStack[stackPos++] = pMap[nextNode++] = nid;
				break;
			}
			case '~': {
				// NOT
				if (stackPos < 1)
					ctx.fatal("[stack underflow]\n");

				pStack[stackPos - 1] ^= IBIT;
				break;
			}
			case '/':
				// separator between pattern/transform
				while (pattern[1])
					pattern++;
				break;
			case ' ':
				// skip spaces
				break;
			default:
				ctx.fatal("[bad token '%c']\n", *pattern);
			}

			if (stackPos > pTree->maxNodes)
				ctx.fatal("[stack overflow]\n");
		}
		if (stackPos != 1)
			ctx.fatal("[stack not empty]\n");

		uint32_t ret = pStack[stackPos - 1];

		pTree->freeMap(pStack);
		pTree->freeMap(pMap);
		if (transformList)
			pTree->freeMap(transformList);

		return ret;
	}

	/*
	 * import/fold
	 */

};

#endif // _BASEEXPLAIN_H
