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
#include <linux/netdevice.h>
#endif

#include <rtai.h>
#include <rtai_sched.h>

#ifdef CONFIG_PROC_FS
#include <rtai_proc_fs.h>
#endif

#include <rtdev_mgr.h>
#include <rtnet_crc32.h>
#include <rtnet_internal.h>
#include <rtnet_socket.h>
#include <rtnet_chrdev.h>
#include <stack_mgr.h>
#include <ipv4/af_inet.h>

MODULE_LICENSE("GPL");

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
    int i;
    struct rtnet_device *rtdev;
    unsigned int rtskb_len;

    PROC_PRINT("\nRTnet\n\n");
    PROC_PRINT("Devices:\n");
    for (i = 1; i <= MAX_RT_DEVICES; i++) {
        rtdev = rtdev_get_by_index(i);
        if (rtdev != NULL) {
            PROC_PRINT("  %s: %s rxq=%d\n",
                rtdev->name,
                (rtdev->flags & IFF_UP) ? "UP" : "DOWN",
                rtdev->rxqueue_len);
            rtdev_dereference(rtdev);
        }
    }

    rtskb_len = ALIGN_RTSKB_STRUCT_LEN + SKB_DATA_ALIGN(RTSKB_SIZE);
    PROC_PRINT("\nrtskb pools current/max:       %d / %d\n"
               "rtskbs current/max:            %d / %d\n"
               "rtskb memory need current/max: %d / %d\n\n",
               rtskb_pools, rtskb_pools_max,
               rtskb_amount, rtskb_amount_max,
               rtskb_amount * rtskb_len, rtskb_amount_max * rtskb_len);

    PROC_PRINT_DONE;
}

static int rtnet_proc_register(void)
{
    static struct proc_dir_entry *proc_rtnet_mgr;
    proc_rtnet_mgr = create_proc_entry(RTNET_PROC_NAME, S_IFREG | S_IRUGO | S_IWUSR, rtai_proc_root);
    if (!proc_rtnet_mgr) {
        rt_printk ("Unable to initialize /proc/rtai/rtnet\n");
        return -1;
    }
    proc_rtnet_mgr->read_proc = rtnet_mgr_read_proc;
    return 0;
}

static void rtnet_proc_unregister(void)
{
    remove_proc_entry (RTNET_PROC_NAME, rtai_proc_root);
}
#endif  /* CONFIG_PROC_FS */




/**
 *  rtnet_init()
 */
int rtnet_init(void)
{
    int err = 0;


    printk("\n*** RTnet - built on %s, %s ***\n\n", __DATE__, __TIME__);
    printk("RTnet: initialising real-time networking\n");

    if ((err = init_crc32()) != 0)
        goto err_out1;

    if ((err = rtskb_pools_init()) != 0)
        goto err_out2;

    /* initialize the Stack-Manager */
    if ((err=rt_stack_mgr_init(&STACK_manager)) != 0)
        goto err_out2;

    /* initialize the RTDEV-Manager */
    if ((err=rt_rtdev_mgr_init(&RTDEV_manager)) != 0)
        goto err_out3;

    rtsockets_init();
    rt_inet_proto_init();
    rtnet_chrdev_init();

#ifdef CONFIG_PROC_FS
    if ((err = rtnet_proc_register()) != 0)
        goto err_out4;
#endif

    return 0;

err_out4:
    rtnet_chrdev_release();
    rt_inet_proto_release();
    rtsockets_release();

    rt_rtdev_mgr_delete(&RTDEV_manager);

err_out3:
    rt_stack_mgr_delete(&STACK_manager);

err_out2:
    rtskb_pools_release();

err_out1:
    cleanup_crc32();

    return err;
}




/**
 *  rtnet_release()
 */
void rtnet_release(void)
{
#ifdef CONFIG_PROC_FS
    rtnet_proc_unregister();
#endif
    rtnet_chrdev_release();

    rt_stack_mgr_delete(&STACK_manager);
    rt_rtdev_mgr_delete(&RTDEV_manager);

    rt_inet_proto_release();
    rtsockets_release();
    rtskb_pools_release();

    cleanup_crc32();

    printk("RTnet: unloaded\n");
}



module_init(rtnet_init);
module_exit(rtnet_release);
