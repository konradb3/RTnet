/* rtmac_disc.c
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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

#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/netdevice.h>

#include <rtai.h>

#include <rtmac/rtmac_disc.h>


struct rtpacket_type rtmac_packet_type;

/***
 *	rtmac_disc_init
 *
 * @rtdev	init discipline for device
 * @disc	discipline to init
 *
 * 0		success
 * -EINVAL	called with rtdev=NULL or not complete discipline
 * -ENODEV	discipline unknown
 * -EBUSY	other discipline active
 * -ENOMEM	could not allocate memory
 *
 */
int rtmac_disc_init(struct rtnet_device *rtdev, struct rtmac_disc_type *disc)
{
	int ret;
	struct rtmac_device *rtmac;
	
	memset(&rtmac_packet_type, 0, sizeof(struct rtpacket_type));
	rtmac_packet_type.name = "RTmac";
	rtmac_packet_type.type =__constant_htons(ETH_RTMAC);
	rtmac_packet_type.dev = NULL;
	rtmac_packet_type.handler = disc->packet_rx;
	rtmac_packet_type.private = (void *)1;

	if (!rtdev) {
		rt_printk("RTmac: %s called with rtdev=NULL\n",__FUNCTION__);
		return -EINVAL;
	}


	if (rtdev->rtmac) {
		rt_printk("RTmac: another discipline for rtdev '%s' active.\n", rtdev->name);
		return -EBUSY;
	}


	if (!(disc && disc->disc_ops)) {			//FIXME: zusammenfassen in fkt rtmac_disc_ops_check()?
		rt_printk("RTmac: discipline not complete\n");
		return -EINVAL;
	}

	// alloc memory
	rtmac = kmalloc(sizeof(struct rtmac_device), GFP_KERNEL);
	if (NULL == rtmac) {
		rt_printk("RTmac: kmalloc returned NULL for rtmac!\n");
		return -ENOMEM;
	}
	rtdev->rtmac = rtmac;
	rtmac->rtdev = rtdev;

	/* insert pointer to tx functions in disc_type */
	rtmac->disc_type = disc;

	// install rx_rtmac fkt and *_ops
	rtmac->disc_ops = disc->disc_ops;
	rtmac->ioctl_ops = disc->ioctl_ops;

	// call init function of discipline
	ret = disc->disc_ops->init(rtdev);

	/*
	 * install our layer 3 packet type
	 */
	rtdev_add_pack(&rtmac_packet_type);

	return ret;
}


/***
 *	rtmac_disc_release
 *
 * @rtdev	release discipline on device
 *
 * 0		success
 * -1		discipline has no release function
 * -EINVAL	called with rtdev=NULL
 * -ENODEV	no discipline active on dev
 */
int rtmac_disc_release(struct rtnet_device *rtdev)
{
	int ret;

	if( !rtdev ) {
		rt_printk("RTmac: %s() called with rtdev=NULL\n",__FUNCTION__);
		return -EINVAL;
	}

	if( !(rtdev->rtmac) ) {
		rt_printk("RTmac: no discipline active on rtdev '%s'\n", rtdev->name);
		return -ENODEV;
	}

	if( !(rtdev->rtmac->disc_ops->release) ) {
		rt_printk("RTmac: fatal error: disc has no release function!\n");
		return -1;	// FIXME: find apropriate errno
	}

	/*
	 * remove packet type from stack manager
	 */
	rtdev_remove_pack(&rtmac_packet_type);
	
	// call release function of discipline
	ret = rtdev->rtmac->disc_ops->release(rtdev);
	
	// restore drivers packet_tx routine
	rtdev->rtmac->disc_type = NULL;
	
	// free memory, and remove pointer from rtdev
	kfree(rtdev->rtmac);
	rtdev->rtmac = NULL;

	return ret;
}
