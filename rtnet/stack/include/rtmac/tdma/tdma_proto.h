/***
 *
 *  include/rtmac/tdma/tdma_proto.h
 *
 *  RTmac - real-time networking media access control subsystem
 *  Copyright (C) 2002       Marc Kleine-Budde <kleine-budde@gmx.de>,
 *                2003, 2004 Jan Kiszka <Jan.Kiszka@web.de>
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

#ifndef __TDMA_PROTO_H_
#define __TDMA_PROTO_H_

#include <rtdev.h>

#include <rtmac/tdma/tdma.h>


#define TDMA_FRM_VERSION    0x0200

#define TDMA_FRM_SYNC       0x0000
#define TDMA_FRM_CAL_REQ    0x0010
#define TDMA_FRM_CAL_RPL    0x0011


struct tdma_frm_head {
    u16                     version;
    u16                     id;
} __attribute__((packed));


#define SYNC_FRM(head)      ((struct tdma_frm_sync *)(head))

struct tdma_frm_sync {
    struct tdma_frm_head    head;
    u16                     cycle_no;
    u64                     xmit_stamp;
    u64                     sched_xmit_stamp;
} __attribute__((packed));


void tdma_xmit_sync_frame(struct tdma_priv *tdma);

int tdma_rt_packet_tx(struct rtskb *rtskb, struct rtnet_device *rtdev);
int tdma_nrt_packet_tx(struct rtskb *rtskb);

int tdma_packet_rx(struct rtskb *rtskb);

#endif /* __TDMA_PROTO_H_ */
