---
layout: default
title: "Glossary"
image: assets/favimage-472x472.jpg
date: 2020-01-20 21:59:36
---

# Glossary

Collection of used terms and codewords.

* `4n9` / `5n9`

    General form being `<#nodes>`n`<#endpoints>`
     
    Dataset of micro-fractals consisting of max 4 `QTF` operators and 9 endpoints. 

* algebraic address space

    The diversity structures can take. 
    Non-normalised address space is the theoretical variation any given structure can have.
    Given 9 variables and their inverts
    
    0n9 spans zero, the 9 variables and their inverts. resulting in 20 "coordinates"
    
    1n9 is the node with 3 operands, each can take 20 forms, resulting in 20*20*20=8000 coordinates
    
    2n9 has two nodes, the second can also reference the first (and its invert) in addition to zero and variables.
    However, at least one operand must reference the first node.

              +- zero and 9 endpoints for QTF and QnTF [(1+9)*2]
              |   +-- triple for the Q/T/F components
              |   |     +- N[10] must be referenced, for QTF and QTnF
              |   |     |     +-- zero, 9 endpoints and N[10] [(1+9+1)*2]
              | --+--   |     |
    `2n9` = (20*20*20)*(2*22*22) = 7744000.
            ----+----- ----+----
                |          |
                |          +-- number of possibilities for the second node `N[11]`
                +-- number of possibilities for the first node `N[10]`

    `3n9` = (20*20*20)*(2*22*22)*(2*24*24) = 8.921e9

    `4n9` = (20*20*20)*(2*22*22)*(2*24*24)*(2*24*24)*(2*26*26) = 1.206e13

    `5n9` = (20*20*20)*(2*22*22)*(2*24*24)*(2*24*24)*(2*26*26)*(2*28*28) = 1.891e16
    
    `untangle` is designed to work in `5n9` space.
    
* back reference

* candidate

* collapse
   Structural collapse, most likely because a node folded resulting in a smaller structure.
    
* component 

* coordinate

   The collection of all possible structure, notations and names is called the address space.
   An element of that set is called a coordinate.
   
   There are two address spaces, algebraic and fractal. 
   Both use a different coordinate system with unique properties and abilities.
   Each structure has an unique coordiante in both spaces.
   
   Higher normalisations switch between spaces to benefit from both worlds.  

* endpoint

    The leaf nodes of expressions trees.
    They are placeholder representing input variables or (more likely) the roots of other trees.
    The number of endpoints are the physical number of endpoints a tree can have. 
    The number of endpoints equals `"<number-of-nodes>*2+1"`.

* fold

  "Similar to "constant folding". 
   
* footprint

    Bit vector containing the results of the micro-fractal evaluations for all possible endpoint values.
    "The result".

* fractal space

    Fractal space best described as graph and icon based.
    There are 791646 different icons with 9 inputs each.
    To handle the vast amount of data interference patterns are created between icons creating Moire patterns. 
    Information is located/manipulated in these patterns and stored by the icons creating them.   

* heads

    Group of nodes nearest to root.

* imprint
 
    Extension of `footprint` including all possible endpoint permutations.

* interleave 

* kstart / istart / ostart / istart / nstart
    
    Tree indices indicating the start of variable groups with specific higher-level semantics.
    Variables are special kind of endpoints and best compared with tree leaves.
    
    NOTE: within the generator programs, endpoints generally refer to variables whereas within
          solver programs, endpoints generally refer to other trees.  
    
    The values are constrained to `kstart` <= `istart` <= `ostart` <= `ustart` <= `nstart` 
    
    The range `"kstart <= x <= nstart"` is considered input which is excluded when evaluated.
    Node evaluation start from `nstart`.
    Evaluation results are generally stored in the output vector `"roots[]"`
    
    Historically `K` stands for Key, `I` for input, `O` for output, `U` for user-defined and `N` for nodes.

* member

* normalised (algebraic) address space

    Normalised address space is a subset of algebraic address space.
    
    The normalising properties are:
    
    - The `Q` and `F` component are never inverted.
    - The three operands are unique, the only exception being `XOR` where the `T` component equals the inverted `F` component.
    - Endpoints are assigned in tree walking order.

    The mapping between non-normalised and normalised space can be achieved by level 1-3 normalisation and skins.
    
    Normalised 4n9 space can be divided into the following table.
    Each cell represents the number of normalised structures for each structure size in nodes (rows) and variables/endpoints (columns). 
    
    | space  |\*n0 |\*n1 |  \*n2  |  \*n3   |   \*n4   |  \*n5   |  \*n6   |  \*n7  | \*n8  | \*n9
    |:------:|:---:|:---:|:------:|:-------:|:--------:|:-------:|:-------:|:------:|:-----:|:---:|
    | 0n\*   |  1  |  1  |      0 |       0 |        0 |       0 |       0 |      0 |     0 |   0
    | 1n\*   |  0  |  0  |      4 |       2 |        0 |       0 |       0 |      0 |     0 |   0
    | 2n\*   |  0  |  0  |     88 |     218 |      106 |      12 |       0 |      0 |     0 |   0
    | 3n\*   |  0  |  0  |   4723 |   26723 |    32820 |   13443 |    2029 |     96 |     0 |   0
    | 4n\*   |  0  |  0  | 486739 | 4902941 | 10841464 | 8601839 | 2962840 | 474153 | 34134 | 880

    Mapping back to non-normalised algebraic space requires that each cell be multiplied by the number of available skins for the given number of placeholders.
    The applied factor is shown in the following table:

    |        |  0  |  1  |   2  |    3    |   4  |   5   |   6   |    7   |    8    |   9
    |:------:|:---:|:---:|:----:|:-------:|:----:|:-----:|:-----:|:------:|:-------:|:---:|
    | skins  |  1  |  9  | 9\*8 | 9\*8\*7 | 3024 | 15120 | 60480 | 181440 | 362880 | 362880

    resulting in:

     | space  |\*n0 |\*n1 |    \*n2  |    \*n3    |    \*n4    |     \*n5     |     \*n6     |    \*n7     |    \*n8     |    \*n9
     |:------:|:---:|:---:|:--------:|:----------:|:----------:|:------------:|:------------:|:-----------:|:-----------:|:---------:|
     | 0n\*   |  1  |  9  |        0 |          0 |          0 |            0 |            0 |           0 |           0 |         0
     | 1n\*   |  0  |  0  |      288 |       1008 |          0 |            0 |            0 |           0 |           0 |         0
     | 2n\*   |  0  |  0  |     6336 |     109872 |     320544 |       181440 |            0 |           0 |           0 |         0
     | 3n\*   |  0  |  0  |   340056 |   13468392 |   99247680 |    203258160 |    122713920 |    17418240 |           0 |         0
     | 4n\*   |  0  |  0  | 35045208 | 2471082264 | 1169130560 | 130059805680 | 179192563200 | 86030320320 | 12386545920 | 319334400  
   
    Giving a grand total of 413496350074 (4.13e11) of the available 13894630244352000 (1.39e16).
    
    Non-normalised algebraic space is a waste of space because its majority of structures are nonsense structures. 

* ordered structure

    A structure of which the endpoints are positioned in such a way that their ordering follows that
    of the path taken to walk the structure. For example `"cab>&"` 
   
* placeholder

    Part of the `"placeholder/skin"` notation.
    Placeholders are used as index to find the actual endpoints as described in the skin.
    Placeholder `'a'` should be substituted by the first endpoints found in `"skin"`.
    Placeholders may or may not be unique.
    
* polarising    
   
* prime structure

* pure
     
* `QTF` / `QnTF` / `QTnF`

    `QTF` representing `Q ? T : F` where `T` and/or `F` can be inverted.
    
    `QnTF` representing `Q ? ~T : F` where T is always inverted and F is never inverted
    
    `QTnF` representing `Q ? T : ~F` where T is never inverted and T is always inverted
 
* recursive structures

  `abc^abc^^!`=`abc^^`
  
* self
 
* skin

    Part of the `"placeholder/skin"` notation.
    The skin contains what the placeholders should be replaced with. 
    
* slots

    Slots is a historic name. It is best comparable with "bus-width" in computer science. 
    It is used to determine the size and number of transforms, skins, structure endpoins, evaluator test vectors.
   
* tail

    Group of nodes nearest the endpoints

* transform

    A connection remapping. It contains instructions on how to attach endpoints to structures to change 
    the result/effect of the structure. There are two types of transforms, a forward and reverse. The reverse 
    transform is to undo the effect of the forward.
    
* transparent skin

    The default skin "abcdefghi" which does not change the look or effect of a structure.
   
* versioned memory
    
* zero    
