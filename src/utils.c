#include "utils.h"
#include <stdio.h>
#include <string.h>
/**
 * @brief ip 转字符串串
 *
 * @param ip ip 地址
 * @return char* 生成的字符串
 */
char *iptos(uint8_t *ip)
{
    static char output[3 * 4 + 3 + 1];
    sprintf(output, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    return output;
}

/**
 * @brief mac 转字符串
 *
 * @param mac mac 地址
 * @return char* 生成的字符串
 */
char *mactos(uint8_t *mac)
{
    static char output[2 * 6 + 5 + 1];
    sprintf(output, "%02X-%02X-%02X-%02X-%02X-%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return output;
}

/**
 * @brief 时间戳转字符串
 *
 * @param timestamp 时间戳
 * @return char* 生成的字符串
 */
char *timetos(time_t timestamp)
{
    static char output[20];
    struct tm *utc_time = gmtime(&timestamp);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
    sprintf(output, "%04d-%02d-%02d %02d:%02d:%02d", utc_time->tm_year + 1900, utc_time->tm_mon + 1, utc_time->tm_mday, utc_time->tm_hour, utc_time->tm_min, utc_time->tm_sec);
    return output;
#pragma GCC diagnostic pop
}

/**
 * @brief ip 前缀匹配
 *
 * @param ipa 第一个 ip
 * @param ipb 第二个 ip
 * @return uint8_t 两个 ip 相同的前缀长度
 */
uint8_t ip_prefix_match(uint8_t *ipa, uint8_t *ipb)
{
    uint8_t count = 0;
    for (size_t i = 0; i < 4; i++)
    {
        uint8_t flag = ipa[i] ^ ipb[i];
        for (size_t j = 0; j < 8; j++)
        {
            if (flag & (1 << 7))
                return count;
            else
                count++, flag <<= 1;
        }
    }
    return count;
}

/**
 * @brief 计算 16 位校验和
 *
 * @param buf 要计算的数据包
 * @param len 要计算的长度
 * @return uint16_t 校验和
 */
uint16_t checksum16(uint16_t *data, size_t len)
{
    // TO-DO
    uint16_t checksum16 = 0;

    // Step1
    // 把 data 看成是每 16 个 bit（即 2 个字节）组成一个数，相加。
    // 注意，16 位加法的结果可能会超过 16 位，因此加法结果需要用 32 位数来保存。
    uint32_t res32 = 0;
    for (int i = 0; i < len; i++)
    {
        if (i == len - 1) // Step2 如果最后还剩 8 个 bit 值，也要相加这个 8bit 值。
        {
            res32 += data[i] & 0xFF;
        }
        else
        {
            res32 += data[i];
        }
    }

    // Step3
    // 判断相加后 32bit 结果值的高 16 位是否为 0，如果不为 0，则将高 16 位和低 16 位相加，依次循环，直至高 16 位为 0 为止。
    while (res32 >> 16)
    {
        res32 = (res32 >> 16) + (res32 & 0xFFFF);
    }

    // Step4
    // 将上述的和（低 16 位）取反，即得到校验和。
    checksum16 = ~(uint16_t)res32;
    return checksum16;
}