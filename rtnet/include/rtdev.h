/* rtdev.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 1999      Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002      Ulrich Marx <marx@kammer.uni-hannover.de>
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
#ifndef __RTDEV_H_
#define __RTDEV_H_

#ifdef __KERNEL__

#include <asm/atomic.h>
#include <linux/netdevice.h>

#include <rtskb.h>


#define MAX_RT_DEVICES      8

/***
 *  rtnet_device
 */
struct rtnet_device {
    /* Many field are borrowed from struct net_device in
     * <linux/netdevice.h> - WY
     */
    char                name[IFNAMSIZ];

    unsigned long       rmem_end;   /* shmem "recv" end     */
    unsigned long       rmem_start; /* shmem "recv" start   */
    unsigned long       mem_end;    /* shared mem end       */
    unsigned long       mem_start;  /* shared mem start     */
    unsigned long       base_addr;  /* device I/O address   */
    unsigned int        irq;        /* device IRQ number    */

    /*
     *  Some hardware also needs these fields, but they are not
     *  part of the usual set specified in Space.c.
     */
    unsigned char       if_port;    /* Selectable AUI, TP,..*/
    unsigned char       dma;        /* DMA channel          */

    unsigned long       state;
    int                 ifindex;
    atomic_t            refcount;

    struct module       *owner;

    __u32               local_addr; /* in network order */

    struct rtsocket     *protocols;

    unsigned short      flags;      /* interface flags (a la BSD)   */
    unsigned short      gflags;
    unsigned int        mtu;        /* eth = 1536, tr = 4...        */
    unsigned short      type;       /* interface hardware type      */
    unsigned short      hard_header_len;    /* hardware hdr length  */
    void                *priv;      /* pointer to private data      */
    int                 features;   /* NETIF_F_*                    */

    /* Interface address info. */
    unsigned char       broadcast[MAX_ADDR_LEN];    /* hw bcast add */
    unsigned char       dev_addr[MAX_ADDR_LEN];     /* hw address   */
    unsigned char       addr_len;   /* hardware address length      */

    struct dev_mc_list  *mc_list;   /* Multicast mac addresses      */
    int                 mc_count;   /* Number of installed mcasts   */
    int                 promiscuity;
    int                 allmulti;

    int                 rxqueue_len;
    rtos_event_sem_t    *stack_event;
/*    MBX                 *rtdev_mbx;*/

    rtos_res_lock_t     xmit_lock;

    unsigned int        add_rtskbs; /* additionally allocated global rtskbs */

    /* RTmac related fields */
    struct rtmac_disc   *mac_disc;
    struct rtmac_priv   *mac_priv;

    /* Device operations */
    int                 (*open)(struct rtnet_device *rtdev);
    int                 (*stop)(struct rtnet_device *rtdev);
    int                 (*hard_header)(struct rtskb *, struct rtnet_device *,
                                       unsigned short type, void *daddr,
                                       void *saddr, unsigned int len);
    int                 (*rebuild_header)(struct rtskb *);
    int                 (*hard_start_xmit)(struct rtskb *skb,
                                           struct rtnet_device *dev);
    int                 (*hw_reset)(struct rtnet_device *rtdev);
};


extern struct rtnet_device *rtnet_devices[];

extern struct rtnet_device *rt_alloc_etherdev(int sizeof_priv);
extern void rtdev_free(struct rtnet_device *rtdev);

extern int rt_register_rtnetdev(struct rtnet_device *rtdev);
extern int rt_unregister_rtnetdev(struct rtnet_device *rtdev);

extern void rtdev_alloc_name (struct rtnet_device *rtdev, const char *name_mask);

extern struct rtnet_device *rtdev_get_by_name(const char *if_name);
extern struct rtnet_device *rtdev_get_by_index(int ifindex);
extern struct rtnet_device *rtdev_get_by_hwaddr(unsigned short type,char *ha);
#define rtdev_reference(rtdev)      atomic_inc(&(rtdev)->refcount)
#define rtdev_dereference(rtdev)    atomic_dec(&(rtdev)->refcount)

extern int rtdev_xmit(struct rtskb *skb);

#ifdef CONFIG_RTNET_PROXY
extern int rtdev_xmit_proxy(struct rtskb *skb);
#endif

extern int rtdev_open(struct rtnet_device *rtdev);
extern int rtdev_close(struct rtnet_device *rtdev);


#endif  /* __KERNEL__ */

#endif  /* __RTDEV_H_ */
