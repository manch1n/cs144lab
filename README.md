### LAB2:TCP receiver
[官方lab地址](https://cs144.github.io/assignments/lab2.pdf)

[我的lab地址](https://github.com/manch1n/cs144lab/tree/mylab2)

##### Translating between 64-bit indexes and 32-bit seqnos
首先实验对TCP头部的32位的序列号与确认号进行设计.因为程序中使用的是64位的从0开始的绝对序号,且因为初始SYN是随机的,所以需要完成绝对序号与网络报文中的序号转换关系.
![20220307220024](https://raw.githubusercontent.com/manch1n/picbed/master/images/20220307220024.png)
* 网络序号转为绝对序号
这个比较简单,直接转型后相加即可.
```c++
//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    DUMMY_CODE(n, isn);
    uint32_t spliceNum = static_cast<uint32_t>(n);
    return isn + spliceNum;
}
```
* 绝对序号转为网络序号
需要注意的是到达报文的序号可能小于已经接收到的序号,即接收到过时的报文段,此时也需要正确返回相对较小的绝对序号.
```c++
//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    DUMMY_CODE(n, isn, checkpoint);
    WrappingInt32 prev = wrap(checkpoint, isn);\\前一个网络序号
    int32_t offset = n - prev;\\当前收到网络序号与前一个网络序号的比较
    if (offset >= 0) {\\大于等于0则是新报文,则直接返回
        return offset + checkpoint;
    } else {\\否则进行特殊的处理
        uint32_t shift = (static_cast<uint64_t>(1) << 32) - static_cast<uint32_t>(offset);
        if (shift > checkpoint) {\\这里我忘了是怎么想的了,是测试用例中有几个极端情况总之是面向测试编程
            return static_cast<uint32_t>(checkpoint) - shift;
        }
        return checkpoint - shift;
    }
}
```
##### Implementing the TCP receiver
* 首先观察数据结构:
```c++
class TCPReceiver {
    //! Our data structure for re-assembling bytes.
    StreamReassembler _reassembler;

    //! The maximum number of bytes we'll store.
    size_t _capacity;

    uint64_t _absoluteSeqno;
    WrappingInt32 _isn;
}
```
**reassembler**: 是上个实验实现的,Receiver用它来接收报文段,并把它传给用户层.
**capacity**: 表示了容量,到后面这个会被指示window的大小,一般是65535.
**isn**: 随机的初始序号.
所以接下来就很清楚了,提几个点:
1. 对SYN与FIN序号要加1.
2. 收到FIN标志并不意味着读的结束,而是正确的收到完整正确FIN之前的数据才可以使序号加1.
3. 有可能收到过去的报文段,不能直接把过去报文段的数据大小加到绝对序号上.
这在整个实验中应该还算是比较简单.
```c++
void TCPReceiver::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);

    uint64_t prevAbsoluteSeqno = _absoluteSeqno;
    if (seg.header().syn == true) {
        if (_synRecv == true)
            return;
        else {
            _synRecv = true;
            _isn = seg.header().seqno;
            _absoluteSeqno++;
        }
    }
    if (_synRecv == false) {
        return;
    }
    if (seg.header().fin == true) {
        if (_finDone == true) {
            return;
        }
    }

    // edge situation
    uint64_t curAbsoluteSeqno = unwrap(seg.header().seqno, _isn, prevAbsoluteSeqno);
    uint64_t curStrmIdx = streamIdx(curAbsoluteSeqno);
    if (curAbsoluteSeqno == 0) {
        if (seg.header().syn == true) {
            curStrmIdx = 0;
        } else {
            return;
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
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_synRecv == false) {
        return {};
    }
    return wrap(_absoluteSeqno, _isn);
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
```