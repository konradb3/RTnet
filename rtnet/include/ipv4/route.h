/* ipv4/route.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 1999,2000 Zentropic Computing, LLC
 *               2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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
#ifndef __RTNET_ROUTE_H_
#define __RTNET_ROUTE_H_

#include <linux/types.h>

#include <rtdev.h>
#include <rtskb.h>


/*
 *  we have two routing tables, the generic routing table and the
 *  specific routing table.  The specific routing table is the table
 *  actually used to route outgoing packets.  The generic routing
 *  table is used to discover specific routes when needed.
 */
struct rt_rtable {
	struct rt_rtable	*prev;
	struct rt_rtable	*next;

	unsigned int 		use_count;

	__u32			rt_dst;
	__u32			rt_dst_mask;
	char			rt_dst_mac_addr[6];

	__u32			rt_src;
	
	int			rt_ifindex;

	struct rtnet_device 	*rt_dev;
};


extern struct rt_rtable *rt_rtables;
extern struct rt_rtable *rt_rtables_generic;

extern struct rt_rtable *rt_ip_route_add(struct rtnet_device *rtdev, u32 addr, u32 mask);
extern struct rt_rtable *rt_ip_route_add_specific(struct rtnet_device *rtdev, u32 addr,
                                                  unsigned char *hw_addr);
extern void rt_ip_route_del(struct rtnet_device *rtdev);
extern void rt_ip_route_del_specific(struct rtnet_device *rtdev, u32 addr);
extern struct rt_rtable *rt_ip_route_find(u32 daddr);

extern int rt_ip_route_input(struct rtskb *skb, u32 daddr, u32 saddr, struct rtnet_device *rtdev);
extern int rt_ip_route_output(struct rt_rtable **rp, u32 daddr, u32 saddr);

extern void rt_ip_routing_init(void);
extern void rt_ip_routing_release(void);


#endif  /* __RTNET_ROUTE_H_ */
