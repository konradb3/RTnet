/***
 *
 *  rtmac_disc.c
 *
 *  rtmac - real-time networking media access control subsystem
 *  Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>,
 *                2003, 2004 Jan Kiszka <Jan.Kiszka@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/spinlock.h>

#include <rtnet_internal.h>
#include <rtmac/rtmac_disc.h>
#include <rtmac/rtmac_proc.h>
#include <rtmac/rtmac_vnic.h>



static rwlock_t disc_list_lock = RW_LOCK_UNLOCKED;

LIST_HEAD(disc_list);

int __rtmac_disc_detach(struct rtnet_device *rtdev);



/***
 *  rtmac_disc_attach
 *
 *  @rtdev       attaches a discipline to a device
 *  @disc        discipline to attach
 *
 *  0            success
 *  -EBUSY       other discipline active
 *  -ENOMEM      could not allocate memory
 */
int rtmac_disc_attach(struct rtnet_device *rtdev, struct rtmac_disc *disc)
{
    int                 ret;
    struct rtmac_priv   *priv;


    RTNET_ASSERT(rtdev != NULL, return -EINVAL;);
    RTNET_ASSERT(disc != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->attach != NULL, return -EINVAL;);

    if (down_interruptible(&rtdev->nrt_sem))
        return -ERESTARTSYS;

    if (rtdev->mac_disc) {
        up(&rtdev->nrt_sem);
        printk("RTmac: another discipline for rtdev '%s' active.\n", rtdev->name);
        return -EBUSY;
    }


    /* alloc memory */
    priv = kmalloc(sizeof(struct rtmac_priv) + disc->priv_size, GFP_KERNEL);
    if (!priv) {
        up(&rtdev->nrt_sem);
        printk("RTmac: kmalloc returned NULL for rtmac!\n");
        return -ENOMEM;
    }
    priv->hard_start_xmit = rtdev->hard_start_xmit;

    /* call attach function of discipline */
    ret = disc->attach(rtdev, priv->disc_priv);
    if (ret < 0) {
        up(&rtdev->nrt_sem);
        kfree(priv);
        return ret;
    }

    /* now attach RTmac to device */
    rtdev->hard_start_xmit = disc->rt_packet_tx;
    rtdev->mac_disc = disc;
    rtdev->mac_priv = priv;
    rtdev->mac_detach = __rtmac_disc_detach;

    __MOD_INC_USE_COUNT(rtdev->owner);
    rtdev_reference(rtdev);

    /* create the VNIC */
    ret = rtmac_vnic_add(rtdev);
    if (ret < 0) {
        printk("RTmac: Warning, VNIC creation failed for rtdev %s.\n", rtdev->name);
    }

    up(&rtdev->nrt_sem);

    return 0;
}



/***
 *  rtmac_disc_detach
 *
 *  @rtdev       detaches a discipline from a device
 *
 *  0            success
 *  -1           discipline has no detach function
 *  -EINVAL      called with rtdev=NULL
 *  -ENODEV      no discipline active on dev
 */
int rtmac_disc_detach(struct rtnet_device *rtdev)
{
    int ret;


    RTNET_ASSERT(rtdev != NULL, return -EINVAL;);

    if (down_interruptible(&rtdev->nrt_sem))
        return -ERESTARTSYS;

    ret = __rtmac_disc_detach(rtdev);

    up(&rtdev->nrt_sem);

    return ret;
}



int __rtmac_disc_detach(struct rtnet_device *rtdev)
{
    int                 ret;
    struct rtmac_disc   *disc;
    struct rtmac_priv   *priv;


    disc = rtdev->mac_disc;
    if (!disc)
        return -ENODEV;

    RTNET_ASSERT(disc->detach != NULL, return -EINVAL;);

    priv = rtdev->mac_priv;
    RTNET_ASSERT(priv != NULL, return -EINVAL;);

    /* remove the VNIC */
    rtmac_vnic_remove(rtdev);

    /* call release function of discipline */
    ret = disc->detach(rtdev, priv->disc_priv);
    if (ret < 0)
        return ret;

    /* restore hard_start_xmit */
    rtdev->hard_start_xmit = priv->hard_start_xmit;

    /* remove pointers from rtdev */
    rtdev->mac_disc   = NULL;
    rtdev->mac_priv   = NULL;
    rtdev->mac_detach = NULL;

    __MOD_DEC_USE_COUNT(rtdev->owner);
    rtdev_dereference(rtdev);

    kfree(priv);

    return 0;
}



struct rtmac_disc *rtmac_get_disc_by_name(const char *name)
{
    struct list_head    *disc;


    read_lock_bh(&disc_list_lock);

    list_for_each(disc, &disc_list) {
        if (strcmp(((struct rtmac_disc *)disc)->name, name) == 0) {
            read_unlock_bh(&disc_list_lock);
            return (struct rtmac_disc *)disc;
        }
    }

    read_unlock_bh(&disc_list_lock);

    return NULL;
}



int rtmac_disc_register(struct rtmac_disc *disc)
{
    int ret;


    RTNET_ASSERT(disc != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->name != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->rt_packet_tx != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->nrt_packet_tx != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->attach != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->detach != NULL, return -EINVAL;);

    if (rtmac_get_disc_by_name(disc->name) != NULL)
    {
        printk("RTmac: discipline '%s' already registered!\n", disc->name);
        return -EBUSY;
    }

    ret = rtnet_register_ioctls(&disc->ioctls);
    if (ret < 0)
        return ret;

#ifdef CONFIG_PROC_FS
    ret = rtmac_disc_proc_register(disc);
    if (ret < 0) {
        rtnet_unregister_ioctls(&disc->ioctls);
        return ret;
    }
#endif /* CONFIG_PROC_FS */

    read_lock_bh(&disc_list_lock);

    list_add(&disc->list, &disc_list);

    read_unlock_bh(&disc_list_lock);

    return 0;
}



void rtmac_disc_deregister(struct rtmac_disc *disc)
{
    RTNET_ASSERT(disc != NULL, return;);

    read_lock_bh(&disc_list_lock);

    list_del(&disc->list);

    read_unlock_bh(&disc_list_lock);

    rtnet_unregister_ioctls(&disc->ioctls);

#ifdef CONFIG_PROC_FS
    rtmac_disc_proc_unregister(disc);
#endif /* CONFIG_PROC_FS */
}



#ifdef CONFIG_PROC_FS
int rtmac_proc_read_disc(char *buf, char **start, off_t offset, int count,
                         int *eof, void *data)
{
    struct list_head    *disc;
    RTNET_PROC_PRINT_VARS;


    RTNET_PROC_PRINT("Name\t\tID\n");

    read_lock_bh(&disc_list_lock);

    list_for_each(disc, &disc_list)
        RTNET_PROC_PRINT("%-15s %04X\n", ((struct rtmac_disc *)disc)->name,
                         ntohs(((struct rtmac_disc *)disc)->disc_type));

    read_unlock_bh(&disc_list_lock);

    RTNET_PROC_PRINT_DONE;
}
#endif /* CONFIG_PROC_FS */
