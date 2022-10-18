from typing import List
from const import DATA_PLANE_OP
import utils
import math
import pdb

class Squid_CP_Las_Vagas(object):
    def __init__(self, rows, columns, fullness, sample_z, emptyness, delta, log):
        self.log = log
        self.rows = rows
        self.columns = columns
        self.n = self.rows * self.columns
        self.sample_z = sample_z
        self.rcv_cnt = 0

        self.fullness = int(self.rows * self.columns * fullness) # how many items are there when we identify the arrays as "full"
        self.emptyness = int(self.rows * self.columns * emptyness)
        self.delta = int(self.rows * self.columns * delta)

        self.buffer = [[], []] # two empty buffers
        self.old_threshold = 1
    
    def install_dataplane_simulator(self, simulator, dataplane):
        self.simulator = simulator
        self.dataplane = dataplane

    def accept_sample(self, timestamp, sample_item: List[int]):
        self.rcv_cnt += 1
        if self.rcv_cnt * 2 <= self.sample_z:
            self.buffer[0].append(sample_item[1] & 0xfffffffe)
        else:
            self.buffer[1].append(sample_item[1] & 0xfffffffe)
            self.buffer[0].append(sample_item[1] & 0xfffffffe)

        if self.rcv_cnt == self.sample_z: # full
            self.buffer[0] = sorted(self.buffer[0], reverse=True)
            self.buffer[1] = sorted(self.buffer[1], reverse=True)

            k1 = int(self.sample_z * (self.emptyness + self.delta / 2.5) / self.n)
            if k1 >= len(self.buffer[0]): # should never happen
                k1 = len(self.buffer[0]) - 1
            self.threshold1 = max(self.buffer[0][k1], self.old_threshold)

            k2 = int(self.sample_z // 2 * (self.emptyness + self.delta / 2.5) / self.n)
            if k2 >= len(self.buffer[1]): # should never happen
                k2 = len(self.buffer[1]) - 1
            self.threshold2 = max(self.buffer[1][k2], self.old_threshold)

            block_size = int(math.sqrt(self.rows))
            ptr = 0
            while ptr < self.rows:
                self.simulator.schedule(DATA_PLANE_OP, self.dataplane.verify, ptr, min(ptr + block_size, self.rows), self.threshold1, self.threshold2, 0, 0)

                ptr += block_size
            
            self.rcv_cnt = 0
            self.verify_rcv_cnt = 0
            self.counter1 = 0
            self.counter2 = 0

    def verify(self, timestamp, counter1, counter2):
        self.counter1 += counter1
        self.counter2 += counter2
        self.verify_rcv_cnt += 1
        if self.verify_rcv_cnt == int(math.sqrt(self.rows)):

            if self.threshold1 < self.threshold2:
                self.threshold1, self.threshold2 = self.threshold2, self.threshold1
                self.counter1, self.counter2 = self.counter2, self.counter1

            # pdb.set_trace()
            
            if self.counter1 > self.emptyness + self.delta:
                self.log.write("Control plane: failed\n")
                threshold, minus_cnt = self.slow_cp_op()
            elif self.counter2 < self.emptyness:
                self.log.write("Control plane: failed\n")
                threshold, minus_cnt = self.slow_cp_op()
            elif self.counter1 >= self.emptyness:
                self.log.write("Control plane: success\n")
                threshold = self.threshold1 # always pickup the large
                minus_cnt = self.fullness - self.counter1
            else:
                self.log.write("Control plane: success\n")
                threshold = self.threshold2
                minus_cnt = self.fullness - self.counter2
            
            self.old_threshold = threshold
        
            self.simulator.schedule(DATA_PLANE_OP, self.dataplane.adjust, threshold, minus_cnt)
        
    def slow_cp_op(self):
        arrays = self.simulator.request(DATA_PLANE_OP, "array")
        bitmap = self.simulator.request(DATA_PLANE_OP, "bitmap")
        phase = self.simulator.request(DATA_PLANE_OP, "phase")
        threshold = self.simulator.request(DATA_PLANE_OP, "threshold")
        items = []
        for i in range(self.columns):
            for j in range(self.rows):
                if utils.check_bitmap_digit(bitmap, j, i) != 0: # bitmap[j >> 4] & (1 << (((j & 15) << 2) + i)) == 1:
                    items.append(0)
                else:
                    items.append(arrays[i][j][1] & 0xfffffffe)
        
        items = sorted(items, reversed=True)
        return items[self.emptyness - 1], self.fullness - self.emptyness
    

class Squid_CP_Monte_Carlo(object):
    def __init__(self, rows, columns, fullness, sample_z, emptyness, delta, log):
        self.log = log
        self.rows = rows
        self.columns = columns
        self.n = self.rows * self.columns
        self.sample_z = sample_z
        self.rcv_cnt = 0

        self.fullness = int(self.rows * self.columns * fullness) # how many items are there when we identify the arrays as "full"
        self.emptyness = int(self.rows * self.columns * emptyness)
        self.delta = int(self.rows * self.columns * delta)

        self.buffer = [] # two empty buffers
    
    def install_dataplane_simulator(self, simulator, dataplane):
        self.simulator = simulator
        self.dataplane = dataplane

    def accept_sample(self, timestamp, sample_item: List[int]):
        self.rcv_cnt += 1
        
        self.buffer.append(sample_item[1])

        if self.rcv_cnt == self.sample_z: # full
            self.buffer = sorted(self.buffer, reverse=True)

            k = int(self.sample_z * (self.emptyness + self.delta / 2.5) / self.n)
            if k >= len(self.buffer): # should never happen
                k = len(self.buffer) - 1
            self.threshold = self.buffer[k]

            self.simulator.schedule(DATA_PLANE_OP, self.dataplane.adjust, self.threshold, self.fullness - self.emptyness - self.delta // 2)
            
            self.rcv_cnt = 0
