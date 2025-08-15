// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-

#ifndef LCP_H
#define LCP_H

/*
 * A UEC source and sink
 */
#include "config.h"
#include "eventlist.h"
#include "fairpullqueue.h"
#include "lcp_pacer.h"
// #include "datacenter/logsim-interface.h"
#include "network.h"
#include "trigger.h"
#include "uecpacket.h"
#include <functional>
#include <list>
#include <map>
#include <set>

class LcpSink;
// class LogSimInterface;

class LcpSentPacket {
  public:
    LcpSentPacket(simtime_picosec t, uint64_t s, bool a, bool n, bool to, uint16_t _size, simtime_picosec _sent_time, uint32_t ev_used,  unsigned long _parity_id=0)
            : timer{t}, seqno{s}, acked{a}, nacked{n}, timedOut{to}, size{_size}, sent_time(_sent_time), entropy_used(ev_used),  parity_id(_parity_id) {}
    LcpSentPacket(const LcpSentPacket &sp)
            : timer{sp.timer}, seqno{sp.seqno}, acked{sp.acked}, nacked{sp.nacked}, timedOut{sp.timedOut}, size{sp.size}, sent_time(sp.sent_time), entropy_used(sp.entropy_used), parity_id(sp.parity_id) {}
    simtime_picosec timer;
    uint64_t seqno;
    bool acked;
    bool nacked;
    bool timedOut;
    uint16_t size;
    simtime_picosec sent_time;
    unsigned long parity_id;
    uint32_t entropy_used;
};

class LcpSrc : public PacketSink, public EventSource, public TriggerTarget {
    friend class LcpSink;

  public:
    LcpSrc(UecLogger *logger, TrafficLogger *pktLogger, EventList &eventList, uint64_t rtt, uint64_t bdp,
           uint64_t queueDrainTime, int hops);
    virtual ~LcpSrc();

    virtual void doNextEvent() override;

    static std::string phantom_algo;


    int received_so_far = 0;
    void receivePacket(Packet &pkt) override;
    const string &nodename() override;

    virtual void connect(Route *routeout, Route *routeback, LcpSink &sink, simtime_picosec startTime);
    void startflow();
    void set_paths(vector<const Route *> *rt);
    void set_paths(uint32_t no_of_paths);
    void map_entropies();

    void set_dst(uint32_t dst) { _dstaddr = dst; }

    // called from a trigger to start the flow.
    virtual void activate() { startflow(); }

    void set_end_trigger(Trigger &trigger);

    inline void set_flowid(flowid_t flow_id) { _flow.set_flowid(flow_id); }
    inline flowid_t flow_id() const { return _flow.flow_id(); }

    void setCwnd(uint64_t cwnd) { _cwnd = cwnd; };
    void setReuse(bool reuse) { _use_good_entropies = reuse; };
    void setNumberEntropies(int num_entropies) { _num_entropies = num_entropies; };
    void setHopCount(int hops) {
        _hop_count = hops;
        printf("Hop Count is %d\n", hops);
    };
    void setFlowSize(uint64_t flow_size) { _flow_size = flow_size; }

    uint64_t getCwnd() { return _cwnd; };
    uint64_t getHighestSent() { return _highest_sent; }
    uint32_t getUnacked() { return _unacked; }
    uint32_t getReceivedSize() { return _received_ecn.size(); }
    uint32_t getRto() { return _rto; }

    int choose_route();
    int next_route();

    void set_traffic_logger(TrafficLogger *pktlogger);
    static void set_queue_type(std::string value) { queue_type = value; }
    static void set_epoch_type(std::string value) { epoch_type = value; }
    static void set_qa_type(std::string value) { qa_type = value; }
    void set_use_pacing(bool value) { use_pacing = value; }
    void set_pacing_delay(simtime_picosec value) { pacing_delay = value * 1000; }
    void set_use_qa(bool value) { use_qa = value; }
    void set_use_fi(bool value) { use_fi = value; }
    void set_use_rtt(bool value) { use_rtt = value; }
    void set_use_ecn(bool value) { use_ecn = value; }
    // void set_use_erasure_coding(bool value) { use_erasure_coding = value; }
    // void set_ec_handler(string value) { ec_handler = value; }
    // void set_parity_group_size(unsigned int value) { parity_group_size = value; }
    // void set_parity_correction_capacity(unsigned int value) { parity_correction_capacity = value; }
    void set_use_trimming(bool value) { use_trimming = value; }
    void set_use_rto_cwnd_reduction(bool value) { use_rto_cwnd_reduction = value; }
    void set_lcp_k(double value) { lcp_k = value; }
    void set_lcp_k_scale(double value) { lcp_k_scale = value; }
    void set_lcp_ecn_alpha(float value) { lcp_ecn_alpha = value; }
    void set_target_to_baremetal_ratio(float value) { target_to_baremetal_ratio = value; }
    void set_starting_cwnd_bdp_ratio(float value) { starting_cwnd_bdp_ratio = value; }
    void set_constant_timestamp_epoch_ratio(float value) { constant_timestamp_epoch_ratio = value; }
    void set_md_gain_ecn(float value) { md_gain_ecn = value; }
    void set_md_factor_rtt(float value) { md_factor_rtt = value; }
    void set_maxcwnd_bdp_ratio(float value) { _maxcwnd_bdp_ratio = value; }
    void set_queue_idx(int value) { queue_idx = value; }
    void set_fi_threshold(uint32_t value) { fi_threshold = value; }
    void set_ai_bytes(double value) { ai_bytes = value; }
    void set_ai_bytes_scale(double value) { ai_bytes_scale = value; }
    void set_apply_ai_per_epoch(bool value) { apply_ai_per_epoch = value; }
    void set_cc_algorithm_type(string value) { cc_algorithm_type = value; }

    // static void set_os_ratio_stage_1(double value) { ratio_os_stage_1 = value; }
    static void set_frequency(int value) { freq = value; }
    static void set_precision_ts(int value) { precision_ts = value; }
    static void set_starting_cwnd(double value) { starting_cwnd = value; }
    static void set_bts(bool use_b) { use_bts = use_b; }
    static void setRouteStrategy(RouteStrategy strat) { _route_strategy = strat; }

    void set_flow_over_hook(std::function<void(const Packet &)> hook) { f_flow_over_hook = hook; }

    simtime_picosec targetDelay(uint32_t cwnd);

    virtual void rtx_timer_hook(simtime_picosec now, simtime_picosec period);
    void pacedSend();
    static void set_interdc_delay(uint64_t delay) { _interdc_delay = delay; }
    virtual void updateParams(uint64_t base_rtt_intra, 
      uint64_t base_rtt_inter, 
      uint64_t bdp_intra, 
      uint64_t bdp_inter, 
      uint64_t intra_queuesize, 
      uint64_t inter_queuesize);

    void track_sending_rate();
    void track_ecn_rate();
    void check_limits_cwnd();

    // MIMD
    bool apply_mimd = false;
    static std::string lcp_type;
    double kmin = -1;
    double kmax = -1;
    void set_apply_mimd(bool value) { apply_mimd = value; }
    void set_kmin(double value) { kmin = value; }
    void set_kmax(double value) { kmax = value; }
    double queue_latency_ps;

    // Contract
    simtime_picosec _base_rtt_inter;
    simtime_picosec _base_rtt_intra;
    linkspeed_bps _network_linkspeed;
    double max_cwnd_scaling = 10;
    simtime_picosec t_last_update = 0;
    double contract_scaling = 10;
    void adjust_window_mimd(bool skip, simtime_picosec delay, mem_b newly_acked_bytes);
    // void set_contract_target_delay();
    simtime_picosec (LcpSrc::*contract_target_delay)(mem_b cwnd, simtime_picosec rtt, simtime_picosec delay);
    simtime_picosec contract_target_delay_vegas(mem_b cwnd, simtime_picosec rtt, simtime_picosec delay);
    void clamp_cwnd();

    Trigger *_end_trigger = 0;
    // should really be private, but loggers want to see:
    uint64_t _highest_sent; // seqno is in bytes
    uint64_t _bytes_sent; // seqno is in bytes
    uint64_t _agg_bytes_sent; // seqno is in bytes
    bool need_quick_adapt = false;
    uint64_t _packets_sent;
    uint64_t _new_packets_sent;
    uint64_t _rtx_packets_sent;
    uint64_t _acks_received;
    uint64_t _nacks_received;
    static simtime_picosec _interdc_delay; // The one way delay between datacenters.
    double _cwnd;
    uint32_t bytes_acked_in_qa_period = 0;
    simtime_picosec qa_period_time = 0; // Keeps track of the measurement period for received bytes.
    uint32_t saved_trimmed_bytes = 0;
    static int freq; // Modifies the frequencey with which we count QA.
    uint32_t _flight_size;
    uint32_t _dstaddr;
    uint32_t _acked_packets;
    uint64_t _flow_start_time;
    bool flow_started = false;
    uint64_t _next_check_window;
    uint64_t next_window_end = 0;
    uint64_t qa_window_start = 0;
    bool update_next_window = true;
    bool _start_timer_window = true;
    bool _paced_packet = false;
    bool fast_drop = false;
    int ignore_for = 0;
    int count_received = 0;
    int count_ecn_in_rtt = 0;
    int count_trimmed_in_rtt = 0;
    bool increasing = false;
    int bts_received = 0;
    int tot_pkt_seen_qa = 0;

    int total_routes;
    int routes_changed = 0;
    bool pause_send = false;
    int total_nack = 0;
    uint64_t send_size = 0;

    // Custom Parameters
    static std::string queue_type;
    static int precision_ts;
    static bool use_fast_drop;
    bool was_zero_before = false;
    double ideal_x = 0;
    int qa_count = 0;
    static bool use_bts;
    double phantom_size_calc = 0;
    simtime_picosec last_phantom_increase = 0;
    simtime_picosec last_qa_event = 0;
    simtime_picosec next_increase_at = 0;
    int increasing_for = 1;
    int src_dc = 0;
    int dest_dc = 0;
    int num_trim = 0;
    vector<pair<simtime_picosec, float>> _list_cwd;

    static uint16_t _total_subflows;
    static bool _subflow_adaptive_reroute;
    vector<int> _subflow_entropies;
    vector<int> _old_subflow_entropies;
    vector<simtime_picosec> _subflow_last_reroute;
    vector<simtime_picosec> _subflow_last_ack;

    int number_retx = 0;

    // PLB
    int plb_entropy = 0;
    int plb_congested_rounds = 0;
    double plb_congested = 0;
    double plb_delivered = 0;
    simtime_picosec plb_last_rtt = 0;
    simtime_picosec plb_timeout_wait = 0;


    static uint64_t _switch_queue_size;
    simtime_picosec last_adjust_ts;
    vector<int> _good_entropies_list;
    int curr_entropy = 0;

    static double starting_cwnd;
    static RouteStrategy _route_strategy;
    bool use_pacing = true;
    simtime_picosec pacing_delay;
    bool first_quick_adapt = false;

    // Merging MPRDMA & BBR with LCP
    string cc_algorithm_type = "lcp";

    // Erasure coding
    unsigned long highest_possible_parity_id = 0;
    set<unsigned long> completed_parity_set;  // just used in receiver-based EC
    simtime_picosec ec_wait_time = 0;
    bool last_batch_all_acked();
    void processParityNack(UecNack &pkt);
    

    // LCP.
    float old_ecn_ewma = 0;
    float old_ecn_ewma_ago = 0;
    simtime_picosec last_ewma_update = 0;
    float ecn_fraction_ewma;
    float _ecn_count_this_window;
    float _good_count_this_window;
    uint32_t _consecutive_good_epochs; // Counts number of epochs without congestion.
    uint64_t _next_qa_sn; // Next sequence number that must be received before window changes are allowed.
    simtime_picosec qa_measurement_period_end; // Time that current QA ends
    vector<simtime_picosec> _list_is_rtt_congested;
    vector<simtime_picosec> _list_is_ecn_congested;
    vector<tuple<simtime_picosec, uint64_t, uint64_t, float, float, double>> _list_epoch_ended;
    vector<tuple<simtime_picosec, double>> _list_cwnd_reduction_factor;
    vector<tuple<simtime_picosec, uint32_t, float, float, simtime_picosec, simtime_picosec>> _list_qa_ended;
    vector<simtime_picosec> _list_qa_rtt_triggerred;
    vector<simtime_picosec> _list_qa_bytes_acked_triggerred;
    vector<tuple<simtime_picosec, uint64_t>> _list_retransmit_cwnd_reduce;
    vector<pair<simtime_picosec, uint64_t>> _list_retrans;
    vector<tuple<simtime_picosec, uint64_t, uint64_t>> _list_timeout;
    uint32_t _consecutive_decreases;
    simtime_picosec _fct;

    simtime_picosec target_rtt = 0; // Low threshold to perform reductions for RTT.
    simtime_picosec baremetal_rtt = 0; // Baremetal latency.
    double ai_bytes = 1; // Additive increase constant for LCP.
    double ai_bytes_scale = -1; // ai_bytes / bdp = ai_bytes_scale
    float md_gain_ecn = -1; // Multiplicative decrease gain for ECN-based reduction md_gan should be >= 0 and <= 1 so 10
    float md_factor_rtt = -1; // Multiplicative decrease gain for RTT-based reduction
    simtime_picosec target_qdelay = 0;
    bool use_qa = true;
    bool use_fi = true;
    bool use_rtt = true;
    bool use_ecn = true;
    bool use_rto_cwnd_reduction = true;
    bool use_trimming = false;
    bool apply_ai_per_epoch = false;  // default apply ai is per ACK
    double lcp_k = 0;
    double lcp_k_scale = 1.0;
    float target_to_baremetal_ratio = 0; // what target_rtt = baremetal_rtt * target_to_baremetal_ratio
    float starting_cwnd_bdp_ratio = 0; // if starting_cwnd = 1 then cwnd becomes _bdp * starting_cwnd_bdp_ratio
    uint32_t fi_threshold = 3; // fast increase threshold
    int fast_increase_round = 0;

    float window[5];
    int window_index = 0;
    int window_count = 0;
    static bool smooth_ecn;


    int average_count = 0;
    double average_online = 0;
    simtime_picosec last_average_update = 0;
    double old_average_online = 0;
    double current_average_online = 0;

    // Function to add new count to the window and return current average
    float update_window(float new_count) {
      window[window_index] = new_count;
      window_index = (window_index + 1) % 5;
      if (window_count < 5)
          window_count++;
      
      float sum = 0;
      for (int i = 0; i < window_count; i++) {
          sum += window[i];
      }
      return sum / window_count;
    }

    float lcp_ecn_alpha = 1.0;
    simtime_picosec qa_trigger_rtt = 0;


    uint16_t _mss;
    bool _flow_finished = false;
    uint64_t _bdp;

    uint64_t total_new_bytes_acked_in_epoch = 0;
    uint64_t ecn_marked_bytes_in_epoch;

    bool use_per_ack_md = false;
    void set_use_per_ack_md(bool value) { use_per_ack_md = value; }

    double calculate_drain_time_ratio(double slowdown_percentage, double phantom_queue_max_ratio);

    double epoch_start_cwnd = 0;

    bool use_per_ack_ewma = false;
    void set_use_per_ack_ewma(bool value) { use_per_ack_ewma = value; }

    static bool _subflow_reroute;
    int _fixed_route = -1;
    virtual void adjust_window(simtime_picosec ts, bool ecn, simtime_picosec rtt, uint32_t ackno, uint64_t num_bytes_acked, simtime_picosec acked_pkt_ts);
    
    bool collection_initiated = false;
    uint32_t bytes_acked_in_collection_period = 0;
    simtime_picosec collection_period_end;
    vector<pair<simtime_picosec, uint64_t>> _list_bytes_acked_collected;
    void additive_increase(uint64_t num_bytes_acked);
    simtime_picosec _base_rtt; // Picoseconds.
    float computeEwma(double ewma_estimate, double new_value, double alpha);
    vector<pair<simtime_picosec, double>> _list_ecn_ewma;
    virtual void send_packets();
    bool epoch_enabled;


    double alpha_dctcp = 0;
    static bool using_dctcp;
    int packets_dctcp;
    int ecn_packets_dctcp;
    simtime_picosec last_dctcp_alpha_update;
    bool need_decrease = false;
    bool need_increase = false;
    bool detected_ecn = false;
    static int force_cwnd;
    int ssthresh = 10000000;
    bool stop_raising = false;

    uint64_t aggregate_timedout = 0;  // aggreagate bytes that are timedout
    uint64_t aggregate_retrans = 0;  // aggreagate bytes retransmitted
    bool _rtx_pending;
    bool resend_packet(std::size_t i);
    void reduce_unacked(uint64_t amount);
    vector<tuple<simtime_picosec, uint64_t>> _list_bytes_sent;
    vector<tuple<simtime_picosec, uint64_t>> _list_agg_bytes_sent;
    uint64_t _rto_margin;
    vector<LcpSentPacket> _sent_packets;
    uint64_t _rto;


  private:
    uint32_t _unacked;
    uint32_t _effcwnd;
    uint64_t _flow_size;
    // uint64_t _rtt;
    float _maxcwnd_bdp_ratio = 1.0;
    uint16_t _crt_path = 0;
    uint32_t target_window;
    // LogSimInterface *_lgs;
   
    uint64_t _maxcwnd;

    uint16_t _current_subflow = 0;

    int queue_idx = -1;

    
    


    // new CC variables
    uint64_t _queue_size;
    uint64_t last_pac_change = 0;
    uint64_t previous_window_end = 0;
    bool _target_based_received;
    bool _using_lgs = false;
    UecLogger *_logger;
    LcpSink *_sink;

    // uint16_t _crt_direction;
    vector<int> _path_ids;                 // path IDs to be used for ECMP FIB.
    vector<const Route *> _paths;          // paths in current permutation order
    vector<const Route *> _original_paths; // paths in original permutation
                                           // order
    const Route *_route;
    // order
    vector<int16_t> _path_acks;   // keeps path scores
    vector<int16_t> _path_ecns;   // keeps path scores
    vector<int16_t> _path_nacks;  // keeps path scores
    vector<int16_t> _avoid_ratio; // keeps path scores
    vector<int16_t> _avoid_score; // keeps path scores
    vector<bool> _bad_path;       // keeps path scores

    LcpSmarttPacer *generic_pacer = NULL;
    PacketFlow _flow;

    simtime_picosec tracking_period = 0;
    simtime_picosec qa_period = 0;
    simtime_picosec last_track_ts = 0;
    uint64_t tracking_bytes = 0;

    string _nodename;
    std::function<void(const Packet &p)> f_flow_over_hook;


    list<std::tuple<simtime_picosec, bool, uint64_t, uint64_t>> _received_ecn; // list of packets received
    unsigned _nack_rtx_pending;
    vector<tuple<simtime_picosec, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t>> _list_rtt;
    vector<pair<simtime_picosec, uint64_t>> _list_unacked;
    vector<pair<simtime_picosec, uint64_t>> _list_sent;
    vector<pair<simtime_picosec, uint64_t>> _list_acked_bytes;
    vector<pair<simtime_picosec, uint64_t>> _list_ecn_received;
    vector<pair<simtime_picosec, uint64_t>> _list_nack;
    vector<pair<simtime_picosec, uint64_t>> _list_bts;
    vector<pair<simtime_picosec, uint64_t>> _list_fast_increase_event;

    // for debug
    vector<tuple<simtime_picosec, uint64_t, uint64_t, uint64_t, uint64_t>> _list_rto_info;

    vector<const Route *> _good_entropies;
    bool _use_good_entropies;
    std::size_t _max_good_entropies;
    std::size_t _next_good_entropy;
    std::vector<int> _entropy_array;
    int _num_entropies = -1;
    int current_entropy = 0;
    bool _enableDistanceBasedRtx;
    bool _bts_enabled = true;
    uint64_t next_byte_to_send;
    int _next_pathid;
    int _hop_count;
    int data_count_idx = 0;
    simtime_picosec count_rtt = 0;
    simtime_picosec next_qa = 0;

    // This is the part for various epoch end techniques
    simtime_picosec epoch_end_time = 0;
    static std::string epoch_type;
    static std::string qa_type;
    float epoch_counter;
    bool qa_enabled;
    double constant_timestamp_epoch_ratio = 1.0;
    vector<pair<simtime_picosec, int>> list_ecn_rate;
    vector<pair<simtime_picosec, double>> list_sending_rate;

    uint64_t get_unacked();

    virtual void adjust_window_aimd(simtime_picosec ts, bool ecn, simtime_picosec rtt, uint32_t ackno, uint64_t num_bytes_acked, simtime_picosec acked_pkt_ts);
    virtual void adjust_window_aimd_phantom(simtime_picosec ts, bool ecn, simtime_picosec rtt, uint32_t ackno, uint64_t num_bytes_acked, simtime_picosec acked_pkt_ts);
    const Route *get_path();
    tuple<uint64_t, simtime_picosec> update_pending_packets_list(UecAck &pkt); // (new_bytes_acked, acked_pkt_ts)
    void add_ack_path(const Route *rt);
    virtual void retransmit_packet();
    
    void finish_flow();
    void processAck(UecAck &pkt, bool);
    std::size_t get_sent_packet_idx(uint32_t pkt_seqno);

    // void reduce_cwnd(uint64_t amount);
    void processNack(UecNack &nack);
    void processBts(UecPacket *nack);
    // void simulateTrimEvent(UecAck &nack);
    // void apply_timeout_penalty();
    void update_pacing_delay();
    

    bool shouldTriggerEpochEnd(uint32_t ackno, simtime_picosec acked_pkt_ts);
    void resetEpochParams();
    void processEpochEnd(simtime_picosec rtt);
    void processQaMeasurementEnd(simtime_picosec rtt);
    void update_next_byte_to_send();
};

class LcpSink : public PacketSink, public DataReceiver {
    friend class LcpSrc;

  public:
    LcpSink();
    virtual ~LcpSink();

    void receivePacket(Packet &pkt) override;
    const string &nodename() override;

    void set_end_trigger(Trigger &trigger);

    uint64_t cumulative_ack() override;
    uint32_t drops() override;
    void connect(LcpSrc &src, const Route *route);
    void set_paths(uint32_t num_paths);
    void set_src(uint32_t s) { _srcaddr = s; }
    uint32_t from = -1;
    uint32_t to = -2;
    uint32_t tag = 0;
    static void setRouteStrategy(RouteStrategy strat) { _route_strategy = strat; }
    static RouteStrategy _route_strategy;
    Trigger *_end_trigger = 0;
    int pfc_just_seen = -10;
    
    bool use_trimming = false;
    void set_use_trimming(bool value) { use_trimming = value; }
    
    int queue_idx = -1;
    void set_queue_idx(int value) { queue_idx = value; }

    LcpSrc *_src;

    // Erasure coding
    virtual void parity_timer_hook(simtime_picosec now, simtime_picosec period);

    simtime_picosec _flow_start_time = 0;

    bool collection_initiated = false;
    simtime_picosec collection_period_end;
    vector<tuple<simtime_picosec, uint64_t>> _list_bytes_rcvd;
    uint64_t bytes_rcvd = 0;


  private:
    UecAck::seq_t _cumulative_ack;
    uint64_t _packets;
    uint32_t _srcaddr;
    uint32_t _drops;
    int ack_count_idx = 0;
    string _nodename;
    list<UecAck::seq_t> _received; // list of packets received OOO
    uint16_t _crt_path;
    const Route *_route;
    vector<const Route *> _paths;
    vector<int> _path_ids;                 // path IDs to be used for ECMP FIB.
    vector<const Route *> _original_paths; // paths in original permutation
                                           // order
    vector<int> _good_entropies_list;

    void send_ack(simtime_picosec ts, bool marked, UecAck::seq_t seqno, UecAck::seq_t ackno, const Route *rt,
                  const Route *inRoute, int path_id, unsigned long completed_batch_parity_id=0, unsigned long highest_completed_parity_id=0, int subflow_id=0);
    void send_nack(simtime_picosec ts, bool marked, UecAck::seq_t seqno, UecAck::seq_t ackno, const Route *rt, int,
                   bool);
    void send_parity_nack(const Route *rt, unsigned long nack_parity_id=0, unsigned long highest_completed_parity_id=0);
    bool already_received(UecPacket &pkt);
};

class LcpRtxTimerScanner : public EventSource {
  public:
    LcpRtxTimerScanner(simtime_picosec scanPeriod, EventList &eventlist);
    void doNextEvent();
    void registerLcp(LcpSrc &LcpSrc);

  private:
    simtime_picosec _scanPeriod;
    simtime_picosec _lastScan;
    typedef list<LcpSrc *> lcps_t;
    lcps_t _lcps;
};

class LcpParityTimerScanner : public EventSource {
  public:
  LcpParityTimerScanner(simtime_picosec scanPeriod, EventList &eventlist);
    void doNextEvent();
    void registerLcp(LcpSink &LcpSink);

  private:
    simtime_picosec _scanPeriod;
    simtime_picosec _lastScan;
    typedef list<LcpSink *> lcps_t;
    lcps_t _lcps;
};

#endif
