/***
 *
 *  include/rtnet_sys_xenomai.h
 *
 *  RTnet - real-time networking subsystem
 *          RTOS abstraction layer - Xenomai version
 *
 *  Copyright (C) 2004, 2005 Jan Kiszka <jan.kiszka@web.de>
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

#ifndef __RTNET_SYS_XENOMAI_H_
#define __RTNET_SYS_XENOMAI_H_

#include <nucleus/pod.h>
#include <rtdm/rtdm_driver.h>

/* basic types */
typedef rtdm_lock_t                 rtos_spinlock_t;
typedef rtdm_task_t                 rtos_task_t;
typedef rtdm_event_t                rtos_event_t;
typedef rtdm_sem_t                  rtos_sem_t;
typedef rtdm_mutex_t                rtos_res_lock_t;
typedef rtdm_nrtsig_t               rtos_nrt_signal_t;
typedef rtdm_irq_t                  rtos_irq_t;
typedef rtdm_irq_handler_t          rtos_irq_handler_t;

#define ALIGN_RTOS_TASK             16  /* alignment of rtdm_tast_t */


/* print output messages */
#define rtos_print                  rtdm_printk


/* time handling */
static inline __u64 rtos_get_time(void)
{
    return rtdm_clock_read();
}

static inline void rtos_ns_to_timeval(__u64 time, struct timeval *tval)
{
    tval->tv_sec = rthal_ulldiv(time, 1000000000,
                                (unsigned long *)&tval->tv_usec);
    tval->tv_usec /= 1000;
}


/* real-time spin locks */
#define RTOS_SPIN_LOCK_UNLOCKED     RTDM_LOCK_UNLOCKED  /* init */
#define rtos_spin_lock_init(lock)   rtdm_lock_init(lock)

#define rtos_spin_lock(lock)        rtdm_lock_get(lock)
#define rtos_spin_unlock(lock)      rtdm_lock_put(lock)

#define rtos_spin_lock_irqsave(lock, flags) \
    rtdm_lock_get_irqsave(lock, flags)
#define rtos_spin_unlock_irqrestore(lock, flags) \
    rtdm_lock_put_irqrestore(lock, flags)

#define rtos_local_irqsave(flags)   \
    rtdm_lock_irqsave(flags)
#define rtos_local_irqrestore(flags) \
    rtdm_lock_irqrestore(flags)


/* RT-tasks */
#define RTOS_LOWEST_RT_PRIORITY     RTDM_TASK_LOWEST_PRIORITY
#define RTOS_HIGHEST_RT_PRIORITY    RTDM_TASK_HIGHEST_PRIORITY
#define RTOS_RAISE_PRIORITY         (+1)
#define RTOS_LOWER_PRIORITY         (-1)

static inline int rtos_task_init(rtos_task_t *task, void (*task_proc)(void *),
                                 void *arg, int priority)
{
    return rtdm_task_init(task, NULL, task_proc, arg, priority, 0);
}

static inline int rtos_task_init_periodic(rtos_task_t *task,
                                          void (*task_proc)(void *),
                                          void *arg, int priority,
                                          __u64 period)
{
    return rtdm_task_init(task, NULL, task_proc, arg, priority, period);
}

#define rtos_task_wakeup(task)      rtdm_task_unblock(task)
#define rtos_task_delete(task)      rtdm_task_destroy(task)
#define rtos_task_set_priority(task, priority)  \
    rtdm_task_set_priority(task, priority)

#define CONFIG_RTOS_STARTSTOP_TIMER 1

static inline int rtos_timer_start_oneshot(void)
{
    return xnpod_start_timer(XN_APERIODIC_TICK, XNPOD_DEFAULT_TICKHANDLER);
}

static inline void rtos_timer_stop(void)
{
    xnpod_stop_timer();
}

#define rtos_task_wait_period(task)         rtdm_task_wait_period()
#define rtos_busy_sleep(nanosecs)           rtdm_task_busy_sleep(nanosecs)

#define rtos_task_sleep_until(wakeup_time)  rtdm_task_sleep_until(wakeup_time)

#define rtos_in_rt_context()                rtdm_in_rt_context()


/* event signaling */
#define rtos_event_init(event)              rtdm_event_init(event, 0)
#define rtos_event_delete(event)            rtdm_event_destroy(event)
#define rtos_event_broadcast(event)         rtdm_event_pulse(event)
#define rtos_event_signal(event)            rtdm_event_signal(event)
#define rtos_event_wait(event)              rtdm_event_wait(event)
#define rtos_event_timedwait(event, timeout) \
    rtdm_event_timedwait(event, timeout, NULL)


/* semaphores */
#define rtos_sem_init(sem)                  rtdm_sem_init(sem, 0)
#define rtos_sem_delete(sem)                rtdm_sem_destroy(sem)
#define rtos_sem_down(sem)                  rtdm_sem_down(sem)
#define rtos_sem_timeddown(sem, timeout) \
    rtdm_sem_timeddown(sem, timeout, NULL)
#define rtos_sem_up(sem)                    rtdm_sem_up(sem)


/* resource locks */
#define rtos_res_lock_init(lock)            rtdm_mutex_init(lock)
#define rtos_res_lock_delete(lock)          rtdm_mutex_destroy(lock)
#define rtos_res_lock(lock)                 rtdm_mutex_lock(lock)
#define rtos_res_unlock(lock)               rtdm_mutex_unlock(lock)


/* non-RT signals */
#define rtos_nrt_signal_init(nrt_sig, handler)  \
    rtdm_nrtsig_init(nrt_sig, (rtdm_nrtsig_handler_t)handler)
#define rtos_nrt_signal_delete(nrt_sig)     rtdm_nrtsig_destroy(nrt_sig)
#define rtos_nrt_pend_signal(nrt_sig)       rtdm_nrtsig_pend(nrt_sig)


/* RT memory management */
#define rtos_malloc(size)                   rtdm_malloc(size)
#define rtos_free(buffer)                   rtdm_free(buffer)


/* IRQ management */
#define RTOS_IRQ_HANDLER_PROTO(name)        int name(rtdm_irq_t *irq_handle)
#define RTOS_IRQ_GET_ARG(type)              rtdm_irq_get_arg(irq_handle, type)
#define RTOS_IRQ_RETURN_HANDLED()           return RTDM_IRQ_ENABLE
#define RTOS_IRQ_RETURN_UNHANDLED()         return 0 /* mask, don't propgt. */

#define rtos_irq_request(irq_handle, irq_no, handler, arg)  \
    rtdm_irq_request(irq_handle, irq_no, handler, 0, NULL, arg)
#define rtos_irq_free(irq_handle)           rtdm_irq_free(irq_handle)
#define rtos_irq_enable(irq_handle)         rtdm_irq_enable(irq_handle)
#define rtos_irq_disable(irq_handle)        rtdm_irq_disable(irq_handle)

#define rtos_irq_end(irq_handle)    /* done by returning RT_INTR_ENABLE */

static inline void rtos_irq_release_lock(void)
{
    xnpod_set_thread_mode(xnpod_current_thread(), 0, XNLOCK);
    rthal_local_irq_enable_hw();
}

static inline void rtos_irq_reacquire_lock(void)
{
    rthal_local_irq_disable_hw();
    xnpod_set_thread_mode(xnpod_current_thread(), XNLOCK, 0);
}


#endif /* __RTNET_SYS_XENOMAI_H_ */
