/* include/rtmac/tdma/tdma_ioctl.h
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

#ifndef __TDMA_IOCTL_H_
#define __TDMA_IOCTL_H_

#ifdef __KERNEL__

#include <rtdev.h>


extern int tdma_ioctl(struct rtnet_device *rtdev, unsigned int request,
                      unsigned long arg);

#endif //__KERNEL__

#endif //__TDMA_IOCTL_H_
