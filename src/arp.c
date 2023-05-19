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
 * 补充说明：我要给某个 IP 发消息但不知道 mac 地址，我先发个 arp request，这时本来要发的消息需要缓存一下，就存在 arp_buf 中
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
    buf_init(&txbuf, sizeof(arp_pkt_t)); // 初始化为 arp 包长度
    arp_pkt_t *pkt = (arp_pkt_t *)txbuf.data;

    // Step2
    // 填写 ARP 报头。
    memcpy(pkt, &arp_init_pkt, sizeof(arp_pkt_t)); // 用前面初始化好的 arp 数据包
    memcpy(pkt->target_ip, target_ip, NET_IP_LEN);

    // Step3
    // ARP 操作类型为 ARP_REQUEST，注意大小端转换。
    pkt->opcode16 = swap16(ARP_REQUEST);

    // Step4
    // 调用 ethernet_out 函数将 ARP 报文发送出去。
    // 注意：ARP announcement 或 ARP 请求报文都是广播报文，其目标 MAC 地址应该是广播地址：FF-FF-FF-FF-FF-FF。
    ethernet_out(&txbuf, ether_broadcast_mac, NET_PROTOCOL_ARP);
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
    buf_init(&txbuf, sizeof(arp_pkt_t)); // 初始化为 arp 包长度
    arp_pkt_t *pkt = (arp_pkt_t *)txbuf.data;

    // Step2
    // 接着，填写 ARP 报头首部。
    memcpy(pkt, &arp_init_pkt, sizeof(arp_pkt_t)); // 用前面初始化好的 arp 数据包
    memcpy(pkt->target_ip, target_ip, NET_IP_LEN);
    memcpy(pkt->target_mac, target_mac, NET_MAC_LEN); // 注意在应答报文中就要补上 target_mac，因为从另一方的 request 知道了 target_mac

    // ARP 操作类型为 ARP_REPLY，注意大小端转换。
    pkt->opcode16 = swap16(ARP_REPLY);

    // Step3
    // 调用 ethernet_out() 函数将填充好的 ARP 报文发送出去。
    ethernet_out(&txbuf, target_mac, NET_PROTOCOL_ARP);
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
    if (buf->len < sizeof(arp_pkt_t))
    {
        return;
    }

    // Step2
    // 接着，做报头检查，查看报文是否完整，检测内容包括：
    // ARP 报头的硬件类型、上层协议类型、MAC 硬件地址长度、IP 协议地址长度、操作类型，检测该报头是否符合协议规定。
    arp_pkt_t *pkt = (arp_pkt_t *)buf->data;
    // 硬件类型为以太网
    if (swap16(pkt->hw_type16) != ARP_HW_ETHER)
    {
        return;
    }
    // （硬件地址要映射的协议地址类型）映射 IP 地址时的值为 0x0800
    if (swap16(pkt->pro_type16) != NET_PROTOCOL_IP)
    {
        return;
    }
    // MAC 硬件地址长度为 6
    if (pkt->hw_len != NET_MAC_LEN)
    {
        return;
    }
    // IP 协议地址长度为 4
    if (pkt->pro_len != NET_IP_LEN)
    {
        return;
    }
    // 检测该报头是否符合协议规定：ARP 请求和应答报文两种
    if (swap16(pkt->opcode16) != ARP_REQUEST && swap16(pkt->opcode16) != ARP_REPLY)
    {
        return;
    }

    // Step3
    // 调用 map_set() 函数更新 ARP 表项。（arp 地址转换表，<ip,mac>的容器）
    map_set(&arp_table, pkt->sender_ip, pkt->sender_mac); // 即存下了 sender 的 ip 和 mac 的键值对

    // Step4
    // 调用 map_get() 函数查看该接收报文的 IP 地址是否有对应的 arp_buf 缓存。
    buf_t *arp_buf_i = (buf_t *)map_get(&arp_buf, pkt->sender_ip);
    // 如果有，则说明 ARP 分组队列里面有待发送的数据包。
    // 也就是上一次调用 arp_out() 函数发送来自 IP 层的数据包时，由于没有找到对应的 MAC 地址进而先发送的 ARP request 报文，此时收到了该 request 的应答报文。
    // 然后，将缓存的数据包 arp_buf 再发送给以太网层，即调用 ethernet_out() 函数直接发出去，接着调用 map_delete() 函数将这个缓存的数据包删除掉。
    if (arp_buf_i != NULL)
    {
        ethernet_out(arp_buf_i, pkt->sender_mac, NET_PROTOCOL_IP);
        map_delete(&arp_buf, pkt->sender_ip);
        return;
    }
    // 如果该接收报文的 IP 地址没有对应的 arp_buf 缓存，还需要判断接收到的报文是否为 ARP_REQUEST 请求报文，
    // 并且该请求报文的 target_ip 是本机的 IP，则认为是请求本主机 MAC 地址的 ARP 请求报文，则调用 arp_resp() 函数回应一个响应报文。
    if (swap16(pkt->opcode16) == ARP_REQUEST && memcmp(pkt->target_ip, net_if_ip, NET_IP_LEN) == 0)
    {
        arp_resp(pkt->sender_ip, pkt->sender_mac);
    }
    // 有笨比写成 pkt->target_ip == net_if_ip 了
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
    uint8_t *mac = (uint8_t *)map_get(&arp_table, ip);
    // 由指导书得知如果超时那么就不会被取出，也是 NULL

    // Step2
    // 如果能找到该 IP 地址对应的 MAC 地址，则将数据包直接发送给以太网层，即调用 ethernet_out 函数直接发出去。
    if (mac != NULL)
    {
        ethernet_out(buf, mac, NET_PROTOCOL_IP); // 是 IP 协议
        return;
    }

    // Step3
    // 如果没有找到对应的 MAC 地址，进一步判断 arp_buf 是否已经有包了：
    buf_t *arp_buf_i = (buf_t *)map_get(&arp_buf, ip);
    // 如果有，则说明正在等待该 ip 回应 ARP 请求，此时不能再发送 arp 请求；
    if (arp_buf_i != NULL)
    {
        return;
    }
    // 如果没有包，则调用 map_set() 函数将来自 IP 层的数据包缓存到 arp_buf，
    map_set(&arp_buf, ip, buf);
    // 然后，调用 arp_req() 函数，发一个请求目标 IP 地址对应的 MAC 地址的 ARP request 报文。
    arp_req(ip);
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