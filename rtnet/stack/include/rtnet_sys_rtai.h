/***
 *
 *  include/rtnet_sys_rtai.h
 *
 *  RTnet - real-time networking subsystem
 *          RTOS abstraction layer - RTAI version (3.3 or better)
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

#ifndef __RTNET_SYS_RTAI_H_
#define __RTNET_SYS_RTAI_H_

#include <rtdm/rtdm_driver.h>


/* not yet moved to RTAI's RTDM version */
#ifndef RTDM_TASK_RAISE_PRIORITY
# define RTDM_TASK_RAISE_PRIORITY   (+1)
# define RTDM_TASK_LOWER_PRIORITY   (-1)
#endif

/* workarounds for various shortcomings in RTAI's RTDM port */
#define FUSION_LOW_PRIO             RT_SCHED_LOWEST_PRIORITY
#define FUSION_HIGH_PRIO            RT_SCHED_HIGHEST_PRIORITY
#define rthal_spin_lock             rt_spin_lock
#define rthal_spin_unlock           rt_spin_unlock
#define rthal_local_irq_save(flags) hard_save_flags_and_cli(flags)
#define rthal_local_irq_restore(flags) \
    hard_restore_flags(flags)
#undef SLEEP



static inline void nano_to_timeval(__u64 time, struct timeval *tval)
{
    tval->tv_sec = rtai_ulldiv(time, 1000000000,
                               (unsigned long *)&tval->tv_usec);
    tval->tv_usec /= 1000;
}


#define CONFIG_RTOS_STARTSTOP_TIMER 1

static inline int rtos_timer_start(void)
{
    rt_set_oneshot_mode();
    start_rt_timer(0);
    return 0;
}

static inline void rtos_timer_stop(void)
{
    stop_rt_timer();
}


static inline void rtos_irq_release_lock(void)
{
    rt_sched_lock();
    hard_sti();
}

static inline void rtos_irq_reacquire_lock(void)
{
    hard_cli();
    rt_sched_unlock();
}


#endif /* __RTNET_SYS_RTAI_H_ */
