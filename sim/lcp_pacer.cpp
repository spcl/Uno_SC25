#include "lcp.h"

LcpSmarttPacer::LcpSmarttPacer(EventList &event_list, LcpSrc &flow)
        : EventSource(event_list, "generic_pacer"), flow(&flow),
          _interpacket_delay(0) {
    _last_send = eventlist().now();
}

void LcpSmarttPacer::schedule_send(simtime_picosec delay) {
    assert(delay > 0);
    // std::cout << "delay is: " << delay << "ps" << endl;
    _interpacket_delay = delay;
    _next_send = _last_send + _interpacket_delay;
    if (_next_send <= eventlist().now()) {
        _next_send = eventlist().now();
        doNextEvent();
        return;
    }
    // cout << "Schedule " << flow->flow_id() << " for " << _next_send << endl;
    eventlist().sourceIsPending(*this, _next_send);
}

void LcpSmarttPacer::cancel() {
    // std::cout << "Cancelling!" << endl;
    _interpacket_delay = 0;
    _next_send = 0;
    eventlist().cancelPendingSource(*this);
}

void LcpSmarttPacer::just_sent() { _last_send = eventlist().now(); }

void LcpSmarttPacer::doNextEvent() {
    // cout << "LcpSmarttPacer::doNextEvent(): " << flow->flow_id() << endl;
    // cout << "_next_send: " <<  _next_send << endl;
    // cout << "now: " << eventlist().now() << endl;
    assert(eventlist().now() == _next_send);
    flow->pacedSend();

    _last_send = eventlist().now();

    if (_interpacket_delay > 0) {
        // cout << eventlist().now() << ": Scheduling 1 (" << flow->flow_id() << ")" << endl;
        eventlist().cancelPendingSource(*this);
        schedule_send(_interpacket_delay);
    } else {
        _interpacket_delay = 0;
        _next_send = 0;
    }
    // std::cout << "------------------" << endl;
}