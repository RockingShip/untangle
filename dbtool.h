#ifndef _DBTOOL_H
#define _DBTOOL_H

/*
 * @date 2020-04-27 18:47:26
 *
 * A collection of utilities shared across database creation tools `gensignature`, `genhint`, `genmember` and more.
 *
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
#include "context.h"
#include "database.h"
#include "generator.h"
#include "metrics.h"

struct dbtool_t : callable_t {

	/*
	 * @date 2020-03-25 15:06:54
	 */
	enum {
		/*
		 * @date 2020-03-30 16:17:28
		 *
		 * default interleave (taken from `ratioMetrics_X[]`)
		 * In general 504 seems to be best choice
		 * However, with 4-nodes, 120 is just as fast as 504 but uses half storage.
		 * With 4n9-i120 imprint storage is 8G. On machines with 32G memory this gives about 4 workers with each 4G local and 8G shared memory
		 *
		 * @date 2020-04-04 20:56:35
		 *
		 * After experience, 504 is definitely faster
		 */
		METRICS_DEFAULT_INTERLEAVE = 504,

		// default ratio (taken from `ratioMetrics_X[]`). NOTE: Times 10!
		METRICS_DEFAULT_RATIO = 50, // NOTE: Its actually 5.0
	};

	/// @var {context_t} I/O context
	context_t &ctx;

	/// @var {number} size of imprint index WARNING: must be prime
	unsigned opt_imprintIndexSize;
	/// @var {number} size of hint index WARNING: must be prime
	unsigned opt_hintIndexSize;
	/// @var {number} interleave for associative imprint index
	unsigned opt_interleave;
	/// @var {number} Maximum number of hints to be stored database
	unsigned opt_maxHint;
	/// @var {number} Maximum number of imprints to be stored database
	unsigned opt_maxImprint;
	/// @var {number} Maximum number of members to be stored database
	unsigned opt_maxMember;
	/// @var {number} Maximum number of signatures to be stored database
	unsigned opt_maxSignature;
	/// @var {number} size of member index WARNING: must be prime
	unsigned opt_memberIndexSize;
	/// @var {number} index/data ratio
	double opt_ratio;
	/// @var {number} size of signature index WARNING: must be prime
	unsigned opt_signatureIndexSize;

	// primary sections, ones that get modified and need to be writable
	unsigned primarySections;
	// sections that need rebuilding
	unsigned rebuildSections;
	// sections to inherit from original database. Can also be interpreted as ReadOnly.
	unsigned inheritSections;

	/**
	 * Constructor
	 */
	dbtool_t(context_t &ctx) : ctx(ctx) {
		// arguments and options
		opt_imprintIndexSize = 0;
		opt_hintIndexSize = 0;
		opt_interleave = 0;
		opt_maxHint = 0;
		opt_maxImprint = 0;
		opt_maxMember = 0;
		opt_maxSignature = 0;
		opt_memberIndexSize = 0;
		opt_ratio = METRICS_DEFAULT_RATIO / 10.0;
		opt_signatureIndexSize = 0;

		primarySections = 0;
		rebuildSections = 0;
		inheritSections = database_t::ALLOCMASK_TRANSFORM;
	}

	/**
	 * @date 2020-04-25 00:05:32
	 *
	 * NOTE: `signatureIndex`, `hintIndex` and `imprintIndex` are first-level indices derived from `signatures`, `hints` and `imprints`
	 *       `imprints` is a second-level index derived from `signatures`
	 *
	 * workflow:
	 *   - No output specified make primary sections/indices secondary
	 *   - Size output sections according to command-line overrides
	 *   - If none given for primary sections (signatures,imprints) take from metrics
	 *   - If none given for secondary sections (hints) inherit from input database
	 *   - Any changes that change the hashing properties of indices invalidate them and require rebuilding
	 *   - Any primary section/index have their contents copied
	 *   - Any secondary section/index that remain same size get inherited
	 *   - All indices must have at least one entry more then their data
	 *   - All primary sections must have at least the reserved first entry
	 *   - Any secondary section may have zero entries
	 *
	 * @date 2020-04-21 19:59:47
	 *
	 * if (inheritSection)
	 *   inherit();
	 * else if (rebuildSection)
	 *   rebuild();
	 * else
	 *   copy();
	 *
	 * @param {database_t} store - writable output database
	 * @param {database_t} db - read-only input database
	 * @param {number} numNodes - to find matching metrics
	 */
	void sizeDatabaseSections(database_t &store, const database_t &db, unsigned numNodes) {

		/*
		 * @date 2020-03-17 13:57:25
		 *
		 * Database indices are hashlookup tables with overflow.
		 * The art is to have a hash function that distributes evenly over the hashtable.
		 * If index entries are in use, then jump to overflow entries.
		 * The larger the index in comparison to the number of data entries the lower the chance an overflow will occur.
		 * The ratio between index and data size is called `ratio`.
		 */

		// signatures
		if (this->opt_maxSignature != 0) {
			store.maxSignature = this->opt_maxSignature; // user specified
		} else if (~primarySections & database_t::ALLOCMASK_SIGNATURE) {
			store.maxSignature = db.maxSignature; // keep section read-only
			store.signatureIndexSize = db.signatureIndexSize;
		} else {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, numNodes);
			if (!pMetrics)
				ctx.fatal("no preset for --maxsignature\n");

			store.maxSignature = pMetrics->numSignature;
		}

		if (store.signatureIndexSize == 0) {
			if (this->opt_signatureIndexSize == 0)
				store.signatureIndexSize = ctx.nextPrime(store.maxSignature * this->opt_ratio);
			else
				store.signatureIndexSize = this->opt_signatureIndexSize;
		}

		// hints
		if (this->opt_maxHint != 0) {
			store.maxHint = this->opt_maxHint; // user specified
		} else if (~primarySections & database_t::ALLOCMASK_HINT) {
			store.maxHint = db.maxHint; // keep section read-only
			store.hintIndexSize = db.hintIndexSize;
		} else {
			ctx.fatal("no preset for --maxhint\n");
		}

		if (store.hintIndexSize == 0) {
			if (this->opt_hintIndexSize == 0)
				store.hintIndexSize = ctx.nextPrime(store.maxHint * this->opt_ratio);
			else
				store.hintIndexSize = this->opt_hintIndexSize;
		}

		// interleave
		if (this->opt_interleave)
			store.interleave = this->opt_interleave; // manual
		else if (db.interleave)
			store.interleave = db.interleave; // inherit
		else
			store.interleave = METRICS_DEFAULT_INTERLEAVE; // default

		// find matching `interleaveStep`
		{
			const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, store.interleave);
			if (!pMetrics)
				ctx.fatal("no preset for --interleave\n");

			store.interleaveStep = pMetrics->interleaveStep;
		}

		// imprints
		if (this->opt_maxImprint != 0) {
			store.maxImprint = this->opt_maxImprint; // user specified
		} else if ((~primarySections & database_t::ALLOCMASK_IMPRINT) && store.interleave == db.interleave) {
			store.maxImprint = db.maxImprint; // keep section read-only BUT only with unchanged interleave
			store.imprintIndexSize = db.imprintIndexSize;
		} else {
			const metricsImprint_t *pMetrics = getMetricsImprint(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, store.interleave, numNodes);
			if (!pMetrics)
				ctx.fatal("no preset for --maximprint\n");

			store.maxImprint = pMetrics->numImprint;
		}

		if (store.imprintIndexSize == 0) {
			if (this->opt_imprintIndexSize != 0)
				store.imprintIndexSize = this->opt_imprintIndexSize;
			else
				store.imprintIndexSize = ctx.nextPrime(store.maxImprint * this->opt_ratio);
		}

		// members
		if (this->opt_maxMember != 0) {
			store.maxMember = this->opt_maxMember; // user specified
		} else if (~primarySections & database_t::ALLOCMASK_MEMBER) {
			store.maxMember = db.maxMember; // keep section read-only
			store.memberIndexSize = db.memberIndexSize;
		} else {
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, numNodes);
			if (!pMetrics)
				ctx.fatal("no preset for --maxmember\n");

			store.maxMember = pMetrics->numMember;
		}

		if (store.memberIndexSize == 0) {
			if (!(this->opt_memberIndexSize == 0))
				store.memberIndexSize = this->opt_memberIndexSize;
			else
				store.memberIndexSize = ctx.nextPrime(store.maxMember * this->opt_ratio);
		}

		if (store.maxSignature && store.maxSignature > store.signatureIndexSize + 1)
			ctx.fatal("--maxsignature=%u exceeds --signatureIndexSize=%u\n", store.maxSignature, store.signatureIndexSize);
		if (store.maxHint && store.maxHint > store.hintIndexSize + 1)
			ctx.fatal("--maxhint=%u exceeds --hintIndexSize=%u\n", store.maxHint, store.hintIndexSize);
		if (store.maxImprint && store.maxImprint > store.imprintIndexSize + 1)
			ctx.fatal("--maximprint=%u exceeds --imprintIndexSize=%u\n", store.maxImprint, store.imprintIndexSize);
	}

	/**
	 * @date 2020-04-27 19:39:38
	 *
	 * Determine is sections need to be rebuilt, inherited or copied.
	 *
	 * @param {database_t} store - writable output database
	 * @param {database_t} db - read-only input database
	 * @param {number} numNodes - to find matching metrics
	 */
	void modeDatabaseSections(database_t &store, const database_t &db) {
		// signatures
		if (db.numSignature > 0) {
			if ((~primarySections & database_t::ALLOCMASK_SIGNATURE) && store.maxSignature <= db.maxSignature)
				inheritSections |= database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_SIGNATUREINDEX;
			if (store.signatureIndexSize != db.signatureIndexSize)
				rebuildSections |= database_t::ALLOCMASK_SIGNATUREINDEX;
		} else if (store.maxSignature > 0) {
			// on first create, rebuild
			rebuildSections |= database_t::ALLOCMASK_SIGNATUREINDEX;
		}

		// hints
		if (db.numHint > 0) {
			if ((~primarySections & database_t::ALLOCMASK_HINT) && store.maxHint <= db.maxHint)
				inheritSections |= database_t::ALLOCMASK_HINT | database_t::ALLOCMASK_HINTINDEX;
			if (store.hintIndexSize != db.hintIndexSize)
				rebuildSections |= database_t::ALLOCMASK_HINTINDEX;
		} else if (store.maxHint > 0) {
			// on first create, rebuild
			rebuildSections |= database_t::ALLOCMASK_HINTINDEX;
		}

		// changing interleave needs imprint rebuilding. This also invalidates imprintIndex
		if (store.interleave != db.interleave)
			rebuildSections |= database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX;

		// imprints
		if (db.numImprint > 0) {
			if ((~primarySections & database_t::ALLOCMASK_IMPRINT) && store.maxImprint <= db.maxImprint)
				inheritSections |= database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX;
			if (store.imprintIndexSize != db.imprintIndexSize || db.numImprint == 0)
				rebuildSections |= database_t::ALLOCMASK_IMPRINTINDEX;
		} else if (store.maxImprint > 0) {
			// on first create, rebuild
			rebuildSections |= database_t::ALLOCMASK_IMPRINTINDEX;
		}

		// imprints are level-2 index. If absent re-create.
		if (db.numSignature > 0 && db.numImprint == 0)
			rebuildSections |= database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX;

		// members
		if (db.numMember > 0) {
			if ((~primarySections & database_t::ALLOCMASK_MEMBER) && store.maxMember <= db.maxMember)
				inheritSections |= database_t::ALLOCMASK_MEMBER | database_t::ALLOCMASK_MEMBERINDEX;
			if (store.memberIndexSize != db.memberIndexSize || db.numMember == 0)
				rebuildSections |= database_t::ALLOCMASK_MEMBERINDEX;
		} else if (store.maxMember > 0) {
			// on first create, rebuild
			rebuildSections |= database_t::ALLOCMASK_MEMBERINDEX;
		}

		// rebuilt (rw) sections may not be inherited (ro)
		inheritSections &= ~rebuildSections;

	}

	/**
	 * @date 2020-04-27 20:08:14
	 *
	 * With copy-on-write, only `::memcpy()` when the output section if larger, otherwise inherit
	 *
	 * @param {database_t} store - writable output database
	 * @param {database_t} db - read-only input database
	 */
	void populateDatabaseSections(database_t &store, const database_t &db) {

		// transforms are never invalid or resized
		if (~rebuildSections & database_t::ALLOCMASK_TRANSFORM) {
			assert(~store.allocFlags & database_t::ALLOCMASK_TRANSFORM);

			assert(db.numTransform == MAXTRANSFORM);
			store.maxTransform = db.maxTransform;
			store.numTransform = db.numTransform;

			store.fwdTransformData = db.fwdTransformData;
			store.revTransformData = db.revTransformData;
			store.fwdTransformNames = db.fwdTransformNames;
			store.revTransformNames = db.revTransformNames;
			store.revTransformIds = db.revTransformIds;

			assert(db.transformIndexSize > 0);
			store.transformIndexSize = db.transformIndexSize;

			store.fwdTransformNameIndex = db.fwdTransformNameIndex;
			store.revTransformNameIndex = db.revTransformNameIndex;
		}

		// signatures
		if (~rebuildSections & database_t::ALLOCMASK_SIGNATURE) {
			if (store.maxSignature <= db.maxSignature) {
				assert(~store.allocFlags & database_t::ALLOCMASK_SIGNATURE);
				store.maxSignature = db.maxSignature;
				store.numSignature = db.numSignature;
				store.signatures = db.signatures;
			} else if (db.numSignature > 0) {
				fprintf(stderr, "[%s] Copying signature section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_SIGNATURE);
				assert(store.maxSignature >= db.numSignature);
				::memcpy(store.signatures, db.signatures, db.numSignature * sizeof(*db.signatures));
				store.numSignature = db.numSignature;
			}

			if (store.numSignature == 0 && (primarySections & database_t::ALLOCMASK_SIGNATURE))
				store.numSignature = 1; // skip reserved entry
		}

		if (~rebuildSections & database_t::ALLOCMASK_SIGNATUREINDEX) {
			assert(store.signatureIndexSize == db.signatureIndexSize);

			assert(~store.allocFlags & database_t::ALLOCMASK_SIGNATUREINDEX);
			store.signatureIndexSize = db.signatureIndexSize;
			store.signatureIndex = db.signatureIndex;

			assert(store.signatureIndexSize == 0 || store.signatureIndexSize > store.maxSignature + 1);
		}

		// hints
		if (~rebuildSections & database_t::ALLOCMASK_HINT) {
			if (store.maxHint <= db.maxHint) {
				assert(~store.allocFlags & database_t::ALLOCMASK_HINT);
				store.maxHint = db.maxHint;
				store.numHint = db.numHint;
				store.hints = db.hints;
			} else if (db.numHint > 0) {
				fprintf(stderr, "[%s] Copying hint section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_HINT);
				assert(store.maxHint >= db.numHint);
				::memcpy(store.hints, db.hints, db.numHint * sizeof(*db.hints));
				store.numHint = db.numHint;
			}

			if (store.numHint == 0 && (primarySections & database_t::ALLOCMASK_HINT))
				store.numHint = 1; // skip reserved entry
		}

		if (~rebuildSections & database_t::ALLOCMASK_HINTINDEX) {
			assert(store.hintIndexSize == db.hintIndexSize);

			assert(~store.allocFlags & database_t::ALLOCMASK_HINTINDEX);
			store.hintIndexSize = db.hintIndexSize;
			store.hintIndex = db.hintIndex;

			assert(store.hintIndexSize == 0 || store.hintIndexSize > store.maxHint + 1);
		}

		// imprints
		if (~rebuildSections & database_t::ALLOCMASK_IMPRINT) {
			if (store.maxImprint <= db.maxImprint) {
				assert(~store.allocFlags & database_t::ALLOCMASK_IMPRINT);
				store.maxImprint = db.maxImprint;
				store.numImprint = db.numImprint;
				store.imprints = db.imprints;
			} else if (db.numImprint > 0) {
				fprintf(stderr, "[%s] Copying imprint section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_IMPRINT);
				assert(store.maxImprint >= db.numImprint);
				::memcpy(store.imprints, db.imprints, db.numImprint * sizeof(*db.imprints));
				store.numImprint = db.numImprint;
			}

			if (store.numImprint == 0 && (primarySections & database_t::ALLOCMASK_IMPRINT))
				store.numImprint = 1; // skip reserved entry
		}

		if (~rebuildSections & database_t::ALLOCMASK_IMPRINTINDEX) {
			assert(store.imprintIndexSize == db.imprintIndexSize);

			assert(~store.allocFlags & database_t::ALLOCMASK_IMPRINTINDEX);
			store.imprintIndexSize = db.imprintIndexSize;
			store.imprintIndex = db.imprintIndex;

			assert(store.imprintIndexSize == 0 || store.imprintIndexSize > store.maxImprint + 1);
		}


		// members
		if (~rebuildSections & database_t::ALLOCMASK_MEMBER) {
			if (store.maxMember <= db.maxMember) {
				assert(~store.allocFlags & database_t::ALLOCMASK_MEMBER);
				store.maxMember = db.maxMember;
				store.numMember = db.numMember;
				store.members = db.members;
			} else if (db.numMember > 0) {
				fprintf(stderr, "[%s] Copying member section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_MEMBER);
				assert(store.maxMember >= db.numMember);
				::memcpy(store.members, db.members, db.numMember * sizeof(*db.members));
				store.numMember = db.numMember;
			}

			if (store.numMember == 0 && (primarySections & database_t::ALLOCMASK_MEMBER))
				store.numMember = 1; // skip reserved entry
		}

		if (~rebuildSections & database_t::ALLOCMASK_MEMBERINDEX) {
			assert(store.memberIndexSize == db.memberIndexSize);

			assert(~store.allocFlags & database_t::ALLOCMASK_MEMBERINDEX);
			store.memberIndexSize = db.memberIndexSize;
			store.memberIndex = db.memberIndex;

			assert(store.memberIndexSize == 0 || store.memberIndexSize > store.maxImprint + 1);
		}
	}
};

#endif