#! /bin/bash

# This script does a system invert of 9bit.dat (9 inputs/9 outputs)
# It tests the basics and all optimisations are disabled

./ksystem sys.dat 9bit.dat --force              # convert expression to balanced system

./kextract 9bit.k0.dat sys.dat k0 --force       # extract key from system (answer)
./validate 9bit.json 9bit.k0.dat                # verify correctness
./kjoin sys.1.dat 9bit.k0.dat sys.dat --force   # remove key from system (remainder)
./validate 9bit.json sys.1.dat

./kextract 9bit.k1.dat sys.1.dat k1 --force
./validate 9bit.json 9bit.k1.dat
./kjoin sys.2.dat 9bit.k1.dat sys.1.dat --force
./validate 9bit.json sys.2.dat

./kextract 9bit.k2.dat sys.2.dat k2 --force
./validate 9bit.json 9bit.k2.dat
./kjoin sys.3.dat 9bit.k2.dat sys.2.dat --force
./validate 9bit.json sys.3.dat

./kextract 9bit.k3.dat sys.3.dat k3 --force
./validate 9bit.json 9bit.k3.dat
./kjoin sys.4.dat 9bit.k3.dat sys.3.dat --force
./validate 9bit.json sys.4.dat

./kextract 9bit.k4.dat sys.4.dat k4 --force
./validate 9bit.json 9bit.k4.dat
./kjoin sys.5.dat 9bit.k4.dat sys.4.dat --force
./validate 9bit.json sys.5.dat

./kextract 9bit.k5.dat sys.5.dat k5 --force
./validate 9bit.json 9bit.k5.dat
./kjoin sys.6.dat 9bit.k5.dat sys.5.dat --force
./validate 9bit.json sys.6.dat

./kextract 9bit.k6.dat sys.6.dat k6 --force
./validate 9bit.json 9bit.k6.dat
./kjoin sys.7.dat 9bit.k6.dat sys.6.dat --force
./validate 9bit.json sys.7.dat

./kextract 9bit.k7.dat sys.7.dat k7 --force
./validate 9bit.json 9bit.k7.dat
./kjoin sys.8.dat 9bit.k7.dat sys.7.dat --force
./validate 9bit.json sys.8.dat

./kextract 9bit.k8.dat sys.8.dat k8 --force
./validate 9bit.json 9bit.k8.dat
./kjoin sys.9.dat 9bit.k8.dat sys.8.dat --force
./validate 9bit.json sys.9.dat

# the following is over-extracting and should trigger a validation error
./kextract 9bit.o0.dat sys.9.dat o0 --force
./validate 9bit.json 9bit.o0.dat
