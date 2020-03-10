---
layout: post
title: "Level-2 normalisation"
date: 2020-03-10 12:41:45
tags: normalisation
---

<!-- @date 2020-03-10 12:41:45 -->

# Level-2 normalisation

With algebra you can do nice things.
It has one flaw, it is full of redundancy.

Take for example the classic expressions `"a*(b+c)"` and `"(a*b)+(a*c)"`.
Although structurally different, both have the same effect.

Some expresssions are trivial, most are not.
For example: `"b?(a==b):(a!=c)"` and `"a!=(c>b)"`.

This redundancy makes the difference between identical (same structure) and similar (same footprint).
 
Expressions modify data. 
They consist of algebraic instructions (cause) to create a result (affect).

Level-1 normalisation focuses on cause (structure), 
level-2 normalisation focuses on effect (footprint).

A footprint is a vector containing the results for all the possible states the inputs can take.
For example, expressions with 9 variables would have a vector with 512 (2^9) outcomes.

# Structure and skin separation

The expressions "a!=(c>b)" and "c!=(a>b)" have identical structure yet different footprints.
The difference is how endpoints are connected to the structure. 
This connection mapping is called a "skin".

The default skin is a "transparent" skin.
Skins are always applied to ordered structures.
A structure is called ordered if the endpoints are assigned in order of the path used to walk through the tree.

# Structure sizes

Examples of 4-node trees:
```
            |                        |                  |       
     +------+------+          +------+------+     +-----+----+
     |      |      |          |      |      |     |     |    |  
   +-+-+  +-+-+  +-+-+      +-+-+  +-+-+    i   +-+-+   2    f
   | | |  | | |  | | |      | | |  | | |        | | |
   a b c  d e f  g h i      a b |  f g h        a b |
                                |                   |
                              +-+-+               +-+-+
                              | | |               | | |
                              c d e               c d e
```

In the right most example, `"2"` is a back reference to the `"cde"` node. 
Although expanding the back-reference will produce a 4-node tree, the storage only has three.
It is therefor considered a 3-node tree. 

# Metrics

The collection of 4-node, 9-endpoint trees is called the `"4n9"` dataset. 
After level-2 normalisation, the complete tree and any fragment of that tree consisting of 4 directly connected nodes
are stored as structures found in the 4n9 collection.

There is also a second collection `"5n9"` used by the detector of the normalisation.
It consists of all possible `"5-node,9-endpoint"' that share simmilar footprints found in `"4n9"`.

 - There are 9! different skins.
 - `4n9` spans 48295088 normalised and ordered structures.
 - `4n9` has 791647 unique footprints
 - `5n9` spans 33212086528 normalised and ordered structures.
 - About 80% of `5n9` can be rewritten in terms of `4n9` structures. 
 
Three and powers of three are reoccurring numbers found in the code. 
Unless specifically motivated many arbitrary choices are based on that observation.
The maximum number of endpoints per tree is one of those.
  
#  Normalising

 - Take a non-normalised structure.
 - Separate into ordered-structure and skin
 - Evaluate the ordered-structure to create its footprint
 - Perform an associative lookup on dataset based on the footprint
 - Search result is a replacement structure and skin
 - Merge both skins
 - Result is replacement structure and merged skins
 
 # Associative index
 
 With a complete associative lookup, the dataset index contains all possible 9! skin variations of the footprints.
 A footprint requires 64 bytes storage implying a total storage for the index of "64*791647*9!" = 18Tbyte
 
 At the other end of the scale the dataset contains a single footprint. 
 For each lookup all 9! skin variations are generated where each variation performs an index search.
 This implies 9! index queries per lookup.
 
 The first optimizes on speed by sacrificing storage. 
 The second optimizes on storage sacrificing speed.
 
 The actual index implementation is a hybrid of both.
 Place the 9! skins into a grid of "row*columns".
 The rows are optimized for speed, the columns optimized for storage.
 
 Given the 9! skins, one implementation could be (1*2*3*4*5) rows and (6*7*8*9) columns.
 For a given lookup, grab the first endpoints of a skin and permute all 120 possibilities.
 For each alternative perform an index search to see if it matches one of the 3024 stored footprints.
 When a index search hits, a match is found and the skin can computed accordingly. 
     
 