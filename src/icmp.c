#include "net.h"
#include "icmp.h"
#include "ip.h"

/**
 * @brief 发送 icmp 响应
 *
 * @param req_buf 收到的 icmp 请求包
 * @param src_ip 源 ip 地址
 */
static void icmp_resp(buf_t *req_buf, uint8_t *src_ip)
{
    // TO-DO

    // Step1
    // 调用 buf_init() 来初始化 txbuf，然后封装报头和数据，数据部分可以拷贝来自接收的回显请求报文中的数据。

    // Step2
    // 填写校验和，ICMP 的校验和和 IP 协议校验和算法是一样的。

    // Step3
    // 调用 ip_out() 函数将数据报发送出去。
}

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_ip 源 ip 地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip)
{
    // TO-DO

    // Step1
    // 首先做报头检测，如果接收到的包长小于 ICMP 头部长度，则丢弃不处理。

    // Step2
    // 接着，查看该报文的 ICMP 类型是否为回显请求。

    // Step3
    // 如果是，则调用 icmp_resp() 函数回送一个回显应答（ping 应答）。
}

/**
 * @brief 发送 icmp 不可达
 *
 * @param recv_buf 收到的 ip 数据包
 * @param src_ip 源 ip 地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code)
{
    // TO-DO

    // Step1
    // 首先调用 buf_init() 来初始化 txbuf，填写 ICMP 报头首部。

    // Step2
    // 接着，填写 ICMP 数据部分，包括 IP 数据报首部和 IP 数据报的前 8 个字节的数据字段，填写校验和。

    // Step3
    // 调用 ip_out() 函数将数据报发送出去。
}

/**
 * @brief 初始化 icmp 协议
 *
 */
void icmp_init()
{
    net_add_protocol(NET_PROTOCOL_ICMP, icmp_in);
}