rows=(4096 16384 32768 65536 65536 65536)
columns=(4 4 4 4 8 12)

#rows=(4096 65536 65536)
#columns=(4 8 12)

for i in {0..5}
do
    python3 benchmark.py --config config_hashing_mc_lrfu_approx_caida.json --rows ${rows[i]} --cols ${columns[i]}
    # python3 benchmark.py --config config_hashing_mc_lrfu_approx_99.json --rows ${rows[i]} --cols ${columns[i]}
done