/***
 *
 *  rtmac/tdma/tdma_module.c
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

#include <asm/semaphore.h>
#include <linux/init.h>
#include <linux/module.h>

#include <rtnet_sys.h>
#include <rtmac/tdma/tdma.h>
#include <rtmac/tdma/tdma_dev.h>
#include <rtmac/tdma/tdma_ioctl.h>
#include <rtmac/tdma/tdma_proto.h>
#include <rtmac/tdma/tdma_worker.h>


#ifdef CONFIG_PROC_FS
LIST_HEAD(tdma_devices);
DECLARE_MUTEX(tdma_nrt_lock);


int tdma_proc_read(char *buf, char **start, off_t offset, int count,
                    int *eof, void *data)
{
    struct tdma_priv *entry;
    RTNET_PROC_PRINT_VARS;


    RTNET_PROC_PRINT("Interface       API Device      Operation Mode\n");
    down(&tdma_nrt_lock);

    list_for_each_entry(entry, &tdma_devices, list_entry) {
        RTNET_PROC_PRINT("%-15s %-15s ", entry->rtdev->name,
                         entry->api_device.device_name);
        if (test_bit(TDMA_FLAG_MASTER, &entry->flags)) {
            if (test_bit(TDMA_FLAG_BACKUP_MASTER, &entry->flags))
                RTNET_PROC_PRINT("Backup ");
            RTNET_PROC_PRINT("Master\n");
        } else
            RTNET_PROC_PRINT("Slave\n");
    }

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

    rtos_spin_lock_init(&tdma->lock);

    if ((ret = rtos_event_sem_init(&tdma->worker_wakeup)) != 0)
        goto err_out1;
    if ((ret = rtos_event_init(&tdma->xmit_event)) != 0)
        goto err_out2;
    if ((ret = rtos_event_init(&tdma->sync_event)) != 0)
        goto err_out3;

    ret = tdma_dev_init(rtdev, tdma);
    if (ret < 0)
        goto err_out4;

    ret = rtos_task_init(&tdma->worker_task, tdma_worker, (int)tdma, DEF_WORKER_PRIO);
    if (ret != 0)
        goto err_out5;

    RTNET_MOD_INC_USE_COUNT;

#ifdef CONFIG_PROC_FS
    down(&tdma_nrt_lock);
    list_add(&tdma->list_entry, &tdma_devices);
    up(&tdma_nrt_lock);
#endif /* CONFIG_PROC_FS */

    return 0;


  err_out5:
    tdma_dev_release(tdma);

  err_out4:
    rtos_event_delete(&tdma->sync_event);

  err_out3:
    rtos_event_delete(&tdma->xmit_event);

  err_out2:
    rtos_event_sem_delete(&tdma->worker_wakeup);

  err_out1:
    return ret;
}



int tdma_detach(struct rtnet_device *rtdev, void *priv)
{
    struct tdma_priv    *tdma = (struct tdma_priv *)priv;
    int                 i;
    int                 ret;


    set_bit(TDMA_FLAG_SHUTDOWN, &tdma->flags);
    rtos_event_sem_signal(&tdma->worker_wakeup);

    rtos_event_broadcast(&tdma->sync_event);

    ret =  tdma_dev_release(tdma);
    if (ret < 0)
        return ret;

    rtos_event_delete(&tdma->sync_event);
    rtos_event_delete(&tdma->xmit_event);
    rtos_event_sem_delete(&tdma->worker_wakeup);

    if (tdma->slot_table) {
        for (i = 0; i <= tdma->max_slot_id; i++)
            tdma_cleanup_slot(tdma, i);
        kfree(tdma->slot_table);
    }

    rtos_task_delete(&tdma->worker_task);

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


    printk("RTmac/TDMA: init time division multiple access control mechanism\n");

    ret = rtmac_disc_register(&tdma_disc);
    if (ret < 0)
        return ret;

    return 0;
}



void tdma_release(void)
{
    rtmac_disc_deregister(&tdma_disc);

    printk("RTmac/TDMA: unloaded\n");
}



module_init(tdma_init);
module_exit(tdma_release);

MODULE_AUTHOR("Jan Kiszka");
MODULE_LICENSE("GPL");
