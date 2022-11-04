#include "tcp_receiver.hh"
#include <cassert>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // DUMMY_CODE(seg);
    auto header = seg.header();
    if(!header.syn && !_senderISN.has_value())
        return ;

    if(header.syn && !_senderISN.has_value()) {
        // assert(!_senderISN.has_value());

        _senderISN = header.seqno;
        _checkpoint = 0;
    }

    uint64_t abIdx = unwrap(header.seqno, _senderISN.value(), _checkpoint);
    _checkpoint = abIdx;
    string data = seg.payload().copy();
    if(abIdx == 0 && !header.syn) //abidx 0 => syn, prevent no syn datagram rewrite to abIdx 0
        return ;
    _reassembler.push_substring(data, abIdx ? abIdx - 1 : 0, header.fin? true: false);

    if(header.fin)
        receiveFin = true;

    if(receiveFin && unassembled_bytes() == 0)
        stream_out().end_input();
}

optional<WrappingInt32> TCPReceiver::ackno() const {
   if(!_senderISN.has_value()) return optional<WrappingInt32>{};
   uint64_t idx = stream_out().bytes_written() + stream_out().input_ended() + _senderISN.has_value();
   return wrap(idx, _senderISN.value());
}

size_t TCPReceiver::window_size() const { 
    return _capacity - stream_out().buffer_size();
}
