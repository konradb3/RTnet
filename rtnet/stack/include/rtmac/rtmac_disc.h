/***
 *
 *  include/rtmac/rtmac_disc.h
 *
 *  rtmac - real-time networking media access control subsystem
 *  Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>,
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

#ifndef __RTMAC_DISC_H_
#define __RTMAC_DISC_H_

#include <linux/list.h>
#include <linux/netdevice.h>

#include <rtdev.h>
#include <rtnet_chrdev.h>


struct rtmac_priv {
    int (*orig_start_xmit)(struct rtskb *skb, struct rtnet_device *dev);
    struct net_device       vnic;
    struct net_device_stats vnic_stats;
    struct rtskb_queue      vnic_skb_pool;
    int                     vnic_registered;

    u8                      disc_priv[0] __attribute__ ((aligned(16)));
};

struct rtmac_proc_entry {
    const char *name;
    int (*handler)(char *buf, char **start, off_t offset, int count, int *eof,
                   void *data);
};

struct rtmac_disc {
    struct list_head    list;

    const char          *name;
    unsigned int        priv_size;      /* size of rtmac_priv.disc_priv */
    u16                 disc_type;

    int                 (*packet_rx)(struct rtskb *skb);
    /* rt_packet_tx prototype must be compatible with hard_start_xmit */
    int                 (*rt_packet_tx)(struct rtskb *skb,
                                        struct rtnet_device *dev);
    int                 (*nrt_packet_tx)(struct rtskb *skb);

    int                 (*attach)(struct rtnet_device *rtdev, void *disc_priv);
    int                 (*detach)(struct rtnet_device *rtdev, void *disc_priv);

    struct rtnet_ioctls ioctls;

    struct rtmac_proc_entry *proc_entries;
};


extern int rtmac_disc_attach(struct rtnet_device *rtdev,
                             struct rtmac_disc *disc);
extern int rtmac_disc_detach(struct rtnet_device *rtdev);

extern struct rtmac_disc *rtmac_get_disc_by_name(const char *name);

extern int rtmac_disc_register(struct rtmac_disc *disc);
extern void rtmac_disc_deregister(struct rtmac_disc *disc);

#ifdef CONFIG_PROC_FS
extern int rtmac_proc_read_disc(char *buf, char **start, off_t offset,
                                int count, int *eof, void *data);
#endif /* CONFIG_PROC_FS */


#endif /* __RTMAC_DISC_H_ */
