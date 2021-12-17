#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t ByteStream::capacity() const { return _buf.size(); }

ByteStream::ByteStream(const size_t capacity)
    : _buf(capacity), _start(0), _end(0), _totalWriten(0), _totalRead(0), _remainByte(0), _endInput(false) {
    DUMMY_CODE(capacity);
}

size_t ByteStream::write(const string &data) {
    DUMMY_CODE(data);
    if (buffer_size() == capacity()) {
        return 0;
    }
    size_t wantWrite = data.size();
    size_t actualWrite = remaining_capacity();
    if (wantWrite < actualWrite) {
        actualWrite = wantWrite;
    }
    if (_end + actualWrite > capacity()) {
        std::copy(data.begin(), data.begin() + capacity() - _end, _buf.begin() + _end);
        std::copy(data.begin() + capacity() - _end, data.end(), _buf.begin());
    } else {
        std::copy(data.begin(), data.end(), _buf.begin() + _end);
    }
    writeShrink(actualWrite);
    _totalWriten += actualWrite;
    _remainByte += actualWrite;
    return actualWrite;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    DUMMY_CODE(len);
    if (buffer_empty() == true) {
        return {};
    }
    size_t actualPeek = buffer_size();
    if (len < actualPeek) {
        actualPeek = len;
    }
    std::string result(actualPeek, '\0');
    if (_start < _end) {
        std::copy(_buf.begin() + _start, _buf.begin() + _start + actualPeek, result.begin());
    } else if (_start >= _end) {
        if ((_start + actualPeek) > capacity()) {
            size_t offset = (_start + actualPeek) - capacity();
            std::copy(_buf.begin() + _start, _buf.end(), result.begin());
            std::copy(
                _buf.begin(), _buf.begin() + offset, result.begin() + std::distance(_buf.begin() + _start, _buf.end()));
        } else {
            std::copy(_buf.begin() + _start, _buf.begin() + _start + actualPeek, result.begin());
        }
    }
    return result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    DUMMY_CODE(len);
    size_t actualPop = buffer_size();
    if (len < actualPop) {
        actualPop = len;
    }
    readExpand(actualPop);
    _totalRead += actualPop;
    _remainByte -= actualPop;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    DUMMY_CODE(len);
    std::string result = peek_output(len);
    pop_output(result.size());
    return result;
}

void ByteStream::end_input() { _endInput = true; }

bool ByteStream::input_ended() const { return _endInput; }

size_t ByteStream::buffer_size() const { return _remainByte; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _totalWriten; }

size_t ByteStream::bytes_read() const { return _totalRead; }

size_t ByteStream::remaining_capacity() const { return capacity() - buffer_size(); }
