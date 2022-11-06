#include "tcp_connection.hh"

#include <cassert>
#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return timeSinceLastSeg; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    if (!active())
        return;

    timeSinceLastSeg = 0;

    if (rtTimeCount)
        rtTimeCount = 0;

    const auto &header = seg.header();

    if (header.rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }

    bool keepAliveTestCase = _receiver.ackno().has_value() && 
        header.seqno != _receiver.ackno().value() && 
        !seg.length_in_sequence_space();

    _receiver.segment_received(seg);
    if (header.ack) {
        const WrappingInt32 &ackno = header.ackno;
        const uint16_t &windowSize = header.win;

        _sender.ack_received(ackno, windowSize);
    } 

    if (header.syn && !_sender.next_seqno_absolute()) {
        connect();
        return ;
    }

    if (seg.length_in_sequence_space()) {
        _sender.fill_window();
        //输出数据流没有数据的情况
        if (!_sender.segments_out().size())
            _sender.send_empty_segment();
    } else if( 
            keepAliveTestCase    //单纯的Ack报文 SeqNo占刚好下一个AckNo
        )
    {
        //当只是ack时，不需要发送一个空报文去ack一个ack，只对于keepalive才回复
        _sender.send_empty_segment();
    }

    send_to_segout();
    activeEnd();
}

bool TCPConnection::active() const {
    bool uncleanShutDown = _sender.stream_in().error() && _receiver.stream_out().error();
    return !uncleanShutDown && !cleanClosed;
}

size_t TCPConnection::write(const string &data) {
    // DUMMY_CODE(data);
    return _sender.stream_in().write(data);
}

void TCPConnection::clearBothQueue() {
    auto &senderQ = _sender.segments_out();
    while (!senderQ.empty()) {
        senderQ.pop();
    }
    auto &connectionQ = segments_out();
    while (!connectionQ.empty()) {
        connectionQ.pop();
    }
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    // DUMMY_CODE(ms_since_last_tick);
    assert(ms_since_last_tick + timeSinceLastSeg >= timeSinceLastSeg);  // prevent overflow
    timeSinceLastSeg += ms_since_last_tick;

    _sender.tick(ms_since_last_tick);

    unsigned int retransTimes = _sender.consecutive_retransmissions();
    if (retransTimes > TCPConfig::MAX_RETX_ATTEMPTS && active()) {
        clearBothQueue();
        _sender.send_empty_segment();
    }
    send_to_segout(true);

    if (rtTimeCount && ms_since_last_tick >= rtTimeCount) {
        cleanClosed = true;
    } else if (rtTimeCount && rtTimeCount > ms_since_last_tick)
        rtTimeCount -= ms_since_last_tick;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();

    //是否要将现在Stream中的数据输出完毕？
    _sender.fill_window();
    send_to_segout();
}

void TCPConnection::connect() {
    assert(_sender.next_seqno_absolute() == 0);
    _sender.fill_window();

    auto &senderQueue = _sender.segments_out();
    auto &seg = senderQueue.front();

    const auto &ackno = _receiver.ackno();
    const auto &wsz = _receiver.window_size();
    auto &header = seg.header();
    if (ackno.has_value()) {
        header.ack = true;
        header.ackno = ackno.value();
        header.win = wsz;
    }

    _segments_out.push(seg);
    senderQueue.pop();
}

void TCPConnection::activeEnd() {
    const auto &inboundStream = _receiver.stream_out();
    const auto &outboundStream = _sender.stream_in();
    bool preReq1 = inboundStream.input_ended();

    bool preReq2And3 = outboundStream.eof() &&
        _sender.next_seqno_absolute() == outboundStream.bytes_written() + 2 &&
        _sender.bytes_in_flight() == 0;

    if (preReq1 && preReq2And3) {
        if(_linger_after_streams_finish && !rtTimeCount)
            rtTimeCount = 10 * _cfg.rt_timeout;
        else if(!_linger_after_streams_finish)
            cleanClosed = true;
    } 
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            clearBothQueue();
            _sender.send_empty_segment();
            send_to_segout(true);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_to_segout(bool ifLastRst) {
    auto &senderQ = _sender.segments_out();
    auto qSize = senderQ.size();
    if (!qSize)
        return;

    auto wsz = _receiver.window_size();
    auto ackno = _receiver.ackno();
    while (qSize--) {
        auto &seg = senderQ.front();
        auto &header = seg.header();

        if(!hasFin && _receiver.stream_out().input_ended()) {
            _linger_after_streams_finish = false;
        } 

        if(header.fin)
            hasFin = true;

        header.win = wsz;
        if (ackno.has_value()) {
            header.ack = true;
            header.ackno = ackno.value();
        }

        if (!qSize && ifLastRst) {
            header.rst = true;
        }

        _segments_out.push(seg);
        senderQ.pop();
    }

    assert(!_sender.segments_out().size());
}