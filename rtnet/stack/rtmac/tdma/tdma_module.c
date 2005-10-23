/***
 *
 *  rtmac/tdma/tdma_module.c
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

#include <asm/div64.h>
#include <asm/semaphore.h>
#include <linux/init.h>
#include <linux/module.h>

#include <rtnet_sys.h>
#include <rtmac/rtmac_vnic.h>
#include <rtmac/tdma/tdma.h>
#include <rtmac/tdma/tdma_dev.h>
#include <rtmac/tdma/tdma_ioctl.h>
#include <rtmac/tdma/tdma_proto.h>
#include <rtmac/tdma/tdma_worker.h>


/* RTAI-specific: start scheduling timer */
#ifdef CONFIG_RTOS_STARTSTOP_TIMER
static int start_timer = 0;

MODULE_PARM(start_timer, "i");
MODULE_PARM_DESC(start_timer, "set to non-zero to start RTAI timer");
#endif

#ifdef CONFIG_PROC_FS
LIST_HEAD(tdma_devices);
DECLARE_MUTEX(tdma_nrt_lock);


int tdma_proc_read(char *buf, char **start, off_t offset, int count,
                    int *eof, void *data)
{
    struct tdma_priv    *entry;
    const char          *state;
#ifdef CONFIG_RTNET_TDMA_MASTER
    nanosecs_t          cycle;
#endif
    RTNET_PROC_PRINT_VARS(80);


    down(&tdma_nrt_lock);

    if (!RTNET_PROC_PRINT("Interface       API Device      Operation Mode  "
                          "Cycle   State\n"))
        goto done;

    list_for_each_entry(entry, &tdma_devices, list_entry) {
        if (!RTNET_PROC_PRINT("%-15s %-15s ", entry->rtdev->name,
                              entry->api_device.device_name))
            break;
        if (test_bit(TDMA_FLAG_CALIBRATED, &entry->flags)) {
#ifdef CONFIG_RTNET_TDMA_MASTER
            if (test_bit(TDMA_FLAG_BACKUP_MASTER, &entry->flags) &&
                !test_bit(TDMA_FLAG_BACKUP_ACTIVE, &entry->flags))
                state = "stand-by";
            else
#endif /* CONFIG_RTNET_TDMA_MASTER */
                state = "active";
        } else
            state = "init";
#ifdef CONFIG_RTNET_TDMA_MASTER
        if (test_bit(TDMA_FLAG_MASTER, &entry->flags)) {
            cycle = entry->cycle_period + 500;
            do_div(cycle, 1000);
            if (test_bit(TDMA_FLAG_BACKUP_MASTER, &entry->flags)) {
                if (!RTNET_PROC_PRINT("Backup Master   %-7ld %s\n",
                                      (unsigned long)cycle, state))
                    break;
            } else {
                if (!RTNET_PROC_PRINT("Master          %-7ld %s\n",
                                      (unsigned long)cycle, state))
                    break;
            }
        } else
#endif /* CONFIG_RTNET_TDMA_MASTER */
            if (!RTNET_PROC_PRINT("Slave           -       %s\n", state))
                break;
    }

  done:
    up(&tdma_nrt_lock);

    RTNET_PROC_PRINT_DONE;
}



int tdma_slots_proc_read(char *buf, char **start, off_t offset, int count,
                         int *eof, void *data)
{
    struct tdma_priv    *entry;
    struct tdma_slot    *slot;
    int                 i;
    nanosecs_t          slot_offset;
    RTNET_PROC_PRINT_VARS(80);


    down(&tdma_nrt_lock);

    if (!RTNET_PROC_PRINT("Interface       "
                          "Slots (id:offset:phasing/period:size)\n"))
        goto done;

    list_for_each_entry(entry, &tdma_devices, list_entry) {
        if (!RTNET_PROC_PRINT("%-15s ", entry->rtdev->name))
            break;

#ifdef CONFIG_RTNET_TDMA_MASTER
        if (test_bit(TDMA_FLAG_BACKUP_MASTER, &entry->flags)) {
            slot_offset = entry->backup_sync_inc - entry->cycle_period + 500;
            do_div(slot_offset, 1000);
            if (!RTNET_PROC_PRINT("bak:%ld  ", (unsigned long)slot_offset))
                break;
        }
#endif /* CONFIG_RTNET_TDMA_MASTER */

        if (entry->slot_table) {
            if (down_interruptible(&entry->rtdev->nrt_lock))
                break;

            for (i = 0; i <= entry->max_slot_id; i++) {
                slot = entry->slot_table[i];
                if (!slot ||
                    ((i == DEFAULT_NRT_SLOT) &&
                     (entry->slot_table[DEFAULT_SLOT] == slot)))
                    continue;

                slot_offset = slot->offset + 500;
                do_div(slot_offset, 1000);
                if (!RTNET_PROC_PRINT("%d:%ld:%d/%d:%d  ", i,
                        (unsigned long)slot_offset, slot->phasing + 1,
                        slot->period, slot->mtu)) {
                    up(&entry->rtdev->nrt_lock);
                    goto done;
                }
            }

            up(&entry->rtdev->nrt_lock);
        }
        if (!RTNET_PROC_PRINT("\n"))
            break;
    }

  done:
    up(&tdma_nrt_lock);

    RTNET_PROC_PRINT_DONE;
}
#endif /* CONFIG_PROC_FS */



int tdma_attach(struct rtnet_device *rtdev, void *priv)
{
    struct tdma_priv   *tdma = (struct tdma_priv *)priv;
    int                 ret;


    memset(tdma, 0, sizeof(struct tdma_priv));

    tdma->magic        = TDMA_MAGIC;
    tdma->rtdev        = rtdev;

    rtdm_lock_init(&tdma->lock);

    rtdm_event_init(&tdma->worker_wakeup, 0);
    rtdm_event_init(&tdma->xmit_event, 0);
    rtdm_event_init(&tdma->sync_event, 0);

    ret = tdma_dev_init(rtdev, tdma);
    if (ret < 0)
        goto err_out1;

    ret = rtdm_task_init(&tdma->worker_task, "rtnet-tdma", tdma_worker, tdma,
                         DEF_WORKER_PRIO, 0);
    if (ret != 0)
        goto err_out2;

    RTNET_MOD_INC_USE_COUNT;

#ifdef CONFIG_PROC_FS
    down(&tdma_nrt_lock);
    list_add(&tdma->list_entry, &tdma_devices);
    up(&tdma_nrt_lock);
#endif /* CONFIG_PROC_FS */

    return 0;


  err_out2:
    tdma_dev_release(tdma);

  err_out1:
    rtdm_event_destroy(&tdma->sync_event);
    rtdm_event_destroy(&tdma->xmit_event);
    rtdm_event_destroy(&tdma->worker_wakeup);

    return ret;
}



int tdma_detach(struct rtnet_device *rtdev, void *priv)
{
    struct tdma_priv    *tdma = (struct tdma_priv *)priv;
    struct tdma_job     *job;
    rtdm_lockctx_t      context;
    int                 ret;


    set_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags);

    rtdm_event_destroy(&tdma->sync_event);
    rtdm_event_destroy(&tdma->xmit_event);
    rtdm_event_destroy(&tdma->worker_wakeup);

    ret =  tdma_dev_release(tdma);
    if (ret < 0)
        return ret;

    list_for_each_entry(job, &tdma->first_job->entry, entry) {
        if (job->id >= 0)
            tdma_cleanup_slot(tdma, SLOT_JOB(job));
        else if (job->id == XMIT_RPL_CAL) {
            rtdm_lock_get_irqsave(&tdma->lock, context);

            __list_del(job->entry.prev, job->entry.next);

            while (job->ref_count > 0) {
                rtdm_lock_put_irqrestore(&tdma->lock, context);
                set_current_state(TASK_UNINTERRUPTIBLE);
                schedule_timeout(HZ/10); /* wait 100 ms */
                rtdm_lock_get_irqsave(&tdma->lock, context);
            }

            kfree_rtskb(REPLY_CAL_JOB(job)->reply_rtskb);

            rtdm_lock_put_irqrestore(&tdma->lock, context);
        }
    }

    rtdm_task_destroy(&tdma->worker_task);

    if (tdma->slot_table)
        kfree(tdma->slot_table);

#ifdef CONFIG_RTNET_TDMA_MASTER
    rtskb_pool_release(&tdma->cal_rtskb_pool);
#endif

    RTNET_MOD_DEC_USE_COUNT;

#ifdef CONFIG_PROC_FS
    down(&tdma_nrt_lock);
    list_del(&tdma->list_entry);
    up(&tdma_nrt_lock);
#endif /* CONFIG_PROC_FS */

    return 0;
}



#ifdef CONFIG_PROC_FS
struct rtmac_proc_entry tdma_proc_entries[] = {
    { name: "tdma", handler: tdma_proc_read },
    { name: "tdma_slots", handler: tdma_slots_proc_read },
    { name: NULL, handler: NULL }
};
#endif /* CONFIG_PROC_FS */

struct rtmac_disc tdma_disc = {
    name:           "TDMA",
    priv_size:      sizeof(struct tdma_priv),
    disc_type:      __constant_htons(RTMAC_TYPE_TDMA),

    packet_rx:      tdma_packet_rx,
    rt_packet_tx:   tdma_rt_packet_tx,
    nrt_packet_tx:  tdma_nrt_packet_tx,

    get_mtu:        tdma_get_mtu,

    vnic_xmit:      RTMAC_DEFAULT_VNIC,

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


    printk("RTmac/TDMA: init time division multiple access control "
           "mechanism\n");

    ret = rtmac_disc_register(&tdma_disc);
    if (ret < 0)
        return ret;

#ifdef CONFIG_RTOS_STARTSTOP_TIMER
    if (start_timer)
        rtos_timer_start();
#endif

    return 0;
}



void tdma_release(void)
{
    rtmac_disc_deregister(&tdma_disc);

#ifdef CONFIG_RTOS_STARTSTOP_TIMER
    if (start_timer)
        rtos_timer_stop();
#endif

    printk("RTmac/TDMA: unloaded\n");
}



module_init(tdma_init);
module_exit(tdma_release);

MODULE_AUTHOR("Jan Kiszka");
MODULE_LICENSE("GPL");
