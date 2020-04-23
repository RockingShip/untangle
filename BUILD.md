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

## candidates

If you are in need for textual lists of candidates (about 1Gbyte):

```sh
    ./gensignature transform.db 0 0n9.db --text=1 >0n9-1.txt
    ./gensignature transform.db 0        --text=2 >0n9-2.txt
    ./gensignature 0n9.db       1 1n9.db --text=1 >1n9-1.txt
    ./gensignature 0n9.db       1        --text=2 >1n9-2.txt
    ./gensignature 1n9.db       2 2n9.db --text=1 >2n9-1.txt
    ./gensignature 1n9.db       2        --text=2 >2n9-2.txt
    ./gensignature 2n9.db       3 3n9.db --text=1 >3n9-1.txt
    ./gensignature 2n9.db       3        --text=2 >3n9-2.txt 
    ./gensignature 3n9.db       4 4n9.db --text=1 >4n9-1.txt
    ./gensignature 3n9.db       4        --text=2 >4n9-2.txt

    ./gentransform 0n9 0 --no-generate --text=3 > 0n9-3.txt
    ./gentransform 1n9 1 --no-generate --text=3 > 1n9-3.txt
    ./gentransform 2n9 2 --no-generate --text=3 > 2n9-3.txt
    ./gentransform 3n9 3 --no-generate --text=3 > 3n9-3.txt
    ./gentransform 4n9 4 --no-generate --text=3 > 4n9-3.txt
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
    diff -q -s merged.lst 4n9-3.txt
```

## `6n9-pure` with `gensignature`

Example of how to tackle large address spaces with incomplete metrics.

First you need a matching `restartData[]` for the given address space.
Update `metricsGenerator[]` accordingly and run `./genrestartdata`.
Then guessimate how many tasks would cover the whole address space.
Run tasks in read-only mode for better memory usage.

As final metrics are unknown and merging all candidates is cpu-intensive.
Precautions need to be taken that merging does not fail before completeion due to database exhaustion.

```sh
    # divide and conquer takes about an hour
    mkdir logs
    qsub -cwd -o logs -e logs -b y -t 1-999 -q 8G.q ./gensignature 4n9.db 5 --pure --task=sge --text

    # check/count all jobs finished properly
    grep done -r logs/gensignature.e47632.*  --files-without-match

    #rerun any missing jobs with:
    qsub -cwd -o logs -e logs -b y -q 8G.q ./gensignature 4n9.db 5 --pure --task=n,999 --text

    # check/count no stray errors
    grep error -r logs/gensignature.o*

```


## members

If you are in need for textual lists of members:

```sh
    ./genmember 4n9.db        0 member-0n9.db --text=1 >member-0n9-1.txt
    ./genmember member-0n9.db 1 member-1n9.db --text=1 >member-1n9-1.txt
    ./genmember member-1n9.db 2 member-2n9.db --text=1 >member-2n9-1.txt
    ./genmember member-2n9.db 3 member-3n9.db --text=1 >member-3n9-1.txt
    ./genmember member-3n9.db 4 member-4n9.db --text=1 >member-4n9-1.txt
```

```sh
    ./genmember 4n9.db             0 member-0n9-pure.db --pure --text=1 >member-0n9-pure-1.txt
    ./genmember member-0n9-pure.db 1 member-1n9-pure.db --pure --text=1 >member-1n9-pure-1.txt
    ./genmember member-1n9-pure.db 2 member-2n9-pure.db --pure --text=1 >member-2n9-pure-1.txt
    ./genmember member-2n9-pure.db 3 member-3n9-pure.db --pure --text=1 >member-3n9-pure-1.txt
    ./genmember member-3n9-pure.db 4 member-4n9-pure.db --pure --text=1 >member-4n9-pure-1.txt
```

or use pre-determined member list created with `genmember --text=1` or `genmember text=3`

```sh
    ./genmember 4n9.db             0 member-0n9-pure.db --pure --no-generate --load=member-0n9-1.txt
    ./genmember member-0n9-pure.db 1 member-1n9-pure.db --pure --no-generate --load=member-1n9-pure-1.txt
    ./genmember member-1n9-pure.db 2 member-2n9-pure.db --pure --no-generate --load=member-2n9-pure-1.txt
    ./genmember member-2n9-pure.db 3 member-3n9-pure.db --pure --no-generate --load=member-3n9-pure-1.txt
    ./genmember member-3n9-pure.db 4 member-4n9-pure.db --pure --no-generate --load=member-4n9-pure-1.txt
```
