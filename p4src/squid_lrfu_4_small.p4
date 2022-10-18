#include <core.p4>
#if __TARGET_TOFINO__ == 3
#include <t3na.p4>
#elif __TARGET_TOFINO__ == 2
#include <t2na.p4>
#else
#include <tna.p4>
#endif

// TODO: split one array with 65536 items into two arrays with 32768 items each
/*
 * 0/1: Normal packet ----> forwarding packet + (if need sample) resample packet. 0: in, 1: out
 * resubmit: resample packet ----> cpu_packet
 * 2: adjust_counter packet ----> adjust counter + drop
 * (Control plane) read all info of recorded elements in registers; write the threshold value
 *
 * 0.3 full: q-max
 * 0.6 full: start sampling && all incoming packets regarded as "next-round packets"
 * after sampling (expected 0.7): start verification
 * 
 * insertion: p = rand(); for (i = 0; i < 4; i++){if (blank(p)) {insertion(p); break;}}
 * sampling: randomly pick one register and then randomly pick one position. 
 * Do sampling if counter < Z
 * Given Z: do sampling every 0.1n * 0.6 / Z packets
 * Z = 1000
 *
 * For Caching algorithm, on the query
 */

typedef bit<48> mac_addr_t;
typedef bit<32> ipv4_addr_t;
typedef bit<16> eth_type_t;

const bit<32> NEG_ONE = 4294967295;

const eth_type_t ETH_IPV4 = 16w0x0800;

typedef bit<8> ip_proto_t;

const ip_proto_t IP_PROTO_TCP = 6;
const ip_proto_t IP_PROTO_UDP = 17;

typedef bit<8> ctrl_proto_t;
const ctrl_proto_t CTRL_INBOUND = 0;
const ctrl_proto_t CTRL_OUTBOUND = 1;
const ctrl_proto_t CTRL_ADJUST = 2;
const ctrl_proto_t CTRL_REINSERT_SAMPLE = 3;

#define ARRAY_LEN 32768 // 0.256M items
#define COLUMN_CNT 4 // upscaled to power of 2
#define ARRAY_BITWIDTH 16
#define COLUMN_BITWIDTH 2
#define FULL_CNT 153600 // 60% * 0.256M
#define FULL_CNT_MINUS_ONE 153599
#define SAMPLE_Z 2000  // number of 
#define SAMPLE_PROB 9362 // (2^16) / 7
#define RECIRC_PORT 68

#define SAMPLE_COL_THRES1 16384
#define SAMPLE_COL_THRES2 32768
#define SAMPLE_COL_THRES3 49152


header ethernet_t {
    mac_addr_t dst_addr;
    mac_addr_t src_addr;
    eth_type_t ethertype;
}

header ipv4_t {
    bit<4>    version;
    bit<4>    ihl;
    bit<6>    dscp;
    bit<2>    ecn;
    bit<16>   total_len;
    bit<16>   identification;
    bit<3>    flags;
    bit<13>   frag_offset;
    bit<8>    ttl;
    bit<8>    protocol;
    bit<16>   hdr_checksum;
    ipv4_addr_t src;
    ipv4_addr_t dst;
}

header tcp_t {
    bit<16> src_port;
    bit<16> dst_port;
    
    bit<32> seq_no;
    bit<32> ack_no;
    bit<4> data_offset;
    bit<4> res;
    bit<8> flags;
    bit<16> window;
    bit<16> checksum;
    bit<16> urgent_ptr;
}

header udp_t {
    bit<16> src_port;
    bit<16> dst_port;
    bit<16> udp_total_len;
}

header ctrl_t {
    ctrl_proto_t proto;
}

header adjust_t {
    int<32> minus_cnt;
}

header cpu_t {
    int<32> score;
    bit<8> is_post_sample;
}


header flow_info_t {
    int<32> key;
    bit<32> val;
    int<32> score;
}

header ig_eg_info_t{
    bit<32> affected_col;
    int<32> inbound_old_val;
    bit<16> index;
    bit<32> sample_column;
    int<32> pkt_counter;
}

struct item_t {
    int<32> key;
    int<32> score;
}

struct header_t{
    ethernet_t ethernet;
    ipv4_t ipv4;
    tcp_t tcp;
    udp_t udp;
    ctrl_t control_hdr;
    flow_info_t flow_info;
    adjust_t adjust;
    ig_eg_info_t ig_eg_info;
    cpu_t cpu;
}

struct ig_metadata_t {
    bit<32> threshold;
    int<32> sample_col_thres1;
    int<32> sample_col_thres2;
    int<32> sample_col_thres3;

    int<32> cur_res;
    int<32> cond;
    bit<16> sample_idx;
    int<32> sample_cnt;
    bit<32> sample_column;
    
    int<32> cur_pkt_counter;

    bit<16> is_sample;
    int<32> qmax_array_md1;
    int<32> qmax_array_md2;
}

parser TofinoIngressParser(
        packet_in pkt,
        out ingress_intrinsic_metadata_t ig_intr_md) {
    state start {
        pkt.extract(ig_intr_md);

        transition select(ig_intr_md.resubmit_flag) {
            1 : parse_resubmit;
            0 : parse_port_metadata;
        }
    }

    state parse_resubmit {
        // Parse resubmitted packet here.
        // pkt.advance(64); 
        // pkt.extract(ig_md.resubmit_data_read);
        transition accept;
    }

    state parse_port_metadata {
        transition accept;
    }
}

parser SwitchIngressParser(
        packet_in pkt,
        out header_t hdr,
        out ig_metadata_t ig_md,
        out ingress_intrinsic_metadata_t ig_intr_md) {

    TofinoIngressParser() tofino_parser;

    state start {
        ig_md.threshold = 0;
        ig_md.cond = -1;

        ig_md.sample_idx = 0;
        ig_md.sample_cnt = 0;
        ig_md.sample_column = 0;

        ig_md.cur_pkt_counter = 0;
        ig_md.sample_col_thres1 = 0;
        ig_md.sample_col_thres2 = 0;
        ig_md.sample_col_thres3 = 0;

        ig_md.qmax_array_md1 = 0;
        ig_md.qmax_array_md2 = 0;

        ig_md.is_sample = 0;
        ig_md.cur_res = 0;

        tofino_parser.apply(pkt, ig_intr_md);

        transition parse_ethernet;
    }
    
    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        transition select (hdr.ethernet.ethertype) {
            ETH_IPV4: parse_ipv4;
            default: reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        transition select (hdr.ipv4.protocol) {
            IP_PROTO_UDP: parse_udp;
            IP_PROTO_TCP: parse_tcp;
            default: reject;
        }
    }
    
    state parse_udp {
        pkt.extract(hdr.udp);
        transition parse_control;
    }

    state parse_tcp {
        pkt.extract(hdr.tcp);
        
        transition parse_control;
    }

    state parse_control {
        pkt.extract(hdr.control_hdr);
        transition select (hdr.control_hdr.proto){
            CTRL_INBOUND: parse_traffic_info;
            CTRL_OUTBOUND: parse_traffic_info;
            CTRL_ADJUST: parse_adjust;
            default: reject;
        }
    }

    state parse_traffic_info {
        pkt.extract(hdr.flow_info);
        transition accept;
    }

    state parse_adjust {
        pkt.extract(hdr.adjust);
        transition accept;
    }
}

control SwitchIngress(inout header_t hdr,
                inout ig_metadata_t ig_md,
                in ingress_intrinsic_metadata_t ig_intr_md,
                in ingress_intrinsic_metadata_from_parser_t ig_intr_prsr_md,
                inout ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md,
                inout ingress_intrinsic_metadata_for_tm_t ig_intr_tm_md) {
    int<32> cond = 0;
    Hash<bit<16> >(HashAlgorithm_t.CRC32) insert_idx_hash;
    Random<bit<16> >() sample_idx_random;
    Random<bit<16> >() sample_column_random;
    Random<bit<16> >() need_sample_random;

    action pkt_drop() {
        ig_intr_dprsr_md.drop_ctl = 1;
        exit;
    }
    
    Register<int<32>, bit<1> >(32w1) score_counter;
    RegisterAction<int<32>, bit<1>, int<32> >(score_counter) score_counter_increment_action = {
        void apply(inout int<32> value, out int<32> read_value) {
            value = value + 2; // for c = 0.75, score = 2/(-log(c)) * (-log(c)) i
            read_value = value;
        }
    };

    Register<int<32>, bit<1> >(32w1) pkt_counter;
    RegisterAction<int<32>, bit<1>, int<32> >(pkt_counter) pkt_counter_decrement_action = {
        void apply(inout int<32> value, out int<32> read_value) {
            if ((ig_md.cur_res != 0) && (ig_md.cond <= 0)) {
                value = value - 1;   
            }
            read_value = value;
        }
    };
    RegisterAction<int<32>, bit<1>, void >(pkt_counter) pkt_counter_adjust_action = {
        void apply(inout int<32> value) {
            value = value + ig_md.cur_res;//hdr.adjust.minus_cnt;
        }
    };

    Register<int<32>, bit<1> >(32w1) sample_cnt; // read value always subtracted by sample_z
    RegisterAction<int<32>, bit<1>, int<32> >(sample_cnt) sample_cnt_decrement_action = {
        void apply(inout int<32> value, out int<32> read_value) {
            if ((ig_md.is_sample < SAMPLE_PROB) && (ig_md.cur_pkt_counter <= 0)) {
                read_value = value;
                value = value - 1;
            }
            else{
                read_value = 0; // 0: enough samples or do not sample at this time
            }
        }
    };
    RegisterAction<int<32>, bit<1>, int<32> >(sample_cnt) sample_cnt_clear_action = {
        void apply(inout int<32> value, out int<32> read_value) {
            read_value = 0;
            value = SAMPLE_Z;
        }
    };


    #define GEN_QMAX_ARRAY(id)     \
    RegisterParam<int<32> >(0) qmax_array_threshold## id ##; \
    Register<item_t, bit<16> >(ARRAY_LEN) qmax_array## id ##; \
    RegisterAction<item_t, bit<16>, int<32> >(qmax_array## id ##) qmax_array## id ##_insert_action = { \
        void apply(inout item_t value, out int<32> read_value) { \
            if ((value.key == ig_md.qmax_array_md1) || (value.score < qmax_array_threshold## id ##.read())) { \
                read_value = 1; \
                value.key = ig_md.qmax_array_md1; \
                value.score = ig_md.qmax_array_md2; \
            } \
            else{ \
                read_value = 0;\
            } \
        } \
    }; \
    RegisterAction<item_t, bit<16>, int<32> >(qmax_array## id ##) qmax_array## id ##_query_score_action = { \
        void apply(inout item_t value, out int<32> read_value) { \
            read_value = value.score; \
        } \
    }; \
    RegisterAction<item_t, bit<16>, int<32> >(qmax_array## id ##) qmax_array## id ##_lookup_update_action = { \
        void apply(inout item_t value, out int<32> read_value) { \
            if (value.key == ig_md.qmax_array_md1) { \
                read_value = value.score; \
                if (value.score - ig_md.qmax_array_md2 < -5) { \
                    value.score = ig_md.qmax_array_md2; \
                } \
                else{ \
                    value.score = value.score + 5; \
                } \
            } \
            else{ \
                read_value = 0; \
            } \
        } \
    }; \

    // qmax_array_md1: threshold; new_phase
    // done

    GEN_QMAX_ARRAY(1)
    GEN_QMAX_ARRAY(2)
    GEN_QMAX_ARRAY(3)
    GEN_QMAX_ARRAY(4)

    #define GET_CONST_COND_IPV4(id) \
    action get_const_ipv4_action## id ##(bit<32> threshold, PortId_t port) { \
        ig_md.threshold = threshold; \
        ig_md.is_sample = need_sample_random.get(); \
        ig_intr_tm_md.ucast_egress_port = port; \
        ig_md.qmax_array_md1 = hdr.flow_info.key; \
        hdr.ig_eg_info.setValid(); \
        hdr.ig_eg_info.index = insert_idx_hash.get({hdr.flow_info.key}); \
        hdr.ig_eg_info.inbound_old_val = 0; \
        hdr.ig_eg_info.affected_col = 0; \
    } \
    table get_const_ipv4_tab## id ## { \
        key = { \
            hdr.ipv4.dst: lpm; \
        } \
        actions = { \
            get_const_ipv4_action## id ##; \
            pkt_drop; \
        } \
        size = 32; \
        default_action = pkt_drop(); \
    }

    GET_CONST_COND_IPV4(1)

    action init_sample_action(bit<32> threshold, PortId_t cpu_port){
        hdr.cpu.setValid();
        hdr.ig_eg_info.setValid();
        ig_md.threshold = threshold;
        ig_intr_tm_md.ucast_egress_port = cpu_port;
        ig_md.sample_idx = sample_idx_random.get();
        ig_md.sample_column[15:0] = sample_column_random.get();
    }

    table init_sample_tab{
        key = {
            hdr.ethernet.ethertype: exact;
        }
        actions = {
            init_sample_action;
            pkt_drop;
        }
        size = 2;
        default_action = pkt_drop();
    }

    action normal_need_resample_action(){
        ig_intr_dprsr_md.resubmit_type = 1;
    }
    table normal_need_resample_tab {
        key = {
            ig_md.sample_cnt: ternary; // > 0: sample, <= 0: do not sample
        }
        actions = {
            normal_need_resample_action;
            NoAction;
        }
        size = 3;
        default_action = NoAction;
    }

    action init_sample_column_action(){
        ig_md.sample_col_thres1 = (int<32>)ig_md.sample_column - SAMPLE_COL_THRES1;
        ig_md.sample_col_thres2 = (int<32>)ig_md.sample_column - SAMPLE_COL_THRES2;
        ig_md.sample_col_thres3 = (int<32>)ig_md.sample_column - SAMPLE_COL_THRES3;
        hdr.control_hdr.proto = CTRL_REINSERT_SAMPLE;
    }
    table init_sample_column_tab {
        key = {
            ig_intr_md.resubmit_flag: exact;
        }
        actions = {
            init_sample_column_action;
        }
        size = 1;
        default_action = init_sample_column_action();
    }

    apply {
        
        if (ig_intr_md.resubmit_flag == 1) {
            init_sample_tab.apply();
            
            init_sample_column_tab.apply();
            // ig_md.sample_col_thres1 = (int<32>)ig_md.sample_column - SAMPLE_COL_THRES1;
            // ig_md.sample_col_thres2 = (int<32>)ig_md.sample_column - SAMPLE_COL_THRES2;
            // ig_md.sample_col_thres3 = (int<32>)ig_md.sample_column - SAMPLE_COL_THRES3;
            // hdr.control_hdr.proto = CTRL_REINSERT_SAMPLE;


            if (ig_md.sample_col_thres1 >= 0) {
                hdr.cpu.score = qmax_array1_query_score_action.execute(ig_md.sample_idx);
                hdr.ig_eg_info.sample_column = 0;
            }
            else if (ig_md.sample_col_thres2 >= 0) {
                hdr.cpu.score = qmax_array2_query_score_action.execute(ig_md.sample_idx);
                hdr.ig_eg_info.sample_column = 1;
            }
            else if (ig_md.sample_col_thres3 >= 0) {
                hdr.cpu.score = qmax_array3_query_score_action.execute(ig_md.sample_idx);
                hdr.ig_eg_info.sample_column = 2;
            }
            else {
                hdr.cpu.score = qmax_array4_query_score_action.execute(ig_md.sample_idx);
                hdr.ig_eg_info.sample_column = 3;
            }
        }

        else if (hdr.control_hdr.proto == CTRL_ADJUST) {
            ig_md.cur_res = hdr.adjust.minus_cnt;
            
            pkt_counter_adjust_action.execute(0);
            
            sample_cnt_clear_action.execute(0);
            
            pkt_drop();
        }
        
        else {
            get_const_ipv4_tab1.apply();
            if (hdr.control_hdr.proto == CTRL_INBOUND) {
                ig_md.qmax_array_md2 = score_counter_increment_action.execute(0);
                
                ig_md.cur_res = qmax_array1_lookup_update_action.execute(hdr.ig_eg_info.index);
                if (ig_md.cur_res == 0){
                    ig_md.cur_res = qmax_array2_lookup_update_action.execute(hdr.ig_eg_info.index);
                }
                else{
                    hdr.ig_eg_info.affected_col = 1;
                }
                if (ig_md.cur_res == 0){
                    ig_md.cur_res = qmax_array3_lookup_update_action.execute(hdr.ig_eg_info.index);
                }
                else if (hdr.ig_eg_info.affected_col == 0){
                    hdr.ig_eg_info.affected_col = 2;
                }
                if (ig_md.cur_res == 0){
                    ig_md.cur_res = qmax_array4_lookup_update_action.execute(hdr.ig_eg_info.index);
                }
                else if (hdr.ig_eg_info.affected_col == 0){
                    hdr.ig_eg_info.affected_col = 3;
                }
                if ((ig_md.cur_res != 0) && (hdr.ig_eg_info.affected_col == 0)){
                    hdr.ig_eg_info.affected_col = 4;
                }

                ig_md.cond = ig_md.cur_res - (int<32>)ig_md.threshold;
                hdr.ig_eg_info.inbound_old_val = ig_md.cur_res - (int<32>)ig_md.threshold;
                if (ig_md.cur_res != 0){
                    ig_intr_tm_md.ucast_egress_port = ig_intr_md.ingress_port;
                }
            }
            else{
                ig_md.cur_res = qmax_array1_insert_action.execute(hdr.ig_eg_info.index);
                if (ig_md.cur_res == 0) {
                    ig_md.cur_res = qmax_array2_insert_action.execute(hdr.ig_eg_info.index);
                }
                else if (hdr.ig_eg_info.affected_col == 0){
                    hdr.ig_eg_info.affected_col = 1;
                }
                if (ig_md.cur_res == 0) {
                    ig_md.cur_res = qmax_array3_insert_action.execute(hdr.ig_eg_info.index);
                }
                else if (hdr.ig_eg_info.affected_col == 0){
                    hdr.ig_eg_info.affected_col = 2;
                }
                if (ig_md.cur_res == 0) {
                    ig_md.cur_res = qmax_array4_insert_action.execute(hdr.ig_eg_info.index);
                }
                else if (hdr.ig_eg_info.affected_col == 0){
                    hdr.ig_eg_info.affected_col = 3;
                }
                if ((ig_md.cur_res != 0) && (hdr.ig_eg_info.affected_col == 0)){
                    hdr.ig_eg_info.affected_col = 4;
                }
            }
            
            ig_md.cur_pkt_counter = pkt_counter_decrement_action.execute(0); // decrement condition: cur_res != 0 and cond <= 0
            hdr.ig_eg_info.pkt_counter = ig_md.cur_pkt_counter;

            ig_md.sample_cnt = sample_cnt_decrement_action.execute(0);

            normal_need_resample_tab.apply();
        }
    }
}


control SwitchIngressDeparser(
        packet_out pkt,
        inout header_t hdr,
        in ig_metadata_t ig_md,
        in ingress_intrinsic_metadata_for_deparser_t ig_intr_dprsr_md) {
    apply {
        pkt.emit(hdr);
    }
}

struct eg_metadata_t {
    bit<16> checksum;
    bit<32> bm_md1;
    bit<32> bm_md2;
    bit<32> bm_lookup_res;
    bit<32> index;
    bit<32> index1;
    bit<32> index2;
    bit<32> affected_col;
    bit<32> tmp_src;
    bit<48> tmp_eth_src;
}

parser SwitchEgressParser(
        packet_in pkt,
        out header_t hdr,
        out eg_metadata_t eg_md,
        out egress_intrinsic_metadata_t eg_intr_md) {

    Checksum () tcp_checksum;

    state start {
        pkt.extract(eg_intr_md);
        eg_md.bm_md1 = 0;
        eg_md.bm_md2 = 0;
        eg_md.bm_lookup_res = 0;
        eg_md.index = 0;
        eg_md.index1 = 0;
        eg_md.index2 = 1;
        eg_md.affected_col = 0;

        transition parse_ethernet;
    }
    
    state parse_ethernet {
        pkt.extract(hdr.ethernet);
        eg_md.tmp_eth_src = hdr.ethernet.src_addr;
        eg_md.checksum = 0;
        transition select (hdr.ethernet.ethertype) {
            ETH_IPV4: parse_ipv4;
            default: reject;
        }
    }

    state parse_ipv4 {
        pkt.extract(hdr.ipv4);
        eg_md.tmp_src = hdr.ipv4.src;
        transition select (hdr.ipv4.protocol) {
            IP_PROTO_UDP: parse_udp;
            IP_PROTO_TCP: parse_tcp;
            default: reject;
        }
    }
    
    state parse_udp {
        pkt.extract(hdr.udp);
        transition parse_control;
    }

    state parse_tcp {
        pkt.extract(hdr.tcp);
        tcp_checksum.subtract({hdr.tcp.checksum});

        eg_md.checksum = tcp_checksum.get();

        transition parse_control;
    }

    state parse_control {
        pkt.extract(hdr.control_hdr);
        tcp_checksum.subtract({hdr.control_hdr.proto});
        transition select (hdr.control_hdr.proto){
            CTRL_INBOUND: parse_traffic_info;
            CTRL_OUTBOUND: parse_traffic_info;
            CTRL_REINSERT_SAMPLE: parse_reinsert_sample;
            CTRL_ADJUST: reject; // should not reach here
            default: reject;
        }
    }

    state parse_traffic_info {
        pkt.extract(hdr.flow_info);
        pkt.extract(hdr.ig_eg_info);
        
        tcp_checksum.subtract({hdr.flow_info.key, hdr.flow_info.val});
        transition accept;
    }

    state parse_reinsert_sample {
        pkt.extract(hdr.ig_eg_info);
        pkt.extract(hdr.cpu);
        transition accept;
    }
}

control SwitchEgress(
        inout header_t hdr,
        inout eg_metadata_t eg_md,
        /* Intrinsic */    
        in    egress_intrinsic_metadata_t                  eg_intr_md,
        in    egress_intrinsic_metadata_from_parser_t      eg_intr_md_from_prsr,
        inout egress_intrinsic_metadata_for_deparser_t     eg_intr_md_for_dprsr,
        inout egress_intrinsic_metadata_for_output_port_t  eg_intr_md_for_oport){

    Register<bit<32>, bit<32> >((COLUMN_CNT * ARRAY_LEN) >> 5) post_sample_bitmap;
    RegisterAction<bit<32>, bit<32>, bit<32> >(post_sample_bitmap) bitmap_query = {
        void apply(inout bit<32> value, out bit<32> read_value) {
            bit<32> tmp;
            tmp = value & eg_md.bm_md2;
            read_value = tmp;
        }
    };
    RegisterAction<bit<32>, bit<32>, void>(post_sample_bitmap) bitmap_modify = {
        void apply(inout bit<32> value) {
            if (eg_md.bm_md1 + 1 == 0) {
                value = 0;
            }
            else {
                value = eg_md.bm_md2;
            }
        }
    };

    #define VALUE_CACHE_DEF(id) \
    Register<bit<32>, bit<32> >(ARRAY_LEN * 2) value_cache## id ##; \
    RegisterAction<bit<32>, bit<32>, void> (value_cache## id ##) value_cache## id ##_insert_action = { \
        void apply(inout bit<32> value) { \
            value = hdr.flow_info.val; \
        } \
    }; \
    RegisterAction<bit<32>, bit<32>, bit<32> >(value_cache## id ##) value_cache## id ##_query_action = { \
        void apply(inout bit<32> value, out bit<32> read_value) { \
            read_value = value; \
        } \
    }; \
    RegisterAction<bit<32>, bit<32>, bit<32> >(value_cache## id ##) value_cache## id ##_insertquery_action = { \
        void apply(inout bit<32> value, out bit<32> read_value) { \
            if (hdr.control_hdr.proto == CTRL_OUTBOUND){ \
                value = hdr.flow_info.val; \
            } \
            read_value = value; \
        } \
    };

    VALUE_CACHE_DEF(1)
    VALUE_CACHE_DEF(2)
    
    action init_sample_index_action(bit<32> bitmask) {
        eg_md.bm_md2 = bitmask;
        eg_md.index[ARRAY_BITWIDTH - 6:0] = hdr.ig_eg_info.index[ARRAY_BITWIDTH - 6:0];
        eg_md.index[ARRAY_BITWIDTH - 1:ARRAY_BITWIDTH - 5] = hdr.ig_eg_info.sample_column[4:0];
    }
    table init_sample_index_tab {
        key = {
            hdr.ig_eg_info.index: ternary;
        }
        actions = {
            init_sample_index_action;
        }
        size = 64;
        default_action = init_sample_index_action(0);
    }

    action init_normal_index_post_action(bit<1> shift, bit<32> bitmask) {
        eg_md.bm_md2 = bitmask;
        eg_md.index[ARRAY_BITWIDTH - 6:0] = hdr.ig_eg_info.index[ARRAY_BITWIDTH - 6:0];
        eg_md.index[ARRAY_BITWIDTH - 1:ARRAY_BITWIDTH - 5] = eg_md.affected_col[4:0];

        eg_md.index1[16:1] = hdr.ig_eg_info.index;
        eg_md.index1[0:0] = shift;
    }

    action init_normal_index_pre_action(bit<1> shift) {
        eg_md.index[ARRAY_BITWIDTH + COLUMN_BITWIDTH - 6: 0] = hdr.ig_eg_info.pkt_counter[ARRAY_BITWIDTH + COLUMN_BITWIDTH - 6: 0];
        eg_md.bm_md1 = NEG_ONE;
        eg_md.index1[16:1] = hdr.ig_eg_info.index;
        eg_md.index1[0:0] = shift;
    }

    table init_normal_index_tab {
        key = {
            hdr.ig_eg_info.index: ternary;
            hdr.ig_eg_info.pkt_counter: ternary;
            hdr.ig_eg_info.affected_col: exact;
        }
        actions = {
            init_normal_index_post_action;
            init_normal_index_pre_action;
        }
        size = 640;
        default_action = init_normal_index_post_action(0, 0);
    }

    action post_update_affected_action() {
        
        hdr.control_hdr.proto = CTRL_OUTBOUND;
        hdr.ipv4.src = hdr.ipv4.dst;
        hdr.ipv4.dst = eg_md.tmp_src;

        hdr.ethernet.src_addr = hdr.ethernet.dst_addr;
        hdr.ethernet.dst_addr = eg_md.tmp_eth_src;
    
        hdr.ig_eg_info.setInvalid();
    }
    action post_update_unaffected_action() {
        hdr.ig_eg_info.setInvalid();
    }

    table post_update_tab {
        key = {
            hdr.ig_eg_info.affected_col: ternary;
        }

        actions = {
            post_update_affected_action;
            post_update_unaffected_action;
        }

        size = 1;
        default_action = post_update_unaffected_action();
    }

    apply {
        eg_md.affected_col = hdr.ig_eg_info.affected_col - 1;
        if (hdr.control_hdr.proto == CTRL_REINSERT_SAMPLE) {
            init_sample_index_tab.apply();

            eg_md.bm_lookup_res = bitmap_query.execute(eg_md.index);

            if (eg_md.bm_lookup_res != 0){
                hdr.cpu.is_post_sample = 1;
            }
            else{
                hdr.cpu.is_post_sample = 0;
            }
            
            hdr.ig_eg_info.setInvalid();
            hdr.flow_info.setInvalid();
        }
        else{
            init_normal_index_tab.apply();
            if ((hdr.ig_eg_info.affected_col != 0) || (hdr.ig_eg_info.pkt_counter >= 0)) {
                bitmap_modify.execute(eg_md.index);
            }

            
            if ((hdr.ig_eg_info.affected_col == 1) || (hdr.ig_eg_info.affected_col == 2)){ // combined insertion / query
                @stage(6){
                    hdr.flow_info.val = value_cache1_insertquery_action.execute(eg_md.index1);
                }
            }
            
            else if ((hdr.ig_eg_info.affected_col == 3) || (hdr.ig_eg_info.affected_col == 4)){
                @stage(6){
                    hdr.flow_info.val = value_cache2_insertquery_action.execute(eg_md.index1);
                }
            }
            

            if (hdr.control_hdr.proto == CTRL_OUTBOUND) {
                hdr.ig_eg_info.setInvalid();
            }

            else { // inbound
                post_update_tab.apply();
            }
        }
    }
}

control SwitchEgressDeparser(packet_out pkt,
            inout header_t hdr,
            in eg_metadata_t eg_md,
            /* Intrinsic */
            in    egress_intrinsic_metadata_for_deparser_t  eg_dprsr_md){
    Checksum () ipv4_checksum;
    Checksum () tcp_checksum;

    apply {
        // if (hdr.ipv4.isValid()){
        //     // update the IPv4 checksum
        //     hdr.ipv4.hdr_checksum = ipv4_checksum.update({
        //         hdr.ipv4.version,
        //         hdr.ipv4.ihl,
        //         hdr.ipv4.dscp,
        //         hdr.ipv4.ecn,
        //         hdr.ipv4.total_len,
        //         hdr.ipv4.identification,
        //         hdr.ipv4.flags,
        //         hdr.ipv4.frag_offset,
        //         hdr.ipv4.ttl,
        //         hdr.ipv4.protocol,
        //         hdr.ipv4.src,
        //         hdr.ipv4.dst
        //     });
        // }

        // if(hdr.tcp.isValid()){
        //     // update the TCP checksum
        //     hdr.tcp.checksum = tcp_checksum.update({ // TODO: add tcp checksum update in the ingress deparser
        //         hdr.control_hdr.proto,
        //         hdr.cpu.is_post_sample,
        //         hdr.cpu.score,
        //         hdr.flow_info.key,
        //         hdr.flow_info.val,

        //         eg_md.checksum
        //     });
        // }
        pkt.emit(hdr);
    }
}

/*************************************************************************
***********************  S W I T C H  *******************************
*************************************************************************/

//switch architecture

Pipeline(SwitchIngressParser(),
         SwitchIngress(),
         SwitchIngressDeparser(),
         SwitchEgressParser(),
         SwitchEgress(),
         SwitchEgressDeparser()) pipe;

Switch(pipe) main;
