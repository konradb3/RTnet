/*
 * rtnet_chrdev.c - implement char device for user space communication
 * Copyright (C) 1999    Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002, Ulrich Marx <marx@fet.uni-hannover.de>
 *               2003, Jan Kiszka <jan.kiszka@web.de>
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
#include <linux/spinlock.h>

#include <rtnet_chrdev.h>
#include <rtnet_internal.h>
#include <rtnet_rtpc.h>
#include <ipv4/arp.h>
#include <ipv4/route.h>


struct route_solicit_params {
    struct rtnet_device *rtdev;
    __u32               ip_addr;
};


static spinlock_t ioctl_handler_lock = SPIN_LOCK_UNLOCKED;

LIST_HEAD(ioctl_handlers);


/**
 * rtnet_ioctl -
 * @inode:
 * @file:
 * @request:
 * @arg:
 */
static int rtnet_ioctl(struct inode *inode, struct file *file,
                       unsigned int request, unsigned long arg)
{
    struct rtnet_ioctl_head head;
    struct rtnet_device     *rtdev;
    struct rtnet_ioctls     *ioctls;
    unsigned long           flags;
    struct list_head        *entry;
    int                     ret;


    if (!suser())
        return -EPERM;

    ret = copy_from_user(&head, (void *)arg, sizeof(head));
    if (ret != 0)
        return -EFAULT;

    rtdev = rtdev_get_by_name(head.if_name);
    if (!rtdev)
        return -ENODEV;

    spin_lock_irqsave(&ioctl_handler_lock, flags);

    list_for_each(entry, &ioctl_handlers) {
        ioctls = list_entry(entry, struct rtnet_ioctls, entry);

        if (ioctls->ioctl_type == _IOC_TYPE(request)) {
            atomic_inc(&ioctls->ref_count);

            spin_unlock_irqrestore(&ioctl_handler_lock, flags);

            ret = ioctls->handler(rtdev, request, arg);

            rtdev_dereference(rtdev);
            atomic_dec(&ioctls->ref_count);

            return ret;
        }
    }

    spin_unlock_irqrestore(&ioctl_handler_lock, flags);

    rtdev_dereference(rtdev);
    return -ENOTTY;
}



static int route_solicit_handler(struct rt_proc_call *call)
{
    struct route_solicit_params *param;


    param = rtpc_get_priv(call, struct route_solicit_params);
    rt_arp_solicit(param->rtdev, param->ip_addr);

    return 0;
}



static int rtnet_core_ioctl(struct rtnet_device *rtdev, unsigned int request,
                            unsigned long arg)
{
    struct rtnet_core_cfg       cfg;
    struct route_solicit_params params;
    int                         ret;


    ret = copy_from_user(&cfg, (void *)arg, sizeof(cfg));
    if (ret != 0)
        return -EFAULT;

    switch (request) {
        case IOC_RT_IFUP:
            rtdev->flags |= cfg.set_dev_flags;
            rtdev->flags &= ~cfg.clear_dev_flags;
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
                rtos_print("rtnet: rtmac is active on dev %s, "
                           "cannot shut down\n", rtdev->name);
                ret = -ENOTTY;
                break;
            }
            rt_ip_route_del(rtdev);
            rt_arp_table_del(rtdev->local_addr);
            rtdev_close(rtdev);
            ret = 0;
            break;

        case IOC_RT_ROUTE_SOLICIT:
            params.rtdev   = rtdev;
            params.ip_addr = cfg.ip_addr;
            ret = rtpc_dispatch_call(route_solicit_handler, 0, &params,
                                     sizeof(params), NULL, NULL);
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

    return ret;
}



int rtnet_register_ioctls(struct rtnet_ioctls *ioctls)
{
    unsigned long       flags;
    struct list_head    *entry;
    struct rtnet_ioctls *registered_ioctls;


    RTNET_ASSERT(ioctls->handler != NULL, return -EINVAL;);

    spin_lock_irqsave(&ioctl_handler_lock, flags);

    list_for_each(entry, &ioctl_handlers) {
        registered_ioctls = list_entry(entry, struct rtnet_ioctls, entry);
        if (registered_ioctls->ioctl_type == ioctls->ioctl_type) {
            spin_unlock_irqrestore(&ioctl_handler_lock, flags);
            return -EEXIST;
        }
    }

    list_add_tail(&ioctls->entry, &ioctl_handlers);
    atomic_set(&ioctls->ref_count, 0);

    spin_unlock_irqrestore(&ioctl_handler_lock, flags);

    return 0;
}



void rtnet_unregister_ioctls(struct rtnet_ioctls *ioctls)
{
    unsigned long       flags;


    spin_lock_irqsave(&ioctl_handler_lock, flags);

    while (atomic_read(&ioctls->ref_count) != 0) {
        spin_unlock_irqrestore(&ioctl_handler_lock, flags);

        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */

        spin_lock_irqsave(&ioctl_handler_lock, flags);
    }

    list_del(&ioctls->entry);

    spin_unlock_irqrestore(&ioctl_handler_lock, flags);
}



static struct file_operations rtnet_fops = {
    ioctl:  rtnet_ioctl,
};

static struct miscdevice rtnet_chr_misc_dev = {
    minor:  RTNET_MINOR,
    name:   "RTnet",
    fops:   &rtnet_fops,
};

static struct rtnet_ioctls core_ioctls = {
    service_name:   "RTnet Core",
    ioctl_type:     RTNET_IOC_TYPE_CORE,
    handler:        rtnet_core_ioctl
};



/**
 * rtnet_chrdev_init -
 *
 */
int __init rtnet_chrdev_init(void)
{
    int ret = misc_register(&rtnet_chr_misc_dev);

    if (ret < 0)
        printk("RTnet: unable to register rtnet character device "
               "(error %d)\n", ret);

    rtnet_register_ioctls(&core_ioctls);

    return ret;
}



/**
 * rtnet_chrdev_release -
 *
 */
void rtnet_chrdev_release(void)
{
    misc_deregister(&rtnet_chr_misc_dev);
}
