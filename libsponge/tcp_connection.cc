#include "tcp_connection.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity(); 
}

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const {
    return timeSinceLastSeg; 
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    assert(active());
    timeSinceLastSeg = 0;
    if(rtTimeCount && !lingerInactive)  
        rtTimeCount = 0;

    const auto &header = seg.header();

    if (header.rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    }

    _receiver.segment_received(seg);
    if(header.syn) {
        connect();
    }
    // if (header.fin) {
    // }

    if (header.ack) {
        const WrappingInt32& ackno = header.ackno;
        const uint16_t& windowSize = header.win;

        _sender.ack_received(ackno, windowSize);
    }

    const auto& senderQueueSizeBefore = _sender.segments_out().size();
    if (seg.length_in_sequence_space() == 0 && _receiver.ackno().has_value()) {
        _sender.send_empty_segment();
    }

    if (seg.length_in_sequence_space()) {
        _sender.fill_window();
    }

    assert(_sender.segments_out().size() - senderQueueSizeBefore >= 1);
    send_to_segout();
}

bool TCPConnection::active() const {
    bool uncleanShutDown = _sender.stream_in().error() && _receiver.stream_out().error();
    bool opB = _receiver.stream_out().input_ended() && hasSendFin;
    return !(uncleanShutDown || lingerInactive || opB); 

}

size_t TCPConnection::write(const string &data) {
    // DUMMY_CODE(data);
    return _sender.stream_in().write(data);
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // DUMMY_CODE(ms_since_last_tick);
    assert(ms_since_last_tick + timeSinceLastSeg >= timeSinceLastSeg);  // prevent overflow
    timeSinceLastSeg += ms_since_last_tick;

    _sender.tick(ms_since_last_tick);

    unsigned int retransTimes = _sender.consecutive_retransmissions();
    if (retransTimes > TCPConfig::MAX_RETX_ATTEMPTS && active()) {
        send_to_segout(true);
    }

    const auto& inboundStream = _receiver.stream_out();
    const auto& outboundStream = _sender.stream_in();
    bool preReq1 = inboundStream.input_ended() && !_receiver.unassembled_bytes();
    
    bool finAsked = outboundStream.eof() && 
        _sender.next_seqno_absolute() == outboundStream.bytes_written() + 2 && 
        _sender.bytes_in_flight() == 0;
    bool preReq2And3 = finAsked;
    
    if(preReq1 && preReq2And3) {
        if(rtTimeCount > ms_since_last_tick)
            rtTimeCount -= ms_since_last_tick;
        else if(rtTimeCount <= ms_since_last_tick && rtTimeCount){
            rtTimeCount = 0;
            lingerInactive = true;
        }

        if(!rtTimeCount && !lingerInactive) {
            rtTimeCount = 10 * _cfg.rt_timeout;
        }
    }
    
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input(); 
}

void TCPConnection::connect() {
    assert(!_sender.next_seqno_absolute());

    _sender.fill_window();

    auto &senderQueue = _sender.segments_out();
    auto &seg = senderQueue.front();

    assert(senderQueue.size() == 1 && seg.header().syn == true);

    const auto &ackno = _receiver.ackno();
    const auto &wsz = _receiver.window_size();
    auto &header = seg.header();
    if (ackno.has_value()) {
        header.ack = true;
        header.ackno = ackno.value();
        header.win = wsz;
    }

    if(header.fin)
        hasSendFin = true;

    _segments_out.push(seg);
    senderQueue.pop();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            send_to_segout(true);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_to_segout(bool ifRst) {
    _sender.send_empty_segment();
    auto senderQ = _sender.segments_out();
    auto qSize = senderQ.size();

    assert(qSize > 0);

    auto wsz = _receiver.window_size();
    auto ackno = _receiver.ackno();
    while (qSize--) {
        auto &seg = senderQ.front();
        auto &header = seg.header();

        if(header.fin)
            hasSendFin = true;

        header.win = wsz;
        if (ackno.has_value()) {
            header.ack = true;
            header.ackno = ackno.value();
        }

        if (qSize == 0 && ifRst) {
            header.rst = true;
        }
        _segments_out.push(seg);
        senderQ.pop();
    }

    assert(!_sender.segments_out().size());
}