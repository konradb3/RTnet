/* rtnet_module.c
 *
 * rtnet - real-time networking subsystem
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#endif

#include <rtai.h>
#include <rtai_sched.h>

#ifdef CONFIG_PROC_FS
#include <rtai_proc_fs.h>
#endif

#include <rtnet.h>
#include <rtnet_internal.h>

struct rtnet_mgr STACK_manager;
struct rtnet_mgr RTDEV_manager;

/***
 *      proc filesystem section
 */
#ifdef CONFIG_PROC_FS
static int rtnet_mgr_read_proc (char *page, char **start,
                                 off_t off, int count, int *eof, void *data)
{
        PROC_PRINT_VARS;
        PROC_PRINT("\nRTnet\n\n");
        PROC_PRINT_DONE;
}

static int rtnet_mgr_proc_register(void)
{
        static struct proc_dir_entry *proc_rtnet_mgr;
        proc_rtnet_mgr = create_proc_entry(RTNET_PROC_NAME, S_IFREG | S_IRUGO | S_IWUSR, rtai_proc_root);
        if (!proc_rtnet_mgr) {
                rt_printk ("Unable to initialize: /proc/rtai/rtnet_mgr\n");
                return -1;
        }
        proc_rtnet_mgr->read_proc = rtnet_mgr_read_proc;
        return 0;
}

static void rtnet_mgr_proc_unregister(void)
{
        remove_proc_entry (RTNET_PROC_NAME, rtai_proc_root);
}
#endif  /* CONFIG_PROC_FS */




/**
 *      rtnet_mgr_init():       initialize the RTnet
 *
 */
int rtnet_init(void)
{
        int err = 0;

        printk("RTnet: init real-time networking\n");
        init_crc32();

        if ( (err=rtskb_pool_init()) )
                return err;

        rtsockets_init();
        rtnet_dev_init();
        rt_inet_proto_init();
        rtnet_chrdev_init();

#ifdef CONFIG_PROC_FS
        err = rtnet_mgr_proc_register ();
#endif

        /* initialise the Stack-Manager */
        if ( (err=rt_stack_mgr_init(&STACK_manager)) )
		return err;

        /* initialise the RTDEV-Manager */
        if ( (err=rt_rtdev_mgr_init(&RTDEV_manager)) )
		return err;

        return 0;
}




/**
 *      rtnet_mgr_release():    release the RTnet-Manager
 *
 */
void rtnet_release(void)
{
        rt_printk("RTnet: End real-time networking\n");
	rt_stack_mgr_delete(&STACK_manager);
	rt_rtdev_mgr_delete(&RTDEV_manager);

#ifdef CONFIG_PROC_FS
        rtnet_mgr_proc_unregister ();
#endif
        rtnet_chrdev_release();
        rt_inet_proto_release();
        rtnet_dev_release();
        rtsockets_release();
        rtskb_pool_release();

        cleanup_crc32();
}



module_init(rtnet_init);
module_exit(rtnet_release);



