#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_mac 源 mac 地址
 *
 * 注：分片重组是附加题，所以先不考虑重组的问题
 */
void ip_in(buf_t *buf, uint8_t *src_mac)
{
    // TO-DO

    // S1 取出需要的数据
    ip_hdr_t *hdr = (ip_hdr_t *)buf->data;
    // 常规数据（根据后面需要来加到这里）
    uint8_t protocol = hdr->protocol;
    uint8_t src_ip[NET_IP_LEN];
    memcpy(src_ip, hdr->src_ip, NET_IP_LEN);
    // uint16 的 swap16
    uint16_t total_len16 = swap16(hdr->total_len16);

    // S2 常规检查
    if (buf->len < IP_HDR_LEN_PER_BYTE * hdr->hdr_len) // 如果数据包的长度小于 IP 头部长度，丢弃不处理。
    {
        return; // 也有的人使用的是 sizeof(ip_hdr_t)，不过我觉得不妥。
    }
    if (hdr->version != IP_VERSION_4) // IP 头部的版本号是否为 IPv4: 0100，也即 4
    {
        return;
    }
    if (total_len16 > buf->len) // 总长度字段小于或等于收到的包的长度，因为包中可能有 padding
    {
        return;
    }
    if (memcmp(hdr->dst_ip, net_if_ip, NET_IP_LEN) != 0) // 对比目的 IP 地址是否为本机的 IP 地址，如果不是，则丢弃不处理。
    {
        return;
    }

    // S3 首部校验和再计算
    uint16_t hdr_checksum16 = hdr->hdr_checksum16;          // 先把 IP 头部的头部校验和字段用其他变量保存起来
    hdr->hdr_checksum16 = 0;                                // 将该头部校验和字段置 0
    uint16_t re_checksum16 = checksum16((uint16_t *)hdr, hdr->hdr_len); // 然后调用 checksum16 函数来计算头部校验和
    if (hdr_checksum16 != swap16(re_checksum16))              // 如果与 IP 头部的首部校验和字段不一致，丢弃不处理
    {
        return;
    }
    hdr->hdr_checksum16 = hdr_checksum16; // 如果一致，则再将该头部校验和字段恢复成原来的值。

    // S4 去 padding
    // 如果接收到的数据包的长度大于 IP 头部的总长度字段，则说明该数据包有填充字段，可调用 buf_remove_padding() 函数去除填充字段。
    int padding_len = buf->len - hdr->total_len16; // 能到这一步就是一定大于等于 0 了
    if (padding_len > 0)
    {
        buf_add_padding(buf, padding_len);
    }

    // S5 去掉 IP 报头
    // 调用 buf_remove_header() 函数去掉 IP 报头。
    // 同样注意 hdr 之后就不能用了，所以删除之前存一下要用的内容
    buf_remove_header(buf, IP_HDR_LEN_PER_BYTE * hdr->hdr_len);

    // S6 调用 net_in() 函数向上层传递数据包
    // 调用 net_in() 函数向上层传递数据包。如果是不能识别的协议类型，即调用 icmp_unreachable() 返回 ICMP 协议不可达信息。
    if ( // protocol == NET_PROTOCOL_TCP ||
        protocol == NET_PROTOCOL_UDP ||
        protocol == NET_PROTOCOL_ICMP) // ICMP 使用 IP 数据包传输，某种程度上算是 IP 的上层协议了
    {
        net_in(buf, protocol, src_ip);
    }
    icmp_unreachable(buf, src_ip, ICMP_CODE_PROTOCOL_UNREACH);
    // 必做任务只要求做到 UDP，TCP 不需要做，所以在做 IP/ICMP 自测时，当收到 TCP 报文可以当作不能处理，需回送一个 ICMP 协议不可达报文。
    // 但是当你已经做到 UDP/TCP 以上协议，用另外一套自测环境，就不用再返回来做 IP/ICMP 自测。
}

/**
 * @brief 处理一个要发送的 ip 分片
 *
 * @param buf 要发送的分片
 * @param ip 目标 ip 地址
 * @param protocol 上层协议
 * @param id 数据包 id
 * @param offset 分片 offset，必须被 8 整除
 * @param mf 分片 mf 标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    // TO-DO

    // Step1
    // 调用 buf_add_header() 增加 IP 数据报头部缓存空间。
    buf_add_header(buf, sizeof(ip_hdr_t));
    ip_hdr_t *hdr = (ip_hdr_t *)buf->data;

    // Step2
    // 填写 IP 数据报头部字段。
    hdr->hdr_len = sizeof(ip_hdr_t) / IP_HDR_LEN_PER_BYTE;
    hdr->version = IP_VERSION_4;
    hdr->tos = 0;
    hdr->total_len16 = swap16(buf->len);
    hdr->id16 = swap16(id);
    hdr->flags_fragment16 = mf ? swap16(IP_MORE_FRAGMENT | (offset >> 3)) : swap16(offset >> 3);
    hdr->ttl = IP_DEFALUT_TTL;
    hdr->protocol = protocol;
    memcpy(hdr->dst_ip, ip, NET_IP_LEN);
    memcpy(hdr->src_ip, net_if_ip, NET_IP_LEN);

    // Step3
    // 先把 IP 头部的首部校验和字段填 0，再调用 checksum16 函数计算校验和，然后把计算出来的校验和填入首部校验和字段。
    hdr->hdr_checksum16 = 0;
    hdr->hdr_checksum16 = checksum16((uint16_t *)hdr, sizeof(ip_hdr_t));

    // Step4
    // 调用 arp_out 函数 () 将封装后的 IP 头部和数据发送出去。
    arp_out(buf, ip);
}

/**
 * @brief 处理一个要发送的 ip 数据包
 *
 * @param buf 要处理的包
 * @param ip 目标 ip 地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    // TO-DO

    // Step1
    // 首先检查从上层传递下来的数据报包长是否大于 IP 协议最大负载包长
    // 即（1500 字节（MTU）减去 IP 首部长度）
    int fragment_len = ETHERNET_MAX_TRANSPORT_UNIT - sizeof(ip_hdr_t);

    // Step2
    // 如果超过 IP 协议最大负载包长，则需要分片发送。
    buf_t ip_buf;
    static int id = 0;
    // IP 协议利用一个计数器，每产生 IP 分组（而非分片）计数器加 1，作为该 IP 分组的标识
    int i = 0; // 分片数标记
    while (buf->len > fragment_len) {
        buf_init(&ip_buf, fragment_len); // 首先调用 buf_init() 初始化一个 ip_buf
        memcpy(&ip_buf.data, buf->data, fragment_len);
        ip_fragment_out(&ip_buf, ip, protocol, id, i * fragment_len, 1); // 调用 ip_fragment_out() 函数发送出去
        buf_remove_header(buf, fragment_len); // 剔除已发送部分
        i++;
    }

    // 如果截断后最后的一个分片小于或等于 IP 协议最大负载包长——这样就放到 Step3 处理
    // 调用 buf_init() 初始化一个 ip_buf，大小等于该分片大小，再调用 ip_fragment_out() 函数发送出去。注意，最后一个分片的 MF = 0。

    // Step3
    // 如果没有超过 IP 协议最大负载包长，则直接调用 ip_fragment_out() 函数发送出去。
    buf_init(&ip_buf, buf->len);
    memcpy(ip_buf.data, buf->data, buf->len);
    ip_fragment_out(&ip_buf, ip, protocol, id, i * fragment_len, 0);
    // 分片的最后一片，片偏移是 i * fragment_len
    // 单独的一片的片偏移也可以表示为 i * fragment_len，因为如果不经历分片，则 i = 0，也是一样的效果

    // 最后 id 增 1
    id++;
}

/**
 * @brief 初始化 ip 协议
 *
 */
void ip_init()
{
    net_add_protocol(NET_PROTOCOL_IP, ip_in);
}