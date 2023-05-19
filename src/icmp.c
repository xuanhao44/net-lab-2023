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

    // S1 组装响应报文
    buf_init(&txbuf, req_buf->len);
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

/**
 * @brief 处理一个收到的数据包
 *
 * @param buf 要处理的数据包
 * @param src_ip 源 ip 地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip)
{
    // TO-DO

    // 首先做报头检测，如果接收到的包长小于 ICMP 头部长度，则丢弃不处理。
    if (buf->len < sizeof(icmp_hdr_t))
    {
        return;
    }

    icmp_hdr_t *hdr = (icmp_hdr_t *)buf->data;
    if (hdr->type == ICMP_TYPE_ECHO_REQUEST) // 接着，查看该报文的 ICMP 类型是否为回显请求。
    {
        icmp_resp(buf, src_ip); // 如果是，则调用 icmp_resp() 函数回送一个回显应答（ping 应答）。
    }

    return;
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

    // S1 差错报文数据：使用收到的 IP 报头与其报文前 8 字节
    buf_init(&txbuf, sizeof(ip_hdr_t) + 8);
    memcpy(txbuf.data, recv_buf->data, sizeof(ip_hdr_t) + 8);

    // S2 添加 ICMP 报头
    buf_add_header(&txbuf, sizeof(icmp_hdr_t));
    icmp_hdr_t *un_hdr = (icmp_hdr_t *)txbuf.data;
    un_hdr->type = ICMP_TYPE_UNREACH;
    un_hdr->code = code;
    // id 与 seq 字段在差错报文中未用，必须为 0！
    un_hdr->id16 = 0;
    un_hdr->seq16 = 0;

    // S3 计算校验和，范围为整个 ICMP 报文
    un_hdr->checksum16 = 0;
    un_hdr->checksum16 = checksum16((uint16_t *)txbuf.data, txbuf.len);

    // S4 发送数据包
    ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 初始化 icmp 协议
 *
 */
void icmp_init()
{
    net_add_protocol(NET_PROTOCOL_ICMP, icmp_in);
}