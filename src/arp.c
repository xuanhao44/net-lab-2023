#include <string.h>
#include <stdio.h>
#include "net.h"
#include "arp.h"
#include "ethernet.h"
/**
 * @brief 初始的 arp 包
 *
 * 很明显并不是每一项都被赋值了
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type16 = constswap16(ARP_HW_ETHER),     // 值为 1 时表示为以太网地址
    .pro_type16 = constswap16(NET_PROTOCOL_IP), // 映射 IP 地址时的值为 0x0800。
    .hw_len = NET_MAC_LEN,                      // 标识 MAC 地址长度
    .pro_len = NET_IP_LEN,                      // 标识 IP 地址长度
    .sender_ip = NET_IF_IP,                     // 标识发送方设备的 IP 地址
    .sender_mac = NET_IF_MAC,                   // 标识发送设备的硬件地址
    .target_mac = {0}};                         // 表示接收方设备的硬件地址，在请求报文中该字段值全为 0 表示任意地址，因为现在不知道。

/**
 * @brief arp 地址转换表，<ip,mac>的容器
 *
 */
map_t arp_table;

/**
 * @brief arp buffer，<ip,buf_t>的容器
 *
 */
map_t arp_buf;

/**
 * @brief 打印一条 arp 表项
 *
 * @param ip 表项的 ip 地址
 * @param mac 表项的 mac 地址
 * @param timestamp 表项的更新时间
 */
void arp_entry_print(void *ip, void *mac, time_t *timestamp)
{
    printf("%s | %s | %s\n", iptos(ip), mactos(mac), timetos(*timestamp));
}

/**
 * @brief 打印整个 arp 表
 *
 */
void arp_print()
{
    printf("===ARP TABLE BEGIN===\n");
    map_foreach(&arp_table, arp_entry_print);
    printf("===ARP TABLE  END ===\n");
}

/**
 * @brief 发送一个 arp 请求
 *
 * @param target_ip 想要知道的目标的 ip 地址
 */
void arp_req(uint8_t *target_ip)
{
    // TO-DO

    // Step1
    // 调用 buf_init() 对 txbuf 进行初始化。

    // Step2
    // 填写 ARP 报头。

    // Step3
    // ARP 操作类型为 ARP_REQUEST，注意大小端转换。

    // Step4
    // 调用 ethernet_out 函数将 ARP 报文发送出去。
    // 注意：ARP announcement 或 ARP 请求报文都是广播报文，其目标 MAC 地址应该是广播地址：FF-FF-FF-FF-FF-FF。
}

/**
 * @brief 发送一个 arp 响应
 *
 * @param target_ip 目标 ip 地址
 * @param target_mac 目标 mac 地址
 */
void arp_resp(uint8_t *target_ip, uint8_t *target_mac)
{
    // TO-DO

    // Step1
    // 首先调用 buf_init() 来初始化 txbuf。

    // Step2
    // 接着，填写 ARP 报头首部。

    // Step3
    // 调用 ethernet_out() 函数将填充好的 ARP 报文发送出去。
}

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源 mac 地址
 */
void arp_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO

    // Step1
    // 首先判断数据长度，如果数据长度小于 ARP 头部长度，则认为数据包不完整，丢弃不处理。

    // Step2
    // 接着，做报头检查，查看报文是否完整，检测内容包括：
    // ARP 报头的硬件类型、上层协议类型、MAC 硬件地址长度、IP 协议地址长度、操作类型，检测该报头是否符合协议规定。

    // Step3
    // 调用 map_set() 函数更新 ARP 表项。

    // Step4
    // 调用 map_get() 函数查看该接收报文的 IP 地址是否有对应的 arp_buf 缓存。

    // 如果有，则说明 ARP 分组队列里面有待发送的数据包。
    // 也就是上一次调用 arp_out() 函数发送来自 IP 层的数据包时，由于没有找到对应的 MAC 地址进而先发送的 ARP request 报文，此时收到了该 request 的应答报文。
    // 然后，将缓存的数据包 arp_buf 再发送给以太网层，即调用 ethernet_out() 函数直接发出去，接着调用 map_delete() 函数将这个缓存的数据包删除掉。

    // 如果该接收报文的 IP 地址没有对应的 arp_buf 缓存，还需要判断接收到的报文是否为 ARP_REQUEST 请求报文，
    // 并且该请求报文的 target_ip 是本机的 IP，则认为是请求本主机 MAC 地址的 ARP 请求报文，则调用 arp_resp() 函数回应一个响应报文。
}

/**
 * @brief 处理一个要发送的数据包
 *
 * @param buf 要处理的数据包
 * @param ip 目标 ip 地址
 * @param protocol 上层协议
 */
void arp_out(buf_t *buf, uint8_t *ip)
{
    // TO-DO

    // Step1
    // 调用 map_get() 函数，根据 IP 地址来查找 ARP 表 (arp_table)。

    // Step2
    // 如果能找到该 IP 地址对应的 MAC 地址，则将数据包直接发送给以太网层，即调用 ethernet_out 函数直接发出去。

    // Step3
    // 如果没有找到对应的 MAC 地址，进一步判断 arp_buf 是否已经有包了：
    // 如果有，则说明正在等待该 ip 回应 ARP 请求，此时不能再发送 arp 请求；
    // 如果没有包，则调用 map_set() 函数将来自 IP 层的数据包缓存到 arp_buf，然后，调用 arp_req() 函数，发一个请求目标 IP 地址对应的 MAC 地址的 ARP request 报文。
}

/**
 * @brief 初始化 arp 协议
 *
 */
void arp_init()
{
    // 调用 map_init() 函数，初始化用于存储 IP 地址和 MAC 地址的 ARP 表 arp_table，并设置超时时间为 ARP_TIMEOUT_SEC。
    map_init(&arp_table, NET_IP_LEN, NET_MAC_LEN, 0, ARP_TIMEOUT_SEC, NULL);
    // 调用 map_init() 函数，初始化用于缓存来自 IP 层的数据包，并设置超时时间为 ARP_MIN_INTERVAL。
    map_init(&arp_buf, NET_IP_LEN, sizeof(buf_t), 0, ARP_MIN_INTERVAL, buf_copy);
    // 调用 net_add_protocol() 函数，增加 key：NET_PROTOCOL_ARP 和 vaule：arp_in 的键值对。
    net_add_protocol(NET_PROTOCOL_ARP, arp_in);
    // 在初始化阶段（系统启用网卡）时，要向网络上发送无回报 ARP 包（ARP announcemennt），即广播包，告诉所有人自己的 IP 地址和 MAC 地址。
    // 在实验代码中，调用 arp_req() 函数来发送一个无回报 ARP 包。
    arp_req(net_if_ip);
    // 无回报 ARP 包（ARP announcement）：
    // 用于昭示天下（LAN）本机要使用某个 IP 地址了，是一个 Sender IP 和 Traget IP 填充的都是本机 IP 地址的 ARP request。
}