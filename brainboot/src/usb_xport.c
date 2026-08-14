/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "xport.h"
#include "board.h"

#ifndef HOST_BUILD

int usb_xport_probe(struct xport_status *st)
{
    u32 usb = IMX_USB0_BASE;
    u32 cmd, g, mode, port, otg;

    st->usb0_host = 0;
    st->usb0_ccs = 0;
    st->usb_id = 0;
    st->usb_id_reg = readl(usb + USB_ID);

    cmd = readl(usb + USB_USBCMD);
    writel(cmd | USBCMD_RST, usb + USB_USBCMD);
    g = 10000;
    while ((readl(usb + USB_USBCMD) & USBCMD_RST) && --g)
        udelay(10);
    if (readl(usb + USB_USBCMD) & USBCMD_RST)
        return -1;

    mode = readl(usb + USB_USBMODE) & USBMODE_CM_MASK;
    port = readl(usb + USB_PORTSC1);
    otg = readl(usb + USB_OTGSC);
    st->usb0_host = (mode == USBMODE_CM_HOST);
    st->usb0_ccs = (port & PORTSC_CCS) ? 1 : 0;
    st->usb_id = (otg & OTGSC_ID) ? 1 : 0;
    return 0;
}

#else
int usb_xport_probe(struct xport_status *st)
{
    st->usb0_host = 1;
    st->usb0_ccs = 0;
    st->usb_id = 1;
    st->usb_id_reg = 0;
    return 0;
}
#endif
