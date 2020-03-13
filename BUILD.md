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

* ## create initial database ##

    `gentransform` creates te initial database containing all the skin and transform data.
    
    Self test to determine if assumptions are correct
    
    ```sh
    ./gentransform --selftest
    ```
    
    Create database
    
    ```sh
    ./gentransform transform.db
    ```
  