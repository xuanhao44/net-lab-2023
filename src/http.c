#include "http.h"
#include "tcp.h"
#include "net.h"
#include "assert.h"

#define TCP_FIFO_SIZE 40

typedef struct http_fifo
{
    tcp_connect_t *buffer[TCP_FIFO_SIZE];
    uint8_t front, tail, count;
} http_fifo_t;

static http_fifo_t http_fifo_v;

static void http_fifo_init(http_fifo_t *fifo)
{
    fifo->count = 0;
    fifo->front = 0;
    fifo->tail = 0;
}

static int http_fifo_in(http_fifo_t *fifo, tcp_connect_t *tcp)
{
    if (fifo->count >= TCP_FIFO_SIZE)
    {
        return -1;
    }
    fifo->buffer[fifo->front] = tcp;
    fifo->front++;
    if (fifo->front >= TCP_FIFO_SIZE)
    {
        fifo->front = 0;
    }
    fifo->count++;
    return 0;
}

static tcp_connect_t *http_fifo_out(http_fifo_t *fifo)
{
    if (fifo->count == 0)
    {
        return NULL;
    }
    tcp_connect_t *tcp = fifo->buffer[fifo->tail];
    fifo->tail++;
    if (fifo->tail >= TCP_FIFO_SIZE)
    {
        fifo->tail = 0;
    }
    fifo->count--;
    return tcp;
}

static size_t get_line(tcp_connect_t *tcp, char *buf, size_t size)
{
    size_t i = 0;
    while (i < size)
    {
        char c;
        if (tcp_connect_read(tcp, (uint8_t *)&c, 1) > 0)
        {
            if (c == '\n')
            {
                break;
            }
            if (c != '\n' && c != '\r')
            {
                buf[i] = c;
                i++;
            }
        }
        net_poll();
    }
    buf[i] = '\0';
    return i;
}

static size_t http_send(tcp_connect_t *tcp, const char *buf, size_t size)
{
    size_t send = 0;
    while (send < size)
    {
        send += tcp_connect_write(tcp, (const uint8_t *)buf + send, size - send);
        net_poll();
    }
    return send;
}

static void close_http(tcp_connect_t *tcp)
{
    tcp_connect_close(tcp);
    printf("http closed.\n");
}

static void send_file(tcp_connect_t *tcp, const char *url)
{
    FILE *file;
    // const char* content_type = "text/html";
    char file_path[255];
    char tx_buffer[1024];

    /*
    解析 url 路径，查看是否是查看 XHTTP_DOC_DIR 目录下的文件
    如果不是，则发送 404 NOT FOUND
    如果是，则用 HTTP/1.0 协议发送

    注意，本实验的 WEB 服务器网页存放在 XHTTP_DOC_DIR 目录中
    */

    // TODO
    // 根据 url 获取指定的文件
    // 查看 include/http.h: XHTTP_DOC_DIR : "../htmldocs"
    memcpy(file_path, XHTTP_DOC_DIR, sizeof(XHTTP_DOC_DIR));
    strcat(file_path, url);
    if (strcmp(url, "/") == 0)
    {
        strcat(file_path, "index.html");
    }
    file = fopen(file_path, "rb");

    //  若文件不存在，发送 HTTP ERROR 404
    if (file == NULL)
    {
        memset(tx_buffer, 0, sizeof(tx_buffer));
        strcpy(tx_buffer, "HTTP/1.0 404 NOT FOUND\r\n");
        strcat(tx_buffer, "Sever: \r\n");
        strcat(tx_buffer, "Content-Type: text/html\r\n");
        strcat(tx_buffer, "\r\n");
        http_send(tcp, tx_buffer, strlen(tx_buffer));
        return;
    }

    // 如果是，则用 HTTP/1.0 协议发送
    // 准备 HTTP 报头
    memset(tx_buffer, 0, sizeof(tx_buffer));
    strcpy(tx_buffer, "HTTP/1.0 200 OK\r\n");
    strcat(tx_buffer, "Sever: \r\n");
    strcat(tx_buffer, "Content-Type: \r\n");
    strcat(tx_buffer, "\r\n");
    http_send(tcp, tx_buffer, strlen(tx_buffer));

    // 读取文件并发送
    memset(tx_buffer, 0, sizeof(tx_buffer));
    while (fread(tx_buffer, sizeof(char), sizeof(tx_buffer), file) > 0)
    {
        http_send(tcp, tx_buffer, sizeof(tx_buffer));
        memset(tx_buffer, 0, sizeof(tx_buffer));
    }

    // 发送完毕后关闭文件
    fclose(file);
}

static void http_handler(tcp_connect_t *tcp, connect_state_t state)
{
    if (state == TCP_CONN_CONNECTED)
    {
        http_fifo_in(&http_fifo_v, tcp);
        printf("http conntected.\n");
    }
    else if (state == TCP_CONN_DATA_RECV)
    {
    }
    else if (state == TCP_CONN_CLOSED)
    {
        printf("http closed.\n");
    }
    else
    {
        assert(0);
    }
}

// 在端口上创建服务器。
int http_server_open(uint16_t port)
{
    if (!tcp_open(port, http_handler))
    {
        return -1;
    }
    http_fifo_init(&http_fifo_v);
    return 0;
}

// 从 FIFO 取出请求并处理。新的 HTTP 请求时会发送到 FIFO 中等待处理。
void http_server_run(void)
{
    tcp_connect_t *tcp;
    char url_path[255];
    char rx_buffer[1024];

    while ((tcp = http_fifo_out(&http_fifo_v)) != NULL)
    {
        char *c = rx_buffer;

        // 1 调用 get_line 向 rx_buffer 中写入一行数据，
        // 如果没有数据，则调用 close_http 关闭 tcp，并继续循环
        // TODO
        if (get_line(tcp, c, sizeof(rx_buffer)) == 0)
        {
            close_http(tcp);
            continue;
        }

        // 2 检查是否有 GET 请求，如："GET / HTTP1.1"
        // 如果没有，则调用 close_http 关闭 tcp，并继续循环
        // TODO
        if (memcmp(rx_buffer, "GET", 3) != 0)
        {
            close_http(tcp);
            continue;
        }

        // 3 解析 GET 请求的路径，注意跳过空格，找到 GET 请求的文件，调用 send_file 发送文件
        // TODO
        int i;
        while (c[i + 4] != ' ')
        {
            url_path[i] = c[i + 4];
            i++;
        }
        url_path[i] = '\0';
        send_file(tcp, url_path);

        // 4 调用 close_http 关掉连接
        // TODO
        close_http(tcp);

        printf("!! final close\n");
    }
}
