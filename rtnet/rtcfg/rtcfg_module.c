/***
 *
 *  rtcfg/rtcfg_module.c
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003, 2004 Jan Kiszka <jan.kiszka@web.de>
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
#include <linux/kernel.h>

#include <rtcfg/rtcfg_event.h>
#include <rtcfg/rtcfg_frame.h>
#include <rtcfg/rtcfg_ioctl.h>


/* RTAI-specific: start scheduling timer */
#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30)
static int start_timer = 1;

MODULE_PARM(start_timer, "i");
MODULE_PARM_DESC(start_timer, "set to zero if scheduler already runs");
#endif

MODULE_LICENSE("GPL");



int __init rtcfg_init(void)
{
    int ret;


    printk("RTcfg: init real-time configuration distribution protocol\n");

#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30)
    if (start_timer) {
        rt_set_oneshot_mode();
        start_rt_timer(0);
    }
#endif

    ret = rtcfg_init_ioctls();
    if (ret != 0)
        goto error1;

    rtcfg_init_state_machines();

    ret = rtcfg_init_frames();
    if (ret != 0)
        goto error2;

    return 0;

  error2:
    rtcfg_cleanup_state_machines();
    rtcfg_cleanup_ioctls();

  error1:
#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30)
    if (start_timer)
        stop_rt_timer();
#endif

    return ret;
}



void rtcfg_cleanup(void)
{
#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30)
    if (start_timer)
        stop_rt_timer();
#endif

    rtcfg_cleanup_frames();
    rtcfg_cleanup_state_machines();
    rtcfg_cleanup_ioctls();

    printk("RTcfg: unloaded\n");
}



module_init(rtcfg_init);
module_exit(rtcfg_cleanup);
