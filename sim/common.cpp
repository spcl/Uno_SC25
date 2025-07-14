#include <filesystem>
#include <stdint.h>
#include <sys/types.h>

typedef uint64_t simtime_picosec;

// Global
simtime_picosec GLOBAL_TIME = 0; // Global variable for current sim time
bool COLLECT_DATA = true;
uint64_t HOPS = 6;                          // Max Hops Topology
uint64_t INFINITE_BUFFER_SIZE = 1000000000; // Assume infinite buffer space
std::string FOLDER_NAME = "sim/";

// Values for "modern" networking simulations
int PKT_SIZE_MODERN = 4096;       // Bytes
uint64_t LINK_SPEED_MODERN = 400; // Gb/s
uint64_t INTER_LINK_SPEED_MODERN = 100;
uint64_t SINGLE_PKT_TRASMISSION_TIME_MODERN =
        PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN;
int LINK_DELAY_MODERN = 400; // ns
uint64_t BASE_RTT_MODERN =
        (HOPS * LINK_DELAY_MODERN) +
        (PKT_SIZE_MODERN * 8 / LINK_SPEED_MODERN * HOPS) +
        (HOPS * LINK_DELAY_MODERN) +
        (40 * 8 / LINK_SPEED_MODERN * HOPS); // Target RTT in ns
// UEC Specific Values
uint64_t TARGET_RTT_MODERN = BASE_RTT_MODERN * 1.25; // Target RTT in ns
uint64_t BDP_MODERN_UEC = BASE_RTT_MODERN * LINK_SPEED_MODERN / 8; // BDP
uint64_t MAX_CWD_MODERN_UEC = BDP_MODERN_UEC * 1.0;                // BDP * 1.0
uint64_t MIN_K_ECN_MODERN =
        BDP_MODERN_UEC * 8 / LINK_SPEED_MODERN * 0.2; // 20% BDP
uint64_t MAX_K_ECN_MODERN =
        BDP_MODERN_UEC * 8 / LINK_SPEED_MODERN * 0.8; // 100% BDP
// NDP values
uint64_t BDP_MODERN_NDP = BASE_RTT_MODERN * LINK_SPEED_MODERN / 8; // BDP
uint64_t MAX_CWD_MODERN_NDP =
        BDP_MODERN_NDP *
        3; // BDP * 3. Based on their choice to use 23 cwd with 9000 pacekts
bool ENABLE_FAST_DROP = false;
bool IGNORE_ECN_DATA_BTS = true;

// Values for "old" networking simulations
int PKT_SIZE_OLD = 9000;      // Bytes
uint64_t LINK_SPEED_OLD = 10; // Gb/s
uint64_t FLOW_ACTIVE = 0; // Gb/s
int SINGLE_PKT_TRASMISSION_TIME_OLD = PKT_SIZE_OLD * 8 / LINK_SPEED_OLD;
int LINK_DELAY_OLD = 1000; // ns
uint64_t BASE_RTT_OLD =
        (HOPS * LINK_DELAY_OLD) + (PKT_SIZE_OLD * 8 / LINK_SPEED_OLD * HOPS) +
        (HOPS * LINK_DELAY_OLD) +
        (40 * 8 / LINK_SPEED_OLD * HOPS) * 1000; // Target RTT in ps
// UEC Specific Values
uint64_t TARGET_RTT_OLD = BASE_RTT_OLD * 1.2;             // Target RTT in ps
uint64_t BDP_OLD_UEC = BASE_RTT_OLD * LINK_SPEED_OLD / 8; // BDP
uint64_t MAX_CWD_OLD_UEC = BDP_OLD_UEC * 1.2;             // BDP * 1.2
uint64_t MIN_K_ECN_OLD = BDP_OLD_UEC * 8 / LINK_SPEED_OLD * 0.2; // 20% BDP
uint64_t MAX_K_ECN_OLD = BDP_OLD_UEC * 8 / LINK_SPEED_OLD * 1;   // 100% BDP
uint64_t BUFFER_SIZE_OLD = 8 * PKT_SIZE_OLD;                     // 8 Pkts
// NDP Specific Values
uint64_t BDP_OLD_NDP = BASE_RTT_OLD * LINK_SPEED_OLD / 8; // BDP
uint64_t MAX_CWD_OLD_NDP = BDP_OLD_NDP * 3;               // BDP * 1.2
std::filesystem::path PROJECT_ROOT_PATH;

// localized in lcp and can differ between inter- and intra- lcp
simtime_picosec TARGET_RTT = 0; // Low threshold to perform reductions for RTT.
simtime_picosec BAREMETAL_RTT = 0; // Baremetal latency.
uint32_t AI_BYTES = 1; // just for mprdma!
uint32_t AI_BYTES_INTRA = 1; // Additive increase constant for LCP intra.
uint32_t AI_BYTES_INTER = 1; // Additive increase constant for LCP inter.
float LCP_ECN_ALPHA = 1.0; // Gain for ECN fraction EWMA calculations.
double LCP_K = 50; // The marking threshold for LCP. if it's zero, it will be set to bdp/7
bool LCP_USE_QUICK_ADAPT_INTER = true; // Whether to use quick adapt.
bool LCP_USE_QUICK_ADAPT_INTRA = true; // Whether to use quick adapt.
bool LCP_USE_FAST_INCREASE = true; // Whether to use fast increase.
bool LCP_RTO_REDUCE_CWND = true; // Whether to reduce cwnd when packets get timedout.
bool LCP_USE_RTT = true; // Whether to use rtt in epoch.
bool LCP_USE_ECN = true; // Whether to use ecn in epoch.
bool LCP_USE_PACING = true; // Whether to use pacing.
bool LCP_USE_TRIMMING = false; // Whether to use trimming
simtime_picosec QA_TRIGGER_RTT = 0; // High threshold to perform QA for RTT.
float MD_GAIN_ECN = 1; // Multiplicative decrease gain for ECN-based reduction
float MD_FACTOR_RTT = 1; // Multiplicative decrease gain for RTT-based reduction
simtime_picosec TARGET_QDELAY = 0;
float TARGET_TO_BAREMETAL_RATIO = 1.05;
float STARTING_CWND_BDP_RATIO = 1.0;
float MAX_CWND_BDP_RATIO = 1.0;
bool LCP_USE_DIFFERENT_QUEUES = false; // Whether to use different queues for inter- and intra-DC flows.
bool LCP_APPLY_AI_PER_EPOCH = false;

// global and shared between inter- and intra- lcp
uint32_t LCP_FAST_INCREASE_THRESHOLD_INTRA = 3; // Number of consecutive good epochs to trigger fast increase.
uint32_t LCP_FAST_INCREASE_THRESHOLD_INTER = 3; // Number of consecutive good epochs to trigger fast increase.
double LCP_PACING_BONUS = 0.0; // The reduction to perform to pacing to reduce underutilization.
float QA_CWND_RATIO_THRESHOLD = 0.5;
float QA_TRIGGER_RTT_FRACTION = 0.9; // Fraction of queueing latency to add to the baremetal RTT.