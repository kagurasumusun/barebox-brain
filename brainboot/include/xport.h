/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef XPORT_H
#define XPORT_H

#include "brainboot.h"

/*
 * Download transports from Freescale EBOOT BLMenu (CE BL 1.4).
 * Protocols are implemented and host-tested. Silicon is probed;
 * this board has no ENET PHY, USB0 is host, GPMI pins are LCD.
 */

#define ETH_HLEN        14
#define IP_HLEN         20
#define UDP_HLEN        8
#define DHCP_SPORT      68
#define DHCP_DPORT      67
#define TFTP_PORT       69
#define EDBG_PORT       980

#define DHCP_DISCOVER   1
#define DHCP_OFFER      2
#define DHCP_REQUEST    3
#define DHCP_ACK        5
#define DHCP_MAGIC      0x63825363u

#define TFTP_RRQ        1
#define TFTP_DATA       3
#define TFTP_ACK        4
#define TFTP_ERROR      5
#define TFTP_OACK       6

#define RNDIS_INIT      0x00000002u
#define RNDIS_INIT_C    0x80000002u
#define RNDIS_HALT      0x00000003u
#define RNDIS_QUERY     0x00000004u
#define RNDIS_QUERY_C   0x80000004u
#define RNDIS_SET       0x00000005u
#define RNDIS_SET_C     0x80000005u
#define RNDIS_RESET     0x00000006u
#define RNDIS_RESET_C   0x80000006u
#define RNDIS_KEEPALIVE 0x00000008u
#define RNDIS_KEEPALIVE_C 0x80000008u
#define RNDIS_PACKET    0x00000001u

#define FCB_MAGIC       0x20424346u  /* 'FCB ' */

struct dhcp_lease {
    u32 yiaddr;
    u32 siaddr;
    u32 netmask;
    u32 router;
    u32 server_id;
    u32 lease_sec;
    u8  type;
};

struct tftp_ack {
    u16 block;
};

struct rndis_init_cmplt {
    u32 type;
    u32 len;
    u32 req_id;
    u32 status;
    u32 major, minor;
    u32 dev_flags;
    u32 medium;
    u32 max_pkt;
    u32 max_xfer;
    u32 pkt_align;
    u32 af_list;
    u32 af_size;
};

struct fcb_info {
    u32 magic;
    u32 version;
    u32 page_data_size;
    u32 total_page_size;
    u32 sectors_per_block;
    u32 pages_per_block;
    int valid;
};

struct xport_status {
    int enet_clk;
    int fec_present;
    int phy_addr;       /* -1 none */
    int usb0_host;
    int usb0_ccs;
    int usb_id;
    u32 usb_id_reg;
    int gpmi_gated;
    int gpmi_pins_are_gpio;
    u32 gpmi_version;
};

u16 ip_checksum(const void *data, u32 len);
int  dhcp_build_discover(u8 *out, u32 outsz, const u8 mac[6], u32 xid);
int  dhcp_build_request(u8 *out, u32 outsz, const u8 mac[6], u32 xid,
                        u32 yiaddr, u32 server_id);
int  dhcp_parse(const u8 *pkt, u32 len, struct dhcp_lease *out);
int  tftp_build_rrq(u8 *out, u32 outsz, const char *name);
int  tftp_build_ack(u8 *out, u32 outsz, u16 block);
int  tftp_parse_data(const u8 *pkt, u32 len, u16 *block,
                     const u8 **payload, u32 *plen);
int  rndis_build_init(u8 *out, u32 outsz, u32 req_id);
int  rndis_parse_init_cmplt(const u8 *p, u32 len, struct rndis_init_cmplt *o);
int  rndis_unwrap_packet(const u8 *p, u32 len, const u8 **eth, u32 *ethlen);
int  edbg_build_bootme(u8 *out, u32 outsz, const char *name, const u8 mac[6]);
int  fcb_parse(const u8 *page, u32 len, struct fcb_info *out);

int  xport_probe(struct xport_status *st);
int  xport_dhcp(struct dhcp_lease *lease);
int  xport_tftp(const char *name, u8 *dst, u32 dstsz, u32 *got);
int  xport_usb_serial(void);
int  xport_usb_rndis(void);
int  xport_nand_ipl(u8 *dst, u32 dstsz, u32 *got);
void xport_print(const struct xport_status *st);

#endif
