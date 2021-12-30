#### ByteStream
完成一个基于内存且固定大小的字节流，之后的很多组件都是基于bytestream的。
##### 思路：
* 最简单的方法是用queue管理这些传进来的string，并且维护大小，但这也显而易见的是性能的降低。
```c++
size_t ByteStream::write(const string &data)
```
接口的形参为cr，queue基于deque，导致了多余的构造析构分配内存开销。
*  基于一个固定大小vector的循环队列,维护start,end两个指针和队内大小size，当有数据到来时end+=data.size()，当需要取出数据时start+=popSize。用户可取的空间为[start,end)。一般来说，分为4种情况：
**1)start==end && size\==0**
显然这时候stream为空
**2)start==end && size\==capacity()**
显然这个时候为满
**3)start < end**
这时候可取数据区间为[start,end)
**4)start > end**
这时候数据可取区间为[start,capacity())与[0,end),图片中红色部分才为可取用数据。
![20211230181830](https://raw.githubusercontent.com/manch1n/picbed/master/images/20211230181830.png)

#####  结果
![20211230182546](https://raw.githubusercontent.com/manch1n/picbed/master/images/20211230182546.png)

##### 我的github地址 觉得可以的给个star吧:)
[我的github](https://github.com/manch1n/cs144lab)