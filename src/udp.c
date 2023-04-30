#include "udp.h"
#include "ip.h"
#include "icmp.h"

/**
 * @brief udp 处理程序表
 *
 */
map_t udp_table;

/**
 * @brief udp 伪校验和计算
 *
 * @param buf 要计算的包
 * @param src_ip 源 ip 地址
 * @param dst_ip 目的 ip 地址
 * @return uint16_t 伪校验和
 */
static uint16_t udp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dst_ip)
{
    // TO-DO

    // Step1
    // 首先调用 buf_add_header() 函数增加 UDP 伪头部。

    // Step2
    // 将被 UDP 伪头部覆盖的 IP 头部拷贝出来，暂存 IP 头部，以免被覆盖。

    // Step3
    // 填写 UDP 伪头部的 12 字节字段。

    // Step4
    // 计算 UDP 校验和。

    // Step5
    // 再将 Step2 中暂存的 IP 头部拷贝回来。

    // Step6
    // 调用 buf_remove_header() 函数去掉 UDP 伪头部。

    // Step7
    // 返回计算出来的校验和值。
}

/**
 * @brief 处理一个收到的 udp 数据包
 *
 * @param buf 要处理的包
 * @param src_ip 源 ip 地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip)
{
    // TO-DO

    // Step1
    // 首先做包检查，检测该数据报的长度是否小于 UDP 首部长度，或者接收到的包长度小于 UDP 首部长度字段给出的长度，如果是，则丢弃不处理。

    // Step2
    // 接着重新计算校验和，先把首部的校验和字段保存起来，然后把该字段填充 0，调用 udp_checksum() 函数计算出校验和，如果该值与接收到的 UDP 数据报的校验和不一致，则丢弃不处理。

    // Step3
    // 调用 map_get() 函数查询 udp_table 是否有该目的端口号对应的处理函数（回调函数）。

    // Step4
    // 如果没有找到，则调用 buf_add_header() 函数增加 IPv4 数据报头部，再调用 icmp_unreachable() 函数发送一个端口不可达的 ICMP 差错报文。

    // Step5
    // 如果能找到，则去掉 UDP 报头，调用处理函数来做相应处理。
}

/**
 * @brief 处理一个要发送的数据包
 *
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dst_ip 目的 ip 地址
 * @param dst_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port)
{
    // TO-DO

    // Step1
    // 首先调用 buf_add_header() 函数添加 UDP 报头。

    // Step2
    // 接着，填充 UDP 首部字段。

    // Step3
    // 先将校验和字段填充 0，然后调用 udp_checksum() 函数计算出校验和，再将计算出来的校验和结果填入校验和字段。

    // Step4
    // 调用 ip_out() 函数发送 UDP 数据报。
}

/**
 * @brief 初始化 udp 协议
 *
 */
void udp_init()
{
    map_init(&udp_table, sizeof(uint16_t), sizeof(udp_handler_t), 0, 0, NULL);
    net_add_protocol(NET_PROTOCOL_UDP, udp_in);
}

/**
 * @brief 打开一个 udp 端口并注册处理程序
 *
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为 0，失败为 -1
 */
int udp_open(uint16_t port, udp_handler_t handler)
{
    return map_set(&udp_table, &port, &handler);
}

/**
 * @brief 关闭一个 udp 端口
 *
 * @param port 端口号
 */
void udp_close(uint16_t port)
{
    map_delete(&udp_table, &port);
}

/**
 * @brief 发送一个 udp 包
 *
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dst_ip 目的 ip 地址
 * @param dst_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port)
{
    buf_init(&txbuf, len);
    memcpy(txbuf.data, data, len);
    udp_out(&txbuf, src_port, dst_ip, dst_port);
}