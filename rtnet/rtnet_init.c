/* rtnet_init.c
 *
 * rtnet - real-time networking subsystem
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
#include <linux/if_arp.h> /* ARPHDR_ETHER */

#include <rtai.h>
#include <rtnet.h>
#include <rtnet_internal.h>



/**
 * rtalloc_etherdev - Allocates and sets up an ethernet device
 * @sizeof_priv: size of additional driver-private structure to 
 *		 be allocated for this ethernet device
 *
 * Fill in the fields of the device structure with ethernet-generic
 * values. Basically does everything except registering the device.
 *
 * Constructs a new net device, complete with a private data area of
 * size @sizeof_priv.  A 32-byte (not bit) alignment is enforced for
 * this private data area.
 */
struct rtnet_device *rt_alloc_etherdev(int sizeof_priv)
{
	struct rtnet_device *rtdev;

	rtdev = rtdev_alloc(sizeof_priv);
	if (!rtdev)
		return NULL;

	rtdev->hard_header	= rt_eth_header;

	rtdev->mtu 		= ETH_FRAME_LEN;
	rtdev->type		= ARPHRD_ETHER;
	rtdev->hard_header_len 	= ETH_HLEN;
	rtdev->mtu		= 1500; /* eth_mtu */
	rtdev->addr_len		= ETH_ALEN;
	rtdev->flags		= IFF_BROADCAST; /* TODO: IFF_MULTICAST; */
	
	memset(rtdev->broadcast, 0xFF, ETH_ALEN);
	strcpy(rtdev->name, "rteth%d");

	return rtdev;
}


#if 0
/* FIXME: port this function */
/**
 * rtalloc_etherdev - Allocates and sets up an tokenring device
 * @sizeof_priv: size of additional driver-private structure to 
 *		 be allocated for this ethernet device
 *
 * Fill in the fields of the device structure with ethernet-generic
 * values. Basically does everything except registering the device.
 *
 * Constructs a new net device, complete with a private data area of
 * size @sizeof_priv.  A 32-byte (not bit) alignment is enforced for
 * this private data area.
 */
struct rtnet_device *rt_alloc_trdev(int sizeof_priv)
{
	struct net_device *dev;
	struct rtnet_device *rtdev;
	if ( !(dev=alloc_etherdev(0)) ){
		rt_printk("RTnet: cannot allocate ethernet device\n");
	        return NULL;
	}
	if (sizeof_priv) {
		if ( !(dev->priv=kmalloc (sizeof_priv, GFP_KERNEL)) ) {
			rt_printk("RTnet: cannot allocate ethernet device\n");
			kfree(dev);
			return NULL;
		}
		memset(dev->priv, 0, sizeof_priv);
	}
	rtdev=rtdev_alloc(dev);
	rtdev->mtu=2000;
	return (rtdev);
}
#endif


/***
 *	rt_register_rtnetdev:	register a new rtnet_device (linux-like)
 *	@rtdev:			the device
 */
int rt_register_rtnetdev(struct rtnet_device *rtdev)
{
	struct rtnet_device *d, **dp;
	unsigned long flags;

	hard_save_flags_and_cli(flags);
	write_lock(&rtnet_devices_lock);
	
	/* FIXME: do we need rtdev->ifindex? */
	rtdev->ifindex = rtdev_new_index();

	if (strchr(rtdev->name,'%') != NULL)
		rtdev_alloc_name(rtdev, rtdev->name);

	/* Check for existence, and append to tail of chain */
	for (dp = &rtnet_devices; (d=*dp) != NULL; dp = &d->next) {
		if (d == rtdev || strcmp(d->name, rtdev->name) == 0) {
			write_unlock(&rtnet_devices_lock);
			hard_restore_flags(flags);
			return -EEXIST;
		}
	}

	/*
	 *	Default initial state at registry is that the
	 *	device is present.
	 */
	set_bit(__LINK_STATE_PRESENT, &rtdev->state);

	/* insert to list */
	*dp = rtdev;
	rtdev->next = NULL;

	write_unlock(&rtnet_devices_lock);
	hard_restore_flags(flags);
	rt_printk("RTnet: rt_register_netdevice(%s)\n", rtdev->name);

	return 0;
}




/***
 *	rt_unregister_rtnetdev:	unregister a rtnet_device
 *	@rtdev:			the device
 */
int rt_unregister_rtnetdev(struct rtnet_device *rtdev)
{
	struct rtnet_device *d, **dp;
	unsigned long flags;

	rt_printk("RTnet: rt_unregister_netdevice(%s)\n", rtdev->name);

	/* If device is running, close it first. */
	if (rtdev->flags & IFF_UP)
		rtdev_close(rtdev);

	hard_save_flags_and_cli(flags);
	write_lock(&rtnet_devices_lock);
	/* Unlink it from device chain. */
	for (dp = &rtnet_devices; (d=*dp)!=NULL; dp=&d->next) {
		if (d==rtdev) {
			*dp = d->next;
			break;
		}
	}
	write_unlock(&rtnet_devices_lock);
	hard_restore_flags(flags);

	if ( !d ) {
		rt_printk("RTnet: device %s/%p never was registered\n", rtdev->name, rtdev);
		return -ENODEV;
	}
	clear_bit(__LINK_STATE_PRESENT, &rtdev->state);

	return 0;
}
