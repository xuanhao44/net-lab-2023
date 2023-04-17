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
    // 首先判断数据长度，如果数据长度小于以太网头部长度，则认为数据包不完整，丢弃不处理

    // Step2
    // 调用 buf_remove_header() 函数移除加以太网包头

    // Step3
    // 调用 net_in() 函数向上层传递数据包
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

    // Step2
    // 调用 buf_add_header() 函数添加以太网包头

    // Step3
    // 填写目的 MAC 地址

    // Step4
    // 填写源 MAC 地址，即本机的 MAC 地址

    // Step5
    // 填写协议类型 protocol

    // Step6
    // 调用驱动层封装好的 driver_send() 发送函数，将添加了以太网包头的数据帧发送到驱动层
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
