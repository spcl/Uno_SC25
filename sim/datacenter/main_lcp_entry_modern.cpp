// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "config.h"
#include "network.h"
#include "queue_lossless_input.h"
#include "randomqueue.h"
#include <iostream>
#include <math.h>

#include <sstream>
#include <string.h>
// #include "subflow_control.h"
#include "clock.h"
#include "compositequeue.h"
#include "compositequeue_no_ecn.h"
#include "connection_matrix.h"
#include "eventlist.h"
#include "firstfit.h"
#include "logfile.h"
#include "loggers.h"
#include "logsim-interface.h"
#include "pipe.h"
#include "shortflows.h"
#include "topology.h"
#include "lcp.h"
#include "gemini.h"
#include "uec.h"
#include "bbr.h"
#include "mprdma.h"
#include <filesystem>
// #include "vl2_topology.h"

// Fat Tree topology was modified to work with this script, others won't work
// correctly
#include "fat_tree_interdc_topology.h"
// #include "oversubscribed_fat_tree_topology.h"
// #include "multihomed_fat_tree_topology.h"
// #include "star_topology.h"
// #include "bcube_topology.h"
#include <list>

// Simulation params

#define PRINT_PATHS 0

#define PERIODIC 0
#include "main.h"

// int RTT = 10; // this is per link delay; identical RTT microseconds = 0.02 ms
uint32_t RTT = 400; // this is per link delay in ns; identical RTT microseconds
                    // = 0.02 ms
int DEFAULT_NODES = 128;
#define DEFAULT_QUEUE_SIZE 100000000 // ~100MB, just a large value so we can ignore queues
// int N=128;

FirstFit *ff = NULL;
unsigned int subflow_count = 1;

string ntoa(double n);
string itoa(uint64_t n);

// #define SWITCH_BUFFER (SERVICE * RTT / 1000)
#define USE_FIRST_FIT 0
#define FIRST_FIT_INTERVAL 100

EventList eventlist;

Logfile *lg;

void exit_error(char *progr) {
    cout << "Usage " << progr
         << " [UNCOUPLED(DEFAULT)|COUPLED_INC|FULLY_COUPLED|COUPLED_EPSILON] "
            "[epsilon][COUPLED_SCALABLE_TCP"
         << endl;
    exit(1);
}

void print_path(std::ofstream &paths, const Route *rt) {
    for (unsigned int i = 1; i < rt->size() - 1; i += 2) {
        RandomQueue *q = (RandomQueue *)rt->at(i);
        if (q != NULL)
            paths << q->str() << " ";
        else
            paths << "NULL ";
    }

    paths << endl;
}

int main(int argc, char **argv) {
    // eventlist.setEndtime(timeFromSec(1));
    Clock c(timeFromSec(100 / 100.), eventlist);
    mem_b inter_queuesize = INFINITE_BUFFER_SIZE;
    mem_b intra_queuesize = INFINITE_BUFFER_SIZE;
    int no_of_conns = 0, cwnd = MAX_CWD_MODERN_UEC, no_of_nodes = DEFAULT_NODES;
    stringstream filename(ios_base::out);
    RouteStrategy route_strategy = NOT_SET;
    std::string goal_filename;
    linkspeed_bps linkspeed = speedFromMbps((double)HOST_NIC);
    simtime_picosec hop_latency = timeFromNs((uint32_t)RTT);
    simtime_picosec switch_latency = timeFromNs((uint32_t)0);
    simtime_picosec pacing_delay = 0;
    int kmin = -1;
    int kmax = -1;



    int force_kmin = -1;
    int force_kmax = -1;


    // inter-dc ECN
    int inter_dc_kmin = -1;
    int inter_dc_kmax = -1;
    bool use_inter_dc_ecn = false;

    int force_queue_size = -1;
    int bts_threshold = -1;
    int seed = -1;
    bool reuse_entropy = false;
    int number_entropies = 256;
    queue_type queue_choice = COMPOSITE;
    bool ignore_ecn_data = true;
    bool ignore_ecn_ack = true;
    bool do_jitter = false;
    bool do_exponential_gain = false;
    bool use_fast_increase = false;
    int target_rtt_percentage_over_base = 50;
    bool collect_data = false;
    int fat_tree_k = 1; // 1:1 default
    COLLECT_DATA = collect_data;
    bool use_super_fast_increase = false;
    double y_gain = 1;
    double x_gain = 0.15;
    double z_gain = 1;
    double w_gain = 1;
    double bonus_drop = 1;
    double drop_value_buffer = 1;
    uint64_t actual_starting_cwnd = 1;
    uint64_t explicit_base_rtt = 0;
    uint64_t explicit_target_rtt = 0;
    double queue_size_ratio = 1.0;
    bool disable_case_3 = false;
    bool disable_case_4 = false;
    int ratio_os_stage_1 = 1;
    int pfc_low = 0;
    int pfc_high = 0;
    int pfc_marking = 0;
    double quickadapt_lossless_rtt = 2.0;
    int reaction_delay = 1;
    char *tm_file = NULL;
    int precision_ts = 1;
    int once_per_rtt = 0;
    bool enable_bts = false;
    bool use_mixed = false;
    int phantom_size;
    int phantom_slowdown = 10;
    bool use_phantom = false;
    double exp_avg_ecn_value = .3;
    double exp_avg_rtt_value = .3;
    char *topo_file = NULL;
    double exp_avg_alpha = 0.125;
    bool use_exp_avg_ecn = false;
    bool use_exp_avg_rtt = false;
    int stop_pacing_after_rtt = 0;
    int num_failed_links = 0;
    bool topology_normal = true;
    simtime_picosec interdc_delay = 0;
    double def_end_time = 5000.0;
    int num_periods = 1;
    bool use_inter_gemini = false;
    bool use_intra_gemini = false;
    bool use_bbr = false;
    bool use_uec = false;
    bool use_mprdma = false;
    bool use_inter_lcp = false;
    bool use_intra_lcp = false;
    bool lcp_per_ack_md = false;
    bool lcp_per_ack_ewma = false;
    
    // erasure coding
    bool use_erasure_coding = false;
    string ec_handler = "dst";
    unsigned int parity_group_size = 0;
    unsigned int parity_correction_capacity = 0;

    // MIMD
    std::string lcp_type = "aimd";

    int i = 1;
    filename << "logout.dat";

    use_inter_lcp = true;
    // these two vars are for using different files as CCs
    std::string inter_algo = "lcp";
    std::string intra_algo = "lcp";
    use_intra_lcp = true;
    float lcp_md_gain_ecn_inter = -1;
    float lcp_md_gain_ecn_intra = -1;
    float lcp_md_gain_rtt = -1;
    // these two variables are for different algorithms in lcp code
    std::string cc_algorithm_intra = "lcp";
    std::string cc_algorithm_inter = "lcp";
    double random_drop_perc = -1;
    double percentage_link_down = 0;
    double H_gemini = 0.00000012; // gemini suggested value for H (1.2 * 10^-7)
    double beta_gemini = 0.2;
    // T_gemini is in ms
    double T_gemini = 5;
    double lcp_k_scale = 1.0;
    int force_cwnd = 0;
    double ai_bytes_scale = -1;
    std::string lcpAlgo = "aimd";

    while (i < argc) {
        if (!strcmp(argv[i], "-o")) {
            filename.str(std::string());
            filename << argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-sub")) {
            subflow_count = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-conns")) {
            no_of_conns = atoi(argv[i + 1]);
            cout << "no_of_conns " << no_of_conns << endl;
            cout << "!!currently hardcoded to 8, value will be ignored!!" << endl;
            i++;
        } else if (!strcmp(argv[i], "-nodes")) {
            no_of_nodes = atoi(argv[i + 1]);
            cout << "no_of_nodes " << no_of_nodes << endl;
            i++;
        } else if (!strcmp(argv[i], "-cwnd")) {
            cwnd = atoi(argv[i + 1]);
            cout << "cwnd " << cwnd << endl;
            i++;
        } else if (!strcmp(argv[i], "-use_mixed")) {
            use_mixed = atoi(argv[i + 1]);
            // LcpSrc::set_use_mixed(use_mixed);
            CompositeQueue::set_use_mixed(use_mixed);
            // printf("UseMixed: %d\n", use_mixed);
            i++;
        } else if (!strcmp(argv[i], "-randomDrop")) {
            random_drop_perc = atof(argv[i + 1]);
            cout << "Randomly dropping packets. random_drop_perc: " << random_drop_perc << endl;
            if (random_drop_perc <= 0 || random_drop_perc > 1) {
                cout << "How is random_drop_perc <= 0 || random_drop_perc > 1" << endl;
                abort();
            }
            i++;
        } else if (!strcmp(argv[i], "-percentageLinksDown")) {
            percentage_link_down = atof(argv[i + 1]);
            cout << "Disabling this number of link (percentage): " << percentage_link_down << endl;
            if (percentage_link_down <= 0 || percentage_link_down > 1) {
                cout << "How is percentageLinksDown <= 0 || percentageLinksDown > 1" << endl;
                abort();
            }
            FatTreeInterDCTopology::_percentage_link_down = percentage_link_down;
            i++;
        } else if (!strcmp(argv[i], "-topo")) {
            topo_file = argv[i + 1];
            cout << "FatTree topology input file: " << topo_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-once_per_rtt")) {
            once_per_rtt = atoi(argv[i + 1]);
            // LcpSrc::set_once_per_rtt(once_per_rtt);
            UecSrc::set_once_per_rtt(once_per_rtt);
            // printf("OnceRTTDecrease: %d\n", once_per_rtt);
            i++;
        } else if (!strcmp(argv[i], "-stop_pacing_after_rtt")) {
            stop_pacing_after_rtt = atoi(argv[i + 1]);
            // LcpSrc::set_stop_pacing(stop_pacing_after_rtt);
            UecSrc::set_stop_pacing(stop_pacing_after_rtt);
            i++;
        } else if (!strcmp(argv[i], "-linkspeed")) {
            // linkspeed specified is in Mbps
            linkspeed = speedFromMbps(atof(argv[i + 1]));
            LINK_SPEED_MODERN = atoi(argv[i + 1]);
            cout << "Link speed: " << atof(argv[i + 1]) << " Mbps" << endl;
            LINK_SPEED_MODERN = LINK_SPEED_MODERN / 1000; // switch to gbps
            INTER_LINK_SPEED_MODERN = LINK_SPEED_MODERN;
            // Saving this for UEC reference, Gbps
            i++;
        } else if (!strcmp(argv[i], "-kmin")) {
            // kmin as percentage of queue size (0..100)
            kmin = atoi(argv[i + 1]);
            cout << "KMin: " << atoi(argv[i + 1]) << endl;
            // CompositeQueue::set_kMin(kmin);
            // LcpSrc::set_kmin(kmin / 100.0);
            MprdmaSrc::set_kmin(kmin / 100.0);
            UecSrc::set_kmin(kmin / 100.0);
            i++;
        } else if (!strcmp(argv[i], "-interKmin")) {
            // kmin as percentage of inter-dc queue size (0..100)
            inter_dc_kmin = atoi(argv[i + 1]);
            cout << "Inter-DC KMin: " << atoi(argv[i + 1]) << endl;
            i++;
        } else if (!strcmp(argv[i], "-interKmax")) {
            // kmax as percentage of inter-dc queue size (0..100)
            inter_dc_kmax = atoi(argv[i + 1]);
            cout << "Inter-DC KMax: " << atoi(argv[i + 1]) << endl;
            i++;
        } else if (!strcmp(argv[i], "-forceKmax")) {
            // kmax as percentage of inter-dc queue size (0..100)
            force_kmax = atoi(argv[i + 1]);
            cout << "Inter-DC KMax: " << atoi(argv[i + 1]) << endl;
            CompositeQueue::force_kmax = force_kmax;
            i++;
        } else if (!strcmp(argv[i], "-forceKmin")) {
            // kmax as percentage of inter-dc queue size (0..100)
            force_kmin = atoi(argv[i + 1]);
            cout << "Inter-DC KMin: " << atoi(argv[i + 1]) << endl;
            CompositeQueue::force_kmin = force_kmin;
            i++;
        } else if (!strcmp(argv[i], "-forceCWND")) {
            // kmax as percentage of inter-dc queue size (0..100)
            force_cwnd = atoi(argv[i + 1]);
            LcpSrc::force_cwnd = force_cwnd;
            i++;
        } else if (!strcmp(argv[i], "-subflow_numbers")) {
            LcpSrc::_total_subflows = atoi(argv[i + 1]);
            cout << "Using number of subflows: " << atoi(argv[i + 1]) << endl;
            i++;
        } else if (!strcmp(argv[i], "-interEcn")) {
            cout << "Using ECN for inter-DC queues" << endl;
            use_inter_dc_ecn = true;
        } else if (!strcmp(argv[i], "-multiple_failures")) {
            cout << "Using ECN for inter-DC queues" << endl;
            CompositeQueue::failure_multiples = true;
        } else if (!strcmp(argv[i], "-dctcp")) {
            LcpSrc::using_dctcp = true;
            cout << "Using DCTCP" << endl;
        } else if (!strcmp(argv[i], "-adaptive_reroute")) {
            cout << "Using Adaptive Re-Route" << endl;
            LcpSrc::_subflow_adaptive_reroute = true;
        } else if (!strcmp(argv[i], "-subflow_reroute")) {
            cout << "Re-route on subflow" << endl;
            LcpSrc::_subflow_reroute = true;
        } else if (!strcmp(argv[i], "-fail_one")) {
            FatTreeInterDCTopology::_fail_one = true;
        } else if (!strcmp(argv[i], "-k")) {
            fat_tree_k = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-ratio_os_stage_1")) {
            ratio_os_stage_1 = atoi(argv[i + 1]);
            // LcpSrc::set_os_ratio_stage_1(ratio_os_stage_1);
            UecSrc::set_os_ratio_stage_1(ratio_os_stage_1);
            i++;
        } else if (!strcmp(argv[i], "-kmax")) {
            // kmin as percentage of queue size (0..100)
            kmax = atoi(argv[i + 1]);
            cout << "KMax: " << atoi(argv[i + 1]) << endl;
            // CompositeQueue::set_kMax(kmax);
            // LcpSrc::set_kmax(kmax / 100.0);
            MprdmaSrc::set_kmax(kmax / 100.0);
            UecSrc::set_kmax(kmax / 100.0);
            i++;
        } else if (!strcmp(argv[i], "-pfc_marking")) {
            pfc_marking = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-quickadapt_lossless_rtt")) {
            quickadapt_lossless_rtt = std::stod(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-bts_trigger")) {
            bts_threshold = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-forceQueueSize")) {
            force_queue_size = atoi(argv[i + 1]);
            i++;
        }  else if (!strcmp(argv[i], "-mtu")) {
            int packet_size = atoi(argv[i + 1]);
            PKT_SIZE_MODERN = packet_size; // Saving this for UEC reference, Bytes
            i++;
        } else if (!strcmp(argv[i], "-reuse_entropy")) {
            reuse_entropy = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-num_periods")) {
            num_periods = atoi(argv[i + 1]);
            LcpSrc::set_frequency(num_periods);
            UecSrc::set_frequency(num_periods);
            i++;
        } else if (!strcmp(argv[i], "-disable_case_3")) {
            disable_case_3 = atoi(argv[i + 1]);
            // LcpSrc::set_disable_case_3(disable_case_3);
            UecSrc::set_disable_case_3(disable_case_3);
            // printf("DisableCase3: %d\n", disable_case_3);
            i++;
        } else if (!strcmp(argv[i], "-jump_to")) {
            UecSrc::jump_to = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-reaction_delay")) {
            reaction_delay = atoi(argv[i + 1]);
            // LcpSrc::set_reaction_delay(reaction_delay);
            UecSrc::set_reaction_delay(reaction_delay);
            // printf("ReactionDelay: %d\n", reaction_delay);
            i++;
        } else if (!strcmp(argv[i], "-precision_ts")) {
            precision_ts = atoi(argv[i + 1]);
            FatTreeSwitch::set_precision_ts(precision_ts * 1000);
            LcpSrc::set_precision_ts(precision_ts * 1000);
            UecSrc::set_precision_ts(precision_ts * 1000);
            // printf("Precision: %d\n", precision_ts * 1000);
            i++;
        } else if (!strcmp(argv[i], "-disable_case_4")) {
            disable_case_4 = atoi(argv[i + 1]);
            // LcpSrc::set_disable_case_4(disable_case_4);
            UecSrc::set_disable_case_4(disable_case_4);
            // printf("DisableCase4: %d\n", disable_case_4);
            i++;
        } else if (!strcmp(argv[i], "-stop_after_quick")) {
            // LcpSrc::set_stop_after_quick(true);
            UecSrc::set_stop_after_quick(true);
            // printf("StopAfterQuick: %d\n", true);

        } else if (!strcmp(argv[i], "-number_entropies")) {
            number_entropies = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-switch_latency")) {
            switch_latency = timeFromNs(atof(argv[i + 1]));
            i++;
            cout << "Switch latency is " << switch_latency << " ps" << endl;
        } else if (!strcmp(argv[i], "-hop_latency")) {
            hop_latency = timeFromNs(atof(argv[i + 1]));
            LINK_DELAY_MODERN = hop_latency / 1000; // Saving this for UEC reference, ps to ns
            cout << "link/hop latency is " << hop_latency << "ps" << endl;
            i++;
        } else if (!strcmp(argv[i], "-ignore_ecn_ack")) {
            ignore_ecn_ack = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-ignore_ecn_data")) {
            ignore_ecn_data = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-pacing_delay")) {
            pacing_delay = atoi(argv[i + 1]);
            UecSrc::set_pacing_delay(pacing_delay);
            i++;
        } else if (!strcmp(argv[i], "-usePacing")) {
            bool use_pacing = atoi(argv[i + 1]);
            cout << "Use pacing? " << use_pacing << endl;
            LCP_USE_PACING = use_pacing;
            UecSrc::set_use_pacing(use_pacing);
            MprdmaSrc::set_use_pacing(use_pacing);
            i++;
        } else if (!strcmp(argv[i], "-target_to_baremetal_ratio")) {
            float target_to_baremetal_ratio = atof(argv[i + 1]);
            cout << "target_to_baremetal_ratio: " << target_to_baremetal_ratio << endl;
            TARGET_TO_BAREMETAL_RATIO = target_to_baremetal_ratio;
            i++;
        } else if (!strcmp(argv[i], "-cwndBdpRatio")) {
            float starting_cwnd_bdp_ratio = atof(argv[i + 1]);
            cout << "starting_cwnd_bdp_ratio: " << starting_cwnd_bdp_ratio << endl;
            STARTING_CWND_BDP_RATIO = starting_cwnd_bdp_ratio;
            i++;
        } else if (!strcmp(argv[i], "-fast_drop")) {
            // LcpSrc::set_fast_drop(atoi(argv[i + 1]));
            UecSrc::set_fast_drop(atoi(argv[i + 1]));
            // printf("FastDrop: %d\n", atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-seed")) {
            seed = atoi(argv[i + 1]);
            cout << "Seed is: " << atoi(argv[i + 1]) << endl;
            i++;
        } else if (!strcmp(argv[i], "-interdcDelay")) {
            interdc_delay = timeFromNs(atoi(argv[i + 1]));
            cout << "interdc_delay is: " << interdc_delay << "ps" << endl;
            i++;
        } else if (!strcmp(argv[i], "-pfc_low")) {
            pfc_low = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-pfc_high")) {
            pfc_high = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-collect_data")) {
            collect_data = atoi(argv[i + 1]);
            COLLECT_DATA = collect_data;
            i++;
            std::cout << "Collecting data in on" << endl;
        } else if (!strcmp(argv[i], "-do_jitter")) {
            do_jitter = atoi(argv[i + 1]);
            // LcpSrc::set_do_jitter(do_jitter);
            UecSrc::set_do_jitter(do_jitter);
            // printf("DoJitter: %d\n", do_jitter);
            i++;
        } else if (!strcmp(argv[i], "-do_exponential_gain")) {
            do_exponential_gain = atoi(argv[i + 1]);
            // LcpSrc::set_do_exponential_gain(do_exponential_gain);
            UecSrc::set_do_exponential_gain(do_exponential_gain);
            // printf("DoExpGain: %d\n", do_exponential_gain);
            i++;
        } else if (!strcmp(argv[i], "-use_fast_increase")) {
            use_fast_increase = atoi(argv[i + 1]);
            // LcpSrc::set_use_fast_increase(use_fast_increase);
            UecSrc::set_use_fast_increase(use_fast_increase);
            // printf("FastIncrease: %d\n", use_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-use_super_fast_increase")) {
            use_super_fast_increase = atoi(argv[i + 1]);
            // LcpSrc::set_use_super_fast_increase(use_super_fast_increase);
            UecSrc::set_use_super_fast_increase(use_super_fast_increase);
            // printf("FastIncreaseSuper: %d\n", use_super_fast_increase);
            i++;
        } else if (!strcmp(argv[i], "-decrease_on_nack")) {
            double decrease_on_nack = std::stod(argv[i + 1]);
            // LcpSrc::set_decrease_on_nack(decrease_on_nack);
            UecSrc::set_decrease_on_nack(decrease_on_nack);
            i++;
        } else if (!strcmp(argv[i], "-phantom_in_series")) {
            CompositeQueue::set_use_phantom_in_series();
            // printf("PhantomQueueInSeries: %d\n", 1);
            // i++;
        } else if (!strcmp(argv[i], "-phantom_observe")) {
            CompositeQueue::_phantom_observe = true;
            // printf("PhantomQueueInSeries: %d\n", 1);
            // i++;
        } else if (!strcmp(argv[i], "-enable_bts")) {
            CompositeQueue::set_bts(true);
            LcpSrc::set_bts(true);
            UecSrc::set_bts(true);
            // printf("BTS: %d\n", 1);
            // i++;
        } else if (!strcmp(argv[i], "-phantom_both_queues")) {
            CompositeQueue::set_use_both_queues();
            // printf("PhantomUseBothForECNMarking: %d\n", 1);
        } else if (!strcmp(argv[i], "-lcp_phantom_smooth")) {
            LcpSrc::smooth_ecn = true;
            // printf("PhantomUseBothForECNMarking: %d\n", 1);
        } else if (!strcmp(argv[i], "-phantom_both_queues_true")) {
            CompositeQueue::set_use_both_queues_true();
            // printf("PhantomUseBothForECNMarking: %d\n", 1);
        }  else if (!strcmp(argv[i], "-tm")) {
            tm_file = argv[i + 1];
            cout << "traffic matrix input file: " << tm_file << endl;
            i++;
        } else if (!strcmp(argv[i], "-target_rtt_percentage_over_base")) {
            target_rtt_percentage_over_base = atoi(argv[i + 1]);
            // LcpSrc::set_target_rtt_percentage_over_base(target_rtt_percentage_over_base);
            UecSrc::set_target_rtt_percentage_over_base(target_rtt_percentage_over_base);
            // printf("TargetRTT: %d\n", target_rtt_percentage_over_base);
            i++;
        } else if (!strcmp(argv[i], "-num_failed_links")) {
            num_failed_links = atoi(argv[i + 1]);
            FatTreeTopology::set_failed_links(num_failed_links);
            i++;
        } else if (!strcmp(argv[i], "-fast_drop_rtt")) {
            // LcpSrc::set_fast_drop_rtt(atoi(argv[i + 1]));
            UecSrc::set_fast_drop_rtt(atoi(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-y_gain")) {
            y_gain = std::stod(argv[i + 1]);
            // LcpSrc::set_y_gain(y_gain);
            UecSrc::set_y_gain(y_gain);
            // printf("YGain: %f\n", y_gain);
            i++;
        } else if (!strcmp(argv[i], "-x_gain")) {
            x_gain = std::stod(argv[i + 1]);
            // LcpSrc::set_x_gain(x_gain);
            UecSrc::set_x_gain(x_gain);
            // printf("XGain: %f\n", x_gain);
            i++;
        } else if (!strcmp(argv[i], "-z_gain")) {
            z_gain = std::stod(argv[i + 1]);
            // LcpSrc::set_z_gain(z_gain);
            UecSrc::set_z_gain(z_gain);
            // printf("ZGain: %f\n", z_gain);
            i++;
        } else if (!strcmp(argv[i], "-end_time")) {
            def_end_time = std::stod(argv[i + 1]);
            // printf("def_end_time: %f\n", def_end_time);
            i++;
        } else if (!strcmp(argv[i], "-w_gain")) {
            w_gain = std::stod(argv[i + 1]);
            // LcpSrc::set_w_gain(w_gain);
            UecSrc::set_w_gain(w_gain);
            // printf("WGain: %f\n", w_gain);
            i++;
        } else if (!strcmp(argv[i], "-starting_cwnd")) {
            actual_starting_cwnd = atoi(argv[i + 1]);
            LcpSrc::set_starting_cwnd(actual_starting_cwnd);
            std::cout << "Starting cwnd set to " << actual_starting_cwnd << endl;
            // printf("StartingWindowForced: %d\n", actual_starting_cwnd);
            i++;
        } else if (!strcmp(argv[i], "-explicit_base_rtt")) {
            explicit_base_rtt = ((uint64_t)atoi(argv[i + 1])) * 1000;
            // printf("BaseRTTForced: %d\n", explicit_base_rtt);
            // LcpSrc::set_explicit_rtt(explicit_base_rtt);
            UecSrc::set_explicit_rtt(explicit_base_rtt);
            i++;
        } else if (!strcmp(argv[i], "-explicit_target_rtt")) {
            explicit_target_rtt = ((uint64_t)atoi(argv[i + 1])) * 1000;
            // printf("TargetRTTForced: %lu\n", explicit_target_rtt);
            // LcpSrc::set_explicit_target_rtt(explicit_target_rtt);
            UecSrc::set_explicit_target_rtt(explicit_target_rtt);
            i++;
        } else if (!strcmp(argv[i], "-queueSizeRatio")) {
            queue_size_ratio = std::stod(argv[i + 1]);
            cout << "QueueSizeRatio: " << queue_size_ratio << endl;
            if (queue_size_ratio <= 0) {
                cout << "invalid queue_size_ratio" << endl;
                exit(0);
            }
            i++;
        } else if (!strcmp(argv[i], "-bonus_drop")) {
            bonus_drop = std::stod(argv[i + 1]);
            UecSrc::set_bonus_drop(bonus_drop);
            // printf("BonusDrop: %f\n", bonus_drop);
            i++;
        } else if (!strcmp(argv[i], "-drop_value_buffer")) {
            drop_value_buffer = std::stod(argv[i + 1]);
            // LcpSrc::set_buffer_drop(drop_value_buffer);
            UecSrc::set_buffer_drop(drop_value_buffer);
            // printf("BufferDrop: %f\n", drop_value_buffer);
            i++;
        } else if (!strcmp(argv[i], "-goal")) {
            goal_filename = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-use_phantom")) {
            use_phantom = atoi(argv[i + 1]);
            // printf("UsePhantomQueue: %d\n", use_phantom);
            CompositeQueue::set_use_phantom_queue(use_phantom);
            i++;
        } else if (!strcmp(argv[i], "-phantom_kmin")) {
            int phantom_kmin = atoi(argv[i + 1]);
            CompositeQueue::_phantom_kmin = phantom_kmin;
            i++;
        } else if (!strcmp(argv[i], "-phantom_kmax")) {
            int phantom_kmax = atoi(argv[i + 1]);
            CompositeQueue::_phantom_kmax = phantom_kmax;
            i++;
        } else if (!strcmp(argv[i], "-use_exp_avg_ecn")) {
            use_exp_avg_ecn = atoi(argv[i + 1]);
            // printf("UseExpAvgEcn: %d\n", use_exp_avg_ecn);
            // LcpSrc::set_exp_avg_ecn(use_exp_avg_ecn);
            UecSrc::set_exp_avg_ecn(use_exp_avg_ecn);
            i++;
        } else if (!strcmp(argv[i], "-use_exp_avg_rtt")) {
            use_exp_avg_rtt = atoi(argv[i + 1]);
            // printf("UseExpAvgRtt: %d\n", use_exp_avg_rtt);
            // LcpSrc::set_exp_avg_rtt(use_exp_avg_rtt);
            UecSrc::set_exp_avg_rtt(use_exp_avg_rtt);
            i++;
        } else if (!strcmp(argv[i], "-exp_avg_rtt_value")) {
            exp_avg_rtt_value = std::stod(argv[i + 1]);
            // printf("UseExpAvgRttValue: %d\n", exp_avg_rtt_value);
            // LcpSrc::set_exp_avg_rtt_value(exp_avg_rtt_value);
            UecSrc::set_exp_avg_rtt_value(exp_avg_rtt_value);
            i++;
        } else if (!strcmp(argv[i], "-exp_avg_ecn_value")) {
            exp_avg_ecn_value = std::stod(argv[i + 1]);
            // printf("UseExpAvgecn_value: %d\n", exp_avg_ecn_value);
            // LcpSrc::set_exp_avg_ecn_value(exp_avg_ecn_value);
            UecSrc::set_exp_avg_ecn_value(exp_avg_ecn_value);
            i++;
        } else if (!strcmp(argv[i], "-exp_avg_alpha")) {
            exp_avg_alpha = std::stod(argv[i + 1]);
            // printf("UseExpAvgalpha: %d\n", exp_avg_alpha);
            // LcpSrc::set_exp_avg_alpha(exp_avg_alpha);
            UecSrc::set_exp_avg_alpha(exp_avg_alpha);
            i++;
        } else if (!strcmp(argv[i], "-phantom_size")) {
            phantom_size = atoi(argv[i + 1]);
            printf("PhantomQueueSize: %d\n", phantom_size);
            CompositeQueue::set_phantom_queue_size(phantom_size);
            i++;
        } else if (!strcmp(argv[i], "-num_subflow")) {
            LcpSrc::_total_subflows = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-os_border")) {
            int os_b = atoi(argv[i + 1]);
            cout << "set_os_ratio_border is: " << atoi(argv[i + 1]) << endl;
            FatTreeInterDCTopology::set_os_ratio_border(os_b);
            i++;
        } else if (!strcmp(argv[i], "-phantom_slowdown")) {
            phantom_slowdown = atoi(argv[i + 1]);
            // printf("PhantomQueueSize: %d\n", phantom_slowdown);
            CompositeQueue::set_phantom_queue_slowdown(phantom_slowdown);
            i++;
        } else if (!strcmp(argv[i], "-strat")) {
            cout << "routing strategy is: " << argv[i + 1] << endl;
            if (!strcmp(argv[i + 1], "perm")) {
                route_strategy = SCATTER_PERMUTE;
            } else if (!strcmp(argv[i + 1], "rand")) {
                route_strategy = SCATTER_RANDOM;
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "pull")) {
                route_strategy = PULL_BASED;
            } else if (!strcmp(argv[i + 1], "single")) {
                route_strategy = SINGLE_PATH;
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::SINGLE);
            } else if (!strcmp(argv[i + 1], "ecmp_host")) {
                route_strategy = ECMP_FIB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "ecmp_classic")) {
                route_strategy = ECMP_RANDOM_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "simple_subflow")) {
                route_strategy = SIMPLE_SUBFLOW;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "plb")) {
                route_strategy = PLB;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            } else if (!strcmp(argv[i + 1], "ecmp_host_random2_ecn")) {
                route_strategy = ECMP_RANDOM2_ECN;
                FatTreeSwitch::set_strategy(FatTreeSwitch::ECMP);
                FatTreeInterDCSwitch::set_strategy(FatTreeInterDCSwitch::ECMP);
            }
            i++;
        } else if (!strcmp(argv[i], "-topology")) {
            if (!strcmp(argv[i + 1], "normal")) {
                cout << "Topology is intradc" << endl;
                topology_normal = true;
            } else if (!strcmp(argv[i + 1], "interdc")) {
                cout << "Topology is interdc" << endl;
                topology_normal = false;
            }
            i++;
        } else if (!strcmp(argv[i], "-queue_type")) {
            cout << "queueing type is: " << argv[i + 1] << endl;
            if (!strcmp(argv[i + 1], "composite")) {
                queue_choice = COMPOSITE;
                LcpSrc::set_queue_type("composite");
                UecSrc::set_queue_type("composite");
            } else if (!strcmp(argv[i + 1], "composite_bts")) {
                queue_choice = COMPOSITE_BTS;
                LcpSrc::set_queue_type("composite_bts");
                UecSrc::set_queue_type("composite_bts");
                printf("Name Running: UEC BTS\n");
            } else if (!strcmp(argv[i + 1], "lossless_input")) {
                queue_choice = LOSSLESS_INPUT;
                LcpSrc::set_queue_type("lossless_input");
                UecSrc::set_queue_type("lossless_input");
                printf("Name Running: UEC Queueless\n");
            }
            i++;
        } else if (!strcmp(argv[i], "-target-low-us")) {
            cout << "TARGET_RTT is: " << argv[i + 1] << "us" << endl;
            TARGET_RTT = timeFromUs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-target-high-us")) {
            cout << "QA_TRIGGER_RTT is: " << argv[i + 1] << "us" << endl;
            QA_TRIGGER_RTT = timeFromUs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-baremetal-us")) {
            cout << "BAREMETAL_RTT is: " << argv[i + 1] << "us" << endl;
            BAREMETAL_RTT = timeFromUs(atof(argv[i + 1]));
            i++;
        } else if (!strcmp(argv[i], "-ecnAlpha")) {
            cout << "LCP_ECN_ALPHA is: " << argv[i + 1] << endl;
            LCP_ECN_ALPHA = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-aiInter")) {
            cout << "AI_BYTES_INTER is: " << argv[i + 1] << endl;
            AI_BYTES_INTER = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-aiIntra")) {
            cout << "AI_BYTES_INTRA is: " << argv[i + 1] << endl;
            AI_BYTES_INTRA = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-aiScale")) {
            cout << "AI bytes scale is: " << argv[i + 1] << endl;
            ai_bytes_scale = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-mdECNInter")) {
            cout << "mdECNInter is: " << argv[i + 1] << endl;
            lcp_md_gain_ecn_inter = atof(argv[i + 1]);
            if (lcp_md_gain_ecn_inter <= 0 || lcp_md_gain_ecn_inter >= 1) {
                cout << "Invalid mdECNInter" << endl;
                abort();
            }
            i++;
        } else if (!strcmp(argv[i], "-mdECNIntra")) {
            cout << "mdECNIntra is: " << argv[i + 1] << endl;
            lcp_md_gain_ecn_intra = atof(argv[i + 1]);
            if (lcp_md_gain_ecn_intra <= 0 || lcp_md_gain_ecn_intra >= 1) {
                cout << "Invalid mdECNIntra" << endl;
                abort();
            }
            i++;
        } else if (!strcmp(argv[i], "-mdRTT")) {
            cout << "mdRTT is: " << argv[i + 1] << endl;
            lcp_md_gain_rtt = atof(argv[i + 1]);
            if (lcp_md_gain_rtt <= 0 || lcp_md_gain_rtt >= 1) {
                cout << "Invalid mdRTT" << endl;
                abort();
            }
            i++;
        } else if (!strcmp(argv[i], "-lcpK")) {
            cout << "LCP_K (packet num) is: " << argv[i + 1] << endl;
            LCP_K = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-pacingBonus")) {
            cout << "LCP_PACING_BONUS is: " << argv[i + 1] << endl;
            LCP_PACING_BONUS = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-InterFiT")) {
            cout << "LCP_FAST_INCREASE_THRESHOLD_INTER is: " << argv[i + 1] << endl;
            LCP_FAST_INCREASE_THRESHOLD_INTER = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-IntraFiT")) {
            cout << "LCP_FAST_INCREASE_THRESHOLD_INTRA is: " << argv[i + 1] << endl;
            LCP_FAST_INCREASE_THRESHOLD_INTRA = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-epochType")) {
            cout << "epochType is: " << argv[i + 1] << endl;
            LcpSrc::set_epoch_type(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-phantom_algo")) {
            cout << "phantom_algo: " << argv[i + 1] << endl;
            LcpSrc::phantom_algo = argv[i + 1];
            i++;
        } else if (!strcmp(argv[i], "-qaType")) {
            cout << "qaType is: " << argv[i + 1] << endl;
            LcpSrc::set_qa_type(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-qaCwndRatio")) {
            cout << "QA_CWND_RATIO_THRESHOLD is " << argv[i + 1] << endl;
            QA_CWND_RATIO_THRESHOLD = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-maxCwndRatio")) {
            cout << "MAX_CWND_BDP_RATIO is " << argv[i + 1] << endl;
            MAX_CWND_BDP_RATIO = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-erasureSrc")) {
            cout << "Enable erasure coding with SRC as the handler" << endl;
            use_erasure_coding = true;
            ec_handler = "src";
        } else if (!strcmp(argv[i], "-erasureDst")) {
            cout << "Enable erasure coding with Dst as the handler" << endl;
            use_erasure_coding = true;
            ec_handler = "dst";
        } else if (!strcmp(argv[i], "-parityGroup")) {
            cout << "Parity group size is: " << argv[i + 1] << endl;
            parity_group_size = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-geminiH")) {
            cout << "H for gemini is: " << argv[i + 1] << endl;
            H_gemini = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-geminiBeta")) {
            cout << "gemini beta is: " << argv[i + 1] << endl;
            beta_gemini = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-geminiT")) {
            cout << "gemini T is: " << argv[i + 1] << endl;
            T_gemini = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-lcpKScale")) {
            cout << "Lcp K scale: " << argv[i + 1] << endl;
            lcp_k_scale = atof(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-parityCorrect")) {
            cout << "Parity correction capacity is: " << argv[i + 1] << endl;
            parity_correction_capacity = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-noQaInter")) {
            cout << "Disabling QA for Inter-Dc lcp" << endl;
            LCP_USE_QUICK_ADAPT_INTER = false;
        } else if (!strcmp(argv[i], "-noQaIntra")) {
            cout << "Disabling QA for intra DC lcp" << endl;
            LCP_USE_QUICK_ADAPT_INTRA = false;
        } else if (!strcmp(argv[i], "-noFi")) {
            cout << "Disabling fast increase" << endl;
            LCP_USE_FAST_INCREASE = false;
        } else if (!strcmp(argv[i], "-separate")) {
            cout << "Separating the queues for intra- and inter-DC flows" << endl;
            LCP_USE_DIFFERENT_QUEUES = true;
        } else if (!strcmp(argv[i], "-perAckMD")) {
            cout << "Applying MD per ack" << endl;
            lcp_per_ack_md = true;
        } else if (!strcmp(argv[i], "-perAckEwma")) {
            cout << "Applying ECN EWMA per ack" << endl;
            lcp_per_ack_ewma = true;
        } else if (!strcmp(argv[i], "-aiEpoch")) {
            cout << "Applyin AI per epoch" << endl;
            LCP_APPLY_AI_PER_EPOCH = true;
        } else if (!strcmp(argv[i], "-noRto")) {
            cout << "Disabling Cwnd reduction upon retransmission" << endl;
            LCP_RTO_REDUCE_CWND = false;
        } else if (!strcmp(argv[i], "-noRtt")) {
            cout << "Disabling epoch rtt reaction" << endl;
            LCP_USE_RTT = false;
        } else if (!strcmp(argv[i], "-noEcn")) {
            cout << "Disabling epoch ecn reaction" << endl;
            LCP_USE_ECN = false;
        } else if (!strcmp(argv[i], "-interAlgo")) {
            cout << "inter_algo is: " << argv[i + 1] << endl;
            use_inter_lcp = false;  // this might be set to true again but is not necessarily true anymore
            inter_algo = argv[i + 1];
            if (inter_algo == "bbr") {
                use_bbr = true;
            } else if (inter_algo == "uec") {
                use_uec = true;
            } else if (inter_algo == "lcp") {
                use_inter_lcp = true;
            } else if (inter_algo == "gemini") {
                use_inter_gemini = true;
            } else {
                cout << "Unknown inter algo " << inter_algo << endl;
                exit_error(argv[0]);
            }
            i++;
        } else if (!strcmp(argv[i], "-lcpAlgo")) {
            cout << "lcpAlgo is: " << argv[i + 1] << endl;
            std::string optionLcp =  argv[i + 1];

            if (optionLcp == "aimd") {
                lcpAlgo = optionLcp;
                LcpSrc::lcp_type = "aimd";
            } else if (optionLcp == "mimd") {
                lcpAlgo = optionLcp;
                LcpSrc::lcp_type = "mimd";
            } else if (optionLcp == "aimd_phantom") {
                lcpAlgo = optionLcp;
                LcpSrc::lcp_type = "aimd_phantom";
            } else {
                cout << "Unknown lcpAlgo  " << optionLcp << endl;
                exit_error(argv[0]);
            }
            i++;
        } else if (!strcmp(argv[i], "-intraAlgo")) {
            cout << "intra_algo is: " << argv[i + 1] << endl;
            use_intra_lcp = false;  // this might be set to true again but is not necessarily true anymore
            intra_algo = argv[i + 1];
            if (intra_algo == "mprdma") {
                use_mprdma = true;
            } else if (intra_algo == "lcp") {
                use_intra_lcp = true;
            } else if (intra_algo == "gemini") {
                use_intra_gemini = true;
            } else {
                cout << "Unknown intra algo " << intra_algo << endl;
                exit_error(argv[0]);
            }
            i++;
        } else if (!strcmp(argv[i], "-ccAlgoInter")) {
            cout << "CC Algorithms Inter is: " << argv[i + 1] << endl;
            use_inter_lcp = true;  // this is just to make sure we are using lcp
            use_bbr = false;
            cc_algorithm_inter = argv[i + 1];
            if (cc_algorithm_inter != "lcp") {
                cout << "Unknown ccAlgoInter" << endl;
                abort();
            }
            i++;
        } else if (!strcmp(argv[i], "-ccAlgoIntra")) {
            cout << "CC Algorithms Intra is: " << argv[i + 1] << endl;
            use_intra_lcp = true;  // this is just to make sure we are using lcp
            use_mprdma = false;
            cc_algorithm_intra = argv[i + 1];
            if (cc_algorithm_intra != "lcp" && cc_algorithm_intra != "mprdma") {
                cout << "Unknown ccAlgoIntra" << endl;
                abort();
            }
            i++;
        } else if (!strcmp(argv[i], "-interQSize")) {
            cout << "Inter queue size set to: " << argv[i + 1] << endl;
            inter_queuesize = atol(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-intraQSize")) {
            cout << "Intra queue size set to: " << argv[i + 1] << endl;
            intra_queuesize = atol(argv[i + 1]);
            i++;
        } else if (!strcmp(argv[i], "-logging-folder")) {
            FOLDER_NAME = argv[i + 1];
            i++;
        } else {
            cout << "Unknown option " << argv[i] << endl;
            exit_error(argv[0]);
        }
        i++;
    }
    cout << "All the arguments are parsed!" << endl;
    cout << "---------------------------\n\n" << endl;

    Packet::set_packet_size(PKT_SIZE_MODERN);
    cout << "MTU is " << PKT_SIZE_MODERN << " B" << endl;
    
    // Change LCP_K from packets to bytes (_bdp is in bytes so LCP should also be in bytes)
    LCP_K *= PKT_SIZE_MODERN;
    cout << "LCP_K (bytes) is " << LCP_K << " B" << endl;

    UecSrc::set_alogirthm("intersmartt");
    MprdmaSrc::set_alogirthm("mprdma");

    SINGLE_PKT_TRASMISSION_TIME_MODERN = Packet::data_packet_size() * 8 / (LINK_SPEED_MODERN);

    // Initialize Seed, Logging and Other variables
    if (seed != -1) {
        srand(seed);
        srandom(seed);
        CompositeQueueNoEcn::_random_drop_gen.seed(seed);
        CompositeQueueNoEcn::random_drop_dist = std::uniform_real_distribution<double>(0.0, 1.0);
        
    } else {
        srand(time(NULL));
        srandom(time(NULL));
        CompositeQueueNoEcn::_random_drop_gen.seed(time(NULL));
        CompositeQueueNoEcn::random_drop_dist = std::uniform_real_distribution<double>(0.0, 1.0);
    }

    initializeLoggingFolders();

    if (pfc_high != 0) {
        LosslessInputQueue::_high_threshold = pfc_high;
        LosslessInputQueue::_low_threshold = pfc_low;
        LosslessInputQueue::_mark_pfc_amount = pfc_marking;
    } else {
        LosslessInputQueue::_high_threshold = Packet::data_packet_size() * 50;
        LosslessInputQueue::_low_threshold = Packet::data_packet_size() * 25;
        LosslessInputQueue::_mark_pfc_amount = pfc_marking;
    }
    // LcpSrc::set_quickadapt_lossless_rtt(quickadapt_lossless_rtt);
    UecSrc::set_quickadapt_lossless_rtt(quickadapt_lossless_rtt);

    if (route_strategy == NOT_SET) {
        fprintf(stderr, "Route Strategy not set.  Use the -strat param.  "
                        "\nValid values are perm, rand, pull, rg and single\n");
        exit(1);
    }

    // Calculate Network Info
    int inter_hops = 9; // hardcoded for now
    int intra_hops = 6; // hardcoded for now

    // both base rtts are in ps
    uint64_t base_inter_rtt = 2 * (inter_hops - 1) * (1000 * LINK_DELAY_MODERN + switch_latency)  +  // All DCN latencies.
                              1000 * inter_hops * (PKT_SIZE_MODERN + 64) * 8 / INTER_LINK_SPEED_MODERN                +  // Packet transmission delays.
                              1000 * inter_hops * 64 * 8 / INTER_LINK_SPEED_MODERN;                            // Ack transmission delays. 

    cout << "Before adding the inter-DC delay, the base_inter_rtt is " << timeAsUs(base_inter_rtt) << " us." << endl;
    base_inter_rtt += 2 * (interdc_delay); // InterDC latencies.

    uint64_t base_intra_rtt = 2 * intra_hops * (LINK_DELAY_MODERN * 1000 + switch_latency)        +  // All DCN latencies.
                              1000 * intra_hops * (PKT_SIZE_MODERN + 64) * 8 / LINK_SPEED_MODERN                +  // Packet transmission delays.
                              1000 * intra_hops * 64 * 8 / LINK_SPEED_MODERN;                               // Ack transmission delays. 

    // base_inter_rtt / 1000.0 is base_rtt in ns
    uint64_t bdp_inter = (uint64_t) ((float)base_inter_rtt / 1000.0 * (float) INTER_LINK_SPEED_MODERN / 8.0);
    uint64_t bdp_intra = (uint64_t) ((float)base_intra_rtt / 1000.0 * (float) LINK_SPEED_MODERN / 8.0);

    inter_queuesize = bdp_inter; // Equal to BDP if not other info
    inter_queuesize *= queue_size_ratio; // scale inter_queuesize
    intra_queuesize = bdp_intra; // Equal to BDP if not other info  

    if (force_queue_size != -1) {
        inter_queuesize = force_queue_size;
        intra_queuesize = force_queue_size;
    }
    
    // Small mprdpma thing.
    assert (kmax != -1);
    // Use as the F.
    float gemini_k = ((float) kmax / 100.0) * intra_queuesize;
    MprdmaSrc::gemini_f = 4.0 * gemini_k / (gemini_k + bdp_intra);


    CompositeQueue::set_phantom_queue_size(phantom_size);

    cout << "BW: " << LINK_SPEED_MODERN << " Gbps" << endl;
    cout << "Intra: " << "\n  RTT(ps): " << base_intra_rtt << "\n  BDP(B): " << bdp_intra << "\n  Queue(B): " << intra_queuesize << endl;
    cout << "Inter: " << "\n  RTT(ps): " << base_inter_rtt << "\n  BDP(B): " << bdp_inter << "\n  Queue(B): " << inter_queuesize << endl;

    cout << "Using subflow count " << subflow_count << endl;

    // prepare the loggers

    cout << "Logging to " << filename.str() << endl;
    // Logfile
    Logfile logfile(filename.str(), eventlist);

#if PRINT_PATHS
    filename << ".paths";
    cout << "Logging path choices to " << filename.str() << endl;
    std::ofstream paths(filename.str().c_str());
    if (!paths) {
        cout << "Can't open for writing paths file!" << endl;
        exit(1);
    }
#endif

    lg = &logfile;

    logfile.setStartTime(timeFromSec(0));

    // UecLoggerSimple uecLogger;
    // logfile.addLogger(uecLogger);
    TrafficLoggerSimple traffic_logger = TrafficLoggerSimple();
    logfile.addLogger(traffic_logger);


    LcpSrc::setRouteStrategy(route_strategy);
    UecSrc::setRouteStrategy(route_strategy);
    MprdmaSrc::setRouteStrategy(route_strategy);    
    LcpSink::setRouteStrategy(route_strategy);
    UecSink::setRouteStrategy(route_strategy);
    MprdmaSink::setRouteStrategy(route_strategy);
    BBRSrc::setRouteStrategy(route_strategy);
    BBRSink::setRouteStrategy(route_strategy);

    fflush(stdout);


    if (topology_normal) {

    } else {
    }

#if USE_FIRST_FIT
    if (subflow_count == 1) {
        ff = new FirstFit(timeFromMs(FIRST_FIT_INTERVAL), eventlist);
    }
#endif

#ifdef FAT_TREE
#endif

#ifdef FAT_TREE_INTERDC_TOPOLOGY_H

#endif

#ifdef OV_FAT_TREE
    OversubscribedFatTreeTopology *top = new OversubscribedFatTreeTopology(&logfile, &eventlist, ff);
#endif

#ifdef MH_FAT_TREE
    MultihomedFatTreeTopology *top = new MultihomedFatTreeTopology(&logfile, &eventlist, ff);
#endif

#ifdef STAR
    StarTopology *top = new StarTopology(&logfile, &eventlist, ff);
#endif

#ifdef BCUBE
    BCubeTopology *top = new BCubeTopology(&logfile, &eventlist, ff);
    cout << "BCUBE " << K << endl;
#endif

#ifdef VL2
    VL2Topology *top = new VL2Topology(&logfile, &eventlist, ff);
#endif

#if USE_FIRST_FIT
    if (ff)
        ff->net_paths = net_paths;
#endif

    map<int, vector<int> *>::iterator it;

    // used just to print out stats data at the end
    list<const Route *> routes;

    // int receiving_node = 127;
    vector<int> subflows_chosen;

    ConnectionMatrix *conns = NULL;
    LogSimInterface *lgs = NULL;

    if (tm_file != NULL) {

        eventlist.setEndtime(timeFromMs(def_end_time));

        FatTreeInterDCTopology *top_dc = NULL;
        FatTreeTopology *top = NULL;

        if (topology_normal) {
            printf("Normal Topology\n");
            FatTreeTopology::set_tiers(3);
            FatTreeTopology::set_os_stage_2(fat_tree_k);
            FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            FatTreeTopology::set_bts_threshold(bts_threshold);
            FatTreeTopology::set_ignore_data_ecn(ignore_ecn_data);
            top = new FatTreeTopology(no_of_nodes, linkspeed, intra_queuesize, NULL, &eventlist, ff, queue_choice,
                                      hop_latency, switch_latency);
        } else {
            if (interdc_delay != 0) {
                FatTreeInterDCTopology::set_interdc_delay(interdc_delay);
                LcpSrc::set_interdc_delay(interdc_delay);
                UecSrc::set_interdc_delay(interdc_delay);
                BBRSrc::set_interdc_delay(interdc_delay);
            } else {
                FatTreeInterDCTopology::set_interdc_delay(hop_latency);
                LcpSrc::set_interdc_delay(hop_latency);
                UecSrc::set_interdc_delay(hop_latency);
                BBRSrc::set_interdc_delay(hop_latency);
            }
            FatTreeInterDCTopology::set_tiers(3);
            FatTreeInterDCTopology::set_os_stage_2(fat_tree_k);
            FatTreeInterDCTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeInterDCTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            FatTreeInterDCTopology::set_interdc_ecn_thresholds_as_queue_percentage(use_inter_dc_ecn, inter_dc_kmin, inter_dc_kmax);
            FatTreeInterDCTopology::set_bts_threshold(bts_threshold);
            FatTreeInterDCTopology::set_ignore_data_ecn(ignore_ecn_data);
            FatTreeInterDCTopology::set_random_drop_perc(random_drop_perc);
            /* top_dc = new FatTreeInterDCTopology(no_of_nodes, linkspeed, queuesize, NULL, &eventlist, ff,
               queue_choice, hop_latency, switch_latency); */

            if (topo_file) {
                cout << "Loading topology from " << topo_file << endl;
                top_dc = FatTreeInterDCTopology::load(topo_file, NULL, eventlist, intra_queuesize, inter_queuesize, COMPOSITE, FAIR_PRIO);
                if (top_dc->no_of_nodes() != no_of_nodes) {
                    cout << "Mismatch between connection matrix (" << no_of_nodes << " nodes) and topology ("
                         << top_dc->no_of_nodes() << " nodes)" << endl;
                    exit(1);
                }
            } else {
                FatTreeInterDCTopology::set_tiers(3);
                top_dc = new FatTreeInterDCTopology(no_of_nodes, linkspeed, intra_queuesize, inter_queuesize, NULL, &eventlist, NULL,
                                                    COMPOSITE, hop_latency, switch_latency, FAIR_PRIO);
            }
        }

        conns = new ConnectionMatrix(no_of_nodes);

        if (tm_file) {
            cout << "Loading connection matrix from  " << tm_file << endl;
            if (!conns->load(tm_file)) {
                cout << "Failed to load connection matrix " << tm_file << endl;
                exit(-1);
            }
        } else {
            cout << "Loading connection matrix from  standard input" << endl;
            conns->load(cin);
        }

        map<flowid_t, TriggerTarget *> flowmap;
        vector<connection *> *all_conns = conns->getAllConnections();
        vector<MprdmaSrc *> intra_mprdma_srcs;
        vector<LcpSrc *> intra_lcp_srcs;
        vector<LcpSrc *> inter_srcs;
        vector<BBRSrc *> bbr_srcs;
        vector<UecSrc *> uec_srcs;
        LcpSrc *lcpSrc;
        LcpSink *lcpSink;
        GeminiSrc *geminiSrc;
        UecSrc *uecSrc;
        UecSink *uecSink;
        MprdmaSrc *mprdmaSrc;
        MprdmaSink *mprdmaSink;
        BBRSrc *bbrSrc;
        BBRSink *bbrSink;

        vector<const Route *> ***net_paths;
        net_paths = new vector<const Route *> **[no_of_nodes];

        int *is_dest = new int[no_of_nodes];

        for (uint32_t i = 0; i < no_of_nodes; i++) {
            is_dest[i] = 0;
            net_paths[i] = new vector<const Route *> *[no_of_nodes];
            for (uint32_t j = 0; j < no_of_nodes; j++)
                net_paths[i][j] = NULL;
        }

        // LcpRtxTimerScanner lcpRtxScanner(timeFromUs((uint32_t)9), eventlist);
        BBRRtxTimerScanner bbrRtxScanner(timeFromUs((uint32_t)1), eventlist);
        LcpRtxTimerScanner lcpRtxScanner(timeFromUs((uint32_t)1), eventlist);
        MprdmaRtxTimerScanner mprdmaRtxScanner(timeFromUs((uint32_t)4), eventlist);
        LcpParityTimerScanner lcpParityScanner(timeFromUs((uint32_t)1), eventlist);

        FLOW_ACTIVE = all_conns->size();
        cout << "Number of flows: " << all_conns->size() << endl;

        for (size_t c = 0; c < all_conns->size(); c++) {
            connection *crt = all_conns->at(c);
            int src = crt->src;
            int dest = crt->dst;
            int src_dc = top_dc->get_dc_id(src);
            int dest_dc = top_dc->get_dc_id(dest);

            uint64_t rtt = BASE_RTT_MODERN * 1000;
            uint64_t bdp = BDP_MODERN_UEC;

            // printf("Reaching here1\n");
            // fflush(stdout);
            // printf("Setting CWND to %lu\n", actual_starting_cwnd);

            if (src_dc != dest_dc) {
                cout << "Flow from " << src << " to " << dest << " is Inter-DC (ID: " << crt->flowid << ", size: " << crt->size << "). " << endl;
                if (use_bbr) {
                    cout << "Using BBR" << endl;
                    bbrSrc = new BBRSrc(NULL, NULL, eventlist, base_inter_rtt, bdp_inter, 100, 6);
                    
                    bbrRtxScanner.registerBBR(*bbrSrc);

                    bbr_srcs.push_back(bbrSrc);

                    bbrSrc->set_dst(dest);
                    // printf("Reaching here\n");
                    if (crt->flowid) {
                        bbrSrc->set_flowid(crt->flowid);
                        assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                        flowmap[crt->flowid] = bbrSrc;
                    }

                    if (crt->size > 0) {
                        bbrSrc->setFlowSize(crt->size);
                    }

                    if (crt->trigger) {
                        Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                        trig->add_target(*bbrSrc);
                    }
                    if (crt->send_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                        bbrSrc->set_end_trigger(*trig);
                    }

                    bbrSink = new BBRSink();

                    bbrSrc->setName("bbr_" + ntoa(src) + "_" + ntoa(dest));

                    // cout << "bbr_" + ntoa(src) + "_" + ntoa(dest) << endl;
                    logfile.writeName(*bbrSrc);

                    bbrSink->set_src(src);

                    bbrSink->setName("bbr_sink_" + ntoa(src) + "_" + ntoa(dest));
                    logfile.writeName(*bbrSink);
                    if (crt->recv_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                        bbrSink->set_end_trigger(*trig);
                    }

                    switch (route_strategy) {
                        case ECMP_FIB:
                        case ECMP_FIB_ECN:
                        case ECMP_RANDOM2_ECN:
                        case SIMPLE_SUBFLOW:
                        case PLB:
                        case ECMP_RANDOM_ECN:
                        case SINGLE_PATH:
                        case SCATTER_RANDOM:
                        case REACTIVE_ECN: {
                            Route *srctotor = new Route();
                            Route *dsttotor = new Route();

                            if (top != NULL) {
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);
                                bbrSrc->src_dc = top_dc->get_dc_id(src);
                                bbrSrc->dest_dc = top_dc->get_dc_id(dest);
                                bbrSrc->updateParams(intra_queuesize, inter_queuesize);

                                // printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());

                                dsttotor->push_back(
                                        top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());
                            }

                            bbrSrc->from = src;
                            bbrSrc->to = dest;
                            bbrSink->from = src;
                            bbrSink->to = dest;
                            // printf("Creating2 Flow from %d to %d\n", bbrSrc->from, bbrSrc->to);
                            bbrSrc->connect(srctotor, dsttotor, *bbrSink, crt->start);
                            bbrSrc->set_paths(number_entropies);
                            bbrSink->set_paths(number_entropies);

                            // register src and snk to receive packets src their respective
                            // TORs.
                            if (top != NULL) {
                                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, bbrSrc->flow_id(), bbrSrc);
                                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, bbrSrc->flow_id(), bbrSink);
                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);

                                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                                        src % top_dc->no_of_nodes(), bbrSrc->flow_id(), bbrSrc);
                                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                                        dest % top_dc->no_of_nodes(), bbrSrc->flow_id(), bbrSink);
                            }
                            break;
                        }
                        case NOT_SET: {
                            cout << "Route strategy NOT_SET" << endl;
                            abort();
                            break;
                        }
                        default: {
                            cout << "Unknown Route strategy " << route_strategy << endl;
                            abort();
                            break;
                        }
                    }
                } else if (use_uec) {
                    cout << "Using UEC" << endl;
                    uecSrc = new UecSrc(NULL, NULL, eventlist, base_inter_rtt, bdp_inter, 100, 6);

                    // lcpSrc->setNumberEntropies(256);
                    uec_srcs.push_back(uecSrc);

                    uecSrc->set_dst(dest);
                    // printf("Reaching here\n");
                    if (crt->flowid) {
                        uecSrc->set_flowid(crt->flowid);
                        assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                        flowmap[crt->flowid] = uecSrc;
                    }

                    if (crt->size > 0) {
                        std::cout << crt->size << endl;
                        uecSrc->setFlowSize(crt->size);
                    }

                    if (crt->trigger) {
                        Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                        trig->add_target(*uecSrc);
                    }
                    if (crt->send_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                        uecSrc->set_end_trigger(*trig);
                    }

                    uecSink = new UecSink();

                    uecSrc->setName("uec_" + ntoa(src) + "_" + ntoa(dest));

                    // cout << "uec_" + ntoa(src) + "_" + ntoa(dest) << endl;
                    logfile.writeName(*uecSrc);

                    uecSink->set_src(src);

                    uecSink->setName("uec_sink_" + ntoa(src) + "_" + ntoa(dest));
                    logfile.writeName(*uecSink);
                    if (crt->recv_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                        uecSink->set_end_trigger(*trig);
                    }

                    // lcpEpochAgent->doNextEvent();
                    // uecRtxScanner->registerUec(*lcpSrc);

                    switch (route_strategy) {
                        case ECMP_FIB:
                        case ECMP_FIB_ECN:
                        case SIMPLE_SUBFLOW:
                        case PLB:
                        case ECMP_RANDOM_ECN:
                        case ECMP_RANDOM2_ECN:
                        case SINGLE_PATH:
                        case REACTIVE_ECN: {
                            Route *srctotor = new Route();
                            Route *dsttotor = new Route();

                            if (top != NULL) {
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);
                                uecSrc->src_dc = top_dc->get_dc_id(src);
                                uecSrc->dest_dc = top_dc->get_dc_id(dest);
                                uecSrc->updateParams();

                                // printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());

                                dsttotor->push_back(
                                        top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());
                            }

                            uecSrc->from = src;
                            uecSrc->to = dest;
                            uecSink->from = src;
                            uecSink->to = dest;
                            // printf("Creating2 Flow from %d to %d\n", uecSrc->from, uecSrc->to);
                            uecSrc->connect(srctotor, dsttotor, *uecSink, crt->start);
                            uecSrc->set_paths(number_entropies);
                            uecSink->set_paths(number_entropies);

                            // register src and snk to receive packets src their respective
                            // TORs.
                            if (top != NULL) {
                                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, uecSrc->flow_id(), uecSrc);
                                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, uecSrc->flow_id(), uecSink);
                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);

                                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                                        src % top_dc->no_of_nodes(), uecSrc->flow_id(), uecSrc);
                                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                                        dest % top_dc->no_of_nodes(), uecSrc->flow_id(), uecSink);
                            }
                            break;
                        }
                        case NOT_SET: {
                            cout << "Route strategy NOT_SET" << endl;
                            abort();
                            break;
                        }
                        default: {
                            cout << "Unknown Route strategy " << route_strategy << endl;
                            abort();
                            break;
                        }
                    }
                } else if (use_inter_lcp) {
                    cout << "Using inter-LCP" << endl;

                    lcpSrc = new LcpSrc(NULL, NULL, eventlist, rtt, bdp, 100, 6);

                    lcpRtxScanner.registerLcp(*lcpSrc);

                    lcpSrc->setNumberEntropies(256);
                    inter_srcs.push_back(lcpSrc);

                    lcpSrc->set_pacing_delay(pacing_delay);

                    lcpSrc->set_dst(dest);
                    // printf("Reaching here\n");
                    if (crt->flowid) {
                        lcpSrc->set_flowid(crt->flowid);
                        assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                        flowmap[crt->flowid] = lcpSrc;
                    }

                    if (crt->size > 0) {
                        lcpSrc->setFlowSize(crt->size);
                    }

                    if (crt->trigger) {
                        Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                        trig->add_target(*lcpSrc);
                    }
                    if (crt->send_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                        lcpSrc->set_end_trigger(*trig);
                    }

                    lcpSink = new LcpSink();

                    lcpSrc->setName("lcp_" + ntoa(src) + "_" + ntoa(dest) + "_" + ntoa(crt->flowid));

                    // cout << "lcp_" + ntoa(src) + "_" + ntoa(dest) << endl;
                    logfile.writeName(*lcpSrc);

                    lcpSink->set_src(src);

                    lcpSink->setName("lcp_sink_" + ntoa(src) + "_" + ntoa(dest));
                    logfile.writeName(*lcpSink);
                    if (crt->recv_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                        lcpSink->set_end_trigger(*trig);
                    }

                    lcpSrc->set_use_rtt(LCP_USE_RTT);
                    if (use_inter_dc_ecn) {
                        cout << "Setting use_rtt for lcp's inter-dc flows to false since we have inter-DC ECN" << endl;
                        lcpSrc->set_use_rtt(false);
                    }
                    lcpSrc->set_use_ecn(LCP_USE_ECN);
                    lcpSrc->set_use_pacing(LCP_USE_PACING);
                    lcpSrc->set_use_qa(LCP_USE_QUICK_ADAPT_INTER);
                    lcpSrc->set_use_fi(LCP_USE_FAST_INCREASE);
                    lcpSrc->set_use_rto_cwnd_reduction(LCP_RTO_REDUCE_CWND);
                    lcpSrc->set_use_trimming(false);
                    lcpSink->set_use_trimming(false);
                    lcpSrc->set_lcp_k(LCP_K);
                    lcpSrc->set_lcp_k_scale(lcp_k_scale); //scale lcp_k
                    lcpSrc->set_lcp_ecn_alpha(LCP_ECN_ALPHA);
                    lcpSrc->set_target_to_baremetal_ratio(TARGET_TO_BAREMETAL_RATIO);
                    lcpSrc->set_starting_cwnd_bdp_ratio(STARTING_CWND_BDP_RATIO);
                    lcpSrc->set_constant_timestamp_epoch_ratio(bdp_intra * 1.0 / bdp_inter);
                    lcpSrc->set_md_gain_ecn(lcp_md_gain_ecn_inter);
                    lcpSrc->set_md_factor_rtt(lcp_md_gain_rtt);
                    lcpSrc->set_maxcwnd_bdp_ratio(MAX_CWND_BDP_RATIO);
                    lcpSrc->set_fi_threshold(LCP_FAST_INCREASE_THRESHOLD_INTER);
                    lcpSrc->set_apply_ai_per_epoch(LCP_APPLY_AI_PER_EPOCH);
                    lcpSrc->set_cc_algorithm_type(cc_algorithm_inter);

                    // erasure coding
                    lcpSrc->set_use_erasure_coding(use_erasure_coding);
                    lcpSink->set_use_erasure_coding(use_erasure_coding);
                    if (use_erasure_coding) {
                        lcpSrc->set_parity_group_size(parity_group_size);
                        lcpSink->set_parity_group_size(parity_group_size);
                        lcpSrc->set_parity_correction_capacity(parity_correction_capacity);
                        lcpSink->set_parity_correction_capacity(parity_correction_capacity);
                        lcpSrc->set_ec_handler(ec_handler);
                        lcpSink->set_ec_handler(ec_handler);
                        if (ec_handler == "dst")
                            lcpParityScanner.registerLcp(*lcpSink);
                    }

                    // MIMD
                    lcpSrc->lcp_type == lcpAlgo;
                    lcpSrc->set_kmin(kmin / 100.0);
                    lcpSrc->set_kmax(kmax / 100.0);

                    // per ack MD
                    lcpSrc->set_use_per_ack_md(lcp_per_ack_md);

                    // per ack EWMA
                    lcpSrc->set_use_per_ack_ewma(lcp_per_ack_ewma);

                    if (AI_BYTES_INTER != 1)
                        lcpSrc->set_ai_bytes(AI_BYTES_INTER);

                    lcpSrc->set_ai_bytes_scale(ai_bytes_scale);
                    
                    if (LCP_USE_DIFFERENT_QUEUES) {
                        lcpSrc->set_queue_idx(1);
                        lcpSink->set_queue_idx(1);
                    }

                    // lcpEpochAgent->doNextEvent();
                    // uecRtxScanner->registerUec(*lcpSrc);

                    switch (route_strategy) {
                        case ECMP_FIB:
                        case ECMP_FIB_ECN:
                        case ECMP_RANDOM2_ECN:
                        case SINGLE_PATH:
                        case SIMPLE_SUBFLOW:
                        case PLB:
                        case ECMP_RANDOM_ECN:
                        case SCATTER_RANDOM:
                        case REACTIVE_ECN: {
                            Route *srctotor = new Route();
                            Route *dsttotor = new Route();

                            if (top != NULL) {
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);
                                lcpSrc->src_dc = top_dc->get_dc_id(src);
                                lcpSrc->dest_dc = top_dc->get_dc_id(dest);
                                lcpSrc->force_cwnd = force_cwnd ;
                                lcpSrc->updateParams(base_intra_rtt, base_inter_rtt, bdp_intra, bdp_inter, intra_queuesize, inter_queuesize);

                                // printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());

                                dsttotor->push_back(
                                        top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());
                            }

                            lcpSrc->from = src;
                            lcpSrc->to = dest;
                            lcpSink->from = src;
                            lcpSink->to = dest;
                            // printf("Creating2 Flow from %d to %d\n", lcpSrc->from, lcpSrc->to);
                            lcpSrc->connect(srctotor, dsttotor, *lcpSink, crt->start);
                            lcpSrc->set_paths(number_entropies);
                            lcpSink->set_paths(number_entropies);

                            // register src and snk to receive packets src their respective
                            // TORs.
                            if (top != NULL) {
                                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, lcpSrc->flow_id(), lcpSrc);
                                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, lcpSrc->flow_id(), lcpSink);
                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);

                                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                                        src % top_dc->no_of_nodes(), lcpSrc->flow_id(), lcpSrc);
                                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                                        dest % top_dc->no_of_nodes(), lcpSrc->flow_id(), lcpSink);
                            }
                            break;
                        }
                        case NOT_SET: {
                            cout << "Route strategy NOT_SET" << endl;
                            abort();
                            break;
                        }
                        default: {
                            cout << "Unknown Route strategy " << route_strategy << endl;
                            abort();
                            break;
                        }
                    }
                }  else if (use_inter_gemini) {
                    cout << "Using inter-Gemini" << endl;

                    geminiSrc = new GeminiSrc(NULL, NULL, eventlist, rtt, bdp, 100, 6);

                    lcpRtxScanner.registerLcp(*geminiSrc);

                    geminiSrc->setNumberEntropies(256);
                    inter_srcs.push_back(geminiSrc);

                    geminiSrc->set_H_gemini(H_gemini);
                    geminiSrc->set_beta_gemini(beta_gemini);
                    geminiSrc->set_T_gemini(T_gemini);

                    geminiSrc->set_dst(dest);
                    // printf("Reaching here\n");
                    if (crt->flowid) {
                        geminiSrc->set_flowid(crt->flowid);
                        assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                        flowmap[crt->flowid] = geminiSrc;
                    }

                    if (crt->size > 0) {
                        geminiSrc->setFlowSize(crt->size);
                    }

                    if (crt->trigger) {
                        Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                        trig->add_target(*geminiSrc);
                    }
                    if (crt->send_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                        geminiSrc->set_end_trigger(*trig);
                    }

                    lcpSink = new LcpSink();

                    geminiSrc->setName("gemini_" + ntoa(src) + "_" + ntoa(dest) + "_" + ntoa(crt->flowid));

                    // cout << "lcp_" + ntoa(src) + "_" + ntoa(dest) << endl;
                    logfile.writeName(*geminiSrc);

                    lcpSink->set_src(src);

                    lcpSink->setName("lcp_sink_" + ntoa(src) + "_" + ntoa(dest));
                    logfile.writeName(*lcpSink);
                    if (crt->recv_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                        lcpSink->set_end_trigger(*trig);
                    }

                    geminiSrc->set_use_rtt(LCP_USE_RTT);
                    if (use_inter_dc_ecn) {
                        cout << "Setting use_rtt for gemini's inter-dc flows to false since we have inter-DC ECN" << endl;
                        geminiSrc->set_use_rtt(false);
                    }
                    geminiSrc->set_use_ecn(LCP_USE_ECN);
                    geminiSrc->set_use_pacing(LCP_USE_PACING);
                    // Gemini has no QA/FI
                    geminiSrc->set_use_qa(false);
                    geminiSrc->set_use_fi(false);
                    geminiSrc->set_use_rto_cwnd_reduction(LCP_RTO_REDUCE_CWND);
                    geminiSrc->set_use_trimming(false);
                    lcpSink->set_use_trimming(false);
                    geminiSrc->set_lcp_k(LCP_K);
                    geminiSrc->set_lcp_ecn_alpha(LCP_ECN_ALPHA);
                    geminiSrc->set_target_to_baremetal_ratio(TARGET_TO_BAREMETAL_RATIO);
                    geminiSrc->set_starting_cwnd_bdp_ratio(STARTING_CWND_BDP_RATIO);
                    geminiSrc->set_constant_timestamp_epoch_ratio(bdp_intra * 1.0 / bdp_inter);
                    geminiSrc->set_md_gain_ecn(lcp_md_gain_ecn_inter);
                    geminiSrc->set_md_factor_rtt(lcp_md_gain_rtt);
                    geminiSrc->set_maxcwnd_bdp_ratio(MAX_CWND_BDP_RATIO);
                    geminiSrc->set_fi_threshold(LCP_FAST_INCREASE_THRESHOLD_INTER);
                    geminiSrc->set_apply_ai_per_epoch(false);
                    geminiSrc->set_cc_algorithm_type(cc_algorithm_inter);

                    // erasure coding
                    geminiSrc->set_use_erasure_coding(use_erasure_coding);
                    lcpSink->set_use_erasure_coding(use_erasure_coding);
                    if (use_erasure_coding) {
                        geminiSrc->set_parity_group_size(parity_group_size);
                        lcpSink->set_parity_group_size(parity_group_size);
                        geminiSrc->set_parity_correction_capacity(parity_correction_capacity);
                        lcpSink->set_parity_correction_capacity(parity_correction_capacity);
                        geminiSrc->set_ec_handler(ec_handler);
                        lcpSink->set_ec_handler(ec_handler);
                        if (ec_handler == "dst")
                            lcpParityScanner.registerLcp(*lcpSink);
                    }

                    // per ack MD
                    geminiSrc->set_use_per_ack_md(false);

                    // per ack EWMA
                    geminiSrc->set_use_per_ack_ewma(lcp_per_ack_ewma);

                    if (AI_BYTES_INTER != 1)
                        geminiSrc->set_ai_bytes(AI_BYTES_INTER);
                    
                    if (LCP_USE_DIFFERENT_QUEUES) {
                        geminiSrc->set_queue_idx(1);
                        lcpSink->set_queue_idx(1);
                    }

                    // lcpEpochAgent->doNextEvent();
                    // uecRtxScanner->registerUec(*geminiSrc);

                    switch (route_strategy) {
                        case ECMP_FIB:
                        case ECMP_FIB_ECN:
                        case ECMP_RANDOM2_ECN:
                        case ECMP_RANDOM_ECN:
                        case SINGLE_PATH:
                        case SCATTER_RANDOM:
                        case REACTIVE_ECN: {
                            Route *srctotor = new Route();
                            Route *dsttotor = new Route();

                            if (top != NULL) {
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);
                                geminiSrc->src_dc = top_dc->get_dc_id(src);
                                geminiSrc->dest_dc = top_dc->get_dc_id(dest);
                                geminiSrc->updateParams(base_intra_rtt, base_inter_rtt, bdp_intra, bdp_inter, intra_queuesize, inter_queuesize);

                                // printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());

                                dsttotor->push_back(
                                        top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());
                            }

                            geminiSrc->from = src;
                            geminiSrc->to = dest;
                            lcpSink->from = src;
                            lcpSink->to = dest;
                            // printf("Creating2 Flow from %d to %d\n", geminiSrc->from, geminiSrc->to);
                            geminiSrc->connect(srctotor, dsttotor, *lcpSink, crt->start);
                            geminiSrc->set_paths(number_entropies);
                            lcpSink->set_paths(number_entropies);

                            // register src and snk to receive packets src their respective
                            // TORs.
                            if (top != NULL) {
                                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, geminiSrc->flow_id(), geminiSrc);
                                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, geminiSrc->flow_id(), lcpSink);
                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);

                                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                                        src % top_dc->no_of_nodes(), geminiSrc->flow_id(), geminiSrc);
                                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                                        dest % top_dc->no_of_nodes(), geminiSrc->flow_id(), lcpSink);
                            }
                            break;
                        }
                        case NOT_SET: {
                            cout << "Route strategy NOT_SET" << endl;
                            abort();
                            break;
                        }
                        default: {
                            cout << "Unknown Route strategy " << route_strategy << endl;
                            abort();
                            break;
                        }
                    }
                } else {
                    cout << "No inter algorithm set up. Abort!" << endl;
                    abort();
                }
            } else {
                
                cout << "Flow from " << src << " to " << dest << " is Intra-DC (ID: " << crt->flowid << ", size: " << crt->size << "). ";
                if (use_mprdma) {
                    cout << "Using MPRDMA" << endl;
                    MprdmaSrc::set_starting_cwnd(bdp);

                    mprdmaSrc = new MprdmaSrc(NULL, NULL, eventlist, rtt, bdp, 100, 6);
                    mprdmaRtxScanner.registerMprdma(*mprdmaSrc);

                    mprdmaSrc->setNumberEntropies(256);
                    intra_mprdma_srcs.push_back(mprdmaSrc);

                    mprdmaSrc->set_dst(dest);
                    // printf("Reaching here\n");
                    if (crt->flowid) {
                        mprdmaSrc->set_flowid(crt->flowid);
                        assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                        flowmap[crt->flowid] = mprdmaSrc;
                    }

                    if (crt->size > 0) {
                        mprdmaSrc->setFlowSize(crt->size);
                    }

                    if (crt->trigger) {
                        Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                        trig->add_target(*mprdmaSrc);
                    }
                    if (crt->send_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                        mprdmaSrc->set_end_trigger(*trig);
                    }

                    mprdmaSink = new MprdmaSink();

                    mprdmaSrc->setName("mprdma_" + ntoa(src) + "_" + ntoa(dest) + "_" + ntoa(crt->flowid));

                    // cout << "mprdma_" + ntoa(src) + "_" + ntoa(dest) << endl;
                    logfile.writeName(*mprdmaSrc);

                    mprdmaSink->set_src(src);

                    mprdmaSink->setName("mprdma_sink_" + ntoa(src) + "_" + ntoa(dest));
                    logfile.writeName(*mprdmaSink);
                    if (crt->recv_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                        mprdmaSink->set_end_trigger(*trig);
                    }

                    // mprdmaEpochAgent->doNextEvent();
                    // mprdmaRtxScanner->registerMprdma(*mprdmaSrc);

                    switch (route_strategy) {
                        case ECMP_FIB:
                        case ECMP_FIB_ECN:
                        case PLB:
                        case ECMP_RANDOM2_ECN:
                        case SINGLE_PATH:
                        case SIMPLE_SUBFLOW:
                        case ECMP_RANDOM_ECN:
                        case SCATTER_RANDOM:
                        case REACTIVE_ECN: {
                            Route *srctotor = new Route();
                            Route *dsttotor = new Route();

                            if (top != NULL) {
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);
                                mprdmaSrc->src_dc = top_dc->get_dc_id(src);
                                mprdmaSrc->dest_dc = top_dc->get_dc_id(dest);
                                mprdmaSrc->updateParams();

                                // printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());

                                dsttotor->push_back(
                                        top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());
                            }

                            mprdmaSrc->from = src;
                            mprdmaSrc->to = dest;
                            mprdmaSink->from = src;
                            mprdmaSink->to = dest;
                            // printf("Creating2 Flow from %d to %d\n", mprdmaSrc->from, mprdmaSrc->to);
                            mprdmaSrc->connect(srctotor, dsttotor, *mprdmaSink, crt->start);
                            mprdmaSrc->set_paths(number_entropies);
                            mprdmaSink->set_paths(number_entropies);

                            // register src and snk to receive packets src their respective
                            // TORs.
                            if (top != NULL) {
                                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, mprdmaSrc->flow_id(), mprdmaSrc);
                                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, mprdmaSrc->flow_id(), mprdmaSink);
                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);

                                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                                        src % top_dc->no_of_nodes(), mprdmaSrc->flow_id(), mprdmaSrc);
                                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                                        dest % top_dc->no_of_nodes(), mprdmaSrc->flow_id(), mprdmaSink);
                            }
                            break;
                        }
                        case NOT_SET: {
                            cout << "Route strategy NOT_SET" << endl;
                            abort();
                            break;
                        }
                        default: {
                            cout << "Unknown Route strategy " << route_strategy << endl;
                            abort();
                            break;
                        }
                    }
                } else if (use_intra_lcp) {
                    cout << "Using intra-LCP" << endl;

                    lcpSrc = new LcpSrc(NULL, NULL, eventlist, rtt, bdp, 100, 6);

                    lcpRtxScanner.registerLcp(*lcpSrc);

                    lcpSrc->setNumberEntropies(256);
                    intra_lcp_srcs.push_back(lcpSrc);

                    lcpSrc->set_pacing_delay(pacing_delay);

                    lcpSrc->set_dst(dest);
                    // printf("Reaching here\n");
                    if (crt->flowid) {
                        lcpSrc->set_flowid(crt->flowid);
                        assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                        flowmap[crt->flowid] = lcpSrc;
                    }

                    if (crt->size > 0) {
                        lcpSrc->setFlowSize(crt->size);
                    }

                    if (crt->trigger) {
                        Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                        trig->add_target(*lcpSrc);
                    }
                    if (crt->send_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                        lcpSrc->set_end_trigger(*trig);
                    }

                    lcpSink = new LcpSink();

                    lcpSrc->setName("lcp_" + ntoa(src) + "_" + ntoa(dest) + "_" + ntoa(crt->flowid));

                    // cout << "lcp_" + ntoa(src) + "_" + ntoa(dest) << endl;
                    logfile.writeName(*lcpSrc);

                    lcpSink->set_src(src);

                    lcpSink->setName("lcp_sink_" + ntoa(src) + "_" + ntoa(dest));
                    logfile.writeName(*lcpSink);
                    if (crt->recv_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                        lcpSink->set_end_trigger(*trig);
                    }

                    // lcpEpochAgent->doNextEvent();
                    // uecRtxScanner->registerUec(*lcpSrc);

                    lcpSrc->set_use_rtt(false);
                    lcpSrc->set_use_ecn(true);
                    lcpSrc->set_use_pacing(LCP_USE_PACING);
                    lcpSrc->set_use_qa(LCP_USE_QUICK_ADAPT_INTRA);
                    lcpSrc->set_use_fi(LCP_USE_FAST_INCREASE);
                    lcpSrc->set_use_rto_cwnd_reduction(LCP_RTO_REDUCE_CWND);
                    lcpSrc->set_lcp_k(LCP_K);
                    // lcpSrc->set_lcp_k_scale(lcp_k_scale); //scale lcp_k
                    lcpSrc->set_lcp_ecn_alpha(LCP_ECN_ALPHA);
                    lcpSrc->set_use_trimming(false);
                    lcpSink->set_use_trimming(false);
                    lcpSrc->set_target_to_baremetal_ratio(TARGET_TO_BAREMETAL_RATIO);
                    lcpSrc->set_starting_cwnd_bdp_ratio(STARTING_CWND_BDP_RATIO);
                    lcpSrc->set_fi_threshold(LCP_FAST_INCREASE_THRESHOLD_INTRA);
                    lcpSrc->set_apply_ai_per_epoch(LCP_APPLY_AI_PER_EPOCH);
                    lcpSrc->set_md_gain_ecn(lcp_md_gain_ecn_intra);
                    lcpSrc->set_cc_algorithm_type(cc_algorithm_intra);

                    // erasure coding
                    lcpSrc->set_use_erasure_coding(use_erasure_coding);
                    lcpSink->set_use_erasure_coding(use_erasure_coding);
                    if (use_erasure_coding) {
                        lcpSrc->set_parity_group_size(parity_group_size);
                        lcpSink->set_parity_group_size(parity_group_size);
                        lcpSrc->set_parity_correction_capacity(parity_correction_capacity);
                        lcpSink->set_parity_correction_capacity(parity_correction_capacity);
                        lcpSrc->set_ec_handler(ec_handler);
                        lcpSink->set_ec_handler(ec_handler);
                        if (ec_handler == "dst")
                            lcpParityScanner.registerLcp(*lcpSink);
                    }

                    // MIMD
                    lcpSrc->lcp_type == lcpAlgo;
                    lcpSrc->set_kmin(kmin / 100.0);
                    lcpSrc->set_kmax(kmax / 100.0);

                    // per ack MD
                    lcpSrc->set_use_per_ack_md(lcp_per_ack_md);

                    // per ack EWMA
                    lcpSrc->set_use_per_ack_ewma(lcp_per_ack_ewma);

                    if (AI_BYTES_INTRA != 1)
                        lcpSrc->set_ai_bytes(AI_BYTES_INTRA);

                    lcpSrc->set_ai_bytes_scale(ai_bytes_scale);
                    
                    if (LCP_USE_DIFFERENT_QUEUES) {
                        lcpSrc->set_queue_idx(0);
                        lcpSink->set_queue_idx(0);
                    }

                    switch (route_strategy) {
                        case ECMP_FIB:
                        case ECMP_FIB_ECN:
                        case ECMP_RANDOM2_ECN:
                        case SINGLE_PATH:
                        case SCATTER_RANDOM:
                        case PLB:
                        case SIMPLE_SUBFLOW:
                        case ECMP_RANDOM_ECN:
                        case REACTIVE_ECN: {
                            Route *srctotor = new Route();
                            Route *dsttotor = new Route();

                            if (top != NULL) {
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);
                                lcpSrc->src_dc = top_dc->get_dc_id(src);
                                lcpSrc->dest_dc = top_dc->get_dc_id(dest);
                                lcpSrc->force_cwnd = force_cwnd;
                                lcpSrc->updateParams(base_intra_rtt, base_inter_rtt, bdp_intra, bdp_inter, intra_queuesize, inter_queuesize);

                                // printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());

                                dsttotor->push_back(
                                        top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());
                            }

                            lcpSrc->from = src;
                            lcpSrc->to = dest;
                            lcpSink->from = src;
                            lcpSink->to = dest;
                            // printf("Creating2 Flow from %d to %d\n", lcpSrc->from, lcpSrc->to);
                            lcpSrc->connect(srctotor, dsttotor, *lcpSink, crt->start);
                            lcpSrc->set_paths(number_entropies);
                            lcpSink->set_paths(number_entropies);

                            // register src and snk to receive packets src their respective
                            // TORs.
                            if (top != NULL) {
                                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, lcpSrc->flow_id(), lcpSrc);
                                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, lcpSrc->flow_id(), lcpSink);
                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);

                                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                                        src % top_dc->no_of_nodes(), lcpSrc->flow_id(), lcpSrc);
                                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                                        dest % top_dc->no_of_nodes(), lcpSrc->flow_id(), lcpSink);
                            }
                            break;
                        }
                        case NOT_SET: {
                            cout << "Route strategy NOT_SET" << endl;
                            abort();
                            break;
                        }
                        default: {
                            cout << "Unknown Route strategy " << route_strategy << endl;
                            abort();
                            break;
                        }
                    }
                } else if (use_intra_gemini) {
                    cout << "Using intra-Gemini" << endl;

                    geminiSrc = new GeminiSrc(NULL, NULL, eventlist, rtt, bdp, 100, 6);

                    lcpRtxScanner.registerLcp(*geminiSrc);

                    geminiSrc->setNumberEntropies(256);
                    intra_lcp_srcs.push_back(geminiSrc);

                    geminiSrc->set_H_gemini(H_gemini);
                    geminiSrc->set_beta_gemini(beta_gemini);
                    geminiSrc->set_T_gemini(T_gemini);

                    geminiSrc->set_dst(dest);
                    // printf("Reaching here\n");
                    if (crt->flowid) {
                        geminiSrc->set_flowid(crt->flowid);
                        assert(flowmap.find(crt->flowid) == flowmap.end()); // don't have dups
                        flowmap[crt->flowid] = geminiSrc;
                    }

                    if (crt->size > 0) {
                        geminiSrc->setFlowSize(crt->size);
                    }

                    if (crt->trigger) {
                        Trigger *trig = conns->getTrigger(crt->trigger, eventlist);
                        trig->add_target(*geminiSrc);
                    }
                    if (crt->send_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->send_done_trigger, eventlist);
                        geminiSrc->set_end_trigger(*trig);
                    }

                    lcpSink = new LcpSink();

                    geminiSrc->setName("gemini_" + ntoa(src) + "_" + ntoa(dest) + "_" + ntoa(crt->flowid));

                    // cout << "lcp_" + ntoa(src) + "_" + ntoa(dest) << endl;
                    logfile.writeName(*geminiSrc);

                    // don't need to change lcp sink for gemini
                    lcpSink->set_src(src);

                    lcpSink->setName("lcp_sink_" + ntoa(src) + "_" + ntoa(dest));
                    logfile.writeName(*lcpSink);
                    if (crt->recv_done_trigger) {
                        Trigger *trig = conns->getTrigger(crt->recv_done_trigger, eventlist);
                        lcpSink->set_end_trigger(*trig);
                    }

                    // lcpEpochAgent->doNextEvent();
                    // uecRtxScanner->registerUec(*geminiSrc);

                    geminiSrc->set_use_rtt(false);
                    geminiSrc->set_use_ecn(true);
                    geminiSrc->set_use_pacing(LCP_USE_PACING);
                    // Gemini has no QA/FI
                    geminiSrc->set_use_qa(false);
                    geminiSrc->set_use_fi(false);
                    geminiSrc->set_use_rto_cwnd_reduction(LCP_RTO_REDUCE_CWND);
                    geminiSrc->set_lcp_k(LCP_K);
                    geminiSrc->set_lcp_ecn_alpha(LCP_ECN_ALPHA);
                    geminiSrc->set_use_trimming(false);
                    lcpSink->set_use_trimming(false);
                    geminiSrc->set_target_to_baremetal_ratio(TARGET_TO_BAREMETAL_RATIO);
                    geminiSrc->set_starting_cwnd_bdp_ratio(STARTING_CWND_BDP_RATIO);
                    geminiSrc->set_fi_threshold(LCP_FAST_INCREASE_THRESHOLD_INTRA);
                    geminiSrc->set_apply_ai_per_epoch(false);
                    geminiSrc->set_md_gain_ecn(lcp_md_gain_ecn_intra);
                    geminiSrc->set_cc_algorithm_type(cc_algorithm_intra);

                    // erasure coding
                    geminiSrc->set_use_erasure_coding(use_erasure_coding);
                    lcpSink->set_use_erasure_coding(use_erasure_coding);
                    if (use_erasure_coding) {
                        geminiSrc->set_parity_group_size(parity_group_size);
                        lcpSink->set_parity_group_size(parity_group_size);
                        geminiSrc->set_parity_correction_capacity(parity_correction_capacity);
                        lcpSink->set_parity_correction_capacity(parity_correction_capacity);
                        geminiSrc->set_ec_handler(ec_handler);
                        lcpSink->set_ec_handler(ec_handler);
                        if (ec_handler == "dst")
                            lcpParityScanner.registerLcp(*lcpSink);
                    }

                    // per ack MD
                    geminiSrc->set_use_per_ack_md(false);

                    // per ack EWMA
                    geminiSrc->set_use_per_ack_ewma(lcp_per_ack_ewma);

                    if (AI_BYTES_INTRA != 1)
                    geminiSrc->set_ai_bytes(AI_BYTES_INTRA);
                    
                    if (LCP_USE_DIFFERENT_QUEUES) {
                        geminiSrc->set_queue_idx(0);
                        lcpSink->set_queue_idx(0);
                    }

                    switch (route_strategy) {
                        case ECMP_FIB:
                        case ECMP_FIB_ECN:
                        case ECMP_RANDOM2_ECN:
                        case ECMP_RANDOM_ECN:
                        case SINGLE_PATH:
                        case SCATTER_RANDOM:
                        case REACTIVE_ECN: {
                            Route *srctotor = new Route();
                            Route *dsttotor = new Route();

                            if (top != NULL) {
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->pipes_ns_nlp[src][top->HOST_POD_SWITCH(src)]);
                                srctotor->push_back(top->queues_ns_nlp[src][top->HOST_POD_SWITCH(src)]->getRemoteEndpoint());

                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->pipes_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]);
                                dsttotor->push_back(top->queues_ns_nlp[dest][top->HOST_POD_SWITCH(dest)]->getRemoteEndpoint());

                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);
                                geminiSrc->src_dc = top_dc->get_dc_id(src);
                                geminiSrc->dest_dc = top_dc->get_dc_id(dest);
                                geminiSrc->updateParams(base_intra_rtt, base_inter_rtt, bdp_intra, bdp_inter, intra_queuesize, inter_queuesize);

                                // printf("Source in Datacenter %d - Dest in Datacenter %d\n", idx_dc, idx_dc_to);

                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->pipes_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]);
                                srctotor->push_back(top_dc->queues_ns_nlp[idx_dc][src % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());

                                dsttotor->push_back(
                                        top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                            [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->pipes_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]);
                                dsttotor->push_back(top_dc->queues_ns_nlp[idx_dc_to][dest % top_dc->no_of_nodes()]
                                                                        [top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())][0]
                                                                                ->getRemoteEndpoint());
                            }

                            geminiSrc->from = src;
                            geminiSrc->to = dest;
                            lcpSink->from = src;
                            lcpSink->to = dest;
                            // printf("Creating2 Flow from %d to %d\n", geminiSrc->from, geminiSrc->to);
                            geminiSrc->connect(srctotor, dsttotor, *lcpSink, crt->start);
                            geminiSrc->set_paths(number_entropies);
                            lcpSink->set_paths(number_entropies);

                            // register src and snk to receive packets src their respective
                            // TORs.
                            if (top != NULL) {
                                top->switches_lp[top->HOST_POD_SWITCH(src)]->addHostPort(src, geminiSrc->flow_id(), geminiSrc);
                                top->switches_lp[top->HOST_POD_SWITCH(dest)]->addHostPort(dest, geminiSrc->flow_id(), lcpSink);
                            } else if (top_dc != NULL) {
                                int idx_dc = top_dc->get_dc_id(src);
                                int idx_dc_to = top_dc->get_dc_id(dest);

                                top_dc->switches_lp[idx_dc][top_dc->HOST_POD_SWITCH(src % top_dc->no_of_nodes())]->addHostPort(
                                        src % top_dc->no_of_nodes(), geminiSrc->flow_id(), geminiSrc);
                                top_dc->switches_lp[idx_dc_to][top_dc->HOST_POD_SWITCH(dest % top_dc->no_of_nodes())]->addHostPort(
                                        dest % top_dc->no_of_nodes(), geminiSrc->flow_id(), lcpSink);
                            }
                            break;
                        }
                        case NOT_SET: {
                            cout << "Route strategy NOT_SET" << endl;
                            abort();
                            break;
                        }
                        default: {
                            cout << "Unknown Route strategy " << route_strategy << endl;
                            abort();
                            break;
                        }
                    }
                } else {
                    cout << "No intra algorithm set up. Abort!" << endl;
                    abort();
                }
            }
        }

        cout << "\nEntering the event running loop" << endl;
        while (eventlist.doNextEvent()) {}
        
        cout << "Event running loop done" << endl;

        for (std::size_t i = 0; i < inter_srcs.size(); ++i) {
            if (!inter_srcs[i]->_flow_finished) {
                printf("Inter-dc Flow %d did not finish\n", inter_srcs[i]->flow_id());
                // exit(-1);
            }
            delete inter_srcs[i];
        }
        for (std::size_t i = 0; i < intra_lcp_srcs.size(); ++i) {
            if (!intra_lcp_srcs[i]->_flow_finished) {
                printf("Intra-dc Flow %d did not finish\n", intra_lcp_srcs[i]->flow_id());
                // exit(-1);
            }
            delete intra_lcp_srcs[i];
        }
        for (std::size_t i = 0; i < intra_mprdma_srcs.size(); ++i) {
            if (!intra_mprdma_srcs[i]->_flow_finished) {
                printf("Intra-dc Flow %d did not finish\n", intra_mprdma_srcs[i]->flow_id());
                // exit(-1);
            }
            delete intra_mprdma_srcs[i];
        }
        for (std::size_t i = 0; i < bbr_srcs.size(); ++i) {
            if (!bbr_srcs[i]->_flow_finished) {
                printf("Intra-dc Flow %d did not finish\n", bbr_srcs[i]->flow_id());
                // exit(-1);
            }
            delete bbr_srcs[i];
        } 
        for (std::size_t i = 0; i < uec_srcs.size(); ++i) {
            if (!uec_srcs[i]->_flow_finished) {
                printf("Intra-dc Flow %d did not finish\n", uec_srcs[i]->flow_id());
                // exit(-1);
            }
            delete uec_srcs[i];
        }
    } else if (goal_filename.size() > 0) {
        printf("Starting LGS Interface");

        if (topology_normal) {
            printf("Normal Topology\n");
            FatTreeTopology::set_tiers(3);
            FatTreeTopology::set_os_stage_2(fat_tree_k);
            FatTreeTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            FatTreeTopology::set_bts_threshold(bts_threshold);
            FatTreeTopology::set_ignore_data_ecn(ignore_ecn_data);
            FatTreeTopology *top = new FatTreeTopology(no_of_nodes, linkspeed, intra_queuesize, NULL, &eventlist, ff,
                                                       queue_choice, hop_latency, switch_latency);
            lgs = new LogSimInterface(NULL, &traffic_logger, eventlist, top, NULL);
        } else {
            if (interdc_delay != 0) {
                FatTreeInterDCTopology::set_interdc_delay(interdc_delay);
                LcpSrc::set_interdc_delay(interdc_delay);
                BBRSrc::set_interdc_delay(interdc_delay);
            } else {
                FatTreeInterDCTopology::set_interdc_delay(hop_latency);
                LcpSrc::set_interdc_delay(hop_latency);
                BBRSrc::set_interdc_delay(hop_latency);
            }
            FatTreeInterDCTopology::set_tiers(3);
            FatTreeInterDCTopology::set_os_stage_2(fat_tree_k);
            FatTreeInterDCTopology::set_os_stage_1(ratio_os_stage_1);
            FatTreeInterDCTopology::set_ecn_thresholds_as_queue_percentage(kmin, kmax);
            FatTreeInterDCTopology::set_bts_threshold(bts_threshold);
            FatTreeInterDCTopology::set_ignore_data_ecn(ignore_ecn_data);
            FatTreeInterDCTopology *top = new FatTreeInterDCTopology(
                    no_of_nodes, linkspeed, intra_queuesize, inter_queuesize, NULL, &eventlist, ff, queue_choice, hop_latency, switch_latency);
            lgs = new LogSimInterface(NULL, &traffic_logger, eventlist, top, NULL);
        }

        lgs->set_protocol(UEC_PROTOCOL);
        lgs->set_cwd(cwnd);
        lgs->set_queue_size(intra_queuesize);
        lgs->setReuse(reuse_entropy);
        // lgs->setNumberEntropies(number_entropies);
        lgs->setIgnoreEcnAck(ignore_ecn_ack);
        lgs->setIgnoreEcnData(ignore_ecn_data);
        lgs->setNumberPaths(number_entropies);
        start_lgs(goal_filename, *lgs);
    }

    // Record the setup
    int pktsize = Packet::data_packet_size();
    logfile.write("# pktsize=" + ntoa(pktsize) + " bytes");
    logfile.write("# subflows=" + ntoa(subflow_count));
    logfile.write("# hostnicrate = " + ntoa(HOST_NIC) + " pkt/sec");
    logfile.write("# corelinkrate = " + ntoa(HOST_NIC * CORE_TO_HOST) + " pkt/sec");
    // logfile.write("# buffer = " + ntoa((double)
    // (queues_na_ni[0][1]->_maxsize) / ((double) pktsize)) + " pkt");
    double rtt = timeAsSec(timeFromUs(RTT));
    logfile.write("# rtt =" + ntoa(rtt));

    cout << "Done" << endl;
    list<const Route *>::iterator rt_i;
    int counts[10];
    int hop;
    for (int i = 0; i < 10; i++)
        counts[i] = 0;
    for (rt_i = routes.begin(); rt_i != routes.end(); rt_i++) {
        const Route *r = (*rt_i);
        // print_route(*r);
        cout << "Path:" << endl;
        hop = 0;
        for (std::size_t i = 0; i < r->size(); i++) {
            PacketSink *ps = r->at(i);
            CompositeQueue *q = dynamic_cast<CompositeQueue *>(ps);
            if (q == 0) {
                cout << ps->nodename() << endl;
            } else {
                cout << q->nodename() << " id=" << 0 /*q->id*/ << " " << q->num_packets() << "pkts " << q->num_headers()
                     << "hdrs " << q->num_acks() << "acks " << q->num_nacks() << "nacks " << q->num_stripped()
                     << "stripped" << endl; // TODO(tommaso): compositequeues don't have id.
                                            // Need to add that or find an alternative way.
                                            // Verify also that compositequeue is the right
                                            // queue to use here.
                counts[hop] += q->num_stripped();
                hop++;
            }
        }
        cout << endl;
    }
    for (int i = 0; i < 10; i++)
        cout << "Hop " << i << " Count " << counts[i] << endl;
}

string ntoa(double n) {
    stringstream s;
    s << n;
    return s.str();
}

string itoa(uint64_t n) {
    stringstream s;
    s << n;
    return s.str();
}
