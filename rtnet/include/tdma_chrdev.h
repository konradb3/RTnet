/***
 *
 *  include/tdma_chrdev.h
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

#ifndef __TDMA_CHRDEV_H_
#define __TDMA_CHRDEV_H_

#include <rtnet_chrdev.h>


struct tdma_config {
    struct rtnet_ioctl_head head;

    __u32           ip_addr;
    unsigned int    cycle;
    unsigned int    mtu;
    unsigned int    offset;
};


#define TDMA_IOC_CLIENT                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 0, \
                                             struct tdma_config)
#define TDMA_IOC_MASTER                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 1, \
                                             struct tdma_config)
#define TDMA_IOC_UP                     _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 2, \
                                             struct tdma_config)
#define TDMA_IOC_DOWN                   _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 3, \
                                             struct tdma_config)
#define TDMA_IOC_ADD                    _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 4, \
                                             struct tdma_config)
#define TDMA_IOC_REMOVE                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 5, \
                                             struct tdma_config)
#define TDMA_IOC_CYCLE                  _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 6, \
                                             struct tdma_config)
#define TDMA_IOC_MTU                    _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 7, \
                                             struct tdma_config)
#define TDMA_IOC_OFFSET                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 8, \
                                             struct tdma_config)

#endif /* __TDMA_CHRDEV_H_ */
