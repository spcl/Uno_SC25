// -*- c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t -*-
#include "lcp.h"
#include "ecn.h"
#include "queue.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <stdio.h>
#include <utility>
#include <algorithm> // for std::min and std::max

#define timeInf 0

bool LcpSrc::use_bts = false;
std::string LcpSrc::queue_type = "composite";
std::string LcpSrc::epoch_type = "short";
std::string LcpSrc::qa_type = "long";
uint64_t LcpSrc::_interdc_delay = 0;
double LcpSrc::starting_cwnd = 1;
int LcpSrc::precision_ts = 1;
uint64_t LcpSrc::_switch_queue_size = 0;
int LcpSrc::freq = 1;
uint16_t LcpSrc::_total_subflows = 10;
bool LcpSrc::_subflow_adaptive_reroute = false;
bool LcpSrc::using_dctcp = false;
int LcpSrc::force_cwnd = 0;
std::string LcpSrc::phantom_algo = "standard";
std::string LcpSrc::lcp_type = "aimd";
bool LcpSrc::smooth_ecn = false;

bool LcpSrc::_subflow_reroute = true;

RouteStrategy LcpSrc::_route_strategy = NOT_SET;
RouteStrategy LcpSink::_route_strategy = NOT_SET;

LcpSrc::LcpSrc(UecLogger *logger, TrafficLogger *pktLogger, EventList &eventList, uint64_t rtt, uint64_t bdp,
               uint64_t queueDrainTime, int hops)
        : EventSource(eventList, "lcp"), _logger(logger), _flow(pktLogger) {
    _mss = Packet::data_packet_size();
    _unacked = 0;
    _nodename = "LcpSrc";

    _highest_sent = 0;
    _bytes_sent = 0;
    _agg_bytes_sent = 0;
    next_byte_to_send = 0;
    _use_good_entropies = false;
    _next_good_entropy = 0;

    _nack_rtx_pending = 0;
    _next_qa_sn = 0;

    pacing_delay = 0;

    // new CC variables
    _hop_count = hops;

    _base_rtt = ((_hop_count * LINK_DELAY_MODERN) + ((PKT_SIZE_MODERN + 64) * 8 / INTER_LINK_SPEED_MODERN * _hop_count) +
                 +(_hop_count * LINK_DELAY_MODERN) + (64 * 8 / INTER_LINK_SPEED_MODERN * _hop_count)) *
                1000;

    if (precision_ts != 1) {
        _base_rtt = (((_base_rtt + precision_ts - 1) / precision_ts) * precision_ts);
    }

    // if (precision_ts != 1) {
    //     _target_rtt = (((_target_rtt + precision_ts - 1) / precision_ts) * precision_ts);
    // }

    // _rtt = _base_rtt;
    //_rto = _base_rtt * 3;   // rto is set later in update params
    _rto_margin = _base_rtt / 8;
    _rtx_pending = false;
    _crt_path = 0;
    _flow_size = _mss * 934;

    _next_pathid = 1;

    // _bdp is in bytes
    _bdp = (_base_rtt * INTER_LINK_SPEED_MODERN / 8) / 1000;
    _queue_size = _bdp; // Temporary

    _maxcwnd = _maxcwnd_bdp_ratio * bdp;
    _cwnd = starting_cwnd;
    target_window = _cwnd;

    _max_good_entropies = 10; // TODO: experimental value
    _enableDistanceBasedRtx = false;
    f_flow_over_hook = nullptr;
    last_pac_change = 0;

    if (queue_type == "composite_bts") {
        _bts_enabled = true;
    } else {
        _bts_enabled = false;
    }

    // reset epoch params but set next_epoch_start_ack_seqno to _cwnd
    resetEpochParams();

    _consecutive_good_epochs = 0;
    qa_measurement_period_end = 0;
    ecn_fraction_ewma = 0;
    _ecn_count_this_window = 0;
    _good_count_this_window = 0;
    _consecutive_decreases = 0;

    _flow_start_time = 0;

    collection_initiated = false;

    contract_target_delay = &LcpSrc::contract_target_delay_vegas;
}

// Add deconstructor and save data once we are done.
LcpSrc::~LcpSrc() {
    // If we are collecting specific logs
    if (COLLECT_DATA) {
        std::cout << "Collecting data!" << endl;
        // RTT
        std::string file_name = PROJECT_ROOT_PATH / ("output/rtt/rtt" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFile(file_name, std::ios_base::app);
        for (const auto &p : _list_rtt) {
            MyFile << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << "," << get<3>(p) << "," << get<4>(p) << "," << get<5>(p) << "," << get<6>(p) << "," << get<7>(p) << std::endl;
        }
        MyFile.close();

        // CWD
        file_name = PROJECT_ROOT_PATH / ("output/cwd/cwd" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCWD(file_name, std::ios_base::app);
        for (const auto &p : _list_cwd) {
            MyFileCWD << p.first << "," << uint64_t(p.second) << std::endl;
        }
        MyFileCWD.close();

        // ECN Congested.
        file_name = PROJECT_ROOT_PATH / ("output/ecn_congested/ecn_congested" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileEcnCongested(file_name, std::ios_base::app);
        for (const auto &p : _list_is_ecn_congested) {
            MyFileEcnCongested << p << std::endl;
        }
        MyFileEcnCongested.close();

        // RTT congested.
        file_name = PROJECT_ROOT_PATH / ("output/rtt_congested/rtt_congested" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileRttCongested(file_name, std::ios_base::app);
        for (const auto &p : _list_is_rtt_congested) {
            MyFileRttCongested << p << std::endl;
        }
        MyFileRttCongested.close();

        // Epoch ended.
        file_name = PROJECT_ROOT_PATH / ("output/epoch_ended/epoch_ended" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileEpochEnded(file_name, std::ios_base::app);
        for (const auto &p : _list_epoch_ended) {
            MyFileEpochEnded << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << "," << get<3>(p) << "," << get<4>(p) << "," << get<5>(p) << std::endl;
        }
        MyFileEpochEnded.close();

        // CWND reduction factor.
        file_name = PROJECT_ROOT_PATH / ("output/cwnd_reduction_factor/cwnd_reduction_factor" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileCwndReductionFactor(file_name, std::ios_base::app);
        for (const auto &p : _list_cwnd_reduction_factor) {
            MyFileCwndReductionFactor << get<0>(p) << "," << get<1>(p) << std::endl;
        }
        MyFileCwndReductionFactor.close();

        // Qa ended.
        file_name = PROJECT_ROOT_PATH / ("output/qa_ended/qa_ended" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileQaEnded(file_name, std::ios_base::app);
        for (const auto &p : _list_qa_ended) {
            MyFileQaEnded << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << "," << get<3>(p) << "," << get<4>(p) << "," << get<5>(p) << std::endl;
        }
        MyFileQaEnded.close();

        // Qa RTT triggerred.
        file_name = PROJECT_ROOT_PATH / ("output/qa_rtt_triggerred/qa_rtt_triggerred" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileQaRTTTriggerred(file_name, std::ios_base::app);
        for (const auto &p : _list_qa_rtt_triggerred) {
            MyFileQaRTTTriggerred << p << std::endl;
        }
        MyFileQaRTTTriggerred.close();

        // Qa bytes acked triggerred.
        file_name = PROJECT_ROOT_PATH / ("output/qa_bytes_acked_triggerred/qa_bytes_acked_triggerred" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileQaBytesAckedTriggerred(file_name, std::ios_base::app);
        for (const auto &p : _list_qa_bytes_acked_triggerred) {
            MyFileQaBytesAckedTriggerred << p << std::endl;
        }
        MyFileQaBytesAckedTriggerred.close();

        // CWND reduced due to packet retransmission.
        file_name = PROJECT_ROOT_PATH / ("output/retrans_cwnd_reduce/retrans_cwnd_reduce" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileRetransCwndReduce(file_name, std::ios_base::app);
        for (const auto &p : _list_retransmit_cwnd_reduce) {
            MyFileRetransCwndReduce << get<0>(p) << "," << get<1>(p) << std::endl;
        }
        MyFileRetransCwndReduce.close();

        // ECN ewma.
        file_name = PROJECT_ROOT_PATH / ("output/ecn_ewma/ecn_ewma" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileEcnEwma(file_name, std::ios_base::app);
        for (const auto &p : _list_ecn_ewma) {
            MyFileEcnEwma << p.first << "," << p.second << std::endl;
        }
        MyFileEcnEwma.close();

        // Unacked
        file_name = PROJECT_ROOT_PATH / ("output/unacked/unacked" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileUnack(file_name, std::ios_base::app);
        for (const auto &p : _list_unacked) {
            MyFileUnack << p.first << "," << p.second << std::endl;
        }
        MyFileUnack.close();

        // Sent
        file_name = PROJECT_ROOT_PATH / ("output/sent/sent" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileSent(file_name, std::ios_base::app);
        for (const auto &p : _list_sent) {
            MyFileSent << p.first << "," << p.second << std::endl;
        }
        MyFileSent.close();

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

        // ECN Received
        file_name = PROJECT_ROOT_PATH / ("output/ecn/ecn" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileEcnReceived(file_name, std::ios_base::app);
        for (const auto &p : _list_ecn_received) {
            MyFileEcnReceived << p.first << "," << p.second << std::endl;
        }
        MyFileEcnReceived.close();

        // Fast Increase
        file_name = PROJECT_ROOT_PATH / ("output/fasti/fasti" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileFastInc(file_name, std::ios_base::app);
        for (const auto &p : _list_fast_increase_event) {
            MyFileFastInc << p.first << "," << p.second << std::endl;
        }
        MyFileFastInc.close();

        // Sending Rate
        file_name = PROJECT_ROOT_PATH /
                    ("output/sending_rate/sending_rate" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileSendingRate(file_name, std::ios_base::app);
        for (const auto &p : list_sending_rate) {
            MyFileSendingRate << p.first << "," << p.second << std::endl;
        }
        MyFileSendingRate.close();

        // ECN RATE
        file_name = PROJECT_ROOT_PATH / ("output/ecn_rate/ecn_rate" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileECNRate(file_name, std::ios_base::app);
        for (const auto &p : list_ecn_rate) {
            MyFileECNRate << p.first << "," << p.second << std::endl;
        }
        MyFileECNRate.close();

        // Number of bytes acked in specific time slots
        file_name = PROJECT_ROOT_PATH / ("output/bytes_acked/bytes_acked" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileBytedAcked(file_name, std::ios_base::app);
        for (const auto &p : _list_bytes_acked_collected) {
            MyFileBytedAcked << p.first << "," << p.second << std::endl;
        }
        MyFileBytedAcked.close();

        // rto info for sent/re-sent packets.
        file_name = PROJECT_ROOT_PATH / ("output/rto_info/rto_info" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileRtoInfo(file_name, std::ios_base::app);
        for (const auto &p : _list_rto_info) {
            MyFileRtoInfo << get<0>(p) << "," << get<1>(p) << "," << get<2>(p) << "," << get<3>(p) << "," << get<4>(p) << std::endl;
        }
        MyFileRtoInfo.close();    
    }
}

void LcpSrc::update_pacing_delay() {
    bool is_time_to_update = (last_pac_change == 0) || ((eventlist().now() - last_pac_change) > _base_rtt / 20);
    if (use_pacing && is_time_to_update) {
        // pacing_delay = (((double)(_mss + UecPacket::acksize)) / (((double)_cwnd) / (_base_rtt / 1000.0))) * (1.0 - LCP_PACING_BONUS);
        // pacing_delay *= 1000; // ps
        simtime_picosec one_window_send_time = (_mss + UecPacket::acksize) * _base_rtt;
        pacing_delay = one_window_send_time * (1.0 / _cwnd);
        pacing_delay *= (1.0 - LCP_PACING_BONUS);
        //std::cout << "cwnd is " << _cwnd << " bytes" << endl;
        if (generic_pacer != NULL && generic_pacer->is_pending()) {
            generic_pacer->cancel();
            last_pac_change = eventlist().now();
            generic_pacer->schedule_send(pacing_delay);
        } else {
            generic_pacer = new LcpSmarttPacer(eventlist(), *this);
            generic_pacer->cancel();
            last_pac_change = eventlist().now();
        }
    }
}

// Start the flow
void LcpSrc::doNextEvent() {
    // cout << "LcpSrc::doNextEvent" << endl;
    startflow(); 
}

// Triggers for connection matrixes
void LcpSrc::set_end_trigger(Trigger &end_trigger) { _end_trigger = &end_trigger; }

// Update Network Parameters
void LcpSrc::updateParams(uint64_t base_rtt_intra, uint64_t base_rtt_inter, uint64_t bdp_intra, uint64_t bdp_inter, uint64_t intra_queuesize, uint64_t inter_queuesize) {
    uint64_t queuesize_bytes = 0;
    bool is_flow_inter_dc = (src_dc != dest_dc);
    _base_rtt_intra = base_rtt_intra;
    _base_rtt_inter = base_rtt_inter;
    _network_linkspeed = INTER_LINK_SPEED_MODERN * 1e9;
    if (is_flow_inter_dc) {
        _hop_count = 9;
        _base_rtt = base_rtt_inter;
        _bdp = bdp_inter;
        queuesize_bytes = inter_queuesize;
    } else {
        _hop_count = 6;
        _base_rtt = base_rtt_intra;
        _bdp = bdp_intra;
        queuesize_bytes = intra_queuesize;
    }

    if (precision_ts != 1) {
        _base_rtt = (((_base_rtt + precision_ts - 1) / precision_ts) * precision_ts);
    }

    int time_to_drain_queue = _switch_queue_size * 8 / INTER_LINK_SPEED_MODERN * 1000;

    double inter_queue_latency_ns = (float) inter_queuesize * 8 / (float) INTER_LINK_SPEED_MODERN;
    double inter_queue_latency_ps = inter_queue_latency_ns * 1000;
    double intra_queue_latency_ns = (float) intra_queuesize * 8 / (float) INTER_LINK_SPEED_MODERN;
    double intra_queue_latency_ps = intra_queue_latency_ns * 1000;
    if (is_flow_inter_dc) {
        // going and ack coming back
        // _rto = _base_rtt + 2* (7 * intra_queue_latency_ps + inter_queue_latency_ps);
        _rto = _base_rtt + (5 * intra_queue_latency_ps + 3 * inter_queue_latency_ps);
        // _rto = _base_rtt + 2* (5 * intra_queue_latency_ps + 3 * inter_queue_latency_ps);
        // ec_wait_time = 7 * intra_queue_latency_ps + inter_queue_latency_ps;
        ec_wait_time = 5 * intra_queue_latency_ps + 3 * inter_queue_latency_ps;
    } else {
        _rto = _base_rtt + (5 * intra_queue_latency_ps);
        // _rto = _base_rtt + 2* (5 * intra_queue_latency_ps);
        ec_wait_time = 5 * intra_queue_latency_ps;
    }

    // _rto = _base_rtt * 3;
    // _rto_margin = _base_rtt / 8;
    _rto_margin = 0;
    _rtx_pending = false;
    _crt_path = 0;
    

    _next_pathid = 1;
    next_window_end = eventlist().now();

    assert(starting_cwnd_bdp_ratio > 0);

    printf("BDP is %lu\n", _bdp);
    printf("Starting cwnd is %f\n", starting_cwnd);
    printf("Starting cwnd bdp ratio is %f\n", starting_cwnd_bdp_ratio);

    if (starting_cwnd == 1) {
        _cwnd = _bdp * starting_cwnd_bdp_ratio;
        printf("CWND3 HERE IS %f\n", _cwnd);
    } else {
        _cwnd = starting_cwnd;
        printf("CWND4 HERE IS %f\n", _cwnd);
    }

    if (force_cwnd != 0) {
        _cwnd = force_cwnd;
        printf("555 CWND HERE IS %f\n", _cwnd);
    }

    printf("CWND2 HERE IS %f\n", _cwnd);

    if (ai_bytes == 1 && ai_bytes_scale <= 0)
        ai_bytes = _bdp * 0.001;
    else if (ai_bytes_scale > 0) {
        if (ai_bytes_scale > 1) {
            cout << "ai_bytes_scale should be in (0, 1]" << endl;
            abort();
        }
        ai_bytes = _bdp * ai_bytes_scale;
    }
    
    baremetal_rtt = _base_rtt;
    assert(target_to_baremetal_ratio > 0);
    target_rtt = baremetal_rtt * target_to_baremetal_ratio;
    qa_measurement_period_end = target_rtt;
    float queue_latency_ns = (float) queuesize_bytes * 8 / (float) INTER_LINK_SPEED_MODERN;
    queue_latency_ps = queue_latency_ns * 1000.0;
    qa_trigger_rtt = QA_TRIGGER_RTT_FRACTION * queue_latency_ns * 1000.0 + baremetal_rtt;
    target_qdelay = target_rtt - baremetal_rtt;

    // based on gemini, lcp_k and bdp are both in bytes
    if (lcp_k <= 0) {
        lcp_k = (_bdp * 1.0 / 7);
    }
    lcp_k /= lcp_k_scale;
    // ai_bytes /= lcp_k_scale;

    if (md_gain_ecn <= 0 || md_gain_ecn >= 1) {
        md_gain_ecn = lcp_k * 4.0 / (_bdp + lcp_k);
        md_gain_ecn = md_gain_ecn * 1;
    } else {
        cout << "md_gain_ecn is over-written to a constant value: " << md_gain_ecn << " but this should only happen for epoch_type == long" << endl;
    }

    if (md_factor_rtt <= 0 || md_factor_rtt >= 1)
        md_factor_rtt = target_qdelay * 1.0 / (target_qdelay + target_rtt);
    else {
        cout << "md_factor_rtt is over-written to a constant value: " << md_factor_rtt << " but this should only happen for epoch_type == short" << endl;
    }

    if (md_gain_ecn >= 1 || md_factor_rtt >= 1) {
        cout << "md_gain_ecn: " << md_gain_ecn << endl;
        cout << "md_factor_rtt: " << md_factor_rtt << endl;
        cout << "Invalid md_gain_ecn or md_factor_rtt" << endl;
        //abort();
    }

    // assert(qa_trigger_rtt > target_rtt);

    if (epoch_type != "short" && qa_type != "long") {
        cout << "When epoch_type != constant_timestamp, we cannot have qa_type != long" << endl;
        abort();
    }

    if (use_erasure_coding && ec_handler != "src" && ec_handler != "dst") {
        cout << "The entity that manages EC should either be src or dst!" << endl;
        abort();
    }

    if (use_erasure_coding && ec_handler == "src") {
        if (parity_group_size <= 0) {
            cout << "parity_group_size should be > 0" << endl;
            abort();
        }
    }

    // just push a zero initially
    _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, 0, 0));

    // Write the parameters to a file for easy access.
    std::string file_name = PROJECT_ROOT_PATH / ("output/params/params" + _name + "_" + std::to_string(tag) + ".txt");
    std::ofstream MyFile(file_name, std::ios_base::app);

    MyFile << "cc_algorithm_type," << cc_algorithm_type << std::endl;
    MyFile << "Link speed (Gbps)," << INTER_LINK_SPEED_MODERN << std::endl;
    MyFile << "BDP (KB)," << _bdp / 1000 << std::endl;
    MyFile << "Baremetal RTT (us)," << baremetal_rtt / 1000000 << std::endl;
    MyFile << "Target RTT (us)," << target_rtt / 1000000 << std::endl;
    MyFile << "QA Trigger RTT (us)," << qa_trigger_rtt / 1000000 << std::endl;
    MyFile << "Target QDELAY (ns)," << target_qdelay / 1000 << std::endl;
    MyFile << "MSS (bytes)," << PKT_SIZE_MODERN << std::endl;
    float max_queueing_latency_us = ((float) (queuesize_bytes * 8) / (float) INTER_LINK_SPEED_MODERN) / 1000.0;
    MyFile << "Max Queueing Latency (us)," << max_queueing_latency_us << std::endl;
    MyFile << "Starting cwnd (bytes)," << starting_cwnd << std::endl;
    MyFile << "Queue Size (bytes)," << queuesize_bytes << std::endl;
    MyFile << "AI Bytes," << ai_bytes << std::endl;
    MyFile << "K (bytes)," << lcp_k << std::endl;
    MyFile << "Fast Increase Threshold," << fi_threshold << std::endl;
    MyFile << "Use Quick Adapt," << use_qa << std::endl;
    MyFile << "Reduce Cwnd at Timeout," << use_rto_cwnd_reduction << std::endl;
    MyFile << "Use Pacing," << use_pacing << std::endl;
    MyFile << "Use Fast Increase," << use_fi << std::endl;
    MyFile << "Use RTT in epochEnd," << use_rtt << std::endl;
    MyFile << "Use ECN in epochEnd," << use_ecn << std::endl;
    MyFile << "Applying AI per epoch," << apply_ai_per_epoch << std::endl;
    MyFile << "Use Trimming," << use_trimming << std::endl;
    MyFile << "Pacing Bonus," << LCP_PACING_BONUS << std::endl;
    MyFile << "Multiplicative Decrease Gain for ECN," << md_gain_ecn << std::endl;
    MyFile << "Multiplicative Decrease Gain for RTT," << md_factor_rtt << std::endl;
    MyFile << "QA_CWND_RATIO_THRESHOLD," << QA_CWND_RATIO_THRESHOLD << std::endl;
    MyFile << "TARGET_TO_BAREMETAL_RATIO," << target_to_baremetal_ratio << std::endl;
    MyFile << "STARTING_CWND_BDP_RATIO," << starting_cwnd_bdp_ratio << std::endl;
    MyFile << "USE_ERASURE_CODING," << use_erasure_coding << std::endl;
    MyFile << "EC_handler," << ec_handler << std::endl;
    MyFile << "EC Timer (us)," << timeAsUs(ec_wait_time) << std::endl;
    MyFile << "USE_PER_ACK_MD," << use_per_ack_md << std::endl;
    MyFile << "RTO (us)," << timeAsUs(_rto) << std::endl;
    MyFile << "Apply MIMD," << apply_mimd << std::endl;
    MyFile << "inter bdp / intra bdp," << bdp_inter * 1.0 / bdp_intra << std::endl;


    cout << "cc_algorithm_type," << cc_algorithm_type << std::endl;
    cout << "Link speed (Gbps)," << INTER_LINK_SPEED_MODERN << std::endl;
    cout << "BDP (KB)," << _bdp / 1000 << std::endl;
    cout << "Baremetal RTT (us)," << baremetal_rtt / 1000000 << std::endl;
    cout << "Target RTT (us)," << target_rtt / 1000000 << std::endl;
    cout << "QA Trigger RTT (us)," << qa_trigger_rtt / 1000000 << std::endl;
    cout << "Target QDELAY (ns)," << target_qdelay / 1000 << std::endl;
    cout << "MSS (bytes)," << PKT_SIZE_MODERN << std::endl;
    cout << "Max Queueing Latency (us)," << max_queueing_latency_us << std::endl;
    cout << "Starting cwnd (bytes)," << starting_cwnd << std::endl;
    cout << "Queue Size (bytes)," << queuesize_bytes << std::endl;
    cout << "AI Bytes," << ai_bytes << std::endl;
    cout << "K (bytes)," << lcp_k << std::endl;
    cout << "Fast Increase Threshold," << fi_threshold << std::endl;
    cout << "Use Quick Adapt," << use_qa << std::endl;
    cout << "Reduce Cwnd at Timeout," << use_rto_cwnd_reduction << std::endl;
    cout << "Use Pacing," << use_pacing << std::endl;
    cout << "Use Fast Increase," << use_fi << std::endl;
    cout << "Use RTT in epochEnd," << use_rtt << std::endl;
    cout << "Use ECN in epochEnd," << use_ecn << std::endl;
    cout << "Applying AI per epoch," << apply_ai_per_epoch << std::endl;
    cout << "Use Trimming," << use_trimming << std::endl;
    cout << "Pacing Bonus," << LCP_PACING_BONUS << std::endl;
    cout << "Multiplicative Decrease Gain for ECN," << md_gain_ecn << std::endl;
    cout << "Multiplicative Decrease Gain for RTT," << md_factor_rtt << std::endl;
    cout << "QA_CWND_RATIO_THRESHOLD," << QA_CWND_RATIO_THRESHOLD << std::endl;
    cout << "TARGET_TO_BAREMETAL_RATIO," << target_to_baremetal_ratio << std::endl;
    cout << "STARTING_CWND_BDP_RATIO," << starting_cwnd_bdp_ratio << std::endl;
    cout << "USE_ERASURE_CODING," << use_erasure_coding << std::endl;
    cout << "EC_handler," << ec_handler << std::endl;
    cout << "EC Timer (us)," << timeAsUs(ec_wait_time) << std::endl;
    cout << "USE_PER_ACK_MD," << use_per_ack_md << std::endl;
    cout << "RTO (us)," << timeAsUs(_rto) << std::endl;
    cout << "Apply MIMD," << apply_mimd << std::endl;
    cout << "inter bdp / intra bdp," << bdp_inter * 1.0 / bdp_intra << std::endl;

    

    MyFile.close();

    _maxcwnd = _bdp;
    // _cwnd = _bdp;

    printf("Current CWND is %f while BDP is %lu -- mAx %lu\n", _cwnd, _bdp, _maxcwnd);

    printf("MD GAIN IS %f\n", md_gain_ecn);

    target_window = _cwnd;
    // _target_based_received = true;

    tracking_period = 300000 * 1000;

    qa_period = _base_rtt / freq;

    _max_good_entropies = 10; // TODO: experimental value
    _enableDistanceBasedRtx = false;
    last_pac_change = 0;

    update_pacing_delay();

    if (use_per_ack_md && cc_algorithm_type != "lcp") {
        cout << "use_per_ack_md is only deployed for LCP!" << endl;
        abort();
    }
}

void LcpSrc::processBts(UecPacket *pkt) {

    num_trim++;
    count_trimmed_in_rtt++;
    // trimmed_last_rtt++;
    // consecutive_good_medium = 0;
    bytes_acked_in_qa_period += 64;
    saved_trimmed_bytes += 64;

    /* printf("Just NA CK from %d at %lu - %d\n", from, eventlist().now() / 1000, pkt->is_failed); */
    /* printf("BTS1 %d at %lu - %d\n", from, eventlist().now() / 1000, _cwnd); */
    /* printf("BTS2 %d at %lu - %d\n", from, eventlist().now() / 1000, _cwnd); */
    // Reduce Window Or Do Fast Drop
    if (count_received >= ignore_for) {
        if (eventlist().now() > next_qa) {
            need_quick_adapt = true;
            // quick_adapt(true);
        }
    }

    // last_ecn_seen = eventlist().now();
    last_phantom_increase = eventlist().now();

    _list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));

    check_limits_cwnd();

    // mark corresponding packet for retransmission
    auto i = get_sent_packet_idx(pkt->seqno());
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
    send_packets();
}

std::size_t LcpSrc::get_sent_packet_idx(uint32_t pkt_seqno) {
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        if (pkt_seqno == _sent_packets[i].seqno) {
            return i;
        }
    }
    return _sent_packets.size();
}

tuple<uint64_t, simtime_picosec> LcpSrc::update_pending_packets_list(UecAck &pkt) {
    // cummulative ack
    uint64_t num_new_bytes_acked = 0;
    simtime_picosec acked_pkt_ts = 0;
    if (pkt.seqno() == 1) {
        bool skip = true;
        while (!_sent_packets.empty() && (_sent_packets[0].seqno <= pkt.ackno() || _sent_packets[0].acked)) {
            if (!_sent_packets[0].acked) {
                // new bytes getting ackeddata_packet_size()
                num_new_bytes_acked += _sent_packets[0].size;
                acked_pkt_ts = max(acked_pkt_ts, _sent_packets[0].sent_time);
                // skip reduce_unacked for the first new packet acked because it is already acked in receivePacket
                if (skip)
                    skip = false;
                else
                    reduce_unacked(_sent_packets[0].size);
            }
            _sent_packets.erase(_sent_packets.begin());
        }
        return make_tuple(num_new_bytes_acked, acked_pkt_ts);
    }
    if (_sent_packets.empty() || _sent_packets[0].seqno > pkt.ackno()) {
        // duplicate ACK -- since we support OOO, this must be caused by
        // duplicate retransmission
        // now new bytes acked
        return make_tuple(0, 0);
    }
    auto i = get_sent_packet_idx(pkt.seqno());

    if (i == 0) {
        // this should not happen because of cummulative acks, but
        // shouldn't cause harm either
        do {
            if (!_sent_packets[0].acked) {
                num_new_bytes_acked += _sent_packets[0].size;
                acked_pkt_ts = max(acked_pkt_ts, _sent_packets[0].sent_time);
            }
            _sent_packets.erase(_sent_packets.begin());
        } while (!_sent_packets.empty() && _sent_packets[0].acked);
    } else {
        assert(i < _sent_packets.size());
        auto timer = _sent_packets[i].timer;
        auto seqno = _sent_packets[i].seqno;
        auto nacked = _sent_packets[i].nacked;
        auto acked = _sent_packets[i].acked;
        auto sent_packet_size = _sent_packets[i].size;
        auto sent_time = _sent_packets[i].sent_time;
        auto ev_u = _sent_packets[i].entropy_used;
        auto parity_id = _sent_packets[i].parity_id;
        if (!acked) {
            num_new_bytes_acked += sent_packet_size;
            acked_pkt_ts = max(acked_pkt_ts, sent_time);
        }
        _sent_packets[i] = LcpSentPacket(timer, seqno, true, false, false, sent_packet_size, sent_time, ev_u, parity_id);
        if (nacked) {
            --_nack_rtx_pending;
        }
    }
    return make_tuple(num_new_bytes_acked, acked_pkt_ts);
}

void LcpSrc::add_ack_path(const Route *rt) {
    for (auto &r : _good_entropies) {
        if (r == rt) {
            return;
        }
    }
    if (_good_entropies.size() < _max_good_entropies) {
        _good_entropies.push_back(rt);
    } else {
        _good_entropies[_next_good_entropy] = rt;
        ++_next_good_entropy;
        _next_good_entropy %= _max_good_entropies;
    }
}

void LcpSrc::set_traffic_logger(TrafficLogger *pktlogger) { _flow.set_logger(pktlogger); }


void LcpSrc::reduce_unacked(uint64_t amount) {
    if (_unacked >= amount) {
        _unacked -= amount;
    } else {
        _unacked = 0;
    }
    _list_unacked.push_back(std::make_pair(eventlist().now() / 1000, _unacked));
}

void LcpSrc::check_limits_cwnd() {
    // Upper Limit
    // don't check upper limit for mimd
    if (!apply_mimd && _cwnd > _maxcwnd) {
        _cwnd = _maxcwnd;
    }
    
    // Lower Limit
    if (_cwnd < _mss) {
        _cwnd = _mss;
    }

    update_pacing_delay();
}

void LcpSrc::processParityNack(UecNack &pkt) {
    if (!use_erasure_coding || ec_handler != "dst") {
        cout << "How are we receiving parity nack when not using EC with dst as the handler?" << endl;
        abort();
    }
    
    // check if parity is already completed!
    if (pkt.nack_parity_id <= highest_completed_parity_id)
        return;

    // cout << eventlist().now() << ": processParityNack --> nackParity: " << pkt.nack_parity_id << endl;
    
    set<unsigned long>::iterator parity_completed_itr = completed_parity_set.find(pkt.nack_parity_id);
    if (parity_completed_itr != completed_parity_set.end())
        return;

    _list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));


    if (_subflow_reroute) {
        int old_entropy = pkt.subflow_number;

        for (int i = 0; i < _subflow_last_ack.size(); i++) {
            if ((eventlist().now()) - _subflow_last_reroute[i] > _base_rtt && 
                (eventlist().now()) - _subflow_last_ack[i] > _base_rtt * 0.5) {
                _subflow_last_reroute[i] = eventlist().now();
                _subflow_entropies[i] = rand() % 250;
            } 
        }                         
    }

    if (_route_strategy == PLB) {
        if (eventlist().now() > plb_timeout_wait) {
            plb_timeout_wait = eventlist().now() + _rto;
            plb_congested_rounds = 0;
            plb_entropy = rand() % 250;
        }
    }

    /* printf("Received Parity NACK for %d at %lu\n", pkt.nack_parity_id, eventlist().now() / 1000);

    simtime_picosec max_time = 0;
    int max_time_index = -1;

    int starting_idx = rand() % _subflow_entropies.size();
    for (int i = starting_idx; i < (starting_idx + _subflow_entropies.size()); i++) {
        printf("Time Index: %d (%d) and Time %f\n", max_time_index, i, timeAsUs(_subflow_last_ack[i % _subflow_entropies.size()]));

        if (_subflow_last_ack[i % _subflow_entropies.size()] > max_time) {
            max_time = _subflow_last_ack[i % _subflow_entropies.size()];
            max_time_index = i % _subflow_entropies.size();
            printf("Max Time Index: %d (%d) and Time %f\n", max_time_index, i, timeAsUs(max_time));
        }
    }

    for (int i = 0; i < _subflow_entropies.size(); i++) {
        if (eventlist().now() - _subflow_last_ack[i] >= ec_wait_time && eventlist().now() - _subflow_last_reroute[i] >= _base_rtt) {
            printf("Changing Parity NACK - From Subflow %d to %d - Entropy from %d to %d - Time %f\n", i, max_time_index, _subflow_entropies[i], _subflow_entropies[max_time_index], timeAsUs(eventlist().now() - _flow_start_time));
            _subflow_entropies[i] = _subflow_entropies[max_time_index];
            _subflow_last_reroute[i] = eventlist().now();
        }
    } */

    
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        auto &sp = _sent_packets[i];
        if (!sp.acked && sp.parity_id == pkt.nack_parity_id) {
            // we consider this packet as dropped!
            // cout << "trying to resend packet: " << sp.seqno << ", rtx_pending? " << _rtx_pending << endl;
            // upon re-sending the RTO timer will be reset
            sp.nacked = true;
            reduce_unacked(sp.size);
            if (!resend_packet(i)) {
                // cout << "not sent! " << get_unacked() << ", " << _cwnd << endl;
                _rtx_pending = true;
            } else {
                // cout << "re-sent!" << endl;
                aggregate_retrans += sp.size;
                _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, aggregate_timedout, aggregate_retrans));
            }
        }
    }
}

void LcpSrc::processNack(UecNack &pkt) {
    if (!use_trimming) {
        std::cout << "LCP is not using trimming, how are we getting nacks?" << endl;
        exit(1);
    }
    num_trim++;
    count_trimmed_in_rtt++;
    // trimmed_last_rtt++;
    // consecutive_good_medium = 0;
    bytes_acked_in_qa_period += 64;
    saved_trimmed_bytes += 64;

    // last_ecn_seen = eventlist().now();
    last_phantom_increase = eventlist().now();

    _cwnd -= _mss;
    check_limits_cwnd();

    if (!pkt.is_failed) {
        _list_nack.push_back(std::make_pair(eventlist().now() / 1000, 1));
    }

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
    send_packets();
}

/* Choose a route for a particular packet */
int LcpSrc::choose_route() {

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
            printf("as[%d]: %d\n", path_id, _avoid_score[path_id]);
            _avoid_score[path_id]--;
            ctr++;
            // re-choosing path
            std::cout << "re-choosing path " << path_id << endl;
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
    case PLB: {
        int plb_n = 1;
        int plb_m = 1;

        if (plb_congested_rounds > plb_n) {
            plb_congested_rounds = 0;
            plb_entropy = rand() % 250;
        } 
        _crt_path = plb_entropy;
        break;
    }
    case SIMPLE_SUBFLOW:
        _current_subflow = (_current_subflow + 1) % _subflow_entropies.size();
        _crt_path = _subflow_entropies[_current_subflow];

        if (_fixed_route != -1) {
            _crt_path = _fixed_route;
        }
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

        if (false) {
            uint64_t allpathssizes = _mss * _paths.size();
            if (_highest_sent < max(_maxcwnd, (uint64_t)1)) {
                curr_entropy++;
                _crt_path++;
                if (_crt_path == _paths.size()) {
                    _crt_path = 0;
                }
            } else {
                if (!_good_entropies_list.empty()) {
                    _crt_path = _good_entropies_list.back();
                    //_good_entropies_list.pop_back();
                } else {
                    curr_entropy++;
                    _crt_path = curr_entropy % _paths.size();
                }
            }
            break;
        } else {
            uint64_t allpathssizes = _mss * _paths.size();
            if (_highest_sent < max(_maxcwnd, (uint64_t)1)) {
                /*printf("Trying this for %d // Highest Sent %d - cwnd %d - "
                       "allpathsize %d\n",
                       from, _highest_sent, _maxcwnd, allpathssizes);*/
                _crt_path++;
                // printf("Trying this for %d\n", from);
                if (_crt_path == _paths.size()) {
                    // permute_paths();
                    _crt_path = 0;
                }
            } else {
                if (_next_pathid == -1) {
                    assert(_paths.size() > 0);
                    _crt_path = random() % _paths.size();
                } else {
                    _crt_path = _next_pathid;
                }
            }
            break;
        }
    }
    case ECMP_RANDOM_ECN: {
        _crt_path = from;
        // _crt_path = (random() * 1) % _paths.size();
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
            } else {
                // eventually we'll find one that's zero
                break;
            }
        }
        break;
    }
    case SINGLE_PATH:
        // abort(); // not sure if this can ever happen - if it can, remove this
                 // line
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

int LcpSrc::next_route() {
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

void LcpSrc::finish_flow() {

    _flow_finished = true;
    FLOW_ACTIVE--;
    _sent_packets.clear();

    if (COLLECT_DATA) {
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
    }
    cout << "Flow id=" << _flow.flow_id() << 
        " Completion time is " << timeAsUs(eventlist().now()) - timeAsUs(_flow_start_time) << 
        "us - Flow Finishing Time " << timeAsUs(eventlist().now()) << 
        "us - Flow Start Time "<< timeAsUs(_flow_start_time) << 
        " - Size Finished Flow " << _flow_size << 
        " - From " << from << " - To " << to 
        << " - Flow Retx Count " << number_retx << endl;

    if (_end_trigger) {
        _end_trigger->activate();
    }

    if (generic_pacer != NULL)
        generic_pacer->cancel();

    eventlist().cancelPendingSource(*this);
}

bool LcpSrc::last_batch_all_acked() {
    if (_sent_packets.size() == 0) {
        // there are no packets left. Finish
        return true;
    }
    int idx = _sent_packets.size() - 1;
    while(idx >= 0 && _sent_packets[idx].parity_id == highest_possible_parity_id) {
        if (!_sent_packets[idx].acked)
            return false;
        idx--;
    }
    return true;
}


void LcpSrc::processAck(UecAck &pkt, bool force_marked) {
    tuple<uint64_t, simtime_picosec> pkt_acked_info = update_pending_packets_list(pkt);
    uint64_t num_bytes_acked = get<0>(pkt_acked_info);
    simtime_picosec acked_pkt_ts = get<1>(pkt_acked_info);
    UecAck::seq_t seqno = pkt.ackno();

    bool pkt_ecn_marked = pkt.flags() & ECN_ECHO;   // ECN was marked on data packet and echoed on ACK

    
    if (_route_strategy == PLB) {
        plb_delivered += 4096;
        if (pkt_ecn_marked) {
            plb_congested += 4096;
        }

        double plb_k = 0.25;

        if (eventlist().now() > plb_last_rtt) {
            if (plb_congested > plb_k * plb_delivered) {
                plb_congested_rounds++;
            } else {
                plb_congested_rounds = 0;
            }  
            plb_delivered = 0;
            plb_congested = 0;
            plb_last_rtt = eventlist().now() + _base_rtt;
        }
    }

    if (pkt_ecn_marked && COLLECT_DATA)
        _list_is_ecn_congested.push_back(eventlist().now() / 1000);

    if (use_per_ack_ewma) {
        // pass 1 if ecn_maked and 0 otherwise
        ecn_fraction_ewma = computeEwma(ecn_fraction_ewma, pkt_ecn_marked, lcp_ecn_alpha);
    }

    //received_so_far++;

    //printf("Node %d - Received %d at %lu\n", from, received_so_far,  eventlist().now() / 1000);

    _subflow_last_ack[pkt.subflow_number] = eventlist().now();

    // erasure coding
    if (use_erasure_coding) {
        if (ec_handler == "src") {
            unsigned long parity_id = get_parity_id(seqno, _mss);
            if (parity_id <= 0) {
                cout << "How is the parity id of the received packet <= 0!" << endl;
                abort();
            }

            if (parity_id > highest_completed_parity_id) {
                map<unsigned long, unsigned int>::iterator parity_id_found_itr = rcvd_parity_ids_map.find(parity_id);
                if (parity_id_found_itr == rcvd_parity_ids_map.end()) {
                    rcvd_parity_ids_map.insert(make_pair(parity_id, 0));
                    parity_id_found_itr = rcvd_parity_ids_map.find(parity_id);
                }

                if (num_bytes_acked > 0) {
                    // new packets are being acked
                    if (num_bytes_acked % _mss != 0) {
                        cout << "How is num_bytes_acked % _mss != 0" << endl;
                    }
                    parity_id_found_itr->second += int(num_bytes_acked / _mss);

                    if (parity_id_found_itr->second > parity_group_size) {
                        cout << "How is the number of packets acked with parity id of " << parity_id_found_itr->second <<
                            " more than parity_group_size" << endl;
                        abort();
                    }

                    // update highest_completed_parity_id
                    parity_id_found_itr = rcvd_parity_ids_map.find(highest_completed_parity_id + 1);
                    while (parity_id_found_itr != rcvd_parity_ids_map.end() && parity_id_found_itr->second >= (parity_group_size - parity_correction_capacity)) {
                        rcvd_parity_ids_map.erase(parity_id_found_itr);
                        highest_completed_parity_id++;
                        parity_id_found_itr = rcvd_parity_ids_map.find(highest_completed_parity_id + 1);
                    }
                }
            }
        } else if (ec_handler == "dst") {
            if (pkt.completed_batch_parity_id > 0)
                completed_parity_set.insert(pkt.completed_batch_parity_id);
            if (pkt.highest_completed_parity_id > highest_completed_parity_id) {
                highest_completed_parity_id = pkt.highest_completed_parity_id;
                // erase the unneeded parities from completed_parity_set
                auto it = completed_parity_set.begin();
                while (it != completed_parity_set.end()) {
                    if (*it < highest_completed_parity_id) {
                        // Erase returns an iterator pointing to the element after the erased one.
                        it = completed_parity_set.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }
    
    if (epoch_enabled) {
        total_new_bytes_acked_in_epoch += num_bytes_acked;
        if (pkt_ecn_marked) {
            _list_ecn_received.push_back(std::make_pair(eventlist().now() / 1000, 1));
            ecn_marked_bytes_in_epoch += num_bytes_acked;
        }
    }

    

    uint64_t now_time = 0;
    if (precision_ts == 1) {
        now_time = eventlist().now();
    } else {
        now_time = (((eventlist().now() + precision_ts - 1) / precision_ts) * precision_ts);
    }
    simtime_picosec ts = pkt.ts();
    uint64_t newRtt = now_time - ts;

    if (qa_enabled) {
        bytes_acked_in_qa_period += num_bytes_acked;
    }
    bytes_acked_in_collection_period += num_bytes_acked;
    
    update_pacing_delay();

    if (!pkt_ecn_marked) {
        _next_pathid = pkt.pathid_echo;
        _good_entropies_list.push_back(pkt.pathid_echo);
    } else {
        _next_pathid = -1;
        // ecn_last_rtt = true;
    }
    
    if (highest_completed_parity_id > highest_possible_parity_id) {
        cout << "highest_completed_parity_id: " << highest_completed_parity_id << endl;
        cout << "highest_possible_parity_id: " << highest_possible_parity_id << endl;
        cout << "How is highest_completed_parity_id > highest_possible_parity_id?" << endl;
        abort();
    }

    if (_flow_finished) {
        // flow is already finished, no need to check the extra packets
        return;
    }

    //printf("Flow Id %d - Received %d of %d\n", _flow.flow_id(), seqno, _flow_size);

    bool trigger_flow_finished = (seqno >= _flow_size && _sent_packets.empty() && !_flow_finished);
    if (use_erasure_coding) {
        trigger_flow_finished = trigger_flow_finished || (highest_completed_parity_id == highest_possible_parity_id);
        if (!trigger_flow_finished && highest_completed_parity_id == highest_possible_parity_id - 1) {
            // check if the last batch that has the highest_possible_parity_id are all acked
            trigger_flow_finished = last_batch_all_acked();
        }
    }

    if (trigger_flow_finished) {

        if (f_flow_over_hook) {
            f_flow_over_hook(pkt);
        }
        finish_flow();
        return;
    }

    // TODO: new ack, we don't care about
    // ordering for now. Check later though
    if (seqno >= _highest_sent) {
        _highest_sent = seqno;
        update_next_byte_to_send();
    }


    if (using_dctcp || true) {
        //printf("Received Packet at %f - RTT %f - CWND %f\n", timeAsUs(eventlist().now()), timeAsUs(newRtt), _cwnd);
    }

    adjust_window(ts, pkt_ecn_marked, newRtt, seqno, num_bytes_acked, acked_pkt_ts);

    if (COLLECT_DATA) {
        _received_ecn.push_back(std::make_tuple(eventlist().now(), pkt_ecn_marked, _mss, newRtt));
        _list_rtt.push_back(std::make_tuple(eventlist().now() / 1000, newRtt / 1000, pkt.seqno(), pkt.ackno(),
                                            _base_rtt / 1000, target_rtt / 1000, qa_trigger_rtt / 1000, baremetal_rtt / 1000));
    }

    _effcwnd = _cwnd;
    send_packets();
}

uint64_t LcpSrc::get_unacked() {
    return _unacked;
}

void LcpSrc::receivePacket(Packet &pkt) {
    // Every packet received represents one less packet in flight
    if (pkt._queue_full || pkt.bounced() == false) {
        reduce_unacked(_mss);
    } else {
        exit(0);
        printf("Never here\n");
    }
    tot_pkt_seen_qa++;

    // TODO: receive window?
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);

    if (_logger) {
        _logger->logUec(*this, UecLogger::UEC_RCV);
    }

    switch (pkt.type()) {
        case UEC:
            // BTS
            if (_bts_enabled) {
                if (pkt.bounced()) {
                    // processBts((UecPacket *)(&pkt));
                    increasing = false;
                }
            }
            break;
        case UECACK:
            count_received++;
            if (COLLECT_DATA) {
                _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
            }
            //printf("Received Packet EV %d - %d\n", pkt.pathid_echo, pkt.pathid());
            processAck(dynamic_cast<UecAck &>(pkt), false);

            pkt.free();
            break;
        case ETH_PAUSE:
            printf("Src received a Pause\n");
            // processPause((const EthPausePacket &)pkt);
            pkt.free();
            return;
        case UECNACK:
            // Todo: fail simulation...
            if (COLLECT_DATA) {
                _list_cwd.push_back(std::make_pair(eventlist().now() / 1000, _cwnd));
            }
            if (pkt.nack_parity_id > 0) {
                processParityNack(dynamic_cast<UecNack &>(pkt));
                pkt.free();
            } else if (use_trimming) {
                _next_pathid = -1;
                count_received++;
                processNack(dynamic_cast<UecNack &>(pkt));
                pkt.free();
            }
            break;
        default:
            std::cout << "unknown packet receive with type code: " << pkt.type() << "\n";
            return;
    }

    // if we have some packets waiting or inflight, then check regularly for timedout packets
    if (get_unacked() < _cwnd && !_flow_finished && (!_sent_packets.empty() || get_unacked() > 0)) {
        eventlist().sourceIsPendingRel(*this, 1000);
    }
}

bool LcpSrc::shouldTriggerEpochEnd(uint32_t ackno, simtime_picosec acked_pkt_ts) {
    // Epoch is only defined for LCP
    if (cc_algorithm_type != "lcp")
        return false;
    
    if (!epoch_enabled)
        return false;
    
    if (epoch_type == "long" || epoch_type == "short") {
        return (acked_pkt_ts >= epoch_end_time);
    }
    cout << "Uknown epoch type:" << epoch_type << endl;
    exit(0);
}

void LcpSrc::resetEpochParams() {
    ecn_marked_bytes_in_epoch = 0;
    total_new_bytes_acked_in_epoch = 0;
    // epoch_end_time = eventlist().now() + most_recent_rtt;
    epoch_end_time = eventlist().now();
    epoch_start_cwnd = _cwnd;
}

void LcpSrc::processQaMeasurementEnd(simtime_picosec rtt) {

    if (COLLECT_DATA) {
        _list_qa_ended.push_back(std::make_tuple(eventlist().now() / 1000, bytes_acked_in_qa_period, _cwnd, QA_CWND_RATIO_THRESHOLD, rtt, qa_trigger_rtt));
    }
    // if use_rtt is false, it should not work for QA either
    bool trigger_qa_rtt = use_rtt && (rtt > qa_trigger_rtt);
    if (COLLECT_DATA && trigger_qa_rtt)
        _list_qa_rtt_triggerred.push_back(eventlist().now() / 1000);

    bool trigger_qa_bytes_acked;
    
    if (qa_type == "long")
       trigger_qa_bytes_acked = (bytes_acked_in_qa_period < _cwnd * QA_CWND_RATIO_THRESHOLD);
    else if (qa_type == "short")
        trigger_qa_bytes_acked = (bytes_acked_in_qa_period < (_cwnd * QA_CWND_RATIO_THRESHOLD * 1.0 * constant_timestamp_epoch_ratio));

    if (COLLECT_DATA && trigger_qa_bytes_acked) {
        _list_qa_bytes_acked_triggerred.push_back(eventlist().now() / 1000);
    }

    bool trigger_qa = trigger_qa_rtt || trigger_qa_bytes_acked;
    if (trigger_qa) {
        // Reduce cwnd to be equal to the bytes received in this QA period. 
        _cwnd = bytes_acked_in_qa_period * 0.96;
        
        // Reset epoch info
        resetEpochParams();
        _consecutive_good_epochs = 0;
        epoch_enabled = false;
        fast_increase_round = 0;
        epoch_counter = _cwnd;

        // reset qa info
        qa_enabled = false;
        qa_measurement_period_end = eventlist().now();
    } else {
        if (qa_type == "long")
            qa_measurement_period_end = eventlist().now() + target_rtt;
        else if (qa_type == "short")
            qa_measurement_period_end = eventlist().now() + (target_rtt * 1.0 * constant_timestamp_epoch_ratio);
    }
    bytes_acked_in_qa_period = 0;
    check_limits_cwnd();
}

void LcpSrc::adjust_window_aimd(simtime_picosec ts, bool ecn, simtime_picosec rtt, uint32_t ackno, uint64_t num_bytes_acked, simtime_picosec acked_pkt_ts) {

    if (apply_mimd) {
        cout << "How is adjust_window_aimd called in MIMD!" << endl;
        abort();
    }
    
    // QA is only for LCP
    if (use_qa && cc_algorithm_type == "lcp") {
        if (!qa_enabled && acked_pkt_ts >= qa_measurement_period_end) {
            qa_enabled = true;
            if (qa_type == "long")
                qa_measurement_period_end = eventlist().now() + target_rtt;
            else if (qa_type == "short")
                qa_measurement_period_end = eventlist().now() + (target_rtt * 1.0 * constant_timestamp_epoch_ratio);
            if (COLLECT_DATA) {
                _list_qa_ended.push_back(std::make_tuple(eventlist().now() / 1000, 0, -1, -1, 0, 0));
            }
        } else if (qa_enabled && eventlist().now() >= qa_measurement_period_end) {
            processQaMeasurementEnd(rtt);
        }
    }

    if ((!use_ecn || !ecn) && (!use_rtt || rtt < target_rtt)) {
        if (!apply_ai_per_epoch) {
            additive_increase(num_bytes_acked);
        }
    } else {
        _consecutive_good_epochs = 0;
        fast_increase_round = 0;
        if (cc_algorithm_type == "mprdma") {
            if (use_rtt) {
                cout << "use_rtt should not be enabled with mprdma!" << endl;
                abort();
            }
            _cwnd -= (0.5 * _mss);
        } else if (cc_algorithm_type == "lcp") {
            if (use_per_ack_md) {
                if (!use_ecn) {
                    cout << "ecn must be enabled for using per_ack_md" << endl;
                    abort();
                }
                // if rtt is congested, we keep the cwnd constant
                if (ecn) {
                    // apply MD
                    _cwnd -= md_gain_ecn * num_bytes_acked;
                }
            }
        } else {
            cout << "Unknown cc_algorithm_type: " << cc_algorithm_type << endl;
            abort();
        }
    }
    
    if (shouldTriggerEpochEnd(ackno, acked_pkt_ts))
        processEpochEnd(rtt);

    check_limits_cwnd();
}


void LcpSrc::adjust_window_aimd_phantom(simtime_picosec ts, bool ecn, simtime_picosec rtt, uint32_t ackno, uint64_t num_bytes_acked, simtime_picosec acked_pkt_ts) {

    if (apply_mimd) {
        cout << "How is adjust_window_aimd called in MIMD!" << endl;
        abort();
    }
    
    // QA is only for LCP
    if (use_qa && cc_algorithm_type == "lcp") {
        if (!qa_enabled && acked_pkt_ts >= qa_measurement_period_end) {
            qa_enabled = true;
            if (qa_type == "long")
                qa_measurement_period_end = eventlist().now() + target_rtt;
            else if (qa_type == "short")
                qa_measurement_period_end = eventlist().now() + (target_rtt * 1.0 * constant_timestamp_epoch_ratio);
            if (COLLECT_DATA) {
                _list_qa_ended.push_back(std::make_tuple(eventlist().now() / 1000, 0, -1, -1, 0, 0));
            }
        } else if (qa_enabled && eventlist().now() >= qa_measurement_period_end) {
            processQaMeasurementEnd(rtt);
        }
    }

    if ((!use_ecn || !ecn) && (!use_rtt || rtt < target_rtt)) {
        if (!apply_ai_per_epoch) {
            additive_increase(num_bytes_acked);
        }
    } else {
        _consecutive_good_epochs = 0;
        if (cc_algorithm_type == "mprdma") {
            if (use_rtt) {
                cout << "use_rtt should not be enabled with mprdma!" << endl;
                abort();
            }
            _cwnd -= (0.5 * _mss);
        } else if (cc_algorithm_type == "lcp") {
            if (use_per_ack_md) {
                if (!use_ecn) {
                    cout << "ecn must be enabled for using per_ack_md" << endl;
                    abort();
                }
                // if rtt is congested, we keep the cwnd constant
                if (ecn) {
                    // apply MD
                    _cwnd -= md_gain_ecn * num_bytes_acked;
                }
            }
        } else {
            cout << "Unknown cc_algorithm_type: " << cc_algorithm_type << endl;
            abort();
        }
    }
    
    if (shouldTriggerEpochEnd(ackno, acked_pkt_ts))
        processEpochEnd(rtt);

    check_limits_cwnd();
}

double LcpSrc::calculate_drain_time_ratio(double slowdown_percentage, double phantom_queue_max_ratio) {
    // slowdown_percentage: The slowdown as a percentage (e.g., 5 for 5%)
    // phantom_queue_max_ratio: Ratio of phantom queue max size to real queue max size (e.g., 10)
    
    // Convert slowdown percentage to decimal (e.g., 5% -> 0.05)
    double slowdown_decimal = slowdown_percentage / 100.0;
    
    // Calculate drain rate as percentage of line rate (e.g., 0.95 for 5% slowdown)
    double drain_rate = 1.0 - slowdown_decimal;
    
    // Calculate ratio of drain times
    double drain_time_ratio = 1.0 / (phantom_queue_max_ratio / drain_rate);
    
    return drain_time_ratio;
}

void LcpSrc::processEpochEnd(simtime_picosec rtt) {

    if (apply_mimd) {
        cout << "processEpochEnd while MIMD is activated!!" << endl;
        abort();
    }

    double ecn_fraction = ecn_marked_bytes_in_epoch * 1.0 / total_new_bytes_acked_in_epoch;
    if (!use_per_ack_ewma)
        ecn_fraction_ewma = computeEwma(ecn_fraction_ewma, ecn_fraction, lcp_ecn_alpha);


    average_online += ecn_fraction_ewma;
    average_count++;
    if (eventlist().now() - last_average_update > _base_rtt / 2) {
        old_average_online = current_average_online;
        current_average_online = average_online / average_count;
        average_online = 0;
        average_count = 0;
        last_average_update = eventlist().now();
    }

    _list_ecn_ewma.push_back(std::make_pair(eventlist().now() / 1000, ecn_fraction_ewma));

    bool ecn_congested = use_ecn && (ecn_fraction > 0);
    // for per ack md, we only check for rtt congested in epoch end
    if (use_per_ack_md)
        ecn_congested = false;
    bool rtt_congested = use_rtt && (rtt > target_rtt);
    if (rtt_congested && COLLECT_DATA)
        _list_is_rtt_congested.push_back(eventlist().now() / 1000);

    if (ecn_congested || rtt_congested) {
        // Multiplicative decrease using worse of ECN and RTT decrease factor values.
        double ecn_reduction_factor = (ecn_fraction_ewma * md_gain_ecn);
        double rtt_reduction_factor = (rtt_congested ? md_factor_rtt : 0);
        assert(ecn_reduction_factor >= 0 && ecn_reduction_factor < 1);
        assert(rtt_reduction_factor >= 0 && ecn_reduction_factor < 1);
        // cout << timeAsUs(eventlist().now()) << ":" << flow_id() << ", ecn_fraction_ewma: " << ecn_fraction_ewma << ", ecn_reduction_factor: " << ecn_reduction_factor << ", old cwnd: " << _cwnd;
        if (use_per_ack_md) {
            _cwnd = min(_cwnd, epoch_start_cwnd * (1 - rtt_reduction_factor));
        } else {

            if (lcp_type == "aimd_phantom") {

                double beta = 0;
                bool did_increase = false;

                if (old_ecn_ewma == 0 || ecn_fraction_ewma > 0.9) { // If we are in the phantom queue    
                    beta = md_gain_ecn;
                } else if (ecn_fraction_ewma < old_ecn_ewma) { // If Phantom queue is decreasing, we do additive increase
                    additive_increase(total_new_bytes_acked_in_epoch * 1);
                    did_increase = true;
                } else {
                    beta = md_gain_ecn;
                } 

                bool is_phantom_only_congestion = true;
                if (rtt > _base_rtt * 1.055) { // Need to Scale with Phantom Queue Size
                    is_phantom_only_congestion = false;
                }

                if (ecn_fraction_ewma > 0) {
                    if (is_phantom_only_congestion) {
                        beta *= 0.35; // Scale beta if phantom only congestion
                    } 
                }
                if (did_increase) {
                    beta = 0; // If we just increased, then no need to also decrease
                }   

                if (!did_increase) {
                    ecn_reduction_factor = (ecn_fraction_ewma * beta);
                    _cwnd *= (1-ecn_reduction_factor);
                }

            } else {
                _list_cwnd_reduction_factor.push_back(std::make_tuple(eventlist().now() / 1000, max(ecn_reduction_factor, rtt_reduction_factor)));
                _cwnd *= (1 - max(ecn_reduction_factor, rtt_reduction_factor));
                
            }      
       
        }
        // cout << ", new cwnd: " << _cwnd << endl;
        _consecutive_good_epochs = 0;
        fast_increase_round = 0;
    } else {
        // Declare epoch as uncongested
        _consecutive_good_epochs++;
        fast_increase_round++;
        if (apply_ai_per_epoch)
            additive_increase(total_new_bytes_acked_in_epoch);
    }

    check_limits_cwnd();
    old_ecn_ewma = ecn_fraction_ewma;

    if (COLLECT_DATA)
        _list_epoch_ended.push_back(std::make_tuple(eventlist().now() / 1000, ecn_marked_bytes_in_epoch, total_new_bytes_acked_in_epoch, ecn_fraction, ecn_fraction_ewma, epoch_start_cwnd));

    simtime_picosec previous_epoch_end_time = epoch_end_time;
    resetEpochParams();
    if (epoch_type == "short") {
        epoch_end_time = previous_epoch_end_time + (rtt * 1.0 * constant_timestamp_epoch_ratio);
    }
}

float LcpSrc::computeEwma(double ewma_estimate, double new_value, double alpha) {
    // if (ewma_estimate != (ewma_estimate * (1 - alpha)) + (new_value * alpha)) {
    //     cout << "old ewma_estimate: " << ewma_estimate << ", new_value: " << new_value << ", new ewma_estimate: " << 
    //         (ewma_estimate * (1 - alpha)) + (new_value * alpha) << endl;
    //     cout << "------------------------" << endl;
    // }
    return (ewma_estimate * (1 - alpha)) + (new_value * alpha);
}

simtime_picosec LcpSrc::contract_target_delay_vegas(mem_b cwnd, simtime_picosec rtt, simtime_picosec delay) {
    // With 2 flows the delay is 0.1 rtprop.
    double rate = (double)cwnd * 8 / timeAsSec(rtt);  // bits per second
    // For Internet CC, the linkspeed is not known, so some constant should be used.
    double ratio = rate / (double)_network_linkspeed;
    simtime_picosec target_delay = ((double)_base_rtt_inter / contract_scaling) * (1 / ratio);
    // cout << "Cwnd: " << cwnd << " RTT: " << rtt << " Delay: " << delay << endl;
    // cout << "Rate: " << rate << " Ratio: " << ratio << " Target Delay: " << target_delay << endl;
    return max((simtime_picosec)0, target_delay);
}

void LcpSrc::adjust_window_mimd(bool skip, simtime_picosec rtt, mem_b newly_acked_bytes) {
    simtime_picosec delay = rtt - _base_rtt;
    // simtime_picosec rtt = delay + _base_rtt;
    simtime_picosec now = eventlist().now();
    simtime_picosec target_delay = (this->*contract_target_delay)(_cwnd, rtt, delay);

    // MIMD
    double delay_ratio = 2;
    if (delay > 0)
        delay_ratio = (double)target_delay / (double)delay;
    delay_ratio = min(2.0, delay_ratio);

    double target_rtt = _base_rtt + target_delay;
    double current_rate = (double)_cwnd / timeAsSec(rtt);  // bytes per second
    double target_cwnd = current_rate * timeAsSec(target_rtt);  // bytes
    double cwnd_ratio = (double)target_cwnd / (double)_cwnd;

    double rate = (double)_cwnd * 8 / timeAsSec(rtt);  // bits per second
    double rate_ratio = rate / (double)_network_linkspeed;

    // cout << "Cwnd: " << _cwnd << " RTT: " << rtt << " Delay: " << delay
    //      << " Target Delay: " << target_delay << " Target RTT: " << target_rtt
    //      << " Delay ratio: " << delay_ratio << " Cwnd Ratio: " << cwnd_ratio << endl;
    // cout << "Target delay ratio: " << (double)target_delay * contract_scaling / (double)_network_rtt
    //      << " Rate: " << rate << " Rate Ratio: " << rate_ratio << endl;

    // Every RTT we want:
    // To obtain rtt that has been produced due to cwnd change, we need to wait 2 rtts.
    if (now > t_last_update + 2 * rtt) {
        // cwnd is mean of current and target_cwnd = _cwnd * ratio;
        double new_cwnd = ((double)_cwnd + target_cwnd) / 2;
        // double new_cwnd = (double)_cwnd * (1 + delay_ratio) / 2;
        // // it rtprop was 0 then we'd want to average, but now we just want to
        // // increase cwnd so that the delay increases.
        // // new_cwnd = new_cwnd - (double)_base_bdp * (ratio - 1) / 2;
        _cwnd = min((mem_b)new_cwnd, (mem_b)(2 * _cwnd));
        t_last_update = now;
    }

    // Alternate updates for cwnd

    // // So every packet, we want:
    // _cwnd += (ratio - 1) * _mtu;
    // // If ratio is 1, no change, if ratio is 2, we double the cwnd, if ratio is
    // // 0.5, we half the cwnd.

    // // Poseidon style
    // if (cwnd_ratio >= 1) {
    //     _cwnd += (mem_b) ((cwnd_ratio - 1) * newly_acked_bytes);
    // } else if (cwnd_ratio < 1 && now > t_last_decrease + rtt) {
    //     t_last_decrease = now;
    //     _cwnd = (mem_b) (cwnd_ratio * (double) _cwnd);
    // }

    // // AIMD
    // if (delay > target_delay && now >= t_last_decrease + rtt) {
    //     // MD
    //     _cwnd -= _mtu * fair_decrease_scaling_factor *
    //              (1.0 - ((double) target_delay / (double) delay));  // per packet update
    //     logMetricCCEvent(CCEventType::FAIR_DECREASE, _cwnd);
    // }
    // else if (delay <= target_delay) {
    //     // AI
    //     _cwnd +=
    //         ((double)_mtu / _cwnd) * (fair_increase_scaling_factor * _network_bdp);
    //     logMetricCCEvent(CCEventType::FAIR_INCREASE, _cwnd);
    // }

    check_limits_cwnd();
}

void LcpSrc::clamp_cwnd() {
    if (_cwnd < _mss)
        _cwnd = _mss;

    if (_cwnd > _maxcwnd)
        _cwnd = _maxcwnd;
}

void LcpSrc::additive_increase(uint64_t num_bytes_acked) {
    // Per-ack increase (additive or fast).

    if (use_fi && _consecutive_good_epochs >= fi_threshold) {
        // Fast increase
        //_cwnd += num_bytes_acked;

        _cwnd += ((ai_bytes * num_bytes_acked / _cwnd) * fast_increase_round);

        if (COLLECT_DATA)
            _list_fast_increase_event.push_back(std::make_pair(eventlist().now() / 1000, 1));
    } else {
        // Additive increase
        if (cc_algorithm_type == "lcp") {
            _cwnd += (ai_bytes * num_bytes_acked / _cwnd);
            //printf("Doing Additive Increase %s - Time %f - Amound %f - Cwnd %f - Ai Bytes %f\n", _name.c_str(), timeAsUs(eventlist().now()), ai_bytes * num_bytes_acked / _cwnd, _cwnd, ai_bytes);
        } else if (cc_algorithm_type == "mprdma") {
            _cwnd += _mss * (_mss * 1.0 / _cwnd);
        } else {
            cout << "Unknown cc_algorithm_type" << endl;
            abort();
        }
    }
}

void LcpSrc::adjust_window(simtime_picosec ts, bool ecn, simtime_picosec rtt, uint32_t ackno, uint64_t num_bytes_acked, simtime_picosec acked_pkt_ts) {

    if (using_dctcp) { // FIX ME: Move this eventually, can ignore for now
        double g = 1.0 / 16.0;  // Gain factor for exponential moving average
    
        // Track packet counts
        packets_dctcp++;  
        if (ecn) {
            ecn_packets_dctcp++;  // Count ECN-marked packets
        }
    
        // Update alpha and apply window reduction once per RTT
        if (eventlist().now() - last_dctcp_alpha_update > _base_rtt) {
            alpha_dctcp = (1 - g) * alpha_dctcp + g * ((double) ecn_packets_dctcp / packets_dctcp);
            last_dctcp_alpha_update = eventlist().now();
    
            
            // Apply congestion window changes (only once per RTT)
            if (ecn_packets_dctcp > 0) {  // Reduce cwnd if ECN was seen
                _cwnd = _cwnd * (1 - alpha_dctcp / 2);
                ssthresh = _cwnd * (1 - alpha_dctcp / 2);
            } else {  // No ECN: increase cwnd
                if (_cwnd < ssthresh) {
                    _cwnd *= 2;  // Slow start: double per RTT
                } else {
                    _cwnd += _mss;  // Congestion avoidance: linear increase
                }
            }

            // Reset counters for the next RTT
            packets_dctcp = 0;
            ecn_packets_dctcp = 0;
    
            check_limits_cwnd();  // Ensure cwnd does not go below min value
        }
        return;
    }

    if (!collection_initiated) {
        // Start the timer for QA from the time that the first ACK arrives
        collection_initiated = true;
        collection_period_end = eventlist().now() + baremetal_rtt;
        bytes_acked_in_collection_period = 0;
        _list_bytes_acked_collected.push_back(std::make_pair(eventlist().now() / 1000, bytes_acked_in_collection_period));
    } else {
        if (eventlist().now() >= collection_period_end) {
            collection_period_end = eventlist().now() + baremetal_rtt;
            _list_bytes_acked_collected.push_back(std::make_pair(eventlist().now() / 1000, bytes_acked_in_collection_period));
            bytes_acked_in_collection_period = 0;
            _list_bytes_sent.push_back(std::make_tuple(eventlist().now() / 1000, _bytes_sent));
            _bytes_sent = 0;
            _list_agg_bytes_sent.push_back(std::make_tuple(eventlist().now() / 1000, _agg_bytes_sent));
        }
    }

    if (lcp_type == "mimd") {
        adjust_window_mimd(ecn, rtt, num_bytes_acked);
    } else if (lcp_type == "aimd") {
        adjust_window_aimd(ts, ecn, rtt, ackno, num_bytes_acked, acked_pkt_ts);
    } else if (lcp_type == "aimd_phantom") {
        adjust_window_aimd_phantom(ts, ecn, rtt, ackno, num_bytes_acked, acked_pkt_ts);
    }
        
    check_limits_cwnd();  
}

const string &LcpSrc::nodename() { return _nodename; }

void LcpSrc::connect(Route *routeout, Route *routeback, LcpSink &sink, simtime_picosec starttime) {
    if (_route_strategy == SINGLE_PATH || _route_strategy == ECMP_FIB || _route_strategy == ECMP_FIB_ECN ||
    _route_strategy == REACTIVE_ECN || _route_strategy == ECMP_RANDOM2_ECN || _route_strategy == ECMP_RANDOM_ECN || _route_strategy == SIMPLE_SUBFLOW || _route_strategy == PLB || _route_strategy == SCATTER_RANDOM) {
        assert(routeout);
        _route = routeout;
        // cout << "Source connect: " << _route << endl;
    }

    _sink = &sink;
    _flow.set_id(get_id()); // identify the packet flow with the NDP source
                            // that generated it
    _flow._name = _name;
    _sink->connect(*this, routeback);

    printf("StartTime %s is %lu\n", _name.c_str(), starttime); 

    eventlist().sourceIsPending(*this, starttime);
}

void LcpSrc::startflow() {
    if (!flow_started) {
        _flow_start_time = eventlist().now();
        flow_started = true;
        cout << "LcpSrc::Starting flow (ID: " << _flow.flow_id() << ", size: " << _flow_size << ") at time " << timeAsUs(_flow_start_time) << "us" << endl;
        _sink->_flow_start_time = _flow_start_time;

        if (use_erasure_coding) {
            int lul = ceil(_flow_size / (_mss * 1.0 * (parity_group_size - parity_correction_capacity))) * parity_group_size * _mss;
            double new_flow_size_double = _flow_size * parity_group_size * 1.0 / (parity_group_size - parity_correction_capacity);
            int64_t new_flow_size = ceil(new_flow_size_double);
            new_flow_size = lul;
            highest_possible_parity_id = get_parity_id(new_flow_size, _mss);
            cout << "Using erasure coding. For every " << (parity_group_size - parity_correction_capacity) << " packets, we are adding " 
                << parity_correction_capacity << " packets as parity. Thus, flow size increases from " << _flow_size << 
                "B to " << new_flow_size << "B and highest possible parity_id is " << highest_possible_parity_id << endl;
            _flow_size = new_flow_size;

            printf("New flow size %d %d\n", lul, new_flow_size);
        }

        epoch_counter = _cwnd;
        epoch_enabled = false;
        qa_enabled = false;
        qa_measurement_period_end = eventlist().now();
        epoch_end_time = 0;
        for (int i = 0; i < _total_subflows; i++) {
            _subflow_entropies.push_back(rand() % 250);
            _subflow_last_reroute.push_back(eventlist().now());
            _subflow_last_ack.push_back(eventlist().now());
        }
    }

    /* for (int i = 0; i < 5; i++) {
        window[i] = 0;
    } */

    send_packets();
}

const Route *LcpSrc::get_path() {
    if (_use_good_entropies && !_good_entropies.empty()) {
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

void LcpSrc::map_entropies() {
    for (int i = 0; i < _num_entropies; i++) {
        _entropy_array.push_back(random() % _paths.size());
    }
    printf("Printing my Paths: ");
    for (int i = 0; i < _num_entropies; i++) {
        printf("%d - ", _entropy_array[i]);
    }
    printf("\n");
}

void LcpSrc::pacedSend() {
    _paced_packet = true;
    send_packets();
}

void LcpSrc::update_next_byte_to_send() {
    if (_highest_sent >= next_byte_to_send)
        next_byte_to_send = _highest_sent + 1;
}

void LcpSrc::send_packets() {
    // cout << "rtx_pending: " << _rtx_pending << endl;
    // cout << "cwnd: " << _cwnd << endl;
    if (_rtx_pending) {
        //printf("Retransmitting packet pending\n");
        retransmit_packet();
    }
    unsigned c = _cwnd;

    while (get_unacked() + _mss <= c && _highest_sent < _flow_size) {
        // Stop sending
        if (pause_send) {
            // printf("Not sending at %lu\n", GLOBAL_TIME / 1000);
            break;
        }

        // Check pacer and set timeout
        if (!_paced_packet && use_pacing) {
            if (generic_pacer != NULL && !generic_pacer->is_pending()) {
                /* printf("scheduling send\n"); */
                // cout << eventlist().now() << ": Scheduling 2 (" << _flow.flow_id() << "), " << pacing_delay << endl;
                generic_pacer->schedule_send(pacing_delay);
                return;
            } else if (generic_pacer != NULL) {
                return;
            }
        }

        uint64_t data_seq = 0;
        UecPacket *p = UecPacket::newpkt(_flow, *_route, _highest_sent + 1, data_seq, _mss, false, _dstaddr);

        unsigned long parity_id = 0;
        if (use_erasure_coding) {
            parity_id = get_parity_id(_highest_sent + 1, _mss);
            if (parity_id < highest_completed_parity_id) {
                cout << "parity_id: " << parity_id << ", highest_completed_parity_id: " << highest_completed_parity_id << endl;
                cout << "parity id of a packet that is being newly sent should not be less than highest_completed_parity_id" << endl;
                abort();
            }
            if (ec_handler == "dst") {
                p->parity_id = parity_id;
                p->ec_wait_time = ec_wait_time;
            }
        }

        p->set_queue_idx(queue_idx);

        p->set_route(*_route);
        int crt = choose_route();
        p->subflow_number = _current_subflow;
        p->is_bts_pkt = false;


        //printf("Choose CRT %d - Path ID %d - Size path ID %d\n", crt, _path_ids[crt], _path_ids.size());

        p->set_pathid(crt);
        p->from = this->from;
        p->to = this->to;
        p->tag = this->tag;
        p->my_idx = data_count_idx++;

        p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATESEND);
        p->set_ts(eventlist().now());

        // send packet
        _highest_sent += _mss;
        update_next_byte_to_send();
        _packets_sent += _mss;
        _unacked += _mss;
        // Getting time until packet is really sent
        /* printf("Send on at %lu -- %d %d\n", GLOBAL_TIME / 1000, pause_send, stop_after_quick); */

        PacketSink *sink = p->sendOn();
        track_sending_rate();
        tracking_bytes += _mss;
        HostQueue *q = dynamic_cast<HostQueue *>(sink);
        assert(q);
        uint32_t service_time = q->serviceTime(*p);

        //printf("Flow Id %d - Sent %d of %d\n", _flow.flow_id(), p->seqno(), _flow_size);

        //printf("Sent Packet Entropy %d - Flow %d - Time %f - SeqNo %d -- Pacing Delay %lu - Cwnd %f \n", p->pathid(), p->subflow_number, timeAsUs(GLOBAL_TIME - _flow_start_time), p->seqno(), pacing_delay, _cwnd);



        _bytes_sent += p->data_packet_size();
        _agg_bytes_sent += p->data_packet_size();
        // _list_agg_bytes_sent.push_back(std::make_tuple(eventlist().now() / 1000, _agg_bytes_sent));
        _sent_packets.push_back(LcpSentPacket(eventlist().now() + service_time + _rto, p->seqno(), false, false, false, p->data_packet_size(), eventlist().now(), _current_subflow, parity_id));
        _list_rto_info.push_back(std::make_tuple(eventlist().now() / 1000, service_time/1000, _rto/1000, (service_time+_rto)/1000, _rto_margin/1000));


        if (epoch_counter > 0) {
            epoch_counter -= p->data_packet_size();
            if (epoch_counter <= 0) {
                resetEpochParams();
                epoch_enabled = true;

                if (COLLECT_DATA)
                    _list_epoch_ended.push_back(std::make_tuple(eventlist().now() / 1000, 0, 0, -1, -1, epoch_start_cwnd));

            }
        }

        // cout << "DEBUGMSGSENT: Node: " << _name << "_" << std::to_string(tag) << " Time: " << eventlist().now() / 1000000 << "  sent_packet: " << p->seqno() << endl;

        if (generic_pacer != NULL && use_pacing) {
            generic_pacer->just_sent();
            _paced_packet = false;
        }
        // if (from == 226 && to == 117) {
        // printf("Packet Sent1 from %d to %d at %lu\n", from, to, GLOBAL_TIME / 1000);
        // }
        _list_sent.push_back(std::make_pair(eventlist().now() / 1000, p->seqno()));
        _list_unacked.push_back(std::make_pair(eventlist().now() / 1000, _unacked));
    }
}

void permute_sequence_lcp(vector<int> &seq) {
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

void LcpSrc::set_paths(uint32_t no_of_paths) {

    _path_ids.resize(no_of_paths);
    permute_sequence_lcp(_path_ids);

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

void LcpSrc::set_paths(vector<const Route *> *rt_list) {
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

    // Check every path has been been intialized.
    for (size_t i = 0; i < no_of_paths; i++) {
        if (_paths[i] == NULL) {
            std::cout << "Path " << i << " not initialized" << endl;
            abort();
        }
    }
}

// void LcpSrc::apply_timeout_penalty() {
//     if (_trimming_enabled) {
//         reduce_cwnd(_mss);
//     } else {
//         reduce_cwnd(_mss);
//     }
// }

void LcpSrc::rtx_timer_hook(simtime_picosec now, simtime_picosec period) { 
    retransmit_packet();
}

void LcpSrc::track_sending_rate() {
    if (eventlist().now() > last_track_ts + tracking_period) {
        double rate = (double)(tracking_bytes * 8.0 / ((eventlist().now() - last_track_ts) / 1000));
        list_sending_rate.push_back(std::make_pair(eventlist().now() / 1000, rate));
        tracking_bytes = 0;
        last_track_ts = eventlist().now();
    }
}

void LcpSrc::track_ecn_rate() {}

bool LcpSrc::resend_packet(std::size_t idx) {
    if (get_unacked() >= _cwnd || (pause_send)) {
        // printf("Not sending at %lu\n", GLOBAL_TIME / 1000);
        // std::cout << "Not sending at " << eventlist().now() << endl;
        // std::cout << "unacked: " << get_unacked() << endl;
        // std::cout << "cwnd: " << _cwnd << endl;
        return false;
    }

    // Check pacer and set timeout
    if (!_paced_packet && use_pacing) {
        if (generic_pacer != NULL && !generic_pacer->is_pending()) {
            // cout << eventlist().now() << ": Scheduling 3 (" << _flow.flow_id() << ")" << endl;
            generic_pacer->schedule_send(pacing_delay);
            return false;
        } else if (generic_pacer != NULL) {
            return false;
        }
    }

    assert(!_sent_packets[idx].acked);

    // this will cause retransmission not only of the offending
    // packet, but others close to timeout
    // _rto_margin = _base_rtt / 2;

    _unacked += _mss;
    UecPacket *p = UecPacket::newpkt(_flow, *_route, _sent_packets[idx].seqno, 0, _mss, true, _dstaddr);
    p->set_ts(eventlist().now());
    p->is_bts_pkt = false;

    if (use_erasure_coding && ec_handler == "dst") {
            p->parity_id = _sent_packets[idx].parity_id;
            p->ec_wait_time = target_rtt;
    }
    
    p->set_queue_idx(queue_idx);

    p->set_route(*_route);
    int crt = choose_route();
    p->from = this->from;
    p->to = this->to;
    p->tag = this->tag;
    p->subflow_number = _current_subflow;

    //printf("Resending on subflow %d at time %f\n", crt, timeAsUs(eventlist().now()));
    number_retx += 1;

    p->set_pathid(crt);

    //printf("Sent Rtx Packet Entropy %d - Flow %d - Time %f - SeqNo %d \n", p->pathid(), p->subflow_number, timeAsUs(GLOBAL_TIME - _flow_start_time), p->seqno());


    p->flow().logTraffic(*p, *this, TrafficLogger::PKT_CREATE);
    /* printf("Send on at %lu -- %d %d\n", GLOBAL_TIME / 1000, pause_send, stop_after_quick); */
    PacketSink *sink = p->sendOn();
    track_sending_rate();
    tracking_bytes += _mss;
    HostQueue *q = dynamic_cast<HostQueue *>(sink);
    assert(q);
    uint32_t service_time = q->serviceTime(*p);
    if (_sent_packets[idx].nacked) {
        --_nack_rtx_pending;
        _sent_packets[idx].nacked = false;
    }
    _sent_packets[idx].timer = eventlist().now() + service_time + _rto;
    _sent_packets[idx].sent_time = eventlist().now();
    _list_rto_info.push_back(std::make_tuple(eventlist().now() / 1000, service_time/1000, _rto/1000, (service_time+_rto)/1000, _rto_margin/1000));
    _sent_packets[idx].timedOut = false;
    if (generic_pacer != NULL) {
        generic_pacer->just_sent();
        _paced_packet = false;
    }
    
    _list_retrans.push_back(std::make_pair(eventlist().now() / 1000, _sent_packets[idx].seqno));
    _list_unacked.push_back(std::make_pair(eventlist().now() / 1000, _unacked));
    _bytes_sent += p->data_packet_size();
    _agg_bytes_sent += p->data_packet_size();

    if (epoch_counter > 0) {
        epoch_counter -= p->data_packet_size();
        if (epoch_counter <= 0) {
            resetEpochParams();
            epoch_enabled = true;

            if (COLLECT_DATA)
                _list_epoch_ended.push_back(std::make_tuple(eventlist().now() / 1000, 0, 0, -1, -1, epoch_start_cwnd));
        }
    }

    // cout << "DEBUGMSGRETRANS: Node: " << _name << "_" << std::to_string(tag) << " Time: " << eventlist().now() / 1000000 << "  retrans_packet: " << p->seqno() << endl;
    return true;
}

// retransmission for timeout
void LcpSrc::retransmit_packet() {
    //printf("Do we ever enter here?\n");
    //_list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, aggregate_timedout, aggregate_retrans));
    _rtx_pending = false;
    uint64_t cwnd_reduction = 0;
    for (std::size_t i = 0; i < _sent_packets.size(); ++i) {
        auto &sp = _sent_packets[i];

        bool is_parity_id_acked = false; // by default the parity id is not acked
        if (use_erasure_coding) {
            is_parity_id_acked = (sp.parity_id <= highest_completed_parity_id);
            if (sp.parity_id > highest_completed_parity_id) {
                if (ec_handler == "src") {
                    auto parity_id_found_itr = rcvd_parity_ids_map.find(sp.parity_id);
                    if (parity_id_found_itr == rcvd_parity_ids_map.end()) {
                        cout << "Since this parity id is not acked, the parity id MUST exist in rcvd_parity_ids_map" << endl;
                        abort();
                    }
                    is_parity_id_acked = (parity_id_found_itr->second >= (parity_group_size - parity_correction_capacity));
                } else if (ec_handler == "dst") {
                    set<unsigned long>::iterator parity_completed_itr = completed_parity_set.find(sp.parity_id);
                    is_parity_id_acked = (parity_completed_itr != completed_parity_set.end());
                }
            }
        }

        if (!sp.timedOut && !sp.acked && !sp.nacked && sp.timer <= eventlist().now() + _rto_margin) {
            sp.timedOut = true;

            // TO DO
            /* if (_subflow_reroute) {
                int old_entropy = sp.entropy_used;
                int new_entropy = -1;

                if ((eventlist().now() - 0) - _subflow_last_reroute[old_entropy] > _base_rtt) {
                    simtime_picosec last_re_route = _subflow_last_reroute[old_entropy];
                    _subflow_last_reroute[old_entropy] = eventlist().now() - 0;
                    if (_subflow_adaptive_reroute) {
                        // Find iterator to the maximum element
                        auto max_it = std::max_element(_subflow_last_ack.begin(), _subflow_last_ack.end());

                        if (max_it != _subflow_last_ack.end()) {
                            int index = std::distance(_subflow_last_ack.begin(), max_it);
                            _subflow_entropies[old_entropy] = _subflow_entropies[index];
                            //current_entropy = old_entropy - 1;
                            new_entropy = index;
                            //_fixed_route = _subflow_entropies[old_entropy];
                            printf("%s Changing Entropy at1 %f from %d to %d (%d) - Last ReRoute Time %f, Base %f - Last ACK Received at %f on %d\n", _name.c_str(), timeAsUs(eventlist().now() - _flow_start_time), old_entropy, _subflow_entropies[old_entropy], new_entropy, timeAsUs(last_re_route), timeAsUs(_base_rtt), timeAsUs(_subflow_last_ack[index]), index);
                            for (int i = 0; i < _subflow_last_ack.size(); i++) {
                                if ((eventlist().now() - 0) - _subflow_last_ack[index] > _rto) {
                                    _subflow_entropies[old_entropy] = rand() % 250;
                                    printf("Random Re-Route\n\n");
                                }
                            }
                        } else {
                            std::cout << "The vector is empty!" << std::endl;
                        }

                    } else {
                        _subflow_entropies[old_entropy] = rand() % 250;
                        printf("%s Changing Entropy at2 %f from %d to %d (%d) - Last ReRoute Time %f, Base %f \n", _name.c_str(), timeAsUs(eventlist().now()), old_entropy, _subflow_entropies[old_entropy], new_entropy, timeAsUs(last_re_route), timeAsUs(_base_rtt));

                    }

                    
                }
            } */


            if (_subflow_reroute) {
        
                for (int i = 0; i < _subflow_last_ack.size(); i++) {
                    if ((eventlist().now()) - _subflow_last_reroute[i] > _base_rtt && 
                        (eventlist().now()) - _subflow_last_ack[i] > _base_rtt * 0.5) {
                        _subflow_last_reroute[i] = eventlist().now();
                        _subflow_entropies[i] = rand() % 250;
                    } 
                }                         
            }

            if (_route_strategy == PLB) {
                if (eventlist().now() > plb_timeout_wait) {
                    plb_timeout_wait = eventlist().now() + _rto;
                    plb_congested_rounds = 0;
                    plb_entropy = rand() % 250;
                }
            }
                     
            // only count when the timedout packet has the potential to be retransmitted
            if (!is_parity_id_acked) {
                aggregate_timedout += sp.size;
                _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, aggregate_timedout, aggregate_retrans));
                _consecutive_good_epochs = 0;
                fast_increase_round = 0;
            }
            if (use_rto_cwnd_reduction) {
                _cwnd -= sp.size;
                cwnd_reduction += sp.size;
            }
            check_limits_cwnd();
            reduce_unacked(sp.size);
        }
        if (!sp.acked && (sp.timedOut || sp.nacked)) {
            if (use_erasure_coding && is_parity_id_acked) {
                // already timedout or nacked and is never gonna be resent so consider it as acked
                sp.acked = true;
            } else {
              if (!resend_packet(i)) {
                _rtx_pending = true;
              } else {
                aggregate_retrans += sp.size;
                _list_timeout.push_back(std::make_tuple(eventlist().now() / 1000, aggregate_timedout, aggregate_retrans));
              }
            }
        }
    }

    if (use_erasure_coding) {
        // the reason that we are also counting acked packets is that we might remove the first timed out packet and the next one is actually acked
        // while (!_sent_packets.empty() && 
        //     (_sent_packets[0].acked || (_sent_packets[0].parity_id <= highest_completed_parity_id && 
        //     (_sent_packets[0].timedOut || _sent_packets[0].nacked)))) {
        //     _sent_packets.erase(_sent_packets.begin());
        // }
        while (!_sent_packets.empty() && _sent_packets[0].acked) {
            _sent_packets.erase(_sent_packets.begin());
        }
    }

    if (COLLECT_DATA && cwnd_reduction > 0) {
        // std::cout << eventlist().now() << ": " << cwnd_reduction << endl;
        _list_retransmit_cwnd_reduce.push_back(make_tuple(eventlist().now() / 1000, cwnd_reduction));
    }
}

/**********
 * LcpSink *
 **********/

LcpSink::LcpSink() : DataReceiver("sink"), _cumulative_ack{0}, _drops{0} { _nodename = "LcpSink"; }

LcpSink::~LcpSink() {
    // If we are collecting specific logs
    if (COLLECT_DATA) {
        std::cout << "Callecting data!" << endl;

        // bytes sent
        std::string file_name = PROJECT_ROOT_PATH / ("output/bytes_rcvd/bytes_rcvd" + _name + "_" + std::to_string(tag) + ".txt");
        std::ofstream MyFileBytesRcvd(file_name, std::ios_base::app);
        for (const auto &p : _list_bytes_rcvd) {
            MyFileBytesRcvd << get<0>(p) << "," << get<1>(p) << std::endl;
        }
        MyFileBytesRcvd.close();   
    }
}

void LcpSink::set_end_trigger(Trigger &end_trigger) { _end_trigger = &end_trigger; }

void LcpSink::send_nack(simtime_picosec ts, bool marked, UecAck::seq_t seqno, UecAck::seq_t ackno, const Route *rt,
                        int path_id, bool is_failed) {

    UecNack *nack = UecNack::newpkt(_src->_flow, *_route, seqno, ackno, 0, _srcaddr);
    nack->is_failed = is_failed;
    nack->from = this->from;
    nack->to = this->to;
    nack->tag = this->tag;

    nack->set_queue_idx(queue_idx);

    // printf("Sending NACK at %lu\n", GLOBAL_TIME);
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

bool LcpSink::already_received(UecPacket &pkt) {
    UecPacket::seq_t seqno = pkt.seqno();

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

void LcpSink::send_parity_nack(const Route *rt, unsigned long nack_parity_id, unsigned long highest_completed_parity_id) {

    UecNack *nack = UecNack::newpkt(_src->_flow, *_route, 0, 0, 0, _srcaddr);
    nack->is_failed = false;
    nack->from = this->from;
    nack->to = this->to;
    nack->tag = this->tag;

    nack->set_queue_idx(queue_idx);

    // printf("Sending NACK at %lu\n", GLOBAL_TIME);
    nack->set_pathid(_path_ids[_crt_path]);
    _crt_path++;
    if (_crt_path == _paths.size()) {
    _crt_path = 0;
    }

    nack->pathid_echo = 0;
    nack->is_ack = false;
    nack->set_flags(0);

    nack->nack_parity_id = nack_parity_id;
    nack->highest_completed_parity_id = highest_completed_parity_id;

    nack->sendOn();
}

void LcpSink::parity_timer_hook(simtime_picosec now, simtime_picosec period) { 
    // send nack for timed out parities
    for (std::map<unsigned long, PacketSink::Batch *>::iterator it = rcvd_batches_map.begin(); it != rcvd_batches_map.end(); it++) {
        Batch* batch = it->second;
        if (batch->parity_exp_time > 0 && 
            _src->eventlist().now() >= batch->parity_exp_time && 
            batch->parity_cnt < (parity_group_size - parity_correction_capacity)) {
                // parity is expired
                // batch->parity_exp_time = _src->eventlist().now() + batch->ec_wait_time;
                // deactivating timer. It gets reactivated when we get a new packet
                batch->parity_exp_time = 0;

                // TODO: consider different ways to select paths
                auto crt_path = random() % _paths.size();
                send_parity_nack(_paths.at(crt_path), batch->parity_id, highest_completed_parity_id);
        }
    }
}

void LcpSink::receivePacket(Packet &pkt) {
    /* printf("Sink Received %d %d - Entropy %d - %lu - \n", pkt.from, pkt.id(), pkt.pathid(), GLOBAL_TIME / 1000); */
    if (pkt.pfc_just_happened) {
        pfc_just_seen = 1;
    } else {
        pfc_just_seen = 0;
    }

    if (use_erasure_coding && ec_handler == "dst") {
        if (parity_group_size <= 0) {
            cout << "parity_group_size should be > 0" << endl;
            abort();
        }
    }



    switch (pkt.type()) {
    case UECACK:
    case UECNACK:
        // bounced, ignore
        pkt.free();
        return;
    case UEC:
        // do what comes after the switch
        if (pkt.bounced()) {
            printf("Bounced at Sink, no sense\n");
        }

        break;
    default:
        std::cout << "unknown packet receive with type code: " << pkt.type() << "\n";
        pkt.free();

        return;
    }
    
    UecPacket *p = dynamic_cast<UecPacket *>(&pkt);
    UecPacket::seq_t seqno = p->seqno();
    UecPacket::seq_t ackno = p->seqno() + p->data_packet_size() - 1;
    simtime_picosec ts = p->ts();

    //printf("Flow Id %d - SinkReceived %d of %d\n",0 , p->seqno(), 0);

    //printf("Received Packet Entropy %d - Flow %d - Time %f \n", p->pathid(), p->subflow_number, timeAsUs(GLOBAL_TIME));


    bool marked = p->flags() & ECN_CE;

    //printf("Received Packet SeQNo %d at %f - SubFlow %d Entropy %d\n", p->seqno(), timeAsUs(GLOBAL_TIME - _flow_start_time), p->subflow_number, p->pathid());

    // TODO: consider different ways to select paths
    auto crt_path = random() % _paths.size();

    // packet was trimmed
    if (pkt.header_only() && pkt._is_trim) {
        if (use_trimming) {        
            // cout << "LCP's current version should not activate trimming" << endl;
            if (use_erasure_coding) {
                cout << "send_nack is not yet supported for erasure coding!" << endl;
                abort();
            }
            send_nack(ts, marked, seqno, ackno, _paths.at(crt_path), pkt.pathid(), pkt.is_failed);
        }
        pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_RCVDESTROY);
        p->free();
        // printf("NACKR %d@%d@%d - Time %lu\n", from, to, tag,
        //        GLOBAL_TIME / 1000);
        return;
    }

    unsigned long parity_id = 0;
    simtime_picosec ec_wait_time;
    UecPacket::seq_t pkt_seqno;
    if (use_erasure_coding && ec_handler == "dst") {
        parity_id = p->parity_id;
        ec_wait_time = p->ec_wait_time;
        pkt_seqno = p->seqno();
        if (parity_id <= 0 || ec_wait_time <= 0) {
            cout << "parity_id <= 0 || ec_wait_time <= 0!!" << endl;
            abort();
        }
    }
    
    int size = p->data_packet_size();
    p->free();

    bytes_rcvd += size;

    if (!collection_initiated) {
        // Start the timer for QA from the time that the first ACK arrives
        collection_initiated = true;
        collection_period_end = _src->eventlist().now() + _src->baremetal_rtt;
        bytes_rcvd = 0;
    } else {
        if (_src->eventlist().now() >= collection_period_end) {
            collection_period_end = _src->eventlist().now() + _src->baremetal_rtt;
            _list_bytes_rcvd.push_back(std::make_tuple(_src->eventlist().now() / 1000, bytes_rcvd));
            bytes_rcvd = 0;
        }
    }

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
    // quick way of doing that in htsim printf("Ack Sending
    // From %d - %d\n", this->from,
    int32_t path_id = p->pathid();
    int subflow_id = p->subflow_number;
    /* printf("NORMALACK %d@%d@%d - Time %lu\n", from, to, tag,
           GLOBAL_TIME / 1000); */

    unsigned long completed_batch_parity_id = 0;    // if the current batch is completed, send it through the ACK
    if (parity_id != 0) {
        // we should take care of parity_id in the receiver
        if (parity_id > highest_completed_parity_id) {
            map<unsigned long, Batch*>::iterator batch_found_itr = rcvd_batches_map.find(parity_id);
            if (batch_found_itr == rcvd_batches_map.end()) {
                rcvd_batches_map.insert(make_pair(parity_id, new Batch(parity_id)));
                set<UecPacket::seq_t> seqno_set;
                batch_found_itr = rcvd_batches_map.find(parity_id);
                batch_found_itr->second->parity_exp_time = _src->eventlist().now() + ec_wait_time;
                if (batch_found_itr->second->parity_exp_time == 0) {
                    cout << "Why is nack timer initiated to 0!" << endl;
                    abort();
                }
                batch_found_itr->second->ec_wait_time = ec_wait_time;
            }

            if (batch_found_itr->second->parity_exp_time == 0) {
                // the nack has been sent previously and now we got a packet for this, re-initiate the the timer
                batch_found_itr->second->parity_exp_time = _src->eventlist().now() + batch_found_itr->second->ec_wait_time;
            }

            Batch* batch = batch_found_itr->second;
            set<UecPacket::seq_t>::iterator rcvd_seqno_itr = batch->rcvd_seqno_set.find(pkt_seqno);

            if (rcvd_seqno_itr == batch->rcvd_seqno_set.end()) {
                // new packet's being acked

                batch->parity_cnt++;
                batch->rcvd_seqno_set.insert(pkt_seqno);

                if (batch->parity_cnt > parity_group_size) {
                    cout << "How is the number of packets acked with parity id of " << batch->parity_cnt <<
                        " more than parity_group_size" << endl;
                    abort();
                }
            }

            if (batch->parity_cnt >= (parity_group_size - parity_correction_capacity))
                completed_batch_parity_id = batch->parity_id;

            // update highest_completed_parity_id
            batch_found_itr = rcvd_batches_map.find(highest_completed_parity_id + 1);
            while (batch_found_itr != rcvd_batches_map.end() && batch_found_itr->second->parity_cnt >= (parity_group_size - parity_correction_capacity)) {
                delete batch_found_itr->second;
                rcvd_batches_map.erase(batch_found_itr);
                highest_completed_parity_id++;
                batch_found_itr = rcvd_batches_map.find(highest_completed_parity_id + 1);
            }
        }
    }
    
    // The seqno and ack represent the first and last bytes that are acked by this packet
    // if we observe commulative ack, seqno is set to 1
    send_ack(ts, marked, seqno, ackno, _paths.at(crt_path), pkt.get_route(), path_id, completed_batch_parity_id, highest_completed_parity_id, subflow_id);
}

void LcpSink::send_ack(simtime_picosec ts, bool marked, UecAck::seq_t seqno, UecAck::seq_t ackno, const Route *rt,
                       const Route *inRoute, int path_id, unsigned long completed_batch_parity_id, unsigned long highest_completed_parity_id, int subflow_id) {

    UecAck *ack = 0;

    switch (_route_strategy) {
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
    case ECMP_RANDOM2_ECN:
    case SCATTER_RANDOM:
    case SIMPLE_SUBFLOW:
    case PLB:
    case ECMP_RANDOM_ECN:
        ack = UecAck::newpkt(_src->_flow, *_route, seqno, ackno, 0, _srcaddr);

        ack->set_pathid(_path_ids[_crt_path]);
        _crt_path++;
        if (_crt_path == _paths.size()) {
            _crt_path = 0;
        }
        ack->inc_id++;
        ack->my_idx = ack_count_idx++;

        // set ECN echo only if that is selected strategy
        if (marked) {
            ack->set_flags(ECN_ECHO);
        } else {
            ack->set_flags(0);
        }

        break;
    case SINGLE_PATH:
        ack = UecAck::newpkt(_src->_flow, *_route, seqno, ackno, 0, _srcaddr);
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
    //printf("Received Data with EV %d\n", path_id);
    ack->pfc_just_happened = false;
    if (pfc_just_seen == 1) {
        ack->pfc_just_happened = true;
    }

    if (use_erasure_coding && ec_handler == "dst") {
        ack->highest_completed_parity_id = highest_completed_parity_id;
        ack->completed_batch_parity_id = completed_batch_parity_id;
    }

    // ack->inf = inRoute;
    ack->is_ack = true;
    ack->flow().logTraffic(*ack, *this, TrafficLogger::PKT_CREATE);
    ack->set_ts(ts);

    ack->set_queue_idx(queue_idx);

    // printf("Setting TS to %lu at %lu\n", ts / 1000, GLOBAL_TIME / 1000);
    ack->from = this->from;
    ack->to = this->to;
    ack->tag = this->tag;
    ack->subflow_number = subflow_id;

    ack->sendOn();
}

const string &LcpSink::nodename() { return _nodename; }

uint64_t LcpSink::cumulative_ack() { return _cumulative_ack; }

uint32_t LcpSink::drops() { return _drops; }

void LcpSink::connect(LcpSrc &src, const Route *route) {
    _src = &src;
    switch (_route_strategy) {
    case SINGLE_PATH:
    case ECMP_FIB:
    case ECMP_FIB_ECN:
    case REACTIVE_ECN:
    case ECMP_RANDOM2_ECN:
    case SCATTER_RANDOM:
    case ECMP_RANDOM_ECN:
    case PLB:
    case SIMPLE_SUBFLOW:
        assert(route);
        _route = route;
        // cout << "Setting route: " << route << endl;
        break;
    default:
        // do nothing we shouldn't be using this route - call
        // set_paths() to set routing information
        _route = NULL;
        break;
    }

    _cumulative_ack = 0;
    _drops = 0;
}

void LcpSink::set_paths(uint32_t no_of_paths) {
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
    case PLB:
    case SIMPLE_SUBFLOW:
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
 * LcpRtxTimerScanner *
 **********************/

LcpRtxTimerScanner::LcpRtxTimerScanner(simtime_picosec scanPeriod, EventList &eventlist)
        : EventSource(eventlist, "RtxScanner"), _scanPeriod{scanPeriod} {
    eventlist.sourceIsPendingRel(*this, 0);
}

void LcpRtxTimerScanner::registerLcp(LcpSrc &LcpSrc) {
    _lcps.push_back(&LcpSrc);
}

void LcpRtxTimerScanner::doNextEvent() {
    // cout << "LcpRtxTimerScanner::doNextEvent" << endl;
    simtime_picosec now = eventlist().now();
    lcps_t::iterator i;
    for (i = _lcps.begin(); i != _lcps.end(); i++) {
        if (!((*i)->_flow_finished))
            (*i)->rtx_timer_hook(now, _scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}

/**********************
 * LcpParityTimerScanner *
 **********************/

 LcpParityTimerScanner::LcpParityTimerScanner(simtime_picosec scanPeriod, EventList &eventlist)
        : EventSource(eventlist, "ParityScanner"), _scanPeriod{scanPeriod} {
    eventlist.sourceIsPendingRel(*this, 0);
}

void LcpParityTimerScanner::registerLcp(LcpSink &LcpSink) {
    _lcps.push_back(&LcpSink);
}

void LcpParityTimerScanner::doNextEvent() {
    // cout << "LcpParityTimerScanner::doNextEvent" << endl;
    simtime_picosec now = eventlist().now();
    lcps_t::iterator i;
    for (i = _lcps.begin(); i != _lcps.end(); i++) {
        if (!((*i)->_src->_flow_finished))
            (*i)->parity_timer_hook(now, _scanPeriod);
    }
    eventlist().sourceIsPendingRel(*this, _scanPeriod);
}

