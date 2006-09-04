/***
 *
 *  rtmac/tdma/tdma_worker.c
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

#include <rtmac/rtmac_proto.h>
#include <rtmac/tdma/tdma_proto.h>


void tdma_worker(void *arg)
{
    struct tdma_priv    *tdma = (struct tdma_priv *)arg;
    struct tdma_job     *job, *prev_job;
    struct rtskb        *rtskb;
    rtdm_lockctx_t      context;
    int                 ret;


    rtdm_event_wait(&tdma->worker_wakeup);
    if (test_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags))
        return;

    rtdm_lock_get_irqsave(&tdma->lock, context);
    job = tdma->first_job;
    job->ref_count++;
    rtdm_lock_put_irqrestore(&tdma->lock, context);

    do {
        if (job->id == WAIT_ON_SYNC)
            rtdm_event_wait(&tdma->sync_event);
        else if (job->id >= 0) {
            if ((SLOT_JOB(job)->period == 1) ||
                (tdma->current_cycle % SLOT_JOB(job)->period ==
                        SLOT_JOB(job)->phasing)) {
                /* wait for slot begin, then send one pending packet */
                rtdm_task_sleep_until(tdma->current_cycle_start +
                                      SLOT_JOB(job)->offset);
                rtdm_lock_get_irqsave(&tdma->lock, context);
                rtskb = __rtskb_prio_dequeue(SLOT_JOB(job)->queue);
                if (!rtskb)
                    goto continue_in_lock;
                rtdm_lock_put_irqrestore(&tdma->lock, context);

                rtmac_xmit(rtskb);
            }
            /* else: skip entry for this cycle */

#ifdef CONFIG_RTNET_TDMA_MASTER
        } else if (job->id == XMIT_SYNC) {
            /* wait for beginning of next cycle, then send sync */
            rtdm_task_sleep_until(tdma->current_cycle_start +
                                  tdma->cycle_period);
            rtdm_lock_get_irqsave(&tdma->lock, context);
            tdma->current_cycle++;
            tdma->current_cycle_start += tdma->cycle_period;
            rtdm_lock_put_irqrestore(&tdma->lock, context);

            tdma_xmit_sync_frame(tdma);

        } else if (job->id == BACKUP_SYNC) {
            /* wait for backup slot */
            rtdm_task_sleep_until(tdma->current_cycle_start +
                    tdma->backup_sync_inc);

            /* take over sync transmission if all earlier masters failed */
            if (!test_and_clear_bit(TDMA_FLAG_RECEIVED_SYNC, &tdma->flags)) {
                rtdm_lock_get_irqsave(&tdma->lock, context);
                tdma->current_cycle++;
                tdma->current_cycle_start += tdma->cycle_period;
                rtdm_lock_put_irqrestore(&tdma->lock, context);

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
                rtdm_lock_get_irqsave(&tdma->lock, context);

                __list_del(job->entry.prev, job->entry.next);
                job->ref_count--;
                prev_job = tdma->current_job =
                    list_entry(job->entry.prev, struct tdma_job, entry);
                prev_job->ref_count++;
                tdma->job_list_revision++;

                rtdm_lock_put_irqrestore(&tdma->lock, context);

                rtdm_task_sleep_until(tdma->current_cycle_start +
                                      REQUEST_CAL_JOB(job)->offset);
                ret = tdma_xmit_request_cal_frame(tdma,
                        tdma->current_cycle + REQUEST_CAL_JOB(job)->period,
                        REQUEST_CAL_JOB(job)->offset);

                /* terminate call on error */
                if (ret < 0) {
                    rtdm_lock_get_irqsave(&tdma->lock, context);
                    call = tdma->calibration_call;
                    tdma->calibration_call = NULL;
                    rtdm_lock_put_irqrestore(&tdma->lock, context);

                    if (call)
                        rtpc_complete_call(call, ret);
                }

                job = prev_job;
            }

#ifdef CONFIG_RTNET_TDMA_MASTER
        } else if (job->id == XMIT_RPL_CAL) {
            if (REPLY_CAL_JOB(job)->reply_cycle <= tdma->current_cycle) {
                /* remove the job */
                rtdm_lock_get_irqsave(&tdma->lock, context);

                __list_del(REPLY_CAL_JOB(job)->head.entry.prev,
                           REPLY_CAL_JOB(job)->head.entry.next);
                job->ref_count--;
                prev_job = tdma->current_job =
                    list_entry(job->entry.prev, struct tdma_job, entry);
                prev_job->ref_count++;
                tdma->job_list_revision++;

                rtdm_lock_put_irqrestore(&tdma->lock, context);

                if (REPLY_CAL_JOB(job)->reply_cycle == tdma->current_cycle) {
                    /* send reply in the assigned slot */
                    rtdm_task_sleep_until(tdma->current_cycle_start +
                                          REPLY_CAL_JOB(job)->reply_offset);
                    rtmac_xmit(REPLY_CAL_JOB(job)->reply_rtskb);
                } else {
                    /* cleanup if cycle already passed */
                    kfree_rtskb(REPLY_CAL_JOB(job)->reply_rtskb);
                }

                job = prev_job;
            }
#endif /* CONFIG_RTNET_TDMA_MASTER */
        }

        rtdm_lock_get_irqsave(&tdma->lock, context);

      continue_in_lock:
        job->ref_count--;
        job = tdma->current_job =
            list_entry(job->entry.next, struct tdma_job, entry);
        job->ref_count++;

        rtdm_lock_put_irqrestore(&tdma->lock, context);
    } while (!test_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags));

    rtdm_lock_get_irqsave(&tdma->lock, context);
    job->ref_count--;
    rtdm_lock_put_irqrestore(&tdma->lock, context);
}
