# HTTP 协议

文章来自于：<https://github.com/xuanhao44/net-lab-2023>

## 1 HTTP 协议概览

略。

## 2 结合实验框架的实现

实现 `send_file()` 函数和 `http_server_run()` 函数。框架给出的提示的部分足够完成实验。没有特别要提到的东西。

### 2.1 `http_server_run()`

流程：

1. 调用 `get_line()` 向 `rx_buffer` 中写入一行数据，如果没有数据，则调用 `close_http()` 关闭 TCP，并继续循环。
2. 检查是否有 GET 请求。如果没有，则调用 `close_http()` 关闭 TCP，并继续循环。
3. 解析 GET 请求的路径，注意跳过空格，找到 GET 请求的文件，调用 `send_file()` 发送文件。
4. 调用 `close_http()` 关掉连接。

---

需要注意的点：

1. `rx_buffer` 取出了什么。通过打印可知：

```
"GET / HTTP1.1"
"GET /index.html HTTP/1.1"
"GET /page1.html HTTP/1.1"
```

2. 如何拆分 `rx_buffer`。使用 `strtok()` 函数（来自 `string.h`）

- 函数原型：`char *strtok(char s[], const char*delim);`
- `s[]` 是原字符串，`delim` 为分隔符。
- 返回：字符串拆分后的首地址。
- “拆分”：将分割字符用 '\0’替换
- 特性：
  - `strtok()` 拆分字符串是直接在 **原串** 上操作，所以要求参 1 必须，可读可写（`char *str = “www.baidu.com”` 不行！！！）
  - 第一次拆分，参 1 传待拆分的原串。第 2 次及以后拆分时，参 1 传 NULL.
- 例程：

```c
void test01()
{
    // 1.使用 strtok() 实现分割
    char str[] = "hello,world hello";
    char *str1 = strtok(str, " ,");
    printf("%s\n", str1);
    while (str1 != NULL)
    {
        str1 = strtok(NULL, " ,");
        printf("%s\n", str1);
    }
}
```

### 2.2 `send_file()`

流程：解析 url 路径，查看是否是 `XHTTP_DOC_DIR` 目录下的文件。如果不是，则发送 404 NOT FOUND；如果是，则用 HTTP/1.0 协议发送。

1. 根据 url 获取指定的文件。
2. 若文件不存在，发送 HTTP ERROR 404。
3. 如果是，则用 HTTP/1.0 协议发送：准备 HTTP 报头，读取文件并发送，发送完毕后关闭文件。

---

需要注意的点：

1. url 为 "/" 时候默认访问 "/index.html"。
2. HTTP/1.0 报文的头部我应该是没有填写完全的，不过也能跑。但是 header 之后必须要有一个“空行”。

   <img src="https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/http_response.png" alt="http_response" style="zoom:50%;" />

## 3 实验结果和分析

运行 net-lab：

![http_test_1](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/http_test_1.jpg)

点击“哈工大深圳，春色正浓”：

![http_test_2](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/http_test_2.jpg)

返回主页，再点击“404 页面”：

![http_test_3](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/http_test_3.jpg)

Wireshark 捕获到的报文数据：（Wireshark 的 [http.pcap](../testing/data/http.pcap)）

![net_lab_http_pcap](https://typora-1304621073.cos.ap-guangzhou.myqcloud.com/typora/net_lab/net_lab_http_pcap.png)

## 4 实验中遇到的问题及解决方法

1. 自己写切割字符串不是很理想；找切割字符串的函数找了一会儿，终于找到了一个比较合适的 `strtok()`。
2. 添加了很多 Debug 信息供我检查，一些灵感也是从 Debug 输出中得到的。

## 5 意见和建议

1. http.h 中的 XHTTP_DOC_DIR 居然给的是错的...大离谱。
2. `http_server_run()` 函数的注释 1 是有点错误，应该是“调用 `get_line()` 向 `rx_buffer` 中写入一行数据”。
3. 感觉这个 HTTP 服务器还是太简陋了。
