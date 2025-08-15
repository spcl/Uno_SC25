// -*- c-basic-offset: 4; indent-tabs-mode: nil -*-
#include "compositequeue.h"
#include "ecn.h"
#include "uecpacket.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <math.h>
#include <regex>
#include <sstream>
#include <stdio.h>
#include <utility>
#include <random>
#include <chrono>

bool CompositeQueue::_drop_when_full = true;
bool CompositeQueue::_use_mixed = false;
bool CompositeQueue::_use_phantom = false;
bool CompositeQueue::_use_both_queues = false;
int CompositeQueue::_phantom_queue_size = 0;
bool CompositeQueue::_phantom_in_series = false;
int CompositeQueue::_phantom_queue_slowdown = 10;
bool CompositeQueue::use_bts = false;

int CompositeQueue::force_kmin = -1;
int CompositeQueue::force_kmax = -1;

bool CompositeQueue::_phantom_observe = false;
bool CompositeQueue::failure_multiples = false;

int CompositeQueue::_phantom_kmin = 20;
int CompositeQueue::_phantom_kmax = 80;

bool CompositeQueue::_use_both_queues_true = false;

CompositeQueue::CompositeQueue(linkspeed_bps bitrate, mem_b maxsize, EventList &eventlist, QueueLogger *logger)
        : Queue(bitrate, maxsize, eventlist, logger) {
    _ratio_high = 20;
    _ratio_low = 1;
    _crt = 0;
    _num_headers = 0;
    _num_packets = 0;
    _num_acks = 0;
    _num_nacks = 0;
    _num_pulls = 0;
    _num_drops = 0;
    _num_stripped = 0;
    _num_bounced = 0;
    _ecn_minthresh = maxsize * 2; // don't set ECN by default
    _ecn_maxthresh = maxsize * 2; // don't set ECN by default

    double temp = (static_cast<double>(4096 + 64) * 8.0) / static_cast<double>(LINK_SPEED_MODERN);
    temp += (temp * static_cast<double>(_phantom_queue_slowdown) / 100.0);
    temp *= 1000.0;

    _draining_time_phantom = static_cast<long int>(temp); // Convert to integer at the very end

    //printf("Draining Time Phantom %lu\n", _draining_time_phantom);

    _queuesize_high = _current_queuesize_phatom = 0;
    reset_queuesize_low();

    _serv = QUEUE_INVALID;
    stringstream ss;
    ss << "compqueue(" << bitrate / 1000000 << "Mb/s," << maxsize << "bytes)";
    _nodename = ss.str();
    /* printf("Created Queue with following params: %s - %d - %d - %d - %d - %d - %lu %lu\n", _nodename.c_str(),
           _use_mixed, _use_phantom, _use_both_queues, _phantom_queue_size, _phantom_in_series, maxsize, bitrate); */
}

void CompositeQueue::decreasePhantom() {
    
        
    _current_queuesize_phatom -= (4096 + 64); // parameterize
    if (_current_queuesize_phatom < 0) {
        _current_queuesize_phatom = 0;
    }

    _decrease_phantom_next = eventlist().now() + _draining_time_phantom;
    eventlist().sourceIsPendingRel(*this, _draining_time_phantom);
        
    

    if (FLOW_ACTIVE == 0) {
        eventlist().cancelPendingSource(*this);
    }
}

void CompositeQueue::beginService() {
    if (!_enqueued_high.empty() && !all_enqueued_low_queues_empty()) {
        _crt++;

        if (_crt >= (_ratio_high + _ratio_low))
            _crt = 0;

        if (_crt < _ratio_high) {
            _serv = QUEUE_HIGH;
            eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
            _decrease_normal_next = eventlist().now() + drainTime(_enqueued_high.back());
            //printf("Time TO Drain1 is %lu\n", drainTime(_enqueued_high.back()));
        } else {
            assert(_crt < _ratio_high + _ratio_low);
            _serv = QUEUE_LOW;
            eventlist().sourceIsPendingRel(*this, get_enqueue_low_min_drain_time());
            _decrease_normal_next = eventlist().now() + get_enqueue_low_min_drain_time();
            //printf("Time TO Drain2 is %lu\n", get_enqueue_low_min_drain_time());
        }
        return;
    }

    if (!_enqueued_high.empty()) {
        _serv = QUEUE_HIGH;
        eventlist().sourceIsPendingRel(*this, drainTime(_enqueued_high.back()));
        _decrease_normal_next = eventlist().now() + drainTime(_enqueued_high.back());
        //printf("Time TO Drain3 is %lu\n", drainTime(_enqueued_high.back()));
    } else if (!all_enqueued_low_queues_empty()) {
        _serv = QUEUE_LOW;
        _decrease_normal_next =
                eventlist().now() + get_enqueue_low_min_drain_time(); 
        eventlist().sourceIsPendingRel(*this, get_enqueue_low_min_drain_time());
        //printf("Time TO Drain4 is %lu\n", get_enqueue_low_min_drain_time());
    } else {
        assert(0);
        _serv = QUEUE_INVALID;
    }
}

bool CompositeQueue::decide_ECN(int queue_idx) {

    // ECN mark on deque
    if (_use_both_queues && _use_phantom && !_phantom_observe) {

        if (!_enqueued_low_list[1].empty()) {
            cout << "Multiple _enqueued_low is not configured to work with _use_both_queues && _use_phantom, _enqueued_low_list[1] must always stay empty!" << endl;
            abort();
        }

        // printf("Using both Queues - Size Real %lu\n", _maxsize);

        bool real_queue_ecn = false;
        _ecn_maxthresh = _maxsize / 100 * 80;
        _ecn_minthresh = _maxsize / 100 * 20;

        if (_queuesize_low[0] > _ecn_minthresh) {
            /* printf("Using both Queues1 %s - Size Real %lu %lu - ECN %lu %lu\n",_nodename.c_str(),  _maxsize,
             * _queuesize_low, _ecn_minthresh, _ecn_maxthresh); */

            uint64_t p = (0x7FFFFFFF * (_queuesize_low[0] - _ecn_minthresh)) / (_ecn_maxthresh - _ecn_minthresh);

            if ((uint64_t)random() < p) {
                real_queue_ecn = true;
                /* printf("Using both Queues2 - Size Real %lu\n", _maxsize); */
            }
        }

        int _ecn_maxthresh_ph = _phantom_queue_size / 100 * _phantom_kmax;
        int _ecn_minthresh_ph = _phantom_queue_size / 100 * _phantom_kmin;
        if (_current_queuesize_phatom > _ecn_maxthresh_ph) {
            return true;
        } else if (_current_queuesize_phatom > _ecn_minthresh_ph) {
            uint64_t p =
                    (0x7FFFFFFF * (_current_queuesize_phatom - _ecn_minthresh_ph)) / (_ecn_maxthresh_ph - _ecn_minthresh_ph);
            if (((uint64_t)random() < p) || real_queue_ecn) {
                return true;
            }
        }
        return false;

    } else if (_use_both_queues_true && _use_phantom && !_phantom_observe) {

        if (!_enqueued_low_list[1].empty()) {
            cout << "Multiple _enqueued_low is not configured to work with _use_both_queues && _use_phantom, _enqueued_low_list[1] must always stay empty!" << endl;
            abort();
        }

        // printf("Using both Queues - Size Real %lu\n", _maxsize);

        bool real_queue_ecn = false;
        _ecn_maxthresh = _maxsize / 100 * _kmax_from_input;
        _ecn_minthresh = _maxsize / 100 * _kmin_from_input;

        if (_queuesize_low[0] > _ecn_minthresh) {
            /* printf("Using both Queues1 %s - Size Real %lu %lu - ECN %lu %lu\n",_nodename.c_str(),  _maxsize,
             * _queuesize_low, _ecn_minthresh, _ecn_maxthresh); */

            uint64_t p = (0x7FFFFFFF * (_queuesize_low[0] - _ecn_minthresh)) / (_ecn_maxthresh - _ecn_minthresh);

            if ((uint64_t)random() < p) {
                real_queue_ecn = true;
                /* printf("Using both Queues2 - Size Real %lu\n", _maxsize); */
            }
        }

        int _ecn_maxthresh_ph = _phantom_queue_size / 100 * _phantom_kmax;
        int _ecn_minthresh_ph = _phantom_queue_size / 100 * _phantom_kmin;
        if (_current_queuesize_phatom > _ecn_maxthresh_ph && real_queue_ecn) {
            return true;
        } else if (_current_queuesize_phatom > _ecn_minthresh_ph) {
            uint64_t p =
                    (0x7FFFFFFF * (_current_queuesize_phatom - _ecn_minthresh_ph)) / (_ecn_maxthresh_ph - _ecn_minthresh_ph);
            if (((uint64_t)random() < p) && real_queue_ecn) {
                return true;
            }
        }
        return false;

    } else if (_use_phantom && !_phantom_observe) {
        int _ecn_maxthresh_ph = _phantom_queue_size / 100 * _phantom_kmax;
        int _ecn_minthresh_ph = _phantom_queue_size / 100 * _phantom_kmin;

        //printf("Phantom Queue Size %d - %d %d \n", _current_queuesize_phatom, _phantom_queue_size, _ecn_maxthresh);

        if (_current_queuesize_phatom > _ecn_maxthresh_ph) {
            return true;
        } else if (_current_queuesize_phatom > _ecn_minthresh_ph) {
            uint64_t p =
                    (0x7FFFFFFF * (_current_queuesize_phatom - _ecn_minthresh_ph)) / (_ecn_maxthresh_ph - _ecn_minthresh_ph);
            if ((uint64_t)random() < p) {
                return true;
            }
        }
        return false;
    } else {
        if (queue_idx < 0) {
            cout << "How is queue_idx negative in decideECN?" << endl;
            abort();
        }
            
        /* printf("Using both Queues1 %s - Time %lu - Size Real %lu %lu - ECN %lu %lu\n",_nodename.c_str(), GLOBAL_TIME
         * / 1000, _maxsize, _queuesize_low, _ecn_minthresh, _ecn_maxthresh); */
        _ecn_maxthresh = _maxsize / 100 * _kmax_from_input;
        _ecn_minthresh = _maxsize / 100 * _kmin_from_input;

        //printf("ECN1 KMin %d - KMax %d\n", _ecn_minthresh, _ecn_maxthresh);

        if (force_kmin != -1) {
            _ecn_minthresh = force_kmin;
            _ecn_maxthresh = force_kmax;
        }
//printf("ECN2 KMin %d - KMax %d -- %d\n", _ecn_minthresh, _ecn_maxthresh, _queuesize_low[queue_idx]);

        if (_queuesize_low[queue_idx] > _ecn_maxthresh) {
            // cout << eventlist().now() << ": qsize: " << _queuesize_low << ", maxthresh: " << _ecn_maxthresh << endl;
            //printf("ECN TRUE1\n");  
            return true;
        } else if (_queuesize_low[queue_idx] > _ecn_minthresh) {
            uint64_t p = (0x7FFFFFFF * (_queuesize_low[queue_idx] - _ecn_minthresh)) / (_ecn_maxthresh - _ecn_minthresh);
            if ((uint64_t)random() < p) {
                // cout << eventlist().now() << ": qsize: " << _queuesize_low << ", minthresh: " << _ecn_minthresh << endl;
                //printf("ECN TRUE2\n");  
                return true;
            }
        }
        return false;
    }
}

Packet* CompositeQueue::popLowQueue() {
    check_low_queue_sizes();
    Packet* pkt;
    if (_enqueued_low_list[0].empty()) {
        pkt = _enqueued_low_list[1].pop();
        _queuesize_low[1] -= pkt->size();
        return pkt;
    }
    if (_enqueued_low_list[1].empty()) {
        pkt = _enqueued_low_list[0].pop();
        _queuesize_low[0] -= pkt->size();
        return pkt;
    } else {
        if (weight_cnt < _enqueued_low_list[0].weight) {
            pkt = _enqueued_low_list[0].pop();
            _queuesize_low[0] -= pkt->size();
        } else {
            pkt = _enqueued_low_list[1].pop();
            _queuesize_low[1] -= pkt->size();
        }
        uint64_t max_weight = _enqueued_low_list[0].weight + _enqueued_low_list[1].weight;
        weight_cnt++;
        weight_cnt %= max_weight;
    }
    return pkt;
}

void CompositeQueue::completeService() {
    Packet *pkt;
    if (_serv == QUEUE_LOW) {

        if (_use_mixed) {
            if (!_enqueued_low_list[1].empty()) {
                cout << "_use_mixed is not yet deployed to work with two _enqueued_lows and _enqueued_low_list[1] must always stay empty!" << endl;
                abort();
            }
            std::vector<Packet *> new_queue;
            std::vector<Packet *> temp_queue = _enqueued_low_list[0]._queue;
            int count_good = 0;
            int dropped_p = 0;
            int initial_count = _enqueued_low_list[0]._count;
            for (int i = 0; i < _enqueued_low_list[0]._count; i++) {
                int64_t diff = GLOBAL_TIME - temp_queue[_enqueued_low_list[0]._next_pop + i]->enter_timestamp;
                int64_t diff_budget = temp_queue[_enqueued_low_list[0]._next_pop + i]->timeout_budget - diff;
                int size_p = temp_queue[_enqueued_low_list[0]._next_pop + i]->size();
                if (diff_budget > 0) {
                    _enqueued_low_list[0]._queue[count_good] = temp_queue[_enqueued_low_list[0]._next_pop + i];

                    /*int64_t diff =
                            GLOBAL_TIME -
                            _enqueued_low._queue[count_good]->enter_timestamp;
                    _enqueued_low._queue[count_good]->enter_timestamp =
                            GLOBAL_TIME;
                    int64_t diff_budget =
                            _enqueued_low._queue[count_good]->timeout_budget -
                            diff;*/
                    count_good++;
                } else {
                    dropped_p++;
                    _queuesize_low[0] -= size_p;
                }
            }
            _enqueued_low_list[0]._next_pop = 0;
            _enqueued_low_list[0]._next_push = count_good;
            _enqueued_low_list[0]._count = count_good;

            if (dropped_p > 0) {
                printf("Started With %d Pkts - Dropped %d Pkts - Count %d - "
                       "Pop %d "
                       "- Push %d\n",
                       initial_count, dropped_p, _enqueued_low_list[0]._count, _enqueued_low_list[0]._next_pop,
                       _enqueued_low_list[0]._next_push);
            }
        }

        assert(!all_enqueued_low_queues_empty());

        pkt = popLowQueue();
            
        packets_seen++;

        // std::cout << "pkt->queue_idx(): " << pkt->queue_idx() << endl;

        if (_phantom_in_series) {
            _current_queuesize_phatom += pkt->size();
            if (_current_queuesize_phatom > _phantom_queue_size) {
                _current_queuesize_phatom = _phantom_queue_size;
            }
        }

        // ECN mark on deque
        // if queue_idx is < 0 (not set), we want to pass 0 to decideECN
        int pkt_queue_idx = max(0, pkt->queue_idx());
        if (decide_ECN(pkt_queue_idx)) {
            pkt->set_flags(pkt->flags() | ECN_CE);
        }

        if (_logger)
            _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        _num_packets++;
    } else if (_serv == QUEUE_HIGH) {
        assert(!_enqueued_high.empty());
        pkt = _enqueued_high.pop();
        trimmed_seen++;
        // printf("Queue %s - %d@%d\n", _nodename.c_str(), pkt->from, pkt->to);
        _queuesize_high -= pkt->size();

        if (_logger)
            _logger->logQueue(*this, QueueLogger::PKT_SERVICE, *pkt);
        if (pkt->type() == NDPACK)
            _num_acks++;
        else if (pkt->type() == NDPNACK || pkt->type() == UECNACK || pkt->_is_trim) {
            _num_nacks++;
            if (_phantom_in_series) {
                _current_queuesize_phatom += 4160;
                if (_current_queuesize_phatom > _phantom_queue_size) {
                    _current_queuesize_phatom = _phantom_queue_size;
                }
            }
        } else if (pkt->type() == NDPPULL)
            _num_pulls++;
        else {
            // cout << "Hdr: type=" << pkt->type() << endl;
            _num_headers++;
            // ECN mark on deque of a header, if low priority queue is still
            // over threshold
            // if we have congestion on either of the low_queues, mark it
            // todo: we might require to change this!
            bool mark_ecn_on_dequeue = decide_ECN(0) || decide_ECN(1);
            if (mark_ecn_on_dequeue) {
                pkt->set_flags(pkt->flags() | ECN_CE);
            }
        }
    } else {
        assert(0);
    }

    pkt->flow().logTraffic(*pkt, *this, TrafficLogger::PKT_DEPART);
    pkt->sendOn();

    //_virtual_time += drainTime(pkt);

    _serv = QUEUE_INVALID;

    // cout << "E[ " << _enqueued_low.size() << " " << _enqueued_high.size() <<
    // " ]" << endl;

    if (!_enqueued_high.empty() || !all_enqueued_low_queues_empty())
        beginService();

    if (_current_queuesize_phatom >= 0 && first_time && _use_phantom) {
        first_time = false;
        decreasePhantom();

    }
}

void CompositeQueue::doNextEvent() {
    if (eventlist().now() == _decrease_phantom_next && _use_phantom) {
        decreasePhantom();
        return;
    }
    completeService();
    if (COLLECT_DATA) {
        if (_queuesize_low[0] != 0) {
            std::string file_name = PROJECT_ROOT_PATH /
                                    ("output/queue/queue" + _nodename.substr(_nodename.find(")") + 1) + "_0.txt");
            std::ofstream MyFile(file_name, std::ios_base::app);
            MyFile << eventlist().now() / 1000 << "," << int(_queuesize_low[0]) << ","
                    << int(_ecn_minthresh) << ","
                    << int(_ecn_maxthresh) << ","
                    << int(_maxsize) << std::endl;
            MyFile.close();
        }
        if (_queuesize_low[1] != 0) {
            std::string file_name = PROJECT_ROOT_PATH /
                                    ("output/queue/queue" + _nodename.substr(_nodename.find(")") + 1) + "_1.txt");
            std::ofstream MyFile(file_name, std::ios_base::app);
            MyFile << eventlist().now() / 1000 << "," << int(_queuesize_low[1]) << ","
                    << int(_ecn_minthresh) << ","
                    << int(_ecn_maxthresh) << ","
                    << int(_maxsize) << std::endl;
            MyFile.close();
        }
    } else {
    }
    

    if (COLLECT_DATA && _use_phantom) {
        if (_current_queuesize_phatom != 0) {
            std::string file_name = PROJECT_ROOT_PATH / ("output/queue_phantom/queue_phantom" +
                                                            _nodename.substr(_nodename.find(")") + 1) + ".txt");
            std::ofstream MyFile(file_name, std::ios_base::app);

            MyFile << eventlist().now() / 1000 << "," << int(_current_queuesize_phatom)
                    << "," << int((_phantom_queue_size / 100 * _phantom_kmin)) << ","
                    << int((_phantom_queue_size / 100 * _phantom_kmax)) << "," << int(_phantom_queue_size) <<std::endl;

            MyFile.close();
        }
    }
}

void CompositeQueue::receivePacket(Packet &pkt) {
    pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_ARRIVE);
    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_ARRIVE, pkt);
    // is this a Tofino packet from the egress pipeline?

    if (is_failing) {
        pkt.free();
        dropped_in_a_row++;
       //printf("Dropping Packet EV %d - Subflow %d - Time %f - Sequence %d - Queue %s\n", pkt.pathid(), pkt.subflow_number, timeAsUs(eventlist().now()), pkt.id(), _nodename.c_str());
        return;
    } else {
        dropped_in_a_row = 0;
        //printf("Regurlar Packet EV %d - Subflow %d - Time %f\n", pkt.pathid(), pkt.subflow_number, timeAsUs(eventlist().now()));
    }


    if (failure_multiples && _nodename.find("BORDER") != std::string::npos) {


        /* if (_nodename.find("BORDER") != std::string::npos) {
            int number1 = rand() % 1000;
            if (number1 <= 1 && pkt.size() > 100) {
                pkt.free();
                return;
            }   
        } */
        

        // Measured probabilities for Distance 1 (from your table):
        const double p_loss1 = 3.0e-4;           // Exactly 1 drop per block
        const double p_loss2 = 7.5e-5;           // Exactly 2 drops per block
        const double p_loss3 = 1.6e-5;           // Exactly 3 drops per block
        const double p_drop0 = 1.0 - (p_loss1 + p_loss2 + p_loss3); // 0 drop probability

        static std::mt19937 gen(static_cast<unsigned>(
            std::chrono::system_clock::now().time_since_epoch().count()));
        static std::uniform_real_distribution<double> dis(0.0, 1.0);

        // If at the start of a block, decide how many drops for this block.
        if (currentPacketIndex == 0) {
            dropIndices.clear();
            double r = dis(gen);
            int dropCount = 0;
            if (r < p_drop0) {
                dropCount = 0;
            } else if (r < p_drop0 + p_loss1) {
                dropCount = 1;
            } else if (r < p_drop0 + p_loss1 + p_loss2) {
                dropCount = 2;
            } else {
                printf("Dropping 3 packets\n");
                dropCount = 3;
            }
            // If we need drops, pick dropCount unique packet indices from 0 to blockSize-1.
            if (dropCount > 0) {
                std::uniform_int_distribution<int> index_dis(0, block_size - 1);
                while (dropIndices.size() < static_cast<size_t>(dropCount)) {
                    dropIndices.insert(index_dis(gen));
                }
            }
        }

        bool dropPacket = (dropIndices.find(currentPacketIndex) != dropIndices.end());
        if (dropPacket) {
            currentPacketIndex++;
            if (currentPacketIndex >= block_size) {
                currentPacketIndex = 0;
            }
            //printf("Failure\n");
            pkt.free();
            return;
        } else {
            currentPacketIndex++;
            if (currentPacketIndex >= block_size) {
                currentPacketIndex = 0;
            }
        }

    }

    


    int pkt_queue_idx1 = max(0, pkt.queue_idx());  
    if (_queuesize_low[pkt_queue_idx1] > 100000) {
        //printf("Queue %s is at %d - Max Size %d\n", _nodename.c_str(), _queuesize_low[pkt_queue_idx1], _maxsize);
    }

    if (dropped_in_a_row > 4) {
        //printf("Dropped in a row %d - Time %f\n", dropped_in_a_row, timeAsUs(eventlist().now()));
    }
    //printf("I am Queue %s, Receiving Packet %d\n", _nodename.c_str(), pkt.id());

    if (failed_link && !pkt.header_only()) {
        pkt.strip_payload();
        pkt.is_failed = true;
        /* printf("Queue %s - Time %lu - Broken Link", _nodename.c_str(),
               GLOBAL_TIME / 1000); */

    } else if (!pkt.header_only()) {
        //  Queue
        /* printf("Remote is %s vs %s %d %d - Switch ID - %d %d\n", this->getRemoteEndpoint()->nodename().c_str(),
               getSwitch()->nodename().c_str(), this->getRemoteEndpoint()->dc_id,
               ((Switch *)getRemoteEndpoint())->getID(), pkt.previous_switch_id, getSwitch()->getID()); */

        // if queue idx is not set, set it to 0
        int pkt_queue_idx = max(0, pkt.queue_idx());        

        

        if (_queuesize_low[pkt_queue_idx] + pkt.size() <= _maxsize) {
            // regular packet; don't drop the arriving packet

            /* if (_nodename == "compqueue(100000Mb/s,4480103bytes)DC0-BORDER0->BORDER0_LINK:0") {
                printf("Transiting at Name %s - Packet From %d-%d \n", _nodename.c_str(), pkt.from, pkt.id());
                seen_pkts[pkt.from]++;
                for (const auto& pair : seen_pkts) {
                    std::cout << pair.first << ":" << pair.second << " - ";
                }
                std::cout << std::endl;
            } */

            // we are here because either the queue isn't full or,
            // it might be full and we randomly chose an
            // enqueued packet to trim

            assert(_queuesize_low[pkt_queue_idx] + pkt.size() <= _maxsize);
            Packet *pkt_p = &pkt;
            /*printf("Considering Queue2 %s - From %d - Header Only %d - Size %d
            "
                   "- "
                   "Arrayt Size "
                   "%d\n",
                   _nodename.c_str(), pkt.from, pkt.header_only(),
                   _queuesize_low, _enqueued_low.size());
            fflush(stdout);*/
            pkt_p->enter_timestamp = GLOBAL_TIME;
            _enqueued_low_list[pkt_queue_idx].push(pkt_p);

            /* if (_nodename == "compqueue(50000Mb/s,1250000bytes)DC1-LS7->DST15") {
                printf("Receive: Queue %s - From %d to %d size %d - time %lu - "
                       "Last %f\n",
                       _nodename.c_str(), pkt_p->from, pkt_p->to, pkt_p->size(),
                       GLOBAL_TIME / 1000,
                       GLOBAL_TIME / 1000.0 - last_recv / 1000.0);
                last_recv = GLOBAL_TIME;
            } */

            // Increase PQ on data packet, if in parallel
            if (!_phantom_in_series) {
                //printf("Increasing Phantom Queue Size %d\n", pkt.size());
                _current_queuesize_phatom += pkt.size();
                if (_current_queuesize_phatom > _phantom_queue_size) {
                    _current_queuesize_phatom = _phantom_queue_size;
                }
            }
            _queuesize_low[pkt_queue_idx] += pkt.size();

            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

            if (_serv == QUEUE_INVALID) {
                beginService();
            }

            // cout << "BL[ " << _enqueued_low.size() << " " <<
            // _enqueued_high.size() << " ]" << endl;

            return;
        } else {

            
            /* dropped_pkts[pkt.from]++;
            printf("Dropping at Name %s - Packet From %d-%d \n", _nodename.c_str(), pkt.from, pkt.id());
            for (const auto& pair : dropped_pkts) {
                std::cout << pair.first << ":" << pair.second << " - ";
            }
            std::cout << std::endl; */
            // Increase Phantom Queue when also getting a trim
            if (!_phantom_in_series) {
                _current_queuesize_phatom += pkt.size(); // We increase as if it was a full pkt, not just
                                                   // the header of the trim
                if (_current_queuesize_phatom > _phantom_queue_size) {
                    _current_queuesize_phatom = _phantom_queue_size;
                }
            }
            // strip packet the arriving packet - low priority queue is full
            // cout << "B [ " << _enqueued_low.size() << " " <<
            // _enqueued_high.size() << " ] STRIP" << endl;
            /* printf("Trimming at %s - Packet PathID %d\n", _nodename.c_str(),
                    pkt.pathid());*/
            if (_queuesize_low[pkt_queue_idx] + pkt.size() > _maxsize) {

                queue_overflow_count += pkt.size();

                if (COLLECT_DATA) {
                    std::string file_name = PROJECT_ROOT_PATH /
                                        ("output/queue_overflow/queue_overflow" + _nodename.substr(_nodename.find(")") + 1) + ".txt");
                    std::ofstream MyFileQueueOverflow(file_name, std::ios_base::app);
                    MyFileQueueOverflow << eventlist().now() / 1000 << "," << queue_overflow_count << std::endl;
                    MyFileQueueOverflow.close();
                }

                /* printf("Dropping1 %s Packet From %d-%d \n", nodename().c_str(), pkt.from, pkt.id()); */
                if (use_bts && !pkt.is_bts_pkt) {
                    /* printf("Dropping2 %s Packet From %d-%d \n", nodename().c_str(), pkt.from, pkt.id()); */
                    assert(!pkt.header_only());
                    UecPacket *bts_pkt = UecPacket::newpkt(dynamic_cast<UecPacket &>(pkt));
                    bts_pkt->strip_payload();
                    bts_pkt->is_bts_pkt = true;
                    bts_pkt->set_dst(bts_pkt->from);

                    /* for (int i = 0; i < bts_pkt->route()->size(); i++) {
                        printf(" Route is %s -->", bts_pkt->route()->at(i)->nodename().c_str());
                    }
                    Route *r = new Route(); */

                    // bts_pkt->set_route bts_pkt->set_next_hop(bts_pkt->route()->at(0));
                    /* for (int i = 0; i < bts_pkt->route()->size(); i++) {
                        printf(" Updated Route is %s -->", bts_pkt->route()->at(i)->nodename().c_str());
                        r->push_back(bts_pkt->route()->at(i));
                    } */
                    // bts_pkt->set_route(*r);
                    /* printf("BTS Event at %s - Now %d-%d-%d to %d - %lu\n", _nodename.c_str(), bts_pkt->from,
                    pkt.from, bts_pkt->id(), bts_pkt->dst(), GLOBAL_TIME / 1000); printf("\n\n"); */

                    // bts_pkt->sendOn();
                    pkt.free();
                    getSwitch()->receivePacket(*bts_pkt);

                    return;
                }

                if (_drop_when_full) {
                    // Dropping Packet and returning
                    /*printf("Queue Size %d - Max %d\n", _queuesize_low,
                           _maxsize);*/
                    // printf("Dropping a PKT\n");
                    pkt.free();
                    return;
                }
            }

            printf("Trimming %d@%d - Time %lu\n", pkt.from, pkt.to,
                    GLOBAL_TIME / 1000);
            pkt.strip_payload();
            _num_stripped++;
            pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_TRIM);
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_TRIM, pkt);
        }
    }
    assert(pkt.header_only());

    if (_queuesize_high + pkt.size() > 20000 * _maxsize) {

        /* printf("Dropping a PKT - Max Size %lu - Queue Size %lu \n", _maxsize, _queuesize_high); */
        // drop header
        cout << "dropped header!\n";
        if (pkt.reverse_route() && pkt.bounced() == false) {
            // return the packet to the sender
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_BOUNCE, pkt);
            pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_BOUNCE);
            // XXX what to do with it now?
#if 0
            printf("Bounce1 at %s\n", _nodename.c_str());
            printf("Fwd route:\n");
            print_route(*(pkt.route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            pkt.bounce();
#if 0
            printf("\nRev route:\n");
            print_route(*(pkt.reverse_route()));
            printf("nexthop: %d\n", pkt.nexthop());
#endif
            _num_bounced++;
            pkt.sendOn();
            return;
        } else {
            if (_logger)
                _logger->logQueue(*this, QueueLogger::PKT_DROP, pkt);
            pkt.flow().logTraffic(pkt, *this, TrafficLogger::PKT_DROP);
            cout << "B1[ " << _enqueued_low_list[0].size() + _enqueued_low_list[1].size() << " " << _enqueued_high.size() << " ] DROP " << pkt.flow().get_id()
                 << endl;
            pkt.free();
            _num_drops++;
            return;
        }
    }

    // if (pkt.type()==NDP)
    //   cout << "H " << pkt.flow().str() << endl;

    /* if (true) {
        seen_acks[pkt.from]++;
        printf("Received ACK Name %s - Packet From %d-%d \n", _nodename.c_str(), pkt.from, pkt.id());
        for (const auto& pair : seen_acks) {
            std::cout << pair.first << ":" << pair.second << " - ";
        }
        std::cout << std::endl;
    } */

    Packet *pkt_p = &pkt;
    if (pkt.is_failed) {
        pkt_p->is_failed = true;
        printf("Setting Failure Bit\n");
    }
    _enqueued_high.push(pkt_p);
    _queuesize_high += pkt.size();
    if (_logger)
        _logger->logQueue(*this, QueueLogger::PKT_ENQUEUE, pkt);

    // cout << "BH[ " << _enqueued_low.size() << " " <<
    // _enqueued_high.size() << " ]" << endl;

    if (_serv == QUEUE_INVALID) {
        beginService();
    }
}

mem_b CompositeQueue::queuesize() const { return _queuesize_low[0] + _queuesize_low[1] + _queuesize_high; }
