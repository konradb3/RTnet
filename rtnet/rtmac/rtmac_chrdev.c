/* rtmac_chrdev.c
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

#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>

#include <rtdev.h>
#include <rtmac/rtmac.h>
#include <rtmac/rtmac_chrdev.h>


static int rtmac_chrdev_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	struct rtmac_config cfg;
	struct rtnet_device *rtdev;

	if( !suser() )
		return -EPERM;

	if( copy_from_user(&cfg, (void*)arg, sizeof(struct rtmac_config)) )
		return -EFAULT;

	rtdev = rtdev_get_by_name(cfg.if_name);

	if( !rtdev ) {
		rt_printk("RTmac: invalid interface %s\n", cfg.if_name);
		return -ENODEV;
	}

	if( !(rtdev->rtmac && rtdev->rtmac->ioctl_ops) ) {
		return -ENOTTY;
	}

	switch( cmd ) {
	case RTMAC_IOC_CLIENT:
		return rtmac_ioctl_client(rtdev);
		break;
	case RTMAC_IOC_MASTER:
		return rtmac_ioctl_master(rtdev, cfg.cycle, cfg.mtu);
		break;
	case RTMAC_IOC_UP:
		return rtmac_ioctl_up(rtdev);
		break;
	case RTMAC_IOC_DOWN:
		return rtmac_ioctl_down(rtdev);
		break;
	case RTMAC_IOC_ADD:
		return rtmac_ioctl_add(rtdev, cfg.ip_addr);
		break;
	case RTMAC_IOC_REMOVE:
		return rtmac_ioctl_remove(rtdev, cfg.ip_addr);
		break;
	case RTMAC_IOC_ADD_NRT:
		return rtmac_ioctl_add_nrt(rtdev, cfg.ip_addr);
		break;
	case RTMAC_IOC_REMOVE_NRT:
		return rtmac_ioctl_remove_nrt(rtdev, cfg.ip_addr);
		break;
	case RTMAC_IOC_CYCLE:
		return rtmac_ioctl_cycle(rtdev, cfg.cycle);
		break;
	case RTMAC_IOC_MTU:
		return rtmac_ioctl_mtu(rtdev, cfg.mtu);
		break;
	case RTMAC_IOC_OFFSET:
		return rtmac_ioctl_offset(rtdev, cfg.ip_addr, cfg.offset);
	default:
		return -ENOTTY;
	}
}



static struct file_operations rtmac_chrdev_fops = {
	owner:	THIS_MODULE,
	ioctl:	rtmac_chrdev_ioctl,

};

static struct miscdevice rtmac_chrdev_misc = {
	minor:	RTMAC_MINOR,
	name:	"rtmac",
	fops:	&rtmac_chrdev_fops,
};



int rtmac_chrdev_init(void)
{
	int ret;

	ret = misc_register(&rtmac_chrdev_misc);
	if ( ret < 0 ) {
		rt_printk("RTmac: unable to register rtmac char misc device\n");	// FIXME: print major & minor number?
	}

	return ret;
}

void rtmac_chrdev_release(void)
{
	int ret;

	ret = misc_deregister(&rtmac_chrdev_misc);

	if (ret < 0) {
		rt_printk("RTmac: unregisterung rtmac char misc device causes an error!\n");
	}
}





int rtmac_ioctl_client(struct rtnet_device *rtdev)
{
	if( !(rtdev->rtmac->ioctl_ops->client) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->client(rtdev);
}



int rtmac_ioctl_master(struct rtnet_device *rtdev, unsigned int cycle, unsigned int mtu)
{
	if( !(rtdev->rtmac->ioctl_ops->master) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->master(rtdev, cycle, mtu);
}



int rtmac_ioctl_up(struct rtnet_device *rtdev)
{
	if( !(rtdev->rtmac->ioctl_ops->up) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->up(rtdev);
}



int rtmac_ioctl_down(struct rtnet_device *rtdev)
{
	if( !(rtdev->rtmac->ioctl_ops->down) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->down(rtdev);
}



int rtmac_ioctl_add(struct rtnet_device *rtdev, u32 ip_addr)
{
	if( !(rtdev->rtmac->ioctl_ops->add) )
		return -ENOTTY;
	
	return rtdev->rtmac->ioctl_ops->add(rtdev, ip_addr);
}



int rtmac_ioctl_remove(struct rtnet_device *rtdev, u32 ip_addr)
{
	if( !(rtdev->rtmac->ioctl_ops->remove) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->remove(rtdev, ip_addr);
}



int rtmac_ioctl_add_nrt(struct rtnet_device *rtdev, u32 ip_addr)
{
	if( !(rtdev->rtmac->ioctl_ops->add_nrt) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->add_nrt(rtdev, ip_addr);
}



int rtmac_ioctl_remove_nrt(struct rtnet_device *rtdev, u32 ip_addr)
{
	if( !(rtdev->rtmac->ioctl_ops->remove_nrt) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->remove_nrt(rtdev, ip_addr);
}



int rtmac_ioctl_cycle(struct rtnet_device *rtdev, unsigned int cycle)
{
	if( !(rtdev->rtmac->ioctl_ops->cycle) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->cycle(rtdev, cycle);
}


int rtmac_ioctl_mtu(struct rtnet_device *rtdev, unsigned int mtu)
{
	if( !(rtdev->rtmac->ioctl_ops->mtu) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->mtu(rtdev, mtu);
}

int rtmac_ioctl_offset(struct rtnet_device *rtdev, u32 ip_addr, unsigned int offset)
{
	if( !(rtdev->rtmac->ioctl_ops->offset) )
		return -ENOTTY;

	return rtdev->rtmac->ioctl_ops->offset(rtdev, ip_addr, offset);
}

//EOF
