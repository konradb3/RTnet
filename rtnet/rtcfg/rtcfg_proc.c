/***
 *
 *  rtcfg/rtcfg_proc.c
 *
 *  Real-Time Configuration Distribution Protocol
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

#include <asm/semaphore.h>

#include <rtnet_internal.h>


#ifdef CONFIG_PROC_FS
struct semaphore        nrt_proc_sem;
struct proc_dir_entry   *rtcfg_proc_root;



void rtcfg_update_proc(void)
{
}



int rtcfg_init_proc(void)
{
    sema_init(&nrt_proc_sem, 1);

    rtcfg_proc_root = create_proc_entry("rtcfg", S_IFDIR, rtnet_proc_root);
    if (!rtcfg_proc_root)
        goto err1;

    return 0;

  err1:
    /*ERRMSG*/printk("RTcfg: unable to initialise /proc entries\n");
    return -1;
}



void rtcfg_cleanup_proc(void)
{

    remove_proc_entry("rtcfg", rtnet_proc_root);
}

#endif /* CONFIG_PROC_FS */
