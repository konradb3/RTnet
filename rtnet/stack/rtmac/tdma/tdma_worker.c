/***
 *
 *  rtmac/tdma/tdma_worker.c
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

#include <rtmac/rtmac_proto.h>
#include <rtmac/tdma/tdma_proto.h>


void tdma_worker(int arg)
{
    struct tdma_priv    *tdma = (struct tdma_priv *)arg;
    struct tdma_job     *job;
    struct rtskb        *rtskb;
    rtos_time_t         time;
    unsigned long       flags;


    rtos_event_sem_wait(&tdma->worker_wakeup);
    if (test_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags))
        return;

    rtos_spin_lock_irqsave(&tdma->lock, flags);
    job = tdma->first_job;
    job->ref_count++;
    rtos_spin_unlock_irqrestore(&tdma->lock, flags);

    do {
#ifdef CONFIG_RTNET_TDMA_SLAVE
        if (job->id == WAIT_ON_SYNC)
            rtos_event_wait(&tdma->sync_event);

        else
#endif
        if (job->id >= 0) {
            if ((SLOT_JOB(job)->period == 1) ||
                (tdma->current_cycle % SLOT_JOB(job)->period ==
                    SLOT_JOB(job)->phasing)) {
                rtos_time_sum(&time, &tdma->current_cycle_start,
                              &SLOT_JOB(job)->offset);
                rtos_task_sleep_until(&time);

                rtos_spin_lock_irqsave(&tdma->lock, flags);
                rtskb = __rtskb_prio_dequeue(&SLOT_JOB(job)->queue);
                if (!rtskb)
                    goto continue_in_lock;
                rtos_spin_unlock_irqrestore(&tdma->lock, flags);

                rtmac_xmit(rtskb);
            }
            /* else: skip entry for this cycle */

        } else
#ifdef CONFIG_RTNET_TDMA_MASTER
        if (job->id == XMIT_SYNC) {
            rtos_time_sum(&time, &tdma->current_cycle_start,
                          &tdma->cycle_period);
            rtos_task_sleep_until(&time);

            tdma->current_cycle++;
            rtos_time_sum(&tdma->current_cycle_start,
                &tdma->current_cycle_start, &tdma->cycle_period);
            tdma_xmit_sync_frame(tdma);

        } else if (job->id == BACKUP_SYNC) {
            rtos_time_sum(&time, &tdma->current_cycle_start,
                          &tdma->backup_sync_inc);
            rtos_task_sleep_until(&time);

            if (!test_and_clear_bit(TDMA_FLAG_RECEIVED_SYNC, &tdma->flags))
                tdma_xmit_sync_frame(tdma);

        } else
#endif
        /*if (job->id = XMIT_CAL_REQ) {

            xmit_cal_req();

        } else if (job->id = XMIT_CAL_RPL) {

        } else*/
            rtos_print("TDMA: Unknown job %d\n", job->id);

        rtos_spin_lock_irqsave(&tdma->lock, flags);

      continue_in_lock:
        job->ref_count--;
        job = list_entry(job->entry.next, struct tdma_job, entry);
        job->ref_count++;

        rtos_spin_unlock_irqrestore(&tdma->lock, flags);
    } while (!test_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags));

    rtos_spin_lock_irqsave(&tdma->lock, flags);
    job->ref_count--;
    rtos_spin_unlock_irqrestore(&tdma->lock, flags);
}
