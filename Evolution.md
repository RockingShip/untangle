@date 2021-06-06 01:48:57
For internal use.

upto 2019-03-01

- QTF notation
- folding
- multi-root
- fixed structure rewrite rules
- 4 nodes 9 variables
- dynamic structure rewrite rules
- signatures/patterns/fragments/slots
- footprints (rule generation)
- evaluator
- row/col index (rule generation)
- vector flood-fill (rule generation)
- 5 nodes 9 variables
- multi-node groups
- 'fold-injection'
- QTnF only
- slot swaps (first fast, later slow)
- sub-signature matching
- node-group polarity (obsoleted by QnTF)
- slots sid+tid pair
- grow section
- 6 nodes 9 variables QTnF (partially)
- swap hotspots
- power
- tile section
- QnTF only (`ab&`->`aab<<`, `ab^`->`abab+#`, `abc?`->`aabc#c#`)
- `packedTree_t`
- Invert elimination
- pre-detector `rewriteData`
- structure based `compare()`
- stack based tree walk
- detector patterns: component patterns match (grow) signatures

as of v2
- versioned memory (could be earlier)
- tree walk path and `compare()`
- members
- hints (symmetry)
- add-if-not-found
- rewrite/normalise immunity (safe QnTF)
- 6n9-pure signature set in 2 hours
- saving/cloning multi-root trees with nodes in tree-walk order
- generator/runtime synced ordering for better duplicate detection
- heapsort to order candidate pivot keys
- cascades for communicative dyadics
- signature representative is lowest of all `compare()`
- treewalk placeholder/endpoint assignment
- cross-product expands all members of signature group
- genpattern additional filters against existing members
