#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool StreamReassembler::reloacte(uint64_t idx, const std::string &data, std::pair<uint64_t, std::string> *ret) {
    if (idx + data.size() <= totalRead()) {
        return false;
    } else if (idx + data.size() > totalRead()) {
        if (idx < totalRead()) {
            ret->first = totalRead();
            ret->second = data.substr(totalRead() - idx);
        } else {
            ret->first = idx;
            ret->second = data;
        }
    }
    return true;
}

void StreamReassembler::mergeSubstr() {
    if (_subStrs.size() <= 1) {
        return;
    }
    auto prev = _subStrs.begin();
    auto cur = prev;
    cur++;
    for (; cur != _subStrs.end();) {
        uint64_t xend = prev->first + prev->second.size();
        uint64_t ybegin = cur->first, yend = cur->first + cur->second.size();
        if (yend <= xend) {
            cur = _subStrs.erase(cur);
        } else if (ybegin <= xend) {
            prev->second += cur->second.substr(xend - ybegin);
            cur = _subStrs.erase(cur);
        } else if (ybegin > xend) {
            prev = cur;
            cur++;
        }
    }
}

void StreamReassembler::insertSubStr(std::pair<uint64_t, std::string> &&p) {
    if (_subStrs.find(p.first) == _subStrs.end()) {
        _subStrs.insert(std::move(p));
    } else {
        if (_subStrs[p.first].size() < p.second.size()) {
            _subStrs[p.first] = std::move(p.second);
        }
    }
    mergeSubstr();
}

void StreamReassembler::checkPush() {
    for (auto i = _subStrs.begin(); i != _subStrs.end();) {
        uint64_t idx = i->first;
        std::string &data = i->second;

        if (pushedBytes() == idx - totalRead()) {
            _output.write(data);
            i = _subStrs.erase(i);
        } else if (pushedBytes() > idx - totalRead()) {
            if (idx + data.size() - totalRead() > pushedBytes()) {
                size_t offset = pushedBytes() - idx + totalRead();
                _output.write(data.substr(offset));
            }
            i = _subStrs.erase(i);
        } else {
            break;
        }
    }
}

void StreamReassembler::checkEOF() {
    if (pushedBytes() + totalRead() == _eofSize) {
        _output.end_input();
    }
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _subStrs{}, _eofSize(__LONG_MAX__) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    DUMMY_CODE(data, index, eof);
    if (eof) {
        _eofSize = index + data.size();
    }
    std::pair<uint64_t, std::string> ret;
    if (reloacte(index, data, &ret)) {
        insertSubStr(std::move(ret));
    }
    checkPush();
    checkEOF();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t result = 0;
    result = std::accumulate(
        _subStrs.begin(), _subStrs.end(), 0, [](uint64_t preSum, const std::pair<const uint64_t, std::string> &p) {
            return preSum + p.second.size();
        });
    return result;
}

bool StreamReassembler::empty() const { return _subStrs.empty(); }
