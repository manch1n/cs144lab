#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
    uint64_t prevAbsoluteSeqno = _absoluteSeqno;
    if (seg.header().syn == true) {
        if (_synRecv == true)
            return false;
        else {
            _synRecv = true;
            _isn = seg.header().seqno;
            _absoluteSeqno++;
        }
    }
    if (_synRecv == false) {
        return false;
    }

    // edge situation
    uint64_t curAbsoluteSeqno = unwrap(seg.header().seqno, _isn, prevAbsoluteSeqno);
    if (prevAbsoluteSeqno + window_size() <= curAbsoluteSeqno) {
        return false;
    }
    uint64_t curStrmIdx = streamIdx(curAbsoluteSeqno);
    if (curAbsoluteSeqno == 0) {
        if (seg.header().syn == true) {
            curStrmIdx = 0;
        } else {
            return false;
        }
    }
    if (curAbsoluteSeqno == 1) {
        if (seg.header().fin == true) {
            curStrmIdx = 0;
        }
    }

    size_t prevAckinbuf = _reassembler.stream_out().buffer_size();
    _reassembler.push_substring(seg.payload().copy(), curStrmIdx, seg.header().fin);
    size_t curAckinbuf = _reassembler.stream_out().buffer_size();
    size_t readSize = curAckinbuf - prevAckinbuf;
    _absoluteSeqno += readSize;

    // edge situation if fin totally has handled then let absSeqno plus one
    if (_endFlag == false) {
        if (_reassembler.stream_out().input_ended()) {
            _absoluteSeqno++;
            _finDone = true;
            _endFlag = true;
        }
    }
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_synRecv == false) {
        return {};
    }
    return wrap(_absoluteSeqno, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
