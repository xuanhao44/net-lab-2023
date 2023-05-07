# UDP 协议

文章来自于：<https://github.com/xuanhao44/net-lab-2023>

## 1 UDP 协议概览

UDP 协议的内容和 UDP 报文的结构都比较简单。所以不多赘述。

## 2 结合实验框架的实现

这次实验要实现的 UDP 协议的部分很少，只有：

1. UDP 数据报输出处理 udp_out
2. UDP 数据报输入处理 udp_in
3. UDP 校验和 udp_checksum

可能写的过程比较简单，但是 Debug 的过程相当复杂——尤其是当前面 IP 和 ICMP 实验中有没有被框架测试发现的错误的时候。

### 2.1 udp_out

流程：

1. 首先调用 buf_add_header() 函数添加 UDP 报头。
2. 接着，填充 UDP 首部字段。
3. 先将校验和字段填充 0，然后调用 udp_checksum() 函数计算出校验和，再将计算出来的校验和结果填入校验和字段。
4. 调用 ip_out() 函数发送 UDP 数据报。

需要注意的点：

填入首部的 16bit 数据记得 swap16。

### 2.2 udp_in

流程：

1. 首先做包检查，检测该数据报的长度是否小于 UDP 首部长度，或者接收到的包长度小于 UDP 首部长度字段给出的长度，如果是，则丢弃不处理。

2. 接着重新计算校验和，先把首部的校验和字段保存起来，然后把该字段填充 0，调用 udp_checksum() 函数计算出校验和，如果该值与接收到的 UDP 数据报的校验和不一致，则丢弃不处理。

3. 调用 map_get() 函数查询 udp_table 是否有该目的端口号对应的处理函数（回调函数）。

4. 如果没有找到，则调用 buf_add_header() 函数增加 IPv4 数据报头部，再调用 icmp_unreachable() 函数发送一个端口不可达的 ICMP 差错报文。

5. 如果能找到，则去掉 UDP 报头，调用处理函数来做相应处理。

---

需要注意的点：

```c
uint16_t src_port16 = swap16(hdr->src_port16); // 函数返回值不可取地址
uint16_t dst_port16 = swap16(hdr->dst_port16);
udp_handler_t *handler = (udp_handler_t *)map_get(&udp_table, (void *)&dst_port16);
```

这里第三句，不直接在参数里填 `&swap16(hdr->dst_port16)` 的原因是函数的返回值不能取地址；`&函数名` 会被视为取函数本身的地址；但是，static inline 函数是无法被取地址的（这段话一共说了三件事）。

所以这里只能先存到变量里，然后传到 map_get 函数中。

### 2.3 udp_checksum

并不采用指导书上的写法。因为其 src_ip 和 dst_ip 会被加上去的伪头部覆盖掉，且头部加上去又卸下来实在太麻烦。

思考 udp_checksum 过程，可以发现实际上伪头部仅在这个函数中出现，那么很自然的想法是不改变 buf，可以直接复制一个 buf，然后在这个副本上操作，以避免覆盖的问题，以及不需要装卸的复杂过程。

流程如下：

```c
// S1 复制一整个
buf_t tmp_buf;
buf_copy(&tmp_buf, buf, sizeof(buf));
// S2 增加 UDP 伪头部
buf_add_header(&tmp_buf, sizeof(udp_peso_hdr_t));
// S3 填写伪头部
udp_peso_hdr_t *peso_hdr = (udp_peso_hdr_t *)tmp_buf.data;
memcpy(peso_hdr->src_ip, src_ip, NET_IP_LEN);
memcpy(peso_hdr->dst_ip, dst_ip, NET_IP_LEN);
peso_hdr->placeholder = 0;
peso_hdr->protocol = NET_PROTOCOL_UDP;
peso_hdr->total_len16 = swap16(buf->len);
// S4 计算 UDP 校验和，并返回
// UDP 校验和需要覆盖 UDP 头部、UDP 数据和一个伪头部。
return checksum16((uint16_t *)(tmp_buf.data), tmp_buf.len);
```

可以看到写法相当简洁，tmp_buf 直接用完就丢，也不需要考虑什么卸载掉伪头部的问题了。

## 3 实验结果和分析

命令行和测试工具的输入输出：

![udp_test](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/udp_test.jpg)

wireshark 捕获到的报文数据：（wireshark 的 [udp.pcap](../testing/data/udp.pcap)）

![udp_pcap](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/udp_pcap.png)

可以看到，通信的双方是：

- 192.168.56.1（测试工具）
- 192.168.56.45（框架）

第 5 组，框架首先广播发送了无回报 ARP 包（ARP announcemennt）声明了自己的 IP 和 MAC 地址。

第 6 组，测试工具向框架从 60000 端口到 60000 端口发送了包长为 27 的 UDP 包。

第 7 组，框架虽然接收到了该 UDP 包，但是在发回的时候框架并不知道测试工具的 MAC 地址，所以发送了 ARP request 来询问 192.168.56.1 的 MAC 地址。

第 8 组，测试工具进行了 ARP 应答。

第 9 组，这时，框架才成功的把 UDP 包发送给了测试工具。

第 10 组，可以注意到测试工具的 arp table 可能已经老化，所以又问了一次框架的 MAC。

第 11 组，框架进行了 ARP 应答。

第 12 - 23 组，都是测试工具和框架的互相收发过程：均为测试工具先发，然后框架收到后再发回去。

## 4 实验中遇到的问题及解决方法

遇到的问题主要是环境和 Debug。不过这两个问题直接也有互相的联系。

关于环境这一块，最开始我是使用的我自己电脑的 WSL 的 IP 来实验的，但是发现在这种情况下，测试工具发出的 UDP 包的 IP 的 checksum 都是 0。这意味着网络本身有问题——但是这个道理我在许久之后才想明白。

![udp_problem_1](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/udp_problem_1.png)

当时我在看到 wireshark 里捕获的数据时还以为是我发出的 UDP 包的 checksum 有问题！真是可笑。这说明我一开始的时候**对实验测试的原理不清楚**，且 wireshark 的信息也没有认真的看。

此外，我也不知道该如何去**调试 main 函数**，这也是个大问题。

之后，在老师的指点下，我知道了应该使用学校实验室电脑的虚拟机的网卡来做实验，以及如何调试 main 函数——这样我才站到了能够检查我代码问题的起点了。

视频是我学习以及 Debug 的过程：<https://www.bilibili.com/video/BV1824y1T7ja/>

---

在经过调试之后发现了两个大小端的问题，以及 checksum 函数在先前就写错的问题。

视频是 debug 大小端的过程：<https://www.bilibili.com/video/BV1pX4y127dE/>

在 Debug 结束之后，发现其实和原先更改的地方确实不多，但是错误本身就是很小很小的点，不能应为只有很小一处就觉得这无关紧要，而调试就是一种最合适的解决 bug 的办法。在这次调试的过程中我自认为又学到了很多。

## 5 意见和建议

希望老师下一次补录调试 main 函数的更具体的视频。
