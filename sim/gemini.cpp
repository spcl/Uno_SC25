#include "gemini.h"
#include <iostream>
#include <filesystem>
#include <fstream>

void GeminiSrc::updateParams(uint64_t base_rtt_intra, 
    uint64_t base_rtt_inter, 
    uint64_t bdp_intra, 
    uint64_t bdp_inter, 
    uint64_t intra_queuesize, 
    uint64_t inter_queuesize) {
        LcpSrc::updateParams(base_rtt_intra, base_rtt_inter, bdp_intra, bdp_inter, intra_queuesize, inter_queuesize);

        bool is_flow_inter_dc = (src_dc != dest_dc);
        if (!is_flow_inter_dc) {
            H_gemini *= (bdp_inter * 1.0 / bdp_intra);
        }

        std::string file_name = PROJECT_ROOT_PATH / ("output/params/params" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFile(file_name, std::ios_base::app);

        MyFile << "H_gemini," << H_gemini << std::endl;
        MyFile << "beta_gemini," << beta_gemini << std::endl;
        MyFile << "T_gemini (ms)," << timeAsMs(T_gemini) << std::endl;

        cout << "H_gemini," << H_gemini << std::endl;
        cout << "beta_gemini," << beta_gemini << std::endl;
        cout << "T_gemini (ms)," << timeAsMs(T_gemini) << std::endl;

        MyFile.close();

}

// retransmission for timeout
void GeminiSrc::retransmit_packet() {
    _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, aggregate_timedout, aggregate_retrans));
    _rtx_pending = false;
    double cwnd_reduction_value = 0;
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        auto &sp = _sent_packets[i];

        if (!sp.timedOut && !sp.acked && !sp.nacked && sp.timer <= eventlist().now() + _rto_margin) {
            sp.timedOut = true;
            // only count when the timedout packet has the potential to be retransmitted
            if (use_rto_cwnd_reduction) {
                if (eventlist().now() - most_recent_rto_reduction > _rto) {
                    double old_cwnd = _cwnd;

                    transmission_phase = SLOW_START;
                    ss_thresh = (_cwnd / 2.0);
                    _cwnd = _mss;

                    cwnd_reduction_value = max(cwnd_reduction_value, old_cwnd - _cwnd);
                    previous_cycle_completion_time = eventlist().now();
                    bytes_acked_since_previous_cycle = 0;
                    bytes_marked_since_previous_cycle = 0;
                    rtt_min = 0;
                    last_cwnd_reduction_time = eventlist().now();
                    most_recent_rto_reduction = eventlist().now();
                }
            }
            check_limits_cwnd();
            reduce_unacked(sp.size);
        }
        if (!sp.acked && (sp.timedOut || sp.nacked)) {
            if (!resend_packet(i)) {
                _rtx_pending = true;
            } else {
                aggregate_retrans += sp.size;
            }
        }
    }
    
    if (COLLECT_DATA && cwnd_reduction_value > 0) {
        // std::cout << eventlist().now() << ": " << cwnd_reduction << endl;
        _list_retransmit_cwnd_reduce.push_back(make_tuple(eventlist().now() / 1000, cwnd_reduction_value));
    }
}

void GeminiSrc::adjust_window_aimd(simtime_picosec ts,
    bool ecn,
    simtime_picosec rtt,
    uint32_t ackno,
    uint64_t num_bytes_acked,
    simtime_picosec acked_pkt_ts) {

        // update ecn fraction every one RTT
        bool ecn_congested = (use_ecn && ecn);
        bytes_acked_since_previous_cycle += num_bytes_acked;
        if (ecn_congested)
            bytes_marked_since_previous_cycle += num_bytes_acked;
        
        if (previous_cycle_completion_time != 0 && eventlist().now() - previous_cycle_completion_time > _base_rtt) {
            previous_cycle_completion_time = eventlist().now();
            double ecn_fraction = bytes_marked_since_previous_cycle * 1.0 / bytes_acked_since_previous_cycle;
            ecn_fraction_ewma = computeEwma(ecn_fraction_ewma, ecn_fraction, lcp_ecn_alpha);

            _list_ecn_ewma.push_back(std::make_pair(eventlist().now() / 1000, ecn_fraction_ewma));

            // re-use LCP's epoch ended list
            if (COLLECT_DATA)
                _list_epoch_ended.push_back(std::make_tuple(eventlist().now() / 1000, bytes_marked_since_previous_cycle, bytes_acked_since_previous_cycle, ecn_fraction, ecn_fraction_ewma, 0));
            
            bytes_acked_since_previous_cycle = 0;
            bytes_marked_since_previous_cycle = 0;
            rtt_min = 0;
        }

        if (rtt_min == 0 || rtt < rtt_min) {
            rtt_min = rtt;
        }
        bool rtt_congested = (use_rtt && rtt_min > (_base_rtt + T_gemini));

        if (transmission_phase == SLOW_START && _cwnd >= ss_thresh) {
            ss_thresh = 0;
            transmission_phase = AIMD;
        }

        // if (flow_id() == 1) {
        //     cout << "_cwnd: " << _cwnd << endl;
        //     cout << "rtt: "  << rtt << endl;
        //     cout << "rtt_min: " << rtt_min << endl;
        //     cout << "_base_rtt: " << _base_rtt << endl;
        //     cout << "T_gemini: " << T_gemini << endl;
        //     cout << "-------------------" << endl;
        // }

        if (!rtt_congested && !ecn_congested) {
            // double the _cwnd after one RTT
            if (transmission_phase == SLOW_START)   // slow start
                _cwnd += num_bytes_acked;
            else {
                // additive increase
                double h_gemini = H_gemini * _bdp;
                _cwnd += (h_gemini * (double) num_bytes_acked / _cwnd);
            }

        } else if (last_cwnd_reduction_time != 0 && eventlist().now() - last_cwnd_reduction_time > _base_rtt) {
            // multiplicative decrease
            if (rtt_congested && COLLECT_DATA)
                _list_is_rtt_congested.push_back(eventlist().now() / 1000);
            if (ecn_congested && COLLECT_DATA)
                _list_is_ecn_congested.push_back(eventlist().now() / 1000);
            
            double ecn_reduction_factor = (ecn_congested ? ecn_fraction_ewma * md_gain_ecn : 0);
            double rtt_reduction_factor = (rtt_congested ? beta_gemini : 0);
            assert(ecn_reduction_factor >= 0 && ecn_reduction_factor < 1);
            assert(rtt_reduction_factor >= 0 && ecn_reduction_factor < 1);
            // cout << flow_id() << ": old cwnd is " << _cwnd << ", ewma: " << ecn_fraction_ewma << ", reduction factor: " << ecn_fraction_ewma * md_gain_ecn;
            _cwnd *= (1 - max(ecn_reduction_factor, rtt_reduction_factor));
            // cout << ", new_cwnd: " << _cwnd << endl;
            
            last_cwnd_reduction_time = eventlist().now();
            previous_cycle_completion_time = eventlist().now();
        }

        check_limits_cwnd();
}

void GeminiSrc::send_packets() {
    LcpSrc::send_packets();
    if (epoch_enabled && (previous_cycle_completion_time == 0 || last_cwnd_reduction_time == 0)) {
        // reset status after the last packet of the first cwnd is sent
        last_cwnd_reduction_time = eventlist().now();
        previous_cycle_completion_time = eventlist().now();
        rtt_min = 0;
        bytes_acked_since_previous_cycle = 0;
        bytes_marked_since_previous_cycle = 0;
    }
}