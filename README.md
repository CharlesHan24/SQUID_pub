# SQUID: Faster Analytics via Sampled Quantiles Data-structure

This repository contains the source code of the "SQUID: Faster Analytics via Sampled Quantiles Data-structure".

This code implements the varaints of squid algorithm presented in the paper. The code works on several real-life traces and can be easily extended to other traces.

## compiling SQUID-HH:
    
    g++  -c  -mavx2  -std=c++11 -march=haswell xxhash.c
    g++  -c  -mavx2  -std=c++11 -march=haswell Cuckoo.cpp
    g++ -mavx2  -std=c++11 -march=haswell xxhash.o Cuckoo.o  main.cpp -o main
    
    
# building the graphs:
After running the tests, to build the graphs go to "figures_scripts" and the run the graph scripts.
 
 
