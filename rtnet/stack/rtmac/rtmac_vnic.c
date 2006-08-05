/* rtmac_vnic.c
 *
 * rtmac - real-time networking media access control subsystem
 * Copyright (C) 2002      Marc Kleine-Budde <kleine-budde@gmx.de>,
 *               2003-2005 Jan Kiszka <Jan.Kiszka@web.de>
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


#include <linux/moduleparam.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>

#include <rtnet_internal.h>
#include <rtdev.h>
#include <rtmac/rtmac_disc.h>
#include <rtmac/rtmac_proto.h>
#include <rtmac/rtmac_vnic.h>


static unsigned int vnic_rtskbs = DEFAULT_VNIC_RTSKBS;
module_param(vnic_rtskbs, uint, 0444);
MODULE_PARM_DESC(vnic_rtskbs, "Number of realtime socket buffers per virtual NIC");

static rtdm_nrtsig_t        vnic_signal;
static struct rtskb_queue   rx_queue;



int rtmac_vnic_rx(struct rtskb *rtskb, u16 type)
{
    struct rtmac_priv *mac_priv = rtskb->rtdev->mac_priv;
    struct rtskb_queue *pool = &mac_priv->vnic_skb_pool;


    if (rtskb_acquire(rtskb, pool) != 0) {
        mac_priv->vnic_stats.rx_dropped++;
        kfree_rtskb(rtskb);
        return -1;
    }

    rtskb->protocol = type;

    rtdev_reference(rtskb->rtdev);
    rtskb_queue_tail(&rx_queue, rtskb);
    rtdm_nrtsig_pend(&vnic_signal);

    return 0;
}



static void rtmac_vnic_signal_handler(rtdm_nrtsig_t nrtsig)
{
    struct rtskb            *rtskb;
    struct sk_buff          *skb;
    unsigned                hdrlen;
    struct net_device_stats *stats;
    struct rtnet_device     *rtdev;


    while (1)
    {
        rtskb = rtskb_dequeue(&rx_queue);
        if (!rtskb)
            break;

        rtdev  = rtskb->rtdev;
        hdrlen = rtdev->hard_header_len;

        skb = dev_alloc_skb(hdrlen + rtskb->len + 2);
        if (skb) {
            /* the rtskb stamp is useless (different clock), get new one */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
            __net_timestamp(skb);
#else
            do_gettimeofday(&skb->stamp);
#endif

            skb_reserve(skb, 2); /* Align IP on 16 byte boundaries */

            /* copy Ethernet header */
            memcpy(skb_put(skb, hdrlen),
                   rtskb->data - hdrlen - sizeof(struct rtmac_hdr), hdrlen);

            /* patch the protocol field in the original Ethernet header */
            ((struct ethhdr*)skb->data)->h_proto = rtskb->protocol;

            /* copy data */
            memcpy(skb_put(skb, rtskb->len), rtskb->data, rtskb->len);

            skb->dev      = &rtskb->rtdev->mac_priv->vnic;
            skb->protocol = eth_type_trans(skb, skb->dev);

            stats = &rtskb->rtdev->mac_priv->vnic_stats;

            kfree_rtskb(rtskb);

            stats->rx_packets++;
            stats->rx_bytes += skb->len;

            netif_rx(skb);
        }
        else {
            printk("RTmac: VNIC fails to allocate linux skb\n");
            kfree_rtskb(rtskb);
        }

        rtdev_dereference(rtdev);
    }
}



static int rtmac_vnic_open(struct net_device *dev)
{
    memcpy(dev->dev_addr, ((struct rtnet_device*)dev->priv)->dev_addr,
           sizeof(dev->dev_addr));

    return 0;
}



int rtmac_vnic_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct rtnet_device     *rtdev = (struct rtnet_device*)dev->priv;
    struct net_device_stats *stats = &rtdev->mac_priv->vnic_stats;
    struct rtskb_queue      *pool = &rtdev->mac_priv->vnic_skb_pool;
    struct ethhdr           *ethernet = (struct ethhdr*)skb->data;
    struct rtskb            *rtskb;
    int                     res;
    int                     data_len;


    rtskb =
        alloc_rtskb((skb->len + sizeof(struct rtmac_hdr) + 15) & ~15, pool);
    if (!rtskb) {
        stats->tx_dropped++;
        return -ENOMEM;
    }

    rtskb_reserve(rtskb, rtdev->hard_header_len + sizeof(struct rtmac_hdr));

    data_len = skb->len - dev->hard_header_len;
    memcpy(rtskb_put(rtskb, data_len), skb->data + dev->hard_header_len,
           data_len);

    res = rtmac_add_header(rtdev, ethernet->h_dest, rtskb,
                           ntohs(ethernet->h_proto), RTMAC_FLAG_TUNNEL);
    if (res < 0) {
        stats->tx_dropped++;
        kfree_rtskb(rtskb);
        return res;
    }

    RTNET_ASSERT(rtdev->mac_disc->nrt_packet_tx != NULL, kfree_rtskb(rtskb);
                 return -1;);

    stats->tx_packets++;
    stats->tx_bytes += skb->len;

    res = rtdev->mac_disc->nrt_packet_tx(rtskb);
    if (res < 0) {
        stats->tx_dropped++;
        kfree_rtskb(rtskb);
    } else
        kfree_skb(skb);

    return res;
}



static struct net_device_stats *rtmac_vnic_get_stats(struct net_device *dev)
{
    return &((struct rtnet_device*)dev->priv)->mac_priv->vnic_stats;
}



static int rtmac_vnic_change_mtu(struct net_device *dev, int new_mtu)
{
    if ((new_mtu < 68) ||
        ((unsigned)new_mtu > 1500 - sizeof(struct rtmac_hdr)))
        return -EINVAL;
    dev->mtu = new_mtu;
    return 0;
}



void rtmac_vnic_set_max_mtu(struct rtnet_device *rtdev, unsigned int max_mtu)
{
    struct rtmac_priv   *mac_priv = rtdev->mac_priv;
    unsigned int        prev_mtu  = mac_priv->vnic_max_mtu;


    mac_priv->vnic_max_mtu = max_mtu - sizeof(struct rtmac_hdr);

    /* set vnic mtu in case max_mtu is smaller than the current mtu or
       the current mtu was set to previous max_mtu */
    rtnl_lock();
    if ((mac_priv->vnic.mtu > mac_priv->vnic_max_mtu) || (prev_mtu == mac_priv->vnic_max_mtu)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        dev_set_mtu(&mac_priv->vnic, mac_priv->vnic_max_mtu);
#else   /* LINUX_VERSION_CODE < 2.6.x */
        if (mac_priv->vnic.flags & IFF_UP) {
            dev_close(&mac_priv->vnic);
            mac_priv->vnic.mtu = mac_priv->vnic_max_mtu;
            dev_open(&mac_priv->vnic);
        } else
            mac_priv->vnic.mtu = mac_priv->vnic_max_mtu;
#endif  /* LINUX_VERSION_CODE < 2.6.x */
    }
    rtnl_unlock();
}



static int rtmac_vnic_init(struct net_device *dev)
{
    struct rtnet_device *rtdev = (struct rtnet_device *)dev->priv;


    ether_setup(dev);

    dev->open            = rtmac_vnic_open;
    dev->get_stats       = rtmac_vnic_get_stats;
    dev->change_mtu      = rtmac_vnic_change_mtu;
    dev->set_mac_address = NULL;

    dev->mtu             = rtdev->mac_priv->vnic_max_mtu;
    dev->flags           &= ~IFF_MULTICAST;

    SET_MODULE_OWNER(dev);

    return 0;
}



int rtmac_vnic_add(struct rtnet_device *rtdev, vnic_xmit_handler vnic_xmit)
{
    int                 res;
    struct rtmac_priv   *mac_priv = rtdev->mac_priv;
    struct net_device   *vnic = &mac_priv->vnic;


    /* does the discipline request vnic support? */
    if (!vnic_xmit)
        return 0;

    mac_priv->vnic_registered = 0;
    mac_priv->vnic_max_mtu    = rtdev->mtu - sizeof(struct rtmac_hdr);
    memset(&mac_priv->vnic_stats, 0, sizeof(mac_priv->vnic_stats));

    /* create the rtskb pool */
    if (rtskb_pool_init(&mac_priv->vnic_skb_pool,
                        vnic_rtskbs) < vnic_rtskbs) {
        rtskb_pool_release(&mac_priv->vnic_skb_pool);
        return -ENOMEM;
    }

    memset(vnic, 0, sizeof(struct net_device));

    vnic->init            = rtmac_vnic_init;
    vnic->hard_start_xmit = vnic_xmit;
    vnic->priv            = rtdev;

    snprintf(vnic->name, sizeof(vnic->name), "vnic%d", rtdev->ifindex-1);

    res = register_netdev(vnic);
    if (res == 0)
        mac_priv->vnic_registered = 1;
    else
        rtskb_pool_release(&mac_priv->vnic_skb_pool);

    return res;
}



void rtmac_vnic_unregister(struct rtnet_device *rtdev)
{
    struct rtmac_priv   *mac_priv = rtdev->mac_priv;


    if (mac_priv->vnic_registered) {
        unregister_netdev(&mac_priv->vnic);
        mac_priv->vnic_registered = 0;
    }
}



#ifdef CONFIG_PROC_FS
int rtmac_proc_read_vnic(char *buf, char **start, off_t offset, int count,
                         int *eof, void *data)
{
    struct rtnet_device *rtdev;
    int                 i;
    int                 res = 0;
    RTNET_PROC_PRINT_VARS(80);


    if (!RTNET_PROC_PRINT("RT-NIC name\tVNIC name\n"))
        goto done;

    for (i = 1; i <= MAX_RT_DEVICES; i++) {
        rtdev = rtdev_get_by_index(i);
        if (rtdev == NULL)
            continue;

        if (down_interruptible(&rtdev->nrt_lock)) {
            rtdev_dereference(rtdev);
            return -ERESTARTSYS;
        }

        if (rtdev->mac_priv != NULL) {
            struct rtmac_priv *rtmac;

            rtmac = (struct rtmac_priv *)rtdev->mac_priv;
            res = RTNET_PROC_PRINT("%-15s %s\n",
                                    rtdev->name, rtmac->vnic.name);
        }

        up(&rtdev->nrt_lock);
        rtdev_dereference(rtdev);

        if (!res)
            break;
    }

  done:
    RTNET_PROC_PRINT_DONE;
}
#endif /* CONFIG_PROC_FS */



int __init rtmac_vnic_module_init(void)
{
    rtskb_queue_init(&rx_queue);

    return rtdm_nrtsig_init(&vnic_signal, rtmac_vnic_signal_handler);
}



void rtmac_vnic_module_cleanup(void)
{
    struct rtskb *rtskb;


    rtdm_nrtsig_destroy(&vnic_signal);

    while ((rtskb = rtskb_dequeue(&rx_queue)) != NULL) {
        rtdev_dereference(rtskb->rtdev);
        kfree_rtskb(rtskb);
    }
}
