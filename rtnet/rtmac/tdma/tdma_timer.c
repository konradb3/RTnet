/* tdma_timer.c
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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

#include <rtmac/tdma/tdma_event.h>
#include <rtmac/tdma/tdma_task.h>
#include <rtmac/tdma/tdma_timer.h>


void tdma_timer_start(struct timer_list *ptimer, unsigned long timeout, void *data, TIMER_CALLBACK callback)
{
    del_timer(ptimer);

    ptimer->data = (unsigned long)data;

    /*
     * For most architectures void * is the same as unsigned long, but
     * at least we try to use void * as long as possible. Since the
     * timer functions use unsigned long, we cast the function here
     */
    ptimer->function = (void (*)(unsigned long)) callback;
    ptimer->expires = jiffies + timeout;

    TDMA_DEBUG(6, "RTmac: tdma: timer set, now=%d, timeout=%d\n", (int) jiffies, (int) ptimer->expires);

    add_timer(ptimer);
}




static void tdma_timer_expired_rt_add(void *data)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)data;

    TDMA_DEBUG(6, "RTmac: tdma: %s() timer expired\n",__FUNCTION__);
    tdma_do_event(tdma, EXPIRED_ADD_RT, NULL);
}

void tdma_timer_start_rt_add(struct rtmac_tdma *tdma, unsigned long timeout)
{
    TDMA_DEBUG(6, "RTmac: tdma: %s() timer set\n",__FUNCTION__);
    tdma_timer_start(&tdma->rt_add_timer, timeout, (void *)tdma, tdma_timer_expired_rt_add);
}



static void tdma_timer_expired_master_wait(void *data)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)data;

    TDMA_DEBUG(6, "RTmac: tdma: %s() timer expired\n",__FUNCTION__);
    tdma_do_event(tdma, EXPIRED_MASTER_WAIT, NULL);
}

void tdma_timer_start_master_wait(struct rtmac_tdma *tdma, unsigned long timeout)
{
    TDMA_DEBUG(6, "RTmac: tdma: %s() timer set\n",__FUNCTION__);
    tdma_timer_start(&tdma->master_wait_timer, timeout, (void *)tdma, tdma_timer_expired_master_wait);
}



static void tdma_timer_expired_sent_conf(void *data)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)data;

    TDMA_DEBUG(6, "RTmac: tdma: %s() timer expired\n",__FUNCTION__);
    tdma_do_event(tdma, EXPIRED_MASTER_SENT_CONF, NULL);
}

void tdma_timer_start_sent_conf(struct rtmac_tdma *tdma, unsigned long timeout)
{
    TDMA_DEBUG(6, "RTmac: tdma: %s() timer set\n",__FUNCTION__);
    tdma_timer_start(&tdma->master_sent_conf_timer, timeout, (void *)tdma, tdma_timer_expired_sent_conf);
}



static void tdma_timer_expired_task_change(void *data)
{
    struct timer_task_change_data *task_change_data = (struct timer_task_change_data *)data;
    struct rtmac_tdma *tdma = task_change_data->tdma;
    void (*task) (int rtdev_id) = task_change_data->task;
    unsigned int cycle = task_change_data->cycle;

    tdma_task_change_con(tdma, task, cycle);

    kfree(data);
}

int tdma_timer_start_task_change(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle, unsigned long timeout)
{
    struct timer_task_change_data *task_change_data;
    int ret = 0;

    /*
     * alloc mem for data
     */
    task_change_data = kmalloc(sizeof(struct timer_task_change_data), GFP_KERNEL);
    if (task_change_data == NULL) {
        TDMA_DEBUG(0, "RTmac: tdma: %s() out of memory\n",__FUNCTION__);
        return -1;
    }
    memset(task_change_data, 0, sizeof(struct timer_task_change_data));

    /*
     * fillup mem with data
     */
    task_change_data->tdma = tdma;
    task_change_data->task = task;
    task_change_data->cycle = cycle;

    /*
     * start the timer
     */
    TDMA_DEBUG(6, "RTmac: tdma: %s() timer set\n",__FUNCTION__);
    tdma_timer_start(&tdma->task_change_timer, timeout, (void *)task_change_data, tdma_timer_expired_task_change);

    return ret;
}



static void tdma_timer_expired_sent_test(void *data)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)data;

    TDMA_DEBUG(6, "RTmac: tdma: %s() timer expired\n",__FUNCTION__);
    tdma_do_event(tdma, EXPIRED_MASTER_SENT_TEST, NULL);
}

void tdma_timer_start_sent_test(struct rtmac_tdma *tdma, unsigned long timeout)
{
    TDMA_DEBUG(6, "RTmac: tdma: %s() timer set\n",__FUNCTION__);
    tdma_timer_start(&tdma->master_sent_test_timer, timeout, (void *)tdma, tdma_timer_expired_sent_test);
}



static void tdma_timer_expired_sent_ack(void *data)
{
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)data;

    TDMA_DEBUG(6, "RTmac: tdma: %s() timer expired\n",__FUNCTION__);
    tdma_do_event(tdma, EXPIRED_CLIENT_SENT_ACK, NULL);
}

void tdma_timer_start_sent_ack(struct rtmac_tdma *tdma, unsigned long timeout)
{
    TDMA_DEBUG(6, "RTmac: tdma: %s() timer set\n",__FUNCTION__);
    tdma_timer_start(&tdma->client_sent_ack_timer, timeout, (void *)tdma, tdma_timer_expired_sent_ack);
}
