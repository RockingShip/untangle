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

"The database" contains all information to detect, construct and rewrite fractal based structures.
It has a number of sections containing and used for different topics.

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

Signatures searches for uniqueness in a given address space.
The target signature address space for this vesion of the project is 4-node and 9-endpoint/variables.

- The intermediate databases can be kept small by omitting the index.
  They will be automatically rebuilt on load.
- The last database has lowest interleave is great for storage but slow when using.

```sh
    ./gensignature transform.db 0 0n9.db --no-saveindex
    ./gensignature 0n9.db       1 1n9.db --no-saveindex
    ./gensignature 1n9.db       2 2n9.db --no-saveindex
    ./gensignature 2n9.db       3 3n9.db --no-saveindex
    ./gensignature 3n9.db       4 4n9.db --saveinterleave=1
``

## create hints

Hints measure the symmtery of signatures.
Higher symmetry has less load on the assosiative index.
Hints are used to tune the assosiative index giving faster database construction times.

Creating hints takes about 17 hours.

```sh
    ./genhint 4n9.db hints-4n9.db
```

Alternatively use with SunGridEngine which requires multiple machines.

```sh
    # make temp directories
    mkdir log-hints

    # submit to SGE. Each invocation needs about 1G memory
    qsub -cwd -o log-hints -e log-hints -b y -t 1-499 -q 4G.q ./genhint 4n9.db --task=sge --text

    # check/count all jobs finished properly
    grep done -r log-hints/genhint.e* --files-without-match
    grep done -r log-hints/genhint.e* --files-with-match | wc
    grep error -r log-hints/genhint.o* --files-with-match
     
    #rerun any missing jobs with:
    qsub -cwd -o log-hints -e logs-hint -b y -t 1-499 -q 4G.q ./genhint 4n9.db --task=n,499 --text
     
    # merge all collected hints
    # file will contain 858805139 lines
    cat log-hints/genhint.o*.? log-hints/genhint.o*.?? log-hints/genhint.o*.??? >hints.lst

    # load
    ./genhint 4n9.db hints-4n9.db --load=h.lst --no-generate
```

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
    ./gensignature 0n9.db 0 --no-generate --text=4 >0n9-4.lst
    ./gensignature 1n9.db 1 --no-generate --text=3 >1n9-3.lst
    ./gensignature 1n9.db 1 --no-generate --text=4 >1n9-4.lst
    ./gensignature 2n9.db 2 --no-generate --text=3 >2n9-3.lst
    ./gensignature 2n9.db 2 --no-generate --text=4 >2n9-4.lst
    ./gensignature 3n9.db 3 --no-generate --text=3 >3n9-3.lst
    ./gensignature 3n9.db 3 --no-generate --text=4 >3n9-4.lst
    ./gensignature 4n9.db 4 --no-generate --text=3 >4n9-3.lst
    ./gensignature 4n9.db 4 --no-generate --text=4 >4n9-4.lst
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

vv--------- obsoleted
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
^^---------

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
    cat logs/gensignature.o* >5n9-pure.1.lst

    # sort and unique.
    # Use ultra-fast add-if-not-found, however be aware of false positives.
    # There are so no presets available so raise limits that fit your memory model.
    # use `--text=3` to sort and unique the resulting signatures.
    # Output will contain 57412551 signatures and 57412551 imprints.
    ./gensignature 4n9.db 5 --pure --load=5n9-pure.1.lst --no-generate --no-sort --maxsignature=100000000 --maximprint=500000000 --ainf --interleave=1 --text=3 >5n9-pure.2.lst

    # rerun with better tuned section sizes and bump interleave to reduce amount of false-positives.
    # --maximprint is number of imprints last run times 2 because of new interleave factor.
    # Output will contain 48815521 signatures and 94713089 imprints.
    ./gensignature 4n9.db 5 --pure --load=5n9-pure.2.lst --no-generate --no-sort --maxsignature=57412551 --maximprint=114825102 --ainf --interleave=2 --text=3 >5n9-pure.3.lst

    # rerun.
    # --maximprint is number of imprints last run times 6 because of new interleave factor.
    # note: 568278534 imprints require too much memory. Reduce that to 500000000 and hope that is sufficient when duplicates removed.
    # Yes, it fits. Output will contain 42862728 signatures and 247029190 imprints.
    ./gensignature 4n9.db 5 --pure --load=5n9-pure.3.lst --no-generate --no-sort --maxsignature=48815521 --maximprint=568278534 --ainf --interleave=6 --text=3 >5n9-pure.4.lst

    # add-when-not-found is depleted.
    # Split the list into a number of databases.
    # Use `--interleave=120` as practical lowest interleave that takes about 45 minutes to compute.
    # prepare a sharable database with pre-calculated interleave.
    ./gensignature 4n9.db 4 split0.db --no-generate --interleave=120

    # Split the list and compare them individually collecting/cascading only winners of display name challanges.
    # Effectively, it removes signatures that are found in other lists that score better
    # this is done by using the truncate option which cafely catches when database sections overlow.
    # the `--text=3` will sort and uniq removing any false-positives.
    # keep `--maxsignature=` and `--maximprint` consistent to enable inheriting of database sections
    ./gensignature split0.db 5 split1.db --pure --load=5n9-pure.4.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --truncate --text=3 >split1.lst
    # Truncating at progress=12084611 "faabc!>d1e!ca!!"
    ./gensignature split0.db 5 split2.db --pure --load=5n9-pure.4.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --truncate --window=12084611,0 --text=3 >split2.lst
    # Truncating at progress=19365208 "fgabc!bdae!ac!!!"
    ./gensignature split0.db 5 split3.db --pure --load=5n9-pure.4.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --truncate --window=19365208,0 --text=3 >split3.lst
    # Truncating at progress=25901788 "abc!gaecdef!!!e!"
    ./gensignature split0.db 5 split4.db --pure --load=5n9-pure.4.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --truncate --window=25901788,0 --text=3 >split4.lst
    # Truncating at progress=32084414 "bdabc!c!de!e1+!"
    ./gensignature split0.db 5 split5.db --pure --load=5n9-pure.4.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --truncate --window=32084414,0 --text=3 >split5.lst
    # Truncating at progress=38198459 "abc!de!21^a!f2!"
    ./gensignature split0.db 5 split6.db --pure --load=5n9-pure.4.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --truncate --window=38198459,0 --text=3 >split6.lst

    # list split in six parts with false-positives removed (--because of --text=3`) 
    # Their wordcount being  `4703839+4269774+4588180+4536969+4546816+3701209=26346787`
    # keep database in disk cache for as long as possible
    # each invocation takes about 30 minutes
    cp  split1.db /dev/null # load into disk-cache
    ./gensignature split1.db 5 --pure --load=split2.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split2a.lst
    ./gensignature split1.db 5 --pure --load=split3.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split3a.lst
    ./gensignature split1.db 5 --pure --load=split4.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split4a.lst
    ./gensignature split1.db 5 --pure --load=split5.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split5a.lst
    ./gensignature split1.db 5 --pure --load=split6.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split6a.lst
    # wordcount `4703839+3498983+3073191+2924217+3119443+2435255=19754928`
    cp split2.db /dev/null # load into disk-cache
    ./gensignature split2.db 5 --pure --load=split1.lst  --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split1b.lst
    ./gensignature split2.db 5 --pure --load=split3a.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split3b.lst
    ./gensignature split2.db 5 --pure --load=split4a.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split4b.lst
    ./gensignature split2.db 5 --pure --load=split5a.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split5b.lst
    ./gensignature split2.db 5 --pure --load=split6a.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split6b.lst
    # wordcount `4333533+3498983+2645211+2581264+2806578+2181616=18047185`
    cp split3.db /dev/null # load into disk-cache
    ./gensignature split3.db 5 --pure --load=split1b.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split1c.lst
    ./gensignature split3.db 5 --pure --load=split2a.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split2c.lst
    ./gensignature split3.db 5 --pure --load=split4b.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split4c.lst
    ./gensignature split3.db 5 --pure --load=split5b.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split5c.lst
    ./gensignature split3.db 5 --pure --load=split6b.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split6c.lst
    # wordcount `4159315+3266717+2645211+2362714+2648730+2068295=17150982`
    cp split4.db /dev/null # load into disk-cache
    ./gensignature split4.db 5 --pure --load=split1c.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split1d.lst
    ./gensignature split4.db 5 --pure --load=split2c.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split2d.lst
    ./gensignature split4.db 5 --pure --load=split3b.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split3d.lst
    ./gensignature split4.db 5 --pure --load=split5c.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split5d.lst
    ./gensignature split4.db 5 --pure --load=split6c.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split6d.lst
    # wordcount `4019912+3080922+2480036+2362714+2492614+1942101=16378299`
    cp split5.db /dev/null # load into disk-cache
    ./gensignature split5.db 5 --pure --load=split1d.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split1e.lst
    ./gensignature split5.db 5 --pure --load=split2d.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split2e.lst
    ./gensignature split5.db 5 --pure --load=split3d.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split3e.lst
    ./gensignature split5.db 5 --pure --load=split4c.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split4e.lst
    ./gensignature split5.db 5 --pure --load=split6d.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split6e.lst
    # wordcount `3895383+2888398+2292279+2183268+2492614+1528055=15279997`
    cp split6.db /dev/null # load into disk-cache
    ./gensignature split6.db 5 --pure --load=split1e.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split1f.lst
    ./gensignature split6.db 5 --pure --load=split2e.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split2f.lst
    ./gensignature split6.db 5 --pure --load=split3e.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split3f.lst
    ./gensignature split6.db 5 --pure --load=split4e.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split4f.lst
    ./gensignature split6.db 5 --pure --load=split5d.lst --no-generate --no-sort --maxsignature=42862728 --maximprint=500000000 --text=1 >split5f.lst
    # wordcount `3781062+2770362+2140339+2092718+2386166+1528055=14698702`

    # collect all signatures that challanged all candidate display names and won
    cat split1f.lst split2f.lst split3f.lst split4f.lst split5f.lst split6e.lst >split7.lst
    # final sort. Test how many signatures and imprints
    # all signatures are uniq so add-if-not-found is safe to use
    # Update `metrics.h` with these findings
    ./gensignature 4n9.db 5 --pure --load=split7.lst --no-generate --maxsignature=42862728 --maximprint=500000000 --interleave=1 --ainf --text=3 >5n9-pure-3.lst

    # create a database for signature lookups
    ./gensignature 4n9.db 5 --pure 5n9-pure.db --load=5n9-pure-3.lst --no-generate --maxsignature=15490349 --maximprint=15490349 --interleave=1 --ainf

    # clean-up
    rm 5n9-pure.[1-5].lst
    rm split[0-6].db split[1-6].lst split[1-6][a-f].lst
``` 5n9-pure.1.lst


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
