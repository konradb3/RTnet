/***
 *
 *  include/rtcfg/rtcfg_event.h
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

#ifndef __RTCFG_EVENT_H_
#define __RTCFG_EVENT_H_

#include <linux/netdevice.h>

#include <rtdev.h>


#define EVENT_PENDING       1000 /* result value for blocked events */


typedef enum {
    RTCFG_CMD_SERVER,
    RTCFG_CMD_ADD_IP,
    RTCFG_CMD_DEL,
    RTCFG_CMD_WAIT,
    RTCFG_CMD_CLIENT,
    RTCFG_CMD_ANNOUNCE,
    RTCFG_FRM_STAGE_1_CFG,
    RTCFG_FRM_ANNOUNCE_NEW,
    RTCFG_FRM_ANNOUNCE_REPLY,
    RTCFG_FRM_STAGE_2_CFG,
    RTCFG_FRM_STAGE_2_CFG_FRAG,
    RTCFG_FRM_ACK_CFG,
    RTCFG_FRM_HEARTBEAT
} RTCFG_EVENT;

typedef enum {
    RTCFG_MAIN_OFF,
    RTCFG_MAIN_SERVER_RUNNING,
    RTCFG_MAIN_CLIENT_0,
    RTCFG_MAIN_CLIENT_1,
    RTCFG_MAIN_CLIENT_ANNOUNCED,
    RTCFG_MAIN_CLIENT_ALL_KNOWN,
    RTCFG_MAIN_CLIENT_ALL_FRAMES,
    RTCFG_MAIN_CLIENT_2
} RTCFG_MAIN_STATE;

typedef enum {
    RTCFG_CONN_SEARCHING,
    RTCFG_CONN_1,
    RTCFG_CONN_ESTABLISHED
} RTCFG_CONN_STATE;

struct rtcfg_connection {
    struct list_head entry;
    int              ifindex;
    RTCFG_CONN_STATE state;
    u8               mac_addr[MAX_ADDR_LEN];
    u8               addr_type;
    union {
        u32          ip_addr;
    } addr;
    u32              cfg_offs;
};

struct rtcfg_device {
    RTCFG_MAIN_STATE state;
    struct list_head conn_list;
    u32              clients;
    u32              clients_found;
    SEM              dev_sem;
    struct list_head event_list;
    spinlock_t       event_list_lock;
    RT_TASK          timer_task;

    /* client related */
    u8               addr_type;
    u8               srv_mac_addr[MAX_ADDR_LEN];
    u32              cfg_offs;
    u32              max_clients;
    u8               *client_addr_list;
};


extern struct rtcfg_device device[MAX_RT_DEVICES];


int rtcfg_do_main_event(int ifindex, RTCFG_EVENT event_id, void* event_data);

void rtcfg_init_state_machines(void);
void rtcfg_cleanup_state_machines(void);

#endif /* __RTCFG_EVENT_H_ */
