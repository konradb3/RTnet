/* loopback.c
 *
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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

#include <linux/netdevice.h>
#include <linux/if_arp.h>

#include <crc32.h>
#include <rtnet.h>
#include <rtnet_internal.h>


/***
 *	rt_loopback_open 
 *	@rtdev
 */
static int rt_loopback_open (struct rtnet_device *rtdev) 
{
	
	return 0;
}



/***
 *	rt_loopback_close 
 *	@rtdev
 */
static int rt_loopback_close (struct rtnet_device *rtdev) 
{
	return 0;
}



/***
 * rt_loopback_xmit - begin packet transmission
 * @skb: packet to be sent
 * @dev: network device to which packet is sent
 *
 */
static int rt_loopback_xmit(struct rtskb *skb, struct rtnet_device *rtdev)
{
	int err=0;
	struct rtskb *new_skb;
	
	if ( (new_skb=dev_alloc_rtskb(skb->len))==NULL ) {
		rt_printk("RTnet %s: couldn't allocate a rtskb of size %d.\n", rtdev->name, skb->len);
		return -ENOMEM;
	}
	else {
		rtskb_reserve(skb,2);
		new_skb->rtdev = rtdev;
		memcpy(new_skb->buf_start, skb->buf_start, SKB_DATA_ALIGN(ETH_FRAME_LEN));
		rtskb_put(skb, skb->len);
		rtnetif_rx(skb);
		//dev->last_rx = jiffies;
	}
	
	return err;
}


/***
 *	loopback_init
 */
static int __init loopback_init(void) {
	int err=0;
	struct rtnet_device *rtdev;

	rt_printk("initializing loopback...\n");
	
	if ( (rtdev = rtdev_alloc(0))==NULL )
		return -ENODEV;

	/* We can make mtu larger, but it can cause a problem in dev_alloc_rtskb()
	   called by rt_loopback_xmit(). rtskb's have been preallocated at module
	   load time and the maximum size is 1500. */
	rtdev->mtu 		= ETH_FRAME_LEN;
	rtdev->type		= ARPHRD_LOOPBACK;
	rtdev->hard_header_len 	= 0;
	rtdev->addr_len		= 0;
	
	strcpy(rtdev->name, "lo");

	rtdev->open = &rt_loopback_open;
	rtdev->stop = &rt_loopback_close;
	rtdev->hard_header = rt_eth_header;
	rtdev->hard_start_xmit = &rt_loopback_xmit;

	return err;
}



/***
 *	loopback_cleanup
 */
static void __exit loopback_cleanup(void) {
	struct net_device *dev = &loopback_dev;
	struct rtnet_device *rtdev = rtdev_get_by_dev(dev);

	rt_printk("removing loopback...\n");
	if ( !dev || !rtdev ) 
		rt_printk("no loopback device\n"); 

	kfree(rtdev);
}

module_init(loopback_init);
module_exit(loopback_cleanup);













