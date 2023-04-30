#ifndef CONFIG_H
#define CONFIG_H

#define ETHERNET
#define ARP
#define IP
#define ICMP
#define UDP
#define TCP
#define HTTP

#ifdef TEST
#define NET_IF_IP          \
    {                      \
        192, 168, 163, 103 \
    } // 测试用网卡 ip 地址
#define NET_IF_MAC                         \
    {                                      \
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66 \
    } // 测试用网卡 mac 地址
#else
#define NET_IF_IP        \
    {                    \
        172, 20, 128, 45 \
    } // 自定义网卡 ip 地址
#define NET_IF_MAC                         \
    {                                      \
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55 \
    } // 自定义网卡 mac 地址
#endif

#define ETHERNET_MAX_TRANSPORT_UNIT 1500 // 以太网最大传输单元

#define ARP_TIMEOUT_SEC (60 * 5) // arp 表过期时间
#define ARP_MIN_INTERVAL 1       // 向相同地址发送 arp 请求的最小间隔

#define IP_DEFALUT_TTL 64 // IP 默认 TTL

#define BUF_MAX_LEN (2 * UINT16_MAX + UINT8_MAX) // buf 最大长度

#define MAP_MAX_LEN (16 * BUF_MAX_LEN) // map 最大长度
#endif