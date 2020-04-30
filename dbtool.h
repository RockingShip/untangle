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

	/// @var {number} "0" assume input is read-only, else input is copy-on-write.
	unsigned copyOnWrite;
	/// @var {number} may/maynot make changes to database
	unsigned readOnlyMode;
	// allocated sections that need rebuilding
	unsigned rebuildSections;
	// mmapped sections that are copy-on-write
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

		copyOnWrite = 0;
		inheritSections = database_t::ALLOCMASK_TRANSFORM |
		                  database_t::ALLOCMASK_SIGNATURE | database_t::ALLOCMASK_SIGNATUREINDEX |
		                  database_t::ALLOCMASK_HINT | database_t::ALLOCMASK_HINTINDEX |
		                  database_t::ALLOCMASK_IMPRINT | database_t::ALLOCMASK_IMPRINTINDEX |
		                  database_t::ALLOCMASK_MEMBER | database_t::ALLOCMASK_MEMBERINDEX;
		readOnlyMode = 0;
		rebuildSections = 0;
	}

	/**
	 * @date 2020-04-25 00:05:32
	 *
	 * NOTE: `signatureIndex`, `hintIndex` and `imprintIndex` are first-level indices derived from `signatures`, `hints` and `imprints`.
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

		/*
		 * signatures
		 */

		if (inheritSections & database_t::ALLOCMASK_SIGNATURE) {
			// inherited. pass-though
			store.maxSignature = db.numSignature;
		} else if (this->opt_maxSignature) {
			// user specified
			store.maxSignature = this->opt_maxSignature;
		} else {
			// metrics
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, numNodes);
			if (!pMetrics)
				ctx.fatal("no preset for --maxsignature\n");

			store.maxSignature = pMetrics->numSignature;
		}

		if (inheritSections & database_t::ALLOCMASK_SIGNATUREINDEX) {
			// inherited. pass-though
			store.signatureIndexSize = db.signatureIndexSize;
		} else if (this->opt_signatureIndexSize) {
			// user specified
			store.signatureIndexSize = this->opt_signatureIndexSize;
		} else if (~inheritSections & database_t::ALLOCMASK_SIGNATURE) {
			// data is not inherited, apply fitting indexSize
			store.signatureIndexSize = ctx.nextPrime(store.maxSignature * this->opt_ratio);
			if (this->copyOnWrite && store.signatureIndexSize == db.signatureIndexSize)
				inheritSections |= database_t::ALLOCMASK_SIGNATUREINDEX;
		} else if (db.signatureIndexSize) {
			// inherit input size
			store.signatureIndexSize = db.signatureIndexSize;
		} else {
			// create new index for inherited data
			store.signatureIndexSize = ctx.nextPrime(store.maxSignature * this->opt_ratio);
		}
		// input index empty or unusable
		if (db.numSignature && (~inheritSections & database_t::ALLOCMASK_SIGNATUREINDEX)) {
			if (!db.signatureIndexSize || db.signatureIndexSize != store.signatureIndexSize)
				rebuildSections |= database_t::ALLOCMASK_SIGNATUREINDEX;
		}

		/*
		 * hints
		 */

		if (inheritSections & database_t::ALLOCMASK_HINT) {
			// inherited. pass-though
			store.maxHint = db.numHint;
		} else if (this->opt_maxHint) {
			// user specified
			store.maxHint = this->opt_maxHint;
		} else {
			// metrics
#if 1
			store.maxHint = 255; // hardcoded metrics
#else
			ctx.fatal("no preset for --maxhint\n");
#endif
		}

		if (inheritSections & database_t::ALLOCMASK_HINTINDEX) {
			// inherited. pass-though
			store.hintIndexSize = db.hintIndexSize;
		} else if (this->opt_hintIndexSize) {
			// user specified
			store.hintIndexSize = this->opt_hintIndexSize;
		} else if (~inheritSections & database_t::ALLOCMASK_HINT) {
			// data is not inherited, apply fitting indexSize
			store.hintIndexSize = ctx.nextPrime(store.maxHint * this->opt_ratio);
			if (this->copyOnWrite && store.hintIndexSize == db.hintIndexSize)
				inheritSections |= database_t::ALLOCMASK_HINTINDEX;
		} else if (db.hintIndexSize) {
			// inherit input size
			store.hintIndexSize = db.hintIndexSize;
		} else {
			// create new index for inherited data
			store.hintIndexSize = ctx.nextPrime(store.maxHint * this->opt_ratio);
		}
		// input index empty or unusable
		if (db.numHint && (~inheritSections & database_t::ALLOCMASK_HINTINDEX)) {
			if (!db.hintIndexSize || db.hintIndexSize != store.hintIndexSize)
				rebuildSections |= database_t::ALLOCMASK_HINTINDEX;
		}

		/*
		 * imprints
		 */

		// interleave
		if (this->opt_interleave) {
			// user specified
			const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, this->opt_interleave);
			if (!pMetrics)
				ctx.fatal("no preset for --interleave\n");

			store.interleaveStep = pMetrics->interleaveStep;
			store.interleave = this->opt_interleave;
		} else if (inheritSections & database_t::ALLOCMASK_IMPRINT) {
			// inherited. pass-though
			store.interleave = db.interleave;
			store.interleaveStep = db.interleaveStep;
		} else if (this->opt_interleave) {
			// user specified
			const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, this->opt_interleave);
			if (!pMetrics)
				ctx.fatal("no preset for --interleave\n");

			store.interleaveStep = pMetrics->interleaveStep;
			store.interleave = this->opt_interleave;
		} else if (db.interleave) {
			// unspecified, can inherit
			store.interleave = db.interleave;
			store.interleaveStep = db.interleaveStep;
		} else {
			// set interleave when first time
			this->opt_interleave = METRICS_DEFAULT_INTERLEAVE;

			const metricsInterleave_t *pMetrics = getMetricsInterleave(MAXSLOTS, this->opt_interleave);
			assert(pMetrics);

			store.interleaveStep = pMetrics->interleaveStep;
			store.interleave = this->opt_interleave;
		}

		if (inheritSections & database_t::ALLOCMASK_IMPRINT) {
			// inherited. pass-though
			store.maxImprint = db.numImprint;
		} else if (this->opt_maxImprint) {
			// user specified
			store.maxImprint = this->opt_maxImprint;
		} else {
			// metrics
			const metricsImprint_t *pMetrics = getMetricsImprint(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, store.interleave, numNodes);
			if (!pMetrics)
				ctx.fatal("no preset for --maximprint\n");

			store.maxImprint = pMetrics->numImprint;
		}

		if (inheritSections & database_t::ALLOCMASK_IMPRINTINDEX) {
			// inherited. pass-though
			store.imprintIndexSize = db.imprintIndexSize;
		} else if (this->opt_imprintIndexSize) {
			// user specified
			store.imprintIndexSize = this->opt_imprintIndexSize;
		} else if (~inheritSections & database_t::ALLOCMASK_IMPRINT) {
			// data is not inherited, apply fitting indexSize
			store.imprintIndexSize = ctx.nextPrime(store.maxImprint * this->opt_ratio);
			if (this->copyOnWrite && store.imprintIndexSize == db.imprintIndexSize)
				inheritSections |= database_t::ALLOCMASK_IMPRINTINDEX;
		} else if (db.imprintIndexSize) {
			// inherit input size
			store.imprintIndexSize = db.imprintIndexSize;
		} else {
			// create new index for inherited data
			store.imprintIndexSize = ctx.nextPrime(store.maxImprint * this->opt_ratio);
		}
		// input index empty or unusable
		if (db.numImprint && (~inheritSections & database_t::ALLOCMASK_IMPRINTINDEX)) {
			if (!db.imprintIndexSize || db.imprintIndexSize != store.imprintIndexSize)
				rebuildSections |= database_t::ALLOCMASK_IMPRINTINDEX;
		}

		/*
		 * members
		 */

		if (inheritSections & database_t::ALLOCMASK_MEMBER) {
			// inherited. pass-though
			store.maxMember = db.numMember;
		} else if (this->opt_maxMember) {
			// user specified
			store.maxMember = this->opt_maxMember; // user specified
		} else {
			// metrics
			const metricsGenerator_t *pMetrics = getMetricsGenerator(MAXSLOTS, ctx.flags & ctx.MAGICMASK_PURE, numNodes);
			if (!pMetrics)
				ctx.fatal("no preset for --maxmember\n");

			store.maxMember = pMetrics->numMember;
		}

		if (inheritSections & database_t::ALLOCMASK_MEMBERINDEX) {
			// inherited. pass-though
			store.memberIndexSize = db.memberIndexSize;
		} else if (this->opt_memberIndexSize) {
			// user specified
			store.memberIndexSize = this->opt_memberIndexSize;
		} else if (~inheritSections & database_t::ALLOCMASK_MEMBER) {
			// data is not inherited, apply fitting indexSize
			store.memberIndexSize = ctx.nextPrime(store.maxMember * this->opt_ratio);
			if (this->copyOnWrite && store.memberIndexSize == db.memberIndexSize)
				inheritSections |= database_t::ALLOCMASK_MEMBERINDEX;
		} else if (db.memberIndexSize) {
			// inherit input size
			store.memberIndexSize = db.memberIndexSize;
		} else {
			// create new index for inherited data
			store.memberIndexSize = ctx.nextPrime(store.maxMember * this->opt_ratio);
		}
		// input index empty or unusable
		if (db.numMember && (~inheritSections & database_t::ALLOCMASK_MEMBERINDEX)) {
			if (!db.memberIndexSize || db.memberIndexSize != store.memberIndexSize)
				rebuildSections |= database_t::ALLOCMASK_MEMBERINDEX;
		}

		/*
		 * rebuilt (writable) sections may not be inherited (read-only)
		 */

		inheritSections &= ~rebuildSections;

		/*
		 * validate
		 */

		if (!readOnlyMode) {
			// indices must be at least one larger than their data
			if (store.maxSignature && store.maxSignature > store.signatureIndexSize + 1)
				ctx.fatal("--maxsignature=%u exceeds --signatureIndexSize=%u\n", store.maxSignature, store.signatureIndexSize);
			if (store.maxHint && store.maxHint > store.hintIndexSize + 1)
				ctx.fatal("--maxhint=%u exceeds --hintIndexSize=%u\n", store.maxHint, store.hintIndexSize);
			if (store.maxImprint && store.maxImprint > store.imprintIndexSize + 1)
				ctx.fatal("--maximprint=%u exceeds --imprintIndexSize=%u\n", store.maxImprint, store.imprintIndexSize);
		} else if (ctx.opt_verbose >= ctx.VERBOSE_WARNING) {
			if (rebuildSections)
				fprintf(stderr, "[%s] WARNING: readOnlyMode and database sections [%s] require rebuilding", ctx.timeAsString(), store.sectionToText(rebuildSections));
		}


		// output data must be large enough to fit input data
		if (store.maxSignature < db.numSignature)
			ctx.fatal("--maxsignature=%u needs to be at least %u\n", store.maxSignature, db.numSignature);
		if (store.maxHint < db.numHint)
			ctx.fatal("--maxhint=%u needs to be at least %u\n", store.maxHint, db.numHint);
		if (store.maxMember < db.numMember)
			ctx.fatal("--maxmember=%u needs to be at least %u\n", store.maxMember, db.numMember);
	}

	/**
	 * @date 2020-04-27 20:08:14
	 *
	 * With copy-on-write, only `::memcpy()` when the output section if larger, otherwise inherit
	 *
	 * @date 2020-04-29 10:10:18
	 *
	 * Depending on the mmap() mode.
	 *
	 * It is still undecided to use:
	 *   `mmap(MAP_PRIVATE)` with advantage of copy-on-write but disadvantage that each process has a private copy of (many) page table entries.
	 *   `mmap(MAP_SHARED)` with advantage of shared PTE's but slow `::memcpy()` to private memory.
	 *
	 * Or it could be hybrid that many workers use `MAP_SHARED` and single process use `MAP_PRIVATE`.
	 *
	 * @param {database_t} store - writable output database
	 * @param {database_t} db - read-only input database
	 */
	void populateDatabaseSections(database_t &store, const database_t &db) {

		if (ctx.opt_verbose >= ctx.VERBOSE_VERBOSE) {
			static char inheritText[512], rebuildText[512];
			store.sectionToText(inheritSections, inheritText);
			store.sectionToText(rebuildSections, rebuildText);
			fprintf(stderr, "[%s] copyOnWrite=%u inheritSections=[%s] rebuildSections=[%s]\n", ctx.timeAsString(), copyOnWrite, inheritText, rebuildText);
		}

		/*
		 * transforms are never invalid or resized
		 */

		if (inheritSections & database_t::ALLOCMASK_TRANSFORM) {
			assert(~store.allocFlags & database_t::ALLOCMASK_TRANSFORM);

			assert(db.numTransform == MAXTRANSFORM);
			store.maxTransform = db.numTransform;
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
		} else {
			assert(0);
		}

		/*
		 * signatures
		 */

		if (!store.maxSignature) {
			store.signatures = NULL;
			store.signatureIndex = NULL;
		} else {
			if (inheritSections & database_t::ALLOCMASK_SIGNATURE) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_SIGNATURE);
				store.signatures = db.signatures;
				store.numSignature = db.numSignature;
			} else if (!db.numSignature) {
				// input empty
				assert(store.allocFlags & database_t::ALLOCMASK_SIGNATURE);
				store.numSignature = 1;
			} else if (store.maxSignature <= db.numSignature && copyOnWrite) {
				// small enough to use copy-on-write
				assert(~store.allocFlags & database_t::ALLOCMASK_SIGNATURE);
				store.signatures = db.signatures;
				store.numSignature = db.numSignature;
			} else {
				fprintf(stderr, "[%s] Copying signature section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_SIGNATURE);
				::memcpy(store.signatures, db.signatures, db.numSignature * sizeof(*db.signatures));
				store.numSignature = db.numSignature;
			}

			if (inheritSections & database_t::ALLOCMASK_SIGNATUREINDEX) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_SIGNATUREINDEX);
				store.signatureIndexSize = db.signatureIndexSize;
				store.signatureIndex = db.signatureIndex;
			} else if (rebuildSections & database_t::ALLOCMASK_SIGNATUREINDEX) {
				// post-processing
				assert(store.allocFlags & database_t::ALLOCMASK_SIGNATUREINDEX);
			} else if (!db.signatureIndexSize) {
				// was missing
				assert(store.allocFlags & database_t::ALLOCMASK_SIGNATUREINDEX);
				::memset(store.signatureIndex, 0, store.signatureIndexSize);
			} else if (copyOnWrite) {
				// copy-on-write
				assert(store.signatureIndexSize == db.signatureIndexSize);
				assert(~store.allocFlags & database_t::ALLOCMASK_SIGNATUREINDEX);
				store.signatureIndex = db.signatureIndex;
				store.signatureIndexSize = db.signatureIndexSize;
			} else {
				// copy
				assert(store.signatureIndexSize == db.signatureIndexSize);
				assert(store.allocFlags & database_t::ALLOCMASK_SIGNATUREINDEX);
				::memcpy(store.signatureIndex, db.signatureIndex, db.signatureIndexSize);
				store.signatureIndexSize = db.signatureIndexSize;
			}
		}

		/*
		 * hints
		 */

		if (!store.maxHint) {
			store.hints = NULL;
			store.hintIndex = NULL;
		} else {
			if (inheritSections & database_t::ALLOCMASK_HINT) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_HINT);
				store.hints = db.hints;
				store.numHint = db.numHint;
			} else if (!db.numHint) {
				// input empty
				assert(store.allocFlags & database_t::ALLOCMASK_HINT);
				store.numHint = 1;
			} else if (store.maxHint <= db.numHint && copyOnWrite) {
				// small enough to use copy-on-write
				assert(~store.allocFlags & database_t::ALLOCMASK_HINT);
				store.hints = db.hints;
				store.numHint = db.numHint;
			} else {
				fprintf(stderr, "[%s] Copying hint section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_HINT);
				::memcpy(store.hints, db.hints, db.numHint * sizeof(*db.hints));
				store.numHint = db.numHint;
			}

			if (inheritSections & database_t::ALLOCMASK_HINTINDEX) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_HINTINDEX);
				store.hintIndexSize = db.hintIndexSize;
				store.hintIndex = db.hintIndex;
			} else if (rebuildSections & database_t::ALLOCMASK_HINTINDEX) {
				// post-processing
				assert(store.allocFlags & database_t::ALLOCMASK_HINTINDEX);
			} else if (!db.hintIndexSize) {
				// was missing
				assert(store.allocFlags & database_t::ALLOCMASK_HINTINDEX);
				::memset(store.hintIndex, 0, store.hintIndexSize);
			} else if (copyOnWrite) {
				// copy-on-write
				assert(store.hintIndexSize == db.hintIndexSize);
				assert(~store.allocFlags & database_t::ALLOCMASK_HINTINDEX);
				store.hintIndex = db.hintIndex;
				store.hintIndexSize = db.hintIndexSize;
			} else {
				// copy
				assert(store.hintIndexSize == db.hintIndexSize);
				assert(store.allocFlags & database_t::ALLOCMASK_HINTINDEX);
				::memcpy(store.hintIndex, db.hintIndex, db.hintIndexSize);
				store.hintIndexSize = db.hintIndexSize;
			}
		}

		/*
		 * imprints
		 */

		if (!store.maxImprint) {
			store.imprints = NULL;
			store.imprintIndex = NULL;
		} else {
			if (inheritSections & database_t::ALLOCMASK_IMPRINT) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_IMPRINT);
				store.imprints = db.imprints;
				store.numImprint = db.numImprint;
			} else if (!db.numImprint) {
				// input empty
				assert(store.allocFlags & database_t::ALLOCMASK_IMPRINT);
				store.numImprint = 1;
			} else if (store.maxImprint <= db.numImprint && copyOnWrite) {
				// small enough to use copy-on-write
				assert(~store.allocFlags & database_t::ALLOCMASK_IMPRINT);
				store.imprints = db.imprints;
				store.numImprint = db.numImprint;
			} else {
				fprintf(stderr, "[%s] Copying imprint section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_IMPRINT);
				::memcpy(store.imprints, db.imprints, db.numImprint * sizeof(*db.imprints));
				store.numImprint = db.numImprint;
			}

			if (inheritSections & database_t::ALLOCMASK_IMPRINTINDEX) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_IMPRINTINDEX);
				store.imprintIndexSize = db.imprintIndexSize;
				store.imprintIndex = db.imprintIndex;
			} else if (rebuildSections & database_t::ALLOCMASK_IMPRINTINDEX) {
				// post-processing
				assert(store.allocFlags & database_t::ALLOCMASK_IMPRINTINDEX);
			} else if (!db.imprintIndexSize) {
				// was missing
				assert(store.allocFlags & database_t::ALLOCMASK_IMPRINTINDEX);
				::memset(store.imprintIndex, 0, store.imprintIndexSize);
			} else if (copyOnWrite) {
				// copy-on-write
				assert(store.imprintIndexSize == db.imprintIndexSize);
				assert(~store.allocFlags & database_t::ALLOCMASK_IMPRINTINDEX);
				store.imprintIndex = db.imprintIndex;
				store.imprintIndexSize = db.imprintIndexSize;
			} else {
				// copy
				assert(store.imprintIndexSize == db.imprintIndexSize);
				assert(store.allocFlags & database_t::ALLOCMASK_IMPRINTINDEX);
				::memcpy(store.imprintIndex, db.imprintIndex, db.imprintIndexSize);
				store.imprintIndexSize = db.imprintIndexSize;
			}
		}

		/*
		 * members
		 */

		if (!store.maxMember) {
			store.members = NULL;
			store.memberIndex = NULL;
		} else {
			if (inheritSections & database_t::ALLOCMASK_MEMBER) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_MEMBER);
				store.members = db.members;
				store.numMember = db.numMember;
			} else if (!db.numMember) {
				// input empty
				assert(store.allocFlags & database_t::ALLOCMASK_MEMBER);
				store.numMember = 1;
			} else if (store.maxMember <= db.numMember && copyOnWrite) {
				// small enough to use copy-on-write
				assert(~store.allocFlags & database_t::ALLOCMASK_MEMBER);
				store.members = db.members;
				store.numMember = db.numMember;
			} else {
				fprintf(stderr, "[%s] Copying member section\n", ctx.timeAsString());

				assert(store.allocFlags & database_t::ALLOCMASK_MEMBER);
				::memcpy(store.members, db.members, db.numMember * sizeof(*db.members));
				store.numMember = db.numMember;
			}

			if (inheritSections & database_t::ALLOCMASK_MEMBERINDEX) {
				// inherited. pass-though
				assert(~store.allocFlags & database_t::ALLOCMASK_MEMBERINDEX);
				store.memberIndexSize = db.memberIndexSize;
				store.memberIndex = db.memberIndex;
			} else if (rebuildSections & database_t::ALLOCMASK_MEMBERINDEX) {
				// post-processing
				assert(store.allocFlags & database_t::ALLOCMASK_MEMBERINDEX);
			} else if (!db.memberIndexSize) {
				// was missing
				assert(store.allocFlags & database_t::ALLOCMASK_MEMBERINDEX);
				::memset(store.memberIndex, 0, store.memberIndexSize);
			} else if (copyOnWrite) {
				// copy-on-write
				assert(store.memberIndexSize == db.memberIndexSize);
				assert(~store.allocFlags & database_t::ALLOCMASK_MEMBERINDEX);
				store.memberIndex = db.memberIndex;
				store.memberIndexSize = db.memberIndexSize;
			} else {
				// copy
				assert(store.memberIndexSize == db.memberIndexSize);
				assert(store.allocFlags & database_t::ALLOCMASK_MEMBERINDEX);
				::memcpy(store.memberIndex, db.memberIndex, db.memberIndexSize);
				store.memberIndexSize = db.memberIndexSize;
			}
		}
	}
};

#endif