#include "ethernet.h"
#include "utils.h"
#include "driver.h"
#include "arp.h"
#include "ip.h"
/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 */
void ethernet_in(buf_t *buf)
{
    // TO-DO

    // Step1
    // 判断数据长度，如果数据长度小于以太网头部长度，则认为数据包不完整，丢弃不处理
    if (buf->len < sizeof(ether_hdr_t)) {
        return;
    }
    // 在接收时，也需要将大端序转成小端序存放的数值
    // #define swap16(x) ((((x)&0xFF) << 8) | (((x) >> 8) & 0xFF)) // 为 16 位数据交换大小端
    // 如果不需要这个阶段处理的数据，就直接传输；如果不超过 1 字节，也可以直接取出数据。
    // 在开始的 eth 实验中，ethernet_out 和 ethernet_in 都还未使用这些数据，包括 protocol, mac 在内
    // 故而不先着急使用大小端转换

    ether_hdr_t *hdr = (ether_hdr_t *)buf->data;

    // Step2
    // 调用 buf_remove_header() 函数移除加以太网包头
    buf_remove_header(buf, sizeof(ether_hdr_t));

    // Step3
    // 调用 net_in() 函数向上层传递数据包
    net_in(buf, hdr->protocol16, hdr->src);
}
/**
 * @brief 处理一个要发送的数据包
 *
 * @param buf 要处理的数据包
 * @param mac 目标 MAC 地址
 * @param protocol 上层协议
 */
void ethernet_out(buf_t *buf, const uint8_t *mac, net_protocol_t protocol)
{
    // TO-DO

    // Step1
    // 判断数据长度，如果不足 46 则显式填充 0
    // 填充可以调用 buf_add_padding() 函数来实现
    if (buf->len < ETHERNET_MIN_TRANSPORT_UNIT) // 这个常数就是 46
    {
        buf_add_padding(buf, ETHERNET_MIN_TRANSPORT_UNIT - buf->len);
    }

    // Step2
    // 调用 buf_add_header() 函数添加以太网包头 ether_hdr_t
    // 下面就是参考写法：
    buf_add_header(buf, sizeof(ether_hdr_t)); // 为 buffer 在头部增加协议头长度的空间
    ether_hdr_t *hdr = (ether_hdr_t *)buf->data;

    // Step3
    // 填写目的 MAC 地址
    memcpy(hdr->dst, mac, NET_MAC_LEN);
    // or *hdr->dst = *mac;

    // Step4
    // 填写源 MAC 地址，即本机的 MAC 地址
    memcpy(hdr->src, net_if_mac, NET_MAC_LEN);
    // 大写的似乎不能用，用了小写的 net_if_mac

    // Step5
    // 填写协议类型 protocol
    hdr->protocol16 = protocol;

    // Step6
    // 调用驱动层封装好的 driver_send() 发送函数，将添加了以太网包头的数据帧发送到驱动层
    driver_send(buf);
    // 在发送之前我们需要将小端存储的字节序转换成大端序存储的数值
    // #define swap16(x) ((((x)&0xFF) << 8) | (((x) >> 8) & 0xFF)) // 为 16 位数据交换大小端
    // 如果不需要这个阶段"处理"的数据，就直接传输；如果不超过 1 字节，也可以直接取出数据。
    // 在开始的 eth 实验中，ethernet_out 和 ethernet_in 都还未使用这些数据，包括 protocol, mac 在内
    // 故而不先着急使用大小端转换
}
/**
 * @brief 初始化以太网协议
 *
 */
void ethernet_init()
{
    buf_init(&rxbuf, ETHERNET_MAX_TRANSPORT_UNIT + sizeof(ether_hdr_t));
}

/**
 * @brief 一次以太网轮询
 *
 */
void ethernet_poll()
{
    if (driver_recv(&rxbuf) > 0)
        ethernet_in(&rxbuf);
}
