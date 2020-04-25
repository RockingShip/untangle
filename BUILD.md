Building `untangle`
=

`untangle` is designed with `gcc`, `autotools` and Linux exclusively in mind.

# Requirements

*   SSE4.2 CRC assembler instruction
*   32G-40G physical memory
*   200G SSD storage (expect files upto 24G large)
*   `autotools`
*   `jansson` http://www.digip.org/jansson
*   (optionally) AWS instances with StarGridEngine

# Basic instructions

* ## generate `Makefile` ##
    
    ```sh
    ./autogen
    ```
    
* ## configure and compile ##
    
    ```sh
    ./configure
    make
    ```
    
# Building database

The database is create in steps.
With each step generator tools adds sections and content to the database.

## create initial database

`gentransform` creates te initial database containing all the skin and transform data.
You won't get far without it.
    
First, self test to determine if assumptions are correct
    
```sh
    ./gentransform - --selftest
```
    
Then create database
    
```sh
    ./gentransform transform.db
```

## create signatures

Signatures are basically the uniqueness of a given address space.   
# Developer instructions
 
## `restartData[]`
 
Generate `restartdata.h` that created `restartData[]` for `--task` and `--windowhi/lo`.
This invokes the generator with different settings found in `metricsGenerator[]`.
Each invocation uses a stub `foundTree()` to count how often that is called.
 
```sh
    ./genrestartdata >restartdata.h
```
 
## `numProgress` in `metricsGenerator[]`

Use the log output of `genrestartdata` to update `numProgress` in `metricsGenerator[]`.

## `numCandidate` in `metricsGenerator[]`

Update `metricsGenerator[]` with the output the commands below.

In this mode the `foundTree()` adds the candidate to the database and only displays the first occurance.
You need to add 1 to the total because `wc` does not include the first reserved entry.

```sh
    ./genrestartdata --text --pure 0 2>/dev/null | wc
    ./genrestartdata --text        0 2>/dev/null | wc
    ./genrestartdata --text --pure 1 2>/dev/null | wc
    ./genrestartdata --text        1 2>/dev/null | wc
    ./genrestartdata --text --pure 2 2>/dev/null | wc
    ./genrestartdata --text        2 2>/dev/null | wc
    ./genrestartdata --text --pure 3 2>/dev/null | wc
    ./genrestartdata --text        3 2>/dev/null | wc
    ./genrestartdata --text --pure 4 2>/dev/null | wc
    ./genrestartdata --text        4 2>/dev/null | wc
    ./genrestartdata --text --pure 5 2>/dev/null | wc
    ./genrestartdata --text        5 2>/dev/null | wc
```

## `metricsImprint[]` and `numMember` in `metricsGenerator[]`

Update `numImprint` in `metricsImprint[]` with the output the commands below.
``speed`/`storage` are only for visual indications.

`genSignature` will invoke the generator with different settings found in `metricsImprint[]`.

```sh
    ./gentransform transform.db
    ./gensignature transform.db --metrics
```

## `numMember` in `metricsGenerator[]`

This is basically building the dataset and collecting ticker output from `genmetrics`. 
Update `numMember` in `metricsGenerator[]` accordingly.

If building hits some limit, set `--maxmember` to some higher value and update metrics accordingly.

## textual lists signatures

If you are in need for textual lists of candidates (about 1Gbyte):

```sh
    ./gensignature transform.db 0 0n9.db --text=1 >0n9-1.lst
    ./gensignature 0n9.db       1 1n9.db --text=1 >1n9-1.lst
    ./gensignature 1n9.db       2 2n9.db --text=1 >2n9-1.lst
    ./gensignature 2n9.db       3 3n9.db --text=1 >3n9-1.lst
    ./gensignature 3n9.db       4 4n9.db --text=1 >4n9-1.lst

    # as an alternative, create sorted and unique list by extracting them from the databases
    ./gensignature 0n9.db 0 --no-generate --text=3 >0n9-3.lst
    ./gensignature 1n9.db 1 --no-generate --text=3 >1n9-3.lst
    ./gensignature 2n9.db 2 --no-generate --text=3 >2n9-3.lst
    ./gensignature 3n9.db 3 --no-generate --text=3 >3n9-3.lst
    ./gensignature 4n9.db 4 --no-generate --text=3 >4n9-3.lst
```

## Parallel `gensignature`

Perform `4n9` in parallel to test that tasking/slicing works. 
`genmember` and `genhints` is based on the similar codebase/workflow.

```sh
    # make temp directories
    mkdir logs-gensignature

    # submit to SGE
    qsub -cwd -o logs -e logs -b y -t 1-4 -q 8G.q ./gensignature 3n9.db 4 --task=sge --text

    # check/count all jobs finished properly
    grep done -r logs/gensignature.e*

    # check/count no stray errors
    grep error -r logs/gensignature.o*

    # combine all candidates to a single file
    cat logs/gensignature.o* >tasks.lst

    # count lines so you have an impression how long merging might take
    wc tasks.lst

    # merge, uniq and sort
    ./gensignature 3n9.db 4 --load=tasks.lst --no-generate --text=3 >merged.lst

    # compare with single run
    diff -q -s merged.lst 4n9-3.lst
```

## `5n9-pure` with `gensignature`

Example of how to tackle large address spaces with incomplete metrics.

First you need a matching `restartData[]` for the given address space.
Update `metricsGenerator[]` accordingly and run `./genrestartdata`.

The default interleave of 504 is tuned for 4n9 signatures.
For larger address space 120 would hold about 4x more signatures and 4x slower.


```sh
    # Try how much an inital run would hold
    # Set section sizes to utilize maximum memory.
    # Use 4n9 database to reject signatures already found in 4n9 space.
    # Collect best (single) candidate for every signature.
    # maxsignature/maximprint is roughly 52G Memory and database
    ./gensignature 4n9.db 5 5n9-pure.1.db --pure --inter=120 --truncate --no-sort --maxsignature=20000000 --maximprint=500000000 --text=3 --truncate >>5n9-pure.1.lst

    # As expected the database overflows and at progress position 442082096 which is 48%.
    # This is nice as it indicates that another 1-2 invocations are needed.
    # Using zero for highest `"--window=<low,<high>"` indicates open-ended.
    ./gensignature 4n9.db 5 5n9-pure.2.db --pure --inter=120 --truncate --no-sort --maxsignature=20000000 --maximprint=500000000 --text=3 --truncate --window=442082096,0 >>5n9-pure.2.lst

    # The previous captured 66%.
    ./gensignature 4n9.db 5 5n9-pure.3.db --pure --inter=120 --truncate --no-sort --maxsignature=20000000 --maximprint=500000000 --text=3 --truncate --window=604993616,0 >>5n9-pure.3.lst

    # The previous captured 76%.
    ./gensignature 4n9.db 5 5n9-pure.4.db --pure --inter=120 --truncate --no-sort --maxsignature=20000000 --maximprint=500000000 --text=3 --truncate --window=696177599,0 >>5n9-pure.4.lst

    # The previous captured 80%. (Is it slowing down?)
    ./gensignature 4n9.db 5 5n9-pure.5.db --pure --inter=120 --truncate --no-sort --maxsignature=20000000 --maximprint=500000000 --text=3 --truncate --window=735633644,0 >>5n9-pure.5.lst
```

Alternatively:

```sh
    #divide and conquer takes about an hour
    mkdir logs
    qsub -cwd -o logs -e logs -b y -t 1-499 -q 8G.q ./gensignature 4n9.db 5 --pure --task=sge --text
     
    # check/count all jobs finished properly
    grep done -r logs/gensignature.e* --files-without-match
    grep error -r logs/gensignature.o* --files-with-matches
     
    #rerun any missing jobs with:
    qsub -cwd -o logs -e logs -b y -q 8G.q ./gensignature 4n9.db 5 --pure --task=n,499 --text
     
    # merge all collected candidate signatures
    # file will contain 858805139 lines
    cat logs/gensignature.o* >signatures-5n9-pure.1.lst

    # sort and unique.
    # Use ultra-fast add-if-not-found, however be aware of false positives.
    # There are so no presets available so raise limits that fit four memory model.
    # Output will contain 57412551 signatures and 57412551 imprints.
    ./gensignature 4n9.db 5 --pure --load=signatures-5n9-pure.1.lst --no-generate --no-sort --maxsignature=100000000 --maximprint=500000000 --ainf --interleave=1 --text=3 >signatures-5n9-pure.2.lst

    # rerun with better tuned section sizes and bump interleave to reduce amount of false-positives.
    # --maximprint is number of imprints last run times 2 because of new interleave factor.
    # Output will contain 48815521 signatures and 94713089 imprints.
    ./gensignature 4n9.db 5 --pure --load=signatures-5n9-pure.2.lst --no-generate --no-sort --maxsignature=57412551 --maximprint=114825102 --ainf --interleave=2 --text=3 >signatures-5n9-pure.3.lst

    # rerun.
    # --maximprint is number of imprints last run times 6 because of new interleave factor.
    # note: 568278534 imprints require too much memort. Reduce that to 500000000 and hope that is sufficient when duplicates removed.
    # Yes, it fits. Output will contain 42862728 signatures and 247029190 imprints.
    ./gensignature 4n9.db 5 --pure --load=signatures-5n9-pure.3.lst --no-generate --no-sort --maxsignature=57412551 --maximprint=568278534 --ainf --interleave=6 --text=3 >signatures-5n9-pure.4.lst
```


## members

If you are in need for textual lists of members:

```sh
    ./genmember 4n9.db        0 member-0n9.db --text=1 >member-0n9-1.lst
    ./genmember member-0n9.db 1 member-1n9.db --text=1 >member-1n9-1.lst
    ./genmember member-1n9.db 2 member-2n9.db --text=1 >member-2n9-1.lst
    ./genmember member-2n9.db 3 member-3n9.db --text=1 >member-3n9-1.lst
    ./genmember member-3n9.db 4 member-4n9.db --text=1 >member-4n9-1.lst
```

```sh
    ./genmember 4n9.db             0 member-0n9-pure.db --pure --text=1 >member-0n9-pure-1.lst
    ./genmember member-0n9-pure.db 1 member-1n9-pure.db --pure --text=1 >member-1n9-pure-1.lst
    ./genmember member-1n9-pure.db 2 member-2n9-pure.db --pure --text=1 >member-2n9-pure-1.lst
    ./genmember member-2n9-pure.db 3 member-3n9-pure.db --pure --text=1 >member-3n9-pure-1.lst
    ./genmember member-3n9-pure.db 4 member-4n9-pure.db --pure --text=1 >member-4n9-pure-1.lst
```

or use pre-determined member list created with `genmember --text=1` or `genmember text=3`

```sh
    ./genmember 4n9.db             0 member-0n9-pure.db --pure --no-generate --load=member-0n9-1.lst
    ./genmember member-0n9-pure.db 1 member-1n9-pure.db --pure --no-generate --load=member-1n9-pure-1.lst
    ./genmember member-1n9-pure.db 2 member-2n9-pure.db --pure --no-generate --load=member-2n9-pure-1.lst
    ./genmember member-2n9-pure.db 3 member-3n9-pure.db --pure --no-generate --load=member-3n9-pure-1.lst
    ./genmember member-3n9-pure.db 4 member-4n9-pure.db --pure --no-generate --load=member-4n9-pure-1.lst
```
