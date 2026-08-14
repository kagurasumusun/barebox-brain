/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "policy.h"
#include "board.h"

int policy_want_menu(const struct boot_ctx *ctx)
{
    if (ctx->key_held)
        return 1;
    if (ctx->cfg.menu_force)
        return 1;
    if (ctx->bc.valid && (ctx->bc.flags & BOOTCFG_FLAG_ENGINEER))
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

int policy_silent_action(const struct boot_ctx *ctx)
{
    if (ctx->bc.valid && (ctx->bc.flags & BOOTCFG_FLAG_DIAG))
        return ACT_DIAGOS;
    if (ctx->cfg.default_target == BB_DEFAULT_LINUX)
        return ACT_LINUX;
    if (ctx->cfg.default_target == BB_DEFAULT_DIAG)
        return ACT_DIAGOS;
    if (ctx->cfg.default_target == BB_DEFAULT_SDEXE)
        return ACT_SDEXE;
    if (ctx->cfg.default_target == BB_DEFAULT_NONE)
        return ACT_NONE;
    return ACT_WINCE;
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
    if (!from_sd && ok) {
        printf("Boot Config OK!!\n");
        return;
    }
    if (from_sd && !ok)
        printf("SD BootConfig present but invalid\n");
    printf("Check Card BOOT\n");
}

void policy_announce_action(int act)
{
    switch (act) {
    case ACT_DIAGOS:
        printf("Fast Diag Boot!!!\n");
        printf("Reading NK image to SDHC ...\n");
        break;
    case ACT_WINCE:
        printf("Reading NK image to SDHC ...\n");
        break;
    case ACT_NK2:
        printf("Reading NK image to SDHC ...\n");
        break;
    case ACT_SDEXE:
        printf("Check Card BOOT\n");
        printf("Downloading NK RAM image\n");
        break;
    case ACT_LINUX:
        printf("Boot Linux from SD\n");
        break;
    default:
        break;
    }
}
