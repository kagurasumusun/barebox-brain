/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"

#ifndef HOST_BUILD

static void uart_wait_tx(void)
{
    u32 guard = 100000;
    while ((readl(IMX_DBGUART_BASE + UART_FR) & UART_FR_TXFF) && --guard)
        ;
}

void uart_init(void)
{
    u32 base = IMX_DBGUART_BASE;
    u32 ibrd, fbrd;
    u32 div;

    /* 24 MHz XTAL, 115200 8N1. IBRD = UARTCLK/(16*baud) */
    writel(0, base + UART_CR);
    div = (XTAL_FREQ_HZ * 4u) / 115200u; /* 64ths */
    ibrd = div >> 6;
    fbrd = div & 63u;
    if (ibrd == 0) {
        ibrd = 13;
        fbrd = 1;
    }
    writel(ibrd, base + UART_IBRD);
    writel(fbrd, base + UART_FBRD);
    writel(UART_LCRH_WLEN_8 | UART_LCRH_FEN, base + UART_LCRH);
    writel(0xffffu, base + UART_ICR);
    writel(UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE, base + UART_CR);
}

void uart_putc(char c)
{
    if (c == '\n')
        uart_putc('\r');
    uart_wait_tx();
    writel((u32)(u8)c, IMX_DBGUART_BASE + UART_DR);
}

void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

int uart_tstc(void)
{
    return (readl(IMX_DBGUART_BASE + UART_FR) & UART_FR_RXFE) ? 0 : 1;
}

int uart_getc(void)
{
    while (!uart_tstc())
        ;
    return (int)(readl(IMX_DBGUART_BASE + UART_DR) & 0xffu);
}

int uart_getc_timeout(u32 ms)
{
    while (ms--) {
        if (uart_tstc())
            return uart_getc();
        mdelay(1);
    }
    return -1;
}

#else
void uart_init(void) {}
void uart_putc(char c) { putchar(c); }
void uart_puts(const char *s) { fputs(s, stdout); }
int uart_tstc(void) { return 0; }
int uart_getc(void) { return -1; }
int uart_getc_timeout(u32 ms) { (void)ms; return -1; }
#endif

static void put_dec(u32 v)
{
    char buf[10];
    int i = 0;
    if (v == 0) {
        uart_putc('0');
        return;
    }
    while (v && i < 10) {
        buf[i++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (i--)
        uart_putc(buf[i]);
}

static void put_hex(u32 v, int width)
{
    const char *hex = "0123456789abcdef";
    int i;
    for (i = (width - 1) * 4; i >= 0; i -= 4)
        uart_putc(hex[(v >> i) & 0xf]);
}

void printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    while (*fmt) {
        if (*fmt != '%') {
            uart_putc(*fmt++);
            continue;
        }
        fmt++;
        if (*fmt == '0')
            fmt++;
        if (*fmt == '8' || *fmt == '2' || *fmt == '4')
            fmt++;
        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            uart_puts(s ? s : "(null)");
            break;
        }
        case 'c':
            uart_putc((char)va_arg(ap, int));
            break;
        case 'd':
        case 'u':
            put_dec((u32)va_arg(ap, unsigned int));
            break;
        case 'x':
        case 'p':
            put_hex(va_arg(ap, unsigned int), 8);
            break;
        case '%':
            uart_putc('%');
            break;
        default:
            uart_putc('%');
            if (*fmt)
                uart_putc(*fmt);
            break;
        }
        if (*fmt)
            fmt++;
    }
    va_end(ap);
}

void hexdump(const void *buf, u32 len)
{
    const u8 *p = buf;
    u32 i, j;
    for (i = 0; i < len; i += 16) {
        put_hex(i, 8);
        uart_puts("  ");
        for (j = 0; j < 16; j++) {
            if (i + j < len)
                put_hex(p[i + j], 2);
            else
                uart_puts("  ");
            uart_putc(' ');
        }
        uart_puts(" |");
        for (j = 0; j < 16 && i + j < len; j++) {
            u8 c = p[i + j];
            uart_putc((c >= 32 && c < 127) ? (char)c : '.');
        }
        uart_puts("|\n");
    }
}

void hang(const char *why)
{
    printf("FATAL: %s\n", why ? why : "unknown");
#ifndef HOST_BUILD
    for (;;)
        ;
#else
    exit(1);
#endif
}
