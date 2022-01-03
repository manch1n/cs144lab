#### LAB1:stitching substrings into a byte stream
[官方lab地址](https://cs144.github.io/assignments/lab1.pdf)

[我的lab地址](https://github.com/manch1n/cs144lab/tree/mylab1)

实现一个reassembler,用于之后的TCP Reciever接收报文.
假设收到了网络中乱序的报文,比如先收到序号为100,然后收到99,98...97序号的报文,如何将这些报文正确的重组就是这个实验做的内容.
##### 考虑的数据结构
* 数组(vector)
需要考虑非按序到达的情况,假设把之前的非按序报文的已经按序插入到vector中,又来一个非按序到达的报文,那么插入操作需要O(N)的复杂度先进行移动,然后再插入.考虑最坏的情况,N个非按序到达的报文需要进行N^2次移动,too bad.
* 链表(list)
链表可以以O(N)的时间进行查找后,可以以O(1)的时间进行插入,好处是不需要进行移动之前的元素.是一个可以考虑的结构.
* 哈希(unorder_map)
缺点是无序,在之后merge都需要重排序一次.
* 红黑树(map)
红黑树的插入代价为O(logN),也不需要移动元素,且自有序.缺点是有必要的红黑标记,指针等.显然优于以上三者.
##### 考虑在idx=2 size=1的报文到达后如何merge,并且将数据放到bytestream中?
![20220103155102](https://raw.githubusercontent.com/manch1n/picbed/master/images/20220103155102.png)
遍历该map,将连续的片段合并:
```c++
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
```
#### 注意
当收到带有EOF标志的报文段时若该报文段没被处理,需要记录该EOF的字节索引并不end_input
```c++
void StreamReassembler::checkEOF() {
    if (pushedBytes() + totalRead() == _eofSize) {
        _output.end_input();
    }
}

```