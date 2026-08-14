/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EBL_H
#define EBL_H

#include "brainboot.h"

/*
 * Hardware services that EBL.bin (CE OAL, VA 0x81E80000) performs in
 * OEMInit / RTC / I2C / codec / power. This is not a rehost of the
 * CE kernel IOCTL table; it is the same silicon bring-up, run here
 * before we jump to NK.
 */

struct ebl_rtc {
    u32 year, month, day, hour, min, sec, msec;
    u32 seconds;            /* HW_RTC_SECONDS */
    u32 rtc_stat;
    int present;
    int alarm_present;
};

struct ebl_power {
    u32 sts;
    u32 batt_raw;           /* POWER_BATTMONITOR[25:16] */
    u32 batt_mv;            /* 8 mV units * 8 */
    u32 lradc_ch7;
    int dc_ok;
    int vbus;
    int batt_bo;
};

struct ebl_devinfo_q {
    const char *platform_name;  /* SPI_GETPLATFORMNAME */
    const char *bootme_name;    /* SPI_GETBOOTMENAME */
    const char *oem_info;       /* SPI_GETOEMINFO */
    const char *project_name;   /* SPI_GETPROJECTNAME */
};

struct ebl_state {
    struct ebl_rtc rtc;
    struct ebl_power pwr;
    u16  codec_id;
    int  codec_ok;
    int  i2c_ok;
    u32  i2c0_ctrl;
    u32  edna2_doorbell;
    u32  edna2_status0;
    u32  chipid;
    u32  cache_type;
    u32  cache_size;
};

void ebl_oem_init(struct ebl_state *st);
void ebl_rtc_read(struct ebl_rtc *r);
void ebl_power_read(struct ebl_power *p);
void ebl_device_info(struct ebl_devinfo_q *q);
int  ebl_beep(u32 ms);
void ebl_print_state(const struct ebl_state *st);

#endif
