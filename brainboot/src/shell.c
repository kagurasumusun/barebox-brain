/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "shell.h"
#include "policy.h"
#include "board.h"
#include "ident.h"
#include "keyboard.h"
#include "eboot.h"

#ifdef HOST_BUILD
int shell_run(struct boot_ctx *ctx) { (void)ctx; return ACT_NONE; }
#else

static int read_line(char *buf, int max)
{
    int n = 0;
    buf[0] = 0;
    for (;;) {
        int c;
        if (!uart_tstc()) {
            mdelay(1);
            continue;
        }
        c = uart_getc();
        if (c == '\r' || c == '\n') {
            uart_putc('\n');
            buf[n] = 0;
            return n;
        }
        if (c == 0x08 || c == 0x7f) {
            if (n) {
                n--;
                uart_puts("\b \b");
            }
            continue;
        }
        if (c == 0x03) { /* ^C */
            uart_putc('\n');
            buf[0] = 0;
            return 0;
        }
        if (c >= 32 && c < 127 && n + 1 < max) {
            buf[n++] = (char)c;
            uart_putc((char)c);
        }
    }
}

static u32 parse_u32(const char *s)
{
    u32 v = 0;
    int hex = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        hex = 1;
        s += 2;
    }
    if (!hex) {
        const char *p = s;
        while (*p) {
            if ((*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))
                hex = 1;
            p++;
        }
    }
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9')
            v = hex ? (v * 16u + (u32)(c - '0')) : (v * 10u + (u32)(c - '0'));
        else if (c >= 'a' && c <= 'f')
            v = v * 16u + 10u + (u32)(c - 'a');
        else if (c >= 'A' && c <= 'F')
            v = v * 16u + 10u + (u32)(c - 'A');
        else
            break;
    }
    return v;
}

static const char *next_tok(char **ps)
{
    char *s = *ps;
    char *start;
    while (*s == ' ' || *s == '\t')
        s++;
    if (!*s) {
        *ps = s;
        return NULL;
    }
    start = s;
    while (*s && *s != ' ' && *s != '\t')
        s++;
    if (*s) {
        *s = 0;
        s++;
    }
    *ps = s;
    return start;
}

static void cmd_help(void)
{
    printf("barebox-style commands:\n");
    printf("  help                 this text\n");
    printf("  version              banner / identity\n");
    printf("  detect               live SoC / storage / EDNA2 / I2C\n");
    printf("  ident                DevInfo lookup\n");
    printf("  md [-l] ADDR [N]     memory dump (32-bit)\n");
    printf("  mw ADDR VAL          memory write 32-bit\n");
    printf("  boot wince|linux|diag|sdexe|nk2\n");
    printf("  cfg [flash|sd]        show / write BootConfig\n");
    printf("  go ADDR              branch to address\n");
    printf("  reset                watchdog reset\n");
    printf("  poweroff             HW_POWER_RESET.PWD\n");
    printf("  exit                 leave the shell\n");
}

static void cmd_detect(struct boot_ctx *ctx)
{
    u32 dram = ident_dram_bytes();
    printf("chipid     0x%x\n", board_chipid());
    printf("cpu        %u Hz\n", ctx->cpu_hz);
    printf("ocram      %s\n", ctx->ocram_ok ? "ok" : "fail");
    printf("dram probe %s\n", ctx->dram_ok ? "ok" : "fail");
    if (dram)
        printf("dram ctl   %u MiB (barebox imx28_get_memsize)\n", dram >> 20);
    else
        printf("dram ctl   unreadable / not programmed\n");
    printf("ocotp_lock 0x%x\n", ctx->ocotp_lock);
    printf("power_sts  0x%x\n", ctx->power_sts);
    printf("rtc        %u s\n", ctx->rtc_seconds);
    printf("edna2 db   0x%x @ 0x%x\n", ident_edna2_doorbell(),
           EDNA2_MAILBOX_PHYS + EDNA2_DOORBELL_OFF);
    printf("i2c0 ctrl  0x%x\n", ident_i2c0_ctrl());
    mmc_print_info(&ctx->emmc);
    mmc_print_info(&ctx->sd);
    printf("fat        %s part_lba=%u\n",
           ctx->sdfat.ready ? (ctx->sdfat.fat32 ? "32" : "16") : "none",
           ctx->sdfat.part_lba);
    printf("devinfo    %s ver=%u\n",
           ctx->di.valid ? ctx->di.model : "-",
           ctx->di.valid ? ctx->di.version : 0);
    printf("ident      %s / %s / %s known=%d\n",
           ctx->ident.retail, ctx->ident.internal, ctx->ident.gen,
           ctx->ident.known);
    printf("bootcfg    valid=%d flags=0x%x flash=%d sd=%d\n",
           ctx->bc.valid, ctx->bc.flags, ctx->bc_from_flash, ctx->bc_from_sd);
    printf("factory    %d user=%d develop=%d system=%d card_os=%d\n",
           ctx->factory_ok, ctx->user_ok, ctx->develop_ok, ctx->system_ok,
           ctx->card_exe_os);
    printf("bootmenu   valid=%d flags=0x%x\n", ctx->bm.valid, ctx->bm.flags);
}

static void cmd_ident(struct boot_ctx *ctx)
{
    printf("%s\n", ctx->ident.banner);
    printf("raw=%s key=%s retail=%s gen=%s ver=%u from_devinfo=%d known=%d\n",
           ctx->ident.raw_model, ctx->ident.key, ctx->ident.retail,
           ctx->ident.gen, ctx->ident.version, ctx->ident.from_devinfo,
           ctx->ident.known);
    printf("cfg=%s exe=%s\n", ctx->ident.cfg_name, ctx->ident.exe_name);
}

static void cmd_md(const char *a1, const char *a2, const char *a3)
{
    u32 addr, n, i;
    const char *ap = a1;
    if (a1 && a1[0] == '-' ) {
        ap = a2;
        a2 = a3;
    }
    if (!ap) {
        printf("md: need address\n");
        return;
    }
    addr = parse_u32(ap);
    n = a2 ? parse_u32(a2) : 16;
    if (n > 256)
        n = 256;
    for (i = 0; i < n; i++) {
        if ((i & 3) == 0)
            printf("%x: ", addr + i * 4);
        printf("%x ", readl(addr + i * 4));
        if ((i & 3) == 3)
            printf("\n");
    }
    if (n & 3)
        printf("\n");
}

static int cmd_boot(struct boot_ctx *ctx, const char *what)
{
    (void)ctx;
    if (!what) {
        printf("boot wince|linux|diag|sdexe|nk2\n");
        return ACT_NONE;
    }
    if (strcmp(what, "wince") == 0 || strcmp(what, "nk") == 0)
        return ACT_WINCE;
    if (strcmp(what, "linux") == 0)
        return ACT_LINUX;
    if (strcmp(what, "diag") == 0)
        return ACT_DIAGOS;
    if (strcmp(what, "sdexe") == 0)
        return ACT_SDEXE;
    if (strcmp(what, "nk2") == 0)
        return ACT_NK2;
    printf("boot: unknown target '%s'\n", what);
    return ACT_NONE;
}

int shell_run(struct boot_ctx *ctx)
{
    printf("barebox command line. type `help'. `exit' returns.\n");
    for (;;) {
        char line[160];
        char *p;
        const char *cmd, *a1, *a2, *a3;
        int act;

        uart_puts("barebox@brain:/ ");
        read_line(line, (int)sizeof(line));
        p = line;
        cmd = next_tok(&p);
        if (!cmd)
            continue;
        a1 = next_tok(&p);
        a2 = next_tok(&p);
        a3 = next_tok(&p);

        if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0)
            cmd_help();
        else if (strcmp(cmd, "version") == 0)
            printf("brainboot %s  %s\n", BB_VERSION, ctx->ident.banner);
        else if (strcmp(cmd, "detect") == 0)
            cmd_detect(ctx);
        else if (strcmp(cmd, "ident") == 0)
            cmd_ident(ctx);
        else if (strcmp(cmd, "md") == 0)
            cmd_md(a1, a2, a3);
        else if (strcmp(cmd, "mw") == 0) {
            if (!a1 || !a2)
                printf("mw ADDR VAL\n");
            else
                writel(parse_u32(a2), parse_u32(a1));
        } else if (strcmp(cmd, "boot") == 0) {
            act = cmd_boot(ctx, a1);
            if (act)
                return act;
        } else if (strcmp(cmd, "cfg") == 0) {
            if (!a1) {
                printf("flags=0x%x valid=%d flash=%d sd=%d\n",
                       ctx->bc.flags, ctx->bc.valid,
                       ctx->bc_from_flash, ctx->bc_from_sd);
            } else if (strcmp(a1, "flash") == 0)
                printf("cfg flash rc=%d\n",
                       eboot_store_bootcfg(&ctx->emmc, ctx->bc.flags,
                                           ctx->bc.devflags, ctx->bc.model));
            else if (strcmp(a1, "sd") == 0)
                printf("cfg sd rc=%d\n",
                       eboot_store_bootcfg_sd(&ctx->sdfat, ctx->bc.flags,
                                              ctx->bc.devflags, ctx->bc.model));
            else
                printf("cfg [flash|sd]\n");
        } else if (strcmp(cmd, "go") == 0) {
            if (!a1)
                printf("go ADDR\n");
            else
                jump_os(0, 0, 0, parse_u32(a1));
        } else if (strcmp(cmd, "reset") == 0)
            return ACT_REBOOT;
        else if (strcmp(cmd, "poweroff") == 0)
            return ACT_POWEROFF;
        else if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0)
            return ACT_NONE;
        else
            printf("unknown command '%s' (help)\n", cmd);
    }
}
#endif
