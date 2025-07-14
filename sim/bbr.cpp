// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "bbr.h"
#include "ecn.h"
#include "queue.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <stdio.h>
#include <utility>

#define timeInf 0

// Parameters set as static
std::string BBRSrc::queue_type = "composite";
bool BBRSrc::use_fast_drop = false;
bool BBRSrc::use_pacing = true;
simtime_picosec BBRSrc::pacing_delay = 0;
int BBRSrc::target_rtt_percentage_over_base = 50;
uint64_t BBRSrc::_interdc_delay = 0;
double BBRSrc::x_gain = 0.15;
double BBRSrc::quickadapt_lossless_rtt = 2.0;
double BBRSrc::starting_cwnd = 1;
double BBRSrc::bonus_drop = 1;
int BBRSrc::precision_ts = 1;
int BBRSrc::explicit_target_rtt = 0;
int BBRSrc::explicit_base_rtt = 0;
int BBRSrc::explicit_bdp = 0;
const double PACING_GAIN_CYCLE[8] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};

RouteStrategy BBRSrc::_route_strategy = NOT_SET;
RouteStrategy BBRSink::_route_strategy = NOT_SET;

BBRSrc::BBRSrc(BBRLogger *logger, TrafficLogger *pktLogger, EventList &eventList, uint64_t rtt, uint64_t bdp,
               uint64_t queueDrainTime, int hops)
        : EventSource(eventList, "BBR"), _logger(logger), _flow(pktLogger) {
    _mss = Packet::data_packet_size();
    _unacked = 0;
    _nodename = "BBRsrc";

    _last_acked = 0;
    _highest_sent = 0;
    _use_good_entropies = false;
    _next_good_entropy = 0;

    _nack_rtx_pending = 0;

    // new CC variables
    _hop_count = hops;

    _base_rtt = ((_hop_count * LINK_DELAY_MODERN) + ((PKT_SIZE_MODERN + 64) * 8 / LINK_SPEED_MODERN * _hop_count) +
                 +(_hop_count * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * _hop_count)) *
                1000;

    if (precision_ts != 1) {
        _base_rtt = (((_base_rtt + precision_ts - 1) / precision_ts) * precision_ts);
    }

    _target_rtt = _base_rtt * ((target_rtt_percentage_over_base + 1) / 100.0 + 1);

    if (precision_ts != 1) {
        _target_rtt = (((_target_rtt + precision_ts - 1) / precision_ts) * precision_ts);
    }

    _rtt = _base_rtt;
    // _rto = rtt + _hop_count * queueDrainTime + (rtt * 900000);   // in the original version of this code, _rto was set so high
    _rto = _base_rtt * 3;
    _rto_margin = _rtt / 2;
    _rtx_timeout = timeInf;
    _rtx_timeout_pending = false;
    _rtx_pending = false;
    _crt_path = 0;
    _flow_size = _mss * 934;

    _next_pathid = 1;

    _bdp = (_base_rtt * LINK_SPEED_MODERN / 8) / 1000;
    _queue_size = _bdp; // Temporary
    initial_x_gain = x_gain;

    if (explicit_base_rtt != 0) {
        _base_rtt = explicit_base_rtt;
        _target_rtt = explicit_target_rtt;
        bdp = explicit_bdp;
    }

    _maxcwnd = starting_cwnd;
    _cwnd = starting_cwnd;
    _consecutive_low_rtt = 0;
    target_window = _cwnd;
    _target_based_received = true;

    // // printf("Link Delay %d - Link Speed %lu - Pkt Size %d - Base RTT %lu - "
    //        "Target RTT is %lu - BDP %lu - CWND %u - Hops %d\n",
    //        LINK_DELAY_MODERN, LINK_SPEED_MODERN, PKT_SIZE_MODERN, _base_rtt, _target_rtt, _bdp, _cwnd, _hop_count);

    _max_good_entropies = 10; // TODO: experimental value
    _enableDistanceBasedRtx = false;
    f_flow_over_hook = nullptr;

    if (queue_type == "composite_bts") {
        _bts_enabled = true;
    } else {
        _bts_enabled = false;
    }
}

// Add deconstructor and save data once we are done.
BBRSrc::~BBRSrc() {
    // If we are collecting specific logs
    // // printf("Total NACKs: %lu\n", num_trim);
    if (COLLECT_DATA) {
        // RTT
        std::string file_name = PROJECT_ROOT_PATH / ("output/rtt/rtt" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFile(file_name, std::ios_base::app);

        for (const auto &p : _list_rtt) {
            MyFile << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << "," << get<3>(p) << "," << get<4>(p) << ","
                   << get<5>(p) << std::endl;
        }

        MyFile.close();

        // CWD
        file_name = PROJECT_ROOT_PATH / ("output/cwd/cwd" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCWD(file_name, std::ios_base::app);

        for (const auto &p : _list_cwd) {
            MyFileCWD << p.first << "," << p.second << std::endl;
        }

        MyFileCWD.close();

        // Unacked
        file_name = PROJECT_ROOT_PATH / ("output/unacked/unacked" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileUnack(file_name, std::ios_base::app);

        for (const auto &p : _list_unacked) {
            MyFileUnack << p.first << "," << p.second << std::endl;
        }

        MyFileUnack.close();

        // NACK
        file_name = PROJECT_ROOT_PATH / ("output/nack/nack" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileNack(file_name, std::ios_base::app);

        for (const auto &p : _list_nack) {
            MyFileNack << p.first << "," << p.second << std::endl;
        }

        MyFileNack.close();

        // BTS
        if (_list_bts.size() > 0) {
            file_name = PROJECT_ROOT_PATH / ("output/bts/bts" + _name + "_" + std::to_string(tag) + ".txt");
            std::ofstream MyFileBTS(file_name, std::ios_base::app);

            for (const auto &p : _list_bts) {
                MyFileBTS << p.first << "," << p.second << std::endl;
            }

            MyFileBTS.close();
        }

        // Acked Bytes
        file_name = PROJECT_ROOT_PATH / ("output/acked/acked" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileAcked(file_name, std::ios_base::app);

        for (const auto &p : _list_acked_bytes) {
            MyFileAcked << p.first << "," << p.second << std::endl;
        }

        MyFileAcked.close();

        // Acked ECN
        file_name = PROJECT_ROOT_PATH / ("output/ecn_rtt/ecn_rtt" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileEcnRTT(file_name, std::ios_base::app);

        for (const auto &p : _list_ecn_rtt) {
            MyFileEcnRTT << p.first << "," << p.second << std::endl;
        }

        MyFileEcnRTT.close();

        // ECN Received
        file_name = PROJECT_ROOT_PATH / ("output/ecn/ecn" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileEcnReceived(file_name, std::ios_base::app);

        for (const auto &p : _list_ecn_received) {
            MyFileEcnReceived << p.first << "," << p.second << std::endl;
        }

        MyFileEcnReceived.close();

        // Acked Trimmed
        file_name =
                PROJECT_ROOT_PATH / ("output/trimmed_rtt/trimmed_rtt" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileTrimmedRTT(file_name, std::ios_base::app);

        for (const auto &p : _list_trimmed_rtt) {
            MyFileTrimmedRTT << p.first << "," << p.second << std::endl;
        }

        MyFileTrimmedRTT.close();

        // Fast Increase
        file_name = PROJECT_ROOT_PATH / ("output/fasti/fasti" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileFastInc(file_name, std::ios_base::app);

        for (const auto &p : _list_fast_increase_event) {
            MyFileFastInc << p.first << "," << p.second << std::endl;
        }

        MyFileFastInc.close();

        // Fast Decrease
        file_name = PROJECT_ROOT_PATH / ("output/fastd/fastd" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileFastDec(file_name, std::ios_base::app);

        for (const auto &p : _list_fast_decrease) {
            MyFileFastDec << p.first << "," << p.second << std::endl;
        }

        MyFileFastDec.close();

        // Case 1
        file_name = PROJECT_ROOT_PATH / ("output/case1/case1" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase1(file_name, std::ios_base::app);

        for (const auto &p : count_case_1) {
            MyFileCase1 << p.first << "," << p.second << std::endl;
        }

        MyFileCase1.close();

        // Case 2
        file_name = PROJECT_ROOT_PATH / ("output/case2/case2" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase2(file_name, std::ios_base::app);

        for (const auto &p : count_case_2) {
            MyFileCase2 << p.first << "," << p.second << std::endl;
        }

        MyFileCase2.close();

        // Case 3
        file_name = PROJECT_ROOT_PATH / ("output/case3/case3" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase3(file_name, std::ios_base::app);

        for (const auto &p : count_case_3) {
            MyFileCase3 << p.first << "," << p.second << std::endl;
        }

        MyFileCase3.close();

        // Case 4
        file_name = PROJECT_ROOT_PATH / ("output/case4/case4" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCase4(file_name, std::ios_base::app);

        for (const auto &p : count_case_4) {
            MyFileCase4 << p.first << "," << p.second << std::endl;
        }

        MyFileCase4.close();

        // bytes sent
        file_name = PROJECT_ROOT_PATH / ("output/bytes_sent/bytes_sent" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileBytesSent(file_name, std::ios_base::app);
        for (const auto &p : _list_bytes_sent) {
            MyFileBytesSent << get<0>(p) << "," << get<1>(p) << std::endl;
        }
        MyFileBytesSent.close();

        // bytes sent
        file_name = PROJECT_ROOT_PATH / ("output/agg_bytes_sent/agg_bytes_sent" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileAggBytesSent(file_name, std::ios_base::app);
        for (const auto &p : _list_agg_bytes_sent) {
            MyFileAggBytesSent << get<0>(p) << "," << get<1>(p) << std::endl;
        }
        MyFileAggBytesSent.close();

        // Number of bytes acked in specific time slots
        file_name = PROJECT_ROOT_PATH / ("output/bytes_acked/bytes_acked" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileBytedAcked(file_name, std::ios_base::app);
        for (const auto &p : _list_bytes_acked_collected) {
            MyFileBytedAcked << p.first << "," << p.second << std::endl;
        }
        MyFileBytedAcked.close();

        // Sent
        file_name = PROJECT_ROOT_PATH / ("output/sent/sent" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileSent(file_name, std::ios_base::app);
        for (const auto &p : _list_sent) {
            MyFileSent << p.first << "," << p.second << std::endl;
        }
        MyFileSent.close();

        // Retrans
        file_name = PROJECT_ROOT_PATH / ("output/retrans/retrans" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileRetrans(file_name, std::ios_base::app);
        for (const auto &p : _list_retrans) {
            MyFileRetrans << p.first << "," << p.second << std::endl;
        }
        MyFileRetrans.close();

        // Retrans
        file_name = PROJECT_ROOT_PATH / ("output/timeout/timeout" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileTimeout(file_name, std::ios_base::app);
        for (const auto &p : _list_timeout) {
            MyFileTimeout << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << std::endl;
        }
        MyFileTimeout.close();
    }
}

void BBRSrc::doNextEvent() { startflow(); }

void BBRSrc::set_end_trigger(Trigger &end_trigger) { _end_trigger = &end_trigger; }

std::size_t BBRSrc::get_sent_packet_idx(uint32_t pkt_seqno) {
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        if (pkt_seqno == _sent_packets[i].seqno) {
            return i;
        }
    }
    return _sent_packets.size();
}

void BBRSrc::update_rtx_time() {
    _rtx_timeout = timeInf;
    for (const auto &sp : _sent_packets) {
        auto timeout = sp.timer;
        if (!sp.acked && !sp.nacked && !sp.timedOut && (timeout < _rtx_timeout || _rtx_timeout == timeInf)) {
            _rtx_timeout = timeout;
        }
    }
}

void BBRSrc::mark_received(BBRAck &pkt) {
    // cummulative ack
    if (pkt.seqno() == 1) {
        bool skip = true;
        while (!_sent_packets.empty() && (_sent_packets[0].seqno <= pkt.ackno() || _sent_packets[0].acked)) {
            if (!_sent_packets[0].acked) {
                if (skip)
                    skip = false;
                else
                    reduce_unacked(_mss);
            }
            _sent_packets.erase(_sent_packets.begin());
        }
        update_rtx_time();
        return;
    }
    if (_sent_packets.empty() || _sent_packets[0].seqno > pkt.ackno()) {
        // duplicate ACK -- since we support OOO, this must be caused by
        // duplicate retransmission
        return;
    }
    auto i = get_sent_packet_idx(pkt.seqno());
    if (i == 0) {
        // this should not happen because of cummulative acks, but
        // shouldn't cause harm either
        do {
            _sent_packets.erase(_sent_packets.begin());
        } while (!_sent_packets.empty() && _sent_packets[0].acked);
    } else {
        assert(i < _sent_packets.size());
        auto timer = _sent_packets[i].timer;
        auto seqno = _sent_packets[i].seqno;
        auto nacked = _sent_packets[i].nacked;
        _sent_packets[i] = SentPacketBBR(timer, seqno, true, false, false);
        if (nacked) {
            --_nack_rtx_pending;
        }
        _last_acked = seqno + _mss - 1;
        if (_enableDistanceBasedRtx) {
            bool trigger = true;
            // TODO: this could be optimized with counters or bitsets,
            // but I'm doing this the simple way to avoid bugs while
            // we don't need the optimizations
            for (std::size_t k = 1; k < _sent_packets.size() / 2; ++k) {
                if (!_sent_packets[k].acked) {
                    trigger = false;
                    break;
                }
            }
            if (trigger) {
                // TODO: what's the proper way to act if this packet was
                // NACK'ed? Not super relevant right now as we are not enabling
                // this feature anyway
                _sent_packets[0].timer = eventlist().now();
                _rtx_timeout_pending = true;
            }
        }
    }
    update_rtx_time();
}

void BBRSrc::add_ack_path(const Route *rt) {
    for (auto &r : _good_entropies) {
        if (r == rt) {
            return;
        }
    }
    if (_good_entropies.size() < _max_good_entropies) {
        // // printf("Pushing Back Now, %d\n", from);
        _good_entropies.push_back(rt);
    } else {
        // TODO: this could cause some weird corner cases that would
        // preserve old entropies, but it probably won't be an issue
        // for simulations.
        // Example corner case: if a path is used, then the other
        // paths are replaced up to the point that _next_good_entropy
        // comes back to that path, it could be used again before any
        // of the newer paths.
        _good_entropies[_next_good_entropy] = rt;
        ++_next_good_entropy;
        _next_good_entropy %= _max_good_entropies;
    }
}

void BBRSrc::set_traffic_logger(TrafficLogger *pktlogger) { _flow.set_logger(pktlogger); }

void BBRSrc::reduce_cwnd(uint64_t amount) {
    if (_cwnd >= amount + _mss) {
        _cwnd -= amount * 1;
    } else {
        _cwnd = _mss;
    }
}

void BBRSrc::reduce_unacked(uint64_t amount) {
    if (_unacked >= amount) {
        _unacked -= amount;
    } else {
        _unacked = 0;
    }
}

void BBRSrc::check_limits_cwnd() {
    // Upper Limit
    if (_cwnd > _maxcwnd) {
        _cwnd = _maxcwnd;
    }
    // Lower Limit
    if (_cwnd < _mss) {
        _cwnd = _mss;
    }
}

void BBRSrc::quick_adapt(bool trimmed) {

    if (eventlist().now() >= next_window_end) {
        previous_window_end = next_window_end;
        saved_acked_bytes = acked_bytes;
        acked_bytes = 0;
        next_window_end = eventlist().now() + _base_rtt;
        // Enable Fast Drop
        if ((trimmed || need_quick_adapt) && previous_window_end != 0) {

            // Edge case where we get here receiving a packet a long time after
            // the last one (>> base_rtt). Should not matter in most cases.
            saved_acked_bytes =
                    saved_acked_bytes * ((double)_base_rtt / (eventlist().now() - previous_window_end + _base_rtt));

            // Update window and ignore count
            _cwnd = max((double)(saved_acked_bytes * bonus_drop),
                        (double)_mss); // 1.5 is the amount of target_rtt over
                                       // base_rtt. Simplified here for this
                                       // code share.
            check_limits_cwnd();
            ignore_for = (get_unacked() / (double)_mss);

            // Reset counters, update logs.
            count_received = 0;
            need_quick_adapt = false;
            _list_fast_decrease.push_back(std::make_pair(eventlist().now() / 1000, 1));

            // Update x_gain after large incasts. We want to limit its effect if
            // we move to much smaller windows.
            x_gain = min(initial_x_gain, (_queue_size / 5.0) / (_mss * ((double)_bdp / _cwnd)));
        }
    }
}

// Update Network Parameters
void BBRSrc::updateParams(uint64_t intra_queuesize, uint64_t inter_queuesize) {
    // this version of updateParams better sets RTO
    bool is_flow_inter_dc = (src_dc != dest_dc);
    if (is_flow_inter_dc) {
        _hop_count = 9;
        _base_rtt = ((((_hop_count - 2) * LINK_DELAY_MODERN) + (_interdc_delay / 1000) * 2) +
                     ((PKT_SIZE_MODERN + 64) * 8 / LINK_SPEED_MODERN * _hop_count) + +(_hop_count * LINK_DELAY_MODERN) +
                     (64 * 8 / LINK_SPEED_MODERN * _hop_count)) *
                    1000;
    } else {
        _hop_count = 6;
        _base_rtt = ((_hop_count * LINK_DELAY_MODERN) + ((PKT_SIZE_MODERN + 64) * 8 / LINK_SPEED_MODERN * _hop_count) +
                     +(_hop_count * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * _hop_count)) *
                    1000;
    }

    _bdp = (_base_rtt * LINK_SPEED_MODERN / 8) / 1000;

    double inter_queue_latency_ns = (float) inter_queuesize * 8 / (float) INTER_LINK_SPEED_MODERN;
    double inter_queue_latency_ps = inter_queue_latency_ns * 1000;
    double intra_queue_latency_ns = (float) intra_queuesize * 8 / (float) INTER_LINK_SPEED_MODERN;
    double intra_queue_latency_ps = intra_queue_latency_ns * 1000;
    if (is_flow_inter_dc) {
        // going and ack coming back
        // _rto = _base_rtt + 2* (7 * intra_queue_latency_ps + inter_queue_latency_ps);
        _rto = _base_rtt + (5 * intra_queue_latency_ps + 3 * inter_queue_latency_ps);
    } else {
        _rto = _base_rtt + (5 * intra_queue_latency_ps);
    }

    _maxcwnd = _bdp;
    _cwnd = _bdp;

    _consecutive_low_rtt = 0;
    target_window = _cwnd;
    _target_based_received = true;

    // just push zero initially
    _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, 0, 0));

    fflush(stdout);
    _max_good_entropies = 10; // TODO: experimental value
    _enableDistanceBasedRtx = false;

    for (int i = 0; i < 10; i++) {
        best_bdw_window[i] = 0;
    }

    // Write the parameters to a file for easy access.
    std::string file_name = PROJECT_ROOT_PATH / ("output/params/params" + _name + "_" + std::to_string(tag) + ".txt");
    std::ofstream MyFile(file_name, std::ios_base::app);

    MyFile << "Link speed (Gbps)," << INTER_LINK_SPEED_MODERN << std::endl;
    MyFile << "BDP (KB)," << _bdp / 1000 << std::endl;
    MyFile << "MSS (bytes)," << PKT_SIZE_MODERN << std::endl;
    MyFile << "Starting cwnd (bytes)," << starting_cwnd << std::endl;
    MyFile << "Use Pacing," << use_pacing << std::endl;
    MyFile << "Use Trimming," << use_trimming << std::endl;
    MyFile << "Pacing Bonus," << LCP_PACING_BONUS << std::endl;
    MyFile << "QA_CWND_RATIO_THRESHOLD," << QA_CWND_RATIO_THRESHOLD << std::endl;
    MyFile << "USE_ERASURE_CODING," << use_erasure_coding << std::endl;
    MyFile << "EC_handler," << ec_handler << std::endl;
    MyFile << "RTO (us)," << timeAsUs(_rto) << std::endl;

    cout << "Link speed (Gbps)," << INTER_LINK_SPEED_MODERN << std::endl;
    cout << "BDP (KB)," << _bdp / 1000 << std::endl;
    cout << "MSS (bytes)," << PKT_SIZE_MODERN << std::endl;
    cout << "Starting cwnd (bytes)," << starting_cwnd << std::endl;
    cout << "Use Pacing," << use_pacing << std::endl;
    cout << "Use Trimming," << use_trimming << std::endl;
    cout << "Pacing Bonus," << LCP_PACING_BONUS << std::endl;
    cout << "QA_CWND_RATIO_THRESHOLD," << QA_CWND_RATIO_THRESHOLD << std::endl;
    cout << "USE_ERASURE_CODING," << use_erasure_coding << std::endl;
    cout << "EC_handler," << ec_handler << std::endl;
    cout << "RTO (us)," << timeAsUs(_rto) << std::endl;

    MyFile.close();
}



// Update Network Parameters
void BBRSrc::updateParams() {
    // this version of updateParams keeps the pre-set RTO
    bool is_flow_inter_dc = (src_dc != dest_dc);
    if (is_flow_inter_dc) {
        _hop_count = 9;
        _base_rtt = ((((_hop_count - 2) * LINK_DELAY_MODERN) + (_interdc_delay / 1000) * 2) +
                     ((PKT_SIZE_MODERN + 64) * 8 / LINK_SPEED_MODERN * _hop_count) + +(_hop_count * LINK_DELAY_MODERN) +
                     (64 * 8 / LINK_SPEED_MODERN * _hop_count)) *
                    1000;
    } else {
        _hop_count = 6;
        _base_rtt = ((_hop_count * LINK_DELAY_MODERN) + ((PKT_SIZE_MODERN + 64) * 8 / LINK_SPEED_MODERN * _hop_count) +
                     +(_hop_count * LINK_DELAY_MODERN) + (64 * 8 / LINK_SPEED_MODERN * _hop_count)) *
                    1000;
    }

    _bdp = (_base_rtt * LINK_SPEED_MODERN / 8) / 1000;

    _maxcwnd = _bdp;
    _cwnd = _bdp;

    _consecutive_low_rtt = 0;
    target_window = _cwnd;
    _target_based_received = true;

    // just push zero initially
    _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, 0, 0));

    fflush(stdout);
    _max_good_entropies = 10; // TODO: experimental value
    _enableDistanceBasedRtx = false;

    for (int i = 0; i < 10; i++) {
        best_bdw_window[i] = 0;
    }

    // Write the parameters to a file for easy access.
    std::string file_name = PROJECT_ROOT_PATH / ("output/params/params" + _name + "_" + std::to_string(tag) + ".txt");
    std::ofstream MyFile(file_name, std::ios_base::app);

    MyFile << "Link speed (Gbps)," << INTER_LINK_SPEED_MODERN << std::endl;
    MyFile << "BDP (KB)," << _bdp / 1000 << std::endl;
    MyFile << "MSS (bytes)," << PKT_SIZE_MODERN << std::endl;
    MyFile << "Starting cwnd (bytes)," << starting_cwnd << std::endl;
    MyFile << "Use Pacing," << use_pacing << std::endl;
    MyFile << "Use Trimming," << use_trimming << std::endl;
    MyFile << "Pacing Bonus," << LCP_PACING_BONUS << std::endl;
    MyFile << "QA_CWND_RATIO_THRESHOLD," << QA_CWND_RATIO_THRESHOLD << std::endl;
    MyFile << "USE_ERASURE_CODING," << use_erasure_coding << std::endl;
    MyFile << "EC_handler," << ec_handler << std::endl;
    MyFile << "RTO (us)," << timeAsUs(_rto) << std::endl;

    cout << "Link speed (Gbps)," << INTER_LINK_SPEED_MODERN << std::endl;
    cout << "BDP (KB)," << _bdp / 1000 << std::endl;
    cout << "MSS (bytes)," << PKT_SIZE_MODERN << std::endl;
    cout << "Starting cwnd (bytes)," << starting_cwnd << std::endl;
    cout << "Use Pacing," << use_pacing << std::endl;
    cout << "Use Trimming," << use_trimming << std::endl;
    cout << "Pacing Bonus," << LCP_PACING_BONUS << std::endl;
    cout << "QA_CWND_RATIO_THRESHOLD," << QA_CWND_RATIO_THRESHOLD << std::endl;
    cout << "USE_ERASURE_CODING," << use_erasure_coding << std::endl;
    cout << "EC_handler," << ec_handler << std::endl;
    cout << "RTO (us)," << timeAsUs(_rto) << std::endl;

    MyFile.close();
}

void BBRSrc::processNack(BBRNack &pkt) {
    
    if (!use_trimming) {
        std::cout << "LCP is not using trimming, how are we getting nacks?" << endl;
        exit(1);
    }

    // // printf("Nack from %d - ECN 1, Path %d\n", from, pkt.pathid_echo);
    count_trimmed_in_rtt++;
    consecutive_nack++;
    trimmed_last_rtt++;
    consecutive_good_medium = 0;
    acked_bytes += 64;
    saved_trimmed_bytes += 64;

    // // printf("Just NA CK from %d at %lu\n", from, eventlist().now() / 1000);

    // Reduce Window Or Do Fast Drop
    if (use_fast_drop) {
        if (count_received >= ignore_for) {
            need_quick_adapt = true;
            pause_send = true;
            // quick_adapt(true);
        }
        if (count_received > ignore_for) {
            reduce_cwnd(uint64_t(_mss * 0));
        }
    } else {
        reduce_cwnd(uint64_t(_mss * 0));
    }
    check_limits_cwnd();

    _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
    _consecutive_no_ecn = 0;
    _consecutive_low_rtt = 0;
    _received_ecn.push_back(std::make_tuple(eventlist().now(), true, _mss, _target_rtt + 10000));

    _list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));
    // mark corresponding packet for retransmission
    auto i = get_sent_packet_idx(pkt.seqno());
    assert(i < _sent_packets.size());

    assert(!_sent_packets[i].acked); // TODO: would it be possible for a packet
                                     // to receive a nack after being acked?
    if (!_sent_packets[i].nacked) {
        // ignore duplicate nacks for the same packet
        _sent_packets[i].nacked = true;
        ++_nack_rtx_pending;
    }

    bool success = resend_packet(i);
    if (!_rtx_pending && !success) {
        _rtx_pending = true;
    }
}

void BBRSrc::simulateTrimEvent(BBRAck &pkt) {

    // // printf("Simulated Trim from %d - ECN 1, Path %d\n", from, pkt.pathid_echo);

    consecutive_good_medium = 0;
    // acked_bytes += _mss;
    // saved_trimmed_bytes += 64;

    if (count_received >= ignore_for) {
        need_quick_adapt = true;
    }

    // // printf("Just NA CK from %d at %lu\n", from, eventlist().now() / 1000);

    // Reduce Window Or Do Fast Drop
    if (use_fast_drop) {
        if (count_received >= ignore_for) {
            quick_adapt(true);
        }
    }

    check_limits_cwnd();

    //_list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));
}

/* Choose a route for a particular packet */
int BBRSrc::choose_route() {

    switch (_route_strategy) {
    case PULL_BASED: {
        /* this case is basically SCATTER_PERMUTE, but avoiding bad paths. */

        assert(_paths.size() > 0);
        if (_paths.size() == 1) {
            // special case - no choice
            return 0;
        }
        // otherwise we've got a choice
        _crt_path++;
        if (_crt_path == _paths.size()) {
            // permute_paths();
            _crt_path = 0;
        }
        uint32_t path_id = _path_ids[_crt_path];
        _avoid_score[path_id] = _avoid_ratio[path_id];
        int ctr = 0;
        while (_avoid_score[path_id] > 0 /* && ctr < 2*/) {
            // // printf("as[%d]: %d\n", path_id, _avoid_score[path_id]);
            _avoid_score[path_id]--;
            ctr++;
            // re-choosing path
            cout << "re-choosing path " << path_id << endl;
            _crt_path++;
            if (_crt_path == _paths.size()) {
                // permute_paths();
                _crt_path = 0;
            }
            path_id = _path_ids[_crt_path];
            _avoid_score[path_id] = _avoid_ratio[path_id];
        }
        // cout << "AS: " << _avoid_score[path_id] << " AR: " <<
        // _avoid_ratio[path_id] << endl;
        assert(_avoid_score[path_id] == 0);
        break;
    }
    case SCATTER_RANDOM:
        // ECMP
        assert(_paths.size() > 0);
        _crt_path = random() % _paths.size();
        break;
    case SCATTER_PERMUTE:
    case SCATTER_ECMP:
        // Cycle through a permutation.  Generally gets better load balancing
        // than SCATTER_RANDOM.
        _crt_path++;
        assert(_paths.size() > 0);
        if (_crt_path / 1 == _paths.size()) {
            // permute_paths();
            _crt_path = 0;
        }
        break;
    case ECMP_FIB:
        // Cycle through a permutation.  Generally gets better load balancing
        // than SCATTER_RANDOM.
        _crt_path++;
        if (_crt_path == _paths.size()) {
            // permute_paths();
            _crt_path = 0;
        }
        break;
    case ECMP_RANDOM2_ECN: {
        uint64_t allpathssizes = _mss * _paths.size();
        if (_highest_sent < max(_maxcwnd, allpathssizes)) {
            /*// printf("Trying this for %d // Highest Sent %d - cwnd %d - "
                   "allpathsize %d\n",
                   from, _highest_sent, _maxcwnd, allpathssizes);*/
            _crt_path++;
            // // printf("Trying this for %d\n", from);
            if (_crt_path == _paths.size()) {
                // permute_paths();
                _crt_path = 0;
            }
        } else {
            if (_next_pathid == -1) {
                assert(_paths.size() > 0);
                _crt_path = random() % _paths.size();
                // // printf("New Path %d is %d\n", from, _crt_path);
            } else {
                // // printf("Recycling Path %d is %d\n", from, _next_pathid);
                _crt_path = _next_pathid;
            }
        }
        break;
    }
    case ECMP_RANDOM_ECN: {
        _crt_path = from;
        break;
    }
    case ECMP_FIB_ECN: {
        // Cycle through a permutation, but use ECN to skip paths
        while (1) {
            _crt_path++;
            if (_crt_path == _paths.size()) {
                // permute_paths();
                _crt_path = 0;
            }
            if (_path_ecns[_path_ids[_crt_path]] > 0) {
                _path_ecns[_path_ids[_crt_path]]--;
                /*
                if (_log_me) {
                    cout << eventlist().now() << " skipped " <<
                _path_ids[_crt_path] << " " << _path_ecns[_path_ids[_crt_path]]
                << endl;
                }
                */
            } else {
                // eventually we'll find one that's zero
                break;
            }
        }
        break;
    }
    case SINGLE_PATH:
        return _crt_path;
    case REACTIVE_ECN:
        return _crt_path;
    case NOT_SET:
        abort(); // shouldn't be here at all
    default:
        abort();
        break;
    }

    return _crt_path / 1;
}

int BBRSrc::next_route() {
    // used for reactive ECN.
    // Just move on to the next path blindly
    assert(_route_strategy == REACTIVE_ECN);
    _crt_path++;
    assert(_paths.size() > 0);
    if (_crt_path == _paths.size()) {
        // permute_paths();
        _crt_path = 0;
    }
    return _crt_path;
}

int BBRSrc::ChooseStartingIndex() {
    int my_idx = random() % 8;
    while (my_idx == 1) {
        my_idx = random() % 8;
    }
    return 0;
}

uint32_t BBRSrc::InFlight(double gain) {

    /*if (m_rtProp == Time::Max()) {
        return tcb->m_initialCWnd * tcb->m_segmentSize;
    }*/
    double quanta = 1 * _mss;
    double estimatedBdp = GetBestBw() * (_base_rtt / 1000) / 8.0;

    if (bbr_status == PROBE_BW && m_cycleIndex == 0) {
        return (gain * estimatedBdp) + quanta + (2 * _mss);
    }
    return (gain * estimatedBdp) + quanta;
}

void BBRSrc::CheckCyclePhase() {
    // // printf("Checking we have %d - %lu %lu\n", bbr_status, eventlist().now(),
    //        latest_phase_window_end + _base_rtt);
    if (bbr_status == PROBE_BW && IsNextCyclePhase()) {
        // // printf("Changing Cycle\n");
        pacing_gain = PACING_GAIN_CYCLE[m_cycleIndex];
        // // printf("New gain %d is %f\n", m_cycleIndex, pacing_gain);
        m_cycleIndex++;
        m_cycleIndex = (m_cycleIndex) % 8;
        if (COLLECT_DATA) {
            std::string file_name = PROJECT_ROOT_PATH / ("output/status/status" + _name + ".txt");
            std::ofstream MyFile(file_name, std::ios_base::app);
            MyFile << eventlist().now() / 1000 << "," << 2 << std::endl;
            MyFile.close();
        }
    }
}

double BBRSrc::GetBestBw() {
    // storing the largest number to arr[0]
    double max_bw = 0;
    for (int i = 0; i < 10; ++i) {
        /* // printf("Best BW %d --> %f\n", i, best_bdw_window[i]); */
        if (best_bdw_window[i] > max_bw) {
            max_bw = best_bdw_window[i];
        }
    }
    /* // printf("Returning %f\n", max_bw); */
    return max_bw;
}

void BBRSrc::CheckDrain() {
    if (bbr_status == STARTUP && m_isPipeFilled) {
        if (COLLECT_DATA) {
            std::string file_name = PROJECT_ROOT_PATH / ("output/status/status" + _name + ".txt");
            std::ofstream MyFile(file_name, std::ios_base::app);
            MyFile << eventlist().now() / 1000 << "," << 0 << std::endl;
            MyFile.close();
        }

        bbr_status = DRAIN;
        pacing_gain = 0.35;
        cwnd_gain = 2.89;
    }

    if (bbr_status == DRAIN && get_unacked_raw() <= InFlight(1)) {
        // // printf("%s - Exiting DRAIN, BW is %f at %lu -- %d %d \n",
        // _name.c_str(),
        //        GetBestBw(), GLOBAL_TIME / 1000, get_unacked_raw(),
        //        InFlight(1));
        bbr_status = PROBE_BW;
        m_cycleIndex = ChooseStartingIndex();
        pacing_gain = PACING_GAIN_CYCLE[m_cycleIndex];
        // // printf("New gain %d is %f\n", m_cycleIndex, pacing_gain);
        m_cycleIndex++;
        m_cycleIndex = (m_cycleIndex) % 8;
        cwnd_gain = 2;
        latest_phase_window_end = eventlist().now() + _base_rtt;

        if (COLLECT_DATA) {
            std::string file_name = PROJECT_ROOT_PATH / ("output/status/status" + _name + ".txt");
            std::ofstream MyFile(file_name, std::ios_base::app);
            MyFile << eventlist().now() / 1000 << "," << 1 << std::endl;
            MyFile.close();
        }
    }
}

bool BBRSrc::RTTPassed() {
    if (rtt_passed) {
        last_rtt_startup = eventlist().now() + _base_rtt;
        rtt_passed = false;
    } else {
        if (eventlist().now() > last_rtt_startup) {
            // // printf("%s - Returning true at %lu\n", _name.c_str(),
            // GLOBAL_TIME);
            rtt_passed = true;
        }
    }

    return rtt_passed;
}

void BBRSrc::CheckFullPipe() {
    if (m_isPipeFilled || !RTTPassed()) {
        return;
    }

    /* Check if Bottleneck bandwidth is still growing*/
    if (GetBestBw() >= m_fullBandwidth * 1.25) {
        m_fullBandwidth = GetBestBw();
        m_fullBandwidthCount = 0;
        return;
    }

    m_fullBandwidthCount++;
    // // printf("%s - STARTUP++, BW is %f at %lu\n", _name.c_str(), GetBestBw(),
    //        GLOBAL_TIME / 1000);
    if (m_fullBandwidthCount >= 3) {
        m_isPipeFilled = true;
        //    // printf("%s - Exiting STARTUP, BW is %f at %lu\n", _name.c_str(),
        //           GetBestBw(), GLOBAL_TIME / 1000);
    }
}

bool BBRSrc::IsNextCyclePhase() {
    bool isFullLength = false;
    if (eventlist().now() > latest_phase_window_end) {
        isFullLength = true;
        latest_phase_window_end += _base_rtt;
    }
    if (pacing_gain == 1.0) {
        return isFullLength;
    } else if (pacing_gain > 1.0) {
        //    // printf("Entering here");
        return isFullLength /*&& get_unacked_raw() >= InFlight(pacing_gain)*/;
    } else {
        return isFullLength;
    }
}

void BBRSrc::update_bw_model(double bw) {

    if (COLLECT_DATA) {
        std::string file_name = PROJECT_ROOT_PATH / ("output/out_bw/out_bw" + _name + ".txt");
        std::ofstream MyFile(file_name, std::ios_base::app);
        MyFile << eventlist().now() / 1000 << "," << bw << "," << 1 / 1000 << std::endl;
        MyFile.close();
    }

    best_bdw_current_window.push_back(bw);
    double max = *max_element(best_bdw_current_window.begin(), best_bdw_current_window.end());
    best_bdw_window[m_cycleIndex_bdw] = max;
    if (eventlist().now() > latest_rtt_window_end + _base_rtt) {
        m_cycleIndex_bdw++;
        m_cycleIndex_bdw %= 10;
        best_bdw_current_window.clear();
        latest_rtt_window_end += _base_rtt;
        if (generic_pacer != NULL && eventlist().now() > _base_rtt * 0) {
            pacing_delay = pacer_d;
            pacing_delay *= 1000;
            generic_pacer->cancel();
            generic_pacer->schedule_send(pacing_delay);
        }
    }
}

void BBRSrc::UpdatePacingRate(double bw) {
    double max_b = GetBestBw();
    bw = max_b;
    if (m_isPipeFilled || bw > current_bw) {
        current_bw = bw;
    }

    int old_pacer_d = pacer_d;

    current_bw = max(current_bw, 3.0);
    pacer_d = ((_mss + 64) * 8.0) / (current_bw * pacing_gain);

    if ((pacer_d - old_pacer_d < 4300 * 8 / LINK_SPEED_MODERN) && pacing_gain > 1) {

        pacer_d = old_pacer_d - (4300 * 8 * 2 / LINK_SPEED_MODERN);
        if (pacer_d < 0) {
            pacer_d = (4300 * 8 / LINK_SPEED_MODERN);
        }
        /* // printf("Pacer D %d - old %d - adapted %d\n", pacer_d, old_pacer_d, pacer_d); */
    }

    /* // printf("Choosing new BW of %f (%f and %f) at %lu\n", current_bw * pacing_gain, bw, pacing_gain, GLOBAL_TIME /
     * 1000); */
    if (COLLECT_DATA) {
        std::string file_name = PROJECT_ROOT_PATH / ("output/out_bw_paced/out_bw_paced" + _name + ".txt");
        std::ofstream MyFile(file_name, std::ios_base::app);
        MyFile << eventlist().now() / 1000 << "," << (bw * pacing_gain) << "," << 1 / 1000 << std::endl;
        MyFile.close();
    }
}

void BBRSrc::processAck(BBRAck &pkt, bool force_marked) {
    BBRAck::seq_t seqno = pkt.ackno();
    simtime_picosec ts = pkt.ts();
    // // printf("Received ACK\n");

    consecutive_nack = 0;
    bool marked = pkt.flags() & ECN_ECHO; // ECN was marked on data packet and echoed on ACK

    if (COLLECT_DATA && marked) {
        std::string file_name = PROJECT_ROOT_PATH / ("output/ecn/ecn" + std::to_string(pkt.from) + "_" +
                                                     std::to_string(pkt.to) + ".txt");
        std::ofstream MyFile(file_name, std::ios_base::app);

        MyFile << eventlist().now() / 1000 << "," << marked << std::endl;

        MyFile.close();
    }

    if (!collection_initiated) {
        // Start the timer for QA from the time that the first ACK arrives
        collection_initiated = true;
        collection_period_end = eventlist().now() + _base_rtt;
        bytes_acked_in_collection_period = 0;
        _list_bytes_acked_collected.push_back(std::make_pair(eventlist().now() / 1000, bytes_acked_in_collection_period));
    } else {
        if (eventlist().now() >= collection_period_end) {
            collection_period_end = eventlist().now() + _base_rtt;
            _list_bytes_acked_collected.push_back(std::make_pair(eventlist().now() / 1000, bytes_acked_in_collection_period));
            bytes_acked_in_collection_period = 0;
            _list_bytes_sent.push_back(std::make_tuple(eventlist().now() / 1000, _bytes_sent));
            _bytes_sent = 0;
            _list_agg_bytes_sent.push_back(std::make_tuple(eventlist().now() / 1000, _agg_bytes_sent));
        }
    }

    uint64_t now_time = 0;
    if (precision_ts == 1) {
        now_time = eventlist().now();
    } else {
        now_time = (((eventlist().now() + precision_ts - 1) / precision_ts) * precision_ts);
    }
    uint64_t newRtt = now_time - ts;
    mark_received(pkt);

    count_total_ack++;
    // for now I just put it as mss
    bytes_acked_in_collection_period += _mss;
    
    if (marked) {
        _list_ecn_received.push_back(std::make_pair(eventlist().now() / 1000, 1));
        count_total_ecn++;
        consecutive_good_medium = 0;
    }

    if (from == 0 && count_total_ack % 10 == 0) {
        // // printf("Currently %s at Pkt %d\n", _name.c_str(), count_total_ack);
        // fflush(stdout);
    }

    // BBR Part
    use_pacing = true;
    double delivery_rate = 0;
    delivered_so_far += _mss + 64;
    if ((delivered_so_far - (pkt.delivered_so_far + 4153)) > 0) {
        delivery_rate = (delivered_so_far - (pkt.delivered_so_far + 4153)) * 8 / (double)(newRtt / 1000);
    }

    /* // printf("From %d - New RTT id %lu - Delivery So Far %d - Rate %f\n", from, newRtt, delivered_so_far,
     * delivery_rate); */

    update_bw_model(delivery_rate);
    CheckCyclePhase();
    CheckFullPipe();
    CheckDrain();
    UpdatePacingRate(delivery_rate);

    // // printf("New RTT id %lu\n", newRtt);
    // // printf("%s -> Delivery Rate (%d) / (%f) = %f, pacer %d\n", _name.c_str(),
    //        (delivered_so_far - (pkt.delivered_so_far + 4153)) * 8,
    //        (newRtt / 1000.0), delivery_rate, pacer_d);

    if (!marked) {
        _consecutive_no_ecn += _mss;
        _next_pathid = pkt.pathid_echo;
    } else {
        // // printf("Ack %d - ECN 1, Path %d\n", from, pkt.pathid_echo);
        _next_pathid = -1;
        ecn_last_rtt = true;
        _consecutive_no_ecn = 0;
    }

    if (COLLECT_DATA) {
        _received_ecn.push_back(std::make_tuple(eventlist().now(), marked, _mss, newRtt));
        _list_rtt.push_back(std::make_tuple(eventlist().now() / 1000, newRtt / 1000, pkt.seqno(), pkt.ackno(),
                                            _base_rtt / 1000, _target_rtt / 1000));
    }

    if (newRtt > _base_rtt * quickadapt_lossless_rtt && marked && queue_type == "lossless_input") {
        // // printf("PFC Src Happened at %lu - %d@%d\n", GLOBAL_TIME / 1000, from,
        //        pkt.id());
        simulateTrimEvent(dynamic_cast<BBRAck &>(pkt));
    } else {
        // // printf("ACK Happened at %lu - %d@%d\n", GLOBAL_TIME / 1000, from,
        //        pkt.id());
    }

    if (_flow_finished)
        return;

    // if (flow_id() == 1) {
    //     std::cout << "ack no: " << pkt.ackno() << endl;
    //     std::cout << "seq no: " << pkt.seqno() << endl;
    //     std::cout << "_sent_packets.empty(): " << _sent_packets.size() << endl;
    // }

    if (seqno >= _flow_size && _sent_packets.empty() && !_flow_finished) {
        _flow_finished = true;
        if (f_flow_over_hook) {
            f_flow_over_hook(pkt);
        }

        // FCT.
        auto fct_file_name = PROJECT_ROOT_PATH / ("output/fct/fct" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileFCT(fct_file_name, std::ios_base::app);

        MyFileFCT << _flow_start_time << "," << timeAsUs(eventlist().now()) << "," << timeAsUs(eventlist().now()) - timeAsUs(_flow_start_time) << std::endl;

        MyFileFCT.close();

        // Flow Size.
        auto flow_size_file_name = PROJECT_ROOT_PATH / ("output/flow_size/flow_size" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileFlowSize(flow_size_file_name, std::ios_base::app);

        MyFileFlowSize << _flow_size << std::endl;
        
        MyFileFlowSize.close();

        cout << "Flow id=" << _flow.flow_id() << 
        " Completion time is " << timeAsUs(eventlist().now()) - timeAsUs(_flow_start_time) << 
        "us - Flow Finishing Time " << timeAsUs(eventlist().now()) << 
        "us - Flow Start Time "<< timeAsUs(_flow_start_time) << 
        " - Size Finished Flow " << _flow_size << 
        " - From " << from << " - To " << to << endl;

        // // printf("Overall Completion at %lu\n", GLOBAL_TIME);
        if (_end_trigger) {
            _end_trigger->activate();
            generic_pacer->cancel();
        }
        generic_pacer->cancel();
        
        return;
    }

    if (seqno > _last_acked || true) { // TODO: new ack, we don't care about
                                       // ordering for now. Check later though
        if (seqno >= _highest_sent) {
            _highest_sent = seqno;
        }

        _last_acked = seqno;

        // Compute target congestion window based on estimated bandwidth.
        // (This mimics NS-3’s target: InFlight(cwnd_gain) + aggregation;
        // here we use InFlight(cwnd_gain) as a proxy.)
        uint64_t target_cwnd = InFlight(cwnd_gain);
        
        // If cwnd is below the target (or still in the initial phase),
        // increase it by one MSS (which is equivalent to the bytes acknowledged).
        if (_cwnd < target_cwnd || delivered_so_far < starting_cwnd * _mss) {
            _cwnd += _mss;  // Increase by one MSS for every ACK
            if (_cwnd > target_cwnd)
                _cwnd = target_cwnd;
            check_limits_cwnd(); // Ensure _cwnd stays within defined min/max bounds.
        }


        _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
        // // printf("Window Is %d - From %d To %d\n", _cwnd, from, to);
        current_pkt++;
        // // printf("Triggering ADJ\n");

        acked_bytes += _mss;
        good_bytes += _mss;

        _effcwnd = _cwnd;
        // // printf("Received From %d - Sending More\n", from);
        send_packets();
        return; // TODO: if no further code, this can be removed
    }
}

uint64_t BBRSrc::get_unacked() {
    return _unacked;
    // uint64_t missing = 0;
    // for (const auto &sp : _sent_packets) {
    //     if (!sp.acked && !sp.nacked && !sp.timedOut) {
    //         missing += _mss;
    //     }
    // }
    // return missing;
}

uint64_t BBRSrc::get_unacked_raw() {
    // return _unacked;
    uint64_t missing = 0;
    for (const auto &sp : _sent_packets) {
        if (!sp.acked && !sp.nacked && !sp.timedOut) {
            missing += _mss + 64;
        }
    }
    return missing;
}

void BBRSrc::receivePacket(Packet &pkt) {
    // every packet received represents one less packet in flight

    // // printf("Node %s - Received packet %d - From %d\n",
    // nodename().c_str(),
    //        pkt.id(), pkt.from);

    if (!pkt.bounced()) {
        reduce_unacked(_mss);
    }

    // TODO: receive window?
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);

    if (_logger) {
        _logger->logBBR(*this, BBRLogger::BBR_RCV);
    }
    switch (pkt.type()) {
    case BBR:
        // BTS
        break;
    case BBRACK:
        // fflush(stdout);
        count_received++;

        processAck(dynamic_cast<BBRAck &>(pkt), false);
        /*if (pkt.pfc_just_happened) {
            // printf("PFC Src Happened at %lu - %d@%d\n", GLOBAL_TIME / 1000,
                   from, pkt.id());
            simulateTrimEvent(dynamic_cast<BBRAck &>(pkt));
        }*/
        pkt.free();
        break;
    case ETH_PAUSE:
        // // printf("Src received a Pause\n");
        // processPause((const EthPausePacket &)pkt);
        pkt.free();
        return;
    case BBRNACK:
        num_trim++;
        // fflush(stdout);
        if (use_trimming) {
            _next_pathid = -1;
            count_received++;
            processNack(dynamic_cast<BBRNack &>(pkt));
            pkt.free();
        }
        break;
    default:
        std::cout << "unknown packet receive with type code: " << pkt.type() << "\n";
        return;
    }
    if (get_unacked() < _cwnd && !_flow_finished && _rtx_timeout_pending) {
        eventlist().sourceIsPendingRel(*this, 0);
    }
    // pkt.free();
    // // printf("Free3\n");
    // //fflush(stdout);
}

const string &BBRSrc::nodename() { return _nodename; }

void BBRSrc::connect(Route *routeout, Route *routeback, BBRSink &sink, simtime_picosec starttime) {
    if (_route_strategy == SINGLE_PATH || _route_strategy == ECMP_FIB || _route_strategy == ECMP_FIB_ECN ||
        _route_strategy == REACTIVE_ECN || _route_strategy == ECMP_RANDOM2_ECN || _route_strategy == ECMP_RANDOM_ECN || _route_strategy == SCATTER_RANDOM) {
        assert(routeout);
        _route = routeout;
    }

    _sink = &sink;
    _flow.set_id(get_id()); // identify the packet flow with the NDP source
                            // that generated it
    _flow._name = _name;
    _sink->connect(*this, routeback);

    // // printf("StartTime is %lu\n", starttime);
    eventlist().sourceIsPending(*this, starttime);
}

void BBRSrc::startflow() {
    // printf("Starting a flow\n");
    if(!flow_started) {
        flow_started = true;
        _flow_start_time = eventlist().now();
        cout << "BBRSrc::Starting flow (ID: " << _flow.flow_id() << ", size: " << _flow_size << ") at time " << timeAsUs(_flow_start_time) << "us" << endl;
    }

    ideal_x = x_gain;
    if (use_pacing && generic_pacer == NULL) {
        generic_pacer = new BBRPacer(eventlist(), *this);
    }

    send_packets();
}

const Route *BBRSrc::get_path() {
    // TODO: add other ways to select paths
    // // printf("Entropy Size %d\n", _good_entropies.size());
    if (_use_good_entropies && !_good_entropies.empty()) {
        // auto rt = _good_entropies[_next_good_entropy];
        // ++_next_good_entropy;
        // _next_good_entropy %= _good_entropies.size();
        auto rt = _good_entropies.back();
        _good_entropies.pop_back();
        return rt;
    }

    // Means we want to select a random one out of all paths, the original
    // idea
    if (_num_entropies == -1) {
        _crt_path = random() % _paths.size();
    } else {
        // Else we use our entropy array of a certain size and roud robin it
        _crt_path = _entropy_array[current_entropy];
        current_entropy = current_entropy + 1;
        current_entropy = current_entropy % _num_entropies;
    }

    total_routes = _paths.size();
    return _paths.at(_crt_path);
}

void BBRSrc::map_entropies() {
    for (int i = 0; i < _num_entropies; i++) {
        _entropy_array.push_back(random() % _paths.size());
    }
    // // printf("Printing my Paths: ");
    // //fflush(stdout);
    for (int i = 0; i < _num_entropies; i++) {
        // printf("%d - ", _entropy_array[i]);
    }
    // printf("\n");
}

void BBRSrc::pacedSend() {
    _paced_packet = true;

    send_packets();
}

void BBRSrc::send_packets() {
    // BBR Add

    count_sent = 0;
    if (_rtx_pending) {
        retransmit_packet();
    }
    
    _list_unacked.push_back(std::make_pair(eventlist().now() / 1000, _unacked));
    unsigned c = _cwnd;

    while (get_unacked() + _mss <= c && _highest_sent < _flow_size) {

        if (!_paced_packet && use_pacing) {
            if (generic_pacer != NULL && !generic_pacer->is_pending()) {
                generic_pacer->schedule_send(pacing_delay);
                return;
            } else if (generic_pacer != NULL) {
                return;
            }
        }

        // if (flow_id() == 1) {
        //     cout << "Sending new packet with seqno: " << _highest_sent+1 << endl;
        // }

        // Check pacer and set timeout
        uint64_t data_seq = 0;

        BBRPacket *p = BBRPacket::newpkt(_flow, *_route, _highest_sent + 1, data_seq, _mss, false, _dstaddr);

        p->set_route(*_route);
        int crt = choose_route();
        // crt = random() % _paths.size();

        p->set_pathid(_path_ids[crt]);
        p->delivered_so_far = delivered_so_far;

        p->from = this->from;
        p->to = this->to;
        p->tag = this->tag;
        p->my_idx = data_count_idx++;

        p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());

        // send packet
        _highest_sent += _mss;
        _packets_sent += _mss;
        _unacked += _mss;
        
        _bytes_sent += p->data_packet_size();
        _agg_bytes_sent += p->data_packet_size();
        _list_sent.push_back(std::make_pair(eventlist().now() / 1000, p->seqno()));

        // Getting time until packet is really sent
        // // printf("Sent Packet, %d\n", from);
        // // printf("Actually sending2 %d at %lu\n", p->id(), GLOBAL_TIME / 1000);
        PacketSink *sink = p->sendOn();
        HostQueue *q = dynamic_cast<HostQueue *>(sink);
        assert(q);
        uint32_t service_time = q->serviceTime(*p);

        _sent_packets.push_back(
                SentPacketBBR(eventlist().now() + service_time + _rto, p->seqno(), false, false, false));

        count_sent++;
        if (generic_pacer != NULL) {
            generic_pacer->just_sent();
            _paced_packet = false;
        }
        if (_rtx_timeout == timeInf) {
            update_rtx_time();
        }

    }
}

void permute_sequence_bbr(vector<int> &seq) {
    size_t len = seq.size();
    for (uint32_t i = 0; i < len; i++) {
        seq[i] = i;
    }
    for (uint32_t i = 0; i < len; i++) {
        int ix = random() % (len - i);
        int tmpval = seq[ix];
        seq[ix] = seq[len - 1 - i];
        seq[len - 1 - i] = tmpval;
    }
}

void BBRSrc::set_paths(uint32_t no_of_paths) {

    _path_ids.resize(no_of_paths);
    permute_sequence_bbr(_path_ids);

    _paths.resize(no_of_paths);
    _original_paths.resize(no_of_paths);
    _path_acks.resize(no_of_paths);
    _path_ecns.resize(no_of_paths);
    _path_nacks.resize(no_of_paths);
    _bad_path.resize(no_of_paths);
    _avoid_ratio.resize(no_of_paths);
    _avoid_score.resize(no_of_paths);

    _path_ids.resize(no_of_paths);
    // permute_sequence(_path_ids);
    _paths.resize(no_of_paths);
    _path_ecns.resize(no_of_paths);

    for (size_t i = 0; i < no_of_paths; i++) {
        _paths[i] = NULL;
        _original_paths[i] = NULL;
        _path_acks[i] = 0;
        _path_ecns[i] = 0;
        _path_nacks[i] = 0;
        _avoid_ratio[i] = 0;
        _avoid_score[i] = 0;
        _bad_path[i] = false;
        _path_ids[i] = i;
    }
}

void BBRSrc::set_paths(vector<const Route *> *rt_list) {
    uint32_t no_of_paths = rt_list->size();
    switch (_route_strategy) {
    case NOT_SET:
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
        // shouldn't call this with these strategies
        abort();
    case SINGLE_PATH:
    case SCATTER_PERMUTE:
    case SCATTER_RANDOM:
    case PULL_BASED:
    case SCATTER_ECMP: {
        no_of_paths = min(_num_entropies, (int)no_of_paths);
        _path_ids.resize(no_of_paths);
        _paths.resize(no_of_paths);
        _original_paths.resize(no_of_paths);
        _path_acks.resize(no_of_paths);
        _path_ecns.resize(no_of_paths);
        _path_nacks.resize(no_of_paths);
        _bad_path.resize(no_of_paths);
        _avoid_ratio.resize(no_of_paths);
        _avoid_score.resize(no_of_paths);
#ifdef DEBUG_PATH_STATS
        _path_counts_new.resize(no_of_paths);
        _path_counts_rtx.resize(no_of_paths);
        _path_counts_rto.resize(no_of_paths);
#endif

        // generate a randomize sequence of 0 .. size of rt_list - 1
        vector<int> randseq(rt_list->size());
        if (_route_strategy == SCATTER_ECMP) {
            // randsec may have duplicates, as with ECMP
            // randomize_sequence(randseq);
        } else {
            // randsec will have no duplicates
            // permute_sequence(randseq);
        }

        for (size_t i = 0; i < no_of_paths; i++) {
            // we need to copy the route before adding endpoints, as
            // it may be used in the reverse direction too.
            // Pick a random route from the available ones
            Route *tmp = new Route(*(rt_list->at(randseq[i])), *_sink);
            // Route* tmp = new Route(*(rt_list->at(i)));
            tmp->add_endpoints(this, _sink);
            tmp->set_path_id(i, rt_list->size());
            _paths[i] = tmp;
            _path_ids[i] = i;
            _original_paths[i] = tmp;
#ifdef DEBUG_PATH_STATS
            _path_counts_new[i] = 0;
            _path_counts_rtx[i] = 0;
            _path_counts_rto[i] = 0;
#endif
            _path_acks[i] = 0;
            _path_ecns[i] = 0;
            _path_nacks[i] = 0;
            _avoid_ratio[i] = 0;
            _avoid_score[i] = 0;
            _bad_path[i] = false;
        }
        _crt_path = 0;
        // permute_paths();
        break;
    }
    default: {
        abort();
        break;
    }
    }
}

void BBRSrc::apply_timeout_penalty() {
    if (use_trimming) {
        reduce_cwnd(_mss);
    } else {
        _cwnd = _mss;
    }
}

void BBRSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) {
    // #ifndef RESEND_ON_TIMEOUT
    //     return; // TODO: according to ndp.cpp, rtx is not necessary with
    //     RTS. Check
    //             // if this applies to us
    // #endif

    if (_highest_sent == 0)
        return;
    if (_rtx_timeout == timeInf || now + period < _rtx_timeout)
        return;

    // here we can run into phase effects because the timer is checked
    // only periodically for ALL flows but if we keep the difference
    // between scanning time and real timeout time when restarting the
    // flows we should minimize them !
    if (!_rtx_timeout_pending) {
        _rtx_timeout_pending = true;

        retransmit_packet();

        // check the timer difference between the event and the real value
        simtime_picosec too_early = _rtx_timeout - now;
        if (now > _rtx_timeout) {
            // This might happen because we hold on retransmitting if we
            // have enough packets in flight cout << "late_rtx_timeout: " <<
            // _rtx_timeout << " now: " << now
            //     << " now+rto: " << now + _rto << " rto: " << _rto <<
            //     endl;
            too_early = 0;
        }
        eventlist().sourceIsPendingRel(*this, too_early);
    }
}

bool BBRSrc::resend_packet(std::size_t idx) {

    if (get_unacked() >= _cwnd) {
        return false;
    }

    // Check pacer and set timeout
    if (!_paced_packet && use_pacing) {
        if (generic_pacer != NULL && !generic_pacer->is_pending()) {
            // // printf("scheduling send2\n");
            generic_pacer->schedule_send(pacing_delay);
            return false;
        } else if (generic_pacer != NULL) {
            return false;
        }
    }

    assert(!_sent_packets[idx].acked);

    // this will cause retransmission not only of the offending
    // packet, but others close to timeout
    _rto_margin = _rtt / 2;

    // if (_use_good_entropies && !_good_entropies.empty()) {
    //     rt = _good_entropies[_next_good_entropy];
    //     ++_next_good_entropy;
    //     _next_good_entropy %= _good_entropies.size();
    // } else {
    // }
    // Getting time until packet is really sent
    _unacked += _mss;
    BBRPacket *p = BBRPacket::newpkt(_flow, *_route, _sent_packets[idx].seqno, 0, _mss, true, _dstaddr);
    p->set_ts(eventlist().now());
    p->delivered_so_far = delivered_so_far;

    p->set_route(*_route);
    count_sent++;
    int crt = choose_route();
    // crt = random() % _paths.size();

    p->set_pathid(_path_ids[crt]);
        
    _bytes_sent += p->data_packet_size();
    _agg_bytes_sent += p->data_packet_size();
    _list_retrans.push_back(std::make_pair(eventlist().now() / 1000, p->seqno()));

    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATE);
    PacketSink *sink = p->sendOn();
    HostQueue *q = dynamic_cast<HostQueue *>(sink);
    assert(q);
    uint32_t service_time = q->serviceTime(*p);
    if (_sent_packets[idx].nacked) {
        --_nack_rtx_pending;
        _sent_packets[idx].nacked = false;
    }
    _sent_packets[idx].timer = eventlist().now() + service_time + _rto;
    _sent_packets[idx].timedOut = false;
    update_rtx_time();
    if (generic_pacer != NULL) {
        generic_pacer->just_sent();
        _paced_packet = false;
    }
    return true;
}

// retransmission for timeout
void BBRSrc::retransmit_packet() {
    _rtx_pending = false;
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        auto &sp = _sent_packets[i];
        if (_rtx_timeout_pending && !sp.timedOut && !sp.acked && !sp.nacked && sp.timer <= eventlist().now() + _rto_margin) {
            apply_timeout_penalty();
            sp.timedOut = true;
            aggregate_timedout += _mss;
            _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, aggregate_timedout, aggregate_retrans));
            reduce_unacked(_mss);
        }
        if (!sp.acked && (sp.timedOut || sp.nacked)) {
            if (!resend_packet(i)) {
                _rtx_pending = true;
            } else {
                aggregate_retrans += _mss;
                _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, aggregate_timedout, aggregate_retrans));
            }
        }
    }
    _rtx_timeout_pending = false;
}

/**********
 * BBRSink *
 **********/

BBRSink::BBRSink() : DataReceiver("sink"), _cumulative_ack{0}, _drops{0} { _nodename = "BBRsink"; }

void BBRSink::set_end_trigger(Trigger &end_trigger) { _end_trigger = &end_trigger; }

void BBRSink::send_nack(simtime_picosec ts, bool marked, BBRAck::seq_t seqno, BBRAck::seq_t ackno, const Route *rt,
                        int path_id) {

    BBRNack *nack = BBRNack::newpkt(_src->_flow, *_route, seqno, ackno, 0, _srcaddr);

    // // printf("Sending NACK at %lu\n", GLOBAL_TIME);
    nack->set_pathid(_path_ids[_crt_path]);
    _crt_path++;
    if (_crt_path == _paths.size()) {
        _crt_path = 0;
    }

    nack->pathid_echo = path_id;
    nack->is_ack = false;
    nack->flow().logTraffic(*nack, *this, TrafficLogger::PKT_CREATESEND);
    nack->set_ts(ts);
    if (marked) {
        nack->set_flags(ECN_ECHO);
    } else {
        nack->set_flags(0);
    }

    nack->sendOn();
}

bool BBRSink::already_received(BBRPacket &pkt) {
    BBRPacket::seq_t seqno = pkt.seqno();

    if (seqno <= _cumulative_ack) { // TODO: this assumes
                                    // that all data packets
                                    // have the same size
        return true;
    }
    for (auto it = _received.begin(); it != _received.end(); ++it) {
        if (seqno == *it) {
            return true; // packet received OOO
        }
    }
    return false;
}

void BBRSink::receivePacket(Packet &pkt) {

    switch (pkt.type()) {
    case BBRACK:
    case BBRNACK:
        // bounced, ignore
        pkt.free();
        // // printf("Free4\n");
        // //fflush(stdout);
        return;
    case BBR:
        // do what comes after the switch
        if (pkt.bounced()) {
            // printf("Bounced at Sink, no sense\n");
        }
        break;
    default:
        std::cout << "unknown packet receive with type code: " << pkt.type() << "\n";
        pkt.free();
        // // printf("Free5\n");
        // //fflush(stdout);
        return;
    }
    BBRPacket *p = dynamic_cast<BBRPacket *>(&pkt);
    BBRPacket::seq_t seqno = p->seqno();
    BBRPacket::seq_t ackno = p->seqno() + p->data_packet_size() - 1;
    simtime_picosec ts = p->ts();

    if (p->type() == UEC) {
        /*// printf("NORMALACK, %d at %lu - Time %lu - ID %d\n", this->from,
               GLOBAL_TIME, (GLOBAL_TIME - ts) / 1000, p->id());*/
    }

    bool marked = p->flags() & ECN_CE;

    // // printf("Packet %d ECN %d\n", from, marked);

    // TODO: consider different ways to select paths
    auto crt_path = random() % _paths.size();
    if (already_received(*p)) {
        // duplicate retransmit
        if (use_trimming) { // we can assume
                                        // that they have
                                        // been configured
                                        // similarly, or
                                        // exchanged
                                        // information about
                                        // options somehow
            int32_t path_id = p->pathid();
            send_ack(ts, marked, 1, _cumulative_ack, _paths.at(crt_path), pkt.get_route(), path_id,
                     p->delivered_so_far);
        }
        return;
    }

    // packet was trimmed
    if (pkt.header_only()) {
        if (use_trimming) {
            send_nack(ts, marked, seqno, ackno, _paths.at(crt_path), pkt.pathid());
        }
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
        p->free();
        // // printf("Free6\n");
        // //fflush(stdout);
        // cout << "trimmed packet";
        return;
    }

    int size = p->data_packet_size();
    // pkt._flow().logTraffic(pkt, *this,
    // TrafficLogger::PKT_RCVDESTROY);
    p->free();
    // // printf("Free7\n");
    // //fflush(stdout);

    // if (_src->flow_id() == 1) {
    //     cout << "Packet with seqnum: " << p->seqno() << " received" << endl;
    // }

    _packets += size;

    if (seqno == _cumulative_ack + 1) { // next expected seq no
        _cumulative_ack = seqno + size - 1;
        seqno = 1;

        // handling packets received OOO
        while (!_received.empty() && _received.front() == _cumulative_ack + 1) {
            _received.pop_front();
            _cumulative_ack += size; // this assumes that all
                                     // packets have the same size
        }
        ackno = _cumulative_ack;
    } else if (seqno < _cumulative_ack + 1) { // already ack'ed
        // this space intentionally left empty
        seqno = 1;
        ackno = _cumulative_ack;
    } else { // not the next expected sequence number
        // TODO: what to do when a future packet is
        // received?
        if (_received.empty()) {
            _received.push_front(seqno);
            _drops += (1000 + seqno - _cumulative_ack - 1) / 1000; // TODO: figure out what is this
                                                                   // calculating exactly
        } else if (seqno > _received.back()) {
            _received.push_back(seqno);
        } else {
            for (auto it = _received.begin(); it != _received.end(); ++it) {
                if (seqno == *it)
                    break; // bad retransmit
                if (seqno < (*it)) {
                    _received.insert(it, seqno);
                    break;
                }
            }
        }
    }
    // TODO: reverse_route is likely sending the packet
    // through the same exact links, which is not correct in
    // Packet Spray, but there doesn't seem to be a good,
    // quick way of doing that in htsim // printf("Ack Sending
    // From %d - %d\n", this->from,
    int32_t path_id = p->pathid();
    send_ack(ts, marked, seqno, ackno, _paths.at(crt_path), pkt.get_route(), path_id, p->delivered_so_far);
}

void BBRSink::send_ack(simtime_picosec ts, bool marked, BBRAck::seq_t seqno, BBRAck::seq_t ackno, const Route *rt,
                       const Route *inRoute, int path_id, int so_far_del) {

    BBRAck *ack = 0;

    switch (_route_strategy) {
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
    case ECMP_RANDOM2_ECN:
    case SCATTER_RANDOM:
    case ECMP_RANDOM_ECN:
        ack = BBRAck::newpkt(_src->_flow, *_route, seqno, ackno, 0, _srcaddr);

        ack->set_pathid(_path_ids[_crt_path]);
        _crt_path++;
        if (_crt_path == _paths.size()) {
            _crt_path = 0;
        }
        ack->inc_id++;
        ack->my_idx = ack_count_idx++;

        // set ECN echo only if that is selected strategy
        if (marked) {
            /*// printf("ACK - ECN %d - From %d at %lu\n", path_id, from,
                   GLOBAL_TIME / 1000);*/
            ack->set_flags(ECN_ECHO);
        } else {
            // // printf("ACK - NO ECN\n");
            ack->set_flags(0);
        }

        /*// printf("Sending ACk FlowID %d - SrcAddr %d - Id %d -------- TIme
           now "
               "%lu vs %lu\n",
               _src->_flow.flow_id(), _srcaddr, ack->inc_id, GLOBAL_TIME /
           1000, ts / 1000);*/
        break;
    case SINGLE_PATH:
        ack = BBRAck::newpkt(_src->_flow, *_route, seqno, ackno, 0, _srcaddr);
        ack->set_pathid(_path_ids[_crt_path]);
        ack->inc_id++;
        ack->my_idx = ack_count_idx++;

        // set ECN echo only if that is selected strategy
        if (marked) {
            ack->set_flags(ECN_ECHO);
        } else {
            ack->set_flags(0);
        }
        break;
    case NOT_SET:
        abort();
    default:
        break;
    }
    assert(ack);
    ack->pathid_echo = path_id;

    // ack->inRoute = inRoute;
    ack->is_ack = true;
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATE);
    ack->set_ts(ts);
    ack->delivered_so_far = so_far_del;

    // // printf("Setting TS to %lu at %lu\n", ts /
    // 1000, GLOBAL_TIME / 1000);
    ack->from = this->from;
    ack->to = this->to;
    ack->tag = this->tag;

    ack->sendOn();
}

const string &BBRSink::nodename() { return _nodename; }

uint64_t BBRSink::cumulative_ack() { return _cumulative_ack; }

uint32_t BBRSink::drops() { return _drops; }

void BBRSink::connect(BBRSrc &src, const Route *route) {
    _src = &src;
    switch (_route_strategy) {
    case SINGLE_PATH:
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
    case ECMP_RANDOM2_ECN:
    case ECMP_RANDOM_ECN:
    case SCATTER_RANDOM:
        assert(route);
        //("Setting route\n");
        _route = route;
        break;
    default:
        // do nothing we shouldn't be using this route - call
        // set_paths() to set routing information
        cout << "Sink: Route is being set to NULL" << endl;
        _route = NULL;
        break;
    }

    _cumulative_ack = 0;
    _drops = 0;
}

void BBRSink::set_paths(uint32_t no_of_paths) {
    switch (_route_strategy) {
    case SCATTER_PERMUTE:
    case PULL_BASED:
    case SCATTER_ECMP:
    case NOT_SET:
        abort();
    case SCATTER_RANDOM:
    case SINGLE_PATH:
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case ECMP_RANDOM2_ECN:
    case REACTIVE_ECN:
        assert(_paths.size() == 0);
        _paths.resize(no_of_paths);
        _path_ids.resize(no_of_paths);
        for (unsigned int i = 0; i < no_of_paths; i++) {
            _paths[i] = NULL;
            _path_ids[i] = i;
        }
        _crt_path = 0;
        // permute_paths();
        break;
    case ECMP_RANDOM_ECN:
        assert(_paths.size() == 0);
        _paths.resize(no_of_paths);
        _path_ids.resize(no_of_paths);
        for (unsigned int i = 0; i < no_of_paths; i++) {
            _paths[i] = NULL;
            _path_ids[i] = i;
        }
        _crt_path = 0;
        // permute_paths();
        break;
    default:
        break;
    }
}

/**********************
 * UecRtxTimerScanner *
 **********************/

BBRRtxTimerScanner::BBRRtxTimerScanner(simtime_picosec scanPeriod, EventList &eventlist)
        : EventSource(eventlist, "RtxScanner"), _scanPeriod{scanPeriod} {
    eventlist.sourceIsPendingRel(*this, 0);
}

void BBRRtxTimerScanner::registerBBR(BBRSrc &BBRsrc) { _uecs.push_back(&BBRsrc); }

void BBRRtxTimerScanner::doNextEvent() {
    simtime_picosec now = eventlist().now();
    BBRs_t::iterator i;
    for (i = _uecs.begin(); i != _uecs.end(); i++) {
        if (!((*i)->_flow_finished))
            (*i)->rtx_timer_hook(now, _scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}
