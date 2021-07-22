# Change log

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

```
2021-07-22 22:12:47 Added: `genmember --listunsafe`.
2021-07-22 21:46:16 Changed: metrics.
2021-07-22 21:42:18 Fixed: `genimport`/`genexport`.
2021-07-22 21:37:20 Fixed: Tree compare, structure first then content.
```

## 2021-07-18 21:04:59 [Version 2.7.0]

Import/export used for [https://github.com/RockingShip/untangle-dataset](https://github.com/RockingShip/untangle-dataset)

```
2021-07-18 16:46:16 Changed: Ignore depreciated members more.
2021-07-18 12:22:34 Added: `genimport.cc` and `genport.h`.
2021-07-18 11:19:40 Changed: Moved `opt_saveIndex` to `dbtool_t`.
2021-07-18 10:21:13 Changed: Split selection of generators into `.cc` and `.h` components. No code change.
```

## 2021-07-16 10:18:26 [Version 2.6.0]

Database version "20210708".

Evaluator initial state as database copy-on-write section.
This release is the first approach towards `rewriteData[]`.
Also introduced is the concept of safe members.

 - `beval` evaluate a `baseTree_t` like `eval` for `tinyTree_t`
 - `bexplain` explain how a tree is normalised during construction
 - `gendepreciate` determine which member components are critical
 - `genexport` export a database in as minimalistic text
 - `genrewritedata` construct level-3 normalisation lookup tables
 - `validaterewrite` brute force validation `rewriteData[]`

```
2021-07-16 11:30:14 Changed: Display roots while constructing `bexplain`.
2021-07-16 09:53:10 Added: level4 to `bexplain.cc`.
2021-07-16 01:14:26 Added: `genexport.cc`.
2021-07-15 23:40:44 Changed: database version to 0x20210715.
2021-07-15 23:40:44 Added: Evaluator [copy-on-write] section to database.
2021-07-11 20:55:22 Added: `bexplain.cc`, output needs to be json for GUI presentation.
2021-07-08 23:18:12 Changed: database version to 0x20210708.
2021-07-08 23:15:59 Changed: Q/T/F in `member_t` now pair id's.
2021-07-08 22:18:58 Added: Sid/Mid and Mid/Tid pair section to `database_t` and `dbtool_t`.
2021-07-08 21:08:17 Changed: database version to 0x20210707.
2021-07-08 20:59:12 Changed: `member_t`, Q/T/F now sid/tid pair.
2021-07-08 20:50:30 Removed: `genmember --text=5 (sql)`.
2021-07-08 20:39:06 Changed: `tinyTree_t::initialiseEvaluator()` now static.
2021-07-03 12:23:48 Changed: `slookup --member` display tid's relative to input.
2021-07-03 11:01:13 Changed: member ordering: 1:SAFE, 2:!DEPR, 3:COMP, 4:score, 5:compare
2021-06-28 20:47:11 Added: `gendepreciated`.
2021-06-26 16:58:08 Removed: `genmember --score`.
2021-06-26 16:50:56 Changed: Rewrite all logical `(~X & Y)` into `!(X & Y)`.
2021-06-26 15:31:07 Added: `rewritedata[]`.
2021-06-26 09:33:21 Changed: Renamed `encode()` to `saveString` and `decode()` to `loadString()`.
2021-06-26 09:26:01 Fixed: Component matching in `genmember::findHeadTail()``
2021-06-26 09:05:49 Changed: database version to 0x20210626.
2021-06-26 09:02:52 Changed: added flags and removed flags in `member_t`
2021-06-13 01:01:01 Removed: Legacy QTnF operator mixup (#/!).
2021-06-13 00:07:11 Added: `validaterewrite.cc`.
2021-06-09 11:26:08 Added: `beval.cc` (without --rewrite).
```

## 2021-06-07 22:51:42 [Version 2.4.0]

This release is the third part/chapter of the project.

With the new extended roots, a job scheduler might not be necessary, include on-demand.  
History which belongs to the scheduler is also on-demand.

 - `kextract` extract key from balanced system
 - `kfold` rotate a balanced system through all its keys
 - `ksystem` converting sets of equations to balanced system

Quick guide to extract a key:

```sh
    mkdir tmp
    ./build9bit 9bit.json 9bit.dat         # create system
    ./ksystem sys.dat 9bit.dat             # convert to balanced system
    ./kextract 9bit.k0.dat sys.dat k0      # extract key from system
    ./kfold remain.dat 9bit.k0.dat sys.dat # remove extracted key from system

    # validate correctness
    ./validate bit.json sys.dat     # balanced system
    ./validate bit.json 9bit.k0.dat # key
    ./validate bit.json remain.dat  # remainder
```

```
2021-06-07 22:39:32 Fixed: `ksystem --cascade`.
2021-06-07 17:35:32 Added: `kfold.cc`.
2021-06-05 23:31:23 Added: `invert-9bit.sh`.
2021-06-05 23:11:30 Added: `ksystem`, `kextract`. 
2021-06-04 22:57:59 Added: `kjoin --extend`.
2021-06-04 22:48:23 Added: `kslice --sql`.
2021-06-04 21:39:17 Deleted: `keysId`/`rootsId` are obsolete.
2021-06-03 22:56:37 Changed: Key nodes needs to be QnTF.
2021-06-02 11:06:38 Changed: `baseTree_t::saveFile()` saves nodes in tree walking order.
2021-05-31 22:43:27 Changed: `README.md`.
```

##  2021-05-29 21:19:07 [Version 2.3.0]

`baseTree_t` version "20210613"

This release is the second part/chapter of the project.

 - `basetree.h` contains `baseTree_t`
 - `buildX.cc` builds trees containing different example systems
 - `validateX.h` validations tests for the example systems
 - `genvalidateX.js` creates validation tests for the example systems
 - `kslice.cc` slices a tree into multiple smaller trees
 - `kjoin.cc` combines multiple trees into a larger tree
 - `ksave.cc` export a tree as JSON
 - `kload.cc` import/create a tree from JSON
 - `spongent.cc` spongent, lightweight hash function
 - `validateprefix.cc` validate the endpoint/back-reference prefix logic

Quick guide to construct/test a system:

```sh
    mkdir tmp
    ./buildmd5 md5.json md5.dat       # create system
    ./kslice tmp/md5-%05d.dat md5.dat # slice into smaller trees
    ./kjoin join.dat tmp/md5-*.dat    # collect and join
    ./validate md5.json join.dat      # validate contents
```

```
2021-05-29 21:47:39 Fixed: Disabled `-fno-var-tracking-assignments` for buildX functions.
2021-05-29 00:22:35 Added: `ksave.cc`.
2021-05-28 01:42:06 Added: `kload.cc`.
2021-05-28 00:43:14 Changed: `keyNames`+`rootNames` as `vector<string>`.
2021-05-28 00:36:09 Changed: `getopt_long()` again.
2021-05-22 19:02:50 Added: `valdateprefix.cc`.
2021-05-20 22:26:32 Added: `kjoin.cc`.
2021-05-21 23:27:00 Changed: General consistency.
2021-05-20 22:26:32 Added: `ksplit.cc`.
2021-05-20 16:40:32 Fixed: Tree initialisation, `ostart` and program consistency.
2021-05-19 01:02:46 Changed: Updated `README.md`.
2021-05-17 23:08:33 Fixed: `build... --cascade`.
2021-05-16 22:41:53 Added: `buildaes.cc`.
2021-05-16 22:43:25 Added: `buildspongent.cc`.
2021-05-17 00:07:39 Added: `buildmd5.cc`.
2021-05-16 22:41:53 Added: `builddes.cc`.
2021-05-15 18:53:07 Added: `build9bit.cc`.
2021-05-13 00:47:53 Added: `basetree.h`, `buildtest0.cc` and `validate.cc`.
2021-03-26 18:56:06 Added: Donate button.
2021-03-22 23:04:19 Changed: item order in navbar.
2021-03-21 22:24:11 Added: `moonwalk` theme.
2021-02-24 17:59:14 Changed: Updated `README.md`.
```

## 2020-05-06 00:07:33 [Version 2.2.0]

Database version "20200506".

This release is the first part/chapter of the project.

 - `gentransform` creates permutations for endpoints and skins
 - `gensignature` searches uniqueness in a given address space
 - `genswap` searches for endpoint symmetry and rewrite instructions for normalisation
 - `genhint` determines structure symmetry and delivers tuning metrics for associative lookups
 - `genmember` searches for `building block` for constructing structures/layouts.
 - `eval` reference implementation for level 1 to 3 normalisation
 - `selftest` validate assumptions and basic workings
 - `slookup` database frontend for level 4 queries
 - `tlookup` database frontend for transform queries
 
Included but as a separate archive, datasets to easily create the database
 - `4n9.lst` - signatures for `4n9` space
 - `swap-4n9.lst` - endpoint swap data for 4n9
 - `hint-4n9.lst` - `4n9` tuning metrics for `genmember`
 - `member-5n9.lst` - `4n9` members including `5n9-pure` members to replace `4n9-unsafe` members
 
There (might) also be a number of experimental pure (`QnTF-only`) datasets.
A number of tools are still pending for/with higher level normalisations which will be presented in later chapters.    
 
Quick guide to reconstruct the database:

```sh
    # create initial database
    ./gentransform transform.db

    # load signatures
    ./gensignature transform.db 4 4n9.db --load=4n9.lst --no-generate

    # load swaps
    ./genswap 4n9.db swap-4n9.db --load=swap-4n9.lst --no-generate

    # load hints
    ./genhint swap-4n9.db hint-4n9.db --load=hint-4n9.lst --no-generate

    # load members
    ./genmember hint-4n9.db 5 member-5n9.db --load=member-5n9.lst --no-generate
```

Then throw in a query:

```sh
    ./slookup -D member-5n9.db -i 'ab+ac+&'
```

```
2020-05-05 16:18:36 Delete output when `write()` fails.
2020-05-05 16:18:36 Consistent use of `ctx.fatal()`.
2020-05-05 15:36:30 Improved `genswapContext_t::foundSignatureSwap()`.
2020-05-04 20:52:00 Synced workflow `genhint`,`genswap`.
2020-05-04 22:46:54 Database version 20200506.
2020-05-04 22:46:54 Added `swap_t` to `database_t`
2020-05-03 23:26:20 Added `genswap`.
2020-05-02 22:39:15 Use hints for `genhint --analyse` and `genmember --unsafe`.
2020-05-02 22:28:30 Added `slookup --member`.
2020-05-02 00:46:30 Synced workflow `genhint`,`genmember`,`gensignature`.
2020-05-02 00:42:48 Improved `--text=`.
2020-05-02 00:22:13 Fixed `dbtool_t::sizeDatabaseSections()`.
2020-04-30 00:42:34 Fixed `genhint`,`genmember`,`gensignature`.
2020-04-30 00:09:53 Synced section management.
2020-04-30 00:09:53 Synced `--[no-]saveindex`.
2020-04-29 23:39:27 Synced `readOnlyMode`.
2020-04-29 23:33:58 Added `database_t::sectionToText()`.
2020-04-29 23:27:58 Added database `copy-on-write`.
2020-04-28 00:24:56 Added `genhint --task=sge`.
2020-04-28 00:11:00 Added `dbtool_t` and relocated all related code to it.
2020-04-28 00:07:13 Enabled copy-on-write with `mmap()`.
2020-04-27 14:59:18 Scientific notation for program options.
2020-04-26 22:58:23 Multi-format `gensignature --load=`.
2020-04-26 22:58:23 Added `gensignature --saveinterleave=`.
2020-04-26 23:01:18 Better detection rebuild index `gensignature`.
2020-04-26 22:58:23 Added `gensignature --no-saveindex`.
2020-04-25 23:00:00 Promoted `--ainf` to system flag.
2020-04-25 23:00:00 Added `gensignature --ainf`.
2020-04-24 22:09:32 Added `gensignature --no-sort --truncate`.
2020-04-24 21:56:54 Fixed broken `selftest --metrics`.
2020-04-23 23:09:36 Sync SGE usage.
2020-04-22 20:01:21 Added `selftest` and relocated all related code to it.
2020-04-22 16:54:48 Consistent use of `%u`.
2020-04-22 13:45:53 Consistent use of `uint32_t`.
2020-04-21 22:47:41 Sync workflow between `gensignature`, `genmember` and `genhint`.
2020-04-21 15:38:02 Improved memory usage/detection and database creation.
2020-04-21 11:46:20 Merged `generatorTree_t::restartTick` into `context_t::tick`. (undone)
2020-04-21 10:30:27 Moved `--unsafe` to system flags and enhanced.
2020-04-21 10:19:51 Enhance database with section allocation and inheritance. 
```

## 2020-04-20 00:51:51 [Version 2.1.0]

This release is to mark the change in database version to `"20200419"` and the introduction of hints for interleave and imprint tuning.
 
```
2020-04-20 00:24:45 Added `hint_t` to `database_t`
2020-04-19 00:10:20 Added `genhint`.
2020-04-18 14:01:13 Rename all `QnTF` which are not operator related to `pure`.
2020-04-17 23:16:52 Add versioned memory to `database_t`.
2020-04-17 13:33:03 Disable AVX2.
2020-04-16 22:47:39 Prepare `generatorTree_t` and `genrestartdata` for 7n9.
2020-04-16 22:39:21 Move generator creation to before displaying memory usage.
2020-04-16 21:48:38 AVX2 machine instructions.
2020-04-16 16:36:35 Fixed broken `restartdata[]`.
2020-04-16 12:53:10 Moved/merged system flags to `context_t` so changes have effect after constructors.
2020-04-15 21:53:14 `gensignature` re-create signature index+imprints after sorting
2020-04-15 15:39:56 Dropped `genmember --mode`.
2020-04-15 14:22:47 Separate application and I/O context.
2020-04-15 02:09:54 `eval` now using software version of `crc32` instruction.
2020-04-14 00:54:23 Upgraded `genmember` and made usage simpeler.
2020-04-13 23:02:04 `tinyTree_t::eval()` now using `SSE2` assembler instructions.
2020-04-13 12:18:45 Changed order program arguments.
2020-04-12 23:10:40 Next-generation generator.
```

## 2020-04-11 16:27:00 Version 2.0.0

This release is to archive the current generator.

For the `pure dataset the generator needs to scan `6n9-pure` and possibly partially `7n9-pure` space.
This requires a next-generation generator wit significantly less duplicates (currently 28%) and higher speed (`6n9-pure` 80 minutes). 

Normalisations:

  1. Algebraic (function grouping)
  2. Dyadic ordering (layout ordering)
  3. Imprints (layout orientation "skins")
  4. Signatures (restructuring)
 
Features:

  - Level 1+2 query `eval`
  - Level 3+4 query `slookup`
  - Skin/transform query `tlookup`
  - Database creation `gentransform` `gensignature` `genmember`
  - Build tool `genrestartdata`

[Unreleased]: https://git.rockingship.org/RockingShip/untangle/compare/v2.7.0...HEAD
[Version 2.7.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.6.0...v2.7.0
[Version 2.6.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.4.0...v2.6.0
[Version 2.4.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.3.0...v2.4.0
[Version 2.3.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.2.0...v2.3.0
[Version 2.2.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.1.0...v2.2.0
[Version 2.1.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.0.0...v2.1.0
