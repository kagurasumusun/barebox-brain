/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "xport.h"

static u16 rd16be(const u8 *p)
{
    return (u16)((p[0] << 8) | p[1]);
}

static void wr16be(u8 *p, u16 v)
{
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

static u32 rd32be(const u8 *p)
{
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

static void wr32be(u8 *p, u32 v)
{
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

u16 ip_checksum(const void *data, u32 len)
{
    const u8 *p = data;
    u32 s = 0, i;
    for (i = 0; i + 1 < len; i += 2)
        s += ((u32)p[i] << 8) | p[i + 1];
    if (i < len)
        s += (u32)p[i] << 8;
    while (s >> 16)
        s = (s & 0xffffu) + (s >> 16);
    return (u16)~s;
}

static int dhcp_put_opt(u8 *p, u32 *n, u32 max, u8 tag, const void *val, u8 vlen)
{
    if (*n + 2u + vlen > max)
        return -1;
    p[(*n)++] = tag;
    p[(*n)++] = vlen;
    if (vlen && val) {
        memcpy(p + *n, val, vlen);
        *n += vlen;
    }
    return 0;
}

static int dhcp_bootp(u8 *out, u32 outsz, u8 op, const u8 mac[6], u32 xid)
{
    if (outsz < 244)
        return -1;
    memset(out, 0, 244);
    out[0] = op;          /* BOOTREQUEST */
    out[1] = 1;           /* ethernet */
    out[2] = 6;
    wr32be(out + 4, xid);
    out[10] = 0x80;       /* broadcast */
    memcpy(out + 28, mac, 6);
    wr32be(out + 236, DHCP_MAGIC);
    return 240;
}

int dhcp_build_discover(u8 *out, u32 outsz, const u8 mac[6], u32 xid)
{
    u32 n;
    u8 t = DHCP_DISCOVER;
    u8 prm[] = { 1, 3, 6, 51, 54 };
    int h = dhcp_bootp(out, outsz, 1, mac, xid);
    if (h < 0)
        return h;
    n = (u32)h;
    if (dhcp_put_opt(out, &n, outsz, 53, &t, 1))
        return -2;
    if (dhcp_put_opt(out, &n, outsz, 55, prm, sizeof(prm)))
        return -2;
    if (n >= outsz)
        return -2;
    out[n++] = 255;
    return (int)n;
}

int dhcp_build_request(u8 *out, u32 outsz, const u8 mac[6], u32 xid,
                       u32 yiaddr, u32 server_id)
{
    u32 n;
    u8 t = DHCP_REQUEST;
    u8 ya[4], sid[4];
    int h = dhcp_bootp(out, outsz, 1, mac, xid);
    if (h < 0)
        return h;
    n = (u32)h;
    wr32be(ya, yiaddr);
    wr32be(sid, server_id);
    if (dhcp_put_opt(out, &n, outsz, 53, &t, 1))
        return -2;
    if (dhcp_put_opt(out, &n, outsz, 50, ya, 4))
        return -2;
    if (dhcp_put_opt(out, &n, outsz, 54, sid, 4))
        return -2;
    if (n >= outsz)
        return -2;
    out[n++] = 255;
    return (int)n;
}

int dhcp_parse(const u8 *pkt, u32 len, struct dhcp_lease *out)
{
    u32 i;
    memset(out, 0, sizeof(*out));
    if (len < 241 || pkt[0] != 2 || rd32be(pkt + 236) != DHCP_MAGIC)
        return -1;
    out->yiaddr = rd32be(pkt + 16);
    out->siaddr = rd32be(pkt + 20);
    i = 240;
    while (i < len) {
        u8 tag = pkt[i++];
        u8 ln;
        if (tag == 0)
            continue;
        if (tag == 255)
            break;
        if (i >= len)
            return -2;
        ln = pkt[i++];
        if (i + ln > len)
            return -2;
        if (tag == 53 && ln >= 1)
            out->type = pkt[i];
        else if (tag == 1 && ln >= 4)
            out->netmask = rd32be(pkt + i);
        else if (tag == 3 && ln >= 4)
            out->router = rd32be(pkt + i);
        else if (tag == 54 && ln >= 4)
            out->server_id = rd32be(pkt + i);
        else if (tag == 51 && ln >= 4)
            out->lease_sec = rd32be(pkt + i);
        i += ln;
    }
    if (!out->type)
        return -3;
    return 0;
}

int tftp_build_rrq(u8 *out, u32 outsz, const char *name)
{
    u32 n = 2, i;
    if (!name || !name[0])
        return -1;
    wr16be(out, TFTP_RRQ);
    for (i = 0; name[i]; i++) {
        if (n + 8 >= outsz)
            return -2;
        out[n++] = (u8)name[i];
    }
    out[n++] = 0;
    out[n++] = 'o';
    out[n++] = 'c';
    out[n++] = 't';
    out[n++] = 'e';
    out[n++] = 't';
    out[n++] = 0;
    return (int)n;
}

int tftp_build_ack(u8 *out, u32 outsz, u16 block)
{
    if (outsz < 4)
        return -1;
    wr16be(out, TFTP_ACK);
    wr16be(out + 2, block);
    return 4;
}

int tftp_parse_data(const u8 *pkt, u32 len, u16 *block,
                    const u8 **payload, u32 *plen)
{
    u16 op;
    if (len < 4)
        return -1;
    op = rd16be(pkt);
    if (op == TFTP_ERROR)
        return -2;
    if (op != TFTP_DATA)
        return -3;
    *block = rd16be(pkt + 2);
    *payload = pkt + 4;
    *plen = len - 4;
    return 0;
}

/* CE EDBG BOOTME (ethdbg.h family): id=0x11, then name, MAC. */
int edbg_build_bootme(u8 *out, u32 outsz, const char *name, const u8 mac[6])
{
    u32 i = 0, n;
    if (outsz < 48)
        return -1;
    memset(out, 0, 48);
    out[0] = 0x11; /* BOOTME */
    out[1] = 0x01; /* service download */
    if (name) {
        while (name[i] && i < 16) {
            out[4 + i] = (u8)name[i];
            i++;
        }
    }
    memcpy(out + 24, mac, 6);
    wr16be(out + 30, EDBG_PORT);
    n = 48;
    return (int)n;
}
