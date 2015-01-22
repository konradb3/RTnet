/***
 *
 *  include/rtcfg/rtcfg_event.h
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003-2005 Jan Kiszka <jan.kiszka@web.de>
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


#define FLAG_TIMER_STARTED          0x00010000
#define FLAG_TIMER_SHUTDOWN         0x00020000


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
    RTCFG_MAIN_STATE                state;
    u32                             other_stations;
    u32                             stations_found;
    u32                             stations_ready;
    rtdm_mutex_t                    dev_mutex;
    struct list_head                event_calls;
    rtdm_lock_t                     event_calls_lock;
    rtdm_task_t                     timer_task;
    unsigned int                    flags;
    unsigned int                    burstrate;
#ifdef CONFIG_PROC_FS
    struct proc_dir_entry           *proc_entry;
    char                            proc_entry_name[64];
#endif

    union {
        struct {
            unsigned int            addr_type;
            union {
#ifdef CONFIG_RTNET_RTIPV4
                u32                 ip_addr;
#endif
            } srv_addr;
            u8                      srv_mac_addr[MAX_ADDR_LEN];
            u8                      *stage2_buffer;
            u32                     cfg_len;
            u32                     cfg_offs;
            unsigned int            packet_counter;
            u32                     chain_len;
            struct rtskb            *stage2_chain;
            u32                     max_stations;
            struct rtcfg_station    *station_addr_list;
        } clt;

        struct {
            u32                     clients_configured;
            struct list_head        conn_list;
            u16                     heartbeat;
            u64                     heartbeat_timeout;
        } srv;
    } spec;
};


extern struct rtcfg_device device[MAX_RT_DEVICES];
extern const char *rtcfg_event[];
extern const char *rtcfg_main_state[];


int rtcfg_do_main_event(int ifindex, RTCFG_EVENT event_id, void* event_data);
void rtcfg_next_main_state(int ifindex, RTCFG_MAIN_STATE state);

void rtcfg_queue_blocking_call(int ifindex, struct rt_proc_call *call);
struct rt_proc_call *rtcfg_dequeue_blocking_call(int ifindex);
void rtcfg_complete_cmd(int ifindex, RTCFG_EVENT event_id, int result);
void rtcfg_reset_device(int ifindex);

void rtcfg_init_state_machines(void);
void rtcfg_cleanup_state_machines(void);

#endif /* __RTCFG_EVENT_H_ */
