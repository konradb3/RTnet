/***
 *
 *  include/rtnet_sys_rai.h
 *
 *  RTnet - real-time networking subsystem
 *          RTOS abstraction layer - RTAI version
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

#ifndef __RTNET_SYS_RTAI_H_
#define __RTNET_SYS_RTAI_H_

#include <linux/spinlock.h>

#ifdef CONFIG_RTAI_24
#define INTERFACE_TO_LINUX  /* makes RT_LINUX_PRIORITY visible */
#endif

#include <rtai.h>
#include <rtai_sched.h>

#ifdef CONFIG_RTAI_30
#include <rtai_malloc.h>
#include <rtai_sem.h>
#endif

#ifdef CONFIG_PROC_FS
/* TODO: remove it! */
#include <rtai_proc_fs.h>
#endif


/* basic types */
typedef RTIME      rtos_time_t;       /* high precision time */
typedef spinlock_t rtos_spinlock_t;   /* spin locks with hard IRQ locks */
typedef RT_TASK    rtos_task_t;       /* hard real-time task */
typedef SEM        rtos_event_t;      /* to signal events (non-storing) */
typedef SEM        rtos_event_sem_t;  /* to signal events (storing) */
typedef SEM        rtos_res_lock_t;   /* resource lock with prio inheritance */
typedef int        rtos_nrt_signal_t; /* async signal to non-RT world */

#define ALIGN_RTOS_TASK         16  /* RT_TASK requires 16-bytes alignment */



/* print output messages */
#define rtos_print              rt_printk



/* time handling */
static inline void rtos_get_time(rtos_time_t *time)
{
    *time = rt_get_time();
}


static inline void rtos_nanosecs_to_time(nanosecs_t nano, rtos_time_t *time)
{
    *time = nano2count(nano);
}

static inline nanosecs_t rtos_time_to_nanosecs(rtos_time_t *time)
{
    return (nanosecs_t)count2nano(*time);
}


static inline void rtos_time_to_timeval(rtos_time_t *time,
                                        struct timeval *tval)
{
    count2timeval(*time, tval);
}


static inline void rtos_time_sum(rtos_time_t *result,
                                 rtos_time_t *a, rtos_time_t *b)
{
    *result = *a + *b;
}

#define RTOS_TIME_IS_ZERO(time)    (*(time) == 0)



/* real-time spin locks */
#define RTOS_SPIN_LOCK_UNLOCKED     SPIN_LOCK_UNLOCKED  /* init */
#define rtos_spin_lock_init(lock)   spin_lock_init(lock)

#define rtos_spin_lock(lock)        rt_spin_lock(lock)
#define rtos_spin_unlock(lock)      rt_spin_unlock(lock)

#define rtos_spin_lock_irqsave(lock, flags) \
    flags = rt_spin_lock_irqsave(lock)
#define rtos_spin_unlock_irqrestore(lock, flags) \
    rt_spin_unlock_irqrestore(flags, lock)



/* RT-tasks */
#ifdef CONFIG_RTAI_24
#define RTOS_LOWEST_RT_PRIORITY     RT_LOWEST_PRIORITY
#define RTOS_LINUX_PRIORITY         RT_LINUX_PRIORITY
#else
#define RTOS_LOWEST_RT_PRIORITY     RT_SCHED_LOWEST_PRIORITY
#define RTOS_LINUX_PRIORITY         RT_SCHED_LINUX_PRIORITY
#endif

static inline int rtos_task_init(rtos_task_t *task, void (*task_proc)(int),
                                 int arg, int priority)
{
    int ret;

    ret = rt_task_init(task, task_proc, arg, 4096, priority, 0, NULL);
    if (ret < 0)
        return ret;

    return rt_task_resume(task);
}

static inline int rtos_task_init_periodic(rtos_task_t *task,
                                          void (*task_proc)(int), int arg,
                                          int priority, rtos_time_t *period)
{
    int ret;

    ret = rt_task_init(task, task_proc, arg, 4096, priority, 0, NULL);
    if (ret < 0)
        return ret;

    return rt_task_make_periodic(task, rt_get_time(), *period);
}

static inline void rtos_task_delete(rtos_task_t *task)
{
    rt_task_delete(task);
}


#define rtos_task_wait_period()     rt_task_wait_period()
#define rtos_busy_sleep(nanosecs)   rt_busy_sleep(nanosecs)

static inline void rtos_task_sleep_until(rtos_time_t *wakeup_time)
{
    rt_sleep_until(*wakeup_time);
}


static inline int rtos_in_rt_context(void)
{
    return (rt_whoami()->priority != RTOS_LINUX_PRIORITY);
}



/* event signaling */
#define RTOS_EVENT_TIMEOUT          SEM_TIMOUT
#define RTOS_EVENT_ERROR(result)    ((result) == 0xFFFF /* SEM_ERR */)

/* note: event is initially set to a non-signaled state */
static inline int rtos_event_init(rtos_event_t *event)
{
    rt_typed_sem_init(event, 0, CNT_SEM);
    return 0;
}

/* note: event is initially set to a non-signaled state */
static inline int rtos_event_sem_init(rtos_event_sem_t *event)
{
    rt_typed_sem_init(event, 0, CNT_SEM);
    return 0;
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
    rt_sem_signal(event);
}


static inline int rtos_event_wait(rtos_event_sem_t *event)
{
    return rt_sem_wait(event);
}

static inline int rtos_event_sem_wait(rtos_event_sem_t *event)
{
    return rt_sem_wait(event);
}

static inline int rtos_event_sem_wait_timed(rtos_event_sem_t *event,
                                            rtos_time_t *timeout)
{
    return rt_sem_wait_timed(event, *timeout);
}



/* resource locks */
static inline int rtos_res_lock_init(rtos_res_lock_t *lock)
{
    rt_typed_sem_init(lock, 1, RES_SEM);
    return 0;
}

static inline int rtos_res_lock_delete(rtos_res_lock_t *lock)
{
    rt_sem_delete(lock);
    return 0;
}


static inline void rtos_res_lock(rtos_res_lock_t *lock)
{
    rt_sem_wait(lock);
}

static inline void rtos_res_unlock(rtos_res_lock_t *lock)
{
    rt_sem_signal(lock);
}


/* non-RT signals */
static inline int rtos_nrt_signal_init(rtos_nrt_signal_t *nrt_sig,
                                       void (*handler)(void))
{
    *nrt_sig = rt_request_srq(0, handler, 0);
    return *nrt_sig;
}

static inline void rtos_nrt_signal_delete(rtos_nrt_signal_t *nrt_sig)
{
    rt_free_srq(*nrt_sig);
}


static inline void rtos_pend_nrt_signal(rtos_nrt_signal_t *nrt_sig)
{
    rt_pend_linux_srq(*nrt_sig);
}



/* RT memory management */
#define rtos_malloc(size)           rt_malloc(size)
#define rtos_free(buffer)           rt_free(buffer)



/* IRQ management */
static inline int rtos_irq_request(unsigned int irq,
    void (*handler)(unsigned int, void *), void *arg)
{
#if defined(CONFIG_ARCH_I386)
    return rt_request_global_irq_ext(irq,
        (void (*)(void))handler, (unsigned long)arg);
#elif defined(CONFIG_ARCH_PPC)
    return rt_request_global_irq_ext(irq,
        (int (*)(unsigned int, unsigned long))handler, (unsigned long)arg);
#else
    #error Unsupported architecture.
#endif
}

static inline int rtos_irq_free(unsigned int irq)
{
    return rt_free_global_irq(irq);
}


#define rtos_irq_enable(irq)        rt_enable_irq(irq)
#define rtos_irq_disable(irq)       rt_disable_irq(irq)
#if defined(CONFIG_ARCH_I386)
    #define rtos_irq_end(irq)       rt_enable_irq(irq)
#elif defined(CONFIG_ARCH_PPC)
    #define rtos_irq_end(irq)       rt_unmask_irq(irq)
#else
    #error Unsupported architecture.
#endif



/* proc filesystem */
/* TODO: make it RTOS independent (it's part of Linux!) */
#define RTNET_PROC_PRINT_VARS       PROC_PRINT_VARS
#define RTNET_PROC_PRINT            PROC_PRINT
#define RTNET_PROC_PRINT_DONE       PROC_PRINT_DONE


#endif /* __RTNET_SYS_RTAI_H_ */
