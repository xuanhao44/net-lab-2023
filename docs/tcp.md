# TCP 协议

文章来自于：<https://github.com/xuanhao44/net-lab-2023>

## 1 TCP 协议概览

TCP 协议的内容和 TCP 报文的结构都比较简单。所以不多赘述。这次的重点是客户端和服务器建立连接和释放连接的过程。

## 2 结合实验框架的实现

这次实验要实现的 TCP 协议的部分只有 `tcp_in`（服务器端 TCP 收包）。并且框架给出的提示的部分足够完成实验。但是具体的建立连接和释放的过程，可以去参考王道考研上总结的部分。

比较重要的是服务器的状态。

建立连接：(CLOSE 省略)，LISTEN(初始创建的状态)，SYN-REVD，ESTABLISHED

释放连接：(先前状态为 ESTABLISHED)，(CLOSE-WAIT 被省略)，LAST-ACK，(CLOSE 省略)

### 2.1 流程

1. 大小检查。检查 `buf` 长度是否小于 tcp 头部。如果是，则丢弃。
2. 检查 `checksum` 字段。如果 `checksum` 出错，则丢弃。
3. 从 tcp 头部字段中获取以下参数：`source port`, `destination port`, `sequence number`, `acknowledge number`, `flags`，以及 `window_size` 等需要的参数，最后注意**大小端转换**。
4. 调用 `map_get` 函数，根据 `destination port` 查找对应的 `handler` 函数。
5. 调用 `new_tcp_key` 函数，根据通信五元组中的：源 IP 地址、目标 IP 地址、目标端口号，确定一个 tcp 链接 key。
6. 调用 `map_get` 函数，根据 `key` 查找一个 `tcp_connect_t* connect`。如果没有找到，则调用 `map_set` 建立新的链接，并设置为 `CONNECT_LISTEN` 状态，然后调用 `mag_get` 获取到该链接。
7. （状态 `TCP_LISTEN`）：如果为 `TCP_LISTEN` 状态，则需要完成如下功能：
   1. 如果收到的 `flag` 带有 `rst`，则 `close_tcp` 关闭 tcp 链接；
   2. 如果收到的 `flag` 不是 `syn`，则 `reset_tcp` 复位通知，因为收到的第一个包必须是 `syn`；
   3. 调用 `init_tcp_connect_rcvd` 函数，初始化 `connect`，将状态设为 `TCP_SYN_RCVD`；
   4. 填充 `connect` 字段，包括：
      - `local_port`, `remote_port`, `ip`, `unack_seq`（设为随机值）；
      - 由于是对 `syn` 的 `ack` 应答包，`next_seq` 与 `unack_seq` 一致；
      - `ack` 设为对方的 `sequence number+1`；
      - 设置 `remote_win` 为对方的窗口大小，注意大小端转换；
   5. 调用 `buf_init` 初始化 `txbuf`；
   6. 调用 `tcp_send` 将 `txbuf` 发送出去，也就是回复一个 `tcp_flags_ack_syn`（SYN + ACK）报文；
   7. 处理结束，返回。
8. （状态 `TCP_LISTEN`）：检查接收到的 `sequence number`，如果与 `ack` 序号不一致，则 `reset_tcp` 复位通知。
9. 检查 `flags` 是否有 `rst` 标志，如果有，则 `close_tcp` 连接重置。
10. 序号相同时的处理，调用 `buf_remove_header` 去除头部后剩下的都是数据。
11. （状态 `TCP_SYN_RCVD`）：在 `RCVD` 状态，如果收到的包没有 `ack flag`，则不做任何处理。
12. （状态 `TCP_SYN_RCVD`）：如果是 `ack` 包，需要完成如下功能：
    1. 将 `unack_seq+1`；
    2. 将状态转成 `ESTABLISHED`；
    3. 调用回调函数，完成三次握手，进入连接状态 `TCP_CONN_CONNECTED`。
13. （状态 `TCP_ESTABLISHED`）：如果收到的包没有 `ack` 且没有 `fin` 这两个标志，则不做任何处理。
14. （状态 `TCP_ESTABLISHED`）：处理 `ACK` 的值。
    - 如果是 `ack` 包，
    - 且 `unack_seq` 小于 `sequence number`（说明有部分数据被对端接收确认了，否则可能是之前重发的 `ack`，可以不处理），
    - 且 `next_seq` `大于 sequence number`，
    - 则调用 `buf_remove_header` 函数，去掉被对端接收确认的部分数据，并更新 `unack_seq` 值。
15. （状态 `TCP_ESTABLISHED`）：接收数据，调用 `tcp_read_from_buf` 函数，把 `buf` 放入 `rx_buf` 中。
16. （状态 `TCP_ESTABLISHED`）：根据当前的标志位进一步处理：
    1. 首先调用 `buf_init` 初始化 `txbuf`；
    2. 判断是否收到关闭请求（`FIN`），如果是，将状态改为 `TCP_LAST_ACK`，`ack + 1`，再发送一个 ACK + FIN 包，并退出，这样就无需进入 `CLOSE_WAIT`，直接等待对方的 ACK；
    3. 如果不是 FIN，则看看是否有数据，如果有，则发 ACK 相应，并调用 `handler` 回调函数进行处理；
    4. 调用 `tcp_write_to_buf` 函数，看看是否有数据需要发送，如果有，同时发数据和 ACK；
    5. 没有收到数据，可能对方只发一个 ACK，可以不响应。
17. （状态 `TCP_FIN_WAIT_1`）：如果收到 FIN && ACK，则 `close_tcp` 直接关闭 TCP；如果只收到 ACK，则将状态转为 `TCP_FIN_WAIT_2`。
18. （状态 `TCP_FIN_WAIT_1`）：如果不是 FIN，则不做处理；如果是，则：
    1. 将 ACK + 1；
    2. 调用 `buf_init` 初始化 `txbuf`；
    3. 调用 `tcp_send` 发送一个 ACK 数据包；
    4. 再 `close_tcp` 关闭 TCP。
19. （状态 `TCP_LAST_ACK`）：如果不是 ACK，则不做处理；如果是，则：
    1. 调用 `handler` 函数，进入 `TCP_CONN_CLOSED` 状态；
    2. 再 `close_tcp` 关闭 TCP。

### 2.2 需要注意的点

1. `tcp_checksum` 函数写的很差，建议改成自己之前写的 UDP 的函数，但是注意把 UDP 替换成 TCP。（我就是没替换完全，不过还好通过 debug 发现了这个问题）
2. 还是要把 ip 代码中 tcp 的判断加上。（同样通过 debug 发现了该问题）

## 3 实验结果和分析

