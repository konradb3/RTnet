/***
 *
 *  rtnet_rtpc.c
 *
 *  RTnet - real-time networking subsystem
 *
 *  Copyright (C) 2003 Jan Kiszka <jan.kiszka@web.de>
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

#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtnet_rtpc.h>


static spinlock_t  pending_calls_lock = SPIN_LOCK_UNLOCKED;
static spinlock_t  processed_calls_lock = SPIN_LOCK_UNLOCKED;
static SEM         dispatch_sem;
static RT_TASK     dispatch_task;
static int         rtpc_srq;

LIST_HEAD(pending_calls);
LIST_HEAD(processed_calls);


#define __wait_event_interruptible_timeout(wq, condition, timeout, ret)     \
do {                                                                        \
    signed long __timeout;                                                  \
    wait_queue_t __wait;                                                    \
    init_waitqueue_entry(&__wait, current);                                 \
                                                                            \
    __timeout = timeout;                                                    \
    add_wait_queue(&wq, &__wait);                                           \
    for (;;) {                                                              \
        set_current_state(TASK_INTERRUPTIBLE);                              \
        if (condition)                                                      \
            break;                                                          \
        if (!signal_pending(current)) {                                     \
            if ((__timeout = schedule_timeout(__timeout)) == 0) {           \
                ret = -ETIME;                                               \
                break;                                                      \
            }                                                               \
            continue;                                                       \
        }                                                                   \
        ret = -ERESTARTSYS;                                                 \
        break;                                                              \
    }                                                                       \
    current->state = TASK_RUNNING;                                          \
    remove_wait_queue(&wq, &__wait);                                        \
} while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)            \
({                                                                          \
    int __ret = 0;                                                          \
    if (!(condition))                                                       \
        __wait_event_interruptible_timeout(wq, condition, timeout, __ret);  \
    __ret;                                                                  \
})



int rtpc_dispatch_call(rtpc_proc proc, unsigned int timeout,
                       void* priv_data, size_t priv_data_size,
                       rtpc_cleanup_proc cleanup_handler)
{
    struct rt_proc_call *call;
    unsigned long       flags;
    int                 ret;


    call = kmalloc(sizeof(struct rt_proc_call) + priv_data_size, GFP_KERNEL);
    if (call == NULL)
        return -ENOMEM;

    memcpy(call->priv_data, priv_data, priv_data_size);

    call->processed       = 0;
    call->proc            = proc;
    call->result          = 0;
    call->cleanup_handler = cleanup_handler;
    atomic_set(&call->ref_count, 2);    /* dispatcher + rt-procedure */
    init_waitqueue_head(&call->call_wq);

    flags = rt_spin_lock_irqsave(&pending_calls_lock);
    list_add_tail(&call->list_entry, &pending_calls);
    rt_spin_unlock_irqrestore(flags, &pending_calls_lock);

    rt_sem_signal(&dispatch_sem);

    if (timeout > 0)
        ret = wait_event_interruptible_timeout(call->call_wq,
            call->processed, (timeout * HZ) / 1000);
    else
        ret = wait_event_interruptible(call->call_wq, call->processed);
    if (ret == 0)
        ret = call->result;

    if (atomic_dec_and_test(&call->ref_count)) {
        if (call->cleanup_handler != NULL)
            call->cleanup_handler(call);
        kfree(call);
    }

    return ret;
}



static inline struct rt_proc_call *rtpc_dequeue_pending_call(void)
{
    unsigned long       flags;
    struct rt_proc_call *call;


    flags = rt_spin_lock_irqsave(&pending_calls_lock);
    call = (struct rt_proc_call *)pending_calls.next;
    list_del(&call->list_entry);
    rt_spin_unlock_irqrestore(flags, &pending_calls_lock);

    return call;
}



static inline void rtpc_queue_processed_call(struct rt_proc_call *call)
{
    unsigned long flags;


    flags = rt_spin_lock_irqsave(&processed_calls_lock);
    list_add_tail(&call->list_entry, &processed_calls);
    rt_spin_unlock_irqrestore(flags, &processed_calls_lock);

    rt_pend_linux_srq(rtpc_srq);
}



static inline struct rt_proc_call *rtpc_dequeue_processed_call(void)
{
    unsigned long flags;
    struct rt_proc_call *call;


    flags = rt_spin_lock_irqsave(&processed_calls_lock);
    if (!list_empty(&processed_calls)) {
        call = (struct rt_proc_call *)processed_calls.next;
        list_del(&call->list_entry);
    } else
        call = NULL;
    rt_spin_unlock_irqrestore(flags, &processed_calls_lock);

    return call;
}



static void rtpc_dispatch_handler(int arg)
{
    struct rt_proc_call *call;
    int                 ret;


    while (1) {
        rt_sem_wait(&dispatch_sem);

        call = rtpc_dequeue_pending_call();

        ret = call->proc(call);
        if (ret != -CALL_PENDING)
            rtpc_complete_call(call, ret);
    }
}



static void rtpc_srq_handler(void)
{
    struct rt_proc_call *call;


    while ((call = rtpc_dequeue_processed_call()) != NULL) {
        call->processed = 1;
        wake_up(&call->call_wq);

        if (atomic_dec_and_test(&call->ref_count)) {
            if (call->cleanup_handler != NULL)
                call->cleanup_handler(call);
            kfree(call);
        }
    }
}



void rtpc_complete_call(struct rt_proc_call *call, int result)
{
    call->result = result;
    rtpc_queue_processed_call(call);
}



void rtpc_complete_call_nrt(struct rt_proc_call *call, int result)
{
    call->processed = 1;
    wake_up(&call->call_wq);

    if (atomic_dec_and_test(&call->ref_count)) {
        if (call->cleanup_handler != NULL)
            call->cleanup_handler(call);
        kfree(call);
    }
}



int __init rtpc_init(void)
{
    int ret;


    rtpc_srq = rt_request_srq(0, rtpc_srq_handler, 0);
    if (rtpc_srq < 0)
        return rtpc_srq;

    rt_typed_sem_init(&dispatch_sem, 0, CNT_SEM);

    ret = rt_task_init(&dispatch_task, rtpc_dispatch_handler, 0, 4096,
                       RT_LOWEST_PRIORITY, 0, NULL);
    if (ret != 0)
        rt_free_srq(rtpc_srq);
    else
        rt_task_resume(&dispatch_task);

    return ret;
}



void rtpc_cleanup(void)
{
    rt_task_delete(&dispatch_task);
    rt_sem_delete(&dispatch_sem);
    rt_free_srq(rtpc_srq);
}
