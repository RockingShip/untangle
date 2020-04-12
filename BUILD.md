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
    ./genrestartdata --text --qntf 0 2>/dev/null | wc
    ./genrestartdata --text        0 2>/dev/null | wc
    ./genrestartdata --text --qntf 1 2>/dev/null | wc
    ./genrestartdata --text        1 2>/dev/null | wc
    ./genrestartdata --text --qntf 2 2>/dev/null | wc
    ./genrestartdata --text        2 2>/dev/null | wc
    ./genrestartdata --text --qntf 3 2>/dev/null | wc
    ./genrestartdata --text        3 2>/dev/null | wc
    ./genrestartdata --text --qntf 4 2>/dev/null | wc
    ./genrestartdata --text        4 2>/dev/null | wc
    ./genrestartdata --text --qntf 5 2>/dev/null | wc
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
    ./gensignature 0n9.db transform.db 0 --text=1 >0n9-1.txt
    ./gensignature 0n9.db transform.db 0 --text=2 >0n9-2.txt --test
    ./gensignature 1n9.db 0n9.db 1 --text=1 >1n9-1.txt
    ./gensignature 1n9.db 0n9.db 1 --text=2 >1n9-2.txt --test 
    ./gensignature 2n9.db 1n9.db 2 --text=1 >2n9-1.txt
    ./gensignature 2n9.db 1n9.db 2 --text=2 >2n9-2.txt --test
    ./gensignature 3n9.db 2n9.db 3 --text=1 >3n9-1.txt
    ./gensignature 3n9.db 2n9.db 3 --text=2 >3n9-2.txt --test
    ./gensignature 4n9.db 3n9.db 4 --text=1 >4n9-1.txt
    ./gensignature 4n9.db 3n9.db 4 --text=2 >4n9-2.txt --test
```

## members

If you are in need for textual lists of members:

```sh
    ./genmember member-0n9.db 4n9.db 0 --text=1 >member-0n9-1.txt
    ./genmember member-1n9.db member-0n9.db 1 --text=1 >member-1n9-1.txt
    ./genmember member-2n9.db member-1n9.db 2 --text=1 >member-2n9-1.txt
    ./genmember member-3n9.db member-2n9.db 3 --text=1 >member-3n9-1.txt
    ./genmember member-4n9.db member-3n9.db 4 --text=1 >member-4n9-1.txt
```

```sh
    ./genmember member-0n9-qntf.db 4n9.db 0 --qntf --text=1 >member-0n9-1.txt
    ./genmember member-1n9-qntf.db member-0n9-qntf.db 1 --qntf --text=1 >member-1n9-qntf-1.txt
    ./genmember member-2n9-qntf.db member-1n9-qntf.db 2 --qntf --text=1 >member-2n9-qntf-1.txt
    ./genmember member-3n9-qntf.db member-2n9-qntf.db 3 --qntf --text=1 >member-3n9-qntf-1.txt
    ./genmember member-4n9-qntf.db member-3n9-qntf.db 4 --qntf --text=1 >member-4n9-qntf-1.txt
```