/* rtmac_task.c
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

#include <rtai.h>

#include <ipv4/arp.h>
#include <rtmac/tdma/tdma.h>
#include <rtmac/tdma/tdma_cleanup.h>
#include <rtmac/tdma/tdma_event.h>
#include <rtmac/tdma/tdma_timer.h>


void tdma_task_shutdown(struct rtmac_tdma *tdma)
{
    if (tdma->flags.task_active == 1) {
        tdma->flags.shutdown_task = 1;

        /* In case the application has stopped the timer, it's
         * likely that the tx_task will be waiting forever in
         * rt_task_wait_period().  So we wakeup the task here for
         * sure.
         * -WY-
         */
        rt_task_wakeup_sleeping(&tdma->tx_task);

        /*
         * unblock all tasks by deleting semas
         */
        rt_sem_delete(&tdma->client_tx);    /* tdma_task_client() */

        /*
         * re-init semas
         */
        rt_sem_init(&tdma->client_tx, 0);
    }
}



int tdma_task_change(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle)
{
    int ret = 0;

    /*
     * shutdown the task
     */
    tdma_task_shutdown(tdma);

    ret = tdma_timer_start_task_change(tdma, task, cycle, TDMA_NOTIFY_TASK_CYCLE);

    return ret;
}



int tdma_task_change_con(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle)
{
    struct rtnet_device *rtdev = tdma->rtdev;
    int ret = 0;

    if (tdma->flags.task_active) {
        rt_printk("RTmac: tdma: %s() task was not shutted down.\n",__FUNCTION__);
        rt_task_delete(&tdma->tx_task);
    }

    ret = rt_task_init(&tdma->tx_task, task, (int)rtdev, 4096, TDMA_PRIO_TX_TASK, 0, 0);

    if (cycle != 0)
        ret = rt_task_make_periodic_relative_ns(&tdma->tx_task, 1000*1000, cycle);
    else
        ret = rt_task_resume(&tdma->tx_task);

    if (ret != 0)
        rt_printk("RTmac: tdma: %s() not successful\n",__FUNCTION__);
    else
        TDMA_DEBUG(2, "RTmac: tdma: %s() succsessful\n",__FUNCTION__);


    tdma->flags.task_active = 1;
    return ret;
}



void tdma_task_notify(int rtdev_id)
{
    struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;

    struct rtskb *skb;
    void *data;

    while (tdma->flags.shutdown_task == 0) {
        /*
         * alloc message
         */
        skb = tdma_make_msg(rtdev, NULL, NOTIFY_MASTER, &data);

        /*
         * wait 'till begin of next period
         */
        rt_task_wait_period();

        /*
         * transmit packet
         */
        tdma_xmit(skb);
    }

    TDMA_DEBUG(2, "RTmac: tdma: %s() shutdown complete\n",__FUNCTION__);
    tdma->flags.task_active = 0;
    tdma->flags.shutdown_task = 0;
}





void tdma_task_config(int rtdev_id)
{
    struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    struct rtskb *skb;
    struct tdma_test_msg *test_msg;
    void *data = &test_msg;
    struct tdma_rt_entry *rt_entry;
    struct list_head *lh;
    int i, max;

    max = TDMA_MASTER_MAX_TEST;

    list_for_each(lh, &tdma->rt_list) {
        rt_entry = list_entry(lh, struct tdma_rt_entry, list);

        if (rt_entry->state != RT_RCVD_CONF )
            goto out;

        TDMA_DEBUG(4, "RTmac: tdma: %s() sending %d test packets to %u.%u.%u.%u\n",
            __FUNCTION__,max, NIPQUAD(rt_entry->arp->ip_addr));

        for (i = 0; i < max; i++) {
            if (!(rt_entry->state == RT_RCVD_CONF || rt_entry->state == RT_RCVD_TEST))
                goto out;

            TDMA_DEBUG(6, "RTmac: tdma: %s() sending test packet #%d to %u.%u.%u.%u\n",
                __FUNCTION__,i, NIPQUAD(rt_entry->arp->ip_addr));

            /*
            * alloc skb, and put counter and time into it....
            */
            skb = tdma_make_msg(rtdev, rt_entry->arp->hw_addr, REQUEST_TEST, data);
            rt_entry->counter = test_msg->counter = i;
            rt_entry->state = RT_SENT_TEST;
            rt_entry->tx = test_msg->tx = rt_get_time();

            /*
            * transmit packet
            */
            tdma_xmit(skb);

            /*
            * wait
            */
            rt_task_wait_period();
        }
    }

    TDMA_DEBUG(3, "RTmac: tdma: %s() shutdown complete\n",__FUNCTION__);
    tdma->flags.task_active = 0;
    tdma->flags.shutdown_task = 0;

    tdma_timer_start_sent_test(tdma, TDMA_MASTER_WAIT_TEST);
    tdma_next_state(tdma, TDMA_MASTER_SENT_TEST);

    return;

out:
    rt_printk("RTmac: tdma: *** WARNING *** %s() received not ACK from station %d, IP %u.%u.%u.%u, going into DOWN state\n",
        __FUNCTION__,rt_entry->station, NIPQUAD(rt_entry->arp->ip_addr));
    tdma_cleanup_master_rt(tdma);
    tdma_next_state(tdma, TDMA_DOWN);

    tdma->flags.task_active = 0;
    tdma->flags.shutdown_task = 0;
    TDMA_DEBUG(2, "RTmac: tdma: %s() shutdown complete\n",__FUNCTION__);
}





void tdma_task_master(int rtdev_id)
{
    struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    struct rtskb *skb;
    void *data;
    RTIME time_stamp;

    while (tdma->flags.shutdown_task == 0) {
        /*
         * make, resp. get master skb
         */
        /*FIXME: rtskb_queue_empty(tdma->master_queue) enable to send real msgs...*/

        skb = tdma_make_msg(rtdev, NULL, START_OF_FRAME, &data);

        if (!skb) {
            rt_task_wait_period();
            continue;
        }

        rt_task_wait_period();

        /* Store timestamp in SOF. I assume that there is enough space. */
        time_stamp = rt_get_time_ns();
        *(RTIME *)data = cpu_to_be64(time_stamp);

        tdma_xmit(skb);

        /* Calculate delta_t for the master by assuming the current
         * to be the virtual receiption time. Then inform all listings
         * tasks that the SOF has been sent.
         * -JK-
         */
        tdma->delta_t = time_stamp-rt_get_time_ns();
        rt_sem_broadcast(&tdma->client_tx);

        /*
         * get client skb out of queue and send it
         */

        skb = rtskb_dequeue(&tdma->rt_tx_queue);
        if (!skb)
            skb = rtskb_dequeue(&tdma->nrt_tx_queue);

        if (skb)
            tdma_xmit(skb);
    }
    TDMA_DEBUG(2, "RTmac: tdma: %s() shutdown complete\n",__FUNCTION__);
    tdma->flags.task_active = 0;
    tdma->flags.shutdown_task = 0;
}




void tdma_task_client(int rtdev_id)
{
    struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    struct rtskb *skb;

    while(tdma->flags.shutdown_task == 0) {
        if (rt_sem_wait(&tdma->client_tx) == 0xFFFF) {
            rt_printk("RTmac: tdma: %s() rt_sem_wait(client_tx) failed\n",__FUNCTION__);
            break;
        }

        rt_sleep_until(tdma->wakeup);

        skb = rtskb_dequeue(&tdma->rt_tx_queue);
        if (!skb)
            skb = rtskb_dequeue(&tdma->nrt_tx_queue);

        if (skb)
            tdma_xmit(skb);
    }

    TDMA_DEBUG(2, "RTmac: tdma: %s() shutdown complete\n",__FUNCTION__);
    tdma->flags.task_active = 0;
    tdma->flags.shutdown_task = 0;
}
