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
		return false;
	}
};

#endif
