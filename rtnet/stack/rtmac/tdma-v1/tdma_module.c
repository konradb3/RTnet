/***
 *
 *  rtmac/tdma/tdma_module.c
 *
 *  rtmac - real-time networking media access control subsystem
 *  Copyright (C) 2002       Marc Kleine-Budde <kleine-budde@gmx.de>,
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>

#include <rtnet_sys.h>
#include <rtmac/rtmac_disc.h>
#include <rtmac/tdma-v1/tdma_cleanup.h>
#include <rtmac/tdma-v1/tdma_dev.h>
#include <rtmac/tdma-v1/tdma_event.h>
#include <rtmac/tdma-v1/tdma_ioctl.h>
#include <rtmac/tdma-v1/tdma_rx.h>


/* RTAI-specific: start scheduling timer */
#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30) || defined(CONFIG_RTAI_31) || defined(CONFIG_RTAI_32)
static int start_timer = 1;

MODULE_PARM(start_timer, "i");
MODULE_PARM_DESC(start_timer, "set to zero if RTAI timer is already running");
#endif

__u32 tdma_debug = TDMA_DEFAULT_DEBUG_LEVEL;

MODULE_PARM(tdma_debug, "i");
MODULE_PARM_DESC(tdma_debug, "tdma debug level");


#ifdef CONFIG_PROC_FS
int tdma_proc_read(char *buf, char **start, off_t offset, int count,
                   int *eof, void *data)
{
    RTNET_PROC_PRINT_VARS;


    RTNET_PROC_PRINT("todo\n");

    RTNET_PROC_PRINT_DONE;
}
#endif /* CONFIG_PROC_FS */



int tdma_attach(struct rtnet_device *rtdev, void *priv)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)priv;


    memset(tdma, 0, sizeof(struct rtmac_tdma));
    tdma->magic = TDMA_MAGIC;

    rtos_spin_lock_init(&tdma->delta_t_lock);

    tdma->flags.mac_active = 1;
    tdma->rtdev = rtdev;

    /*
     * init event
     * It is set to signaled state when the SOF arrived at the client.
     */
    rtos_event_init(&tdma->client_tx);

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


    return tdma_dev_init(rtdev, tdma);
}



int tdma_detach(struct rtnet_device *rtdev, void *priv)
{
    struct rtmac_tdma   *tdma = (struct rtmac_tdma *)priv;
    struct tdma_info    info;
    int                 ret;


    memset(&info, 0, sizeof(struct tdma_info));
    info.rtdev = rtdev;

    ret = tdma_do_event(tdma, REQUEST_DOWN, &info);
    if (ret < 0)
        return ret;

    ret = tdma_dev_release(tdma);
    if (ret < 0)
        return ret;

    /*
     * delete rt specific stuff
     * - lists
     *   * rt_add_list
     *   * rt_list
     *
     * FIXME: all these thingies _should_ be clean...test them
     */
    tdma_cleanup_master_rt_check(tdma);
    rtos_task_delete(&tdma->tx_task);

    /*
     * delete timers
     */
    del_timer(&tdma->task_change_timer);
    del_timer(&tdma->rt_add_timer);
    del_timer(&tdma->master_wait_timer);
    del_timer(&tdma->master_sent_conf_timer);
    del_timer(&tdma->master_sent_test_timer);

    /*
     * delete tx tasks event
     */
    rtos_event_delete(&tdma->client_tx);

    tdma->magic = ~TDMA_MAGIC;

    return 0;
}



int tdma_rt_packet_tx(struct rtskb *skb, struct rtnet_device *rtdev)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;

    rtcap_mark_rtmac_enqueue(skb);

    if (tdma->flags.mac_active == 0)
        return rtmac_xmit(skb);

    rtskb_prio_queue_tail(&tdma->tx_queue, skb);

    return 0;
}



int tdma_nrt_packet_tx(struct rtskb *skb)
{
    struct rtmac_tdma *tdma =
        (struct rtmac_tdma *)skb->rtdev->mac_priv->disc_priv;

    rtcap_mark_rtmac_enqueue(skb);

    if (tdma->flags.mac_active == 0)
        return rtmac_xmit(skb);

    skb->priority = QUEUE_MIN_PRIO;
    rtskb_prio_queue_tail(&tdma->tx_queue, skb);

    return 0;
}



#ifdef CONFIG_PROC_FS
struct rtmac_proc_entry tdma_proc_entries[] = {
    { name: "tdma", handler: tdma_proc_read },
    { name: NULL, handler: NULL }
};
#endif /* CONFIG_PROC_FS */

struct rtmac_disc tdma_disc = {
    name:           "TDMA",
    priv_size:      sizeof(struct rtmac_tdma),
    disc_type:      __constant_htons(ETH_TDMA),

    packet_rx:      tdma_packet_rx,
    rt_packet_tx:   tdma_rt_packet_tx,
    nrt_packet_tx:  tdma_nrt_packet_tx,

    attach:         tdma_attach,
    detach:         tdma_detach,

    ioctls:         {
        service_name:   "RTmac/TDMA",
        ioctl_type:     RTNET_IOC_TYPE_RTMAC_TDMA,
        handler:        tdma_ioctl
    },

#ifdef CONFIG_PROC_FS
    proc_entries:   tdma_proc_entries
#endif /* CONFIG_PROC_FS */
};



int __init tdma_init(void)
{
    int ret;


    printk("RTmac/TDMA: init time devision multiple access for realtime stations\n");

    ret = rtmac_disc_register(&tdma_disc);
    if (ret < 0)
        return ret;

#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30) || defined(CONFIG_RTAI_31) || defined(CONFIG_RTAI_32)
    if (start_timer) {
        rt_set_oneshot_mode();
        start_rt_timer(0);
    }
#endif

    return 0;
}



void tdma_release(void)
{
#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30) || defined(CONFIG_RTAI_31) || defined(CONFIG_RTAI_32)
    if (start_timer)
        stop_rt_timer();
#endif

    rtmac_disc_deregister(&tdma_disc);

    printk("RTmac/TDMA: unloaded\n");
}



module_init(tdma_init);
module_exit(tdma_release);

MODULE_AUTHOR("Marc Kleine-Budde, Jan Kiszka");
MODULE_LICENSE("GPL");
