/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "xport.h"
#include "board.h"

int enet_probe(struct xport_status *st);
int usb_xport_probe(struct xport_status *st);
int nand_ipl_probe(struct xport_status *st);

int xport_probe(struct xport_status *st)
{
    memset(st, 0, sizeof(*st));
    st->phy_addr = -1;
    enet_probe(st);
    usb_xport_probe(st);
    nand_ipl_probe(st);
    return 0;
}

void xport_print(const struct xport_status *st)
{
    printf("ENET clk=%d fec=%d phy=%d\n",
           st->enet_clk, st->fec_present, st->phy_addr);
    printf("USB0 idreg=0x%x host=%d ccs=%d otg_id=%d\n",
           st->usb_id_reg, st->usb0_host, st->usb0_ccs, st->usb_id);
    printf("GPMI gated=%d pins_gpio=%d ver=0x%x\n",
           st->gpmi_gated, st->gpmi_pins_are_gpio, st->gpmi_version);
}

int xport_dhcp(struct dhcp_lease *lease)
{
    struct xport_status st;
    u8 disc[300];
    int n;
    u8 mac[6] = { 0x00, 0x04, 0x9f, 0x00, 0x00, 0x01 };

    memset(lease, 0, sizeof(*lease));
    xport_probe(&st);
    n = dhcp_build_discover(disc, sizeof(disc), mac, 0x1234abcd);
    printf("DHCP DISCOVER %d bytes\n", n);
    if (st.phy_addr < 0) {
        printf("DHCP: no MDIO PHY (ENET pads are GPIO on this board)\n");
        return -2;
    }
    printf("DHCP: PHY %d present — TX not completed (no link state)\n",
           st.phy_addr);
    return -3;
}

int xport_tftp(const char *name, u8 *dst, u32 dstsz, u32 *got)
{
    struct xport_status st;
    u8 rrq[128];
    int n;

    (void)dst;
    (void)dstsz;
    if (got)
        *got = 0;
    xport_probe(&st);
    n = tftp_build_rrq(rrq, sizeof(rrq), name ? name : "nk.bin");
    printf("TFTP RRQ %s  %d bytes\n", name ? name : "nk.bin", n);
    if (st.phy_addr < 0) {
        printf("TFTP: no Ethernet PHY — cannot send RRQ\n");
        return -2;
    }
    return -3;
}

int xport_usb_serial(void)
{
    struct xport_status st;
    xport_probe(&st);
    printf("USB Serial gadget (CDC-ACM) — EBOOT download transport\n");
    printf("USB0 ID=0x%x host=%d CCS=%d\n",
           st.usb_id_reg, st.usb0_host, st.usb0_ccs);
    if (st.usb0_ccs == 0)
        printf("USB: no device attached (host port, qemu has no data path)\n");
    printf("This board DTS sets usb0 dr_mode=host. A PC cannot see a gadget.\n");
    return -2;
}

int xport_usb_rndis(void)
{
    struct xport_status st;
    u8 init[24];
    int n;
    xport_probe(&st);
    n = rndis_build_init(init, sizeof(init), 1);
    printf("RNDIS INIT %d bytes\n", n);
    printf("USB0 host=%d CCS=%d — RNDIS gadget needs device/OTG mode\n",
           st.usb0_host, st.usb0_ccs);
    return -2;
}

int xport_nand_ipl(u8 *dst, u32 dstsz, u32 *got)
{
    struct xport_status st;
    (void)dst;
    (void)dstsz;
    if (got)
        *got = 0;
    xport_probe(&st);
    printf("NAND IPL: GPMI ver=0x%x gated=%d\n",
           st.gpmi_version, st.gpmi_gated);
    printf("GPMI_ALE/CLE are LCD GPIO on PW-SH6 (imx28-brain.dtsi hog).\n");
    printf("No NAND media. IPL on this unit is eMMC type 0x53 SB.\n");
    return -2;
}
