/***
 *
 *  rtmac/tdma/tdma_proto.c
 *
 *  RTmac - real-time networking media access control subsystem
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

#include <rtdev.h>
#include <rtmac/rtmac_proto.h>
#include <rtmac/tdma/tdma_proto.h>


void tdma_xmit_sync_frame(struct tdma_priv *tdma)
{
    struct rtnet_device     *rtdev = tdma->rtdev;
    struct rtskb            *rtskb;
    struct tdma_frm_sync    *sync;


    rtskb = alloc_rtskb(rtdev->hard_header_len + sizeof(struct rtmac_hdr) +
                        sizeof(struct tdma_frm_sync) + 15, &global_pool);
    if (!rtskb)
        goto err_out;

    rtskb_reserve(rtskb,
        (rtdev->hard_header_len + sizeof(struct rtmac_hdr) + 15) & ~15);

    sync = (struct tdma_frm_sync *)rtskb_put(rtskb,
                                             sizeof(struct tdma_frm_sync));

    if (rtmac_add_header(rtdev, rtdev->broadcast,
                         rtskb, RTMAC_TYPE_TDMA, 0) < 0) {
        kfree_rtskb(rtskb);
        goto err_out;
    }

    sync->head.version = __constant_htons(TDMA_FRM_VERSION);
    sync->head.id      = __constant_htons(TDMA_FRM_SYNC);

    sync->cycle_no         = htons(tdma->current_cycle);
    sync->xmit_stamp       = rtos_time_to_nanosecs(&tdma->clock_offset);
    sync->sched_xmit_stamp =
        cpu_to_be64(rtos_time_to_nanosecs(&tdma->current_cycle_start));

    rtskb->xmit_stamp = &sync->xmit_stamp;

    rtmac_xmit(rtskb);

    return;

  err_out:
    /*ERROR*/rtos_print("TDMA: Failed to transmit sync frame!\n");
    return;
}


int tdma_rt_packet_tx(struct rtskb *rtskb, struct rtnet_device *rtdev)
{
    struct tdma_priv    *tdma;
    unsigned long       flags;
    struct tdma_slot    *slot;


    tdma = (struct tdma_priv *)rtdev->mac_priv->disc_priv;

    rtcap_mark_rtmac_enqueue(rtskb);

    rtos_spin_lock_irqsave(&tdma->lock, flags);

// HACK
slot = tdma->slot_table[DEFAULT_SLOT];
    if (!slot) {
        rtos_spin_unlock_irqrestore(&tdma->lock, flags);
        return -EAGAIN;
    }

    __rtskb_prio_queue_tail(&slot->queue, rtskb);

    rtos_spin_unlock_irqrestore(&tdma->lock, flags);

    return 0;
}



int tdma_nrt_packet_tx(struct rtskb *rtskb)
{
    struct tdma_priv    *tdma;
    unsigned long       flags;
    struct tdma_slot    *slot;


    tdma = (struct tdma_priv *)rtskb->rtdev->mac_priv->disc_priv;

    rtcap_mark_rtmac_enqueue(rtskb);

    rtskb->priority = QUEUE_MIN_PRIO;

    rtos_spin_lock_irqsave(&tdma->lock, flags);

// HACK
slot = tdma->slot_table[DEFAULT_NRT_SLOT];
    if (!slot) {
        rtos_spin_unlock_irqrestore(&tdma->lock, flags);
        return -EAGAIN;
    }

    __rtskb_prio_queue_tail(&slot->queue, rtskb);

    rtos_spin_unlock_irqrestore(&tdma->lock, flags);

    return 0;
}



int tdma_packet_rx(struct rtskb *rtskb)
{
    struct tdma_priv        *tdma;
    struct tdma_frm_head    *head;


    tdma = (struct tdma_priv *)rtskb->rtdev->mac_priv->disc_priv;

    head = (struct tdma_frm_head *)rtskb->data;

    if (head->version != __constant_htons(TDMA_FRM_VERSION))
        goto kfree_out;

    switch (head->id) {
        case __constant_htons(TDMA_FRM_SYNC):
            rtskb_pull(rtskb, sizeof(struct tdma_frm_sync));

            tdma->current_cycle = ntohs(SYNC_FRM(head)->cycle_no);

            /* see "Time Arithmetics" in the TDMA specification */
            rtos_nanosecs_to_time(be64_to_cpu(SYNC_FRM(head)->xmit_stamp) +
                                  tdma->master_packet_delay_ns,
                                  &tdma->clock_offset);
            rtos_time_diff(&tdma->clock_offset,
                           &tdma->clock_offset,
                           &rtskb->time_stamp);

            rtos_nanosecs_to_time(
                be64_to_cpu(SYNC_FRM(head)->sched_xmit_stamp),
                &tdma->current_cycle_start);
            rtos_time_diff(&tdma->current_cycle_start,
                           &tdma->current_cycle_start,
                           &tdma->clock_offset);

            set_bit(TDMA_FLAG_RECEIVED_SYNC, &tdma->flags);

            rtos_event_broadcast(&tdma->sync_event);
            break;

        default:
            /*ERROR*/rtos_print("TDMA: Unknown frame %d!\n", ntohs(head->id));
    }

  kfree_out:
    kfree_rtskb(rtskb);
    return 0;
}
