---
layout: post
title: "QTF operator"
date: 2020-03-05 12:58:00
tags: core
---

<!-- @date 2020-03-05 12:58:00 -->

Remember... this is a duale system: two opposite values identical in every shape and form.
The only difference is how they compare to a reference value.

Like Yin and Yang.
Both identical in every aspect except they differ as being "white, non-white" or "black, non-black" depending on your reference.

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

## Traditional implementation

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

# Unified operator

The unified operator can be constructed by a combinations of the [ternary operator] and catalyst operator `"NOT"`.
The unified operator is not the ternary operator used in computer science although they do have similarities.
Catalyst is to emphasise that the NOT operator may be required to build the tree but once built it is no longer needed.

The unified operator has two notation forms, the preferred [postfix] notation and [infix] usually intended for documentation.

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

# Level-1 normalisation

(unless explicitly noted, all code and data assumes to be at least level-1 normalised)

Given 3 variables and the value `"false"` each with an optional NOT allows for "8x8x8=512" different operator possibilities.

## Variable substitution

When focusing on variables, many of these possibilities can be be rewritten as substitutions.
For example, `"c?a:b"` can be rewritten as `"a?b:c"` given `"a=>c, b=>a, c=>b"`.

Isolating the base variables and possible substitution mappings gives the following "16*8=128" base possibilities.
The "8" representing all the combinations were `"NOT"` can appear.

| [~]q | [~]t | [~]f
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

## `"~Q ? T : F"` -> `"Q ? F : T"`

If the question is negated, it can be normalised by negating the question and swapping the inputs accordingly.

This halves the collection to 64 possibilities.

## `"false ? T : F"` -> `"F"`

If the question evaluates to zero, then the outcome equals the `"when-false"` input.

This reduces the collection down to 40 possibilities.

## `"Q ? Q : F"` -> `"Q ? ~false : F"`

If `"Q"` evaluates to `"true"` and `"when-true"` equals `"Q"` then `"when-true"` can also be `"NOT-false"`.

This reduces the collection to 34 possibilities.

## `"Q ? ~Q : F"` -> `"Q ? false : F"`

If `"Q"` evaluates to `"true"` and `"when-true"` equals `"NOT-Q"` then `"when-true"` can also be `"false"`.

This reduces the collection to 28 possibilities.

## `"Q ? T : Q"` -> `"Q ? T : false"`

If `"Q"` evaluates to `"false"` and `"when-false"` equals `"Q"` then `"when-false"` can also be `"false"`.

This reduces the collection to 24 possibilities.

## `"Q ? T : ~Q"` -> `"Q ? T : ~false"`

If `"Q"` evaluates to `"false"` and `"when-false"` equals `"NOT-Q"` then `"when-false"` can also be `"NOT-false"`.

This reduces the collection to 20 possibilities.

## `"Q ? F : F"` -> `"F"`

If both the `"when-true"` and `"when-false"` are identical, the question is irrelevant.

This reduces the collection to 16 possibilities.

## `"Q ? T : ~F"` -> `"~(Q ? ~T : F)"`

If the `"when-false"` input is negated you can negate both inputs and output.

This halves the collection to 8 possibilities.

NOTE: if the normalisation would be `"Q ? ~T : F"` -> `"~(Q ? T : ~F)"`,
then there is no possibility to create the self-generating reference value because
`"X?X:~X"` would alternate states resulting in an oscillating and unstable tree.
More annoyingly it will invert the outcome of the `AND/OR/XOR` operators requiring different notation symbols.

# Semi-final normalised collection

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

## `"a ? ~false : false"` -> `"a"`

Equivalent to `"a XOR false"`

## `"a ? false : b"` -> `"b ? ~a : false"`

Equivalent to `"a < b"` -> `"b > a"`

# Final normalised collection

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

# `QnTF` normalisation

"QTF" (`"a?b:c"`) can be rewritten as `"a?~(a?~b:c):c"`.
This implies that with this normalisation any tree can be constructed exclusively of QnTF operators,
with the penalty that storage is less efficient due to extra nodes as side effect of the substitution.

This allows the creation of a tree consisting of a single operator.
Information stored in structures without data.

# Symmetric ordering

todo: needs content

# QnTF implementation

Example implementation of a `QnTF` only tree designed for gcc.

Tree contains the expression `"d AND ((a OR b) > c) > e"`

```
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
