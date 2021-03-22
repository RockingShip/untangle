---
layout: home
title: "untangle - Extract essence of information and store in fractal structures"
image: assets/favimage-472x472.jpg
---

\[click on image to start the interactive model, NOTE: CPU intensive\]  
[![teaser](assets/favimage-472x472.jpg)](https://rockingship.github.io/untangle-media/shrimp.html)  
\[3D wireframe model of a fractal structure containing zero (yellow), 9 inputs (blue) and 9 outputs (green)\]

# untangle (v2)

Extract essence of information and store/contain in fractal structures

Content grows on-demand as parts of v1 get merged, currently at about 20%

### Welcome to the Wonderful World of fractal logic and computing

_This might be a good spot to break the loop_

All our logic operators share a common component.  
Better worded, all our logic operators are derived from a single operator: the fractal switch.

The fractal world is all about inheritance and transformation.  
Binary values are transformations of a shared reference value: null.

Because there is just a single operator and single value, both can be removed from the equation/system.  
What's left is structure in the form of information and connection.

These structures are self-modifying and `untangle` is their containment field.  
`untangle` isolates and extracts the essence of information.

What `untangle` does is:

  - transform logic expressions into fractal structures
  - let structures digest information
  - solve equations by extracting information

An illustration of some of the many things this project uncovered:
With 9 variables and 4 fractal operators it is possible to construct 1.206e13 different structures.  
After removing all transformations and synonyms, 791647 unique structures remain.
binary-operators used in algebra can describe about 0.1% of them.

### Table of contents

  - [Welcome to the Wonderful World of fractal logic and computing](#welcome-to-the-wonderful-world-of-fractal-logic-and-computing)
    - [Table of contents](#table-of-contents)
    - [Time trail](#time-trail)
  - [QTF operator](#qtf-operator)
    - [Traditional implementation](#traditional-implementation)
    - [Unified operator](#unified-operator)
  - Normalisation(#normalisation)
    - [Level-1 normalisation](#level-1-normalisation)
    - [Level-2 normalisation](#level-2-normalisation)
  - [Requirements](#requirements)
  - [Building and Installation](#building-and-installation)
  - [Versioning](#versioning)
  - [License](#license)

### Time trail

Untangle is a large project developed over many years.  
The previous version (v1.50) is a working small-scale prototype.  
Feedback revealed a fundamental assumption being invalid.  
About choosing a representative structure/expression from a group of synonyms.  
Smaller is not always better and sometimes even makes it worse.

Version 2 is a rewrite and possibly the first large-scale pre-production platform.  
Version 1.50 will be imported in parts and rewritten with the new paradigm in mind.  
Attention goes all over the place in a non-linear fashion.  
To aid future documentation, a trail of "@date" tokens are used as markers.  
Names, descriptions and comments may refer to future explanations.

### Duality

*It's not a bird, it's not a plane, it's a duale value*

Binary values ("0", "1") imply an ordering. Starting with zero and limited by its base (2).

Logical values ("false", "true") imply an absolute reference.

Duale values ("reference", "NOT-reference") are transformations of a shared reference value.

Just like Yin and Yang.  
Both identical in every aspect except they differ as being "white, not-white" or "black, not-black" or anything else depending on your reference.

### Time is a dimension

Time is used to indicate the flow of information through expressions.  
For example with `a+b*c`, the multiply is performed before the addition.  
Time flows from `*` to `+`.  
Concepts that change the direction of time like loops or conditionals do not exist and need to be loop-unrolled or expanded first.

## Constants are a function of time

Constant as a concept changes meaning.  
They are the result of an expression based an expression  fixed transformation of null.

Within the structures they form dead code and dissipate, shrinking the structure size.
Thay are steps of dead code which dissipates out of the structures.

{placeholder}
Constants are a function of time containing all the steps required to determine the value.

## QTF operator

<!-- @date 2020-03-05 12:58:00 -->

# Background

Information can be described by formulas.
Formulas can be rewritten into expressions.
Expressions can be stored in trees.
Trees are structures consisting of nodes.
Nodes are representations of numeric operators.
Numeric operators can be rewritten into machine instructions.
Numeric machine instructions are networks of AND/OR/XOR gates.
Gates are implemented in silicone.

Nodes can be evaluated by passing electrons through different types of gates.
Information can be extracted from trees by evaluating the nodes.

Information can be rewritten into terms of AND/OR/XOR.
Information is stored as structure (trees) and data (choice of node).
Holy Grail is to unify AND/OR/XOR into a single entity.

With an unified operator information is stored exclusively as structure.
Without data evaluation of information becomes magnitudes faster.
Restructuring structure only trees opens interesting possibilities.

### Traditional implementation

```
struct Node {
    enum { AND, OR, XOR } t;   // type of operation this node should perform (data)
    struct Node *left, *right; // the left/right arguments (structure)

    // evaluate node by evaluating the children and applying the configured operation
    // this is effectively bytecode evaluation and highly inefficient
    unsigned evaluate() {
        switch (t) {
            case AND:
                return left->evaluate() & right->evaluate();
            case OR:
                return left->evaluate() | right->evaluate();
            case XOR:
                return left->evaluate() ^ right->evaluate();
        }
    }
};
```

### Unified operator

The unified operator can be constructed by a combinations of the \[ternary operator\] and catalyst operator `"NOT"`.
The unified operator is not the ternary operator used in computer science although they do have similarities.
Catalyst is to emphasise that the NOT operator may be required to build the tree but once built it is no longer needed.

The unified operator has two notation forms, the preferred \[postfix\] notation and \[infix\] usually intended for documentation.

The general infix notation for the unified operator is `"[~]q ? [~]t : [~]f"`
where `"~"` is the `"NOT"` operator, the square brackets indicating it being optional.

NOTE: `"~"` is used instead of `"!"` because within programming languages the latter is used with single operand
whereas the first can be used with vectoring.

Given 3 variables `"q"` (for question), `"t"` (for when true) and `"f"` (for when false),
the outcome of the operator is `"[~]t" if "[~]q"` evaluates to `"true"`,
otherwise to `"[~]f" if "[~]q"` evaluates to `"false"`.

`"true"` and `"false"` are terms of logic.
In the duale system, `"false"` is equivalent to the outcome of `"x ? ~x : x"` and `"true"` is equivalent to `"NOT-false"`.
`"x"` can represent anything, like false, NULL, 0, 10, even Schrödinger's cat.

## Normalisation

In total there are 6 normalisation levels and the project that needed these fractal structures.
 - single node rewrites
 - multiple node rewrites
 - structure substitution

### Level-1 normalisation

(unless explicitly noted, all code and data assumes to be at least level-1 normalised)

Given 3 variables and the value `"false"` each with an optional NOT allows for "8x8x8=512" different operator possibilities.

#### Variable substitution

When focusing on variables, many of these possibilities can be be rewritten as substitutions.
For example, `"c?a:b"` can be rewritten as `"a?b:c"` given `"a=>c, b=>a, c=>b"`.

Isolating the base variables and possible substitution mappings gives the following "16*8=128" base possibilities.
The "8" representing all the combinations were `"NOT"` can appear.

| \[~\]q | \[~\]t | \[~\]f
|:----:|:----:|:---:|
| false | false | false
| false | false | a
| false | false | b
| false | a | false
| false | a | a
| false | a | b
| a | false | false
| a | false | a
| a | false | b
| a | a | false
| a | a | a
| a | a | b
| a | b | false
| a | b | a
| a | b | b
| a | b | c

 -  `"~Q ? T : F"` -> `"Q ? F : T"`

    If the question is negated, it can be normalised by negating the question and swapping the inputs accordingly.

    This halves the collection to 64 possibilities.

 -  `"false ? T : F"` -> `"F"`

    If the question evaluates to zero, then the outcome equals the `"when-false"` input.

    This reduces the collection down to 40 possibilities.

 - `"Q ? Q : F"` -> `"Q ? ~false : F"`

    If `"Q"` evaluates to `"true"` and `"when-true"` equals `"Q"` then `"when-true"` can also be `"NOT-false"`.

    This reduces the collection to 34 possibilities.

 -  `"Q ? ~Q : F"` -> `"Q ? false : F"`

    If `"Q"` evaluates to `"true"` and `"when-true"` equals `"NOT-Q"` then `"when-true"` can also be `"false"`.

    This reduces the collection to 28 possibilities.

 -  `"Q ? T : Q"` -> `"Q ? T : false"`

    If `"Q"` evaluates to `"false"` and `"when-false"` equals `"Q"` then `"when-false"` can also be `"false"`.

    This reduces the collection to 24 possibilities.

 -  `"Q ? T : ~Q"` -> `"Q ? T : ~false"`

    If `"Q"` evaluates to `"false"` and `"when-false"` equals `"NOT-Q"` then `"when-false"` can also be `"NOT-false"`.

    This reduces the collection to 20 possibilities.

 -  `"Q ? F : F"` -> `"F"`

    If both the `"when-true"` and `"when-false"` are identical, the question is irrelevant.

    This reduces the collection to 16 possibilities.

 -  `"Q ? T : ~F"` -> `"~(Q ? ~T : F)"`

    If the `"when-false"` input is negated you can negate both inputs and output.

    This halves the collection to 8 possibilities.

  NOTE: if the normalisation would be `"Q ? ~T : F"` -> `"~(Q ? T : ~F)"`,
  then there is no possibility to create the self-generating reference value because
  `"X?X:~X"` would alternate states resulting in an oscillating and unstable tree.
  More annoyingly it will invert the outcome of the `AND/OR/XOR` operators requiring different notation symbols.

#### Semi-final normalised collection

(for readability 0=false)

| Infix              | a=0,b=0 | a=0,b=~0 | a=~0,b=0 | a=~0,b=~0 | Operator
|:-------------------|:-------:|:--------:|:--------:|:---------:|:--------:
| a ?  false : b     |  false | ~false |  false |  false | LT
| a ? ~false : false |  false |  false | ~false | ~false | a
| a ? ~false : b     |  false | ~false | ~false | ~false | OR
| a ?  b : false     |  false |  false |  false | ~false | AND
| a ? ~b : false     |  false |  false | ~false |  false | GT
| a ? ~b : b         | false  | ~false | ~false |  false | XOR
| a ?  b : c         |        |        |        |        | QTF
| a ? ~b : c         |        |        |        |        | QnTF

 -  `"a ? ~false : false"` -> `"a"`

    Equivalent to `"a XOR false"`

 -  `"a ? false : b"` -> `"b ? ~a : false"`

    Equivalent to `"a < b"` -> `"b > a"`

#### Final normalised collection

(for readability 0=false)

| Infix          | Operator | postfix symbol
|:---------------|:--------:|:--------------:
| a ? ~false : b |   OR     | `"+"`
| a ?  b : false |   AND    | `"&"`
| a ? ~b : false |   GT     | `">"`
| a ? ~b : b     |   XOR    | `"^"`
| a ?  b : c     |   QTF    | `"?"`
| a ? ~b : c     |   QnTF   | `"!"`

NOTE: `"|"` is not used as symbol for `"OR"` because of the visual ambiguity.

NOTE: `"XOR"` is also considered `"NOT-EQUAL"`.

#### `QnTF` normalisation

"QTF" (`"a?b:c"`) can be rewritten as `"a?~(a?~b:c):c"`.
This implies that with this normalisation any tree can be constructed exclusively of QnTF operators,
with the penalty that storage is less efficient due to extra nodes as side effect of the substitution.

This allows the creation of a tree consisting of a single operator.
Information stored in structures without data.

#### Symmetric ordering

todo: needs content

#### QnTF implementation

Example implementation of a `QnTF` only tree designed for gcc.

Tree contains the expression `"d AND ((a OR b) > c) > e"`

```C
({ unsigned _[] = {
// reference value for "false"
0U,
// input variables
a,b,c,d,e,
// expression
_[1] ? !_[0]: _[2],
_[6] ? !_[3]: _[0],
_[4] ? !_[7]: _[0],
_[4] ? !_[8]: _[0],
_[9] ? !_[5]: _[0]};
// result
 _[10];
})
```

[infix]: https://en.wikipedia.org/wiki/Infix_notation
[Yin and yang]: https://en.wikipedia.org/wiki/Yin_and_yang
[postfix]: https://en.wikipedia.org/wiki/Reverse_Polish_notation
[ternary operator]: https://en.wikipedia.org/wiki/Ternary_operation#Computer_science

### Level-2 normalisation

@date 2021-02-24 23:19:10

write larger `5n9` structures in terms of `4n9` or smaller.

<!-- @date 2020-03-10 12:41:45 -->

With algebra you can do nice things.
It has one flaw, it is full of redundancy.

Take for example the classic expressions `"a*(b+c)"` and `"(a*b)+(a*c)"`.
Although structurally different, both have the same effect.

Some expresssions are trivial, most are not.
For example: `"b?(a==b):(a!=c)"` is a synonym of `"a!=(c>b)"`.

This redundancy makes the difference between identical (same structure) and similar (same footprint).

Expressions modify data.  (input values -> expression nodes -> output values).
They consist of algebraic instructions (cause) to create a result (affect).

Level-1 normalisation focuses on cause (structure),
level-2 normalisation focuses on effect (footprint).

A footprint is a vector containing the results for all the possible states the inputs can take.
For example, expressions with 9 variables would have a vector with 512 (2^9) outcomes.

#### Structure and skin separation

The expressions "a!=(c>b)" and "c!=(a>b)" have identical structure yet different footprints.
The difference is how endpoints are connected to the structure.
This connection mapping is called a "skin".

The default skin is a "transparent" skin.
Skins are always applied to ordered structures.
A structure is called ordered if the endpoints are assigned in order of the path used to walk through the tree.

##### Structure sizes

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
It is therefore considered a 3-node tree.

#### Metrics

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

Three and powers of three are reoccurring numbers found in observations.
Unless specifically motivated, many arbitrary choices in the code are based on that.
Selecting `4n9` as base collection for having 9 endpoints per tree is one of those.

#####  Normalising

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

## Requirements

*   64-bits CPU architecture.
*   SSE4.2 CRC assembler instruction.
*   32G physical memory, 64G when developing.
*   200G SSD storage. Expect databases of 24G and files upto 60G when developing.
*   autotools.
*   JSON with jansson. [http://www.digip.org/jansson](http://www.digip.org/jansson).
*   Sun Grid Engine [https://arc.liv.ac.uk/trac/SGE](https://arc.liv.ac.uk/trac/SGE). (optionally).
*   AWS instances with Star Cluster [http://star.mit.edu/cluster](http://star.mit.edu/cluster). (optionally).

## Building and Installation

Please read [BUILD.md](BUILD.md)

## Source code

Grab one of the tarballs at [https://github.com/RockingShip/untangle/releases](https://github.com/RockingShip/untangle/releases) or checkout the latest code:

```sh
  git clone https://github.com/RockingShip/untangle.git
```

## Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/RockingShip/untangle/tags).

## License

This project is licensed under the GNU General Public License v3 - see the [LICENSE.txt](LICENSE.txt) file for details
