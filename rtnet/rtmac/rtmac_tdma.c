/* rtmac_tdma.c
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>

#include <rtai.h>

#include <rtmac/rtmac_chrdev.h>
#include <rtmac/rtmac_disc.h>
#include <rtmac/tdma/tdma_cleanup.h>
#include <rtmac/tdma/tdma_ioctl.h>
#include <rtmac/tdma/tdma_rx.h>


__u32 tdma_debug = TDMA_DEFAULT_DEBUG_LEVEL;
MODULE_PARM(tdma_debug, "i");
MODULE_PARM_DESC(cards, "tdma debug level");




int tdma_attach(struct rtnet_device *rtdev, void *priv)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)priv;

    rt_printk("RTmac: tdma1: init time devision multiple access (tdma) for realtime stations\n");

    memset(tdma, 0, sizeof(struct rtmac_tdma));
    spin_lock_init(&tdma->delta_t_lock);

    tdma->rtdev = rtdev;

    /*
     * init semas, they implement a producer consumer between the
     * sending realtime- and the driver-task
     *
     */
    rt_sem_init(&tdma->client_tx, 0);

    /*
     * init tx queue
     *
     */
    rtskb_prio_queue_init(&tdma->tx_queue);

    /*
     * init rt stuff
     * - timer
     * - list heads
     *
     */
    /* generic */

    /* master */
    init_timer(&tdma->rt_add_timer);
    INIT_LIST_HEAD(&tdma->rt_add_list);
    INIT_LIST_HEAD(&tdma->rt_list);
    INIT_LIST_HEAD(&tdma->rt_list_rate);

    init_timer(&tdma->task_change_timer);
    init_timer(&tdma->master_wait_timer);
    init_timer(&tdma->master_sent_conf_timer);
    init_timer(&tdma->master_sent_test_timer);

    rtskb_queue_init(&tdma->master_queue);


    /* client */
    init_timer(&tdma->client_sent_ack_timer);


    /*
     * start timer
     */
    rt_set_oneshot_mode();
    start_rt_timer(0);

    return 0;
}



int tdma_detach(struct rtnet_device *rtdev, void *priv)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)priv;

    rt_printk("RTmac: tdma: release\n");

    /*
     * delete rt specific stuff
     * - lists
     *   * rt_add_list
     *   * rt_list
     *
     * FIXME: all these thingies _should_ be clean...test them
     */
    tdma_cleanup_master_rt_check(tdma);

    /*
     * delete timers
     */
    del_timer(&tdma->task_change_timer);
    del_timer(&tdma->rt_add_timer);
    del_timer(&tdma->master_wait_timer);
    del_timer(&tdma->master_sent_conf_timer);
    del_timer(&tdma->master_sent_test_timer);

    /*
     * delete tx tasks sema
     */
    rt_sem_delete(&tdma->client_tx);

    return 0;
}



int tdma_rt_packet_tx(struct rtskb *skb, struct rtnet_device *rtdev)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;

    if (tdma->flags.mac_active == 0)
        return tdma_xmit(skb);

    rtskb_prio_queue_tail(&tdma->tx_queue, skb);

    return 0;
}



int tdma_nrt_packet_tx(struct rtskb *skb)
{
    struct rtmac_tdma *tdma =
        (struct rtmac_tdma *)skb->rtdev->mac_priv->disc_priv;

    if (tdma->flags.mac_active == 0)
        return -1;

    skb->priority = QUEUE_MIN_PRIO;
    rtskb_prio_queue_tail(&tdma->tx_queue, skb);

    return 0;
}



/* legacy */
static struct rtmac_ioctl_ops tdma_ioctl_ops = {
    client: &tdma_ioctl_client,
    master: &tdma_ioctl_master,
    up:     &tdma_ioctl_up,
    down:   &tdma_ioctl_down,
    add:    &tdma_ioctl_add,
    remove: &tdma_ioctl_remove,
    cycle:  &tdma_ioctl_cycle,
    mtu:    &tdma_ioctl_mtu,
    offset: &tdma_ioctl_offset,
};
/* end of legacy */

static struct rtmac_disc tdma_disc = {
    name:           "TDMA1",
    priv_size:      sizeof(struct rtmac_tdma),
    disc_type:      __constant_htons(ETH_TDMA),

    packet_rx:      &tdma_packet_rx,
    rt_packet_tx:   &tdma_rt_packet_tx,
    nrt_packet_tx:  &tdma_nrt_packet_tx,

    attach:         &tdma_attach,
    detach:         &tdma_detach,

    ioctl_ops:      &tdma_ioctl_ops /* legacy */
};



int tdma_init(void)
{
    return rtmac_disc_register(&tdma_disc);
}



void tdma_release(void)
{
    rtmac_disc_deregister(&tdma_disc);
}
