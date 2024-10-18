from numpy import insert
import const
import squid_cp
import squid_dp
import utils
import heapq
import pdb

class Sw_Config(object):
    def __init__(self, delay_recirc, delay_backend, bw_dp, delay_cpdp, bw_cp, cp_outbound_rate):
        # bw_dp: time to queue per item
        self.delay_recirc = delay_recirc
        self.delay_backend = delay_backend
        self.bw_dp = bw_dp
        self.delay_cpdp = delay_cpdp
        self.bw_cp = bw_cp
        self.cp_outbound_rate = cp_outbound_rate
    
class Sim_Events(object):
    def __init__(self, q_status, op, func, ts, *args):
        self.q_status = q_status
        self.op = op
        self.func = func
        self.args = args
        self.ts = ts
    
    def __lt__(self, other):
        return self.ts < other.ts


class Simulator(object):
    def __init__(self, rows, columns, fullness, sample_z, emptiness, delta, hash: str, log, data_plane_mode: str, control_plane_mode: str, switch_config: Sw_Config, checkpoint_fout):
        if hash == "crc32":
            self.hash = utils.crc32
        elif hash == "bobhash":
            
            self.hash = utils.bobhash2
        else:
            self.hash = utils.identity_hash
        

        if data_plane_mode == "best_effort":
            self.dataplane = squid_dp.Squid_DP_Best_Effort(rows, columns, fullness, sample_z, self.hash, log, checkpoint_fout)
        elif data_plane_mode == "mapping":
            self.dataplane = squid_dp.Squid_DP_Hashing(rows, columns, fullness, sample_z, self.hash, log, checkpoint_fout)
        elif data_plane_mode == "best_effort_lrfu_accurate":
            self.dataplane = squid_dp.Squid_DP_Best_Effort_LRFU(rows, columns, fullness, sample_z, self.hash, log, checkpoint_fout)
        elif data_plane_mode == "best_effort_lrfu_approximated":
            self.dataplane = squid_dp.Squid_DP_Best_Effort_LRFU(rows, columns, fullness, sample_z, self.hash, log, checkpoint_fout, strategy="approximated")
        else:
            print("Error: data plane mode not understood")
            exit(0)

        if control_plane_mode == "las_vagas":
            self.controller = squid_cp.Squid_CP_Las_Vagas(rows, columns, fullness, sample_z, emptiness, delta, log)
        elif control_plane_mode == "monte_carlo":
            self.controller = squid_cp.Squid_CP_Monte_Carlo(rows, columns, fullness, sample_z, emptiness, delta, log)
        else:
            print("Error: control plane mode not understood")
            exit(0)

        self.dataplane.install_controller_simulator(self, self.controller)
        self.controller.install_dataplane_simulator(self, self.dataplane)
        self.switch_config = switch_config

        self.events_queue = []

        self.dp_queue = 0
        self.cp_inb_queue = 0
        self.cp_outb_queue = 0
        self.cur_ts = 0

        self.src_status = const.DATA_PLANE_OP

    def dp_recirc_pb_ts(self, cur_ts):
        cur_ts += self.switch_config.delay_recirc
        self.dp_queue = max(cur_ts, self.dp_queue) + self.switch_config.bw_dp
        return self.dp_queue

    def outb_ts(self, cur_ts):
        self.cp_outb_queue = max(self.cp_outb_queue, cur_ts) + self.switch_config.cp_outbound_rate
        return self.cp_outb_queue
    
    def dp_pb_ts(self, cur_ts):
        self.dp_queue = max(cur_ts, self.dp_queue) + self.switch_config.bw_dp
        return self.dp_queue
    
    def cpdp_pb_ts(self, cur_ts):
        cur_ts += self.switch_config.delay_cpdp
        self.dp_queue = max(cur_ts, self.dp_queue) + self.switch_config.bw_dp
        return self.dp_queue
    
    def dpcp_pb_ts(self, cur_ts):
        cur_ts += self.switch_config.delay_cpdp
        self.cp_inb_queue = max(cur_ts, self.cp_inb_queue) + self.switch_config.bw_cp
        return self.cp_inb_queue

        
    def request(self, op, field_str):
        assert(op == const.DATA_PLANE_OP)

        if field_str == "array":
            return self.dataplane.array
        elif field_str == "phase":
            return self.dataplane.phase
        elif field_str == "threshold":
            return self.dataplane.threshold
        elif field_str == "bitmap":
            return self.dataplane.bitmap
        else:
            print("Bug detected: unexpected request.")

    def run(self, data):
        for item in data:
            heapq.heappush(self.events_queue, Sim_Events(0, const.DATA_PLANE_OP, self.dataplane.insert_inbound, item[0], item[1], item[2]))
        
        while len(self.events_queue) > 0:
            item: Sim_Events = heapq.heappop(self.events_queue)
            self.cur_ts = item.ts
            if item.q_status == 1:
                self.src_status = item.op
                item.func(self.cur_ts, *item.args)
            else:
                assert(item.op == const.DATA_PLANE_OP)
                new_ts = self.dp_pb_ts(self.cur_ts)
                heapq.heappush(self.events_queue, Sim_Events(1, const.DATA_PLANE_OP, item.func, new_ts, item.args))
    
    def run_generator(self, data):
        heapq.heappush(self.events_queue, Sim_Events(1, const.DATA_PLANE_OP, None, const.INF_TIME, None))

        next_data = next(data)
        
        # pdb.set_trace()
        while len(self.events_queue) > 0:
            item: Sim_Events = self.events_queue[0]
            if next_data[0] == const.INF_TIME and item.ts == const.INF_TIME: # simulation ends
                return

            if next_data[0] < item.ts:
                new_ts = self.dp_pb_ts(next_data[0])
                heapq.heappush(self.events_queue, Sim_Events(1, const.DATA_PLANE_OP, self.dataplane.insert_inbound, new_ts, next_data[1], next_data[2]))
                self.cur_ts = new_ts

                next_data = next(data)

            else:
                heapq.heappop(self.events_queue)
                self.cur_ts = item.ts
                self.src_status = item.op
                item.func(self.cur_ts, *item.args)
            
    def schedule(self, op, func, *args):
        if op == const.BACKEND_OP:
            new_ts = self.switch_config.delay_backend + self.cur_ts
        elif op == const.DATA_PLANE_OP and self.src_status == const.DATA_PLANE_OP:
            new_ts = self.dp_recirc_pb_ts(self.cur_ts)
        elif op == const.CONTROL_PLANE_OP and self.src_status == const.DATA_PLANE_OP:
            new_ts = self.dpcp_pb_ts(self.cur_ts)
        elif op == const.DATA_PLANE_OP and self.src_status == const.CONTROL_PLANE_OP:
            new_ts = self.outb_ts(self.cur_ts)
            new_ts = self.cpdp_pb_ts(new_ts)
        else:
            print("Invalid scheduling operation. Scheduling a CP operation after a CP operation is not allowed (and can be combined as a single event).")


        heapq.heappush(self.events_queue, Sim_Events(1, op, func, new_ts, *args))
    