/* include/rtmac/rtmac_vnic.h
 *
 * rtmac - real-time networking media access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>,
 *               2003 Jan Kiszka <Jan.Kiszka@web.de>
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

#ifndef __RTMAC_VNIC_H_
#define __RTMAC_VNIC_H_

#ifdef __KERNEL__


#define DEFAULT_VNIC_RTSKBS     32


extern int rtmac_vnic_rx(struct rtskb *skb, u16 type);

extern int rtmac_vnic_add(struct rtnet_device *rtdev);
extern void rtmac_vnic_remove(struct rtnet_device *rtdev);

extern int __init rtmac_vnic_module_init(void);
extern void rtmac_vnic_module_cleanup(void);


#endif /* __KERNEL__ */

#endif /* __RTMAC_VNIC_H_ */
