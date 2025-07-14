// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#ifndef CONFIG_H
#define CONFIG_H

#include <assert.h>
#include <filesystem>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>

double drand();

#ifdef _WIN32
// Ways to refer to integer types
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef signed __int64 sint64_t;
#else
typedef long long sint64_t;
#endif

// Specify units for simulation time, link speed, buffer capacity
typedef uint64_t simtime_picosec;

extern simtime_picosec GLOBAL_TIME;
extern std::string FOLDER_NAME;
extern int PKT_SIZE_MODERN;
extern uint64_t LINK_SPEED_MODERN;
extern uint64_t INTER_LINK_SPEED_MODERN;
extern int SINGLE_PKT_TRASMISSION_TIME_MODERN;
extern int LINK_DELAY_MODERN;
extern uint64_t HOPS;
extern uint64_t BASE_RTT_MODERN;
extern uint64_t TARGET_RTT_MODERN;
extern uint64_t BDP_MODERN_UEC;
extern uint64_t MAX_CWD_MODERN_UEC;
extern bool COLLECT_DATA;
extern bool SIM_DONE;
extern uint64_t MAX_CWD_MODERN_UEC;
extern uint64_t MIN_K_ECN_MODERN;
extern uint64_t MAX_K_ECN_MODERN;
extern uint64_t INFINITE_BUFFER_SIZE;
extern uint64_t BDP_MODERN_NDP;
extern uint64_t MAX_CWD_MODERN_NDP;
extern uint64_t BDP_OLD_NDP;
extern uint64_t MAX_CWD_OLD_NDP;
extern uint64_t ENABLE_FAST_DROP;
extern std::filesystem::path PROJECT_ROOT_PATH;
extern uint64_t FLOW_ACTIVE;

// LCP
extern simtime_picosec TARGET_RTT;
extern simtime_picosec QA_TRIGGER_RTT;
extern simtime_picosec BAREMETAL_RTT;
extern uint32_t AI_BYTES;   // just for mprdma!
extern uint32_t AI_BYTES_INTRA;
extern uint32_t AI_BYTES_INTER;
extern double LCP_K;
extern uint32_t LCP_FAST_INCREASE_THRESHOLD_INTRA;
extern uint32_t LCP_FAST_INCREASE_THRESHOLD_INTER;
extern bool LCP_USE_QUICK_ADAPT_INTER;
extern bool LCP_USE_QUICK_ADAPT_INTRA;
extern bool LCP_USE_FAST_INCREASE;
extern bool LCP_RTO_REDUCE_CWND;
extern bool LCP_USE_RTT;
extern bool LCP_USE_ECN;
extern bool LCP_USE_PACING;
extern bool LCP_USE_TRIMMING;
extern double LCP_PACING_BONUS;
extern float LCP_ECN_ALPHA;
extern float QA_TRIGGER_RTT_FRACTION;
extern uint64_t MAX_CWD_OLD_NDP;
extern uint64_t ENABLE_FAST_DROP;
extern std::filesystem::path PROJECT_ROOT_PATH;
extern float TARGET_TO_BAREMETAL_RATIO;
extern float STARTING_CWND_BDP_RATIO;
extern float MAX_CWND_BDP_RATIO;
extern bool LCP_USE_DIFFERENT_QUEUES;
extern bool LCP_APPLY_AI_PER_EPOCH;

extern float MD_GAIN_ECN;
extern float MD_FACTOR_RTT;
extern simtime_picosec TARGET_QDELAY;
extern float QA_CWND_RATIO_THRESHOLD;

int pareto(int xm, int mean);
double exponential(double lambda);

simtime_picosec timeFromSec(double secs);
simtime_picosec timeFromMs(double msecs);
simtime_picosec timeFromMs(int msecs);
simtime_picosec timeFromUs(double usecs);
simtime_picosec timeFromUs(uint32_t usecs);
simtime_picosec timeFromNs(double nsecs);
double timeAsMs(simtime_picosec ps);
double timeAsUs(simtime_picosec ps);
double timeAsSec(simtime_picosec ps);
typedef sint64_t mem_b;
mem_b memFromPkt(double pkts);

typedef uint64_t linkspeed_bps;
linkspeed_bps speedFromGbps(double Gbitps);
linkspeed_bps speedFromMbps(uint64_t Mbitps);
linkspeed_bps speedFromMbps(double Mbitps);
linkspeed_bps speedFromKbps(uint64_t Kbitps);
linkspeed_bps speedFromPktps(double packetsPerSec);
double speedAsPktps(linkspeed_bps bps);
double speedAsGbps(linkspeed_bps bps);
typedef int mem_pkts;
void initializeLoggingFolders();
std::filesystem::path findRootPath(const std::string &rootDirectoryName);
std::filesystem::path resolvePath(const std::string &root, const std::string &relativePath);

typedef uint32_t addr_t;
typedef uint16_t port_t;

// Gumph
#if defined(__cplusplus) && !defined(__STL_NO_NAMESPACES)
using namespace std;
#endif

#ifdef _WIN32
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#endif
