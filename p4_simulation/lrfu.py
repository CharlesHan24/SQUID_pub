from collections import OrderedDict
import const
import pdb
import sortedcontainers
import math

def lrfu_strategy(old_val, new_val):
    dif = old_val - new_val
    if dif > 200:
        return old_val
    elif dif < -200:
        return new_val
    else:
        return new_val + math.log(math.exp(dif) + 1)

def init_val(timestamp):
    timestamp += 1
    c = 0.75
    return -math.log(c) * timestamp

class LRFU(object):
    def __init__(self, cached_cnt, data_gen, log):
        self.data_gen = data_gen
        self.cached_cnt = cached_cnt
        self.cached_item = dict()
        self.cached_value = sortedcontainers.SortedDict()
        self.timestamp = 0
        self.log = log

    def run(self):
        i = 0
        hit = 0
        while True:
            ts, key, val = next(self.data_gen)
            if ts == const.INF_TIME:
                break
            i += 1

            if key in self.cached_item:
                val = self.cached_item[key]
                try:
                    self.cached_value.pop(val)
                except:
                    pdb.set_trace()
                self.timestamp += 1
                new_val = init_val(self.timestamp)
                val = lrfu_strategy(val, new_val)
                self.cached_value[val] = key
                self.cached_item[key] = val
                hit += 1
            
            else:
                self.timestamp += 1
                new_val = init_val(self.timestamp)
                if len(self.cached_item) < self.cached_cnt:
                    self.cached_item[key] = new_val
                    self.cached_value[new_val] = key
                else:
                    val = self.cached_value.peekitem(0)
                    del_key, val = val[1], val[0]

                    self.cached_value.pop(val)
                    self.cached_item.pop(del_key)

                    self.cached_item[key] = new_val
                    self.cached_value[new_val] = key
            
            if i % 1000000 == 0:
                print(i)
                self.log.write("{}/{}\n".format(hit, i))
                self.log.flush()
