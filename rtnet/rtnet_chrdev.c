/***
 *
 *  rtnet_chrdev.c - implements char device for management interface
 *  Copyright (C) 1999    Lineo, Inc
 *                1999,2002 David A. Schleef <ds@schleef.org>
 *                2002 Ulrich Marx <marx@fet.uni-hannover.de>
 *                2003, 2004 Jan Kiszka <jan.kiszka@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
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


static rwlock_t ioctl_handler_lock = RW_LOCK_UNLOCKED;

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
    struct rtnet_device     *rtdev = NULL;
    struct rtnet_ioctls     *ioctls;
    struct list_head        *entry;
    int                     ret;


    if (!suser())
        return -EPERM;

    ret = copy_from_user(&head, (void *)arg, sizeof(head));
    if (ret != 0)
        return -EFAULT;

    read_lock_bh(&ioctl_handler_lock);

    list_for_each(entry, &ioctl_handlers) {
        ioctls = list_entry(entry, struct rtnet_ioctls, entry);

        if (ioctls->ioctl_type == _IOC_TYPE(request)) {
            atomic_inc(&ioctls->ref_count);

            read_unlock_bh(&ioctl_handler_lock);

            if ((_IOC_NR(request) & RTNET_IOC_NODEV_PARAM) == 0) {
                rtdev = rtdev_get_by_name(head.if_name);
                if (!rtdev)
                    return -ENODEV;
            }

            ret = ioctls->handler(rtdev, request, arg);

            if (rtdev)
                rtdev_dereference(rtdev);
            atomic_dec(&ioctls->ref_count);

            return ret;
        }
    }

    read_unlock_bh(&ioctl_handler_lock);

    return -ENOTTY;
}



static int route_solicit_handler(struct rt_proc_call *call)
{
    struct route_solicit_params *param;
    struct rtnet_device         *rtdev;


    param = rtpc_get_priv(call, struct route_solicit_params);
    rtdev = param->rtdev;

    if ((rtdev->flags & IFF_UP) == 0)
        return -ENODEV;

    rt_arp_solicit(rtdev, param->ip_addr);

    return 0;
}



static void cleanup_route_solicit(struct rt_proc_call *call)
{
    struct route_solicit_params *param;


    param = rtpc_get_priv(call, struct route_solicit_params);
    rtdev_dereference(param->rtdev);
}



static int rtnet_core_ioctl(struct rtnet_device *rtdev, unsigned int request,
                            unsigned long arg)
{
    struct rtnet_core_cmd       cmd;
    struct route_solicit_params params;
    struct rtnet_device         *tmp;
    int                         ret;
    unsigned long               flags;
    int                         i;


    ret = copy_from_user(&cmd, (void *)arg, sizeof(cmd));
    if (ret != 0)
        return -EFAULT;

    switch (request) {
        case IOC_RT_IFUP:
            if (down_interruptible(&rtdev->nrt_sem))
                return -ERESTARTSYS;

            set_bit(PRIV_FLAG_UP, &rtdev->priv_flags);

            rtdev->flags |= cmd.args.up.set_dev_flags;
            rtdev->flags &= ~cmd.args.up.clear_dev_flags;

            ret = rtdev_open(rtdev);    /* also = 0 if dev already up */
            if ((ret == 0) && (cmd.args.up.ip_addr != 0)) {
                rt_ip_route_del_all(rtdev); /* cleanup routing table */
                rtdev->local_ip     = cmd.args.up.ip_addr;
                rtdev->broadcast_ip = cmd.args.up.broadcast_ip;

                if (rtdev->flags & IFF_LOOPBACK) {
                    for (i = 0; i < MAX_RT_DEVICES; i++)
                        if ((tmp = rtdev_get_by_index(i)) != NULL) {
                            rt_ip_route_add_host(tmp->local_ip,
                                                 rtdev->dev_addr, rtdev);
                            rtdev_dereference(tmp);
                        }
                } else if ((tmp = rtdev_get_loopback()) != NULL) {
                    rt_ip_route_add_host(cmd.args.up.ip_addr,
                                         tmp->dev_addr, tmp);
                    rtdev_dereference(tmp);
                }

                if (rtdev->flags & IFF_BROADCAST)
                    rt_ip_route_add_host(cmd.args.up.broadcast_ip,
                                         rtdev->broadcast, rtdev);
            }

            up(&rtdev->nrt_sem);
            break;

        case IOC_RT_IFDOWN:
            if (down_interruptible(&rtdev->nrt_sem))
                return -ERESTARTSYS;

            /* spin lock required for sync with routing code */
            rtos_spin_lock_irqsave(&rtdev->rtdev_lock, flags);

            if (test_bit(PRIV_FLAG_ADDING_ROUTE, &rtdev->priv_flags)) {
                rtos_spin_unlock_irqrestore(&rtdev->dev_lock, flags);

                up(&rtdev->nrt_sem);
                return -EBUSY;
            }
            clear_bit(PRIV_FLAG_UP, &rtdev->priv_flags);

            rtos_spin_unlock_irqrestore(&rtdev->rtdev_lock, flags);

            ret = 0;
            if (rtdev->mac_detach != NULL)
                ret = rtdev->mac_detach(rtdev);

            if (ret == 0) {
                rt_ip_route_del_all(rtdev);
                ret = rtdev_close(rtdev);
            }

            up(&rtdev->nrt_sem);
            break;

        case IOC_RT_IFINFO:
            if (cmd.args.info.ifindex > 0)
                rtdev = rtdev_get_by_index(cmd.args.info.ifindex);
            else
                rtdev = rtdev_get_by_name(cmd.head.if_name);
            if (rtdev == NULL)
                return -ENODEV;

            if (down_interruptible(&rtdev->nrt_sem)) {
                rtdev_dereference(rtdev);
                return -ERESTARTSYS;
            }

            memcpy(cmd.head.if_name, rtdev->name, IFNAMSIZ);
            cmd.args.info.ifindex      = rtdev->ifindex;
            cmd.args.info.type         = rtdev->type;
            cmd.args.info.ip_addr      = rtdev->local_ip;
            cmd.args.info.broadcast_ip = rtdev->broadcast_ip;
            cmd.args.info.mtu          = rtdev->mtu;
            cmd.args.info.flags        = rtdev->flags;
            memcpy(cmd.args.info.dev_addr, rtdev->dev_addr, MAX_ADDR_LEN);

            up(&rtdev->nrt_sem);

            rtdev_dereference(rtdev);

            ret = copy_to_user((void *)arg, &cmd, sizeof(cmd));
            if (ret != 0)
                return -EFAULT;
            break;

        case IOC_RT_HOST_ROUTE_ADD:
            if (down_interruptible(&rtdev->nrt_sem))
                return -ERESTARTSYS;

            ret = rt_ip_route_add_host(cmd.args.addhost.ip_addr,
                                       cmd.args.addhost.dev_addr, rtdev);

            up(&rtdev->nrt_sem);
            break;

        case IOC_RT_HOST_ROUTE_SOLICIT:
            if (down_interruptible(&rtdev->nrt_sem))
                return -ERESTARTSYS;

            rtdev_reference(rtdev);
            params.rtdev   = rtdev;
            params.ip_addr = cmd.args.solicit.ip_addr;

            /* We need the rtpc wrapping because rt_arp_solicit can block on a
             * real-time semaphore in the NIC's xmit routine. */
            ret = rtpc_dispatch_call(route_solicit_handler, 0, &params,
                                     sizeof(params), NULL,
                                     cleanup_route_solicit);

            up(&rtdev->nrt_sem);
            break;

        case IOC_RT_HOST_ROUTE_DELETE:
            ret = rt_ip_route_del_host(cmd.args.delhost.ip_addr);
            break;

#ifdef CONFIG_RTNET_NETWORK_ROUTING
        case IOC_RT_NET_ROUTE_ADD:
            ret = rt_ip_route_add_net(cmd.args.addnet.net_addr,
                                      cmd.args.addnet.net_mask,
                                      cmd.args.addnet.gw_addr);
            break;

        case IOC_RT_NET_ROUTE_DELETE:
            ret = rt_ip_route_del_net(cmd.args.delnet.net_addr,
                                      cmd.args.delnet.net_mask);
            break;
#endif /* CONFIG_RTNET_NETWORK_ROUTING */

        default:
            ret = -ENOTTY;
    }

    return ret;
}



int rtnet_register_ioctls(struct rtnet_ioctls *ioctls)
{
    struct list_head    *entry;
    struct rtnet_ioctls *registered_ioctls;


    RTNET_ASSERT(ioctls->handler != NULL, return -EINVAL;);

    write_lock_bh(&ioctl_handler_lock);

    list_for_each(entry, &ioctl_handlers) {
        registered_ioctls = list_entry(entry, struct rtnet_ioctls, entry);
        if (registered_ioctls->ioctl_type == ioctls->ioctl_type) {
            write_unlock_bh(&ioctl_handler_lock);
            return -EEXIST;
        }
    }

    list_add_tail(&ioctls->entry, &ioctl_handlers);
    atomic_set(&ioctls->ref_count, 0);

    write_unlock_bh(&ioctl_handler_lock);

    return 0;
}



void rtnet_unregister_ioctls(struct rtnet_ioctls *ioctls)
{
    write_lock_bh(&ioctl_handler_lock);

    while (atomic_read(&ioctls->ref_count) != 0) {
        write_unlock_bh(&ioctl_handler_lock);

        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */

        write_lock_bh(&ioctl_handler_lock);
    }

    list_del(&ioctls->entry);

    write_unlock_bh(&ioctl_handler_lock);
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
