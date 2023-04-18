# eth 以太网实验

文章来自于：https://github.com/xuanhao44/net-lab-2023

## 1 对结构和功能的概览

根据协议栈架构，eth_in 就是收到从 IP 层发来的数据包，为它加上数据链路层的封装，然后送到物理层；eth_out 就是收到从物理层发来的数据包，拆解其数据链路层的封装，然后送到 IP 层。

数据链路层的封装：去除报头和 FCS，剩余的部分是数据链路层的**数据帧**。

如图所示：

![net_lab_eth_structure](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_eth_structure.png)

## 2 结合实验框架的实现

### 2.1 流程

#### 2.1.1 eth_in

1. 判断数据长度，如果数据长度小于以太网头部长度，则认为数据包不完整，丢弃不处理；
2. 调用 buf_remove_header() 函数移除以太网包头；
3. 调用 net_in() 函数向上层传递数据包。

#### 2.1.2 eth_out

1. 判断数据长度，如果不足 46 则显式填充 0，填充可以调用 buf_add_padding() 函数来实现；
2. 调用 buf_add_header() 函数添加以太网包头 ether_hdr_t；
3. 填写目的 MAC 地址、填写源 MAC 地址（本机的 MAC 地址）、填写协议类型 protocol；
4. 调用驱动层封装好的 driver_send() 发送函数，将添加了以太网包头的数据帧发送到驱动层。

### 2.2 实现注意点

#### 2.2.1 关于框架

1. 移除 eth 报头，实验框架里给数据报结构 buf 添加了便捷的工具函数：如 buf_remove_header() 函数，给出 buf 地址和要减少的长度，就能通过指针的移动改变数据报的大小。

2. net_if_mac

```c
memcpy(hdr->src, net_if_mac, NET_MAC_LEN);
```

这里大写的 NET_IF_MAC 不能用，要用小写的 net_if_mac。大写的是字面量，小写的是地址，memcpy 要填地址。

#### 2.2.2 关于 C 语言

1. memcpy

怎么把 mac、protocol 填到 header 里去？使用 memcpy。

比如：

```c
memcpy(mac, hdr->src, NET_MAC_LEN);
```

需注意前两个参数是指针或地址。

2. 注意野指针

看这段代码：

```c
// 先把 mac(源 MAC 地址) 和 protocol 从首部中取出来
ether_hdr_t *hdr = (ether_hdr_t *)buf->data;

// （中间的代码省略）

// 调用 buf_remove_header() 函数移除以太网包头
buf_remove_header(buf, sizeof(ether_hdr_t));

// 调用 net_in() 函数向上层传递数据包
net_in(buf, hdr->protocol16, hdr->src);
```

需要注意：在 remove 之后，hdr 指向的地址就不再有效，故不应该继续被使用。所以 net_in 的参数是不对的。

#### 2.2.3 大端小端问题

小端字节序：低字节存储于内存低地址，高字节存储于内存高地址。
大端字节序：高字节存储于内存低地址，低字节存储于内存高地址。

按照 TCP/IP 协议规定：网络字节序是大端字节序。但是，X86 平台上是以小端字节序存储。

所以，在发送之前我们需要将小端存储的字节序转换成大端存储的数值；而在接收时，也需要将大端序转成小端序存放的数值。

除此之外，也要注意对象：**多字节数值**。在这部分中只有 protocol 是 16 位的 2 字节数值，所以只有它需要转换。

```c
protocol = swap16(hdr->protocol16);
```

与之形成对比的就是 uint8 数组的 mac 地址，数组这样连续存储的数据是不会被大小端影响的，同时 uint8 也只有一个字节，自然也和大小端没关系——大小端是以字节为单位进行翻转的。

*我在一开始看到大小端的这个问题的时候，还以为整个数据包的数据都需要大小端转换，说明还不够理解大小端的概念。*

## 3 实验结果和分析

结果如下：

eth_in:

![net_lab_eth_in_result](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_eth_in_result.png)

eth_out:

![net_lab_eth_out_result](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_eth_out_result.png)

### 3.1 eth_in 的 in.pcap 分析

选取第一组分析：

![net_lab_eth_in_pcap](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_eth_in_pcap.png)

（pcap 里没看到数据链路层的报头和 FCS，只有数据帧）

可以看到前面提到的 MAC 头部。

### 3.2 eth_out 的 out.pcap 分析

![net_lab_eth_out_pcap](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_eth_out_pcap.png)

可以看到，上层的数据只有 40 字节，故该数据包在传到数据链路层再发往物理层的时候添加了 6 字节的 Padding，以达到最小的数据长度。

## 4 感想

在写实验的同时更深刻记住了一些计网知识。不好意思的说，我在没做实验之前对 MAC 地址只有一点模糊的“是物理地址”的概念，现在知道了更多的内容，比如现在说起 MAC，我会记得是 6 字节 48 比特的地址，典例如广播 MAC 地址 FF:FF:FF:FF。
