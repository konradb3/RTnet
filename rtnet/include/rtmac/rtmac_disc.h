/* include/rtmac/rtmac_disc.h
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

#ifndef __RTMAC_DISC_H_
#define __RTMAC_DISC_H_

#ifdef __KERNEL__

#include <rtdev.h>
#include <rtskb.h>
#include <rtmac/rtmac.h>


struct rtmac_disc_ops {
	int (*init)			(struct rtnet_device *rtdev);
	int (*release)			(struct rtnet_device *rtdev);
	// FIXME: get_mtu + get_cycle
};

struct rtmac_disc_type {
	int (*packet_rx)		(struct rtskb *skb, struct rtnet_device *rtdev, struct rtpacket_type *pt);
	int (*rt_packet_tx)		(struct rtskb *skb, struct rtnet_device *rtdev);
	int (*proxy_packet_tx)		(struct rtskb *skb, struct rtnet_device *rtdev);
	struct rtmac_disc_ops		*disc_ops;
	struct rtmac_ioctl_ops		*ioctl_ops;
};

extern int rtmac_disc_init(struct rtnet_device *rtdev, struct rtmac_disc_type *disc);
extern int rtmac_disc_release(struct rtnet_device *rtdev);


#endif /* __KERNEL__ */

#endif /* __RTMAC_DISC_H_ */
