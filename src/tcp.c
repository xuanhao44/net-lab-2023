#include <assert.h>
#include "map.h"
#include "tcp.h"
#include "ip.h"

static void panic(const char *msg, int line)
{
    printf("panic %s! at line %d\n", msg, line);
    assert(0);
}

static void display_flags(tcp_flags_t flags)
{
    printf("flags:%s%s%s%s%s%s%s%s\n",
           flags.cwr ? " cwr" : "",
           flags.ece ? " ece" : "",
           flags.urg ? " urg" : "",
           flags.ack ? " ack" : "",
           flags.psh ? " psh" : "",
           flags.rst ? " rst" : "",
           flags.syn ? " syn" : "",
           flags.fin ? " fin" : "");
}

// dst-port -> handler
static map_t tcp_table; // tcp_table 里面放了一个 dst_port 的回调函数

// tcp_key_t[IP, src port, dst port] -> tcp_connect_t

/* Connect_table 放置了一堆 TCP 连接，
    KEY 为 [IP，src port，dst port], 即 tcp_key_t，VALUE 为 tcp_connect_t。
*/
static map_t connect_table;

/**
 * @brief 生成一个用于 connect_table 的 key
 *
 * @param ip
 * @param src_port
 * @param dst_port
 * @return tcp_key_t
 */
static tcp_key_t new_tcp_key(uint8_t ip[NET_IP_LEN], uint16_t src_port, uint16_t dst_port)
{
    tcp_key_t key;
    memcpy(key.ip, ip, NET_IP_LEN);
    key.src_port = src_port;
    key.dst_port = dst_port;
    return key;
}

/**
 * @brief 初始化 tcp 在静态区的 map
 *
 * 供应用层使用
 */
void tcp_init()
{
    map_init(&tcp_table, sizeof(uint16_t), sizeof(tcp_handler_t), 0, 0, NULL);
    map_init(&connect_table, sizeof(tcp_key_t), sizeof(tcp_connect_t), 0, 0, NULL);
    net_add_protocol(NET_PROTOCOL_TCP, tcp_in);
}

/**
 * @brief 向 port 注册一个 TCP 连接以及关联的回调函数
 *
 * 供应用层使用
 *
 * @param port
 * @param handler
 * @return int
 */
int tcp_open(uint16_t port, tcp_handler_t handler)
{
    printf("tcp open\n");
    return map_set(&tcp_table, &port, &handler);
}

/**
 * @brief 完成了缓存分配工作，状态也会切换为 TCP_SYN_RCVD
 *        rx_buf 和 tx_buf 在触及边界时会把数据重新移动到头部，防止溢出。
 *
 * @param connect
 */
static void init_tcp_connect_rcvd(tcp_connect_t *connect)
{
    if (connect->state == TCP_LISTEN)
    {
        connect->rx_buf = malloc(sizeof(buf_t));
        connect->tx_buf = malloc(sizeof(buf_t));
    }
    buf_init(connect->rx_buf, 0);
    buf_init(connect->tx_buf, 0);
    connect->state = TCP_SYN_RCVD;
}

/**
 * @brief 释放 TCP 连接，这会释放分配的空间，并把状态变回 LISTEN。
 *        一般这个后边都会跟个 map_delete(&connect_table, &key) 把状态变回 CLOSED
 *
 * @param connect
 */
static void release_tcp_connect(tcp_connect_t *connect)
{
    if (connect->state == TCP_LISTEN)
        return;
    free(connect->rx_buf);
    free(connect->tx_buf);
    connect->state = TCP_LISTEN;
}

static uint16_t tcp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dst_ip)
{
    uint16_t len = (uint16_t)buf->len;
    tcp_peso_hdr_t *peso_hdr = (tcp_peso_hdr_t *)(buf->data - sizeof(tcp_peso_hdr_t));
    tcp_peso_hdr_t pre; // 暂存被覆盖的 IP 头
    memcpy(&pre, peso_hdr, sizeof(tcp_peso_hdr_t));
    memcpy(peso_hdr->src_ip, src_ip, NET_IP_LEN);
    memcpy(peso_hdr->dst_ip, dst_ip, NET_IP_LEN);
    peso_hdr->placeholder = 0;
    peso_hdr->protocol = NET_PROTOCOL_TCP;
    peso_hdr->total_len16 = swap16(len);
    uint16_t checksum = checksum16((uint16_t *)peso_hdr, len + sizeof(tcp_peso_hdr_t));
    memcpy(peso_hdr, &pre, sizeof(tcp_peso_hdr_t));
    return checksum;
}

static _Thread_local uint16_t delete_port;

/**
 * @brief tcp_close 使用这个函数来查找可以关闭的连接，使用 thread-local 变量 delete_port 传递端口号。
 *
 * @param key,value,timestamp
 */
static void close_port_fn(void *key, void *value, time_t *timestamp)
{
    tcp_key_t *tcp_key = key;
    tcp_connect_t *connect = value;
    if (tcp_key->dst_port == delete_port)
    {
        release_tcp_connect(connect);
    }
}

/**
 * @brief 关闭 port 上的 TCP 连接
 *
 * 供应用层使用
 *
 * @param port
 */
void tcp_close(uint16_t port)
{
    delete_port = port;
    map_foreach(&connect_table, close_port_fn);
    map_delete(&tcp_table, &port);
}

/**
 * @brief 从 buf 中读取数据到 connect->rx_buf
 *
 * @param connect
 * @param buf
 * @return uint16_t 字节数
 */
static uint16_t tcp_read_from_buf(tcp_connect_t *connect, buf_t *buf)
{
    uint8_t *dst = connect->rx_buf->data + connect->rx_buf->len;
    buf_add_padding(connect->rx_buf, buf->len);
    memcpy(dst, buf->data, buf->len);
    connect->ack += buf->len;
    return buf->len;
}

/**
 * @brief 把 connect 内 tx_buf 的数据写入到 buf 里面供 tcp_send 使用，buf 原来的内容会无效。
 *
 * @param connect
 * @param buf
 * @return uint16_t 字节数
 */
static uint16_t tcp_write_to_buf(tcp_connect_t *connect, buf_t *buf)
{
    uint16_t sent = connect->next_seq - connect->unack_seq;
    uint16_t size = min32(connect->tx_buf->len - sent, connect->remote_win);
    buf_init(buf, size);
    memcpy(buf->data, connect->tx_buf->data + sent, size);
    connect->next_seq += size;
    return size;
}

/**
 * @brief 发送 TCP 包，seq_number32 = connect->next_seq - buf->len
 *        buf 里的数据将作为负载，加上 tcp 头发送出去。如果 flags 包含 syn 或 fin，seq 会递增。
 *
 * @param buf
 * @param connect
 * @param flags
 */
static void tcp_send(buf_t *buf, tcp_connect_t *connect, tcp_flags_t flags)
{
    // printf("<< tcp send >> sz=%zu\n", buf->len);
    display_flags(flags);
    size_t prev_len = buf->len;
    buf_add_header(buf, sizeof(tcp_hdr_t));
    tcp_hdr_t *hdr = (tcp_hdr_t *)buf->data;
    hdr->src_port16 = swap16(connect->local_port);
    hdr->dst_port16 = swap16(connect->remote_port);
    hdr->seq_number32 = swap32(connect->next_seq - prev_len);
    hdr->ack_number32 = swap32(connect->ack);
    hdr->data_offset = sizeof(tcp_hdr_t) / sizeof(uint32_t);
    hdr->reserved = 0;
    hdr->flags = flags;
    hdr->window_size16 = swap16(connect->remote_win);
    hdr->checksum16 = 0;
    hdr->urgent_pointer16 = 0;
    hdr->checksum16 = tcp_checksum(buf, connect->ip, net_if_ip);
    ip_out(buf, connect->ip, NET_PROTOCOL_TCP);
    if (flags.syn || flags.fin)
    {
        connect->next_seq += 1;
    }
}

/**
 * @brief 从外部关闭一个 TCP 连接，会发送剩余数据
 *
 * 供应用层使用
 *
 * @param connect
 */
void tcp_connect_close(tcp_connect_t *connect)
{
    if (connect->state == TCP_ESTABLISHED)
    {
        tcp_write_to_buf(connect, &txbuf);
        tcp_send(&txbuf, connect, tcp_flags_ack_fin);
        connect->state = TCP_FIN_WAIT_1;
        return;
    }
    tcp_key_t key = new_tcp_key(connect->ip, connect->remote_port, connect->local_port);
    release_tcp_connect(connect);
    map_delete(&connect_table, &key);
}

/**
 * @brief 从 connect 中读取数据到 buf，返回成功的字节数。
 *
 * 供应用层使用
 *
 * @param connect
 * @param data
 * @param len
 * @return size_t
 */
size_t tcp_connect_read(tcp_connect_t *connect, uint8_t *data, size_t len)
{
    buf_t *rx_buf = connect->rx_buf;
    size_t size = min32(rx_buf->len, len);
    memcpy(data, rx_buf->data, size);
    if (buf_remove_header(rx_buf, size) != 0)
    {
        memmove(rx_buf->payload, rx_buf->data, rx_buf->len);
        rx_buf->data = rx_buf->payload;
    }
    return size;
}

/**
 * @brief 往 connect 的 tx_buf 里面写东西，返回成功的字节数，这里要判断窗口够不够，否则图片显示不全。
 *
 * 供应用层使用
 *
 * @param connect
 * @param data
 * @param len
 */
size_t tcp_connect_write(tcp_connect_t *connect, const uint8_t *data, size_t len)
{
    // printf("tcp_connect_write size: %zu\n", len);
    buf_t *tx_buf = connect->tx_buf;

    uint8_t *dst = tx_buf->data + tx_buf->len;
    size_t size = min32(&tx_buf->payload[BUF_MAX_LEN] - dst, len);

    if (connect->next_seq - connect->unack_seq + len >= connect->remote_win)
    {
        return 0;
    }
    if (buf_add_padding(tx_buf, size) != 0)
    {
        memmove(tx_buf->payload, tx_buf->data, tx_buf->len);
        tx_buf->data = tx_buf->payload;
        if (tcp_write_to_buf(connect, &txbuf))
        {
            tcp_send(&txbuf, connect, tcp_flags_ack);
        }
        return 0;
    }
    memcpy(dst, data, size);
    return size;
}

/**
 * @brief 服务器端 TCP 收包
 *
 * @param buf
 * @param src_ip
 */
void tcp_in(buf_t *buf, uint8_t *src_ip)
{
    printf("<<< tcp_in >>>\n");

    // 1 大小检查
    // 检查 buf 长度是否小于 tcp 头部。如果是，则丢弃
    if (buf->len < sizeof(tcp_hdr_t))
    {
        return;
    }

    // 2 检查 checksum 字段。如果 checksum 出错，则丢弃
    tcp_hdr_t *hdr = (tcp_hdr_t *)buf->data;
    uint16_t tmp_checksum16 = hdr->checksum16;
    hdr->checksum16 = 0;
    // tcp_checksum 写的方式害人不浅
    uint8_t tmp_src_ip[NET_IP_LEN];
    memcpy(tmp_src_ip, src_ip, NET_IP_LEN);
    uint16_t re_checksum = tcp_checksum(buf, tmp_src_ip, net_if_ip);
    if (tmp_checksum16 != re_checksum)
    {
        return;
    }
    hdr->checksum16 = tmp_checksum16;

    // 3 从 tcp 头部字段中获取以下参数：
    // source port, destination port, sequence number, acknowledge number, flags
    // 以及 window_size 等需要的参数
    // 注意大小端转换
    uint16_t src_port = swap16(hdr->src_port16);
    uint16_t dest_port = swap16(hdr->dst_port16);
    uint32_t seq_number = swap32(hdr->seq_number32);
    uint32_t get_seq = seq_number;
    uint32_t ack_number = swap32(hdr->ack_number32);
    uint16_t window_size = swap16(hdr->window_size16);
    size_t hdr_len = 4 * (uint16_t)hdr->data_offset;
    size_t data_len = buf->len - hdr_len;
    tcp_flags_t flags = hdr->flags;

    // 4 调用 map_get 函数，根据 destination port 查找对应的 handler 函数
    tcp_handler_t *handler = map_get(&tcp_table, &dest_port);

    // 5 调用 new_tcp_key 函数，根据通信五元组中的：
    // 源 IP 地址、目标 IP 地址、目标端口号
    // 确定一个 tcp 链接 key
    tcp_key_t tcp_key = new_tcp_key(src_ip, src_port, dest_port);

    // 6 调用 map_get 函数，根据 key 查找一个 tcp_connect_t* connect
    // 如果没有找到，则调用 map_set 建立新的链接，并设置为 CONNECT_LISTEN 状态，然后调用 mag_get 获取到该链接。
    tcp_connect_t *connect = map_get(&connect_table, &tcp_key);
    if (connect == NULL)
    {
        tcp_connect_t new_connect;
        new_connect.state = TCP_LISTEN;
        map_set(&connect_table, &tcp_key, &new_connect);
        connect = map_get(&connect_table, &tcp_key);
    }

    // 7 如果为 TCP_LISTEN 状态，则需要完成如下功能
    if (connect->state == TCP_LISTEN)
    {
        // 7.1 如果收到的 flag 带有 rst，则 close_tcp 关闭 tcp 链接
        if (flags.rst)
        {
            goto close_tcp;
        }

        // 7.2 如果收到的 flag 不是 syn，则 reset_tcp 复位通知。因为收到的第一个包必须是 syn
        if (flags.syn == 0)
        {
            goto reset_tcp;
        }

        // 7.3 调用 init_tcp_connect_rcvd 函数，初始化 connect，将状态设为 TCP_SYN_RCVD
        init_tcp_connect_rcvd(connect);

        // 7.4 填充 connect 字段，包括以下：
        connect->local_port = dest_port;
        connect->remote_port = src_port;
        memcpy(connect->ip, src_ip, NET_IP_LEN);
        connect->unack_seq = 191810; // 设为随机值
        connect->next_seq = 191810;  // 对 syn 的 ack 应答包，与 unack_seq 一致
        connect->ack = seq_number + 1;
        connect->remote_win = window_size;

        // 7.5 调用 buf_init 初始化 txbuf
        buf_init(connect->tx_buf, 0);

        // 7.6 调用 tcp_send 将 txbuf 发送出去，也就是回复一个 tcp_flags_ack_syn（SYN+ACK）报文
        tcp_send(connect->tx_buf, connect, tcp_flags_ack_syn);

        // 7.7 处理结束，返回。
        return;
    }

    // 8 检查接收到的 sequence number，如果与 ack 序号不一致，则 reset_tcp 复位通知。
    if (seq_number != connect->ack)
    {
        goto reset_tcp;
    }

    // 9 检查 flags 是否有 rst 标志，如果有，则 close_tcp 连接重置
    if (flags.rst)
    {
        goto close_tcp;
    }

    // 10 序号相同时的处理，调用 buf_remove_header 去除头部后剩下的都是数据
    buf_remove_header(buf, hdr_len);

    /* 状态转换 */
    switch (connect->state)
    {
    case TCP_LISTEN:
        panic("switch TCP_LISTEN", __LINE__);
        break;

    case TCP_SYN_RCVD:
        // 11 在 RCVD 状态，如果收到的包没有 ack flag，则不做任何处理
        if (flags.ack == 0)
        {
            break;
        }

        // 12 如果是 ack 包，需要完成如下功能
        // 12.1 将 unack_seq +1
        connect->unack_seq++;

        // 12.2 将状态转成 ESTABLISHED
        connect->state = TCP_ESTABLISHED;

        // 12.3 调用回调函数，完成三次握手，进入连接状态 TCP_CONN_CONNECTED
        (*handler)(connect, TCP_CONN_CONNECTED);

        break;

    case TCP_ESTABLISHED:
        // 13 如果收到的包没有 ack 且没有 fin 这两个标志，则不做任何处理
        if (flags.ack == 0 && flags.fin == 0)
        {
            break;
        }

        // 14 处理 ACK 的值
        // 如果是 ack 包，
        // 且 unack_seq 小于 sequence number（说明有部分数据被对端接收确认了，否则可能是之前重发的 ack，可以不处理），
        // （且 next_seq 大于 sequence number）
        // 则调用 buf_remove_header 函数，去掉被对端接收确认的部分数据，并更新 unack_seq 值
        if (flags.ack && connect->unack_seq < ack_number) // 存在新确认的数据，更新 ACK 的值
        {
            uint32_t newack_len = ack_number - connect->unack_seq;
            buf_remove_header(connect->tx_buf, newack_len);
            connect->unack_seq = ack_number;
        }

        // 15 接收数据，调用 tcp_read_from_buf 函数，把 buf 放入 rx_buf 中
        tcp_read_from_buf(connect, buf);

        // 16 根据当前的标志位进一步处理
        // 16.1 首先调用 buf_init 初始化 txbuf
        buf_t txbuf;
        buf_init(&txbuf, 0);
        buf_init(connect->tx_buf, 0);

        // 16.2 判断是否收到关闭请求（FIN），如果是，将状态改为 TCP_LAST_ACK，ack +1，再发送一个 ACK + FIN 包，并退出
        // 这样就无需进入 CLOSE_WAIT，直接等待对方的 ACK
        if (flags.fin)
        {
            connect->state = TCP_LAST_ACK;
            connect->ack += 1;
            tcp_send(connect->tx_buf, connect, tcp_flags_ack_fin);
            break;
        }
        else // 16.3 如果不是 FIN，则看看是否有数据，如果有，则发 ACK 相应，并调用 handler 回调函数进行处理
        {
            if (data_len > 0)
            {
                (*handler)(connect, TCP_CONN_DATA_RECV);
            }
            // 16.4 调用 tcp_write_to_buf 函数，看看是否有数据需要发送，如果有，同时发数据和 ACK
            uint16_t len = tcp_write_to_buf(connect, &txbuf);
            if (data_len > 0 || len > 0)
            {
                tcp_send(&txbuf, connect, tcp_flags_ack);
            }
        }
        // 16.5 没有收到数据，可能对方只发一个 ACK，可以不响应
        break;

    case TCP_CLOSE_WAIT:
        panic("switch TCP_CLOSE_WAIT", __LINE__);
        break;

    case TCP_FIN_WAIT_1:
        // 17
        // 17.1 如果收到 FIN && ACK，则 close_tcp 直接关闭 TCP
        if (flags.fin && flags.ack)
        {
            goto close_tcp;
        }

        // 17.2 如果只收到 ACK，则将状态转为 TCP_FIN_WAIT_2
        if (flags.ack)
        {
            connect->state = TCP_FIN_WAIT_2;
        }
        break;

    case TCP_FIN_WAIT_2:
        // 18 如果不是 FIN，则不做处理
        if (flags.fin) // 如果是，则：
        {
            connect->ack += 1;                                 // 将 ACK + 1
            buf_init(connect->tx_buf, 0);                      // 调用 buf_init 初始化 txbuf
            tcp_send(connect->tx_buf, connect, tcp_flags_ack); // 调用 tcp_send 发送一个 ACK 数据包
            goto close_tcp;                                    // 再 close_tcp 关闭 TCP
        }
        break;

    case TCP_LAST_ACK:
        // 19 如果不是 ACK，则不做处理
        if (flags.ack) // 如果是，则：
        {
            (*handler)(connect, TCP_CONN_CLOSED); // 调用 handler 函数，进入 TCP_CONN_CLOSED 状态
            goto close_tcp;                       // 再 close_tcp 关闭 TCP
        }
        break;

    default:
        panic("connect->state", __LINE__);
        break;
    }
    return;

reset_tcp:
    printf("!!! reset tcp !!!\n");
    connect->next_seq = 0;
    connect->ack = get_seq + 1;
    buf_init(&txbuf, 0);
    tcp_send(&txbuf, connect, tcp_flags_ack_rst);
close_tcp:
    release_tcp_connect(connect);
    map_delete(&connect_table, &tcp_key);
    return;
}
