/***
 *
 *  rtmac/tdma/tdma_ioctl.c
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

#include <linux/module.h>
#include <asm/uaccess.h>

#include <tdma_chrdev.h>
#include <rtmac/tdma/tdma.h>


#ifdef CONFIG_RTNET_TDMA_MASTER
static int tdma_ioctl_master(struct rtnet_device *rtdev,
                             struct tdma_config *cfg)
{
    struct tdma_priv    *tdma;
    unsigned int        table_size;
    int                 ret;


    if (rtdev->mac_priv == NULL) {
        ret = rtmac_disc_attach(rtdev, &tdma_disc);
        if (ret < 0)
            return ret;
    }

    tdma = (struct tdma_priv *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC) {
        /* note: we don't clean up an unknown discipline */
        return -ENOTTY;
    }

    if (rtskb_pool_init(&tdma->cal_rtskb_pool,
                        cfg->args.master.max_cal_requests) !=
        cfg->args.master.max_cal_requests) {
        ret = -ENOMEM;
        goto err_out;
    }

    table_size = sizeof(struct tdma_slot *) *
        ((cfg->args.master.max_slot_id >= 1) ?
            cfg->args.master.max_slot_id + 1 : 2);

    tdma->slot_table = (struct tdma_slot **)kmalloc(table_size, GFP_KERNEL);
    if (!tdma->slot_table) {
        ret = -ENOMEM;
        goto err_out;
    }
    tdma->max_slot_id = cfg->args.master.max_slot_id;
    memset(tdma->slot_table, 0, table_size);

    set_bit(TDMA_FLAG_MASTER, &tdma->flags);
    rtos_nanosecs_to_time(cfg->args.master.cycle_period,
                          &tdma->cycle_period);
    tdma->sync_job.ref_count = 0;
    INIT_LIST_HEAD(&tdma->sync_job.entry);

    if (cfg->args.master.backup_sync_offset == 0)
        tdma->sync_job.id = XMIT_SYNC;
    else {
        set_bit(TDMA_FLAG_BACKUP_MASTER, &tdma->flags);
        tdma->sync_job.id = BACKUP_SYNC;
        rtos_nanosecs_to_time(cfg->args.master.backup_sync_offset,
                              &tdma->backup_sync_inc);
        rtos_time_sum(&tdma->backup_sync_inc, &tdma->backup_sync_inc,
                      &tdma->cycle_period);
    }

    tdma->first_job = &tdma->sync_job;

    rtos_get_time(&tdma->current_cycle_start);

    rtos_event_sem_signal(&tdma->worker_wakeup);

    return 0;

  err_out:
    rtmac_disc_detach(rtdev);
    return ret;
}
#endif /* CONFIG_RTNET_TDMA_MASTER */



#ifdef CONFIG_RTNET_TDMA_SLAVE
static int tdma_ioctl_slave(struct rtnet_device *rtdev,
                            struct tdma_config *cfg)
{
    struct tdma_priv    *tdma;
    unsigned int        table_size;
    int                 ret;


    if (rtdev->mac_priv == NULL) {
        ret = rtmac_disc_attach(rtdev, &tdma_disc);
        if (ret < 0)
            return ret;
    }

    tdma = (struct tdma_priv *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC) {
        /* note: we don't clean up an unknown discipline */
        return -ENOTTY;
    }

    table_size = sizeof(struct tdma_slot *) *
        ((cfg->args.slave.max_slot_id >= 1) ?
            cfg->args.slave.max_slot_id + 1 : 2);

    tdma->slot_table = (struct tdma_slot **)kmalloc(table_size, GFP_KERNEL);
    if (!tdma->slot_table) {
        ret = -ENOMEM;
        goto err_out;
    }
    tdma->max_slot_id = cfg->args.slave.max_slot_id;
    memset(tdma->slot_table, 0, table_size);

    tdma->wait_sync_job.id        = WAIT_ON_SYNC;
    tdma->wait_sync_job.ref_count = 0;
    INIT_LIST_HEAD(&tdma->wait_sync_job.entry);

    tdma->first_job = &tdma->wait_sync_job;

    rtos_event_sem_signal(&tdma->worker_wakeup);

    return 0;

  err_out:
    rtmac_disc_detach(rtdev);
    return ret;
}
#endif /* CONFIG_RTNET_TDMA_SLAVE */



static int tdma_ioctl_set_slot(struct rtnet_device *rtdev,
                               struct tdma_config *cfg)
{
    struct tdma_priv    *tdma;
    int                 id;
    struct tdma_slot    *slot, *old_slot;
    struct tdma_job     *job, *prev_job;
    unsigned long       flags;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct tdma_priv *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    id = cfg->args.set_slot.id;
    if (id > tdma->max_slot_id)
        return -EINVAL;

    slot = (struct tdma_slot *)kmalloc(sizeof(struct tdma_slot), GFP_KERNEL);
    if (!slot)
        return -ENOMEM;

    slot->head.id        = id;
    slot->head.ref_count = 0;
    slot->period         = cfg->args.set_slot.period;
    slot->phasing        = cfg->args.set_slot.phasing;
    slot->size           = cfg->args.set_slot.size;
    rtos_nanosecs_to_time(cfg->args.set_slot.offset, &slot->offset);
    rtskb_prio_queue_init(&slot->queue);

    old_slot = tdma->slot_table[id];
    if ((id == DEFAULT_NRT_SLOT) &&
        (old_slot == tdma->slot_table[DEFAULT_SLOT]))
        old_slot = NULL;

    if (!old_slot) {
        prev_job = tdma->first_job;
        job = list_entry(prev_job->entry.next, struct tdma_job, entry);
        while (job != tdma->first_job) {
            prev_job = job;
            if ((job->id >= 0) &&
                (RTOS_TIME_IS_BEFORE(&slot->offset, &SLOT_JOB(job)->offset) ||
                 (RTOS_TIME_EQUALS(&slot->offset, &SLOT_JOB(job)->offset) &&
                  (slot->head.id <= SLOT_JOB(job)->head.id))))
                break;
            job = list_entry(job->entry.next, struct tdma_job, entry);
        }

        rtos_spin_lock_irqsave(&tdma->lock, flags);

    } else {
        prev_job = list_entry(old_slot->head.entry.prev, struct tdma_job, entry);

        rtos_spin_lock_irqsave(&tdma->lock, flags);
        __list_del(old_slot->head.entry.prev, old_slot->head.entry.next);
    }

    list_add(&slot->head.entry, &prev_job->entry);
    tdma->slot_table[id] = slot;
    if ((id == DEFAULT_SLOT) &&
        (tdma->slot_table[DEFAULT_NRT_SLOT] == old_slot))
        tdma->slot_table[DEFAULT_NRT_SLOT] = slot;

    if (old_slot)
        while (old_slot->head.ref_count > 0) {
            rtos_spin_unlock_irqrestore(&tdma->lock, flags);
            set_current_state(TASK_UNINTERRUPTIBLE);
            schedule_timeout(HZ/10); /* wait 100 ms */
            rtos_spin_lock_irqsave(&tdma->lock, flags);
        }

    rtos_spin_unlock_irqrestore(&tdma->lock, flags);

    if (old_slot)
        kfree(old_slot);

    return 0;
}



int tdma_cleanup_slot(struct tdma_priv *tdma, int id)
{
    struct tdma_slot    *slot;
    struct rtskb        *rtskb;
    unsigned long       flags;


    slot = tdma->slot_table[id];

    if (!slot)
        return -EINVAL;

    rtos_spin_lock_irqsave(&tdma->lock, flags);

    __list_del(slot->head.entry.prev, slot->head.entry.next);

    if (id == DEFAULT_NRT_SLOT)
        tdma->slot_table[DEFAULT_NRT_SLOT] = tdma->slot_table[DEFAULT_SLOT];
    else {
        if ((id == DEFAULT_SLOT) &&
            (tdma->slot_table[DEFAULT_NRT_SLOT] == slot))
            tdma->slot_table[DEFAULT_NRT_SLOT] = NULL;
        tdma->slot_table[id] = NULL;
    }

    while (slot->head.ref_count > 0) {
        rtos_spin_unlock_irqrestore(&tdma->lock, flags);
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(HZ/10); /* wait 100 ms */
        rtos_spin_lock_irqsave(&tdma->lock, flags);
    }

    while ((rtskb = __rtskb_prio_dequeue(&slot->queue))) {
        rtos_spin_unlock_irqrestore(&tdma->lock, flags);
        kfree_rtskb(rtskb);
        rtos_spin_lock_irqsave(&tdma->lock, flags);
    }

    rtos_spin_unlock_irqrestore(&tdma->lock, flags);

    kfree(slot);

    return 0;
}



static int tdma_ioctl_remove_slot(struct rtnet_device *rtdev,
                                  struct tdma_config *cfg)
{
    struct tdma_priv    *tdma;
    int                 id;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct tdma_priv *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    id = cfg->args.remove_slot.id;
    if (id > tdma->max_slot_id)
        return -EINVAL;

    return tdma_cleanup_slot(tdma, id);
}



static int tdma_ioctl_detach(struct rtnet_device *rtdev)
{
    struct tdma_priv    *tdma;
    int                 ret;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct tdma_priv *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    ret = rtmac_disc_detach(rtdev);

    return ret;
}



int tdma_ioctl(struct rtnet_device *rtdev, unsigned int request,
               unsigned long arg)
{
    struct tdma_config  cfg;
    int                 ret;


    ret = copy_from_user(&cfg, (void *)arg, sizeof(cfg));
    if (ret != 0)
        return -EFAULT;

    down(&rtdev->nrt_sem);

    switch (request) {
#ifdef CONFIG_RTNET_TDMA_MASTER
        case TDMA_IOC_MASTER:
            ret = tdma_ioctl_master(rtdev, &cfg);
            break;
#endif
#ifdef CONFIG_RTNET_TDMA_SLAVE
        case TDMA_IOC_SLAVE:
            ret = tdma_ioctl_slave(rtdev, &cfg);
            break;
#endif
        case TDMA_IOC_SET_SLOT:
            ret = tdma_ioctl_set_slot(rtdev, &cfg);
            break;

        case TDMA_IOC_REMOVE_SLOT:
            ret = tdma_ioctl_remove_slot(rtdev, &cfg);
            break;

        case TDMA_IOC_DETACH:
            ret = tdma_ioctl_detach(rtdev);
            break;

        default:
            ret = -ENOTTY;
    }

    up(&rtdev->nrt_sem);

    return ret;
}
