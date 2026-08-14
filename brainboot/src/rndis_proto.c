/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "xport.h"

static void w32le(u8 *p, u32 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

static u32 r32le(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

int rndis_build_init(u8 *out, u32 outsz, u32 req_id)
{
    if (outsz < 24)
        return -1;
    memset(out, 0, 24);
    w32le(out + 0, RNDIS_INIT);
    w32le(out + 4, 24);
    w32le(out + 8, req_id);
    w32le(out + 12, 1);  /* major */
    w32le(out + 16, 0);  /* minor */
    w32le(out + 20, 0x4000);
    return 24;
}

int rndis_parse_init_cmplt(const u8 *p, u32 len, struct rndis_init_cmplt *o)
{
    memset(o, 0, sizeof(*o));
    if (len < 52)
        return -1;
    o->type = r32le(p + 0);
    o->len = r32le(p + 4);
    o->req_id = r32le(p + 8);
    o->status = r32le(p + 12);
    o->major = r32le(p + 16);
    o->minor = r32le(p + 20);
    o->dev_flags = r32le(p + 24);
    o->medium = r32le(p + 28);
    o->max_pkt = r32le(p + 32);
    o->max_xfer = r32le(p + 36);
    o->pkt_align = r32le(p + 40);
    o->af_list = r32le(p + 44);
    o->af_size = r32le(p + 48);
    if (o->type != RNDIS_INIT_C)
        return -2;
    return 0;
}

int rndis_unwrap_packet(const u8 *p, u32 len, const u8 **eth, u32 *ethlen)
{
    u32 type, data_off, data_len;
    if (len < 44)
        return -1;
    type = r32le(p);
    if (type != RNDIS_PACKET)
        return -2;
    data_off = r32le(p + 8);
    data_len = r32le(p + 12);
    /* data_off is from the DataOffset field itself (byte 8). */
    if (8 + data_off + data_len > len)
        return -3;
    *eth = p + 8 + data_off;
    *ethlen = data_len;
    return 0;
}

int fcb_parse(const u8 *page, u32 len, struct fcb_info *out)
{
    memset(out, 0, sizeof(*out));
    if (len < 64)
        return -1;
    out->magic = (u32)page[0] | ((u32)page[1] << 8) |
                 ((u32)page[2] << 16) | ((u32)page[3] << 24);
    if (out->magic != FCB_MAGIC)
        return -2;
    out->version = (u32)page[4] | ((u32)page[5] << 8) |
                   ((u32)page[6] << 16) | ((u32)page[7] << 24);
    out->page_data_size = (u32)page[20] | ((u32)page[21] << 8) |
                          ((u32)page[22] << 16) | ((u32)page[23] << 24);
    out->total_page_size = (u32)page[24] | ((u32)page[25] << 8) |
                           ((u32)page[26] << 16) | ((u32)page[27] << 24);
    out->sectors_per_block = (u32)page[28] | ((u32)page[29] << 8) |
                             ((u32)page[30] << 16) | ((u32)page[31] << 24);
    out->pages_per_block = (u32)page[36] | ((u32)page[37] << 8) |
                           ((u32)page[38] << 16) | ((u32)page[39] << 24);
    if (!out->page_data_size || out->page_data_size > 16384)
        return -3;
    out->valid = 1;
    return 0;
}
