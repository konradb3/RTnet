/*
 * rtnet_dev.c - implement char device for user space communication
 * Copyright (C) 1999    Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002, Ulrich Marx <marx@fet.uni-hannover.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/kmod.h>
#include <linux/miscdevice.h>
#include <linux/netdevice.h>

#include <rtnet.h>
#include <rtnet_internal.h>


/**
 * rtnet_ioctl -
 * @inode:
 * @file:
 * @cmd:
 * @arg:
 */
static int rtnet_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rtnet_config cfg;
	struct rtnet_device *rtdev;
	int ret;

	if (!suser())return -EPERM;

	ret = copy_from_user(&cfg, (void *)arg, sizeof(cfg));
	if (ret) return -EFAULT;

	rtdev = rtdev_get_by_name(cfg.if_name);
	if ( !rtdev ) {
		rt_printk("RTnet: invalid interface %s\n", cfg.if_name);
		return -ENODEV;
	}
	switch(cmd){
	case IOC_RT_IFUP:
		ret = rtdev_open(rtdev);				// = 0, if dev already up
		/*
		 * if device already up and ip changes, delete routing table and add new route
		 * if the dev changes state from close->open also add new route
		 *
		 * pretty ugly, isn't it ;)
		 *
		 */
		if( ret == 0 ) {
			if( rtdev->local_addr != cfg.ip_addr ) {
				rt_ip_route_del(rtdev);

				rtdev->local_addr = cfg.ip_addr;
				rt_ip_route_add(rtdev, cfg.ip_netaddr, cfg.ip_mask);
				rt_arp_table_add(cfg.ip_addr, rtdev->dev_addr);
				rt_ip_route_add_specific(rtdev, cfg.ip_broadcast, rtdev->broadcast);
			}
		} else {
			rtdev->local_addr = cfg.ip_addr;
			rt_ip_route_add(rtdev, cfg.ip_netaddr, cfg.ip_mask);
			rt_arp_table_add(cfg.ip_addr, rtdev->dev_addr);
			rt_ip_route_add_specific(rtdev, cfg.ip_broadcast, rtdev->broadcast);

		}
		return 0;
		
	case IOC_RT_IFDOWN:
		/*
		 * if rtmac is active on dev, don't shut it down....
		 *
		 * FIXME: if mac exists shut mac down, then device...
		 */
		if( rtdev->rtmac ) {
			rt_printk("rtnet: rtmac is active on dev %s, cannot shut down\n", rtdev->name);
			return -ENOTTY;
		}
		rt_ip_route_del(rtdev);
		rtdev_close(rtdev);
		return 0;
		
	case IOC_RT_ROUTE_SOLICIT:
		rt_arp_solicit(rtdev,cfg.ip_addr);
		return 0;
		
	default:
		return -ENOTTY;
	}
}



static struct file_operations rtnet_fops = {
	ioctl:	rtnet_ioctl,
};



static struct miscdevice rtnet_chr_misc_dev = {
	minor:		RTNET_MINOR,
	name:		"rtnet",
	fops:		&rtnet_fops,
};



/**
 * rtnet_chrdev_init - 
 *
 */
void rtnet_chrdev_init(void)
{
	if ( misc_register(&rtnet_chr_misc_dev) < 0 ) {
		rt_printk("RTnet: unable to register rtnet char misc device\n");
	}
}



/**
 * rtnet_chrdev_release - 
 *
 */
void rtnet_chrdev_release(void)
{
	misc_deregister(&rtnet_chr_misc_dev);
}










