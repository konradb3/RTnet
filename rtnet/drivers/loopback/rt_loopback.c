/* loopback.c
 *
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 * extended by Jose Carlos Billalabeitia and Jan Kiszka
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
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/netdevice.h>

#include <rtnet_port.h>

MODULE_AUTHOR("Maintainer: Jan Kiszka <Jan.Kiszka@web.de>");
MODULE_DESCRIPTION("RTnet loopback driver");
MODULE_LICENSE("GPL");

static struct rtnet_device* rt_loopback_dev;

/***
 *  rt_loopback_open
 *  @rtdev
 */
static int rt_loopback_open (struct rtnet_device *rtdev)
{
    RTNET_MOD_INC_USE_COUNT;

    rt_stack_connect(rtdev, &STACK_manager);
    rtnetif_start_queue(rtdev);

    return 0;
}


/***
 *  rt_loopback_close
 *  @rtdev
 */
static int rt_loopback_close (struct rtnet_device *rtdev)
{
    rtnetif_stop_queue(rtdev);
    rt_stack_disconnect(rtdev);

    RTNET_MOD_DEC_USE_COUNT;

    return 0;
}


/***
 *  rt_loopback_xmit - begin packet transmission
 *  @skb: packet to be sent
 *  @dev: network device to which packet is sent
 *
 */
static int rt_loopback_xmit(struct rtskb *skb, struct rtnet_device *rtdev)
{
    unsigned short          hash;
    struct rtpacket_type    *pt;
    unsigned long           flags;
    rtos_time_t             time;


    /* write transmission stamp - in case any protocol ever gets the idea to
       ask the lookback device for this service... */
    if (skb->xmit_stamp) {
        rtos_get_time(&time);
        *skb->xmit_stamp =
            cpu_to_be64(rtos_time_to_nanosecs(&time) + *skb->xmit_stamp);
    }

    /* make sure that critical fields are re-intialised */
    skb->chain_end = skb;

    /* parse the Ethernet header as usual */
    skb->protocol = rt_eth_type_trans(skb, rtdev);
    skb->nh.raw   = skb->data;

    rtdev_reference(rtdev);

    rtcap_report_incoming(skb);

    hash = ntohs(skb->protocol) & (MAX_RT_PROTOCOLS-1);

    rtos_spin_lock_irqsave(&rt_packets_lock, flags);

    pt = rt_packets[hash];

    if ((pt != NULL) && (pt->type == skb->protocol)) {
        pt->refcount++;
        rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

        pt->handler(skb, pt);

        rtos_spin_lock_irqsave(&rt_packets_lock, flags);
        pt->refcount--;
        rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);
    } else {
        rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

        rtos_print("RTnet: unknown layer-3 protocol\n");

        kfree_rtskb(skb);
    }

    rtdev_dereference(rtdev);

    return 0;
}


/***
 *  loopback_init
 */
static int __init loopback_init(void)
{
    int err;
    struct rtnet_device *rtdev;

    printk("initializing loopback...\n");

    if ((rtdev = rt_alloc_etherdev(0)) == NULL)
        return -ENODEV;

    rt_rtdev_connect(rtdev, &RTDEV_manager);
    RTNET_SET_MODULE_OWNER(rtdev);

    strcpy(rtdev->name, "rtlo");

    rtdev->vers = RTDEV_VERS_2_0;
    rtdev->open = &rt_loopback_open;
    rtdev->stop = &rt_loopback_close;
    rtdev->hard_start_xmit = &rt_loopback_xmit;
    rtdev->flags |= IFF_LOOPBACK;
    rtdev->flags &= ~IFF_BROADCAST;
    rtdev->features |= RTNETIF_F_NON_EXCLUSIVE_XMIT;

    if ((err = rt_register_rtnetdev(rtdev)) != 0)
    {
        rtdev_free(rtdev);
        return err;
    }

    rt_loopback_dev = rtdev;

    return 0;
}


/***
 *  loopback_cleanup
 */
static void __exit loopback_cleanup(void)
{
    struct rtnet_device *rtdev = rt_loopback_dev;

    printk("removing loopback...\n");

    rt_unregister_rtnetdev(rtdev);
    rt_rtdev_disconnect(rtdev);

    rtdev_free(rtdev);
}

module_init(loopback_init);
module_exit(loopback_cleanup);
