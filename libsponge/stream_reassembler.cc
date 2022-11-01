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
    string data1 = data;
    size_t index1 = index;
    bool eofFlag = eof;
    size_t dataSz = data1.size();
    size_t startIdx = 0;

    if(index1 < _indexedRead && index1 + dataSz <= _indexedRead)
        return ;
    // else if (index1 == _indexedRead) {
    //     // do nothing
    // } 
    else if (index1 < _indexedRead && index1 + dataSz > _indexedRead) {
        startIdx = _indexedRead - index1;
        dataSz = dataSz - (_indexedRead - index1);
    } else if (index1 > _indexedRead) {
        push_into_unassembled_vec(data1, index1, eofFlag);
        return ;
    }

    // size_t room = _capacity - (_unassembledSize + _output.buffer_size());
    size_t roomCanRead = _capacity - _output.buffer_size();

    if(roomCanRead < dataSz) {
        dataSz = roomCanRead;
        eofFlag = false;
    }

    _indexedRead += dataSz;
    _output.write(data1.substr(startIdx, dataSz));
    if (eofFlag)
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
            _unassembledSize -= sz;
            _unassembledDataVec.erase((ri + 1).base());
            n -= sz;
        } else if(sz > n) {
            _unassembledSize -= n;
            ri->data = ri->data.substr(0, sz - n);
            ri->endIndex = ri->endIndex - n;
            if(ri->eof) ri->eof = false;
            n = 0;
        }

        if(!n) return ;    
    }
}

void StreamReassembler::push_into_unassembled_vec(const string &data, const size_t index, const bool eof) {
    auto idx = index > _indexedRead ? index : _indexedRead;
    auto substrStart = index >= _indexedRead ? 0 : _indexedRead - index;
    auto str = data.substr(substrStart);
    bool eofFlag = eof;
    
    remove_duplicate_part(str, idx, eofFlag);
    if(!str.size())
        return ;

    auto endIdx = str.size() ? idx + str.size() - 1 : idx;

    assert(!str.size() || str.size() == endIdx + 1 - idx);

    _unassembledDataVec.push_back({idx, endIdx, str, eofFlag});
    _unassembledSize += str.size();
}

void StreamReassembler::remove_duplicate_part(string &data, size_t& idx, bool& eof) {
    idx = idx > _indexedRead ? idx : _indexedRead;
    auto substrStart = idx >= _indexedRead ? 0 : _indexedRead - idx;
    data = data.substr(substrStart);
    auto endIdx = data.size() ? idx + data.size() - 1 : idx;

    for(auto itor = _unassembledDataVec.begin(); 
        itor != _unassembledDataVec.end();
        ++itor) 
    {
        auto li = itor->index;
        auto ri = itor->endIndex;
        if (idx < li && endIdx <= ri) {
            endIdx = li - 1;
            data = data.substr(0, endIdx + 1 - idx);
        } else if (idx < li && endIdx > ri) {
            remove_unassembled_element(itor);        
        } else if(li <= idx && endIdx <= ri) {
            data = "";
            return ;
        } else if(li <= idx && idx <= ri && endIdx > ri) {
            data = data.substr(ri - idx + 1);
            idx = ri + 1;
            eof = false;
        } 
    }
}


void StreamReassembler::assemble() {
    sort(_unassembledDataVec.begin(),
         _unassembledDataVec.end(), 
         [&](const UnassembledStr& lhs, const UnassembledStr& rhs) {
            return lhs.index < rhs.index;
         }
    );

    bool used = false;
    for(auto itr = _unassembledDataVec.begin(); 
        itr != _unassembledDataVec.end(); 
        ++itr) 
    {
        if(itr->index <= _indexedRead  && _indexedRead <= itr->endIndex && !used) {
            used = true;
            UnassembledStr s(*itr);
            remove_unassembled_element(itr);
            push_substring(s.data, s.index, s.eof);
            break ;
        }
        else if (itr->endIndex < _indexedRead) {
            remove_unassembled_element(itr);
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
