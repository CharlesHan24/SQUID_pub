import lrfu
import json
import struct
import utils
import random
import const
import argparse

def readin_caida(data_fin, amount):
    fin = open(data_fin, "rb")
    results = []
    i = 0
    while True:
        i += 1
        if i >= amount:
            break
        try:
            bts = fin.read(21)
            five_tuple = bts[0:13]
            timestamp = struct.unpack('d', bts[13:])[0]

            hashed_key = utils.bobhash(five_tuple) & 0xfffffffc
            gen_value = random.randint(1, 2**24) * 2
            results.append([timestamp, hashed_key, gen_value])
        except:
            break

    return results


def readin_zipf(data_fin, amount):
    fin = open(data_fin, "rb")
    timestamp = fin.read(4)
    timestamp = struct.unpack("f", timestamp)[0]

    cur_time = 0
    batched_num = 100000
    results = []

    i = 0
    while True:
        if i % 1000000 == 0:
            print(i)
        if i >= amount:
            break
        try:
            bts = fin.read(8 * batched_num)
            for j in range(batched_num):
                key = int.from_bytes(bts[j * 8: j * 8 + 4], "little")
                value = int.from_bytes(bts[j * 8 + 4: j * 8 + 8], "little")
        
                results.append([cur_time, key, value])
                cur_time += timestamp
            i += batched_num
        except:
            break
    
    fin.close()

    return results

def readin_zipf_generator(data_fin, amount):
    fin = open(data_fin, "rb")
    timestamp = fin.read(4)
    timestamp = struct.unpack("f", timestamp)[0]

    cur_time = 0
    batched_num = 100000
    results = []

    i = 0
    while True:
        if i % 100000 == 0:
            print(i)
        if i >= amount:
            break
        try:
            bts = fin.read(8)
            key = int.from_bytes(bts[0: 4], "little")
            value = int.from_bytes(bts[4: 8], "little")
        
            yield [cur_time, key, value]
            cur_time += timestamp
            i += 1
        except:
            break
    
    fin.close()

    yield [const.INF_TIME, 0, 0]


def readin_data(data_fin, data_type, amount):
    # TODO
    if data_type == "CAIDA":
        return readin_caida(data_fin, amount)
    elif data_type == "zipf":
        return readin_zipf_generator(data_fin, amount)
    else:
        return []

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", type=str, required=True)
    parser.add_argument("--rows", type=int, required=True)
    parser.add_argument("--cols", type=int, required=True)
    args = parser.parse_args()

    config_fin = open(args.config, "r")
    config = json.load(config_fin)

    n = args.rows * args.cols

    data = readin_data("data/{}".format(config["dataset"]), "zipf", config["data_amount"])
    lrfu_engine = lrfu.LRFU(n, data, open(config["log_fname"] + "_{}_{}.txt".format(args.rows, args.cols), "w"))
    print("Finish reading the data")
    lrfu_engine.run()