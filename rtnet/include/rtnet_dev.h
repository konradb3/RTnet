/* rtnet_dev.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 1999    Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002, Ulrich Marx <marx@fet.uni-hannover.de>
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
#ifndef __RTNET_DEV_H_
#define __RTNET_DEV_H_

#include <linux/types.h>


#ifdef __KERNEL__

extern void rtnet_chrdev_init(void);
extern void rtnet_chrdev_release(void);

#endif  /* __KERNEL__ */


/* user interface for /dev/rtnet */
#define RTNET_MINOR			240

#define IOC_RT_IFUP			100
#define IOC_RT_IFDOWN			101
#define IOC_RT_IF			102
#define IOC_RT_ROUTE_ADD		103
#define IOC_RT_ROUTE_SOLICIT		104
#define IOC_RT_ROUTE_DELETE		105
#define IOC_RT_ROUTE_GET		106


struct rtnet_config{
	char if_name[16];
	int len;

	u32 ip_addr;
	u32 ip_mask;
	u32 ip_netaddr;
	u32 ip_broadcast;
};


#endif  /* __RTNET_DEV_H_ */
