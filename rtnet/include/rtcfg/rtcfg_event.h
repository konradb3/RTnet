/***
 *
 *  include/rtcfg/rtcfg_event.h
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003, 2004 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/if_ether.h>
#include <linux/netdevice.h>

#include <rtcfg_chrdev.h>
#include <rtdev.h>
#include <rtnet_internal.h>
#include <rtnet_rtpc.h>


typedef enum {
    RTCFG_MAIN_OFF,
    RTCFG_MAIN_SERVER_RUNNING,
    RTCFG_MAIN_CLIENT_0,
    RTCFG_MAIN_CLIENT_1,
    RTCFG_MAIN_CLIENT_ANNOUNCED,
    RTCFG_MAIN_CLIENT_ALL_KNOWN,
    RTCFG_MAIN_CLIENT_ALL_FRAMES,
    RTCFG_MAIN_CLIENT_2,
    RTCFG_MAIN_CLIENT_READY
} RTCFG_MAIN_STATE;

struct rtcfg_station {
    u8 mac_addr[ETH_ALEN]; /* Ethernet-specific! */
    u8 flags;
};

struct rtcfg_device {
    RTCFG_MAIN_STATE        state;
    u32                     other_stations;
    u32                     stations_found;
    u32                     stations_ready;
    rtos_res_lock_t         dev_lock;
    struct list_head        event_calls;
    rtos_spinlock_t         event_calls_lock;
    rtos_task_t             timer_task;
    unsigned int            flags;
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry   *proc_entry;
#endif

    /* client related */
    unsigned int            addr_type;
    u8                      srv_mac_addr[ETH_ALEN]; /* Ethernet-specific! */
    u8                      *stage2_buffer;
    u32                     cfg_len;
    u32                     cfg_offs;
    unsigned int            packet_counter;
    u32                     chain_len;
    struct rtskb            *stage2_chain;
    u32                     max_stations;
    struct rtcfg_station    *station_addr_list;

    /* server related */
    u32                     clients_configured;
    struct list_head        conn_list;
    unsigned int            burstrate;
};


extern struct rtcfg_device device[MAX_RT_DEVICES];


int rtcfg_do_main_event(int ifindex, RTCFG_EVENT event_id, void* event_data);

struct rt_proc_call *rtcfg_dequeue_blocking_call(int ifindex);

void rtcfg_init_state_machines(void);
void rtcfg_cleanup_state_machines(void);

#endif /* __RTCFG_EVENT_H_ */
