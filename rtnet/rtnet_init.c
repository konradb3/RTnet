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
#include <linux/netdevice.h>
#include <linux/etherdevice.h>

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
	rtdev=rtdev_alloc(dev);		// alloc mem for rtdev, link rtdev and dev, inset rtdev into rtdev-list
	rtdev->mtu=ETH_FRAME_LEN;
	return rtdev;
}


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



/***
 *	rt_register_rtnetdev:	register a new rtnet_device (linux-like)
 *	@rtdev:			the device
 */
int rt_register_rtnetdev(struct rtnet_device *rtdev)
{
	struct net_device *d, **dp, *dev = dev_get_by_rtdev(rtdev);

	rt_printk("RTnet: rt_register_netdevice(%s)\n", dev->name);

	dev->iflink = -1;
	/* Init, if this function is available */
	if ( (dev->init) && (dev->init(dev)) ) {
		return -EIO;
	}
	dev->ifindex = dev_new_index();
	if (dev->iflink == -1)
		dev->iflink = dev->ifindex;

	/* Check for existence, and append to tail of chain */
	for (dp=&dev_base; (d=*dp) != NULL; dp=&d->next) {
		if ( (d==dev) || (!strcmp(d->name, dev->name)) ) {
			return -EEXIST;
		}
	}

	/*
	 *	Default initial state at registry is that the
	 *	device is present.
	 */
	set_bit(__LINK_STATE_PRESENT, &dev->state);
	dev->next = NULL;

	/* insert to list */
	write_lock(&dev_base_lock);
	*dp = dev;
	dev->deadbeaf = 0;
	write_unlock(&dev_base_lock);

	return 0;
}




/***
 *	rt_unregister_rtnetdev:	unregister a rtnet_device
 *	@rtdev:			the device
 */
int rt_unregister_rtnetdev(struct rtnet_device *rtdev)
{
	struct net_device *d, **dp, *dev = dev_get_by_rtdev(rtdev);

	rt_printk("RTnet: rt_unregister_netdevice(%s)\n", dev->name);

	/* If device is running, close it first. */
	if (dev->flags & IFF_UP)
		rtdev_close(rtdev);

	dev->deadbeaf = 1;

	/* And unlink it from device chain. */
	for (dp = &dev_base; (d=*dp)!=NULL; dp=&d->next) {
		if (d==dev) {
			write_lock(&dev_base_lock);
			*dp = d->next;
			write_unlock(&dev_base_lock);
			break;
		}
	}

	if ( !d ) {
		rt_printk("RTnet: device %s/%p never was registered\n", dev->name, dev);
		return -ENODEV;
	}

	clear_bit(__LINK_STATE_PRESENT, &dev->state);


	if ( dev->uninit ) 
		dev->uninit(dev);

	return 0;
}






