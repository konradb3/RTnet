/* rtmac_disc.c
 *
 * rtmac - real-time networking media access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>,
 *               2003 Jan Kiszka <Jan.Kiszka@web.de>
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

#include <rtnet_internal.h>
#include <rtmac/rtmac_disc.h>
#include <rtmac/rtmac_vnic.h>



struct list_head    disc_list = LIST_HEAD_INIT(disc_list);


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

    if (rtdev->mac_disc) {
        printk("RTmac: another discipline for rtdev '%s' active.\n", rtdev->name);
        return -EBUSY;
    }


    /* alloc memory */
    priv = kmalloc(sizeof(struct rtmac_priv) + disc->priv_size, GFP_KERNEL);
    if (!priv) {
        printk("RTmac: kmalloc returned NULL for rtmac!\n");
        return -ENOMEM;
    }
    priv->hard_start_xmit = rtdev->hard_start_xmit;

    /* call attach function of discipline */
    ret = disc->attach(rtdev, priv->disc_priv);
    if (ret < 0)
    {
        kfree(priv);
        return ret;
    }

    /* now attach RTmac to device */
    rtdev->hard_start_xmit = disc->rt_packet_tx;
    rtdev->mac_disc = disc;
    rtdev->mac_priv = priv;

    /* create the VNIC */
    ret = rtmac_vnic_add(rtdev);
    if (ret < 0) {
        printk("RTmac: Warning, VNIC creation failed for rtdev %s.\n", rtdev->name);
    }

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
    int                 ret;
    struct rtmac_disc   *disc;
    struct rtmac_priv   *priv;


    RTNET_ASSERT(rtdev != NULL, return -EINVAL;);

    disc = rtdev->mac_disc;
    if (!disc) {
        printk("RTmac: no discipline active on rtdev '%s'\n", rtdev->name);
        return -ENODEV;
    }
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
    rtdev->mac_disc = NULL;
    rtdev->mac_priv = NULL;

    kfree(priv);

    return 0;
}



struct rtmac_disc *rtmac_get_disc_by_name(char *name)
{
    struct list_head *disc;


    list_for_each(disc, &disc_list) {
        if (strcmp(((struct rtmac_disc *)disc)->name, name) == 0)
            return (struct rtmac_disc *)disc;
    }

    return NULL;
}



int rtmac_disc_register(struct rtmac_disc *disc)
{
    RTNET_ASSERT(disc != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->name != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->rt_packet_tx != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->nrt_packet_tx != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->attach != NULL, return -EINVAL;);
    RTNET_ASSERT(disc->detach != NULL, return -EINVAL;);
    /*RTNET_ASSERT(disc->ioctl != NULL, return -EINVAL;);*/ /* not supported yet */
    RTNET_ASSERT(disc->ioctl_ops != NULL, return -EINVAL;);

    if (rtmac_get_disc_by_name(disc->name) != NULL)
    {
        printk("RTmac: discipline '%s' already registered!\n", disc->name);
    }

    list_add(&disc->list, &disc_list);

    return 0;
}



void rtmac_disc_deregister(struct rtmac_disc *disc)
{
    RTNET_ASSERT(disc != NULL, return;);

    list_del(&disc->list);
}
