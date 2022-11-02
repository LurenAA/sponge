#include "stream_reassembler.hh"
#include <cassert>
#include <algorithm>




// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity): _output(capacity), _capacity(capacity), _unassembledSize(0), _indexedRead(0), _unassembledDataVec() {
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // DUMMY_CODE(data, index, eof);
    string dataCopy = data;
    size_t indexCopy = index;
    bool eofCopy = eof;
    size_t dataSz = dataCopy.size();
    size_t dataStartIdx = 0;

    // indexCopy < _indexedRead: data = "" , eof = true
    if(indexCopy < _indexedRead && indexCopy + dataSz <= _indexedRead)
        return ;
    else if (indexCopy < _indexedRead && indexCopy + dataSz > _indexedRead) {
        dataStartIdx = _indexedRead - indexCopy;
        dataSz = dataSz - (_indexedRead - indexCopy);
    } else if (indexCopy > _indexedRead) {
        push_into_unassembled_vec(dataCopy, indexCopy, eofCopy);
        return ;
    }

    size_t roomCanRead = _capacity - _output.buffer_size();

    if(roomCanRead < dataSz) {
        dataSz = roomCanRead;
        eofCopy = false;
    }

    // = for data.size() == 0
    assert(dataStartIdx <= data.size());

    _indexedRead += dataSz;
    _output.write(dataCopy.substr(dataStartIdx, dataSz));
    if (eofCopy)
        _output.end_input();
    
    assemble();
    if(_capacity < _unassembledSize + _output.buffer_size())
        clean();
}

void StreamReassembler::clean() {
    if(_capacity >= _unassembledSize + _output.buffer_size())  
        return ;
    size_t n = _unassembledSize + _output.buffer_size() - _capacity;
 
    for(auto ri = _unassembledDataVec.rbegin(); 
        ri != _unassembledDataVec.rend(); 
        ++ri) {
        auto sz = ri->data.size();
        if(sz <= n) {
            _unassembledDataVec.erase((ri + 1).base());
            n -= sz;
        } else if(sz > n) {
            ri->data = ri->data.substr(0, sz - n);
            ri->endIndex = ri->endIndex - n;
            if(ri->eof) ri->eof = false;
            n = 0;
        }

        if(!n) break ;    
    }

    _unassembledSize -= n;
}

void StreamReassembler::push_into_unassembled_vec(const string &data, const size_t index, const bool eof) {
    auto idx = index > _indexedRead ? index : _indexedRead;
    auto substrStart = index >= _indexedRead ? 0 : _indexedRead - index;
    
    assert(substrStart <= data.size());

    auto str = data.substr(substrStart);
    bool eofFlag = eof;
    
    if(!remove_duplicate_part(str, idx)) return ;


    auto endIdx = str.size() ? idx + str.size() - 1 : idx;

    assert(!str.size() || str.size() == endIdx + 1 - idx);

    _unassembledDataVec.push_back({idx, endIdx, str, eofFlag});
    _unassembledSize += str.size();
}

bool StreamReassembler::remove_duplicate_part(string &data, size_t& idx) {
    auto endIdx = data.size() ? idx + data.size() - 1 : idx;

    for(auto itor = _unassembledDataVec.begin(); 
        itor != _unassembledDataVec.end();
        ++itor) 
    {
        auto li = itor->index;
        auto ri = itor->endIndex;
        if (idx < li && li <= endIdx && endIdx <= ri) {
            endIdx = li - 1;

            assert(endIdx + 1 - idx <= data.size());

            data = data.substr(0, endIdx + 1 - idx);
        } else if (idx < li && endIdx > ri) {
            remove_unassembled_element(itor);        
        } else if(li <= idx && endIdx <= ri) {
            return false;
        } else if(li <= idx && idx <= ri && endIdx > ri) {
            assert(ri - idx + 1 < data.size());

            data = data.substr(ri - idx + 1);
            idx = ri + 1;
        } 
    }

    return true;
}


void StreamReassembler::assemble() {
    sort(_unassembledDataVec.begin(),
         _unassembledDataVec.end(), 
         [&](const UnassembledStr& lhs, const UnassembledStr& rhs) {
            return lhs.index < rhs.index;
         }
    );

    for(auto itr = _unassembledDataVec.begin(); 
        itr != _unassembledDataVec.end(); 
        ++itr) 
    {
        if (itr->endIndex < _indexedRead) {
            remove_unassembled_element(itr);
        }
    }


    for(auto itr = _unassembledDataVec.begin(); 
        itr != _unassembledDataVec.end(); 
        ++itr) 
    {
        if(itr->index <= _indexedRead  && _indexedRead <= itr->endIndex ) {
            UnassembledStr s(*itr);
            remove_unassembled_element(itr);
            push_substring(s.data, s.index, s.eof);
            break ;
        }   
    }
}


void StreamReassembler::remove_unassembled_element(std::vector<UnassembledStr>::iterator& itor){
    auto sz = itor->data.size();
    _unassembledDataVec.erase(itor--);
    _unassembledSize -= sz;
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembledSize; }

bool StreamReassembler::empty() const { return !unassembled_bytes() && _output.buffer_empty(); }
