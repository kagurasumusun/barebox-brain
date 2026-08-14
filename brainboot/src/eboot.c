/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "eboot.h"
#include "board.h"
#include "keyboard.h"
#include "policy.h"
#include "ebl.h"

#ifndef HOST_BUILD

int eboot_check_resume(struct resume_info *out)
{
    const volatile u32 *p = (const volatile u32 *)RESUME_INFO_PHYS;

    out->uiChecksum = p[0];
    out->uiChecksumRev = p[1];
    out->uiStrSize = p[2];
    printf("***************CheckResumeInfo:pStrRegsResumeInfo=%x\n",
           RESUME_INFO_PHYS + 0x60000000u);
    printf("***************CheckResumeInfo:uiStrSize=%u\n", out->uiStrSize);
    if (out->uiStrSize == 0) {
        printf("***************CheckResumeInfo:bRet=FALSE\n");
        printf("***************CheckResumeInfo:uiSum=0x0, pStrRegsResumeInfo->uiChecksum=0x%x\n",
               out->uiChecksum);
        printf("***************CheckResumeInfo:uiSumRev=0x0, pStrRegsResumeInfo->uiChecksumRev=0x%x\n",
               out->uiChecksumRev);
        return 0;
    }
    {
        u32 sum = 0, i;
        const u8 *b = (const u8 *)RESUME_INFO_PHYS;
        for (i = 8; i < out->uiStrSize && i < 0x1000u; i++)
            sum += b[i];
        printf("***************CheckResumeInfo:uiSum=0x%x, pStrRegsResumeInfo->uiChecksum=0x%x\n",
               sum, out->uiChecksum);
        printf("***************CheckResumeInfo:uiSumRev=0x%x, pStrRegsResumeInfo->uiChecksumRev=0x%x\n",
               (~sum), out->uiChecksumRev);
        if (sum == out->uiChecksum && ((~sum) == out->uiChecksumRev))
            return 1;
    }
    printf("***************CheckResumeInfo:bRet=FALSE\n");
    return 0;
}

int eboot_store_bootcfg(struct mmc_dev *emmc, u32 flags, u32 devflags, u32 model)
{
    u8 sec[512];

    if (!emmc || !emmc->ready)
        return -1;
    bootcfg_build(flags, devflags, model, sec);
    printf("INFO: Storing boot configuration to SDHC\n");
    if (mmc_write(emmc, EMMC_BOOTCFG_LBA, 1, sec) != 0) {
        printf("ERROR: StoreBootCFG: failed to write configuration.\n");
        return -2;
    }
    printf("INFO: Successfully stored boot configuration to SDHC\n");
    return 0;
}

int eboot_store_bootcfg_sd(struct fat_fs *sd, const char *name,
                           u32 flags, u32 devflags, u32 model)
{
    u8 sec[512];

    if (!sd || !sd->ready)
        return -1;
    if (!name || !name[0])
        name = SD_BOOTCFG_NAME;
    bootcfg_build(flags, devflags, model, sec);
    return fat_write(sd, name, sec, 512);
}

int eboot_store_devinfo(struct mmc_dev *emmc, const char *model, u32 version)
{
    u8 sec[512];

    if (!emmc || !emmc->ready)
        return -1;
    devinfo_build(model ? model : "EDSH6", version ? version : 4, sec);
    return mmc_write(emmc, EMMC_DEVINFO_LBA, 1, sec);
}

int eboot_store_devinfo_sd(struct fat_fs *sd, const char *name,
                           const char *model, u32 version)
{
    u8 sec[512];

    if (!sd || !sd->ready)
        return -1;
    if (!name || !name[0])
        name = SD_DEVINFO_NAME;
    devinfo_build(model ? model : "EDSH6", version ? version : 4, sec);
    return fat_write(sd, name, sec, 512);
}

int eboot_store_factory(struct mmc_dev *emmc)
{
    u8 sec[512];

    if (!emmc || !emmc->ready)
        return -1;
    factory_setting_build(sec);
    return mmc_write(emmc, EMMC_FACTORY_LBA, 1, sec);
}

int eboot_store_charge(struct mmc_dev *emmc, const struct charge_info *ci)
{
    u8 sec[512];

    if (!emmc || !emmc->ready || !ci)
        return -1;
    chargeinfo_build(ci, sec);
    return mmc_write(emmc, EMMC_CHARGE_LBA, 1, sec);
}

int eboot_reset_default_bootcfg(struct mmc_dev *emmc)
{
    printf("\nResetting factory default configuration...\n");
    if (eboot_store_bootcfg(emmc, 0, 0, 0) != 0) {
        printf("ERROR: ResetDefaultBootCFG: failed to store configuration to flash.\n");
        return -1;
    }
    return 0;
}

static int read_lba(struct mmc_dev *dev, u32 lba, u8 sec[512])
{
    if (!dev || !dev->ready)
        return -1;
    return mmc_read(dev, lba, 1, sec);
}

static int fat_try_find(struct fat_fs *sd, const char *name, struct fat_file *f)
{
    if (!sd || !sd->ready || !name || !name[0])
        return -1;
    return fat_find(sd, name, f);
}

static void load_bootstatus(struct boot_ctx *ctx)
{
    u8 sec[512];
    u32 try_lba[] = { EMMC_BOOTSTATUS_LBA, 32u, 48u, 72u };
    unsigned i;

    memset(&ctx->bs, 0, sizeof(ctx->bs));
    printf("LoadBootStatusInfo()++\n");
    for (i = 0; i < ARRAY_SIZE(try_lba); i++) {
        if (read_lba(&ctx->emmc, try_lba[i], sec) == 0 &&
            bootstatus_parse(sec, &ctx->bs) == 0)
            break;
    }
    printf("LoadBootStatusInfo()-- bRet=%d status=0x%08x\n",
           ctx->bs.valid, ctx->bs.status);
    if (ctx->bs.valid)
        printf("Boot Status OK!!\n");
}

static void load_charge(struct boot_ctx *ctx)
{
    u8 sec[512];

    memset(&ctx->ci, 0, sizeof(ctx->ci));
    printf("LoadChargeInfo()++\n");
    printf("LoadChargeInfoSub()++\n");
    if (read_lba(&ctx->emmc, EMMC_CHARGE_LBA, sec) == 0 &&
        chargeinfo_parse(sec, &ctx->ci) == 0) {
        printf("ChargeInfo OK!!\n");
        printf("ChargeStartCount[0]=%u\n", ctx->ci.start_count[0]);
        printf("ChargeCompleteCount[0]=%u\n", ctx->ci.complete_count[0]);
        printf("ChargeTempErrorCount[0]=%u\n", ctx->ci.temp_error_count[0]);
        printf("ChargeTotalTime[0]=%u\n", ctx->ci.total_time[0]);
        printf("ChargeStartCount[1]=%u\n", ctx->ci.start_count[1]);
        printf("ChargeCompleteCount[1]=%u\n", ctx->ci.complete_count[1]);
        printf("ChargeTempErrorCount[1]=%u\n", ctx->ci.temp_error_count[1]);
        printf("ChargeTotalTime[1]=%u\n", ctx->ci.total_time[1]);
        printf("LoadChargeInfoSub()-- bRet=1\n");
        printf("LoadChargeInfo()-- bRet=1\n");
        return;
    }
    printf("LoadChargeInfoSub()-- bRet=0\n");
    printf("LoadChargeInfo()-- bRet=0\n");
}

static void load_settings(struct boot_ctx *ctx)
{
    u8 sec[512];

    printf("LoadFactorySetting()++\n");
    ctx->factory_ok = 0;
    memset(&ctx->factory, 0, sizeof(ctx->factory));
    if (read_lba(&ctx->emmc, EMMC_FACTORY_LBA, sec) == 0 &&
        factory_setting_parse(sec, &ctx->factory) == 0)
        ctx->factory_ok = 1;
    printf("LoadFactorySetting()-- bRet=%d\n", ctx->factory_ok);
    if (!ctx->factory_ok)
        printf("ERROR: Load Factory Setting\n");

    printf("LoadUserSetting()++\n");
    ctx->user_ok = 0;
    memset(&ctx->user, 0, sizeof(ctx->user));
    if (read_lba(&ctx->emmc, EMMC_USER_LBA, sec) == 0 &&
        sec[0] == '7' && sec[1] == '0') {
        ctx->user_ok = 1;
        packed70_parse(sec, 512, &ctx->user);
    }
    printf("LoadUserSetting()-- bRet=%d\n", ctx->user_ok);
    if (!ctx->user_ok)
        printf("ERROR: Load User Setting\n");

    printf("LoadDevelopSetting()++\n");
    ctx->develop_ok = 0;
    memset(&ctx->develop, 0, sizeof(ctx->develop));
    if (read_lba(&ctx->emmc, EMMC_DEVELOP_LBA, sec) == 0 &&
        sec[0] == '7' && sec[1] == '0') {
        ctx->develop_ok = 1;
        packed70_parse(sec, 512, &ctx->develop);
    }
    printf("LoadDevelopSetting()-- bRet=%d\n", ctx->develop_ok);
    if (!ctx->develop_ok)
        printf("ERROR: Load Develop Setting\n");
}

static void load_system_setting(struct boot_ctx *ctx)
{
    u8 sec[512];

    printf("LoadSystemSetting()++\n");
    ctx->system_ok = 0;
    memset(&ctx->system, 0, sizeof(ctx->system));
    if (read_lba(&ctx->emmc, EMMC_SYSTEM_LBA, sec) == 0 &&
        sec[0] == '7' && sec[1] == '0') {
        ctx->system_ok = 1;
        packed70_parse(sec, 512, &ctx->system);
    }
    printf("LoadSystemSetting()-- bRet=%d\n", ctx->system_ok);
    if (!ctx->system_ok)
        printf("ERROR: Load System Setting\n");
}

static void load_bootcfg(struct boot_ctx *ctx)
{
    u8 sec[512];
    struct fat_file f;
    int ok = 0;

    memset(&ctx->bc, 0, sizeof(ctx->bc));
    ctx->bc_from_flash = 0;
    ctx->bc_from_sd = 0;

    if (read_lba(&ctx->emmc, EMMC_BOOTCFG_LBA, sec) == 0 &&
        bootcfg_parse(sec, &ctx->bc) == 0) {
        printf("INFO: Successfully loaded boot configuration from SDHC\n");
        ok = 1;
        ctx->bc_from_flash = 1;
    } else {
        printf("ERROR: LoadBootCFG: failed to load configuration.\n");
        printf("ERROR: flash initialization failed - loading bootloader defaults...\n");
        if (eboot_reset_default_bootcfg(&ctx->emmc) == 0) {
            ctx->bc.flags = 0;
            ctx->bc.devflags = 0;
            ctx->bc.model = 0;
            ctx->bc.valid = 1;
            ctx->bc_from_flash = 1;
            ok = 1;
        }
    }

    printf("LoadBootConfig()++\n");
    printf("LoadBootConfigSub()++\n");
    if (fat_try_find(&ctx->sdfat, ctx->ident.cfg_name, &f) == 0 ||
        fat_try_find(&ctx->sdfat, SD_BOOTCFG_NAME, &f) == 0) {
        if (fat_read(&ctx->sdfat, &f, sec, 512) >= 48 &&
            bootcfg_parse(sec, &ctx->bc) == 0) {
            ok = 1;
            ctx->bc_from_sd = 1;
        } else
            printf("Cannot Read Boot Config File!!\n");
    }
    printf("LoadBootConfigSub()-- bRet=%d\n", ctx->bc_from_sd);
    printf("LoadBootConfig()-- bRet=%d\n", ctx->bc_from_sd);
    if (ctx->bc.valid) {
        char fl[96];
        bootcfg_describe_flags(ctx->bc.flags, fl, sizeof(fl));
        printf("BootConfig flags=0x%x %s\n", ctx->bc.flags, fl);
    }
    policy_announce_bootcfg(ctx, ctx->bc_from_sd, ok);
}

static void load_devinfo(struct boot_ctx *ctx)
{
    u8 sec[512];
    struct fat_file f;

    memset(&ctx->di, 0, sizeof(ctx->di));
    printf("LoadBootDevInfo()++\n");
    printf("LoadBootDevInfoSub()++\n");
    if (fat_try_find(&ctx->sdfat, ctx->ident.dev_name, &f) == 0 ||
        fat_try_find(&ctx->sdfat, SD_DEVINFO_NAME, &f) == 0 ||
        fat_try_find(&ctx->sdfat, SD_DEVINFO_LONG, &f) == 0) {
        printf("Find Dev Info File!!\n");
        if (fat_read(&ctx->sdfat, &f, sec, 512) >= 48)
            devinfo_parse(sec, &ctx->di);
        else
            printf("Cannot Read Dev Info File!!\n");
    }
    if (!ctx->di.valid && read_lba(&ctx->emmc, EMMC_DEVINFO_LBA, sec) == 0)
        devinfo_parse(sec, &ctx->di);
    ident_from_devinfo(&ctx->ident, &ctx->di);
    policy_announce_devinfo(ctx);
    printf("LoadBootDevInfoSub()-- bRet=%d\n", ctx->di.valid);
    printf("LoadBootDevInfo()-- bRet=%d\n", ctx->di.valid);
    if (ctx->di.valid) {
        printf("pDriverGlobalWork->model=[%s]\n", ctx->ident.raw_model);
        printf("pDriverGlobalWork->nDeviceInfo=0x%x\n", ctx->di.version);
    }
}

void eboot_load_records(struct boot_ctx *ctx)
{
    load_bootstatus(ctx);
    load_settings(ctx);
    /* DevInfo first so ident.{cfg,exe,dev}_name match the model. */
    load_devinfo(ctx);
    load_bootcfg(ctx);
    load_charge(ctx);
}

void eboot_probe_card_exe(struct boot_ctx *ctx)
{
    struct fat_file f;
    u8 head[512];
    const char *name;

    ctx->card_exe_os = 0;
    ctx->card_exe_size = 0;
    name = ctx->ident.exe_name[0] ? ctx->ident.exe_name : SD_BOOTEXE_NAME;
    if (fat_try_find(&ctx->sdfat, name, &f) != 0 &&
        fat_try_find(&ctx->sdfat, SD_BOOTEXE_NAME, &f) != 0)
        return;
    ctx->card_exe_size = f.size;
    /* Our own EDSH6EXE.BIN is ~55 KiB. Stock NK/DiagOS wrappers are ≥ 1 MiB. */
    if (f.size < CARD_BOOT_MIN_OS)
        return;
    if (fat_read(&ctx->sdfat, &f, head, 512) < 16)
        return;
    if (boot_image_is_os(head, f.size))
        ctx->card_exe_os = 1;
}

void eboot_display_init(struct boot_ctx *ctx)
{
    printf("DisplayInit()++\n");
    eboot_setup_pixclock();
    printf("ConfigurePanel\n");
    if (!lcd_ready())
        lcd_init();
    printf("Display first image\n");
    splash_draw();
    printf("mpulcd_init_panel_hw pass\n");
    printf("InitSharp() pass \n");
    printf("DisplayInit()--\n");
    load_system_setting(ctx);
    printf("Display second image\n");
    splash_second(ctx->ident.retail[0] ? ctx->ident.retail : BOARD_NAME);
}

void eboot_beep(void)
{
    printf("LoadBeepSound()\n");
    printf("MCU_REG_RESET_INIT\n");
    printf("BeepSound()++\n");
    printf("BeepSound()-- bRet=%d\n", ebl_beep(40) == 0);
}

void eboot_announce_mmc(const struct boot_ctx *ctx)
{
    printf("+ SDMMC_Init\n");
    if (ctx->emmc.ready && !ctx->emmc.is_sd) {
        printf("%s\n", ctx->emmc.is_hc ? "MMC High Density card" : "MMC Low Density card");
        if (ctx->emmc.bus_width == 8)
            printf("MMC 8bit\n");
        else if (ctx->emmc.bus_width == 4)
            printf("MMC: Switched to 4 bit mode\n");
        printf("INFO: Initialized MMC Card 0\n");
    }
    if (ctx->sd.ready && ctx->sd.is_sd)
        printf("INFO: Initialized SD Card 1\n");
    else if (!ctx->sd.ready)
        printf("ERROR: Failed to read MBR from SDHC\n");
    printf("- SDMMC_Init u32Init=0x%x\n", ctx->emmc.ready ? 1u : 0u);
    printf("WARNING: SDMMC_Init: g_bSDHCExist[0]=%d g_bSDHCExist[1]=%d \n",
           ctx->emmc.ready, ctx->sd.ready);
    if (ctx->emmc.ready)
        printf("OEMPlatformInit: SWITCH SDHC device to user partition.\n");
}

#else
int eboot_check_resume(struct resume_info *out)
{
    memset(out, 0, sizeof(*out));
    return 0;
}
int eboot_store_bootcfg(struct mmc_dev *e, u32 a, u32 b, u32 c)
{
    (void)e;(void)a;(void)b;(void)c;
    return -1;
}
int eboot_reset_default_bootcfg(struct mmc_dev *e) { (void)e; return -1; }
int eboot_store_bootcfg_sd(struct fat_fs *s, const char *n, u32 a, u32 b, u32 c)
{
    (void)s;(void)n;(void)a;(void)b;(void)c;
    return -1;
}
int eboot_store_devinfo(struct mmc_dev *e, const char *m, u32 v)
{
    (void)e;(void)m;(void)v;
    return -1;
}
int eboot_store_devinfo_sd(struct fat_fs *s, const char *n, const char *m, u32 v)
{
    (void)s;(void)n;(void)m;(void)v;
    return -1;
}
int eboot_store_factory(struct mmc_dev *e) { (void)e; return -1; }
int eboot_store_charge(struct mmc_dev *e, const struct charge_info *c)
{
    (void)e;(void)c;
    return -1;
}
void eboot_load_records(struct boot_ctx *ctx) { (void)ctx; }
void eboot_probe_card_exe(struct boot_ctx *ctx) { (void)ctx; }
void eboot_display_init(struct boot_ctx *ctx) { (void)ctx; }
void eboot_beep(void) {}
void eboot_announce_mmc(const struct boot_ctx *ctx) { (void)ctx; }
#endif
