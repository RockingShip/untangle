# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](http://keepachangelog.com/en/1.0.0/)
and this project adheres to [Semantic Versioning](http://semver.org/spec/v2.0.0.html).

## [Unreleased]

```
2020-04-15 02:09:54 `eval` now using software version of `crc32` instruction
2020-04-14 00:54:23 Upgraded `genmember` and made usage simpeler.
2020-04-13 23:02:04 `tinyTree_t::eval()` now using `SSE2` assembler instructions.
2020-04-13 12:18:45 Changed order program arguments.
2020-04-12 23:10:40 Next-generation generator.
```

## 2.0.0 2020-04-11 16:27:00

This release is to archive the current generator.

For the `QnTF-only` dataset the generator needs to scan `6n9-QnTF` and possibly partially `7n9-QnTF` space.
This requires a next-generation generator wit significantly less duplicates (currently 28%) and higher speed (`6n9-QnTF` 80 minutes). 

 
Normalisations:

 1) Algebraic (function grouping)
 2) Dyadic ordering (layout ordering)
 3) Imprints (layout orientation "skins")
 4) Signature groups (restructuring)
 
Features: 
- Level 1+2 query `eval`
- Level 3+4 query `slookup`
- Skin/transform query `tlookup`
- Database creation `gentransform` `gensignature` `genmember`
- Build tool `genrestartdata`

[Unreleased]: https://git.rockingship.org/RockingShip/untangle/compare/v2.0.0...HEAD
