/* ipv4/arp.h
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
#ifndef __RTNET_ARP_H_
#define __RTNET_ARP_H_

#include <linux/types.h>

#include <rtdev.h>


#define RT_ARP_SKB_PRIO  QUEUE_MIN_PRIO-1

#define RT_ARP_ADDR_LEN  6
#define RT_ARP_TABLE_LEN 20


struct rt_arp_table_struct {
	struct rt_arp_table_struct	*next;
	struct rt_arp_table_struct	*prev;

	u32				ip_addr;
	char				hw_addr[RT_ARP_ADDR_LEN];
};


extern struct rt_arp_table_struct *free_arp_list;
extern struct rt_arp_table_struct *arp_list;

extern int rt_arp_solicit(struct rtnet_device *dev,u32 target);
extern void rt_arp_table_add(u32 ip_addr, unsigned char *hw_addr);
extern void rt_arp_table_del(u32 ip_addr);
extern void rt_arp_table_init(void);
extern void rt_arp_init(void);
extern void rt_arp_release(void);
extern struct rt_arp_table_struct *rt_arp_table_lookup(u32 ip_addr);
extern struct rt_arp_table_struct *rt_rarp_table_lookup(char *hw_addr);

extern void rt_arp_table_display(void);


#endif  /* __RTNET_ARP_H_ */
