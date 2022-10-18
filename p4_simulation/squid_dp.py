import random
import copy
import pdb
import pickle

from const import BACKEND_OP, CONTROL_PLANE_OP, DATA_PLANE_OP
import utils

"""
Memory layout for each slot:
Key (32 bit) || value_cmp(31 dgs).
Round: 0, 1 | 0, 1  -> pre/post sampling
bitmap: 0/1. 1: item inserted after samlping begins. Clear bitmap to 0s in a round robin manner after the next round begins.
"""

class Squid_DP(object):
    def __init__(self, rows, columns, fullness, sample_z, hash, log, checkpoint_fout):
        self.log = log
        self.checkpoint_fout = checkpoint_fout

        self.rows = rows
        self.columns = columns
        self.n = self.rows * self.columns
        
        self.array = [[] for i in range(columns)]

        for i in range(self.columns):
            for j in range(self.rows):
                self.array[i].append([0, 0, 1])

        self.bitmap = [0 for i in range(self.columns * self.rows // 64)]
        self.bitmap_len = len(self.bitmap)

        self.threshold = 1
        self.phase = 0
        
        self.sample_counter = 0
        self.fullness = int(self.rows * self.columns * fullness)
        self.pkt_counter_remain = self.fullness

        self.sample_z = sample_z
        self.sample_prob = 1 / (0.1 * self.rows * self.columns * fullness / sample_z)

        self.sta_total_pkt = 0
        self.sta_hit = 0
        self.sta_counter = 0
        self.hash = hash

        self.sta_complete_sample = []

    def install_controller_simulator(self, simulator, controller):
        self.simulator = simulator
        self.controller = controller
    
    def sample(self, timestamp, flow_key, flow_val):
        self.sta_total_pkt += 1

        index = random.randint(0, self.rows - 1)
        col = random.randint(0, self.columns - 1)
        
        self.sample_counter += 1
        
        uploaded_value = copy.deepcopy(self.array[col][index])
        if utils.check_bitmap_digit(self.bitmap, index, col, self.rows, self.columns) != 0:
            uploaded_value[1] = 0
        
        self.simulator.schedule(CONTROL_PLANE_OP, self.controller.accept_sample, self.array[col][index])

    
    def verify(self, timestamp, verify_l, verify_r, threshold1, threshold2, counter1, counter2):
        if verify_l == 0:
            print("Start verifying at phase {}".format(self.phase))
            self.save_log()

        self.sta_total_pkt += 1

        for i in range(self.columns):
            if utils.check_bitmap_digit(self.bitmap, verify_l, i, self.rows, self.columns) != 0:
                continue
            if self.array[i][verify_l][1] >= threshold1:
                counter1 += 1
            if self.array[i][verify_l][1] >= threshold2:
                counter2 += 1
            
        verify_l += 1
        if verify_l < verify_r:
            self.simulator.schedule(DATA_PLANE_OP, self.verify, verify_l, verify_r, threshold1, threshold2, counter1, counter2)
        else:
            self.simulator.schedule(CONTROL_PLANE_OP, self.controller.verify, counter1, counter2)
    
    
    def adjust(self, timestamp, new_threshold, minus_cnt):
        print("New round: phase{}. Counter = {}. New threshold: {}".format(self.phase + 1, self.sta_counter, new_threshold))
        self.save_log()
        self.threshold = new_threshold
        self.pkt_counter_remain += minus_cnt
        self.phase += 1
        self.sample_counter = 0
        self.save_log()

    def save_log(self):
        self.log.write("Phase: {}. Counter = {}. Hit = {}. Threshold = {}. Total_pkt = {}. ".format(self.phase, self.sta_counter, self.sta_hit, self.threshold, self.sta_total_pkt))

        cur_res = 0
        for i in range(self.columns):
            for j in range(self.rows):
                if self.array[i][j][1] >= self.threshold:
                    cur_res += 1
        
        self.log.write("Occupancy: {}/{}\n".format(cur_res, self.n))

        self.log.flush()

        fout = open(self.checkpoint_fout, "wb")

        pickle.dump([self.array, self.bitmap], fout)
        fout.flush()
        fout.close()



class Squid_DP_Best_Effort(Squid_DP):
    def __init__(self, rows, columns, fullness, sample_z, hash, log, checkpoint_fout):
        super().__init__(rows, columns, fullness, sample_z, hash, log, checkpoint_fout)


    def insert_inbound(self, timestamp, flow_key, flow_val):
        self.sta_total_pkt += 1
        self.sta_counter += 1

        index = self.hash(flow_key) % self.rows

        matched = 0

        for i in range(self.columns):
            if self.array[i][index][0] == flow_key:
                # pdb.set_trace()
                self.sta_hit += 1
                matched = 1

                if self.array[i][index][1] < self.threshold and flow_val >= self.threshold:
                    self.pkt_counter_remain -= 1
                self.array[i][index][1] = flow_val

            else:
                if self.array[i][index][1] < self.threshold:
                    self.array[i][index][2] |= 1

        if self.pkt_counter_remain <= 0 and (self.phase % 2) == 0:
            # pdb.set_trace()
            print("Sampling on phase {}. Counter: {}".format(self.phase + 1, self.sta_counter))
            self.save_log()
            self.phase += 1
            self.sta_complete_sample.append(0)

        if self.pkt_counter_remain <= 0 and random.random() < self.sample_prob and self.sample_counter < self.sample_z:
            self.simulator.schedule(DATA_PLANE_OP, self.sample, flow_key, flow_val)
        
            self.sta_complete_sample[-1] += 1

        if matched == 0:
            self.simulator.schedule(BACKEND_OP, self.insert_outbound, flow_key, flow_val)


    def insert_outbound(self, timestamp, flow_key, flow_val):
        # pdb.set_trace()

        if (flow_val < self.threshold):
            return    
        
        index = self.hash(flow_key) % self.rows
        for i in range(self.columns):
            if self.array[i][index][0] == flow_key:
                if self.array[i][index][1] < self.threshold:
                    self.pkt_counter_remain -= 1
                self.array[i][index][1] = flow_val
                self.array[i][index][2] = 0
                
                if self.phase & 1 == 1:
                    self.bitmap = utils.set_bitmap_one(self.bitmap, index, i, self.rows, self.columns)
                else:
                    self.bitmap[self.pkt_counter_remain % self.bitmap_len] = 0
                break


            if self.array[i][index][2] & 1 == 1: # pre-cleared slot
                self.array[i][index][0] = flow_key
                self.array[i][index][1] = flow_val
                self.array[i][index][2] = 0
                self.pkt_counter_remain -= 1

                if self.phase & 1 == 1:
                    self.bitmap = utils.set_bitmap_one(self.bitmap, index, i, self.rows, self.columns)
                else:
                    self.bitmap[self.pkt_counter_remain % self.bitmap_len] = 0
                break

import math
def lrfu_strategy(old_val, new_val):
    dif = old_val - new_val
    if dif > 200:
        return old_val
    elif dif < -200:
        return new_val
    else:
        return new_val + math.log(math.exp(dif) + 1)

def approximated_lrfu_strategy(old_val, new_val):
    dif = old_val - new_val
    if dif < -5:
        return new_val
    else:
        return old_val + 5

def init_val(new_val):
    new_val += 1
    c = 0.75
    return -math.log(c) * new_val

def approximated_init_val(new_val):
    new_val += 1
    c = 0.75
    return 2 * new_val

class Squid_DP_Best_Effort_LRFU(Squid_DP):
    def __init__(self, rows, columns, fullness, sample_z, hash, log, checkpoint_fout, strategy="accurate"):
        super().__init__(rows, columns, fullness, sample_z, hash, log, checkpoint_fout)

        if strategy == "accurate":
            self.init_val = init_val
            self.lrfu_strategy = lrfu_strategy
        else:
            self.init_val = approximated_init_val
            self.lrfu_strategy = approximated_lrfu_strategy


    def insert_inbound(self, timestamp, flow_key, flow_val):
        self.sta_total_pkt += 1
        self.sta_counter += 1

        flow_val = self.init_val(flow_val)

        index = self.hash(flow_key) % self.rows

        matched = 0

        for i in range(self.columns):
            if self.array[i][index][0] == flow_key:
                # pdb.set_trace()
                self.sta_hit += 1
                matched = 1

                old_val = self.array[i][index][1]
                self.array[i][index][1] = self.lrfu_strategy(old_val, flow_val)
                if old_val < self.threshold and self.array[i][index][1] >= self.threshold:
                    self.pkt_counter_remain -= 1

            else:
                if self.array[i][index][1] < self.threshold:
                    self.array[i][index][2] |= 1

        if self.pkt_counter_remain <= 0 and (self.phase % 2) == 0:
            # pdb.set_trace()
            print("Sampling on phase {}. Counter: {}".format(self.phase + 1, self.sta_counter))
            self.save_log()
            self.phase += 1
            self.sta_complete_sample.append(0)

        if self.pkt_counter_remain <= 0 and random.random() < self.sample_prob and self.sample_counter < self.sample_z:
            self.simulator.schedule(DATA_PLANE_OP, self.sample, flow_key, flow_val)
        
            self.sta_complete_sample[-1] += 1

        if matched == 0:
            self.simulator.schedule(BACKEND_OP, self.insert_outbound, flow_key, flow_val)


    def insert_outbound(self, timestamp, flow_key, flow_val):
        # pdb.set_trace()

        if (flow_val < self.threshold):
            return    
        
        index = self.hash(flow_key) % self.rows
        for i in range(self.columns):
            if self.array[i][index][0] == flow_key:
                if self.array[i][index][1] < self.threshold:
                    self.pkt_counter_remain -= 1
                self.array[i][index][1] = self.lrfu_strategy(self.array[i][index][1], flow_val)
                self.array[i][index][2] = 0
                
                if self.phase & 1 == 1:
                    self.bitmap = utils.set_bitmap_one(self.bitmap, index, i, self.rows, self.columns)
                else:
                    self.bitmap[self.pkt_counter_remain % self.bitmap_len] = 0
                break


            if self.array[i][index][2] & 1 == 1: # pre-cleared slot
                self.array[i][index][0] = flow_key
                self.array[i][index][1] = flow_val
                self.array[i][index][2] = 0
                self.pkt_counter_remain -= 1

                if self.phase & 1 == 1:
                    self.bitmap = utils.set_bitmap_one(self.bitmap, index, i, self.rows, self.columns)
                else:
                    self.bitmap[self.pkt_counter_remain % self.bitmap_len] = 0
                break


class Squid_DP_Hashing(Squid_DP):
    def __init__(self, rows, columns, fullness, sample_z, hash, log, checkpoint_fout):
        super().__init__(rows, columns, fullness, sample_z, hash, log, checkpoint_fout)
        self.hash_map = []
        for i in range(self.n):
            self.hash_map.append(0)
        
        

    def insert_inbound(self, timestamp, flow_key, flow_val):
        self.sta_total_pkt += 1
        self.sta_counter += 1

        index = self.hash(flow_key) % self.n
        index = self.hash_map[index]

        matched = 0

        for i in range(self.columns):
            if self.array[i][index][0] == flow_key:
                # pdb.set_trace()
                self.sta_hit += 1
                matched = 1

                if self.array[i][index][1] < self.threshold and flow_val >= self.threshold:
                    self.pkt_counter_remain -= 1
                self.array[i][index][1] = flow_val

            else:
                if self.array[i][index][1] < self.threshold:
                    self.array[i][index][1] |= 1

        if self.pkt_counter_remain <= 0 and (self.phase % 2) == 0:
            print("Sampling on phase {}. Counter: {}".format(self.phase + 1, self.sta_counter))
            self.save_log()
            self.phase += 1
            self.sta_complete_sample.append(0)

        if self.pkt_counter_remain <= 0 and random.random() < self.sample_prob and self.sample_counter < self.sample_z:
            self.simulator.schedule(DATA_PLANE_OP, self.sample, flow_key, flow_val)
        
            self.sta_complete_sample[-1] += 1

        if matched == 0:
            self.simulator.schedule(BACKEND_OP, self.insert_outbound, flow_key, flow_val)


    def insert_outbound(self, timestamp, flow_key, flow_val):
        if (flow_val < self.threshold):
            return
        
        index = self.hash(flow_key) % self.n

        self.hash_map[index] = random.randint(0, self.rows - 1)
        index = self.hash_map[index]

        for i in range(self.columns):
            if self.array[i][index][0] == flow_key:
                if self.array[i][index][1] < self.threshold:
                    self.pkt_counter_remain -= 1
                self.array[i][index][1] = flow_val
                
                if self.phase & 1 == 1:
                    self.bitmap = utils.set_bitmap_one(self.bitmap, index, i, self.rows, self.columns)
                else:
                    self.bitmap[self.pkt_counter_remain % self.bitmap_len] = 0
                break

            if self.array[i][index][1] & 1 == 1:
                # pdb.set_trace()
                self.array[i][index][0] = flow_key
                self.array[i][index][1] = flow_val
                self.pkt_counter_remain -= 1

                if self.phase & 1 == 1:
                    self.bitmap = utils.set_bitmap_one(self.bitmap, index, i, self.rows, self.columns)
                else:
                    self.bitmap[self.pkt_counter_remain % self.bitmap_len] = 0
                break
