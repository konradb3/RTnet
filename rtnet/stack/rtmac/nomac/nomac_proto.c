/***
 *
 *  rtmac/nomac/nomac_proto.c
 *
 *  RTmac - real-time networking media access control subsystem
 *  Copyright (C) 2002      Marc Kleine-Budde <kleine-budde@gmx.de>,
 *                2003-2005 Jan Kiszka <Jan.Kiszka@web.de>
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

#include <rtdev.h>
#include <rtmac/rtmac_proto.h>
#include <rtmac/nomac/nomac.h>


static struct rtskb_queue   nrt_rtskb_queue;
static rtos_task_t          wrapper_task;
static rtos_event_t         wakeup_sem;
static int                  shutdown;


int nomac_rt_packet_tx(struct rtskb *rtskb, struct rtnet_device *rtdev)
{
    struct nomac_priv   *nomac;
    int                 ret;


    nomac = (struct nomac_priv *)rtdev->mac_priv->disc_priv;

    rtcap_mark_rtmac_enqueue(rtskb);

    /* no MAC: we simply transmit the packet under xmit_lock */
    rtos_res_lock(&rtdev->xmit_lock);
    ret = rtmac_xmit(rtskb);
    rtos_res_unlock(&rtdev->xmit_lock);

    return ret;
}



int nomac_nrt_packet_tx(struct rtskb *rtskb)
{
    struct nomac_priv   *nomac;
    struct rtnet_device *rtdev;
    int                 ret;


    nomac = (struct nomac_priv *)rtskb->rtdev->mac_priv->disc_priv;

    rtcap_mark_rtmac_enqueue(rtskb);

    /* note: this routine may be called both in rt and non-rt context
     *       => detect and wrap the context if necessary */
    if (!rtos_in_rt_context()) {
        rtskb_queue_tail(&nrt_rtskb_queue, rtskb);
        rtos_event_signal(&wakeup_sem);
        return 0;
    } else {
        rtdev = rtskb->rtdev;

        /* no MAC: we simply transmit the packet under xmit_lock */
        rtos_res_lock(&rtdev->xmit_lock);
        ret = rtmac_xmit(rtskb);
        rtos_res_unlock(&rtdev->xmit_lock);

        return ret;
    }
}



void nrt_xmit_task(void *arg)
{
    struct rtskb        *rtskb;
    struct rtnet_device *rtdev;


    while (!shutdown) {
        while ((rtskb = rtskb_dequeue(&nrt_rtskb_queue))) {
            rtdev = rtskb->rtdev;

            /* no MAC: we simply transmit the packet under xmit_lock */
            rtos_res_lock(&rtdev->xmit_lock);
            rtmac_xmit(rtskb);
            rtos_res_unlock(&rtdev->xmit_lock);
        }
        rtos_event_wait(&wakeup_sem, 0);
    }
}



int nomac_packet_rx(struct rtskb *rtskb)
{
    /* actually, NoMAC doesn't expect any control packet */
    kfree_rtskb(rtskb);

    return 0;
}



int __init nomac_proto_init(void)
{
    int ret;


    rtskb_queue_init(&nrt_rtskb_queue);
    rtos_event_init(&wakeup_sem);

    ret = rtos_task_init(&wrapper_task, nrt_xmit_task, 0,
                         RTOS_LOWEST_RT_PRIORITY);
    if (ret < 0) {
        rtos_event_delete(&wakeup_sem);
        return ret;
    }

    return 0;
}



void nomac_proto_cleanup(void)
{
    shutdown = 1;
    rtos_event_delete(&wakeup_sem);
    rtos_task_delete(&wrapper_task);
}
