rows=(4096 16384 32768 65536 65536 65536 131072)
columns=(4 4 4 4 8 12 12)

for i in {0..6}
do
    # python3 benchmark_lrfu.py --config config_lrfu_90.json --rows ${rows[i]} --cols ${columns[i]}
    # python3 benchmark_aifo.py --config configs/config_lrfu_caida.json --rows ${rows[i]} --cols ${columns[i]}
    # python3 benchmark_aifo.py --config configs/config_lrfu_mergep.json --rows ${rows[i]} --cols ${columns[i]}
    # python3 benchmark_aifo.py --config configs/config_lrfu_S2.json --rows ${rows[i]} --cols ${columns[i]}
    python3 benchmark_aifo.py --config configs/config_lrfu_95.json --rows ${rows[i]} --cols ${columns[i]}
    python3 benchmark_aifo.py --config configs/config_lrfu_90.json --rows ${rows[i]} --cols ${columns[i]}
    # python3 benchmark_aifo.py --config config_lrfu_90.json --rows ${rows[i]} --cols ${columns[i]}
    # python3 benchmark_aifo.py --config config_lrfu_95.json --rows ${rows[i]} --cols ${columns[i]}
    # python3 benchmark_aifo.py --config config_lrfu_99.json --rows ${rows[i]} --cols ${columns[i]}
done