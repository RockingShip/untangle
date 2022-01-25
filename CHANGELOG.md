# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

TODO: no sorting when read-only
TODO: genpattern save/load
TODO: genpattern list

## [Unreleased]

```
2022-01-25 01:23:35 Added: `grouptree_t::flushLayer()` and layers/group are now always up-to-date on exit.
2022-01-24 22:47:56 Changed: Renamed `groupLayer_t::rebuild()` to `grouptree_t::rebuild:layer()`.
2022-01-24 22:35:36 Changed: Proper switch between `expandSignature()/expandMember()`.
2022-01-25 00:49:51 Changed: Orphans must have a gid.
2022-01-24 22:34:01 Changed: Renamed and fixed champion champion.
2022-01-23 22:43:37 Changed: Replaced `groupTree_t::addToGroup()` with `addOldNode()`.
2022-01-22 21:45:50 Added: Under-construction list (groupLayer_t::ucList) preparations.
2022-01-21 10:23:54 Changed: Renamed `grouptree_t::resolveForward()` into `resolveForwards()` (multiple groups).
2022-01-21 10:05:10 Fixed: Use `groupTree_t::rebuild()` only where necessary.
2022-01-20 17:07:41 Changed: Replaced `groupTree_t::power` with `weight`.
2022-01-20 13:52:33 Fixed: Skip single node groups in `groupTree_t::testAndUnlock().`
2022-01-20 12:49:58 Changed: Unified to single `groupTree_t::compare()`.
2022-01-19 20:21:18 Changed: `groupTree_t::addBasicNode()` returns IGNORE/COLLAPSE/SUCCESS and actualises `layer.gid`.
2022-01-19 00:21:32 Fixed: Implemented `allowForward` in `groupTree_t::updateGroup()`. 
2022-01-18 17:09:31 Changed: Disable asserts.
2022-01-17 17:47:26 Fixed: Detect endpoint-collapse in `groupTree_t::`expandSignature()`.
2022-01-17 17:22:36 Fixed: Detect endpoint-collapse in `groupTree_t::`constructSlots()`.
2022-01-17 17:15:09 Changed: Relocate changed iterators in `grouTree_t::addBasicNode()`.
2022-01-16 00:58:02 Changed: Unified collapse-handling in `groupTree_t::addBasicNode()`.
2022-01-14 21:28:01 Changed: Relocate `groupTree_t::expandSignature()` to Cartesian product loop.
2022-01-14 21:17:29 Added: `groupTree_t::testAndUnlock()` to handle weird and rare `resolveForward()` time related ordering. 
2022-01-14 21:15:45 Fixed: `groupTree_t::mergeGroups()` and self-collapse.
2022-01-14 21:14:26 Fixed: Check new node against challenge before adding to group.
2022-01-14 21:03:50 Fixed: Do NOT `resolveForward()` when `leaveOpen`.
2022-01-14 01:20:18 Fixed: Restart position of `updateGroup()`/`resolveForward()`.
2022-01-13 23:58:25 Added: Cyclic loop detect test for `groupTree_t::resolveForward()`.
2022-01-13 23:26:31 Changed: Spacing (no code change).
2022-01-13 23:18:01 Changed: Iterator collapse and consistent response to "collapsed" return values.
2022-01-13 21:47:13 Changed: Added self-collapse detect to `groupTree_t::updateGroup()` and simplified program flow.
2022-01-13 20:13:08 Changed: Enhanced challenge handling, moved focus to `groupTree_t::addToGroup()`.
2022-01-12 15:45:18 Added: `maxDepth` to `groupTreeHeader_t` (version bump).
2022-01-12 15:11:48 Changed: Enhanced `expandSignature()`.
2022-01-11 19:19:42 Fixed: `mergeGroup()`/`resolveForward()`. 
2022-01-11 19:10:32 Fixed: `groupTree_t::loadFile()`.
2022-01-11 19:05:07 Added: Extra debug/tracking info.
2022-01-11 19:01:14 Changed: Improved `validateTree()`.
2022-01-09 20:46:02 Fixed: Stress test feedback.
2022-01-09 20:43:42 Changed: Fine grade depth. 
2022-01-09 12:50:53 Changed: Re-ordering members in `groupNode_t`.
2022-01-09 00:19:22 Changed: Prepare for nodes "under construction", with their gid set to IBIT. 
2022-01-08 15:22:03 Changed: Adding starting position to `resolveForward()`.
2022-01-07 21:15:57 Changed: `groupTree_t::updateGroup()` now using `addToGroup()`.
2022-01-07 17:29:11 Added: `groupTree_t::addToGroup()`.
2022-01-06 15:51:42 Deleted: `groupTree_t::heapNode_t`, experiment failed.
2022-01-05 23:02:15 Changed: Focus active gid to that in `groupTree_t::groupLayer_t`.
2022-01-05 12:47:28 Changed: `groupTree_t::expandMembers()` working.
2022-01-05 01:17:44 Deleted: `groupTree_t::pGidRefCount`.
2022-01-05 01:13:32 Added: `heapNode_t` to store/delay top-level nodes during group creation.
2022-01-04 22:08:38 Changed: `groupTree_t::updateGroup()` and `resolveForward()` now smarter.
2022-01-04 14:31:20 Added: `groupTree_t::oldId` for debugging to track original nodes. 
2022-01-04 14:29:23 Changed: Always call `groupTree_f::resolveForward()` after `mergeGroups()`.
2022-01-04 13:51:51 Fixed: `groupTree_t::mergeGroups()` flood-fill top-level detection.
2022-01-04 13:21:45 Changed: Improved C-iterator change detect.
2022-01-03 18:16:06 Changed: `groupTree_t::mergeGroup()`, smarter range and flood-fill.
2022-01-03 18:02:53 Added: Statistics `groupTree_t::gcount` for number of created groups.
2022-01-03 14:17:33 Changed: `groupTree_t::mergeGroup()` resolves forward references.
2022-01-03 00:04:18 Fixed: Reuse nodes in `groupTree_t::updateGroup()` if unchanged AND no-forwards.
2022-01-02 13:53:46 Fixed: `groupLayer_t::findGid()`.
2022-01-01 01:30:43 Changed: Improved Cartesian product loop.
2022-01-01 00:33:11 Changed: `groupTree_t::mergeGroups()` and `updateGroup()` merge to lowest of lhs/rhs.
2022-01-01 00:28:49 Changed: Relocated `groupTree_t::expandSignature()` loop from Cartesian product loop to after. 
2021-12-31 12:47:41 Fixed: Return and propagate self-collapse for components in `groupTree_t::expandSignature()`. 
2021-12-31 12:45:52 Changed: Spacing and comments.
2021-12-27 14:15:02 Added: `foldZ[]` and `foldNZ[]` to `signature_t`, no database version bump.
2021-12-27 14:13:27 Changed: Implement core `groupTree_t::saveFile()`.
2021-12-27 14:10:07 Changed: Hide `groupTree_t` logging behind `--debug` settings.
2021-12-27 14:04:37 (MAJOR) Fixed: Limit Cartesian-product for NE/XOR, it is introducing combos that that are never
2021-12-25 13:58:12 Changed: Disabled pattern power in `groupTree_t`.
2021-12-25 13:53:38 Changed: Simplified `groupTree_t::addBasicNode()` Cartesian-product inner loop.
2021-12-25 13:46:30 Changed: Moved all layer initialisations to `grouplayer_t::setGid()`.
2021-12-25 00:41:22 Changed: Detect changed slots after calling `groupTree_t::expandSignature()`.
2021-12-25 00:37:31 Changed: Simplified `groupTree_t::resolveForward()`.
2021-12-25 00:35:20 Changed: Dropped concept of open/closed groups.
2021-12-25 00:31:45 Changed: Spacing.
2021-12-25 00:27:51 Changed: Switched to `groupTree_t::expandMember()`, big indentation with no code change.
2021-12-25 00:17:48 Fixed: `group_t::constructSlots()` count how often slots collapsed. 
2021-12-25 00:06:02 Fixed: `groupTree_t::resolveForward()` had broken detection.
2021-12-25 00:02:27 Fixed: `groupTree_t::compare()` now correctly detects outdates nodes.
2021-12-24 23:56:33 Deleted: `AddToCollection()`, `importGroup() and `scrubGroup().
2021-12-24 23:53:24 Changed: Simplified first part and split second part of `groupTree_t::mergeGroups()` into `updateGroup()`.
2021-12-22 21:31:46 Added: Some statistics for `groupTree_t`.
2021-12-22 18:21:02 Changed: Let `groupTree_t::mergeGroups()` start scanning tree at a smarter location.
2021-12-22 18:19:01 Changed: A bit less paranoid.
2021-12-22 18:17:45 Added: Implemented `groupTree_t::applyFolding()` and added `groupLayer`.
2021-12-22 18:12:37 Changed: Let `groupTree_t` sid lookup index be aware or orphaned entries.
2021-12-22 17:56:07 Added: Moved group construction resources to `groupTree_t::groupLayer_t`.
2021-12-21 20:05:30 Changed: Small fixes for group collapses and merging.
2021-12-21 00:55:36 Changed: Relocated `groupTree_t::constructSlots()`,  `expandSignature()`, `mergeGroups()` and `resolveForward()` higher in the source code (no code change).
2021-12-20 23:09:03 Changed: Redesigned group collision detection and merging.
2021-12-20 01:02:21 Added: `groupTree_t::mergeGroups()` and `pGidRefCount[]`.
2021-12-20 00:47:05 Changed: Restart `groupTree_t::addBasicNode()` when C-product iterators change. 
2021-12-18 23:55:16 Changed: database version to 0x20211218.
2021-12-18 23:55:16 Added: Added slot folding outcomes to `signature_t`.
2021-12-17 01:20:43 Added: `groupTree_t::constructSlots()` now group collapsing aware.
2021-12-17 00:45:34 Changed: Simplified `groupTree_t::compare()` as it was too bloated. 
2021-12-16 20:43:07 Added: Lookup index for all sids in `groupTree_t` group lists.
2021-12-16 19:06:41 Changed: Split core of `groupTree_t::addNormaliseNode()` into `addBasicNode()`.
2021-12-16 15:56:48 Fixed: Ordering of slots entries during construction.
2021-12-13 20:25:05 Removed: Change how `groupTree_t::updateGroups()` determines its starting position, thus dropping `oldCount`.
2021-12-13 20:10:39 Fixed: Relocate call to `GroupTree_t::import()` from `groupTree_t::addToCollection()` to its caller. 
2021-12-13 20:10:39 Changed: Relocate call to `GroupTree_t::import()` from `groupTree_t::addToCollection()` to its caller. 
2021-12-12 18:27:28 Removed: Requirement to have at least one `1n9` per `groupTree_t` group list.
2021-12-12 12:45:13 Changed: Redesigned `groupTree_t::saveString()` to make it independent of `1n9` nodes.
```

## 2021-12-10 14:16:35 [Version 2.12.0]

This version greatly focuses on `groupTree_t` where nodes are collections of signature/member based structures.  
Simply put, `baseTree_t` is `1n9-only` (Q/T/F) whereas `groupTree_t` is signature id based.   
It is still incomplete, and the pending changes need a baseline for comparison.  
Structural consistency is operational, structure manipulation still work-in-progress, structural integrity is questionable.   
NOTE: if `geval` fails, add `--depth=0`.  

Unfinished actions:  

 - Minimum `power` based scrubbing.  
 - Enhance `expandMembers()` to instance signature group members creating fractal trees with minimal and fully connecting/overlapping structures.  
 - Enhance `saveString()` to handle sid/slot based nodes complexer than `1n9`.  
 - Delay `importGroup()` as merging of groups plays havoc with list iterators.  
 - Enhance `updateGroup()` to only scan tree changes.  
 - Throttle logging.  
 
```
2021-12-10 13:40:38 Added: `groupTree_t`, more `depth`.
2021-12-09 18:34:11 `groupTree::importGroup()`, Incorrect test for endpoint. 
2021-12-09 18:29:14 Fixed: `groupTree::addNormaliseNode()`, c-product iterator orphan detection.
2021-12-09 17:52:50 Fixed: `groupTree_t::pruneGroup()`, Update slots, orphan forwarding and renamed to `scrubGroup()`. 
2021-12-09 17:50:33 Fixed: `groupTree_t`, Better detection self-collapse.
2021-12-09 17:28:44 Changed: `groupTree_t`, Unroll `MAXSLOTS` by using `numPlaceholder`.
2021-12-09 17:03:02 Changed: `groupTree_t::updateGroups()`, Harden against infinite forward unrolling. 
2021-12-09 16:52:23 Changed: `groupTree_t::validateTree()`, Harden against missing `0n9`/`1n9`.
2021-12-09 16:37:07 Changed: `groupTree_t::compare()`, Take into account group-id forwarding, drop `orphanWorse()` and replace with `findChampion()`. 
2021-12-09 15:41:02 Fixed: `groupTree_t` Versioned memory usage. (2 distinct changed).
2021-12-09 15:34:15 Added: `groupTree_t::versionMemory_t`, Need to keep current version number bound to vector.
2021-12-06 16:48:40 Added: `groupTree_t::pruneGroup()`, Remove weak nodes that have insufficient power.
2021-12-06 16:45:22 Fixed: `groupTree_t::expandSignatures()`, Folding is components refer to self.
2021-12-06 16:41:22 Changed: `groupTree::addNormaliseNode()`, Alternative approach to top-level ordering.
2021-12-06 14:38:24 Added: `groupTree_t::maxDepth`, To restrict node expansions.
2021-12-06 14:13:52 Fixed: `groupTree::addNormaliseNode()`, Guard c-product iterators against jumps to orphans.
2021-12-06 14:09:42 Added: `groupTree_t::expandMembers()`, Controlled sub-structure creation.
2021-12-06 13:55:23 Fixed: `groupTree_t::addToCollection()`, Orphan worst when adding a better node.
2021-12-06 13:50:34 Changed: `groupTree_t::validateTree()`, Test slot uniqueness.
2021-12-06 13:34:39 Fixed: `groupTree::addNormaliseNode()`, Sid-swap top-level in fallback code.
2021-12-06 13:28:12 Added: `groupTree_t::saveStringNode()`, Displaying nodes based on their sid structure. 
2021-12-02 17:42:11 Added: `context_t::DEBUGMASK_PRUNE`, To hide some tracking.
2021-12-03 18:38:59 Changed: `groupTree_t::expandSignature()`, Accept top-level node folding into gid (it is intentional).
2021-12-03 18:34:47 Changed: `groupTree_t::updateGroups()`, As optimisation, supply lowest from which scanning starts.
2021-12-02 14:10:46 Fixed: `groupTree::scrubGroups()`, Harden iterators against `unlinkNode()`.
2021-12-03 18:09:07 Changed: `groupTree::importGroup()`, Make aware of full-collapse and forward references. 
2021-12-03 17:57:38 Fixed: `groupTree::addNormaliseNode()`, Redesigned c-product to better handle changing iterator groups.
2021-12-03 17:54:21 Fixed: `groupTree::addNormaliseNode()`, Sid-swap top-level slots before lookup.
2021-12-02 17:44:30 Fixed: `groupTree_t::updateGroups()`, Fixed incorrect way of relocating nodes to newer group.
2021-12-02 17:42:11 Added: `context_t::DEBUGMASK_CARTESIAN`, To hide some tracking.
2021-12-02 16:34:23 Fixed: groupTree_t::constructSlots()`, Forgot to sid-swap slots before lookup. 
2021-12-02 14:33:55 Changed: `groupTree::addNormaliseNode()`, Better handling of group merging as effect of creating components.
2021-12-02 14:30:01 Changed: `groupTree::addNormaliseNode()`, C-product iterators better detect group changes.
2021-12-02 14:25:19 Added: `groupTree::importGroup()`, Forward reference friendly.
2021-12-02 14:10:46 Added: `groupTree::scrubGroup()`, And renames/simplified `rebuildGroup()` to `updateGroup()`.
2021-12-02 13:21:03 Added: `groupNode_t`, Add power for minimal layer requirements. 
2021-12-02 13:12:38 Changed: `groupTree_t::validateTree()`, Allow forward references.
2021-12-02 12:57:00 Fixed: `groupTree_t`, Allow `gid` to be zero.
2021-12-01 11:54:06 Added: `glookup`, Forgot to display power.
2021-12-01 00:50:02 Changed: `genpattern`, Optimised name construction for `--text`.
2021-11-30 15:25:41 Added: `glookup`.
2021-11-30 15:18:59 Added: `patternSecond_t`, Added `power`.
2021-11-29 18:47:12 (MAJOR) Fixed: `genpattern`, It was an error to convert to `tid=` BEFORE determining tidT+tidF.
2021-11-29 18:43:18 Deleted: `genpattern --fast`, No noticeable speed difference.
2021-11-28 14:02:24 Changed: `genpattern`, Better handling of read-only mode (no output database).
2021-11-28 13:51:36 Added: `genpattern`, Load file from stdin.
2021-11-25 17:58:06 Changed: `groupTree_t::loadStringSafe()`, Modernise.
2021-11-25 17:52:39 (MAJOR) Fixed: `groupTree_t::constructSlots()`, Was mixing nid/sid.
2021-11-25 17:49:42 Changed: `groupTree_t::addNormalisedNode()`, Allow outdated nodes as arguments.
2021-11-25 14:48:25 Changed: Updated metrics.
2021-11-23 22:45:28 Changed: `gendepreciate`, Open database in new copy-on-write fashion.
2021-11-23 22:04:16 Changed: `gensignature`, Generate (fast) list queries before generating indices. 
2021-11-23 21:30:27 Changed: Textual fixups.
2021-11-23 21:26:21 Changed: `genmember`, Open database in new copy-on-write fashion.
2021-11-20 13:59:05 Changed: `groupTree_t::expandSignature()`, Simplified code.
2021-11-20 13:11:04 Fixed: `groupTree_t::addNormalisedNode()`, Allow arguments to reference forwards.
2021-11-20 13:07:51 Changed: `groupTree_t::mergeGroups()`, Redesigned to become `importGroup()`.
2021-11-20 12:57:35 Added: `groupTree_t::verifySignature()`, Enhance `validateTree()`.
2021-11-18 11:59:11 Fixed: `groupTree_t::saveString`, typo in creating `>`.
2021-11-17 22:36:48 Changed: `groupTree_t`, Reposition functions (no code change).
2021-11-16 21:44:09 Added: `groupTree_t::orphanLesser()`, Do more probing as to not add lesser nodes to lists.
2021-11-16 21:06:51 Changed: `groupTree_t`, Be more node/gid aware.
2021-11-16 20:43:38 Changed: `groupTree_t::constructSlots()`, Moved slot construction to a dedicated function.
2021-11-13 15:59:25 Added: `groupTree_t::applySwapping()`, Apply signature based endpoint swapping.
2021-11-13 15:59:25 Added: `groupTree_t::orphanWorse()`, Assist in choosing competing nodes during a group merge.
2021-11-13 15:59:25 Added: `groupTree_t::updateGroups()`, Resolve forward node references.
2021-11-13 15:07:17 Added: `groupTree_t::expandSignature()`, Recreate node by applying/expanding it's signature.
2021-11-11 12:02:35 Added: `groupTree_t::validateTree()`, Trees are starting to get complex. 
2021-11-11 12:02:35 Changed: `groupTree_t::addNormalisedNode()`, Returns node id, which caller should promote to group id. 
2021-11-11 10:35:50 Fixed: `groupTree_t`, Iterator for double linked lists.
2021-11-11 10:25:05 Fixed: `groupTree_t::unlinkNode()`, Oops.
2021-11-10 19:48:56 Added: `groupTree_t::mergeGroups()`, Combine group lists and rebuild references.
2021-11-10 19:48:56 Added: `groupTree_t::compare()`, Compare with an anonymous node (rhs constant).
2021-11-10 19:34:46 Changed: `groupNode_t`, Now with double linked list.
2021-11-09 22:41:32 Changed: `groupTree_t::addToCollection()`, Delete duplicates/similars. NOTE: may cause index overflow.
2021-11-09 22:39:12 Changed: `groupTree_t::addNormalisedNode()`, Ensure group heads point to the latest lists.
2021-11-09 22:35:51 Changed: `groupTree_t::addNormalisedNode()`, Split `T` into `Tu/Ti`.
2021-11-09 22:32:48 Changed: `groupTree_t::lookupNode()`, Handle deleted entries.
2021-11-09 22:25:06 Fixed: `groupTree_t::compare()`, Now more structure aware.
2021-11-08 17:37:30 Added: `groupTree_t::addToCollection()`, Cascading ordering of slots. 
2021-11-08 17:35:21 Added: `groupTree_t::addNormalisedNode()`, Cached lookup.
2021-11-08 17:25:37 Added: `groupTree_t::addNormalisedNode()`, Added parameters `gid` and `depth`. 
2021-11-08 17:20:14 Fixed: `groupTree_t`, Adding records to index.
2021-11-08 17:11:45 Fixed: Sorting tid (slots) after reverse transform.
2021-11-08 17:08:58 Fixed: Buffer overrun in `selftest.cc`.
2021-11-08 16:52:49 Changed: First (database) record starts at IDFIRST.
2021-11-08 16:40:50 Changed: Move `groupTree_t::SID_*` back to `database_t::SID_*`. 
2021-11-07 00:57:27 Changed: trying to get Cartesian product basics working.
2021-11-07 00:54:40 Fixed: `groupTree_t` Cartesian product loop overhead.
2021-11-07 00:52:41 Fixed: `genpattern.cc` keep collapsing structures.
2021-11-07 00:45:21 Added: `groupTree_t::cmpare()`.
2021-11-07 00:42:46 Fixed: `genpattern` recreating indices on startup.
2021-11-07 00:38:56 Changed: Move `database_t::SID_*` to `groupTree_t::SID_*`. 
2021-11-05 14:27:40 Added: `groupTree_t::addNormaliseNode()`.
2021-11-05 14:21:38 Added: Cartesian product slots for adding nodes.
2021-11-05 00:34:45 Fixed: Fallback `geval` with dumb `1n9` nodes working.
2021-11-04 13:26:03 Changed: Renamed `database_t::rawDatabase` to `database_t::rawData`.
2021-11-04 12:36:02 Added: `SID_` id's for all top-level 1n9 signatures.
2021-11-04 12:29:12 Added: Fingerprint database by adding `sidCRC`.
2021-11-03 00:30:50 Added: Skeleton for `groupTree_t` in geval.cc` and `grouptree.h`.
2021-11-02 02:11:22 Serious: `genswap_t` missing entries and added selftest.
2021-11-02 01:30:42 Added: `genpattern --fast` to skip saninty checks during `--load`.
2021-11-02 00:10:59 Changed: Length of `patternSecond_t::idFirst` increased to 27 bits. No version number bump.
2021-11-02 00:02:34 Fixed: Connect database to application as last before main. 
2021-10-26 23:48:02 Fixed: `genpattern` sid based placeholder swapping for `tidR`.
2021-10-26 23:44:54 Changed: Improved rebuilding indices during program start.
2021-10-26 00:34:40 Changed: `genpattern` needs placeholders for inherited copy-paste blocks.
2021-10-26 00:27:50 Changed: Recreate indices (shrink-to-fit) when saving.
2021-10-26 00:07:21 Changed: usage/arguments database generators.
2021-10-25 20:09:46 Fixed: `genpattern` duplicate detection and error handling.
2021-10-25 18:56:25 Changed: Sid/tid handling in `genpattern`.
2021-10-25 18:48:19 Added: Stat counters and `--wildcard` in `genpattern`.
2021-10-24 12:22:04 Deleted: `gensignature --saveinterleave`.
2021-10-24 11:50:28 Changed: Integrate `genswap` into `gensignature`.
2021-10-24 10:38:55 Changed: `genswap` database creation.
2021-10-24 10:38:55 Changed: `gensignature` database creation.
2021-10-24 10:38:55 Changed: `gentransform` database creation.
2021-10-24 01:30:15 Changed: "Enumeration to text" now `std::string`.
2021-10-24 01:09:11 Changed: Active section detection in `database_t::reallocateSections()`.
2021-10-24 00:00:15 Fixed: Region size `mmap()`/`munmap()` must match.
2021-10-22 22:13:12 Added: `genmember` core routines `foundTreePattern()` and `addPatternToDatabase()`.
2021-10-21 21:41:13 Fixed: `dbtool_t::prepareSections()`.
2021-10-21 21:39:50 Added: `tinyTree_t::addStringFast()`.
2021-10-21 21:36:00 Fixed: `patternFirst`/`patternSecond` tweaks.
2021-10-21 21:32:30 Serious: `patternFirst`/`patternSecond` fixes.
2021-10-20 12:35:44 Added: `genpattern` skeleton. 
2021-10-19 23:03:00 Fixed: `dbtool_t::prepareSections()`.
2021-10-18 23:14:20 Added: `dbtool_t::prepareSections()` for replacing opening database as "read-modify-write" with "update/append".  
2021-10-17 21:57:31 Added: `patternFirst_t`/`patternSecond_t` to database (preserving database version).
2021-10-17 19:36:24 Removed: `hint` section (too complex as optimisation).
2021-10-17 18:58:32 Removed: `gensignature --unsafe` (use `--listunsafe`).
```

## 2021-10-12 20:18:09 [Version 2.11.0]

Fully integrated `--cascade` which also disables right-hand-side nesting.  
With focus on `rewriteQTF()` as engine powering SID->QTF conversion.  
Two programs supporting rewriting are currently `beval` and `kfold`.  

TODO: `rewriteData[]` as static data for `rewriteQTF()`.  
TODO: Caching `baseTree_t::compare()` calls.  

```
2021-10-11 22:31:11 Fixed: Root entries output `kfold`.
2021-10-09 22:20:23 Fixed: Processing collapse in `rewriteTree_t::rewriteString()`.
2021-10-08 22:06:42 Added: `beval --explain` as convienience for `beval --debug=4`.
2021-10-08 22:04:00 Changed: Implemented `rewriteTree_t::rewriteQTF()`, slightly buggy.
2021-10-08 15:43:54 Replaced `baseTree_t::addOrderNode()` with `tinyTree_t::cascadeQTF()`.
2021-10-08 15:25:13 Removed: `bexplain` functionality now `beval --debug=4`.
2021-10-08 14:20:03 Changed: Default flags inherited from database.
2021-10-07 23:20:32 Added: `rewriteTree_t` skeleton.
2021-09-27 21:24:33 Fixed: Spacing.
2021-09-26 10:33:11 Version 2.10.1
2021-09-26 10:25:29 Changed: Lost+Found.
2021-09-26 10:21:57 Fixed: Changed: Sort members on tree size plus name.
2021-09-26 10:19:50 Added: `genmember --cascade`.
2021-09-26 10:11:26 Fixed: Re-ordering signatures invalidated member tids.
2021-09-26 10:04:31 Fixed: `tinyTree_t::compare()` caching breaks cascade end condition.
2021-09-26 10:02:24 Fixed: Removed generator cascade optimisation.
2021-09-26 10:01:28 Added: `gendepreciate --cascade`.
2021-09-22 14:33:42 Fixed: Get metrics working.
2021-09-20 21:35:35 Added: `generator_t::GENERATOR_MAXNODES`.
2021-09-20 20:03:25 Added: `getMetricsRestart()`.
2021-09-16 16:37:17 Added: `selftest.cc:performSelfTestCascade()`.
2021-09-16 16:36:07 Removed: `tinyTree_t:similar()`.
2021-09-17 10:32:35 Changed: Merged `tinyTree_t::addBasicNode()` and `tinyTree_t::addNode()`. 
2021-09-16 16:26:44 Changed: Redesigned `tinyTree_t::addOrderNode()` into `tinyTree_t::cascadeQTF()`.
2021-09-16 17:45:33 Fixed: disable/enable cascading in `tinyTree_t`.
2021-09-16 16:21:13 Changed: Renamed `unsigned` to `uint32_t` in `tinytree.h`. No code change.
2021-09-10 22:52:50 Changed: Small fixes.
```

## 2021-08-31 20:30:19 [Version 2.10.0]

Trees/nodes are cascade aware.
Cascades are exclusively left-hand-side.  
Structure names have endpoints/placeholders assigned in tree walking order.    
Cascading side-effect, many orphaned nodes, even for `tinyTree_t`.  
`TinyTree_t` needs larger `MAXNODES` to accommodate orphaned nodes.  
Unexpectedly, `kfold` with `baseExplain` seems to be less optimal.
 
```
2021-08-31 21:34:40 Changed: `kfold` using `baseExplain` again.
2021-08-31 20:25:14 Changed: `kfold` using history to rescan previous keys.
2021-08-31 20:18:14 Fixed: Redesigned `baseTree_t::importNode()` and `baseTree_t::importActive()`.
2021-08-31 20:13:01 Changed: `baseTree_t::saveString()` and `loadString()` in sync with `tinyTree_t`.
2021-08-30 19:22:32 Changed: Increase `TINYTREE_MAXNODES` because of orphaned nodes when expanding cascades.
2021-08-30 19:19:11 Fixed: `genmember_t::findHeadTail()` cascade awareness when extracting heads.
2021-08-30 19:15:22 Fixed: `tinyTree_t::saveString()` intermediate node id's.
2021-08-30 10:50:24 Changed: `generator_t` now left-hand-side cascading aware.
2021-08-30 10:48:15 Changed: Disable `genmember --cascade` as cascading is now more embedded.
2021-08-29 21:57:22 Changed: `tinyTree_t::saveString()` and `loadString()` assign endpoints in tree walking order.
2021-08-30 01:42:40 Changed: Updated `rewritedata.h`.
2021-08-29 21:20:06 Added: re-allocating `buildTree` into `foundTree` in `generator_t`.
2021-08-29 20:57:26 Changed: Renamed `generatorTree_t` to `generator_t`.
2021-08-29 20:53:23 Fixed: `OR/AND` variant `A<C<B=D` in `tinyTree_t` and `baseTree_t`.
2021-08-28 23:24:07 Added: `tinyTree_t::addOrderNode()` and rename friends.
2021-08-28 21:18:29 Changed: `tinyTree_t::compare()` now cascade aware.
2021-08-27 20:59:10 Removed: static and first argument of `baseTree_t::compare()`.
2021-08-27 20:45:50 Changed: Rebase signature `baseTree_t::addOrderNode()`.
2021-08-27 20:35:14 Changed: Rebase signature `baseTree_t::addOrderNode()` and friends. No code change.
2021-08-25 22:32:08 Added: `baseTree_t::addOrderNode()`.
2021-08-23 13:16:53 Changed: Improved caching `baseTree_t::compare()`.
2021-08-23 13:12:21 Added: `explainOrderedNode()` top-level cascade for `compare()`.
2021-08-22 19:42:11 Changed: Make `baseTree_t::compare()` cascade aware.
2021-08-22 19:36:36 Version 2.9.2
2021-08-19 20:22:03 Added: `kfold` using `baseexplain_t`.
2021-08-18 23:44:17 Added: `baseexplain.h` and updated `bexplain`.
2021-08-18 22:08:54 Fixed: Construction of `rwSlots[]` in `explainNormaliseNode()`.
2021-08-18 21:39:12 Fixed: `baseTree_t::countActive()` and `baseTree_t::importActive()`.
2021-08-18 21:37:21 Changed: Increased `eval::NEND` to 1000000.
2021-08-14 10:50:10 Changed: Renamed `To` to `Tu`. No code change.
2021-08-13 23:00:26 Added: Test if node already present before searching sid in `bexplain`.
2021-08-13 22:55:49 Added: `bexplain::explainOrderedNode()` and fixed recursion.
2021-08-11 22:18:57 Added: `expectId` as recursion end condition in `bexplain`.
2021-08-11 20:19:58 Added: `TinyTree_t::isOR(),isGT(),isNE(),isAND()`.
2021-08-09 23:10:58 Version 2.9.1
2021-08-09 15:43:42 Changed: `gendepreciate` default burst.
2021-08-09 15:42:26 Changed: `gendepreciate` secondary ordering of todo list.
2021-08-09 15:41:09 Added: `gensignature` include flags in checkpoints.
2021-08-09 15:39:31 Added: `genmember --cascade`.
2021-08-03 13:34:46 Renamed: `signature_t::SIGMASK_LOCKED` to `signature_t::SIGMASK_KEY`.
```

## 2021-08-08 11:29:20 [Version 2.9.0]

Focus on tuning the generators and creating safe datasets.  
Dataset areas `full`, `mixed` and `pure`.  
Used to create the dataset version `20210807` dataset [https://rockingship.github.io/untangle-dataset/README.html](https://rockingship.github.io/untangle-dataset/README.html)  

```
2021-08-08 11:23:30 Changed: Split `gendepreciate` into `.cc/.h. No code change.
2021-08-08 11:18:04 Added: `gendepreciate` restarts every 10 minutes to shrink members section.
2021-08-08 11:16:36 Added: `gendepreciate --lookupsafe`.
2021-08-08 11:11:15 Added: Save/load flags with signature names.
2021-08-06 16:13:38 (MAJOR) Fixed: `gendepreciate` heap.
2021-08-06 16:04:52 Fixed: `--mixed` and `--markmixed`.
2021-08-04 16:46:24 Added: `gensignature --listused`.
2021-08-06 15:15:10 Added: `genmember --mixed` and `--safe`.
2021-08-06 15:15:10 Added: `genmember --listlookup`.
2021-08-06 14:56:55 Renamed: `signature_t::SIGMASK_REWRITE` to `SIGMASK_LOOKUP`.
2021-08-04 16:46:24 Added: `gensignature --listincomplete`.
2021-08-04 15:50:36 Added: `gensignature --markmixed`.
2021-08-03 21:50:57 Renamed: `selftest` `Qo/To/Fo` to `Qu/Tu/Fu`.
2021-08-03 21:47:46 Added: `signature_t::SIGMASK_REWRITE` to mark rewrite lookups.
2021-08-03 13:34:46 Renamed: `pair_t::sidmid` to `id`.
2021-08-02 14:13:43 Fixed: Add pairs as very last in `findHeadTail()`.
2021-08-02 13:23:55 Added: `gensignature --mixed=2`.
2021-08-02 13:21:21 Added: Alternative member generator for 7n9 (Experimental).
2021-08-02 13:19:09 Fixed: `genmember --truncate`.
2021-08-01 21:44:12 Changed: Simplified `numEmpty`/`numUnsafe`.
2021-08-01 17:51:27 (MAJOR) Fixed: `genmember` `numPlaceholder` false-positives.
2021-08-01 17:48:47 Added: `gensignature --mixed`.
2021-08-01 17:26:47 Fixed: Allow mixing of `gensignature`/`genmember`.
2021-07-29 20:37:14 Changed: Signatures are always sorted.
2021-07-27 20:24:31 Changed: Cleanup in `genmember`/gendepreciate`.
2021-07-27 20:21:10 Fixed: Nasty typo in `testStringSafe()`.
2021-07-26 22:26:31 Changed: Database record offsets are `uint32_t`.
2021-07-26 12:29:00 Changed: Reject signature swapped alternatives.
2021-07-26 12:14:46 Added: Checks to `finaliseMembers()`.
2021-07-26 12:02:36 Renamed: `genmember::pSafeScore[]` to `pSafeSize`.
2021-07-26 12:01:02 Changed: Moved `--listsafe`/`--listunsafe` to `gensignature`.
2021-07-26 11:49:39 Fixed: Capture flags in `database_t::create()`.
2021-07-26 11:42:17 Changed: Tickers.
2021-07-26 11:40:10 Fixed: Create pairs only when in write mode.
2021-07-26 11:32:40 (CRITICAL) Fixed: member matching in `findHeadTail()`.
2021-07-26 11:30:05 Changed: `lookupImprintAssociative()` with explicit root.
2021-07-26 11:01:56 (CRITICAL) Fixed: Ambiguous `tinyTree_t::saveString()`.
2021-07-25 22:59:28 Fixed: Generator pre-calculated iterator dyadic ordering.
```

## 2021-07-23 20:33:36 [Version 2.8.0]

Extra application/tree samples.  
Used to create the initial version `20210723` dataset [https://rockingship.github.io/untangle-dataset/README.html](https://rockingship.github.io/untangle-dataset/README.html)  

```
2021-07-23 18:00:57 Fixed: Disable `tinyTree_t::compare()` assertion as it triggered a minor historic generator issue.
2021-07-23 17:54:13 Changed: Data listings `--text=3/4` in read-only mode.
2021-07-23 09:25:14 Fixed: Typos and cosmetics.
2021-07-22 23:22:08 Added: `build9bitAdder.cc`.
2021-07-22 22:34:15 Added: `build7bitCount.cc`.
2021-07-22 22:30:29 Changed: `gensignature` `--pure` signatures with mixed top-level.
2021-07-22 22:26:37 Fixed: `genmember` compress before sorting.
2021-07-22 22:17:25 Fixed: `genmember` Erasing signature groups.
2021-07-22 22:13:41 Fixed: `genmember` and `6n9-pure`.
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

[Unreleased]: https://git.rockingship.org/RockingShip/untangle/compare/v2.12.0...HEAD
[Version 2.12.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.11.0...v2.12.0
[Version 2.11.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.10.0...v2.11.0
[Version 2.10.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.9.0...v2.10.0
[Version 2.9.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.8.0...v2.9.0
[Version 2.8.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.7.0...v2.8.0
[Version 2.7.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.6.0...v2.7.0
[Version 2.6.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.4.0...v2.6.0
[Version 2.4.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.3.0...v2.4.0
[Version 2.3.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.2.0...v2.3.0
[Version 2.2.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.1.0...v2.2.0
[Version 2.1.0]: https://git.rockingship.org/RockingShip/untangle/compare/v2.0.0...v2.1.0
