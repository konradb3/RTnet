/***
 *
 *  include/rtmac/tdma/tdma.h
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

#define CONFIG_RTNET_TDMA_MASTER
#define CONFIG_RTNET_TDMA_SLAVE

#ifndef __TDMA_H_
#define __TDMA_H_

#include <rtdm_driver.h>

#include <rtnet_config.h>
#include <rtnet_rtpc.h>
#include <rtmac/rtmac_disc.h>


#define RTMAC_TYPE_TDMA         0x0001

#define TDMA_MAGIC              0x3A0D4D0A

#define TDMA_FLAG_SHUTDOWN      0
#define TDMA_FLAG_CALIBRATED    1
#define TDMA_FLAG_RECEIVED_SYNC 2
#define TDMA_FLAG_MASTER        3   /* also set for backup masters */
#define TDMA_FLAG_BACKUP_MASTER 4
#define TDMA_FLAG_ATTACHED      5

#define DEFAULT_SLOT            0
#define DEFAULT_NRT_SLOT        1

/* job IDs */
#define WAIT_ON_SYNC            -1
#define XMIT_SYNC               -2
#define BACKUP_SYNC             -3
#define XMIT_REQ_CAL            -4
#define XMIT_RPL_CAL            -5


struct tdma_priv;


struct tdma_job {
    struct list_head            entry;
    int                         id;
    unsigned int                ref_count;
};


#define SLOT_JOB(job)           ((struct tdma_slot *)(job))

struct tdma_slot {
    struct tdma_job             head;

    rtos_time_t                 offset;
    unsigned int                period;
    unsigned int                phasing;
    unsigned int                size;
    struct rtskb_prio_queue     queue;
};


#define REQUEST_CAL_JOB(job)    ((struct tdma_request_cal *)(job))

struct tdma_request_cal {
    struct tdma_job             head;

    struct tdma_priv            *tdma;
    rtos_time_t                 offset;
    nanosecs_t                  offset_ns;
    unsigned int                period;
    unsigned int                phasing;
    unsigned int                cal_rounds;
    nanosecs_t                  *cal_results;
    nanosecs_t                  *result_buffer;
};


#define REPLY_CAL_JOB(job)      ((struct tdma_reply_cal *)(job))

struct tdma_reply_cal {
    struct tdma_job             head;

    u32                         reply_cycle;
    rtos_time_t                 reply_offset;
    struct rtskb                *reply_rtskb;
};

struct tdma_priv {
    unsigned int                magic;
    struct rtnet_device         *rtdev;
    struct rtdm_device          api_device;

#ifdef ALIGN_RTOS_TASK
    __u8                        __align[(ALIGN_RTOS_TASK -
                                         ((sizeof(unsigned int) +
                                           sizeof(struct rtnet_device *) +
                                           sizeof(struct rtdm_device)
                                          ) & (ALIGN_RTOS_TASK-1))
                                         ) & (ALIGN_RTOS_TASK-1)];
#endif
    rtos_task_t                 worker_task;
    rtos_event_sem_t            worker_wakeup;
    rtos_event_t                xmit_event;
    rtos_event_t                sync_event;

    unsigned long               flags;
    unsigned int                cal_rounds;
    u32                         current_cycle;
    rtos_time_t                 current_cycle_start;
    nanosecs_t                  master_packet_delay_ns;
    rtos_time_t                 clock_offset;

    struct tdma_job             sync_job;
    struct tdma_job             *first_job;
    struct tdma_job             *current_job;
    volatile unsigned int       job_list_revision;

    unsigned int                max_slot_id;
    struct tdma_slot            **slot_table;

    struct rt_proc_call         *calibration_call;
    unsigned char               master_hw_addr[MAX_ADDR_LEN];

    rtos_spinlock_t             lock;

#ifdef CONFIG_RTNET_TDMA_MASTER
    struct rtskb_queue          cal_rtskb_pool;
    rtos_time_t                 cycle_period;
    rtos_time_t                 backup_sync_inc;
#endif

#ifdef CONFIG_PROC_FS
    struct list_head            list_entry;
#endif
};


extern struct rtmac_disc        tdma_disc;

#define print_jobs()            do { \
    struct tdma_job *entry; \
    rtos_print("%s:%d - ", __FUNCTION__, __LINE__); \
    list_for_each_entry(entry, &tdma->first_job->entry, entry) \
        rtos_print("%d ", entry->id); \
    rtos_print("\n"); \
} while (0)

#endif /* __TDMA_H_ */
