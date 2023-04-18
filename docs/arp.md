# ARP 协议

文章来自于：https://github.com/xuanhao44/net-lab-2023

## 1 ARP 协议概览

ARP 协议以目标 IP 地址为线索，用来定位下一个应该接收数据包的网络设备对应的 MAC 地址。如果目标主机不在同一个链路上，可以通过 ARP 查找下一跳网关的 MAC 地址。注意，ARP 只适用于 IPv4，不能用于 IPv6。IPv6 可以用 ICMPv6 替代 ARP 发送邻居探索消息。

ARP 是借助 ARP 请求与 ARP 响应两种类型的包确定 MAC 地址的。个人认为同一网段的 ARP 解析和不同网段的 ARP 解析过程区别没那么大。不同网段的 ARP 就是需要经过各自的网关的转发而已。

ARP 协议报文格式略，详见指导书。

ARP 表：设备通过 ARP 解析到目的 MAC 之后，将会在自己的 ARP 映射表中增加 IP 地址到 MAC 地址的映射表，以用于后续到同一目的地数据帧的转发。ARP 表项分为动态 ARP 表项和静态 ARP 表项。在实验中，我们需要实现 动态 ARP 表项。动态 ARP 表项由 ARP 协议通过 ARP 报文自动生成和维护，可以被老化，可以被新的 ARP 报文更新，也可以被静态 ARP 表项所覆盖。当到达老化时间或接口关闭时会删除相应的动态 ARP 表项。

## 2 结合实验框架的实现

要完成四个函数：arp_req、arp_out、arp_in、arp_resp。

- arp_req：发送一个 arp 请求
- arp_resp：发送一个 arp 响应
- arp_in：处理一个收到的数据包
- arp_out：处理一个要发送的数据包

你会发现理论知识只和 arp_req 和 arp_resp 有关，依靠前面的知识确实也能写出这两个部分。

那 arp_in 和 arp_out 到底在干啥？指导书这块写的是真的差——我是写完总结才弄清楚他俩干啥的。也许指导书给出了详细的流程，但是关键的目的我是没看见它提到。

下面的具体部分里我会讲解。

### 2.1 arp_req

1. 调用 buf_init() 初始化一个数据包。

2. 填写 ARP 报头。

3. ARP 操作类型为 ARP_REQUEST，注意大小端转换。
4. 调用 ethernet_out 函数将 ARP 报文发送出去。注意：ARP announcement 或 ARP 请求报文都是广播报文，其目标 MAC 地址应该是广播地址：FF-FF-FF-FF-FF-FF。

### 2.2 arp_resp

与 arp_req 类似，不同点在于 response 不是广播，且需要补上包的 target_mac。

1. 首先调用 buf_init() 来初始化 txbuf。

2. 接着，填写 ARP 报头首部。

3. ARP 操作类型为 ARP_REPLY，注意大小端转换。

4. 调用 ethernet_out() 函数将填充好的 ARP 报文发送出去。

### 2.3 arp_out 处理一个要发送的数据包

流程：

1. 调用 map_get() 函数，根据 IP 地址来查找 ARP 表 (arp_table)。

2. 如果能找到该 IP 地址对应的 MAC 地址，则将数据包直接发送给以太网层，即调用 ethernet_out 函数直接发出去。

3. 如果没有找到对应的 MAC 地址，进一步判断 arp_buf 是否已经有包了，

   如果有，则说明正在等待该 ip 回应 ARP 请求，此时不能再发送 ARP 请求；

   如果没有包，则调用 map_set() 函数将来自 IP 层的数据包缓存到 arp_buf，然后，调用 arp_req() 函数，发一个请求目标 IP 地址对应的 MAC 地址的 ARP request 报文。

解释：

首先回到 arp 请求和 arp 应答到底为了什么：拿着 ip 地址问 mac 地址——不仅如此，是**为了向这个 ip 地址发数据包**。

应该还记得之前提到的动态 ARP 表项，也就是 <ip,mac> 的键值对，在该框架中是 arp_table。

很明显在发数据包前需要到表中找这个对应是否存在，如果有的话就可以直接发了；如果没有的话就需要发 arp request 去询问。

然后这里就有一个小小的疑惑点：你发现了一个叫 arp_buf 的东西，然后本来不知道有什么用，就莫名其妙的在 arp_out 的流程中用起来了。实际上他就是我原本准备要发出去的报文！因为不知道目的 mac 地址而先发一个 arp request，所以本来要发的报文就先缓存到 arp_buf 里了。

这时你再看流程，你会发现它把**”arp_buf 是否有包“当作 arp request 发出但仍未收到 response 的状态的判断依据**。所以如果有缓存，那么就不再发，如果没缓存，那么就发一个 request。

*不仔细读就不能总结出来，太鸡贼了！*

同时你要注意 arp_out 里对缓存的操作只有 set 一种，delete 不存在——delete 缓存这种操作自然是等到了 response，得到了 目标 mac 地址，把缓存的包发出去之后顺手做的事情。

### 2.4 arp_in 处理一个收到的数据包

流程：

1. 首先判断数据长度，如果数据长度小于 ARP 头部长度，则认为数据包不完整，丢弃不处理。

2. 接着，做报头检查，查看报文是否完整，检测内容包括：ARP 报头的硬件类型、上层协议类型、MAC 硬件地址长度、IP 协议地址长度、操作类型，检测该报头是否符合协议规定。

3. 调用 map_set() 函数更新 ARP 表项。

4. 调用 map_get() 函数查看该接收报文的 IP 地址是否有**对应的** arp_buf 缓存。

   如果有，则说明 ARP 分组队列里面有待发送的数据包。也就是上一次调用 arp_out() 函数发送来自 IP 层的数据包时，由于没有找到对应的 MAC 地址进而先发送了 ARP request 报文，此时收到了该 request 的应答报文。然后，将缓存的数据包 arp_buf 再发送给以太网层，即调用 ethernet_out() 函数直接发出去，接着调用 map_delete() 函数将这个缓存的数据包删除掉。

   如果该接收报文的 IP 地址没有对应的 arp_buf 缓存，还需要判断接收到的报文是否为 ARP_REQUEST 请求报文，并且该请求报文的 target_ip 是本机的 IP，则认为是请求本主机 MAC 地址的 ARP 请求报文，则调用 arp_resp() 函数回应一个响应报文。

解释：

1 - 3：首先当然是确认合法性以及是否是 arp 包，这都很合理。一旦是 arp 包，那就把这个包携带的 <ip, mac> 信息存到自己的 ARP 表项中——注意**不管是 arp request 还是 arp response，其 sender 的 ip 和 mac 信息都是全部都有的**。

4：看自己的 arp_buf 缓存里的包是不是要发给这个主机的，也就是确认一下，用这个收到的数据包的 sender_ip 是否能对上号。

**注意不关心缓存里是不是有，而是关心是不是有对应的**，虽然没有缓存的话也就谈不上对应了，但确实有区别。

如果有（对应 ip 的包）的话，那么这个就（相当于）是 response 报文。那么之后的操作就是：发出这个缓存包，然后删除缓存。

如果没有（对应 ip 的包）的话，那么就再看看是不是 target_ip 为本机 ip 的 request 报文，是的话就 response 一下。

# 3 实验结果和分析

结果如下：

![net_lab_arp_test_result](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_arp_test_result.png)

in.pcap 和 out.pcap 部分分析：

![net_lab_arp_test_in_out_pcap](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_arp_test_in_out_pcap.png)

out 和 in 交替分析：

out 组 1 是主机的**无回报 ARP 包（ARP announcement）**：用于昭示天下（LAN）本机要使用某个 IP 地址了，是一个 Sender IP 和 Traget IP 填充的都是本机 IP 地址的 ARP request。故点开组 1 就可以看到 Sender IP 和 Traget IP 都是 192.168.163.103。

in 组 1 是主机向 IP 地址为 192.168.163.10 的 DNS 服务器发送 DNS 请求，但是主机不知道该 DNS 服务器的 mac 地址（于是乎缓存了该包）。

out 组 2 是主机为了知道 192.168.163.10 的 mac 地址而发送的 arp 请求报文（是广播报文）。

in 组 2 是前面提到的 IP 地址为 192.168.163.10 的 DNS 服务器向主机（mac 地址为 11:22:33:44:55:66）发送的 arp 应答报文，说明自己的 mac 地址为 21:32:43:54:65:06。

out 组 3 是把先前缓存的 out 组 1 发出了。
