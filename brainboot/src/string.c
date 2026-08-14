/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "brainboot.h"

#ifndef HOST_BUILD
void *memcpy(void *dst, const void *src, size_t n)
{
    u8 *d = dst;
    const u8 *s = src;
    while (n--)
        *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    u8 *d = dst;
    const u8 *s = src;
    if (d == s || n == 0)
        return dst;
    if (d < s)
        return memcpy(dst, src, n);
    d += n;
    s += n;
    while (n--)
        *--d = *--s;
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    u8 *d = dst;
    while (n--)
        *d++ = (u8)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const u8 *pa = a, *pb = b;
    while (n--) {
        if (*pa != *pb)
            return (int)*pa - (int)*pb;
        pa++;
        pb++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return (int)(u8)*a - (int)(u8)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n && *a && *a == *b) {
        a++;
        b++;
        n--;
    }
    if (n == 0)
        return 0;
    return (int)(u8)*a - (int)(u8)*b;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != 0)
        ;
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++)
        dst[i] = src[i];
    for (; i < n; i++)
        dst[i] = 0;
    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    return (c == 0) ? (char *)s : NULL;
}
#endif

void utoa_dec(u32 v, char *buf)
{
    char tmp[11];
    int i = 0, j = 0;
    if (v == 0) {
        buf[0] = '0';
        buf[1] = 0;
        return;
    }
    while (v && i < 10) {
        tmp[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i--)
        buf[j++] = tmp[i];
    buf[j] = 0;
}

void utoa_hex(u32 v, char *buf, int width)
{
    const char *h = "0123456789abcdef";
    int i;
    if (width < 1)
        width = 1;
    if (width > 8)
        width = 8;
    for (i = 0; i < width; i++)
        buf[i] = h[(v >> ((width - 1 - i) * 4)) & 0xf];
    buf[width] = 0;
}

int snprint(char *dst, u32 dstsz, const char *fmt, ...)
{
    va_list ap;
    u32 n = 0;
    if (!dst || dstsz == 0)
        return 0;
    va_start(ap, fmt);
    while (*fmt && n + 1 < dstsz) {
        if (*fmt != '%') {
            dst[n++] = *fmt++;
            continue;
        }
        fmt++;
        while (*fmt == '0' || (*fmt >= '1' && *fmt <= '9'))
            fmt++;
        if (*fmt == 's') {
            const char *s = va_arg(ap, const char *);
            if (!s)
                s = "";
            while (*s && n + 1 < dstsz)
                dst[n++] = *s++;
        } else if (*fmt == 'd' || *fmt == 'u') {
            char tmp[12];
            char *t;
            utoa_dec(va_arg(ap, unsigned int), tmp);
            t = tmp;
            while (*t && n + 1 < dstsz)
                dst[n++] = *t++;
        } else if (*fmt == 'x') {
            char tmp[9];
            char *t;
            utoa_hex(va_arg(ap, unsigned int), tmp, 8);
            t = tmp;
            while (*t && n + 1 < dstsz)
                dst[n++] = *t++;
        } else if (*fmt == '%') {
            dst[n++] = '%';
        }
        if (*fmt)
            fmt++;
    }
    va_end(ap);
    dst[n] = 0;
    return (int)n;
}

void strtoupper(char *s)
{
    while (*s) {
        if (*s >= 'a' && *s <= 'z')
            *s = (char)(*s - 'a' + 'A');
        s++;
    }
}

u16 sum16(const u8 *data, u32 len)
{
    u32 s = 0;
    u32 i;
    for (i = 0; i < len; i++)
        s += data[i];
    return (u16)(s & 0xffffu);
}

u32 sum32(const u8 *data, u32 len)
{
    u32 s = 0;
    u32 i;
    for (i = 0; i < len; i++)
        s += data[i];
    return s;
}

/* Standard IEEE CRC-32 (poly 0xEDB88320), reflected. */
u32 crc32_ieee(const u8 *data, u32 len)
{
    u32 crc = 0xffffffffu;
    u32 i, b, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            b = crc & 1u;
            crc >>= 1;
            if (b)
                crc ^= 0xEDB88320u;
        }
    }
    return crc ^ 0xffffffffu;
}
