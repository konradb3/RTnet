/***
 *
 *  include/rtcfg/rtcfg_ui.h
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

#ifndef __RTCFG_UI_H_
#define __RTCFG_UI_H_

#include <linux/init.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <asm/atomic.h>

#include <rtcfg/rtcfg_event.h>


struct rtcfg_user_event {
    struct list_head     list_entry;
    volatile int         processed;
    int                  result;
    atomic_t             ref_count;
    wait_queue_head_t    event_wq;
    RTCFG_EVENT          event_id;
    int                  ifindex;
    void*                buffer;
    union {
        u32              ip_addr;
        struct {
            unsigned int period;
            unsigned int heartbeat;
            unsigned int threshold;
        } server;
        struct {
            u32          max_clients;
        } client;
    } args;
};


int rtcfg_cmd_server(int ifindex);
int rtcfg_cmd_add_ip(int ifindex, unsigned long ip_addr);
int rtcfg_cmd_wait(int ifindex);

int rtcfg_cmd_client(int ifindex);
int rtcfg_cmd_announce(int ifindex);

void rtcfg_complete_event(struct rtcfg_user_event *event, int result);
void rtcfg_complete_event_nrt(struct rtcfg_user_event *event, int result);

int __init rtcfg_init_ui(void);
void rtcfg_cleanup_ui(void);

#endif /* __RTCFG_UI_H_ */
