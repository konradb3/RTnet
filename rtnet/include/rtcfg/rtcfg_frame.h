/***
 *
 *  include/rtcfg/rtcfg_frame.h
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003 Jan Kiszka <jan.kiszka@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __RTCFG_FRAME_H_
#define __RTCFG_FRAME_H_

#include <linux/init.h>
#include <linux/if_packet.h>

#include <rtcfg/rtcfg_event.h>


#define RTCFG_ID_STAGE_1_CFG        0
#define RTCFG_ID_ANNOUNCE_NEW       1
#define RTCFG_ID_ANNOUNCE_REPLY     2
#define RTCFG_ID_STAGE_2_CFG        3
#define RTCFG_ID_STAGE_2_CFG_FRAG   4
#define RTCFG_ID_ACK_CFG            5
#define RTCFG_ID_HEARTBEAT          6

#define RTCFG_ADDR_MAC              0
#define RTCFG_ADDR_IP               1

#define RTCFG_ADDRSIZE_MAC          0
#define RTCFG_ADDRSIZE_IP           4
#define RTCFG_MAX_ADDRSIZE          RTCFG_ADDRSIZE_IP


struct rtcfg_frm_head {
    u8 id:5,
       version:3;
} __attribute__((packed));

struct rtcfg_frm_stage_1_cfg {
    struct rtcfg_frm_head head;
    u8                    addr_type;
    u8                    client_addr[0];
    u8                    server_addr[0];
    u8                    burst_rate;
    u16                   cfg_len;
    u8                    cfg_data[0];
} __attribute__((packed));

struct rtcfg_frm_announce {
    struct rtcfg_frm_head head;
    u8                    addr_type;
    u8                    addr[0];
} __attribute__((packed));

struct rtcfg_frm_announce_new {
    struct rtcfg_frm_head head;
    u8                    addr_type;
    u8                    addr[0];
    u8                    get_cfg;
    u8                    burst_rate;
} __attribute__((packed));

struct rtcfg_frm_stage_2_cfg {
    struct rtcfg_frm_head head;
    u32                   clients;
    u16                   heartbeat_period;
    u32                   cfg_len;
    u8                    cfg_data[0];
} __attribute__((packed));

struct rtcfg_frm_stage_2_cfg_frag {
    struct rtcfg_frm_head head;
    u32                   frag_offs;
    u8                    cfg_data[0];
} __attribute__((packed));

struct rtcfg_frm_ack_cfg {
    struct rtcfg_frm_head head;
    u32                   ack_len;
} __attribute__((packed));

struct rtcfg_frm_heartbeat {
    struct rtcfg_frm_head head;
} __attribute__((packed));


struct rtcfg_frm_event {
    int                   frm_size;
    struct rtcfg_frm_head *frm_head;
    struct sockaddr_ll    *addr;
};


int rtcfg_send_stage_1(struct rtcfg_connection *conn);
int rtcfg_send_stage_2(struct rtcfg_connection *conn);
int rtcfg_send_announce_new(int ifindex);
int rtcfg_send_announce_reply(int ifindex, u8 *dest_mac_addr);
int rtcfg_send_ack(int ifindex);

int __init rtcfg_init_frames(void);
void rtcfg_cleanup_frames(void);

#endif /* __RTCFG_FRAME_H_ */
