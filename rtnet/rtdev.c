/* rtnet/rtdev.c - implement a rtnet device
 *
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
#include <linux/spinlock.h>
#include <linux/if.h>
#include <linux/netdevice.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtnet.h>
#include <rtnet_internal.h>
#include <rtmac/rtmac_disc.h>


struct rtnet_device *rtnet_devices = NULL;
rwlock_t rtnet_devices_lock = RW_LOCK_UNLOCKED;
struct rtpacket_type *rt_packets[MAX_RT_PROTOCOLS];




/***
 *  rtdev_add_pack:         add protocol (Layer 3) 
 *  @pt:                    the new protocol
 */
void rtdev_add_pack(struct rtpacket_type *pt)
{
    int hash;


    if ( pt->type!=htons(ETH_P_ALL) ) {
        hash=ntohs(pt->type) & (MAX_RT_PROTOCOLS-1);

        if ( !rt_packets[hash] ) {
            rt_packets[hash] = pt;
        }
        else {
            rt_printk("RTnet: protocol place %d is used by %s\n", hash,
                (rt_packets[hash]->name) ? (rt_packets[hash]->name) :
                    "<noname>");
        }
    }
}



/***
 *  rtdev_remove_pack:  remove protocol (Layer 3)
 *  @pt:                protocol
 */
void rtdev_remove_pack(struct rtpacket_type *pt)
{
    int hash;


    if ( (pt) && (pt->type!=htons(ETH_P_ALL)) ) {
        hash=ntohs(pt->type) & (MAX_RT_PROTOCOLS-1);
        if ( (rt_packets[hash]) && (rt_packets[hash]->type==pt->type) ) {
            rt_packets[hash]=NULL;
        }
        else {
            rt_printk("RTnet: protocol %s not found\n",
            (rt_packets[hash]->name)?(rt_packets[hash]->name):"<noname>");
        }
    }
}



/***
 *  rtdev_get_by_name   - find a rtnet_device by its name 
 *  @name: name to find
 *
 *  Find an rt-interface by name. 
 */
static inline struct rtnet_device *__rtdev_get_by_name(const char *name)
{
    struct rtnet_device *rtdev;

    for (rtdev=rtnet_devices; rtdev; rtdev=rtdev->next) {
        if ( strncmp(rtdev->name, name, IFNAMSIZ) == 0 ) {
            return rtdev;
        }
    }
    return NULL;
}


struct rtnet_device *rtdev_get_by_name(const char *name)
{
    struct rtnet_device *rtdev;

    read_lock(&rtnet_devices_lock);
    rtdev = __rtdev_get_by_name(name);
    read_unlock(&rtnet_devices_lock);
    return rtdev;
}



/***
 *  rtdev_get_by_index - find a rtnet_device by its ifindex
 *  @ifindex: index of device
 *
 *  Search for an rt-interface by index.
 */
static inline struct rtnet_device *__rtdev_get_by_index(int ifindex)
{
    struct rtnet_device *rtdev;

    for (rtdev=rtnet_devices; rtdev; rtdev=rtdev->next) {
        if ( rtdev->ifindex == ifindex ) {
            return rtdev;
        }
    }
    return NULL;
}


struct rtnet_device *rtdev_get_by_index(int ifindex)
{
    struct rtnet_device *rtdev;

    read_lock(&rtnet_devices_lock);
    rtdev = __rtdev_get_by_index(ifindex);
    read_unlock(&rtnet_devices_lock);
    return rtdev;
}


int rtdev_new_index(void)
{
    static int ifindex;
    for (;;) {
        if (++ifindex <= 0)
            ifindex = 1;
        if (__rtdev_get_by_index(ifindex) == NULL)
        return ifindex;
    }
}




/***
 *  rtdev_get_by_dev - find a rtnetdevice by its linux-netdevice
 *  @dev: linked linux-device of rtnet_device
 *
 *  Search for an rt-interface by index.
 */
static inline struct rtnet_device *__rtdev_get_by_dev(struct net_device *dev)
{
    struct rtnet_device * rtdev;

    for (rtdev=rtnet_devices; rtdev; rtdev=rtdev->next) {
        if ( rtdev->ldev==dev ) {
            return rtdev;
        }
    }
    return NULL;
}


struct rtnet_device *rtdev_get_by_dev(struct net_device *dev)
{
    struct rtnet_device * rtdev;

    read_lock(&rtnet_devices_lock);
    rtdev = __rtdev_get_by_dev(dev);
    read_unlock(&rtnet_devices_lock);
    return rtdev;
}


/***
 *  rtdev_get_by_hwaddr - find a rtnetdevice by its mac-address
 *  @type:          Type of the net_device (may be ARPHRD_ETHER)
 *  @hw_addr:       MAC-Address
 *
 *  Search for an rt-interface by index.
 *  Remeber: We are searching in the rtdev_base-list, because 
 *  we want to use linux-network's also.
 */
static inline struct rtnet_device *__rtdev_get_by_hwaddr(unsigned short type, char *hw_addr)
{
    struct rtnet_device * rtdev;

    for(rtdev=rtnet_devices;rtdev;rtdev=rtdev->next){
        if ((rtdev->type == type) &&
            (!memcmp(rtdev->dev_addr, hw_addr, rtdev->addr_len))) {
            return rtdev;
        }
    }
    return NULL;
}


struct rtnet_device *rtdev_get_by_hwaddr(unsigned short type, char *hw_addr)
{
    struct rtnet_device * rtdev;

    read_lock(&rtnet_devices_lock);
    rtdev = __rtdev_get_by_hwaddr(type, hw_addr);
    read_unlock(&rtnet_devices_lock);
    return rtdev;
}



/***
 *  rtdev_alloc_name - allocate a name for the rtnet_device
 *  @rtdev:         the rtnet_device
 *  @name_mask:     a name mask
 *                  ("eth%d" for ethernet or "tr%d" for tokenring)
 *
 *  This function have to called from the driver-probe-funtion.
 *  Because we don't know the type of the rtnet_device
 *  (may be tokenring or ethernet).
 */
void rtdev_alloc_name(struct rtnet_device *rtdev, const char *mask)
{
    char buf[IFNAMSIZ];
    int i;

    for (i=0; i < 100; i++) {
        snprintf(buf, IFNAMSIZ, mask, i);
        if (__rtdev_get_by_name(buf) == NULL) {
        strncpy(rtdev->name, buf, IFNAMSIZ);
            break;
        }
    }
}



/***
 *  rtdev_alloc
 *  @int sizeof_priv:
 *
 *  allocate memory for a new rt-network-adapter
 */
struct rtnet_device *rtdev_alloc(int sizeof_priv)
{
    struct rtnet_device *rtdev;
    int alloc_size;

    /* ensure 32-byte alignment of the private area */
    alloc_size = sizeof (*rtdev) + sizeof_priv + 31;

    rtdev = (struct rtnet_device *) kmalloc (alloc_size, GFP_KERNEL);
    if (rtdev == NULL) {
        printk(KERN_ERR "RTnet: cannot allocate rtnet device\n");
        return NULL;
    }

    memset(rtdev, 0, alloc_size);

    rt_typed_sem_init(&rtdev->xmit_sem, 1, RES_SEM);

    if (sizeof_priv)
        rtdev->priv = (void *) (((long)(rtdev + 1) + 31) & ~31);

    rt_printk("rtdev: allocated and initialized\n");

    return rtdev;
}



/***
 *  rtdev_free
 */
void rtdev_free (struct rtnet_device *rtdev)
{
    if (rtdev!=NULL) {
        rtdev->stack_sem=NULL;
        rtdev->rtdev_mbx=NULL;
        rt_sem_delete(&rtdev->xmit_sem);
        kfree(rtdev);
    }
}



/***
 *  rtdev_open
 *
 *  Prepare an interface for use.
 */
int rtdev_open(struct rtnet_device *rtdev)
{
    int ret = 0;

    if (rtdev->flags & IFF_UP)              /* Is it already up?                */
        return 0;

    if (rtdev->open)                        /* Call device private open method  */
        ret = rtdev->open(rtdev);

    if ( !ret )  {
        rtdev->flags |= (IFF_UP | IFF_RUNNING);
        set_bit(__LINK_STATE_START, &rtdev->state);
#if 0
        dev_mc_upload(dev);                 /* Initialize multicasting status   */
#endif
    }

    return ret;
}


/***
 *  rtdev_close
 */
int rtdev_close(struct rtnet_device *rtdev)
{
    int ret = 0;

    if ( !(rtdev->flags & IFF_UP) )
        return 0;

    if (rtdev->stop)
        ret = rtdev->stop(rtdev);

    rtdev->flags &= ~(IFF_UP|IFF_RUNNING);
    clear_bit(__LINK_STATE_START, &rtdev->state);

    return ret;
}



/***
 *  rtdev_xmit - send real-time packet
 */
int rtdev_xmit(struct rtskb *skb) 
{
    struct rtnet_device *rtdev;
    int ret = 0;


    ASSERT(skb != NULL, return -1;);
    ASSERT(skb->rtdev != NULL, return -1;);
    ASSERT(skb->rtdev->hard_start_xmit != NULL, return -1;);

    rtdev = skb->rtdev;

    ret = rtdev->hard_start_xmit(skb, rtdev);
    if (ret != 0)
    {
        rt_printk("xmit returned %d not 0\n",ret);
        /* if an error occured, we must free the skb here! */
        if (skb)
            kfree_rtskb(skb);
    }

    return ret;
}



/***
 *      rtdev_xmit_proxy - send rtproxy packet
 */
int rtdev_xmit_proxy(struct rtskb *skb)
{
    struct rtnet_device *rtdev;
    int ret = 0;


    ASSERT(skb != NULL, return -1;);
    ASSERT(skb->rtdev != NULL, return -1;);
    ASSERT(skb->rtdev->hard_start_xmit != NULL, return -1;);


    rtdev = skb->rtdev;

    if (rtdev->mac_disc) {
        ASSERT(rtdev->mac_disc->nrt_packet_tx != NULL, return -1;);

        ret = rtdev->mac_disc->nrt_packet_tx(skb);
    }
    else
    {
        ret = rtdev->hard_start_xmit(skb, rtdev);
        if (ret != 0)
        {
            rt_printk("xmit returned %d not 0\n",ret);
            /* if an error occured, we must free the skb here! */
            if (skb)
                kfree_rtskb(skb);
        }
    }

    return ret;
}



/***
 *  rt_net_dev_init
 */
int rtnet_dev_init(void)
{
    int i;
    for (i=0; i<MAX_RT_PROTOCOLS; i++) rt_packets[i]=NULL;
    return 0;
}


/***
 *  rtnet_dev_release
 */
int rtnet_dev_release(void)
{
    int i;
    for (i=0; i<MAX_RT_PROTOCOLS; i++) rt_packets[i]=NULL;
    return 0;
}
