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

#include <rtnet_dev.h>
#include <rtnet_internal.h>
#include <ipv4/arp.h>
#include <ipv4/route.h>


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

    if (!suser())
        return -EPERM;

    ret = copy_from_user(&cfg, (void *)arg, sizeof(cfg));
    if (ret != 0)
        return -EFAULT;

    rtdev = rtdev_get_by_name(cfg.if_name);
    if ( !rtdev ) {
        rt_printk("RTnet: invalid interface %s\n", cfg.if_name);
        return -ENODEV;
    }

    switch(cmd){
        case IOC_RT_IFUP:
            ret = rtdev_open(rtdev);    /* also = 0 if dev already up */
            if( ret == 0 ) {
                rt_ip_route_del(rtdev); /* cleanup routing table */

                rtdev->local_addr = cfg.ip_addr;
                rt_ip_route_add(rtdev, cfg.ip_netaddr, cfg.ip_mask);
                rt_arp_table_add(cfg.ip_addr, rtdev->dev_addr);
                rt_ip_route_add_specific(rtdev, cfg.ip_broadcast, rtdev->broadcast);
            }
            break;

        case IOC_RT_IFDOWN:
            /*
             * if rtmac is active on dev, don't shut it down....
             *
             * FIXME: if mac exists shut mac down, then device...
             */
            if( rtdev->mac_disc ) {
                rt_printk("rtnet: rtmac is active on dev %s, cannot shut down\n", rtdev->name);
                ret = -ENOTTY;
                break;
            }
            rt_ip_route_del(rtdev);
            rt_arp_table_del(rtdev->local_addr);
            rtdev_close(rtdev);
            ret = 0;
            break;

        case IOC_RT_ROUTE_SOLICIT:
            rt_arp_solicit(rtdev,cfg.ip_addr);
            ret = 0;
            break;

        case IOC_RT_ROUTE_DELETE:
            // Billa: delete an ARP & ROUTE element in the lists
            rt_arp_table_del(cfg.ip_addr);
            rt_ip_route_del_specific(rtdev,cfg.ip_addr);
            ret = 0;
            break;

        default:
            ret = -ENOTTY;
    }

    rtdev_dereference(rtdev);
    return ret;
}



static struct file_operations rtnet_fops = {
    ioctl:  rtnet_ioctl,
};



static struct miscdevice rtnet_chr_misc_dev = {
    minor:  RTNET_MINOR,
    name:   "rtnet",
    fops:   &rtnet_fops,
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
