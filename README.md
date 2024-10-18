# SQUID: Faster Analytics via Sampled Quantiles Data-structure

This repository contains the source code of the "SQUID: Faster Analytics via Sampled Quantiles Data-structure".

This code implements the variants of squid algorithm presented in the paper. The code works on several real-life traces and can be easily extended to other traces.

## Compiling SQUID-HH:
    
    g++  -c  -mavx2  -std=c++11 -march=haswell xxhash.c
    g++  -c  -mavx2  -std=c++11 -march=haswell Cuckoo.cpp
    g++ -mavx2  -std=c++11 -march=haswell xxhash.o Cuckoo.o  main.cpp -o main
    
    
# building the graphs:
After running the tests, to build the graphs go to "figures_scripts" and then run the graph scripts, the figures will be created in the folder "figures"
 
 
## P4 implementation of SQUID

The `p4src` directory contains the P4 source code of SQUID, and can be compiled into Tofino switches. The `squid_lrfu_4.p4` and `squid_lrfu_4_small.p4` can be compiled to Tofino, while `squid_lrfu_8.p4` and `squid_lrfu_12.p4` can be compiled to Tofino2.

The `p4_simulation` directory is the simulated implementation of the P4 SQUID for evaluating the performance of P4 SQUID, which mimics the behaviors of the network (transmission delay) and the P4 program installed in Tofino switches. 
- To run the simulation, we first prepare for the datasets and place them in the `./data` folder. You can download the datasets we used by following this [link](https://liveuclac-my.sharepoint.com/:u:/g/personal/ucabwh2_ucl_ac_uk/EX6KwUahD-JOpk3c7LymsQgB1JSsC1_6kX9x4SOZ5y70mA?e=bYc48k). Alternatively, you can also generate the Zip-f dataset (`data.dat`, `data2.dat`, `data3.dat` correspond to zipf_95 zipf_99 zipf_90 respectively), by simply running `python3 data_gen.py`. 
- We have provided configuration profiles in json. The configuration files are placed in the `./configs` folder and should be copied to `./`. You will need to make modifications to the fields such as "dataset", "log_fname". It also allows to change the parameters of our algorithm such as "fullness" and "emptiness", etc. 
- There are scripts for automating the simulations. To run the original LRFU, simply execute `bash run_lrfu.sh`. To run the CPU SQUID-LRFU, execute `bash run_squid_lrfu.sh`. To run the simulated P4 SQUID-LRFU, execute `bash run_squid_lrfu_approx.sh`.
