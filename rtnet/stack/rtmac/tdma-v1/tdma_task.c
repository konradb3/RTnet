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

#include <ipv4/arp.h>
#include <rtmac/tdma-v1/tdma.h>
#include <rtmac/tdma-v1/tdma_cleanup.h>
#include <rtmac/tdma-v1/tdma_event.h>
#include <rtmac/tdma-v1/tdma_timer.h>


void tdma_task_shutdown(struct rtmac_tdma *tdma)
{
    if (tdma->flags.task_active == 1) {
        tdma->flags.shutdown_task = 1;

#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30) || defined(CONFIG_RTAI_31) || defined(CONFIG_RTAI_32)
        /* RTAI-specific:
         * In case the application has stopped the timer, it's
         * likely that the tx_task will be waiting forever in
         * rt_task_wait_period(). So we wakeup the task here for
         * sure.
         * -WY-
         */
        rt_task_wakeup_sleeping(&tdma->tx_task);
#endif

        /*
         * unblock all tasks
         */
        rtos_event_broadcast(&tdma->client_tx);     /* tdma_task_client() */
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



int tdma_task_change_con(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle_ns)
{
    struct rtnet_device *rtdev = tdma->rtdev;
    rtos_time_t cycle_time;
    int ret = 0;

//    if (tdma->flags.task_active) {
//        TDMA_DEBUG(0, "RTmac: tdma: %s() task was not shutted down.\n",__FUNCTION__);
        rtos_task_delete(&tdma->tx_task);
//    }

    if (cycle_ns != 0) {
        rtos_nanosecs_to_time(cycle_ns, &cycle_time);
        ret = rtos_task_init_periodic(&tdma->tx_task, task, (int)rtdev,
                                      TDMA_PRIO_TX_TASK, &cycle_time);
    } else
        ret = rtos_task_init(&tdma->tx_task, task, (int)rtdev,
                             TDMA_PRIO_TX_TASK);
    if (ret != 0)
        TDMA_DEBUG(0, "RTmac: tdma: %s() not successful\n",__FUNCTION__);
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
        rtos_task_wait_period();

        /*
         * transmit packet
         */
        rtmac_xmit(skb);
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
    rtos_time_t time_stamp;

    max = TDMA_MASTER_MAX_TEST;

    list_for_each(lh, &tdma->rt_list) {
        rt_entry = list_entry(lh, struct tdma_rt_entry, list);

        if (rt_entry->state != RT_RCVD_CONF )
            goto out;

        TDMA_DEBUG(4, "RTmac: tdma: %s() sending %d test packets to %u.%u.%u.%u\n",
            __FUNCTION__,max, NIPQUAD(rt_entry->arp.ip));

        for (i = 0; i < max; i++) {
            if (!(rt_entry->state == RT_RCVD_CONF || rt_entry->state == RT_RCVD_TEST))
                goto out;

            TDMA_DEBUG(6, "RTmac: tdma: %s() sending test packet #%d to %u.%u.%u.%u\n",
                __FUNCTION__,i, NIPQUAD(rt_entry->arp.ip));

            /*
             * alloc skb, and put counter and time into it....
             */
            skb = tdma_make_msg(rtdev, rt_entry->arp.dev_addr, REQUEST_TEST, data);
            rt_entry->counter = test_msg->counter = i;
            rt_entry->state = RT_SENT_TEST;
            rtos_get_time(&time_stamp);
            rt_entry->tx = test_msg->tx = rtos_time_to_nanosecs(&time_stamp);

            /*
             * transmit packet
             */
            rtmac_xmit(skb);

            /*
             * wait
             */
            rtos_task_wait_period();
        }
    }

    TDMA_DEBUG(3, "RTmac: tdma: %s() shutdown complete\n",__FUNCTION__);
    tdma->flags.task_active = 0;
    tdma->flags.shutdown_task = 0;

    tdma_timer_start_sent_test(tdma, TDMA_MASTER_WAIT_TEST);
    tdma_next_state(tdma, TDMA_MASTER_SENT_TEST);

    return;

out:
    TDMA_DEBUG(0, "RTmac: tdma: *** WARNING *** %s() received not ACK from station %d, IP %u.%u.%u.%u, going into DOWN state\n",
        __FUNCTION__,rt_entry->station, NIPQUAD(rt_entry->arp.ip));
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
    rtos_time_t time_stamp;
    nanosecs_t time_ns;
    unsigned long flags;

    while (tdma->flags.shutdown_task == 0) {
        /*
         * make, resp. get master skb
         */
        /*FIXME: rtskb_queue_empty(tdma->master_queue) enable to send real msgs...*/

        skb = tdma_make_msg(rtdev, NULL, START_OF_FRAME, &data);

        rtos_task_wait_period();

        if (!skb)
            continue;

        /* Store timestamp in SOF. I assume that there is enough space. */
        rtos_get_time(&time_stamp);
        time_ns = rtos_time_to_nanosecs(&time_stamp);
        *(nanosecs_t *)data = 0;
        skb->xmit_stamp = (nanosecs_t *)data;

        rtmac_xmit(skb);

        /* Calculate delta_t for the master by assuming the current
         * to be the virtual receiption time. Then inform all listings
         * tasks that the SOF has been sent.
         * -JK-
         */
        rtos_get_time(&time_stamp);
        time_ns -= rtos_time_to_nanosecs(&time_stamp);
        rtos_spin_lock_irqsave(&tdma->delta_t_lock, flags);
        tdma->delta_t = time_ns;
        rtos_spin_unlock_irqrestore(&tdma->delta_t_lock, flags);

        rtos_event_broadcast(&tdma->client_tx);

        /*
         * get client skb out of queue and send it
         */

        skb = rtskb_prio_dequeue(&tdma->tx_queue);
        if (skb)
            rtmac_xmit(skb);
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
        if (RTOS_EVENT_ERROR(rtos_event_wait(&tdma->client_tx))) {
            TDMA_DEBUG(0, "RTmac: tdma: %s() rt_sem_wait(client_tx) failed\n",
                       __FUNCTION__);
            break;
        }

        if (tdma->flags.shutdown_task != 0)
            break;

        rtos_task_sleep_until(&tdma->wakeup);

        skb = rtskb_prio_dequeue(&tdma->tx_queue);
        if (skb)
            rtmac_xmit(skb);
    }

    TDMA_DEBUG(2, "RTmac: tdma: %s() shutdown complete\n",__FUNCTION__);
    tdma->flags.task_active = 0;
    tdma->flags.shutdown_task = 0;
}
