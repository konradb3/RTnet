/***
 *
 *  include/ipv4/route.h - real-time routing
 *
 *  Copyright (C) 2004 Jan Kiszka <jan.kiszka@web.de>
 *
 *  Rewritten version of the original route by David Schleef and Ulrich Marx
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

#ifndef __RTNET_ROUTE_H_
#define __RTNET_ROUTE_H_

#include <linux/init.h>
#include <linux/types.h>

#include <rtdev.h>


struct dest_route {
    u32                 ip;
    unsigned char       dev_addr[MAX_ADDR_LEN];
    struct rtnet_device *rtdev;
};


extern int rt_ip_route_add_host(u32 addr, unsigned char *dev_addr,
                                struct rtnet_device *rtdev);
extern int rt_ip_route_del_host(u32 addr);
extern void rt_ip_route_del_all(struct rtnet_device *rtdev);

#ifdef CONFIG_RTNET_NETWORK_ROUTING
extern int rt_ip_route_add_net(u32 addr, u32 mask, u32 gw_addr);
extern int rt_ip_route_del_net(u32 addr, u32 mask);
#endif /* CONFIG_RTNET_NETWORK_ROUTING */

extern int rt_ip_route_output(struct dest_route *rt_buf, u32 daddr);

#ifdef CONFIG_RTNET_ROUTER
extern int rt_ip_route_forward(struct rtskb *rtskb, u32 daddr);
#endif /* CONFIG_RTNET_ROUTER */

extern int __init rt_ip_routing_init(void);
extern void rt_ip_routing_release(void);


#endif  /* __RTNET_ROUTE_H_ */
