/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ebl.h"
#include "board.h"
#include "gpio.h"
#include "i2c.h"
#include "ident.h"

#ifndef HOST_BUILD

static u32 read_cp15_cache_type(void)
{
    u32 v;
    asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(v));
    return v;
}

static u32 read_cp15_cache_size(void)
{
    u32 v;
    asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(v));
    return v;
}

static void lradc_kick_ch7(void)
{
    u32 base = IMX_LRADC_BASE;
    u32 g = 10000;
    writel_clr(LRADC_CTRL0_SFTRST | LRADC_CTRL0_CLKGATE, base + HW_LRADC_CTRL0);
    writel(LRADC_CTRL1_IRQ(7), base + HW_LRADC_CTRL1 + REG_CLR);
    writel_set(LRADC_CTRL0_SCHEDULE(7), base + HW_LRADC_CTRL0);
    while (g--) {
        if (readl(base + HW_LRADC_CTRL1) & LRADC_CTRL1_IRQ(7))
            break;
        udelay(10);
    }
}

void ebl_rtc_read(struct ebl_rtc *r)
{
    u32 stat, sec, msec, days, rem;
    memset(r, 0, sizeof(*r));
    stat = readl(IMX_RTC_BASE + HW_RTC_STAT);
    r->rtc_stat = stat;
    r->present = (stat & RTC_STAT_RTC_PRESENT) ? 1 : 0;
    r->alarm_present = (stat & RTC_STAT_ALARM_PRESENT) ? 1 : 0;
    if (!r->present) {
        printf("ERROR: InitRTC: NO RTC Present!\n");
        return;
    }
    r->seconds = readl(IMX_RTC_BASE + HW_RTC_SECONDS);
    msec = readl(IMX_RTC_BASE + HW_RTC_MILLISECONDS);
    r->msec = msec % 1000u;
    sec = r->seconds;
    days = sec / 86400u;
    rem = sec % 86400u;
    r->hour = rem / 3600u;
    r->min = (rem % 3600u) / 60u;
    r->sec = rem % 60u;
    /* Civil date from days since 1980-01-01 (CE SYSTEMTIME epoch used by
     * OEMGetRealTime on this BSP). Not claimed as UTC. */
    {
        u32 y = 1980, d = days;
        for (;;) {
            u32 ly = ((y % 4u) == 0u) ? 366u : 365u;
            if (d < ly)
                break;
            d -= ly;
            y++;
            if (y > 2100)
                break;
        }
        r->year = y;
        {
            static const u8 md[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
            u32 m;
            for (m = 0; m < 12; m++) {
                u32 dim = md[m];
                if (m == 1 && (y % 4u) == 0u)
                    dim = 29;
                if (d < dim)
                    break;
                d -= dim;
            }
            r->month = m + 1;
            r->day = d + 1;
        }
    }
}

void ebl_power_read(struct ebl_power *p)
{
    u32 sts, batt, conv;
    memset(p, 0, sizeof(*p));
    sts = readl(IMX_POWER_BASE + HW_POWER_STS);
    batt = readl(IMX_POWER_BASE + HW_POWER_BATTMONITOR);
    p->sts = sts;
    p->batt_raw = (batt >> POWER_BATT_VAL_SHIFT) & POWER_BATT_VAL_MASK;
    p->batt_mv = p->batt_raw * 8u;
    p->dc_ok = (sts & POWER_STS_DC_OK) ? 1 : 0;
    p->vbus = (sts & POWER_STS_VBUSVALID0) ? 1 : 0;
    p->batt_bo = (sts & POWER_STS_BATT_BO) ? 1 : 0;
    lradc_kick_ch7();
    p->lradc_ch7 = readl(IMX_LRADC_BASE + HW_LRADC_CH7) & LRADC_CH_VALUE_MASK;
    conv = readl(IMX_LRADC_BASE + HW_LRADC_CONVERSION);
    (void)conv;
}

void ebl_device_info(struct ebl_devinfo_q *q)
{
    /* Strings taken from EBL.bin UTF-16 table (SPI_GET*). */
    q->platform_name = "Freescale i.MX28 EVK";
    q->bootme_name   = "ED-SH6";
    q->oem_info      = "SHARP   ED-AA2  V030            ED-SH6";
    q->project_name  = "CEBase";
}

int ebl_beep(u32 ms)
{
    u32 pwm = IMX_PWM_BASE;
    printf("BeepSound()++\n");
    /* PWM4 + GPIO3.26 amp (active low), from imx28-brain.dtsi buzzer. */
    gpio_direction_output(GPIO(3, 26), 0);
    writel_clr(PWM_CTRL_SFTRST | PWM_CTRL_CLKGATE, pwm + HW_PWM_CTRL);
    /* channel 4: ACTIVE at +0x50, PERIOD at +0x60 (stride 0x20 from 0). */
    writel((0x0080u << 16) | 0, pwm + 0x050);
    writel((1u << 20) | (2u << 18) | (3u << 16) | 0x01f3u, pwm + 0x060);
    writel_set(1u << 4, pwm + HW_PWM_CTRL);
    mdelay(ms ? ms : 80);
    writel_clr(1u << 4, pwm + HW_PWM_CTRL);
    gpio_direction_output(GPIO(3, 26), 1);
    printf("BeepSound()-- bRet=1\n");
    return 0;
}

void ebl_oem_init(struct ebl_state *st)
{
    struct ebl_devinfo_q q;
    memset(st, 0, sizeof(*st));
    printf("+OEMInit\n");

    st->chipid = board_chipid();
    st->cache_type = read_cp15_cache_type();
    st->cache_size = read_cp15_cache_size();
    printf("    Cache:\n");
    printf("        L1Flags:       0x%x\n", st->cache_type);

    ebl_rtc_read(&st->rtc);
    if (st->rtc.present)
        printf("+InitRTC(...)  seconds=%u  %u/%u/%u %u:%u:%u\n",
               st->rtc.seconds, st->rtc.year, st->rtc.month, st->rtc.day,
               st->rtc.hour, st->rtc.min, st->rtc.sec);
    else
        printf("ERROR: InitRTC: pv_HWregRTC null pointer!\n");

    ebl_power_read(&st->pwr);
    st->i2c_ok = (i2c_init() == 0);
    st->i2c0_ctrl = readl(IMX_I2C0_BASE + HW_I2C_CTRL0);
    st->codec_ok = (codec_probe(&st->codec_id) == 0);
    if (!st->codec_ok)
        printf("CODEC_READ ERROR addr=0000, data=%x\n", st->codec_id);

    st->edna2_doorbell = ident_edna2_doorbell();
    st->edna2_status0 = readl(EDNA2_MAILBOX_PHYS + EDNA2_STATUS_OFF);

    ebl_device_info(&q);
    printf("SPI_GETPLATFORMNAME %s\n", q.platform_name);
    printf("SPI_GETBOOTMENAME %s\n", q.bootme_name);
    printf("SPI_GETOEMINFO %s\n", q.oem_info);

    printf("-OEMInit\n");
}

void ebl_print_state(const struct ebl_state *st)
{
    printf("ebl: chipid=0x%x rtc=%u present=%d batt=%umV dc_ok=%d vbus=%d\n",
           st->chipid, st->rtc.seconds, st->rtc.present,
           st->pwr.batt_mv, st->pwr.dc_ok, st->pwr.vbus);
    printf("ebl: i2c_ok=%d codec_id=0x%x codec_ok=%d edna2_db=0x%x st0=0x%x\n",
           st->i2c_ok, st->codec_id, st->codec_ok,
           st->edna2_doorbell, st->edna2_status0);
}

#else
void ebl_rtc_read(struct ebl_rtc *r) { memset(r, 0, sizeof(*r)); }
void ebl_power_read(struct ebl_power *p) { memset(p, 0, sizeof(*p)); }
void ebl_device_info(struct ebl_devinfo_q *q)
{
    q->platform_name = "HOST";
    q->bootme_name = "HOST";
    q->oem_info = "HOST";
    q->project_name = "HOST";
}
int ebl_beep(u32 ms) { (void)ms; return -1; }
void ebl_oem_init(struct ebl_state *st) { memset(st, 0, sizeof(*st)); }
void ebl_print_state(const struct ebl_state *st) { (void)st; }
#endif
