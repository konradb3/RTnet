/* rtnet_chrdev.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 1999    Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002, Ulrich Marx <marx@fet.uni-hannover.de>
 *               2003, Jan Kiszka <jan.kiszka@web.de>
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
#ifndef __RTNET_CHRDEV_H_
#define __RTNET_CHRDEV_H_

#ifdef __KERNEL__

#include <linux/list.h>
#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/netdevice.h>
#include <linux/types.h>


struct rtnet_device;

/* new extensible interface */
struct rtnet_ioctls {
    /* internal usage only */
    struct list_head entry;
    atomic_t         ref_count;

    /* provider specification */
    const char       *service_name;
    unsigned int     ioctl_type;
    int              (*handler)(struct rtnet_device *rtdev,
                                unsigned int request, unsigned long arg);
};

extern int rtnet_register_ioctls(struct rtnet_ioctls *ioctls);
extern void rtnet_unregister_ioctls(struct rtnet_ioctls *ioctls);

extern int __init rtnet_chrdev_init(void);
extern void rtnet_chrdev_release(void);

#else   /* ifndef __KERNEL__ */

#include <net/if.h>             /* IFNAMSIZ */
#include <linux/types.h>

#endif  /* __KERNEL__ */


#include <rtdev.h>


#define RTNET_MINOR             240 /* user interface for /dev/rtnet */
#define DEV_ADDR_LEN            32  /* avoids inconsistent MAX_ADDR_LEN */


struct rtnet_ioctl_head {
    char if_name[IFNAMSIZ];
};

struct rtnet_core_cmd {
    struct rtnet_ioctl_head head;

    union {
        /*** rtifconfig **/
        struct {
            __u32           ip_addr;
            __u32           broadcast_ip;
            unsigned int    set_dev_flags;
            unsigned int    clear_dev_flags;
        } up;

        struct {
            int             ifindex;
            unsigned short  type;
            __u32           ip_addr;
            __u32           broadcast_ip;
            unsigned int    mtu;
            unsigned int    flags;
            unsigned char   dev_addr[DEV_ADDR_LEN];
        } info;

        /*** rtroute ***/
        struct {
            __u32           ip_addr;
        } solicit;

        struct {
            __u32           ip_addr;
            unsigned char   dev_addr[DEV_ADDR_LEN];
        } addhost;

        struct {
            __u32           ip_addr;
        } delhost;

        struct {
            __u32           net_addr;
            __u32           net_mask;
            __u32           gw_addr;
        } addnet;

        struct {
            __u32           net_addr;
            __u32           net_mask;
        } delnet;
    } args;
};


#define RTNET_IOC_NODEV_PARAM           0x80

#define RTNET_IOC_TYPE_CORE             0
#define RTNET_IOC_TYPE_RTCFG            1
#define RTNET_IOC_TYPE_RTMAC_TDMA       100

#define IOC_RT_IFUP                     _IOW(RTNET_IOC_TYPE_CORE, 0,    \
                                             struct rtnet_core_cmd)
#define IOC_RT_IFDOWN                   _IOW(RTNET_IOC_TYPE_CORE, 1,    \
                                             struct rtnet_core_cmd)
#define IOC_RT_IFINFO                   _IOWR(RTNET_IOC_TYPE_CORE, 2 |  \
                                              RTNET_IOC_NODEV_PARAM,    \
                                              struct rtnet_core_cmd)
#define IOC_RT_HOST_ROUTE_ADD           _IOW(RTNET_IOC_TYPE_CORE, 3,    \
                                             struct rtnet_core_cmd)
#define IOC_RT_HOST_ROUTE_SOLICIT       _IOW(RTNET_IOC_TYPE_CORE, 4,    \
                                             struct rtnet_core_cmd)
#define IOC_RT_HOST_ROUTE_DELETE        _IOW(RTNET_IOC_TYPE_CORE, 5 |   \
                                             RTNET_IOC_NODEV_PARAM,     \
                                             struct rtnet_core_cmd)
#define IOC_RT_NET_ROUTE_ADD            _IOR(RTNET_IOC_TYPE_CORE, 6 |   \
                                             RTNET_IOC_NODEV_PARAM,     \
                                             struct rtnet_core_cmd)
#define IOC_RT_NET_ROUTE_DELETE         _IOR(RTNET_IOC_TYPE_CORE, 7 |   \
                                             RTNET_IOC_NODEV_PARAM,     \
                                             struct rtnet_core_cmd)

#endif  /* __RTNET_CHRDEV_H_ */
