#include "tcp_connection.hh"

#include <iostream>
#include <numeric>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _msSinceLastRecv; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
#ifdef DEBUG_LOG
    if (seg.header().rst == true) {
        _logMsg.push_back("Recv RST");
    }
    logRecv(seg);
#endif
    TCPState prevState = state();
    if (seg.header().rst == true) {
        _recvRSTOrSomeWrong = true;
        _linger_after_streams_finish = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    if (seg.header().ack) {
        if (prevState == TCPState::State::LISTEN || prevState == TCPState::State::CLOSED) {
            return;
        }
    }
    uint64_t prevAckno = _receiver.getAbsolutSeqno();
    uint64_t prevUnassembleBytes = _receiver.unassembled_bytes();
    if (_receiver.segment_received(seg) == false && prevState != TCPState::State::SYN_SENT) {
        _sender.send_empty_segment();
        pushOutgoingSegments(prevState);
        return;
    }
    if (seg.header().fin && _setLingerOnce) {
        if (sentFIN() == false) {
            _linger_after_streams_finish = false;
        }
        _setLingerOnce = false;
    }
    _msSinceLastRecv = 0;

    if (seg.header().ack) {
        WrappingInt32 ackno = seg.header().ackno;
        uint16_t window = seg.header().win;
        _sender.ack_received(ackno, window);
    }
    uint64_t curAckno = _receiver.getAbsolutSeqno();
    _sender.fill_window();
    if (prevAckno != curAckno && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    if (prevAckno == curAckno && unassembled_bytes() != prevUnassembleBytes && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    if (prevAckno == curAckno && seg.header().fin && _sender.segments_out().empty()) {  // resend ack of fin
        _sender.send_empty_segment();
    }
    pushOutgoingSegments(prevState);
}

bool TCPConnection::active() const {
    if (_recvRSTOrSomeWrong == true) {
        return false;
    }
    if (prereq123() == false) {
        return true;
    }
    if (_linger_after_streams_finish == true && _lingerDone == false) {
        return true;
    }
    return false;
}

size_t TCPConnection::write(const string &data) {
    DUMMY_CODE(data);
    size_t bytesWrite = _sender.stream_in().write(data);
    pushOutgoingSegments(state());
    return bytesWrite;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    if (active() == false) {
        return;
    }
    _msSinceLastRecv += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (state() == TCPState::State::LISTEN) {
        return;
    }
    pushOutgoingSegments(state());
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        sendRSTSeg();
    }
    if (_linger_after_streams_finish == true && prereq123() && _lingerDone == false) {
        _lingerTime += ms_since_last_tick;
        if (_lingerTime >= 10 * _cfg.rt_timeout) {
            _lingerDone = true;
        }
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    pushOutgoingSegments(state());
}

void TCPConnection::pushOutgoingSegments(const TCPState &prevState) {
    _sender.fill_window();
    while (_sender.segments_out().empty() == false) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        seg.header().ackno = _receiver.ackno().value_or(WrappingInt32(0));
        size_t remainCapa = _receiver.window_size();
        if (remainCapa > std::numeric_limits<uint16_t>::max()) {
            remainCapa = std::numeric_limits<uint16_t>::max();
        }
        seg.header().win = remainCapa;
        seg.header().ack = true;
        if (seg.header().syn == true &&
            (prevState == TCPState::State::CLOSED || prevState == TCPState::State::SYN_SENT)) {
            seg.header().ack = false;
        }
        _segments_out.push(seg);
#ifdef DEBUG_LOG
        logSend(seg);
#endif
    }
}

void TCPConnection::sendRSTSeg() {
#ifdef DEBUG_LOG
    _logMsg.push_back("sendRST");
#endif
    _recvRSTOrSomeWrong = true;
    _linger_after_streams_finish = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    TCPSegment rstSegment;
    rstSegment.header().rst = true;
    rstSegment.header().ackno = _receiver.ackno().value_or(WrappingInt32(0));
    rstSegment.header().seqno = _sender.next_seqno();
    _segments_out = std::queue<TCPSegment>();  // emp
#ifdef DEBUG_LOG
    logSend(rstSegment);
#endif
    _segments_out.push(rstSegment);
}

void TCPConnection::connect() {
    _sender.fill_window();
#ifdef DEBUG_LOG
    logSend(_sender.segments_out().front());
#endif
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
}

TCPConnection::~TCPConnection() {
#ifdef DEBUG_LOG
    printLog();
#endif
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            sendRSTSeg();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
