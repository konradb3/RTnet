/***
 *
 *  include/rtnet_sys_rai.h
 *
 *  RTnet - real-time networking subsystem
 *          RTOS abstraction layer - RTAI/fusion version
 *
 *  Copyright (C) 2004 Jan Kiszka <jan.kiszka@web.de>
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

#ifndef __RTNET_SYS_FUSION_H_
#define __RTNET_SYS_FUSION_H_

#include <nucleus/heap.h>
#include <nucleus/pod.h>
#include <rtai/task.h>
#include <rtai/sem.h>
#include <rtai/mutex.h>
#include <rtai/pipe.h>

/* basic types */
typedef struct {
    RTIME          val;               /* high precision time */
} rtos_time_t;
typedef spinlock_t rtos_spinlock_t;   /* spin locks with hard IRQ locks */
typedef RT_TASK    rtos_task_t;       /* hard real-time task */
typedef RT_SEM     rtos_event_t;      /* to signal events (non-storing) */
typedef RT_SEM     rtos_event_sem_t;  /* to signal events (storing) */
typedef RT_MUTEX   rtos_res_lock_t;   /* resource lock with prio inheritance */
typedef int        rtos_nrt_signal_t; /* async signal to non-RT world */
typedef RT_PIPE    rtos_fifo_t;       /* fifo descriptor */

#define ALIGN_RTOS_TASK   16  /* RT_TASK requires 16-bytes alignment */
//#define NR_RT_CPUS        RTHAL_NR_CPUS


/* print output messages */
#define rtos_print              printk


/* time handling */
static inline void rtos_get_time(rtos_time_t *time)
{
    time->val = rt_timer_read();
}


static inline void rtos_nanosecs_to_time(nanosecs_t nano, rtos_time_t *time)
{
    time->val = (RTIME)rt_timer_ns2ticks((SRTIME)nano);
}

static inline nanosecs_t rtos_time_to_nanosecs(rtos_time_t *time)
{
    return (nanosecs_t)rt_timer_ticks2ns((SRTIME)time->val);
}


static inline void rtos_time_to_timeval(rtos_time_t *time,
                                        struct timeval *tval)
{
    tval->tv_sec = rthal_ulldiv(rt_timer_ticks2ns((SRTIME)time->val),
                                1000000000, (unsigned long *)&tval->tv_usec);
    tval->tv_usec /= 1000;
}


static inline void rtos_time_sum(rtos_time_t *result,
                                 rtos_time_t *a, rtos_time_t *b)
{
    result->val = a->val + b->val;
}


static inline void rtos_time_diff(rtos_time_t *result,
                                  rtos_time_t *a, rtos_time_t *b)
{
    result->val = a->val - b->val;
}

#define RTOS_TIME_IS_ZERO(time)     ((time)->val == 0)
#define RTOS_TIME_IS_BEFORE(a, b)   ((a)->val < (b)->val)
#define RTOS_TIME_EQUALS(a, b)      ((a)->val == (b)->val)



/* real-time spin locks */
#define RTOS_SPIN_LOCK_UNLOCKED     SPIN_LOCK_UNLOCKED  /* init */
#define rtos_spin_lock_init(lock)   spin_lock_init(lock)

#define rtos_spin_lock(lock)        rthal_spin_lock(lock)
#define rtos_spin_unlock(lock)      rthal_spin_unlock(lock)

#define rtos_spin_lock_irqsave(lock, flags) \
    (flags) = rthal_spin_lock_irqsave(lock)
#define rtos_spin_unlock_irqrestore(lock, flags) \
    rthal_spin_unlock_irqrestore(flags, lock)

#define rtos_local_irqsave(flags)   \
    rthal_local_irq_save(flags)
#define rtos_local_irqrestore(flags) \
    rthal_local_irq_restore(flags)

#define rtos_saveflags(flags) \
    rthal_local_irq_flags(flags)



/* RT-tasks */
#define RTOS_LOWEST_RT_PRIORITY     T_LOPRIO

static inline int rtos_task_init(rtos_task_t *task, void (*task_proc)(int),
                                 int arg, int priority)
{
    int ret;

    ret = rt_task_create(task, NULL, 4096, priority, 0);

    if (ret)
        return ret;

    ret = rt_task_start(task, (void (*)(void *))task_proc, (void *)arg);

    if (ret)
        rt_task_delete(task);

    return ret;
}

static inline int rtos_task_init_periodic(rtos_task_t *task,
                                          void (*task_proc)(int), int arg,
                                          int priority, rtos_time_t *period)
{
    int ret;

    ret = rt_task_create(task, NULL, 4096, priority, 0);

    if (ret)
        return ret;

    ret = rt_task_set_periodic(task, TM_INFINITE, period->val);

    if (!ret)
        ret = rt_task_start(task, (void (*)(void *))task_proc, (void *)arg);

    if (ret)
        rt_task_delete(task);

    return ret;
}

static inline int rtos_task_init_suspended(rtos_task_t *task,
                                           void (*task_proc)(int),
                                           int arg, int priority)
{
    int ret;

    ret = rt_task_create(task, NULL, 4096, priority, T_SUSP);

    if (ret)
        return ret;

    ret = rt_task_start(task, (void (*)(void *))task_proc, (void *)arg);

    if (ret)
        rt_task_delete(task);

    return ret;
}

static inline int rtos_task_resume(rtos_task_t *task)
{
    return rt_task_resume(task);
}

static inline int rtos_task_wakeup(rtos_task_t *task)
{
    return rt_task_unblock(task);
}

static inline void rtos_task_delete(rtos_task_t *task)
{
    rt_task_delete(task);
}

static inline int rtos_task_set_priority(rtos_task_t *task, int priority)
{
    return rt_task_set_priority(task, priority);
}

#define CONFIG_RTOS_STARTSTOP_TIMER 1

static inline int rtos_timer_start_oneshot(void)
{
    return rt_timer_start(TM_ONESHOT);
}

static inline void rtos_timer_stop(void)
{
    rt_timer_stop();
}

#define rtos_task_wait_period()     rt_task_wait_period()
#define rtos_busy_sleep(nanosecs)   rt_timer_spin(nanosecs)

static inline void rtos_task_sleep_until(rtos_time_t *wakeup_time)
{
    rt_task_sleep_until(wakeup_time->val);
}


static inline int rtos_in_rt_context(void)
{
    return adp_current != adp_root; /* Ask Adeos. */
}



/* event signaling */
#define RTOS_EVENT_TIMEOUT          -ETIMEDOUT
#define RTOS_EVENT_ERROR(result)    ((result) < 0)

/* note: event is initially set to a non-signaled state */
static inline int rtos_event_init(rtos_event_t *event)
{
    return rt_sem_create(event, NULL, 0, S_FIFO);
}

/* note: event is initially set to a non-signaled state */
static inline int rtos_event_sem_init(rtos_event_sem_t *event)
{
    return rt_sem_create(event, NULL, 0, S_FIFO);
}

static inline void rtos_event_delete(rtos_event_t *event)
{
    rt_sem_delete(event);
}

static inline void rtos_event_sem_delete(rtos_event_sem_t *event)
{
    rt_sem_delete(event);
}


/* note: wakes all waiting tasks, does NOT store events if no one is
 *       listening */
static inline void rtos_event_broadcast(rtos_event_t *event)
{
    rt_sem_broadcast(event);
}

/* note: wakes up a single waiting task, must store events if no one is
 *       listening */
static inline void rtos_event_sem_signal(rtos_event_sem_t *event)
{
    rt_sem_v(event);
}


static inline int rtos_event_wait(rtos_event_sem_t *event)
{
    return rt_sem_p(event, TM_INFINITE);
}

static inline int rtos_event_sem_wait(rtos_event_sem_t *event)
{
    return rt_sem_p(event, TM_INFINITE);
}

static inline int rtos_event_sem_wait_timed(rtos_event_sem_t *event,
                                            rtos_time_t *timeout)
{
    return rt_sem_p(event, timeout->val);
}



/* resource locks */
static inline int rtos_res_lock_init(rtos_res_lock_t *lock)
{
    return rt_mutex_create(lock, NULL);
}

static inline int rtos_res_lock_delete(rtos_res_lock_t *lock)
{
    return rt_mutex_delete(lock);
}


static inline void rtos_res_lock(rtos_res_lock_t *lock)
{
    rt_mutex_lock(lock);
}

static inline void rtos_res_unlock(rtos_res_lock_t *lock)
{
    rt_mutex_unlock(lock);
}


/* non-RT signals */
static inline int rtos_nrt_signal_init(rtos_nrt_signal_t *nrt_sig,
                                       void (*handler)(void))
{
    *nrt_sig = adeos_alloc_irq();

    if (*nrt_sig > 0)
        adeos_virtualize_irq_from(adp_root,
                                  *nrt_sig,
                                  (void (*)(unsigned))handler,
                                  NULL,
                                  IPIPE_HANDLE_MASK);
    else
        *nrt_sig = -EBUSY;

    return *nrt_sig;
}

static inline void rtos_nrt_signal_delete(rtos_nrt_signal_t *nrt_sig)
{
    adeos_free_irq(*nrt_sig);
}


static inline void rtos_pend_nrt_signal(rtos_nrt_signal_t *nrt_sig)
{
    adeos_trigger_irq(*nrt_sig);
}


/* Fifo management */
static inline int rtos_fifo_create(rtos_fifo_t *fifo, int minor, int size)
{
    return rt_pipe_open(fifo, minor);
}

static inline void rtos_fifo_destroy(rtos_fifo_t *fifo)
{
    rt_pipe_close(fifo);
}

static inline int rtos_fifo_put(rtos_fifo_t *fifo, void *buf, int size)
{
    return rt_pipe_stream(fifo,buf,size);
}

/* RT memory management */
#define rtos_malloc(size)           xnmalloc(size) /* Ask the nucleus. */
#define rtos_free(buffer)           xnfree(buffer)



/* IRQ management */
static inline int rtos_irq_request(unsigned int irq,
    void (*handler)(unsigned int, void *), void *arg)
{
    return rthal_request_irq(irq, handler, arg);
}

static inline int rtos_irq_free(unsigned int irq)
{
    return rthal_release_irq(irq);
}


#define rtos_irq_enable(irq)        rthal_enable_irq(irq)
#define rtos_irq_disable(irq)       rthal_disable_irq(irq)
#define rtos_irq_end(irq)           rthal_enable_irq(irq)

static inline void rtos_irq_release_lock(void)
{
    rt_task_set_mode(0,T_LOCK,NULL);
    rthal_sti();
}

static inline void rtos_irq_reacquire_lock(void)
{
    rthal_cli();
    rt_task_set_mode(T_LOCK,0,NULL);
}


#endif /* __RTNET_SYS_FUSION_H_ */
