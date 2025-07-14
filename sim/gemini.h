#ifndef GEMINI_H
#define GEMINI_H

#include "lcp.h"

class GeminiSrc : public LcpSrc {
  public:
    simtime_picosec last_cwnd_reduction_time = 0;
    double H_gemini = 0.00000012; // gemini suggested value for H (1.2 * 10^-7)
    double beta_gemini = 0.2;   // gemini paper suggested 0.2
    unsigned long bytes_acked_since_previous_cycle = 0;
    unsigned long bytes_marked_since_previous_cycle = 0;
    unsigned long previous_cycle_completion_time = 0;
    simtime_picosec rtt_min = 0;
    simtime_picosec T_gemini = timeFromMs(5); // default is 5 ms

    GeminiSrc(UecLogger* logger, 
              TrafficLogger* pktLogger, 
              EventList& eventList, 
              uint64_t rtt, 
              uint64_t bdp, 
              uint64_t queueDrainTime, 
              int hops)
        : LcpSrc(logger, pktLogger, eventList, rtt, bdp, queueDrainTime, hops) {}

    virtual ~GeminiSrc() {};

    enum TransmissionPhase {SLOW_START = 1, AIMD};
    int transmission_phase = AIMD;
    simtime_picosec most_recent_rto_reduction = 0;
    double ss_thresh = 0;

    
    virtual void updateParams(uint64_t base_rtt_intra, 
      uint64_t base_rtt_inter, 
      uint64_t bdp_intra, 
      uint64_t bdp_inter, 
      uint64_t intra_queuesize, 
      uint64_t inter_queuesize) override;
    
    virtual void adjust_window_aimd(simtime_picosec ts,
      bool ecn,
      simtime_picosec rtt,
      uint32_t ackno,
      uint64_t num_bytes_acked,
      simtime_picosec acked_pkt_ts) override;

      virtual void send_packets() override;
      
      virtual void retransmit_packet() override;

      void set_H_gemini(double value) { H_gemini = value; }
      void set_beta_gemini(double value) { beta_gemini = value; }
      void set_T_gemini(double value_ms) { T_gemini = timeFromMs(value_ms); }
};

#endif