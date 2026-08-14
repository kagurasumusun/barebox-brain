/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BRAINBOOT_H
#define BRAINBOOT_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t  s32;
typedef int      bool_t;

#define TRUE  1
#define FALSE 0

#define ALIGN_UP(x, a)   (((x) + ((a) - 1)) & ~((a) - 1))
#define ALIGN_DOWN(x, a) ((x) & ~((a) - 1))
#define ARRAY_SIZE(a)    (sizeof(a) / sizeof((a)[0]))
#define MIN(a, b)        ((a) < (b) ? (a) : (b))
#define MAX(a, b)        ((a) > (b) ? (a) : (b))

#ifdef HOST_BUILD
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define readl(a)  (*(volatile u32 *)(uintptr_t)(a))
#define writel(v, a) do { *(volatile u32 *)(uintptr_t)(a) = (v); } while (0)
#define bb_printf printf
#else
#define readl(a)       (*(volatile u32 *)(a))
#define readw(a)       (*(volatile u16 *)(a))
#define readb(a)       (*(volatile u8 *)(a))
#define writel(v, a)   do { *(volatile u32 *)(a) = (v); } while (0)
#define writew(v, a)   do { *(volatile u16 *)(a) = (v); } while (0)
#define writeb(v, a)   do { *(volatile u8 *)(a) = (v); } while (0)
#define setl(v, a)     writel(readl(a) | (v), a)
#define clrl(v, a)     writel(readl(a) & ~(v), a)
#endif

/* MXS-style SET/CLR/TOG windows */
#define REG_SET  0x4
#define REG_CLR  0x8
#define REG_TOG  0xc
#define writel_set(v, a) writel((v), (u32)(a) + REG_SET)
#define writel_clr(v, a) writel((v), (u32)(a) + REG_CLR)

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *dst, const char *src);
char *strncpy(char *dst, const char *src, size_t n);
void strtoupper(char *s);
char *strchr(const char *s, int c);
void utoa_dec(u32 v, char *buf);
void utoa_hex(u32 v, char *buf, int width);
int  snprint(char *dst, u32 dstsz, const char *fmt, ...);

u32 crc32_ieee(const u8 *data, u32 len);
u16 sum16(const u8 *data, u32 len);
u32 sum32(const u8 *data, u32 len);

void delay_cycles(u32 cycles);
void udelay(u32 us);
void mdelay(u32 ms);

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
int uart_tstc(void);
int uart_getc(void);
int uart_getc_timeout(u32 ms);
#ifndef HOST_BUILD
void printf(const char *fmt, ...);
#endif
void hexdump(const void *buf, u32 len);

void hang(const char *why) __attribute__((noreturn));

#endif
