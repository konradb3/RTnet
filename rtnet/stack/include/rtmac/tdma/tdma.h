/***
 *
 *  include/rtmac/tdma/tdma.h
 *
 *  rtmac - real-time networking media access control subsystem
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

#ifndef __TDMA_H_INTERNAL_
#define __TDMA_H_INTERNAL_

#include <linux/types.h>

#include <rtdm_driver.h>

#include <rtdev.h>
#include <rtnet_internal.h>
#include <ipv4/route.h>
#include <rtmac/rtmac_disc.h>
#include <rtmac/rtmac_proto.h>


#define CONFIG_TDMA_DEBUG

#define TDMA_MAGIC                      0xA1B2C301

#define TDMA_RT_REQ_TIMEOUT             100     /* timeout for arp solicit = 100 ms */
#define TDMA_NOTIFY_TASK_CYCLE          100     /* notify every 100 ms the presence of a master */
#define TDMA_MASTER_WAIT_TIMEOUT        TDMA_NOTIFY_TASK_CYCLE * 2  /* time to wait (in ms) before we become master */
#define TDMA_SENT_CLIENT_CONF_TIMEOUT   100     /* time (in ms) that have clients to ack config */
#define TDMA_SENT_CLIENT_ACK_TIMEOUT    200     /* time (in ms) the master has to ack the ack */
#define TDMA_MASTER_MAX_TEST_TIME       2000    /* time (in ms) the master sends test packets to client */
#define TDMA_MASTER_MAX_TEST            (TDMA_MASTER_MAX_TEST_TIME * 1000 / tdma->cycle)    /* number of test packets sent to the client */
#define TDMA_MASTER_WAIT_TEST           100     /* time (in ms) to wait after last test packet */

#define TDMA_MAX_RT                     16

#define TDMA_MAX_TX_QUEUE               4
#define TDMA_PRIO_TX_TASK               0

#define TDMA_VERSION                    0x01
#define ETH_TDMA                        0x9031

#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)

/* tdma states */
typedef enum {
    TDMA_DOWN,

    TDMA_MASTER_DOWN,
    TDMA_MASTER_WAIT,
    TDMA_MASTER_SENT_CONF,
    TDMA_MASTER_SENT_TEST,

    TDMA_OTHER_MASTER,
    TDMA_CLIENT_DOWN,
    TDMA_CLIENT_ACK_CONF,
    TDMA_CLIENT_RCVD_ACK,
} TDMA_STATE;

typedef enum {
    RT_DOWN,
    RT_SENT_CONF,
    RT_RCVD_CONF,
    RT_SENT_TEST,
    RT_RCVD_TEST,
    RT_COMP_TEST,
    RT_CLIENT,
} TDMA_RT_STATE;


/* tdma events */
typedef enum {
    REQUEST_MASTER, /* 0 */
    REQUEST_CLIENT,

    REQUEST_UP, /* 2 */
    REQUEST_DOWN,

    REQUEST_ADD_RT, /* 4 */
    REQUEST_REMOVE_RT,

    REQUEST_ADD_NRT, /* 6 */
    REQUEST_REMOVE_NRT,

    CHANGE_MTU, /* 8 */
    CHANGE_CYCLE,
    CHANGE_OFFSET,

    EXPIRED_ADD_RT, /* 11 */
    EXPIRED_MASTER_WAIT,
    EXPIRED_MASTER_SENT_CONF,
    EXPIRED_MASTER_SENT_TEST,
    EXPIRED_CLIENT_SENT_ACK,

    NOTIFY_MASTER, /* 16 (0x10) */
    REQUEST_TEST,
    ACK_TEST,

    REQUEST_CONF, /* 19 (0x13) */
    ACK_CONF,
    ACK_ACK_CONF,

    STATION_LIST, /* 22 (0x16) */
    REQUEST_CHANGE_OFFSET,

    START_OF_FRAME,
} TDMA_EVENT;


struct tdma_flags {
    unsigned mac_active     : 1;
    unsigned task_active    : 1;
    unsigned shutdown_task  : 1;
};

struct rtmac_tdma {
    unsigned long               magic;
    struct rtskb_prio_queue     tx_queue;

#ifdef ALIGN_RTOS_TASK
    __u8                        __align[(ALIGN_RTOS_TASK -
                                         ((sizeof(unsigned long) +
                                           sizeof(struct rtskb_prio_queue)
                                          ) & (ALIGN_RTOS_TASK-1))
                                         ) & (ALIGN_RTOS_TASK-1)];
#endif
    rtos_task_t                 tx_task;

    struct tdma_flags           flags;

    volatile TDMA_STATE         state;
    unsigned int                cycle;  /* cycle in us */
    unsigned int                mtu;
    struct rtnet_device         *rtdev;

    struct timer_list           task_change_timer;

    struct rtdm_device          api_device;

    /*** rt master specific ***/
    struct timer_list           rt_add_timer;
    struct list_head            rt_add_list;
    struct list_head            rt_list;
    struct list_head            rt_list_rate;

    struct timer_list           master_wait_timer;
    struct timer_list           master_sent_conf_timer;
    struct timer_list           master_sent_test_timer;

    struct rtskb_queue          master_queue;

    /*** rt client specific ***/
    rtos_event_t                client_tx;
    rtos_time_t                 wakeup;
    rtos_time_t                 offset;
    unsigned char               station;
    unsigned char               master_hw_addr[MAX_ADDR_LEN];
    struct timer_list           client_sent_ack_timer;
    rtos_spinlock_t             delta_t_lock;
    nanosecs_t                  delta_t; /* offset to master clock */
};

struct tdma_rt_entry {
    struct list_head            list;
    struct list_head            list_rate;
    struct dest_route           arp;
    unsigned char               station;
    unsigned int                counter;
    nanosecs_t                  tx;
    unsigned int                rtt;
    volatile TDMA_RT_STATE      state;
    unsigned int                offset;
};

struct tdma_rt_add_entry {
    struct list_head            list;
    u32                         ip_addr;
    unsigned char               station;
    unsigned long               timeout;
    unsigned int                offset;
};

struct tdma_info {
    unsigned int                cycle;
    unsigned int                mtu;
    unsigned int                offset;
    u32                         ip_addr;
    struct rtnet_device         *rtdev;
};

struct tdma_hdr {
    u32                         msg;
} __attribute__ ((packed));


struct tdma_conf_msg {
    u8                          station;
    u8                          padding;
    u16                         mtu;
    u32                         cycle;
    /*FIXME: crc32*/
} __attribute__ ((packed));


struct tdma_test_msg {
    u32                         counter;
    s64                         tx;
} __attribute__ ((packed));

struct tdma_station_list_hdr {
    u8                          no_of_stations;
    u8                          padding_foo;
    u16                         padding_bar;
} __attribute__ ((packed));

struct tdma_station_list {
    u32                         ip_addr;
    u8                          station;
    u8                          padding_foo;
    u16                         padding_bar;
} __attribute__ ((packed));

struct tdma_offset_msg {
    u32                         offset;
} __attribute__ ((packed));



static inline int list_len(struct list_head *list)
{
    struct list_head *lh;
    int len = 0;


    list_for_each(lh, list) {
        len++;
    }

    return len;
}


static inline struct rtskb *tdma_make_msg_len(struct rtnet_device *rtdev,
    void *daddr, TDMA_EVENT event, unsigned int data_len, void **data)
{
    struct rtskb    *skb;
    struct tdma_hdr *tdma_ptr;


    if (daddr == NULL)
        daddr = rtdev->broadcast;

    /*
     * allocate packet
     */
    skb = alloc_rtskb(rtdev->hard_header_len + sizeof(struct rtmac_hdr) +
                      sizeof(struct tdma_hdr) + data_len + 15, &global_pool);
    if (!skb)
        return NULL;

    rtskb_reserve(skb,
        (rtdev->hard_header_len + sizeof(struct rtmac_hdr) + 15) & ~15);

    /*
     * setup pointers
     */
    tdma_ptr = (struct tdma_hdr *)rtskb_put(skb, sizeof(struct tdma_hdr));
    *data = (void *)rtskb_put(skb, data_len);

    /*
     * add rtmac and ethernet headers
     */
    if (rtmac_add_header(rtdev, daddr, skb, ETH_TDMA) <0 )
        goto out;

    /*
     * fill tdma header
     */
    tdma_ptr->msg = __constant_htonl(event);

    return skb;

out:
    kfree_rtskb(skb);
    return NULL;
}



static inline struct rtskb *tdma_make_msg(struct rtnet_device *rtdev, void *daddr, TDMA_EVENT event, void **data)
{
    return tdma_make_msg_len(rtdev, daddr, event, 60-14, data); /* note: dev->hard_header_len == 14 */
}



/***
 * debug'in suff
 */
#ifdef CONFIG_TDMA_DEBUG

extern __u32 tdma_debug;

/* use 0 for production, 1 for verification, >2 for debug */
#define TDMA_DEFAULT_DEBUG_LEVEL    2

#define TDMA_DEBUG(n, args...) \
    (tdma_debug >= (n)) ? (rtos_print(KERN_DEBUG args)) : 0
#else
#define TDMA_DEBUG(n, args...)
#endif /* CONFIG_TDMA_DEBUG */

#endif /* __TDMA_H_INTERNAL_ */
