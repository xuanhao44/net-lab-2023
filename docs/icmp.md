# ICMP 协议

文章来自于：<https://github.com/xuanhao44/net-lab-2023>

## 1 ICMP 协议概览

同样是主要关注 ICMP 报文的结构。

1. ICMP 报文是在 IP 数据报内被封装传输的。也就是说结构总是 ICMP 报文外面一层 IPv4 的头部。
2. ICMP 的消息大致可以分为两类：一类是差错报文，另一类是询问或信息报文。两种报文的组成不同，注意区分。

## 2 结合实验框架的实现

这次实验要实现的 ICMP 协议的部分很少，只有：

1. ICMP 数据报输入处理 icmp_in
2. 发送 ICMP 响应报文 icmp_resp
3. ICMP 数据报输出处理 icmp_unreachable

可以说比较简单。

*由于 icmp_in 过于简单，故不在下面列出，详细见源码。*

### 2.1 icmp_resp

要发送 ICMP 响应报文，而收到的是 ICMP 请求包。

由指导书可知，这两者结构是相同的。且应答报文的数据大多数都是拷贝请求包中的内容。只有类型需要改变，以及校验和需要重新计算。至于 type，可由指导书得知，不管是回显请求还是回显应答，都是 0，所以也不需要改变。

再考虑使用框架所给的 buf 相关的函数 buf_copy，可用来直接复制 buf，那么很容易就有下面的思路。

*显然我们还是要创建一个 buf_t 的，毕竟不能改动传进来的 req_buf 指针。*

```c
static void icmp_resp(buf_t *req_buf, uint8_t *src_ip)
{
    // S1 组装响应报文
    buf_t txbuf;
    buf_copy(&txbuf, req_buf, req_buf->len); // 直接拷贝！
    icmp_hdr_t *resp_hdr = (icmp_hdr_t *)txbuf.data;
    resp_hdr->type = ICMP_TYPE_ECHO_REPLY;

    // S2 填写校验和
    // ICMP 的校验和和 IP 协议校验和算法是一样的。
    // 但一定注意是覆盖整个报文，不是只有首部！
    resp_hdr->checksum16 = 0;
    resp_hdr->checksum16 = checksum16((uint16_t *)txbuf.data, txbuf.len);

    // S3 调用 ip_out() 函数将数据报发出。
    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}
```

---

需要注意的点：

ICMP 的校验和和 IP 协议校验和算法是一样的。但一定注意是覆盖整个报文，不是只有首部！

### 2.2 icmp_unreachable

要发送 ICMP 不可达的数据包，而收到的是 IP 数据包。

由指导书图的 ICMP 差错报文结构，可看出需要截取一部分 IP 数据包（首部 + 8 字节），然后再加上 ICMP 首部。所以流程大致为：

1. 差错报文数据：使用收到的 IP 报头与其报文前 8 字节。
2. 添加 ICMP 报头。
3. 计算校验和，范围为整个 ICMP 报文。
4. 发送数据包。

---

需要注意的点：

ICMP 差错报文的首部的构成和回显报文不同，id 与 seq 字段在差错报文中未用，必须为 0。

## 3 实验结果和分析

结果如下：

icmp_test：![net_lab_icmp_result](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_icmp_result.png)

### pcap 分析

![net_lab_icmp_in_out_pcap](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_icmp_in_out_pcap.png)

1. in 的第 8 组，收到了一个 ICMP ping request。

   out 的第 7 组，发送了一个 ICMP ping reply。

   in 的第 11 组，收到了一个 ICMP ping reply。

2. out 的第 9 组，发送了一个 ICMP ping reply。

3. out 的第 10 和 13 组，各发送了一个 ICMP unreachable，查看得知是因为使用了 TCP 协议，这是 IP 实验中就设置好的，很正常。

---

分析 1：

|                      | in 的第 8 组 ICMP request | out 的第 7 组 ICMP reply |
| -------------------- | ------------------------- | ------------------------ |
| Type                 | 8 (Echo (ping) request)   | 0 (Echo (ping) reply)    |
| Code                 | 0                         | 0                        |
| Checksum             | 0x3b6a [correct]          | 0x436a [correct]         |
| Checksum Status      | Good                      | Good                     |
| Identifier (BE)      | 1 (0x0001)                | 1 (0x0001)               |
| Identifier (LE)      | 256 (0x0100)              | 256 (0x0100)             |
| Sequence Number (BE) | 1 (0x0001)                | 1 (0x0001)               |
| Sequence Number (LE) | 256 (0x0100)              | 256 (0X100)              |
| Data                 | (56 bytes)                | (56 bytes)               |

Data 经过检查也是相同的。

## 4 实验中遇到的问题及解决方法

遇到的最大问题是之前写的 IP 的代码错误太多，导致就算 ICMP 的代码写的是对的也无法通过 ICMP 的测试。更糟糕的是，在刚写完 ICMP 的时候，我并不知道这一点，还在傻傻的一遍遍调试 ICMP。最后回看 IP 代码发现了一些问题，然后在课程群里提问得到了同学的解答才意识到要修改 IP 代码，改正错误后才通过 ICMP 测试。

## 5 意见和建议

应该开设课程答疑平台，比如 Pizza。在 QQ 群内的交流实在是过于低效。
