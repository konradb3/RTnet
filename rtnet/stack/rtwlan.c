/* rtwlan.c
 *
 * rtwlan protocol stack
 * Copyright (c) 2006, Daniel Gregorek <dxg@gmx.de> 
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

#include <linux/kernel.h>
#include <linux/module.h>

#include <rtnet_port.h>

#include <rtwlan_io.h>
#include <rtwlan.h>


int rtwlan_rx(struct rtskb * rtskb, struct rtnet_device * rtnet_dev)
{
    struct rtwlan_device * rtwlan = rtnetdev_priv(rtnet_dev);
    struct ieee80211_hdr  * hdr = (struct ieee80211_hdr *)rtskb->data;
    u16 fc = le16_to_cpu(hdr->frame_ctl);

    switch(rtwlan->mode) {
        case RTWLAN_MODE_RAW:
        case RTWLAN_MODE_ACK:
            rtskb_pull(rtskb, ieee80211_get_hdrlen(fc));
            rtskb->protocol = rt_eth_type_trans (rtskb, rtnet_dev);
            break;

        case RTWLAN_MODE_MON:
            rtskb->mac.raw = rtskb->data;
            rtcap_mark_incoming(rtskb);
            break;
    }

    return 0;
}

EXPORT_SYMBOL(rtwlan_rx);


int rtwlan_tx(struct rtskb *rtskb, struct rtnet_device *rtnet_dev)
{
    struct rtwlan_device * rtwlan = rtnetdev_priv(rtnet_dev);
    struct ieee80211_hdr_3addr header = {	/* Ensure zero initialized */
        .duration_id = 0,
        .seq_ctl = 0
    };
    u8 dest[ETH_ALEN], src[ETH_ALEN];

    /* Save source and destination addresses */
    memcpy(dest, rtskb->data, ETH_ALEN);
    memcpy(src, rtskb->data + ETH_ALEN, ETH_ALEN);

    /* Generate ieee80211 compatible header */
    memcpy(header.addr3, src, ETH_ALEN);	/* BSSID */
    memcpy(header.addr2, src, ETH_ALEN);	/* SA */
    memcpy(header.addr1, dest, ETH_ALEN);	/* DA */

    /* Frame Control */
    switch(rtwlan->mode) {
        case RTWLAN_MODE_RAW:
            header.frame_ctl = cpu_to_le16(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
            break;

        case RTWLAN_MODE_ACK:
            header.frame_ctl = cpu_to_le16(IEEE80211_FTYPE_DATA | IEEE80211_STYPE_DATA);
            break;

        default:
            return -1;
    }

    memcpy(rtskb_push(rtskb, IEEE80211_3ADDR_LEN), &header, IEEE80211_3ADDR_LEN);

    return 0;
}

EXPORT_SYMBOL(rtwlan_tx);


/**
 * rtalloc_wlandev - Allocates and sets up a wlan device
 * @sizeof_priv: size of additional driver-private structure to
 *               be allocated for this wlan device
 *
 * Fill in the fields of the device structure with wlan-generic
 * values. Basically does everything except registering the device.
 *
 * A 32-byte alignment is enforced for the private data area.
 */

struct rtnet_device * rtwlan_alloc_dev(int sizeof_priv)
{
    struct rtnet_device *rtnet_dev;

    RTWLAN_DEBUG("Start.\n");

    rtnet_dev = rt_alloc_etherdev(sizeof(struct rtwlan_device) + sizeof_priv);
    if (!rtnet_dev)
        return NULL;

    rtdev_alloc_name(rtnet_dev, "rtwlan%d");

    return rtnet_dev;
}

EXPORT_SYMBOL(rtwlan_alloc_dev);


int rtwlan_ioctl(struct rtnet_device * rtdev,
                 unsigned int request,
                 unsigned long arg)
{
    struct rtwlan_cmd cmd;
    int ret=0;

    if (copy_from_user(&cmd, (void *)arg, sizeof(cmd)) != 0)
        return -EFAULT;

    switch(request) {
        case IOC_RTWLAN_IFINFO:
            if (cmd.args.info.ifindex > 0)
                rtdev = rtdev_get_by_index(cmd.args.info.ifindex);
            else
                rtdev = rtdev_get_by_name(cmd.head.if_name);
            if (rtdev == NULL)
                return -ENODEV;

            if (down_interruptible(&rtdev->nrt_lock)) {
                rtdev_dereference(rtdev);
                return -ERESTARTSYS;
            }

            if (rtdev->do_ioctl)
                ret = rtdev->do_ioctl(rtdev, request, &cmd);
            else 
                ret = -ENORTWLANDEV;

            memcpy(cmd.head.if_name, rtdev->name, IFNAMSIZ);
            cmd.args.info.ifindex      = rtdev->ifindex;
            cmd.args.info.flags        = rtdev->flags;

            up(&rtdev->nrt_lock);

            rtdev_dereference(rtdev);

            break;

        case IOC_RTWLAN_MODE:
        case IOC_RTWLAN_BITRATE:
        case IOC_RTWLAN_CHANNEL:
        case IOC_RTWLAN_TXPOWER:
        case IOC_RTWLAN_DROPBCAST:
        case IOC_RTWLAN_DROPMCAST:
        case IOC_RTWLAN_REGREAD:
        case IOC_RTWLAN_REGWRITE:
        case IOC_RTWLAN_BBPWRITE:
        case IOC_RTWLAN_BBPREAD:
        case IOC_RTWLAN_BBPSENS:
            if (down_interruptible(&rtdev->nrt_lock))
                return -ERESTARTSYS;

            if (rtdev->do_ioctl)
                ret = rtdev->do_ioctl(rtdev, request, &cmd);
            else
                ret = -ENORTWLANDEV;

            up(&rtdev->nrt_lock);

            break;

        default:
            ret = -ENOTTY;
    }

    if (copy_to_user((void *)arg, &cmd, sizeof(cmd)) != 0)
        return -EFAULT;

    return ret;
}


struct rtnet_ioctls rtnet_wlan_ioctls = {
    service_name: "rtwlan ioctl",
    ioctl_type: RTNET_IOC_TYPE_RTWLAN,
    handler: rtwlan_ioctl
};

int __init rtwlan_init(void)
{
    if (rtnet_register_ioctls(&rtnet_wlan_ioctls))
        rtdm_printk(KERN_ERR "Failed to register rtnet_wlan_ioctl!\n");

    return 0;
}


void __exit rtwlan_exit(void)
{
    printk(KERN_INFO "Unloading module: rtwlan\n");
    rtnet_unregister_ioctls(&rtnet_wlan_ioctls);
}
