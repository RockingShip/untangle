_This might be a good spot to break the loop_

# Untangle (v2.x)

Extract essence of information and store/contain in fractal structures.

This project is also "The making of untangle v2.x". its contents grows in an on-demand basis.

There is a walk-through trail marked by timestamped jsdoc comments, easily identifiable by the `@date` tag.
There are also general annotations found in `jekyll/_posts` directory, also with a date based trail.
Words and their meaning might be cyclic-dependent.

Serial-orientated documentation and historic archives will not be made available before v3.

Documentation needs to be kept simple because it will consume too much time I prefer using to get it operational.

# Mindset

There are three important assumptions you should keep in mind:

## This is a dual system, not a binary or numeric system

Numeric is represented by a word consisting of many bits.
This projects breaks it down to the smallest component and operator, smaller than found in traditional hardware.

Binary implies a kind of enumeration where commonly `true` is considered superior to `false`.
In a dual system there are two opposite/complementary values.
They are identical in every and all aspects, the only way to differentiate them is by comparing them to a reference value.
For this project their values are `zero` and `not-zero`.

## Time is a dimension

Time is used to indicate the flow of information through expressions.
For example in `a+b*c`, the multiply is performed before the addition.
Concepts that change the direction of time like loops or conditionals do not exist and need to be loop-unrolled or expanded first.

## Constants are a function of time

{placeholder}
Constants are a function of time containing all the steps required to determine the value.

# Requirements

*   64-bits CPU architecture
*   SSE4.2 CRC assembler instruction
*   32G-40G physical memory
*   200G SSD storage (expect files upto 24G large)
*   autotools
*   jansson http://www.digip.org/jansson
*   (optionally) AWS instances with StarGridEngine

# Installation

Please read [INSTALL.md](INSTALL.md)

# Versioning

We use [SemVer](http://semver.org/) for versioning. For the versions available, see the [tags on this repository](https://github.com/xyzzy/untangle/tags).

# License

This project is licensed under the GNU General Public License v3 - see the [LICENSE](LICENSE) file for details
