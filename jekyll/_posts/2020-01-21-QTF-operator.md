---
layout: post
title: "QTF operator"
date: 2020-01-21 03:22:00
tags: core
---

# Background

This project is basically the creation, evaluation and manipulation of dynamic expressions stored in trees. 

In its most simplest form the tree is bit-oriented and the operator can be one of the three bitwise operators available
in hardware: `AND`, `OR`, and `XOR`.

A typical C++ implementation would be:

```
struct Node {
    enum { AND, OR, XOR } t;   // type of operation this node should perform
    struct Node *left, *right; // the left/right arguments

    // evaluate node by evaluating the children and applying the configured operation
    unsigned evaluate() {
        switch (t) {
            case AND:
                return left->evaluate() & right->evaluate();
            case OR:
                return left->evaluate() | right->evaluate();
            case XOR:
                return left->evaluate() ^ right->evaluate();
        }
    };
};
```
There are a few issues with this. 
Extra storage for `t` is required to mark the operation of the node.
Evaluatuion is bytecode based which is highly inefficient because the overhead is magnitudes more than the final operation.

Holy grail is to find a single (machine) operator that can achieve the same result but without the use of an embedded bytecode.  