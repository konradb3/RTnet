/* rtnet/rtdev.c - implement a rtnet device
 *
 * Copyright (C) 1999      Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002      Ulrich Marx <marx@kammer.uni-hannover.de>
 *               2003      Jan Kiszka <jan.kiszka@web.de>
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
#include <linux/if_arp.h> /* ARPHRD_ETHER */
#include <linux/netdevice.h>
#include <linux/module.h>

#include <rtnet_internal.h>
#include <rtskb.h>
#include <ethernet/eth.h>
#include <rtmac/rtmac_disc.h>


static unsigned int device_rtskbs = DEFAULT_DEVICE_RTSKBS;
MODULE_PARM(device_rtskbs, "i");
MODULE_PARM_DESC(device_rtskbs, "Number of additional global realtime socket "
                 "buffers per network adapter");

static struct rtnet_device  *rtnet_devices[MAX_RT_DEVICES];
static rtos_spinlock_t      rtnet_devices_rt_lock  = RTOS_SPIN_LOCK_UNLOCKED;
static spinlock_t           rtnet_devices_nrt_lock = SPIN_LOCK_UNLOCKED;



/***
 *  __rtdev_get_by_name - find a rtnet_device by its name
 *  @name: name to find
 */
static inline struct rtnet_device *__rtdev_get_by_name(const char *name)
{
    int i;
    struct rtnet_device *rtdev;


    for (i = 0; i < MAX_RT_DEVICES; i++) {
        rtdev = rtnet_devices[i];
        if ((rtdev != NULL) && (strncmp(rtdev->name, name, IFNAMSIZ) == 0))
            return rtdev;
    }
    return NULL;
}


/***
 *  rtdev_get_by_name - find and lock a rtnet_device by its name
 *  @name: name to find
 */
struct rtnet_device *rtdev_get_by_name(const char *name)
{
    struct rtnet_device *rtdev;
    unsigned long flags;


    rtos_spin_lock_irqsave(&rtnet_devices_rt_lock, flags);

    rtdev = __rtdev_get_by_name(name);
    if (rtdev != NULL)
        atomic_inc(&rtdev->refcount);

    rtos_spin_unlock_irqrestore(&rtnet_devices_rt_lock, flags);

    return rtdev;
}



/***
 *  __rtdev_get_by_index - find a rtnet_device by its ifindex
 *  @ifindex: index of device
 */
static inline struct rtnet_device *__rtdev_get_by_index(int ifindex)
{
    return rtnet_devices[ifindex-1];
}



/***
 *  rtdev_get_by_index - find and lock a rtnet_device by its ifindex
 *  @ifindex: index of device
 */
struct rtnet_device *rtdev_get_by_index(int ifindex)
{
    struct rtnet_device *rtdev;
    unsigned long flags;


    if ((ifindex <= 0) || (ifindex > MAX_RT_DEVICES))
        return NULL;

    rtos_spin_lock_irqsave(&rtnet_devices_rt_lock, flags);

    rtdev = __rtdev_get_by_index(ifindex);
    if (rtdev != NULL)
        atomic_inc(&rtdev->refcount);

    rtos_spin_unlock_irqrestore(&rtnet_devices_rt_lock, flags);

    return rtdev;
}



/***
 *  __rtdev_get_by_hwaddr - find a rtnetdevice by its mac-address
 *  @type:          Type of the net_device (may be ARPHRD_ETHER)
 *  @hw_addr:       MAC-Address
 */
static inline struct rtnet_device *__rtdev_get_by_hwaddr(unsigned short type, char *hw_addr)
{
    int i;
    struct rtnet_device *rtdev;


    for (i = 0; i < MAX_RT_DEVICES; i++) {
        rtdev = rtnet_devices[i];
        if ((rtdev != NULL) && (rtdev->type == type) &&
            (!memcmp(rtdev->dev_addr, hw_addr, rtdev->addr_len))) {
            return rtdev;
        }
    }
    return NULL;
}



/***
 *  rtdev_get_by_hwaddr - find and lock a rtnetdevice by its mac-address
 *  @type:          Type of the net_device (may be ARPHRD_ETHER)
 *  @hw_addr:       MAC-Address
 */
struct rtnet_device *rtdev_get_by_hwaddr(unsigned short type, char *hw_addr)
{
    struct rtnet_device * rtdev;
    unsigned long flags;


    rtos_spin_lock_irqsave(&rtnet_devices_rt_lock, flags);

    rtdev = __rtdev_get_by_hwaddr(type, hw_addr);
    if (rtdev != NULL)
        atomic_inc(&rtdev->refcount);

    rtos_spin_unlock_irqrestore(&rtnet_devices_rt_lock, flags);

    return rtdev;
}



/***
 *  rtdev_alloc_name - allocate a name for the rtnet_device
 *  @rtdev:         the rtnet_device
 *  @name_mask:     a name mask (e.g. "rteth%d" for ethernet)
 *
 *  This function have to be called from the driver probe function.
 */
void rtdev_alloc_name(struct rtnet_device *rtdev, const char *mask)
{
    char buf[IFNAMSIZ];
    int i;
    struct rtnet_device *tmp;

    for (i = 0; i < MAX_RT_DEVICES; i++) {
        snprintf(buf, IFNAMSIZ, mask, i);
        if ((tmp = rtdev_get_by_name(buf)) == NULL) {
            strncpy(rtdev->name, buf, IFNAMSIZ);
            break;
        }
        else
            rtdev_dereference(tmp);
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

    rtdev = (struct rtnet_device *)kmalloc(alloc_size, GFP_KERNEL);
    if (rtdev == NULL) {
        printk(KERN_ERR "RTnet: cannot allocate rtnet device\n");
        return NULL;
    }

    memset(rtdev, 0, alloc_size);

    rtos_res_lock_init(&rtdev->xmit_lock);

    atomic_set(&rtdev->refcount, 0);

    /* scale global rtskb pool */
    rtdev->add_rtskbs = rtskb_pool_extend(&global_pool, device_rtskbs);

    if (sizeof_priv)
        rtdev->priv = (void *)(((long)(rtdev + 1) + 31) & ~31);

    return rtdev;
}



/***
 *  rtdev_free
 */
void rtdev_free (struct rtnet_device *rtdev)
{
    if (rtdev != NULL) {
        rtskb_pool_shrink(&global_pool, rtdev->add_rtskbs);
        rtdev->stack_event = NULL;
/*        rtdev->rtdev_mbx = NULL;*/
        rtos_res_lock_delete(&rtdev->xmit_lock);
        kfree(rtdev);
    }
}



/**
 * rtalloc_etherdev - Allocates and sets up an ethernet device
 * @sizeof_priv: size of additional driver-private structure to
 *               be allocated for this ethernet device
 *
 * Fill in the fields of the device structure with ethernet-generic
 * values. Basically does everything except registering the device.
 *
 * A 32-byte alignment is enforced for the private data area.
 */
struct rtnet_device *rt_alloc_etherdev(int sizeof_priv)
{
    struct rtnet_device *rtdev;

    rtdev = rtdev_alloc(sizeof_priv);
    if (!rtdev)
        return NULL;

    rtdev->hard_header     = rt_eth_header;
    rtdev->type            = ARPHRD_ETHER;
    rtdev->hard_header_len = ETH_HLEN;
    rtdev->mtu             = 1500; /* eth_mtu */
    rtdev->addr_len        = ETH_ALEN;
    rtdev->flags           = IFF_BROADCAST; /* TODO: IFF_MULTICAST; */

    memset(rtdev->broadcast, 0xFF, ETH_ALEN);
    strcpy(rtdev->name, "rteth%d");

    return rtdev;
}



static inline int __rtdev_new_index(void)
{
    int i;

    for (i = 0; i < MAX_RT_DEVICES; i++)
        if (rtnet_devices[i] == NULL)
             return i+1;

    return -ENOMEM;
}



/***
 * rt_register_rtnetdev: register a new rtnet_device (linux-like)
 * @rtdev:               the device
 */
int rt_register_rtnetdev(struct rtnet_device *rtdev)
{
    unsigned long flags_nrt, flags_rt;

    spin_lock_irqsave(&rtnet_devices_nrt_lock, flags_nrt);

    rtdev->ifindex = __rtdev_new_index();

    if (strchr(rtdev->name,'%') != NULL)
        rtdev_alloc_name(rtdev, rtdev->name);

    if (__rtdev_get_by_name(rtdev->name) != NULL) {
        spin_unlock_irqrestore(&rtnet_devices_nrt_lock, flags_nrt);
        return -EEXIST;
    }

    rtos_spin_lock_irqsave(&rtnet_devices_rt_lock, flags_rt);

    rtnet_devices[rtdev->ifindex-1] = rtdev;

    rtos_spin_unlock_irqrestore(&rtnet_devices_rt_lock, flags_rt);

    /* Default state at registration is that the device is present. */
    set_bit(__LINK_STATE_PRESENT, &rtdev->state);

    spin_unlock_irqrestore(&rtnet_devices_nrt_lock, flags_nrt);

    printk("RTnet: registered %s\n", rtdev->name);

    return 0;
}



/***
 * rt_unregister_rtnetdev: unregister a rtnet_device
 * @rtdev:                 the device
 */
int rt_unregister_rtnetdev(struct rtnet_device *rtdev)
{
    unsigned long flags_nrt, flags_rt;


    RTNET_ASSERT(rtdev->ifindex != 0,
        rtos_print("RTnet: device %s/%p was not registered\n",
                   rtdev->name, rtdev);
        return -ENODEV;);

    /* If device is running, close it first. */
    if (rtdev->flags & IFF_UP)
        rtdev_close(rtdev);

    spin_lock_irqsave(&rtnet_devices_nrt_lock, flags_nrt);

    rtos_spin_lock_irqsave(&rtnet_devices_rt_lock, flags_rt);

    if (atomic_read(&rtdev->refcount) > 0) {
        rtos_spin_unlock_irqrestore(&rtnet_devices_rt_lock, flags_rt);
        spin_unlock_irqrestore(&rtnet_devices_nrt_lock, flags_nrt);

        printk("RTnet: unable to unregister %s - refcount = %d\n", rtdev->name,
               atomic_read(&rtdev->refcount));

        return -EBUSY;
    }
    RTNET_ASSERT(atomic_read(&rtdev->refcount) == 0,
           rtos_print("RTnet: rtdev reference counter < 0!\n"););

    rtnet_devices[rtdev->ifindex-1] = NULL;

    rtos_spin_unlock_irqrestore(&rtnet_devices_rt_lock, flags_rt);

    clear_bit(__LINK_STATE_PRESENT, &rtdev->state);

    spin_unlock_irqrestore(&rtnet_devices_nrt_lock, flags_nrt);

    printk("RTnet: unregistered %s\n", rtdev->name);

    return 0;
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


    RTNET_ASSERT(skb != NULL, return -1;);
    RTNET_ASSERT(skb->rtdev != NULL, return -1;);
    RTNET_ASSERT(skb->rtdev->hard_start_xmit != NULL, return -1;);

    rtdev = skb->rtdev;

    ret = rtdev->hard_start_xmit(skb, rtdev);
    if (ret != 0)
    {
        rtos_print("hard_start_xmit returned %d\n", ret);
        /* if an error occured, we must free the skb here! */
        if (skb)
            kfree_rtskb(skb);
    }

    return ret;
}



#ifdef CONFIG_RTNET_PROXY
/***
 *      rtdev_xmit_proxy - send rtproxy packet
 */
int rtdev_xmit_proxy(struct rtskb *skb)
{
    struct rtnet_device *rtdev;
    int ret = 0;


    RTNET_ASSERT(skb != NULL, return -1;);
    RTNET_ASSERT(skb->rtdev != NULL, return -1;);
    RTNET_ASSERT(skb->rtdev->hard_start_xmit != NULL, return -1;);


    rtdev = skb->rtdev;

    /* TODO: make these lines race-condition-safe */
    if (rtdev->mac_disc) {
        RTNET_ASSERT(rtdev->mac_disc->nrt_packet_tx != NULL, return -1;);

        ret = rtdev->mac_disc->nrt_packet_tx(skb);
    }
    else
    {
        ret = rtdev->hard_start_xmit(skb, rtdev);
        if (ret != 0)
        {
            rtos_print("hard_start_xmit returned %d\n", ret);
            /* if an error occured, we must free the skb here! */
            if (skb)
                kfree_rtskb(skb);
        }
    }

    return ret;
}
#endif /* CONFIG_RTNET_PROXY */
