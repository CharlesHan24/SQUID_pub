import random
import math
import json
import pdb
import struct

# data/data.dat: 0.95 zipf
# data/data2.dat: 0.99 zipf
path_query = "data/data3.dat"
num_query = 50000000 # 500M items
zipf = 0.9

max_key = 1600000


#Zipf
zeta = [0.0]
for i in range(1, max_key + 1):
    zeta.append(zeta[i - 1] + 1 / pow(i, zipf))

# pdb.set_trace()

field = [0] * (num_query + 1)
k = 1
for i in range(1, num_query + 1):
    if (i > num_query * zeta[k] / zeta[max_key]):
        k = k + 1
    field[i] = k
    if i % 1000000 == 0:
        print(i)

# pdb.set_trace()

#Generate queries
fout = open(path_query, "wb")
config_fin = open("config_hashing_mc_95.json", "r")
config = json.load(config_fin)

fout.write(struct.pack('f', config["time_interv"]))


# pdb.set_trace()
for i in range(num_query):
    if i % 1000000 == 0:
        print(i)
    #Randomly select a key in zipf distribution
    r = random.randint(1, num_query)
    key = field[r]
    value_p = (i + 1) * 2 # priority value
    
    fout.write(key.to_bytes(4, "little"))
    fout.write(value_p.to_bytes(4, "little"))
    #Save the generated query to the file
    
fout.flush()
fout.close()