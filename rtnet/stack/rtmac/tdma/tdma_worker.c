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
    struct tdma_job     *job, *prev_job;
    struct rtskb        *rtskb;
    rtos_time_t         time;
    unsigned long       flags;
    int                 ret;


    rtos_event_sem_wait(&tdma->worker_wakeup);
    if (test_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags))
        return;

    rtos_spin_lock_irqsave(&tdma->lock, flags);
    job = tdma->first_job;
    job->ref_count++;
    rtos_spin_unlock_irqrestore(&tdma->lock, flags);

    do {
//rtos_print("job = %d\n", job->id);
        if (job->id == WAIT_ON_SYNC)
            rtos_event_wait(&tdma->sync_event);
        else if (job->id >= 0) {
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

#ifdef CONFIG_RTNET_TDMA_MASTER
        } else if (job->id == XMIT_SYNC) {
            rtos_time_sum(&time, &tdma->current_cycle_start,
                          &tdma->cycle_period);
            rtos_task_sleep_until(&time);

            rtos_spin_lock_irqsave(&tdma->lock, flags);
            tdma->current_cycle++;
            rtos_time_sum(&tdma->current_cycle_start,
                &tdma->current_cycle_start, &tdma->cycle_period);
            rtos_spin_unlock_irqrestore(&tdma->lock, flags);

            tdma_xmit_sync_frame(tdma);

        } else if (job->id == BACKUP_SYNC) {
            rtos_time_sum(&time, &tdma->current_cycle_start,
                          &tdma->backup_sync_inc);
            rtos_task_sleep_until(&time);

            if (!test_and_clear_bit(TDMA_FLAG_RECEIVED_SYNC, &tdma->flags)) {
                rtos_spin_lock_irqsave(&tdma->lock, flags);
                tdma->current_cycle++;
                rtos_time_sum(&tdma->current_cycle_start,
                    &tdma->current_cycle_start, &tdma->cycle_period);
                rtos_spin_unlock_irqrestore(&tdma->lock, flags);

                tdma_xmit_sync_frame(tdma);

                set_bit(TDMA_FLAG_BACKUP_ACTIVE, &tdma->flags);
            } else
                clear_bit(TDMA_FLAG_BACKUP_ACTIVE, &tdma->flags);
#endif /* CONFIG_RTNET_TDMA_MASTER */

        } else if (job->id == XMIT_REQ_CAL) {
            struct rt_proc_call  *call;

            if ((REQUEST_CAL_JOB(job)->period == 1) ||
                (tdma->current_cycle % REQUEST_CAL_JOB(job)->period ==
                    REQUEST_CAL_JOB(job)->phasing)) {
                /* remove job until we get a reply */
                rtos_spin_lock_irqsave(&tdma->lock, flags);

                __list_del(job->entry.prev, job->entry.next);
                job->ref_count--;
                prev_job = tdma->current_job =
                    list_entry(job->entry.prev, struct tdma_job, entry);
                prev_job->ref_count++;
                tdma->job_list_revision++;

                rtos_spin_unlock_irqrestore(&tdma->lock, flags);

                rtos_time_sum(&time, &tdma->current_cycle_start,
                              &REQUEST_CAL_JOB(job)->offset);
                rtos_task_sleep_until(&time);

                ret = tdma_xmit_request_cal_frame(tdma,
                    tdma->current_cycle + REQUEST_CAL_JOB(job)->period,
                    REQUEST_CAL_JOB(job)->offset_ns);

                /* terminate call on error */
                if (ret < 0) {
                    rtos_spin_lock_irqsave(&tdma->lock, flags);
                    call = tdma->calibration_call;
                    tdma->calibration_call = NULL;
                    rtos_spin_unlock_irqrestore(&tdma->lock, flags);

                    if (call)
                        rtpc_complete_call(call, ret);
                }

                job = prev_job;
            }

#ifdef CONFIG_RTNET_TDMA_MASTER
        } else if (job->id == XMIT_RPL_CAL) {
            if (REPLY_CAL_JOB(job)->reply_cycle <= tdma->current_cycle) {
                /* remove the job */
                rtos_spin_lock_irqsave(&tdma->lock, flags);

                __list_del(REPLY_CAL_JOB(job)->head.entry.prev,
                           REPLY_CAL_JOB(job)->head.entry.next);
                job->ref_count--;
                prev_job = tdma->current_job =
                    list_entry(job->entry.prev, struct tdma_job, entry);
                prev_job->ref_count++;
                tdma->job_list_revision++;

                rtos_spin_unlock_irqrestore(&tdma->lock, flags);

                if (REPLY_CAL_JOB(job)->reply_cycle == tdma->current_cycle) {
                    rtos_time_sum(&time, &tdma->current_cycle_start,
                                  &REPLY_CAL_JOB(job)->reply_offset);
                    rtos_task_sleep_until(&time);

                    rtmac_xmit(REPLY_CAL_JOB(job)->reply_rtskb);
                } else {
                    /* cleanup if cycle already passed */
                    kfree_rtskb(REPLY_CAL_JOB(job)->reply_rtskb);
                }

                job = prev_job;
            }
#endif /* CONFIG_RTNET_TDMA_MASTER */

        } else {
            /*DEBUG*/rtos_print("TDMA: Unknown job %d\n", job->id);
        }

        rtos_spin_lock_irqsave(&tdma->lock, flags);

      continue_in_lock:
        job->ref_count--;
        job = tdma->current_job =
            list_entry(job->entry.next, struct tdma_job, entry);
        job->ref_count++;

        rtos_spin_unlock_irqrestore(&tdma->lock, flags);
    } while (!test_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags));

    rtos_spin_lock_irqsave(&tdma->lock, flags);
    job->ref_count--;
    rtos_spin_unlock_irqrestore(&tdma->lock, flags);
}
