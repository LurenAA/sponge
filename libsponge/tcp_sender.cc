#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <cassert>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity),_rto(_initial_retransmission_timeout),_timerCount(0),_windowSize(0),_outstanding_out(),_consecutiveSendCount(0),_last_acked(0) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t n = 0;
    for(const auto& seg: _outstanding_out) {
        n += seg.segment.length_in_sequence_space();
    }
    return n; 
}

void TCPSender::fill_window() {
    bool closed = !_next_seqno;
    // bool synSent = _next_seqno > 0 && next_seqno_absolute() == bytes_in_flight();
    bool synAsked = (next_seqno_absolute() > bytes_in_flight() && !stream_in().eof() )|| 
        (stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2);
    bool finSent = stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 && bytes_in_flight();
    // bool finAsked = stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 && !bytes_in_flight();
    
    if(closed) {
        TCPSegment seg;
        TCPHeader header;
        header.seqno = wrap(_next_seqno, _isn);
        header.syn = true;
        seg.header() = header;

        _segments_out.push(seg);

        // default not zero window
        _outstanding_out.push_back({seg, false});
        _next_seqno += seg.length_in_sequence_space();

        assert(seg.length_in_sequence_space() == 1);

        if(!_timerCount) 
            _timerCount = _rto;
    }

    if(!synAsked)
        return ;

    // if(!_windowSize) {
    //     // send_empty_segment();
    //     return ;
    // }    
    auto wsz = _windowSize == 0? 1: _windowSize;

    while(wsz + _last_acked > _next_seqno && !finSent) {
        TCPSegment seg;
        TCPHeader header;

        uint64_t maxSeqAllowed = wsz + _last_acked - _next_seqno;

        if(stream_in().eof()) {
            header.fin = true;
        } else if(stream_in().buffer_empty()) {
            break;
        } else if(maxSeqAllowed < stream_in().buffer_size()) {
            string str = stream_in().read(maxSeqAllowed > TCPConfig::MAX_PAYLOAD_SIZE ? TCPConfig::MAX_PAYLOAD_SIZE:maxSeqAllowed);
            seg.payload() =  Buffer(move(str));
        } else if(maxSeqAllowed >= stream_in().buffer_size()) {
            // string str = stream_in().read(stream_in().buffer_size());
            // seg.payload() =  Buffer(move(str));

            // if(stream_in().eof() && siz > seg.payload().size()) 
            //     header.fin = true;
            size_t readsize = stream_in().buffer_size() > TCPConfig::MAX_PAYLOAD_SIZE ? TCPConfig::MAX_PAYLOAD_SIZE : stream_in().buffer_size();
            string str = stream_in().read(readsize);
            seg.payload() =  Buffer(move(str));

            if(maxSeqAllowed > seg.payload().size() && stream_in().eof())
                header.fin = true;
        }

        header.seqno = wrap(_next_seqno, _isn);
        seg.header() = header;
        
        // if(stream_in().eof()) {
        //     seg.header().fin = true;
        // }

        assert(seg.length_in_sequence_space());
        _next_seqno += seg.length_in_sequence_space();

        _segments_out.push(seg);
        _outstanding_out.push_back({seg, _windowSize == 0});

        if(!_timerCount) 
            _timerCount = _rto;
        
        finSent = stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 && bytes_in_flight();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    //  DUMMY_CODE(ackno, window_size); 
    uint64_t acknoAb = unwrap(ackno, _isn, _next_seqno);
    //Impossible ackno (beyond next seqno) is ignored
    if(_next_seqno < acknoAb) return ;

 
    _windowSize =  window_size;
    _last_acked = acknoAb;

    int deleteCount = 0;
    while(_outstanding_out.size()){
        auto head = _outstanding_out.front();
        auto headSeqnoAb = unwrap(head.segment.header().seqno + head.segment.length_in_sequence_space(), _isn, _next_seqno);
        if(headSeqnoAb <= acknoAb) {
            deleteCount += 1;
            _outstanding_out.erase(_outstanding_out.begin());
        } else 
            break;
    }

    if(deleteCount) {
        _rto = _initial_retransmission_timeout;
        _timerCount = _rto;
        _consecutiveSendCount = 0;
    }

    if(_windowSize + _last_acked >= _next_seqno) {
        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // DUMMY_CODE(ms_since_last_tick); 
    if(!_timerCount) return ;
    if(_timerCount > ms_since_last_tick) {
        _timerCount -= ms_since_last_tick;
        return ;
    }
    
    if(!_outstanding_out.size()) {
        _timerCount = 0;
        return ;
    }

    auto seg = _outstanding_out.front();
    _segments_out.push(seg.segment);
    
    if(!seg.ifZeroWindow) {  
        ++_consecutiveSendCount;
        _rto *= 2;
    }

    _timerCount = _rto;
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutiveSendCount; 
}

void TCPSender::send_empty_segment() {
    TCPHeader header;
    header.seqno = wrap(_next_seqno, _isn);
    TCPSegment seg;
    seg.header() = header;
    // if(stream_in().buffer_size()) {
    //     string s(stream_in().peek_output(1));
    //     seg.payload() = Buffer(move(s));
    // }
    // assert(!seg.length_in_sequence_space());

    _segments_out.push(seg);
}
