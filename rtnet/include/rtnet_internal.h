/***
 * rtnet_internal.h - internal declarations
 *
 * Copyright (C) 1999      Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002      Ulrich Marx <marx@kammer.uni-hannover.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef __RTNET_INTERNAL_H_
#define __RTNET_INTERNAL_H_

#ifdef __KERNEL__

#include <rtai.h>
#include <rtai_sched.h>


/* #ifdef CONFIG_RTNET_CHECKED */
#define RTNET_ASSERT(expr, func) \
    if (!(expr)) \
    { \
        rt_printk("Assertion failed! %s:%s:%d %s\n", \
        __FILE__, __FUNCTION__, __LINE__, (#expr)); \
        func \
    }
/* #endif */

/* some configurables */

#define RTNET_PROC_NAME         "rtnet"
#define RTNET_STACK_PRIORITY    1
#define RTNET_RTDEV_PRIORITY    5
#define DROPPING_RTSKB          20


struct rtnet_device;

struct rtnet_msg {
    int                 msg_type;
    struct rtnet_device *rtdev;
};


struct rtnet_mgr {
    RT_TASK task;
    MBX     mbx;
    SEM     sem;
};


extern struct rtnet_mgr STACK_manager;
extern struct rtnet_mgr RTDEV_manager;


#endif /* __KERNEL__ */

#endif /* __RTNET_INTERNAL_H_ */
