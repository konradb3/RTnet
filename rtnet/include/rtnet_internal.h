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

#include <rtnet_sys.h>

#ifdef HAVE_RTAI_SEM_H
#include <rtai_sem.h>
#endif

#ifdef CONFIG_RTNET_CHECKED
#define RTNET_ASSERT(expr, func) \
    if (!(expr)) \
    { \
        rtos_print("Assertion failed! %s:%s:%d %s\n", \
        __FILE__, __FUNCTION__, __LINE__, (#expr)); \
        func \
    }
#else
#define RTNET_ASSERT(expr, func)
#endif /* CONFIG_RTNET_CHECKED */

/* some configurables */

#define RTNET_STACK_PRIORITY    1
/*#define RTNET_RTDEV_PRIORITY    5*/
#define DROPPING_RTSKB          20


struct rtnet_device;

/*struct rtnet_msg {
    int                 msg_type;
    struct rtnet_device *rtdev;
};*/


struct rtnet_mgr {
    rtos_task_t      task;
/*    MBX     mbx;*/
    rtos_event_sem_t event;
};


extern struct rtnet_mgr STACK_manager;
extern struct rtnet_mgr RTDEV_manager;


#ifdef CONFIG_PROC_FS

#include <linux/proc_fs.h>

extern struct proc_dir_entry *rtnet_proc_root;


/* stolen from Erwin Rol's rtai_proc_fs.h */

#define RTNET_PROC_PRINT_VARS                           \
    off_t __pos   = 0;                                  \
    off_t __begin = 0;                                  \
    int   __len   = 0 /* no ";" */


#define RTNET_PROC_PRINT(fmt, args...)                  \
    do {                                                \
        int len = sprintf(buf + __len , fmt, ##args);   \
        __len += len;                                   \
        __pos += len;                                   \
        if (__pos < offset) {                           \
            __len = 0;                                  \
            __begin = __pos;                            \
        }                                               \
        if (__pos > offset + count)                     \
            goto __done;                                \
    } while (0)


#define RTNET_PROC_PRINT_DONE                           \
    do {                                                \
        *eof = 1;                                       \
      __done:                                           \
        *start = buf + (offset - __begin);              \
        __len -= (offset - __begin);                    \
        if (__len > count)                              \
            __len = count;                              \
        if(__len < 0)                                   \
            __len = 0;                                  \
        return __len;                                   \
    } while (0)

#endif /* CONFIG_PROC_FS */

#endif /* __KERNEL__ */


#endif /* __RTNET_INTERNAL_H_ */
