/* include/rtmac_tdma.h
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __TDMA_H_
#define __TDMA_H_

#ifndef _LINUX_NETDEVICE_H
#include <linux/netdevice.h>
#endif

#ifndef __RTNET_H_
#include <rtnet.h>
#endif

#ifndef __RTMAC_H_
#include <rtmac.h>
#endif

#ifndef __TDMA_EVENT_H_
#include <tdma_event.h>
#endif



#ifndef CONFIG_TDMA_DEBUG
#define CONFIG_TDMA_DEBUG
#endif //CONFIG_TDMA_DEBUG





#ifdef __KERNEL__

#define TDMA_RT_REQ_TIMEOUT		100					// timeout for arp solicit = 100 ms
#define TDMA_NOTIFY_TASK_CYCLE		100					// notify every 100 ms the presence of a master
#define TDMA_MASTER_WAIT_TIMEOUT	TDMA_NOTIFY_TASK_CYCLE * 2		// time to wait (in ms) before we become master
#define TDMA_SENT_CLIENT_CONF_TIMEOUT	100					// time (in ms) that have clients to ack config
#define TDMA_SENT_CLIENT_ACK_TIMEOUT	200					// time (in ms) the master has to ack the ack
#define TDMA_MASTER_MAX_TEST_TIME	2000					// time (in ms) the master sends test packets to client
#define TDMA_MASTER_MAX_TEST		(TDMA_MASTER_MAX_TEST_TIME * 1000 / tdma->cycle)	// number of test packets sent to the client
#define TDMA_MASTER_WAIT_TEST		100					// time (in ms) to wait after last test packet

#define TDMA_MAX_RT			16

#define TDMA_MAX_TX_QUEUE		4
#define TDMA_PRIO_TX_TASK		0

#define TDMA_VERSION			0x01

#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)

struct tdma_flags {
	unsigned mac_active	: 1;
	unsigned task_active	: 1;
	unsigned shutdown_task	: 1;
};

struct rtmac_tdma {
	SEM				free;
	SEM				full;
	RT_TASK				tx_task;
	struct rtskb_head		tx_queue;

	struct tdma_flags		flags;

	volatile TDMA_STATE 	      	state;
	unsigned int			cycle;					// cycle in us
	unsigned int			mtu;
	struct rtmac_device		*rtmac;

	struct timer_list		task_change_timer;

	/*** rt master specific ***/
	struct timer_list		rt_add_timer;
	struct list_head		rt_add_list;
	struct list_head		rt_list;
	struct list_head		rt_list_rate;

	struct timer_list	     	master_wait_timer;
	struct timer_list		master_sent_conf_timer;
	struct timer_list		master_sent_test_timer;

	struct rtskb_head		master_queue;

	/*** rt client specific ***/
	SEM				client_tx;
	RTIME				wakeup;
	RTIME				offset;					// in internals counts
	unsigned char			station;
	struct rt_arp_table_struct	*master;
	struct timer_list		client_sent_ack_timer;

	/*** non realtime discipline stuff ***/
	//struct list_head		nrt_list;
};

struct tdma_rt_entry {
	struct list_head		list;
	struct list_head		list_rate;
	struct rt_arp_table_struct	*arp;
	unsigned char			station;
	unsigned int			counter;
	RTIME				tx;
	unsigned int		      	rtt;
	volatile TDMA_RT_STATE 		state;
};

struct tdma_rt_add_entry {
	struct list_head		list;
	u32				ip_addr;
	unsigned char			station;
	int				timeout;
};

struct tdma_info {
	unsigned int 			cycle;
	unsigned int			mtu;
	unsigned int			offset;
	u32			  	ip_addr;
	struct rtnet_device		*rtdev;
};

struct tdma_hdr {
	u32				msg;
} __attribute__ ((packed));


struct tdma_conf_msg {
	u8				station;
	u8				padding;
	u16				mtu;
	u32			   	cycle;
	//FIXME: crc32
} __attribute__ ((packed));


struct tdma_test_msg {
	u32				counter;
	s64				tx;
} __attribute__ ((packed));

struct tdma_station_list_hdr {
	u8				no_of_stations;
	u8				padding_foo;
	u16				padding_bar;
} __attribute__ ((packed));

struct tdma_station_list {
	u32				ip_addr;
	u8				station;
	u8				padding_foo;
	u16				padding_bar;
} __attribute__ ((packed));

struct tdma_offset_msg {
	u32				offset;
} __attribute__ ((packed));

/***
 * rtmac_tdma.c
 */

extern int tdma_init(struct rtnet_device *rtdev);
extern int tdma_release(struct rtnet_device *rtdev);





/***
 * tdma_ioctl.c
 */

extern int tdma_ioctl_client(struct rtnet_device *rtdev);
extern int tdma_ioctl_master(struct rtnet_device *rtdev, unsigned int cycle, unsigned int mtu);
extern int tdma_ioctl_up(struct rtnet_device *rtdev);
extern int tdma_ioctl_down(struct rtnet_device *rtdev);
extern int tdma_ioctl_add(struct rtnet_device *rtdev, u32 ip_addr);
extern int tdma_ioctl_remove(struct rtnet_device *rtdev, u32 ip_addr);
extern int tdma_ioctl_cycle(struct rtnet_device *rtdev, unsigned int cycle);
extern int tdma_ioctl_mtu(struct rtnet_device *rtdev, unsigned int mtu);
extern int tdma_ioctl_offset(struct rtnet_device *rtdev, u32 ip_addr, unsigned int offset);



/***
 * tdma_task.c
 */
extern void tdma_task_shutdown(struct rtmac_tdma *tdma);
extern int tdma_task_change(struct rtmac_tdma *tdma, void (*task) (int rtdev_id), unsigned int cycle);
extern int tdma_task_change_con(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle);


extern void tdma_task_notify(int rtdev_id);
extern void tdma_task_config(int rtdev_id);
extern void tdma_task_master(int rtdev_id);
extern void tdma_task_client(int rtdev_id);



/***
 * tdma_timer.c
 */
typedef void (*TIMER_CALLBACK)(void *);

extern void tdma_timer_start_rt_add(struct rtmac_tdma *tdma, int timeout);
extern void tdma_timer_start_master_wait(struct rtmac_tdma *tdma, int timeout);
extern void tdma_timer_start_sent_conf(struct rtmac_tdma *tdma, int timeout);
extern int tdma_timer_start_task_change(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle, int timeout);
extern void tdma_timer_start_sent_ack(struct rtmac_tdma *tdma, int timeout);
extern void tdma_timer_start_sent_test(struct rtmac_tdma *tdma, int timeout);



/***
 * tdma_event.c
 */
extern void tdma_next_state(struct rtmac_tdma *tdma, TDMA_STATE state);
extern int tdma_do_event(struct rtmac_tdma *tdma, TDMA_EVENT event, struct tdma_info *info);



/***
 * tdma_rx.c
 */
extern int tdma_packet_rx(struct rtskb *skb, struct rtnet_device *rtdev, struct rtpacket_type *pt);



/***
 * tdma_cleanup.c
 */
extern void tdma_cleanup_master_rt(struct rtmac_tdma *tdma);
extern void tdma_cleanup_master_rt_check(struct rtmac_tdma *tdma);
extern void tdma_cleanup_client_rt(struct rtmac_tdma *tdma);





static inline int list_len(struct list_head *list)
{
	struct list_head *lh;
	int len = 0;


	list_for_each(lh, list) {
		len++;
	}
	
	return len;
}



static inline struct rtskb *tdma_make_msg_len(struct rtnet_device *rtdev, void *daddr, TDMA_EVENT event, unsigned int data_len, void **data)
{
	struct net_device *dev = dev_get_by_rtdev(rtdev);
	struct rtskb *skb;
	unsigned int packet_len = 60 + 2 + dev->hard_header_len;

	struct rtmac_hdr *rtmac_ptr;
	struct tdma_hdr *tdma_ptr;

	if (daddr == NULL) 
		daddr = dev->broadcast;
	
	/*
	 * allocate packet
	 */
	skb = alloc_rtskb(packet_len);
	if (!skb) 
		return NULL;

	rtskb_reserve(skb, (dev->hard_header_len+15) & ~15);

	/*
	 * give values to the skb and setup header...
	 */
	skb->rtdev = rtdev;
	skb->protocol = __constant_htons(ETH_RTMAC); // FIXME: needed?

	if(rtdev->hard_header && rtdev->hard_header(skb, rtdev, ETH_RTMAC, daddr, dev->dev_addr, skb->len) < 0)
		goto out;

	/*
	 * setup header-pointer
	 */
	rtmac_ptr = (struct rtmac_hdr *)rtskb_put(skb, data_len);
	tdma_ptr = (struct tdma_hdr *)(rtmac_ptr + 1);
	*data = (void *)(tdma_ptr + 1);

	/*
	 * assign data to pointers
	 */
	rtmac_ptr->disc = TDMA;
	rtmac_ptr->ver = TDMA_VERSION;
	tdma_ptr->msg = __constant_htonl(event);

	return skb;

out:
	kfree_rtskb(skb);
	return NULL;

}


static inline struct rtskb *tdma_make_msg(struct rtnet_device *rtdev, void *daddr, TDMA_EVENT event, void **data)
{
	return tdma_make_msg_len(rtdev, daddr, event, 60-14, data);	//note: dev->hard_header_len == 14
}








/***
 * debug'in suff 
 */
#ifdef CONFIG_TDMA_DEBUG

extern __u32 tdma_debug;

/* use 0 for production, 1 for verification, >2 for debug */
#define TDMA_DEBUG_LEVEL 4

#define TDMA_DEBUG(n, args...) (tdma_debug >= (n)) ? (rt_printk(KERN_DEBUG args)) : 0
#define ASSERT(expr, func) \
if(!(expr)) { \
        rt_printk( "Assertion failed! %s:%s:%d %s\n", \
        __FILE__,__FUNCTION__,__LINE__,(#expr));  \
        func }
#else
#define TDMA_DEBUG(n, args...)
#define ASSERT(expr, func) \
if(!(expr)) do { \
        func } while (0)
#endif /* CONFIG_TDMA_DEBUG */


#endif //__KERNEL__
#endif //__TDMA_H_
