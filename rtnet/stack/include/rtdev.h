/***
 *
 *  rtdev.h
 *
 *  RTnet - real-time networking subsystem
 *  Copyright (C) 1999       Lineo, Inc
 *                1999, 2002 David A. Schleef <ds@schleef.org>
 *                2003-2005  Jan Kiszka <jan.kiszka@web.de>
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

#ifndef __RTDEV_H_
#define __RTDEV_H_

#define MAX_RT_DEVICES                  8


#ifdef __KERNEL__

#include <asm/atomic.h>
#include <asm/semaphore.h>
#include <linux/netdevice.h>

#include <rtskb.h>


#define RTDEV_VERS_2_0                  0x0200

#define PRIV_FLAG_UP                    0
#define PRIV_FLAG_ADDING_ROUTE          1

#define RTNETIF_F_NON_EXCLUSIVE_XMIT    0x00010000


/***
 *  rtnet_device
 */
struct rtnet_device {
    /* Many field are borrowed from struct net_device in
     * <linux/netdevice.h> - WY
     */
    unsigned int        vers;

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
    __u16               __padding;

    unsigned long       state;
    int                 ifindex;
    atomic_t            refcount;

    struct module       *rt_owner;  /* like classic owner, but      *
                                     * forces correct macro usage   */

    unsigned int        flags;      /* interface flags (a la BSD)   */
    unsigned long       priv_flags; /* internal flags               */
    unsigned short      type;       /* interface hardware type      */
    unsigned short      hard_header_len;    /* hardware hdr length  */
    unsigned int        mtu;        /* eth = 1536, tr = 4...        */
    void                *priv;      /* pointer to private data      */
    int                 features;   /* [RT]NETIF_F_*                */

    /* Interface address info. */
    unsigned char       broadcast[MAX_ADDR_LEN];    /* hw bcast add */
    unsigned char       dev_addr[MAX_ADDR_LEN];     /* hw address   */
    unsigned char       addr_len;   /* hardware address length      */

    struct dev_mc_list  *mc_list;   /* Multicast mac addresses      */
    int                 mc_count;   /* Number of installed mcasts   */
    int                 promiscuity;
    int                 allmulti;

    __u32               local_ip;   /* IP address in network order  */
    __u32               broadcast_ip; /* broadcast IP in network order */

    rtdm_event_t        *stack_event;

    rtdm_mutex_t        xmit_mutex; /* protects xmit routine        */
    rtdm_lock_t         rtdev_lock; /* management lock              */
    struct semaphore    nrt_lock;   /* non-real-time locking        */

    unsigned int        add_rtskbs; /* additionally allocated global rtskbs */

    /* RTmac related fields */
    struct rtmac_disc   *mac_disc;
    struct rtmac_priv   *mac_priv;
    int                 (*mac_detach)(struct rtnet_device *rtdev);

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

    /* Transmission hook, managed by the stack core, RTcap, and RTmac
     *
     * If xmit_lock is used, start_xmit points either to rtdev_locked_xmit or
     * the RTmac discipline handler. If xmit_lock is not required, start_xmit
     * points to hard_start_xmit or the discipline handler.
     */
    int                 (*start_xmit)(struct rtskb *skb,
                                      struct rtnet_device *dev);

    /* MTU hook, managed by the stack core and RTmac */
    unsigned int        (*get_mtu)(struct rtnet_device *rtdev,
                                   unsigned int priority);

    int                 (*do_ioctl)(struct rtnet_device *rtdev, 
				    unsigned int request, void * cmd);
};


struct rtnet_core_cmd;

struct rtdev_event_hook {
    struct list_head    entry;
    void                (*register_device)(struct rtnet_device *rtdev);
    void                (*unregister_device)(struct rtnet_device *rtdev);
    void                (*ifup)(struct rtnet_device *rtdev,
                                struct rtnet_core_cmd *up_cmd);
    void                (*ifdown)(struct rtnet_device *rtdev);
};

extern struct list_head event_hook_list;
extern struct semaphore rtnet_devices_nrt_lock;


struct rtnet_device *rt_alloc_etherdev(int sizeof_priv);
void rtdev_free(struct rtnet_device *rtdev);

int rt_register_rtnetdev(struct rtnet_device *rtdev);
int rt_unregister_rtnetdev(struct rtnet_device *rtdev);

void rtdev_add_event_hook(struct rtdev_event_hook *hook);
void rtdev_del_event_hook(struct rtdev_event_hook *hook);

void rtdev_alloc_name (struct rtnet_device *rtdev, const char *name_mask);

struct rtnet_device *rtdev_get_by_name(const char *if_name);
struct rtnet_device *rtdev_get_by_index(int ifindex);
struct rtnet_device *rtdev_get_by_hwaddr(unsigned short type,char *ha);
struct rtnet_device *rtdev_get_loopback(void);
#define rtdev_reference(rtdev)      atomic_inc(&(rtdev)->refcount)
#define rtdev_dereference(rtdev)    atomic_dec(&(rtdev)->refcount)

int rtdev_xmit(struct rtskb *skb);

#ifdef CONFIG_RTNET_ADDON_PROXY
int rtdev_xmit_proxy(struct rtskb *skb);
#endif

unsigned int rt_hard_mtu(struct rtnet_device *rtdev, unsigned int priority);

int rtdev_open(struct rtnet_device *rtdev);
int rtdev_close(struct rtnet_device *rtdev);


#endif  /* __KERNEL__ */

#endif  /* __RTDEV_H_ */
