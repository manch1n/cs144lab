#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

TCPSegment TCPSender::constructNormSegment(std::string &&data, bool synFlag, bool finFlag) {
    TCPHeader header;
    header.syn = synFlag;
    header.fin = finFlag;
    header.seqno = next_seqno();
    Buffer buf(std::move(data));
    TCPSegment ret;
    ret.header() = header;
    ret.payload() = buf;
    return ret;
}

void TCPSender::pushNewSegment(const TCPSegment &segment) {
    _segments_out.push(segment);
    _unackedSegments.push_back(segment);
    _next_seqno += segment.payload().size();
    _next_seqno += static_cast<uint64_t>(segment.header().syn);
    _next_seqno += static_cast<uint64_t>(segment.header().fin);
}

void TCPSender::tryClearUnackedSegments(uint64_t newAckno) {
    for (auto seg = _unackedSegments.begin(); seg != _unackedSegments.end();) {
        uint64_t absoluteSeq = unwrap(seg->header().seqno, _isn, newAckno);
        if ((absoluteSeq + seg->length_in_sequence_space()) <= newAckno) {
            seg = _unackedSegments.erase(seg);
        } else {
            break;
        }
    }
}

// resend the earliest segment
void TCPSender::resend() {
    if (_unackedSegments.empty() == false) {
        _segments_out.push(_unackedSegments.front());
    }
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _lastAckno(0)
    , _timer(retx_timeout)
    , _unackedSegments() {
    _peerReceiveWindow = 0;
    _timer.setRingCallBack(std::bind(&TCPSender::resend, this));
}

uint64_t TCPSender::bytes_in_flight() const {
    size_t totalUnacked = 0;
    for (const auto &seg : _unackedSegments) {
        totalUnacked += seg.payload().size();
        totalUnacked += static_cast<size_t>(seg.header().syn);
        totalUnacked += static_cast<size_t>(seg.header().fin);
    }
    return totalUnacked;
}

void TCPSender::fill_window() {
    if (_synSent == false) {
        _synSent = true;
        TCPSegment synseg = constructNormSegment("", true, false);
        pushNewSegment(synseg);
    }
    size_t usedWindow = _next_seqno - _lastAckno;
    size_t couldUseWindow = _peerReceiveWindow > usedWindow ? (_peerReceiveWindow - usedWindow) : 0;
    if (_peerReceiveWindow == 0 && _synAck == true && _zeroWindowAckTimes == 1) {
        couldUseWindow = 1;
        _zeroWindowAckTimes++;
    }
    while (_stream.buffer_empty() == false && couldUseWindow) {
        size_t thisUseWindow =
            couldUseWindow > TCPConfig::MAX_PAYLOAD_SIZE ? TCPConfig::MAX_PAYLOAD_SIZE : couldUseWindow;
        std::string data = _stream.peek_output(thisUseWindow);
        _stream.pop_output(data.size());
        couldUseWindow -= data.size();

        bool finFlag = false;
        if (_stream.eof() && couldUseWindow && couldUseWindow && _sentFIN == false) {
            _sentFIN = true;
            finFlag = true;
        }

        TCPSegment segment = constructNormSegment(std::move(data), false, finFlag);
        pushNewSegment(segment);
    }
    if (_stream.eof() && couldUseWindow && _sentFIN == false) {
        _sentFIN = true;
        TCPSegment segment = constructNormSegment("", false, true);
        pushNewSegment(segment);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    DUMMY_CODE(ackno, window_size);
    uint64_t absoluteAckno = unwrap(ackno, _isn, _lastAckno);
    if (absoluteAckno > _next_seqno) {
        return;
    }
    if (absoluteAckno < _lastAckno) {
        return;
    } else if (absoluteAckno == _lastAckno) {
        _peerReceiveWindow = window_size;
    } else {
        _synAck = true;
        _zeroWindowAckTimes = 0;
        _lastAckno = absoluteAckno;
        _peerReceiveWindow = window_size;
        _timer.reset();
    }
    tryClearUnackedSegments(absoluteAckno);
    if (_peerReceiveWindow == 0) {
        _zeroWindowAckTimes++;
    } else {
        _zeroWindowAckTimes = 0;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    if (_unackedSegments.empty()) {
        return;
    }
    bool reInit = false;  // FIXME:when ended,close the timer
    if (_peerReceiveWindow == 0 && _synAck == true) {
        reInit = true;
    }
    _timer.tick(ms_since_last_tick, reInit);
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return static_cast<unsigned int>(_timer.getConsecutiveTimes());
}

void TCPSender::send_empty_segment() {
    TCPSegment emptySegment = constructNormSegment("");
    pushNewSegment(emptySegment);
    tryClearUnackedSegments(_lastAckno);
}
