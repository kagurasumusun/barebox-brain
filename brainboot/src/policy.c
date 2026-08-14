/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "policy.h"
#include "board.h"

int policy_want_menu(const struct boot_ctx *ctx)
{
    if (ctx->key_held)
        return 1;
    if (ctx->cfg.loaded && ctx->cfg.menu_force)
        return 1;
    if (ctx->bm.valid && (ctx->bm.flags & BOOTMENU_FLAG_SHOW))
        return 1;
    return 0;
}

int policy_want_shell(const struct boot_ctx *ctx)
{
    if (ctx->bm.valid && (ctx->bm.flags & BOOTMENU_FLAG_SHELL))
        return 1;
    return 0;
}

int policy_stock_action(const struct boot_ctx *ctx)
{
    if (ctx->bc.valid && (ctx->bc.flags & BOOTCFG_FLAG_DIAG))
        return ACT_DIAGOS;
    if (!ctx->bc.valid && ctx->card_exe_os)
        return ACT_SDEXE;
    return ACT_WINCE;
}

int policy_silent_action(const struct boot_ctx *ctx)
{
    /* Silent path is stock EBOOT only. BRAINBOO.CFG default is UI-only. */
    return policy_stock_action(ctx);
}

void policy_announce_devinfo(const struct boot_ctx *ctx)
{
    if (ctx->di.valid)
        printf("DevInfo OK!!\n");
    else
        printf("DevInfo: missing or bad checksum\n");
}

void policy_announce_bootcfg(const struct boot_ctx *ctx, int from_sd, int ok)
{
    (void)ctx;
    if (from_sd && ok) {
        printf("Find Boot Config File!!\n");
        printf("Boot Config OK!!\n");
        return;
    }
    if (!from_sd && ok)
        printf("Boot Config OK!!\n");
}

void policy_announce_action(const struct boot_ctx *ctx, int act)
{
    switch (act) {
    case ACT_DIAGOS:
        printf("Fast Diag Boot!!!\n");
        break;
    case ACT_WINCE:
        if (!ctx->bc.valid)
            printf("INFO: Check Card BOOT\n");
        break;
    case ACT_SDEXE:
        printf("INFO: Check Card BOOT\n");
        printf("Enable Card Boot!!!\n");
        break;
    case ACT_LINUX:
        printf("Boot Linux from SD\n");
        break;
    default:
        break;
    }
    if (ctx->bc.valid && (ctx->bc.flags & BOOTCFG_FLAG_DEVMODE))
        printf("Use Dev Mode = 0x%x\n", ctx->bc.flags);
}
