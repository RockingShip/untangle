# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [2.2.0] 2020-05-06 00:07:33

Database version "20200506".

This release is the first part/chapter of the project.

 - `gentransform` creates permutations for endpoints and skins
 - `gensignature` searches uniqueness in a given address space
 - `genswap` searches for endpoint symmetry and rewrite instructions for normalisation
 - `genhint` determines structure symmetry and delivers tuning metrics for associative lookups
 - `genmember` searches for construction `building block` structures/layouts.
 - `eval` reference implementation for level-1 to 3 normalisation
 - `selftest` validate assumptions and basic workings
 - `slookup` database frontend for level-4 queries
 - `tlookup` database frontend for transform queries
 
Included but as a separate archive, datasets to easily create the database
 - `4n9.lst` - signatures for `4n9` space
 - `swap-4n9.lst` - endpoint swap data for 4n9
 - `hint-4n9.lst` - `4n9` tuning metrics for `genmember`
 - `member-5n9.lst` - `4n9` members including `5n9-pure` members to replace `4n9-unsafe` members
 
There (might) also be a number of experimental pure (`QnTF-only`) datasets.
A number of tools are still pending for/with higher level normalisations which will be presented in later chapters.    
 
Quick guide to reconstruct the database:

```
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

```
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

## [2.1.0] 2020-04-20 00:51:51

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

## 2.0.0 2020-04-11 16:27:00

This release is to archive the current generator.

For the `pure dataset the generator needs to scan `6n9-pure` and possibly partially `7n9-pure` space.
This requires a next-generation generator wit significantly less duplicates (currently 28%) and higher speed (`6n9-pure` 80 minutes). 

Normalisations:

 1) Algebraic (function grouping)
 2) Dyadic ordering (layout ordering)
 3) Imprints (layout orientation "skins")
 4) Signatures (restructuring)
 
Features: 
- Level 1+2 query `eval`
- Level 3+4 query `slookup`
- Skin/transform query `tlookup`
- Database creation `gentransform` `gensignature` `genmember`
- Build tool `genrestartdata`

[Unreleased]: https://git.rockingship.org/RockingShip/untangle/compare/v2.2.0...HEAD
[2.2.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.1.0...v2.2.0
[2.1.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.0.0...v2.1.0
