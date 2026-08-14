/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IMX28_H
#define IMX28_H

#include "brainboot.h"

/* Physical memory (i.MX28 RM + OEMAddressTable measured on this board) */
#define DRAM_PHYS_BASE          0x40000000u
#define DRAM_PHYS_SIZE          0x08000000u   /* 128 MiB */
#define OCRAM_PHYS_BASE         0x00000000u
#define OCRAM_PHYS_SIZE         0x00020000u   /* 128 KiB */

/* SoC register bases — i.MX28 RM / barebox include/mach/mxs/imx28-regs.h */
#define IMX_APBH_BASE           0x80004000u
#define IMX_BCH_BASE            0x8000a000u
#define IMX_GPMI_BASE           0x8000c000u
#define IMX_SSP0_BASE           0x80010000u
#define IMX_SSP1_BASE           0x80012000u
#define IMX_SSP2_BASE           0x80014000u
#define IMX_SSP3_BASE           0x80016000u
#define IMX_PINCTRL_BASE        0x80018000u
#define IMX_DIGCTL_BASE         0x8001c000u
#define IMX_EMI_BASE            0x80020000u
#define IMX_OCOTP_BASE          0x8002c000u
#define IMX_LCDIF_BASE          0x80030000u
#define IMX_CLKCTRL_BASE        0x80040000u
#define IMX_POWER_BASE          0x80044000u
#define IMX_LRADC_BASE          0x80050000u
#define IMX_RTC_BASE            0x80056000u
#define IMX_I2C0_BASE           0x80058000u
#define IMX_I2C1_BASE           0x8005a000u
#define IMX_PWM_BASE            0x80064000u
#define IMX_TIMROT_BASE         0x80068000u
#define IMX_UARTAPP0_BASE       0x8006a000u
#define IMX_DBGUART_BASE        0x80074000u
#define IMX_USBPHY0_BASE        0x8007c000u
#define IMX_USB0_BASE           0x80080000u
#define IMX_WATCHDOG_BASE       IMX_RTC_BASE

/* CLKCTRL (i.MX28) */
#define HW_CLKCTRL_PLL0CTRL0    0x000
#define HW_CLKCTRL_CPU          0x050
#define HW_CLKCTRL_HBUS         0x060
#define HW_CLKCTRL_XBUS         0x070
#define HW_CLKCTRL_XTAL         0x080
#define HW_CLKCTRL_SSP0         0x090
#define HW_CLKCTRL_SSP1         0x0a0
#define HW_CLKCTRL_SSP2         0x0b0
#define HW_CLKCTRL_SSP3         0x0c0
#define HW_CLKCTRL_GPMI         0x0d0
#define HW_CLKCTRL_SPDIF        0x0e0
#define HW_CLKCTRL_EMI          0x0f0
#define HW_CLKCTRL_SAIF0        0x100
#define HW_CLKCTRL_SAIF1        0x110
#define HW_CLKCTRL_LCD          0x120
#define HW_CLKCTRL_ETM          0x130
#define HW_CLKCTRL_ENET         0x140
#define HW_CLKCTRL_HSADC        0x150
#define HW_CLKCTRL_FLEXCAN      0x160
#define HW_CLKCTRL_FRAC0        0x1b0
#define HW_CLKCTRL_FRAC1        0x1c0
#define HW_CLKCTRL_CLKSEQ       0x1d0
#define HW_CLKCTRL_RESET        0x1e0

#define CLKCTRL_CLKSEQ_BYPASS_SSP0   (1u << 3)
#define CLKCTRL_CLKSEQ_BYPASS_SSP1   (1u << 4)
#define CLKCTRL_XTAL_PWM_CLK24M_GATE (1u << 29)
#define CLKCTRL_SSP_CLKGATE          (1u << 31)
#define CLKCTRL_SSP_BUSY             (1u << 29)

/* PINCTRL */
#define HW_PINCTRL_CTRL         0x000
#define HW_PINCTRL_MUXSEL0      0x100
#define HW_PINCTRL_DRIVE0       0x300
#define HW_PINCTRL_PULL0        0x600
#define HW_PINCTRL_DOUT0        0x700
#define HW_PINCTRL_DIN0         0x900
#define HW_PINCTRL_DOE0         0xb00
#define HW_PINCTRL_PIN2IRQ0     0x1000
#define HW_PINCTRL_IRQEN0       0x1100

/* GPIO bank helpers: bank 0..4, pin 0..31 */
#define GPIO_BANK(gpio)         ((gpio) >> 5)
#define GPIO_PIN(gpio)          ((gpio) & 31)
#define GPIO(bank, pin)         (((bank) << 5) | (pin))

/* SSP (i.MX28 layout) */
#define HW_SSP_CTRL0            0x000
#define HW_SSP_CMD0             0x010
#define HW_SSP_CMD1             0x020
#define HW_SSP_XFER_COUNT       0x030
#define HW_SSP_BLOCK_SIZE       0x040
#define HW_SSP_TIMING           0x070
#define HW_SSP_CTRL1            0x080
#define HW_SSP_DATA             0x090
#define HW_SSP_SDRESP0          0x0a0
#define HW_SSP_SDRESP1          0x0b0
#define HW_SSP_SDRESP2          0x0c0
#define HW_SSP_SDRESP3          0x0d0
#define HW_SSP_STATUS           0x100

#define SSP_CTRL0_SFTRST        (1u << 31)
#define SSP_CTRL0_CLKGATE       (1u << 30)
#define SSP_CTRL0_RUN           (1u << 29)
#define SSP_CTRL0_SDIO_IRQ_CHECK (1u << 28)
#define SSP_CTRL0_LOCK_CS       (1u << 27)
#define SSP_CTRL0_IGNORE_CRC    (1u << 26)
#define SSP_CTRL0_READ          (1u << 25)
#define SSP_CTRL0_DATA_XFER     (1u << 24)
#define SSP_CTRL0_BUS_WIDTH(x)  (((x) & 3u) << 22)
#define SSP_CTRL0_WAIT_FOR_IRQ  (1u << 21)
#define SSP_CTRL0_WAIT_FOR_CMD  (1u << 20)
#define SSP_CTRL0_LONG_RESP     (1u << 19)
#define SSP_CTRL0_CHECK_RESP    (1u << 18)
#define SSP_CTRL0_GET_RESP      (1u << 17)
#define SSP_CTRL0_ENABLE        (1u << 16)

#define SSP_CMD0_APPEND_8CYC    (1u << 20)
#define SSP_CMD0_CMD(x)         ((x) & 0xffu)

#define SSP_CTRL1_POLARITY      (1u << 9)
#define SSP_CTRL1_WORD_LENGTH(x) (((x) & 0xfu) << 4)
#define SSP_CTRL1_SSP_MODE(x)   ((x) & 0xfu)
#define SSP_MODE_SD_MMC         0x3

#define SSP_STATUS_PRESENT      (1u << 31)
#define SSP_STATUS_CARD_DETECT  (1u << 28)
#define SSP_STATUS_RESP_CRC_ERR (1u << 16)
#define SSP_STATUS_RESP_ERR     (1u << 15)
#define SSP_STATUS_RESP_TIMEOUT (1u << 14)
#define SSP_STATUS_DATA_CRC_ERR (1u << 13)
#define SSP_STATUS_TIMEOUT      (1u << 12)
#define SSP_STATUS_FIFO_EMPTY   (1u << 5)
#define SSP_STATUS_CMD_BUSY     (1u << 3)
#define SSP_STATUS_DATA_BUSY    (1u << 2)
#define SSP_STATUS_BUSY         (1u << 0)

#define SSP_TIMING_TIMEOUT(x)   ((u32)(x) << 16)
#define SSP_TIMING_CLOCK_DIVIDE(x) (((x) & 0xffu) << 8)
#define SSP_TIMING_CLOCK_RATE(x)   ((x) & 0xffu)

/* LCDIF */
#define HW_LCDIF_CTRL           0x000
#define HW_LCDIF_CTRL1          0x010
#define HW_LCDIF_TRANSFER_COUNT 0x020
#define HW_LCDIF_CUR_BUF        0x030
#define HW_LCDIF_NEXT_BUF       0x040
#define HW_LCDIF_TIMING         0x060
#define HW_LCDIF_VDCTRL0        0x070
#define HW_LCDIF_VDCTRL1        0x080
#define HW_LCDIF_VDCTRL2        0x090
#define HW_LCDIF_VDCTRL3        0x0a0
#define HW_LCDIF_VDCTRL4        0x0b0
#define HW_LCDIF_DATA           0x1b0
#define HW_LCDIF_STAT           0x1d0

#define LCDIF_CTRL_SFTRST           (1u << 31)
#define LCDIF_CTRL_CLKGATE          (1u << 30)
#define LCDIF_CTRL_YCBCR422_INPUT   (1u << 29)
#define LCDIF_CTRL_READ_WRITEB      (1u << 28)
#define LCDIF_CTRL_WAIT_FOR_VSYNC_EDGE (1u << 27)
#define LCDIF_CTRL_DATA_SHIFT_DIR   (1u << 26)
#define LCDIF_CTRL_SHIFT_NUM_BITS(x) (((x) & 0x1fu) << 21)
#define LCDIF_CTRL_DVI_MODE         (1u << 20)
#define LCDIF_CTRL_BYPASS_COUNT     (1u << 19)
#define LCDIF_CTRL_VSYNC_MODE       (1u << 18)
#define LCDIF_CTRL_DOTCLK_MODE      (1u << 17)
#define LCDIF_CTRL_DATA_SELECT      (1u << 16)
#define LCDIF_CTRL_INPUT_SWIZZLE(x) (((x) & 3u) << 14)
#define LCDIF_CTRL_CSC_DATA_SWIZZLE(x) (((x) & 3u) << 12)
#define LCDIF_CTRL_LCD_DATABUS_WIDTH(x) (((x) & 3u) << 10)
#define LCDIF_CTRL_WORD_LENGTH(x)   (((x) & 3u) << 8)
#define LCDIF_CTRL_RGB_TO_YCBCR422  (1u << 7)
#define LCDIF_CTRL_ENABLE_PXP_HANDSHAKE (1u << 6)
#define LCDIF_CTRL_MASTER           (1u << 5)
#define LCDIF_CTRL_DATA_FORMAT_16_BIT (1u << 3)
#define LCDIF_CTRL_DATA_FORMAT_18_BIT (1u << 2)
#define LCDIF_CTRL_DATA_FORMAT_24_BIT (1u << 1)
#define LCDIF_CTRL_RUN              (1u << 0)

#define LCDIF_CTRL1_BYTE_PACKING_FORMAT(x) (((x) & 0xfu) << 16)
#define LCDIF_CTRL1_RESET           (1u << 0)
#define LCDIF_TRANSFER_COUNT(h, v)  (((v) << 16) | ((h) & 0xffffu))

#define LCDIF_TIMING_CMD_HOLD(x)    (((x) & 0xffu) << 24)
#define LCDIF_TIMING_CMD_SETUP(x)   (((x) & 0xffu) << 16)
#define LCDIF_TIMING_DATA_HOLD(x)   (((x) & 0xffu) << 8)
#define LCDIF_TIMING_DATA_SETUP(x)  ((x) & 0xffu)

#define LCDIF_VDCTRL3_VSYNC_ONLY    (1u << 30)

/* PWM */
#define HW_PWM_CTRL             0x000
#define HW_PWM_ACTIVE0          0x010
#define HW_PWM_PERIOD0          0x020
#define HW_PWM_ACTIVE1          0x030
#define HW_PWM_PERIOD1          0x040
#define PWM_CTRL_SFTRST         (1u << 31)
#define PWM_CTRL_CLKGATE        (1u << 30)
#define PWM_CTRL_PWM0_ENABLE    (1u << 0)
#define PWM_CTRL_PWM1_ENABLE    (1u << 1)

/* RTC / watchdog persistent */
#define HW_RTC_CTRL             0x000
#define HW_RTC_STAT             0x010
#define HW_RTC_MILLISECONDS     0x020
#define HW_RTC_SECONDS          0x030
#define HW_RTC_WATCHDOG         0x050
#define HW_RTC_PERSISTENT0      0x060
#define HW_RTC_PERSISTENT1      0x070
#define RTC_CTRL_WATCHDOGEN     (1u << 4)
#define RTC_PERSISTENT0_CLOCKSOURCE     (1u << 2)
#define RTC_PERSISTENT0_AUTO_RESTART    (1u << 17)
#define RTC_PERSISTENT0_SPARE_BIT31     (1u << 31)

/* DIGCTL microseconds counter */
#define HW_DIGCTL_MICROSECONDS  0x0c0
#define HW_DIGCTL_CHIPID        0x310

/* POWER */
#define HW_POWER_CTRL           0x000
#define HW_POWER_5VCTRL         0x010
#define HW_POWER_MINPWR         0x020
#define HW_POWER_CHARGE         0x030
#define HW_POWER_VDDDCTRL       0x040
#define HW_POWER_VDDACTRL       0x050
#define HW_POWER_VDDIOCTRL      0x060
#define HW_POWER_VDDMEMCTRL     0x070
#define HW_POWER_DCDC4P2        0x080
#define HW_POWER_MISC           0x090
#define HW_POWER_DCLIMITS       0x0a0
#define HW_POWER_LOOPCTRL       0x0b0
#define HW_POWER_STS            0x0c0
#define HW_POWER_SPEED          0x110
#define HW_POWER_BATTMONITOR    0x0e0
#define HW_POWER_RESET          0x100
#define POWER_RESET_UNLOCK      0x3e770000u
#define POWER_RESET_PWD         (1u << 0)

/* OCOTP */
#define HW_OCOTP_CTRL           0x000
#define HW_OCOTP_DATA           0x010
#define HW_OCOTP_CUST0          0x020
#define HW_OCOTP_LOCK           0x120
#define OCOTP_CTRL_BUSY         (1u << 8)
#define OCOTP_CTRL_ERROR        (1u << 9)
#define OCOTP_CTRL_RD_BANK_OPEN (1u << 12)

#define SSP_STATUS_FIFO_FULL    (1u << 8)

/* PL011-compatible debug UART */
#define UART_DR                 0x00
#define UART_RSR                0x04
#define UART_FR                 0x18
#define UART_ILPR               0x20
#define UART_IBRD               0x24
#define UART_FBRD               0x28
#define UART_LCRH               0x2c
#define UART_CR                 0x30
#define UART_IFLS               0x34
#define UART_IMSC               0x38
#define UART_ICR                0x44

#define UART_FR_TXFF            (1u << 5)
#define UART_FR_RXFE            (1u << 4)
#define UART_FR_BUSY            (1u << 3)
#define UART_LCRH_WLEN_8        (3u << 5)
#define UART_LCRH_FEN           (1u << 4)
#define UART_CR_UARTEN          (1u << 0)
#define UART_CR_TXE             (1u << 8)
#define UART_CR_RXE             (1u << 9)

#define XTAL_FREQ_HZ            24000000u

/* i.MX28 I2C (RM ch.27, PIO used by EBL ProcessPackets) */
#define HW_I2C_CTRL0            0x000
#define HW_I2C_TIMING0          0x010
#define HW_I2C_TIMING1          0x020
#define HW_I2C_TIMING2          0x030
#define HW_I2C_CTRL1            0x040
#define HW_I2C_STAT             0x050
#define HW_I2C_QUEUECTRL        0x060
#define HW_I2C_QUEUESTAT        0x070
#define HW_I2C_QUEUECMD         0x080
#define HW_I2C_QUEUEDATA        0x090
#define HW_I2C_DATA             0x0a0
#define HW_I2C_DEBUG0           0x0b0
#define HW_I2C_DEBUG1           0x0c0
#define HW_I2C_VERSION          0x0d0

#define I2C_CTRL0_SFTRST        (1u << 31)
#define I2C_CTRL0_CLKGATE       (1u << 30)
#define I2C_CTRL0_RUN           (1u << 29)
#define I2C_CTRL0_PIO_MODE      (1u << 24)
#define I2C_CTRL0_RETAIN_CLOCK  (1u << 22)
#define I2C_CTRL0_POST_SEND_STOP (1u << 21)
#define I2C_CTRL0_PRE_SEND_START (1u << 20)
#define I2C_CTRL0_MASTER_MODE   (1u << 19)
#define I2C_CTRL0_DIRECTION     (1u << 18)
#define I2C_CTRL1_CLR_GOT_A_NAK (1u << 28)
#define I2C_CTRL1_DATA_ENGINE_CMPLT (1u << 6)
#define I2C_CTRL1_NO_SLAVE_ACK  (1u << 5)
#define I2C_STAT_BUS_BUSY       (1u << 11)
#define I2C_STAT_GOT_A_NAK      (1u << 28)

/* LRADC */
#define HW_LRADC_CTRL0          0x000
#define HW_LRADC_CTRL1          0x010
#define HW_LRADC_CTRL2          0x020
#define HW_LRADC_CTRL3          0x030
#define HW_LRADC_STATUS         0x040
#define HW_LRADC_CH0            0x050
#define HW_LRADC_CH7            0x0c0
#define HW_LRADC_CONVERSION     0x140
#define LRADC_CTRL0_SFTRST      (1u << 31)
#define LRADC_CTRL0_CLKGATE     (1u << 30)
#define LRADC_CTRL0_SCHEDULE(ch) (1u << (ch))
#define LRADC_CTRL1_IRQ(ch)     (1u << (ch))
#define LRADC_CH_VALUE_MASK     0x3ffffu
#define LRADC_CONV_AUTOMATIC    (1u << 20)
#define LRADC_CONV_BATT_MASK    0x3ffu

/* RTC extras */
#define HW_RTC_ALARM            0x040
#define HW_RTC_PERSISTENT2      0x080
#define RTC_STAT_RTC_PRESENT    (1u << 31)
#define RTC_STAT_ALARM_PRESENT  (1u << 30)

/* POWER battery */
#define POWER_BATT_VAL_SHIFT    16
#define POWER_BATT_VAL_MASK     0x3ffu
#define POWER_STS_DC_OK         (1u << 9)
#define POWER_STS_VBUSVALID0    (1u << 1)
#define POWER_STS_BATT_BO       (1u << 13)
#define POWER_STS_CHRGSTS       (1u << 11)

#endif
