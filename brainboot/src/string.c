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
#endif

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
