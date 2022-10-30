#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity1):buffer(), capacity(capacity1) {

}

size_t ByteStream::write(const string &data) {
    // DUMMY_CODE(data);
    size_t dataSize = data.size();
    size_t size = buffer.size();
    size_t wrSize = dataSize;
    if(size + dataSize > capacity) 
        wrSize = capacity - size;

    buffer += (wrSize == dataSize ? data : data.substr(0, wrSize));
    bytesWritten += wrSize;

    return wrSize;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    // DUMMY_CODE(len);
    return buffer.substr(0, len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { 
    // DUMMY_CODE(len);
    buffer = buffer.substr(len);
    bytesRead += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // DUMMY_CODE(len);
    auto res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() {
    endInputFlag = true;
}

bool ByteStream::input_ended() const { return endInputFlag; }

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return !buffer.size(); }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return bytesWritten; }

size_t ByteStream::bytes_read() const { return bytesRead; }

size_t ByteStream::remaining_capacity() const { return capacity - buffer.size(); }
