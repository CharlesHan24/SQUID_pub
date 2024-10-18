from collections import OrderedDict
import const
import pdb
import sortedcontainers
import math
import utils
import random
import copy

def lrfu_strategy(old_val, new_val):
    dif = old_val - new_val
    if dif > 200:
        return old_val
    elif dif < -200:
        return new_val
    else:
        return new_val + math.log(math.exp(dif) + 1)

def lru_strategy(old_val, new_val):
    return new_val

def init_val(timestamp, c=0.95):
    timestamp += 1
    # c = 0.99999 # S2
    # c = 0.95
    return -math.log(c) * timestamp

class D_ray_dp_sampling_backup(object):
    def __init__(self, rows, columns, data_gen, log, update_strategy=lrfu_strategy, init_strategy=init_val, nsamples=16):
        self.data_gen = data_gen
        self.rows = rows
        self.columns = columns
        self.timestamp = 0
        self.log = log
        self.update_strategy = update_strategy
        self.init_strategy = init_strategy
        self.array = [[[0, 0] for j in range(columns)] for i in range(rows)]
        self.nsamples = nsamples
        self.samples = [0 for i in range(nsamples)]
        self.sample_pointer = 0

        self.steps_per_sample = 100
        self.quantile_percent = 0.6
        self.quantile = int(self.nsamples * self.quantile_percent)
        self.hash = utils.bobhash2
        self.water_level = 1

    def calc_real_quantile(self, water_level, arr):
        tot_surpassed = 0
        for i in range(self.rows):
            for j in range(self.columns):
                if water_level < arr[i][j][1]:
                    tot_surpassed += 1

        return tot_surpassed / float(self.rows * self.columns)

    def run(self):
        i = 0
        hit = 0
        total_error = 0
        total_measurement = 0
        while True:
            ts, key, val = next(self.data_gen)
            if ts == const.INF_TIME:
                break
            i += 1
            self.timestamp += 1
            init_score = self.init_strategy(self.timestamp)

            index = self.hash(key) % self.rows
            sample_index = random.randint(0, self.rows - 1)
            sample_d = random.randint(0, self.columns - 1)
            sampled_score = self.array[sample_index][sample_d][1]

            is_hit = False

            for d in range(self.columns):
                if key == self.array[index][d][0] and self.array[index][d][1] != 0:
                    hit += 1
                    is_hit = True
                    self.array[index][d][1] = self.update_strategy(self.array[index][d][1], init_score)
                    
            
            if is_hit == False: # passive_eviction
                for d in range(self.columns):
                    if self.array[index][d][1] < self.water_level:
                        self.array[index][d][0] = key
                        self.array[index][d][1] = init_score
                        break

            if i % self.steps_per_sample == 0:
                # pdb.set_trace()
                self.sample_pointer = (self.sample_pointer + 1) % self.nsamples
                self.samples[self.sample_pointer] = sampled_score

                if self.sample_pointer == 0:
                    samples = copy.deepcopy(self.samples)
                    samples = sorted(samples, reverse=True)
                    self.water_level = max(self.water_level, samples[self.quantile])

                    real_quantile = self.calc_real_quantile(self.water_level, self.array)
                    # pdb.set_trace()
                    total_measurement += 1
                    total_error += abs(real_quantile - self.quantile_percent) / self.quantile_percent

            if i % 1000000 == 0:
                print(i, total_error / total_measurement)
                self.log.write("{}/{}\n".format(hit, i))
                self.log.flush()



class D_ray_dp_sampling(object):
    def __init__(self, rows, columns, data_gen, log, data_name=None, update_strategy=lrfu_strategy, init_strategy=init_val, nsamples=16):
        self.data_gen = data_gen
        self.rows = rows
        self.columns = columns
        self.timestamp = 0
        self.log = log
        self.update_strategy = update_strategy
        self.init_strategy = init_strategy
        self.array = [[[0, 0, 0] for j in range(columns)] for i in range(rows)]
        self.nsamples = nsamples
        self.samples = [0 for i in range(nsamples)]
        self.sample_pointer = 0

        self.steps_per_sample = 100
        self.quantile_percent = 0.33
        self.quantile = int(self.nsamples * self.quantile_percent)
        self.hash = utils.bobhash2
        self.water_level = 1
        if data_name == None:
            self.c = 0.95
        else:
            self.c = 0.99999
        

    def calc_real_quantile(self, water_level, arr):
        tot_surpassed = 0
        for i in range(self.rows):
            for j in range(self.columns):
                if water_level < arr[i][j][1]:
                    tot_surpassed += 1

        return tot_surpassed / float(self.rows * self.columns)

    def run(self):
        
        i = 0
        hit = 0
        total_error = 0
        total_measurement = 0
        while True:
            
            ts, key, val = next(self.data_gen)
            if ts == const.INF_TIME:
                break
            i += 1
            self.timestamp += 1
            init_score = self.init_strategy(self.timestamp, self.c)

            index = self.hash(key) % self.rows
            sample_index = random.randint(0, self.rows - 1)
            sample_d = random.randint(0, self.columns - 1)
            sampled_score = self.array[sample_index][sample_d][1]

            is_hit = False

            for d in range(self.columns):
                if key == self.array[index][d][0] and self.array[index][d][1] != 0:
                    
                    is_hit = True
                    
                    self.array[index][d][1] = self.update_strategy(self.array[index][d][1], init_score)
                    if self.array[index][d][2] <= self.timestamp:
                        hit += 1
                    
            
            if is_hit == False: # passive_eviction
                min_d = -1
                min_val = 1e9

                for d in range(self.columns):
                    if self.array[index][d][1] <= self.water_level:
                        if min_val > self.array[index][d][1]:
                            min_val = self.array[index][d][1]
                            min_d = d

                if min_d != -1:
                    self.array[index][min_d][0] = key
                    self.array[index][min_d][1] = init_score
                    self.array[index][min_d][2] = self.timestamp + 250

                # for d in range(self.columns):
                #     if self.array[index][d][1] <= self.water_level:
                #         self.array[index][d][0] = key
                #         self.array[index][d][1] = init_score
                #         self.array[index][d][2] = self.timestamp + 250 # the cached item is only available after querying the backend server
                #         break

            if i % self.steps_per_sample == 0:
                # pdb.set_trace()
                self.sample_pointer = (self.sample_pointer + 1) % self.nsamples
                self.samples[self.sample_pointer] = sampled_score

                if self.sample_pointer == 0:
                    samples = copy.deepcopy(self.samples)
                    samples = sorted(samples, reverse=True)
                    self.water_level = samples[self.quantile] * 1.00001# = max(self.water_level, samples[self.quantile]) * 1.000001

                    real_quantile = self.calc_real_quantile(self.water_level, self.array)
                    # pdb.set_trace()
                    total_measurement += 1
                    total_error += abs(real_quantile - self.quantile_percent) / self.quantile_percent

            if i % 1000000 == 0:
                # if i == 5000000:
                #     pdb.set_trace()
                sampled_scores = []
                for k in range(16):
                    sample_index = random.randint(0, self.rows - 1)
                    sample_d = random.randint(0, self.columns - 1)
                    sampled_score = self.array[sample_index][sample_d][1]
                    sampled_scores.append(sampled_score)
                # pdb.set_trace()
                print(i, total_error / total_measurement)
                
                self.log.write("Error: {}\n".format(total_error / total_measurement))
                self.log.write("{}/{}\n".format(hit, i))
                self.log.flush()

                total_error = 0
                total_measurement = 0
