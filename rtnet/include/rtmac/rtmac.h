/* include/rtmac.h
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

#ifndef __RTMAC_H_INTERNAL_
#define __RTMAC_H_INTERNAL_

#ifdef __KERNEL__

#include <linux/types.h>


#define RTMAC_VERSION	0x1
#define ETH_RTMAC	0x9021


struct rtmac_device {
	struct rtnet_device		*rtdev;
	void				*priv;

	struct rtmac_ioctl_ops		*ioctl_ops;
	struct rtmac_disc_ops		*disc_ops;
	struct rtmac_disc_type		*disc_type;
};

struct rtmac_hdr {
	u16				type;
	u8				ver;
	u8				res;	/* reserved for future use :) */
} __attribute__ ((packed));


#endif /* __KERNEL__ */

#endif /* __RTMAC_H_INTERNAL_ */
