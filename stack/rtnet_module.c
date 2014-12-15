/***
 *
 *  stack/rtnet_module.c - module framework, proc file system
 *
 *  Copyright (C) 2002      Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2003-2006 Jan Kiszka <jan.kiszka@web.de>
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
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <rtdev_mgr.h>
#include <rtnet_chrdev.h>
#include <rtnet_internal.h>
#include <rtnet_socket.h>
#include <rtnet_rtpc.h>
#include <stack_mgr.h>
#include <rtwlan.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("RTnet stack core");


struct rtnet_mgr STACK_manager;
struct rtnet_mgr RTDEV_manager;

EXPORT_SYMBOL(STACK_manager);
EXPORT_SYMBOL(RTDEV_manager);

const char rtnet_rtdm_provider_name[] =
    "(C) 1999-2008 RTnet Development Team, http://www.rtnet.org";

EXPORT_SYMBOL(rtnet_rtdm_provider_name);

#ifdef CONFIG_PROC_FS
/***
 *      proc filesystem section
 */
struct proc_dir_entry *rtnet_proc_root;

EXPORT_SYMBOL(rtnet_proc_root);

static int proc_rtnet_devices_show(struct seq_file *p, void *data)
{
    int i;
    struct rtnet_device *rtdev;

    seq_printf(p, "Index\tName\t\tFlags\n");

    mutex_lock(&rtnet_devices_nrt_lock);
    for (i = 1; i <= MAX_RT_DEVICES; i++) {
        rtdev = __rtdev_get_by_index(i);
        if (rtdev != NULL) {
	  seq_printf(p, "%d\t%-15s %s%s%s%s\n",
		     rtdev->ifindex, rtdev->name,
		     (rtdev->flags & IFF_UP) ? "UP" : "DOWN",
		     (rtdev->flags & IFF_BROADCAST) ? " BROADCAST" : "",
		     (rtdev->flags & IFF_LOOPBACK) ? " LOOPBACK" : "",
		     (rtdev->flags & IFF_PROMISC) ? " PROMISC" : "");
        }
    }
    mutex_unlock(&rtnet_devices_nrt_lock);

    return 0;
}

static int proc_rtnet_devices_open(struct inode *inode, struct  file *file) {
  return single_open(file, proc_rtnet_devices_show, NULL);
}

static const struct file_operations proc_rtnet_devices_fops = {
  .open = proc_rtnet_devices_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};


static int proc_rtnet_rtskb_show(struct seq_file *p, void *data)
{
    unsigned int rtskb_len;

    rtskb_len = ALIGN_RTSKB_STRUCT_LEN + SKB_DATA_ALIGN(RTSKB_SIZE);
    seq_printf(p, "Statistics\t\tCurrent\tMaximum\n"
	       "rtskb pools\t\t%d\t%d\n"
	       "rtskbs\t\t\t%d\t%d\n"
	       "rtskb memory need\t%d\t%d\n",
	       rtskb_pools, rtskb_pools_max,
	       rtskb_amount, rtskb_amount_max,
	       rtskb_amount * rtskb_len, rtskb_amount_max * rtskb_len);

    return 0;
}

static int proc_rtnet_rtskb_open(struct inode *inode, struct  file *file) {
  return single_open(file, proc_rtnet_rtskb_show, NULL);
}

static const struct file_operations proc_rtnet_rtskb_fops = {
  .open = proc_rtnet_rtskb_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};


static int proc_rtnet_version_show(struct seq_file *p, void *data)
{
    const char verstr[] =
        "RTnet " RTNET_PACKAGE_VERSION " - built on " __DATE__ " " __TIME__ "\n"
        "RTcap:      "
#ifdef CONFIG_RTNET_ADDON_RTCAP
            "yes\n"
#else
            "no\n"
#endif
        "rtnetproxy: "
#ifdef CONFIG_RTNET_ADDON_PROXY
            "yes\n"
#else
            "no\n"
#endif
        "bug checks: "
#ifdef CONFIG_RTNET_CHECKED
            "yes\n";
#else
            "no\n";
#endif

    seq_printf(p, "%s", verstr);

    return 0;
}

static int proc_rtnet_version_open(struct inode *inode, struct  file *file) {
  return single_open(file, proc_rtnet_version_show, NULL);
}

static const struct file_operations proc_rtnet_version_fops = {
  .open = proc_rtnet_version_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};


static int proc_rtnet_stats_show(struct seq_file *p, void *data)
{
    int i;
    struct rtnet_device *rtdev;

    seq_printf(p, "Inter-|   Receive                            "
	       "                    |  Transmit\n");
    seq_printf(p, " face |bytes    packets errs drop fifo frame "
	       "compressed multicast|bytes    packets errs "
	       "drop fifo colls carrier compressed\n");

    mutex_lock(&rtnet_devices_nrt_lock);
    for (i = 1; i <= MAX_RT_DEVICES; i++) {
        rtdev = __rtdev_get_by_index(i);
        if (rtdev == NULL)
            continue;
        if (rtdev->get_stats) {
            struct net_device_stats *stats = rtdev->get_stats(rtdev);

            seq_printf(p, "%6s:%8lu %7lu %4lu %4lu %4lu %5lu %10lu %9lu "
		       "%8lu %7lu %4lu %4lu %4lu %5lu %7lu %10lu\n",
		       rtdev->name, stats->rx_bytes, stats->rx_packets,
		       stats->rx_errors,
		       stats->rx_dropped + stats->rx_missed_errors,
		       stats->rx_fifo_errors,
		       stats->rx_length_errors + stats->rx_over_errors +
		       stats->rx_crc_errors + stats->rx_frame_errors,
		       stats->rx_compressed, stats->multicast,
		       stats->tx_bytes, stats->tx_packets,
		       stats->tx_errors, stats->tx_dropped,
		       stats->tx_fifo_errors, stats->collisions,
		       stats->tx_carrier_errors +
		       stats->tx_aborted_errors +
		       stats->tx_window_errors +
		       stats->tx_heartbeat_errors,
		       stats->tx_compressed);
        } else {
	  seq_printf(p, "%6s: No statistics available.\n", rtdev->name);
        }
    }
    mutex_unlock(&rtnet_devices_nrt_lock);

    return 0;
}

static int proc_rtnet_stats_open(struct inode *inode, struct  file *file) {
  return single_open(file, proc_rtnet_stats_show, NULL);
}

static const struct file_operations proc_rtnet_stats_fops = {
  .open = proc_rtnet_stats_open,
  .read = seq_read,
  .llseek = seq_lseek,
  .release = single_release,
};


static int rtnet_proc_register(void)
{
    struct proc_dir_entry *proc_entry;

    rtnet_proc_root = proc_mkdir("rtnet", 0);
    if (!rtnet_proc_root)
        goto error1;

    proc_entry = proc_create("devices", S_IFREG | S_IRUGO | S_IWUSR,
			     rtnet_proc_root, &proc_rtnet_devices_fops);
    if (!proc_entry)
        goto error2;

    proc_entry = proc_create("rtskb", S_IFREG | S_IRUGO | S_IWUSR,
			     rtnet_proc_root, &proc_rtnet_rtskb_fops);
    if (!proc_entry)
        goto error3;

    proc_entry = proc_create("version", S_IFREG | S_IRUGO | S_IWUSR,
			     rtnet_proc_root, &proc_rtnet_version_fops);
    if (!proc_entry)
        goto error4;

    proc_entry = proc_create("stats", S_IRUGO,
			     rtnet_proc_root, &proc_rtnet_stats_fops);
    if (!proc_entry)
        goto error5;

    return 0;

  error5:
    remove_proc_entry("version", rtnet_proc_root);

  error4:
    remove_proc_entry("rtskb", rtnet_proc_root);

  error3:
    remove_proc_entry("devices", rtnet_proc_root);

  error2:
    remove_proc_entry("rtnet", 0);

  error1:
    /*ERRMSG*/printk("RTnet: unable to initialize /proc entries\n");
    return -1;
}



static void rtnet_proc_unregister(void)
{
    remove_proc_entry("devices", rtnet_proc_root);
    remove_proc_entry("rtskb", rtnet_proc_root);
    remove_proc_entry("version", rtnet_proc_root);
    remove_proc_entry("stats", rtnet_proc_root);
    remove_proc_entry("rtnet", 0);
}
#endif  /* CONFIG_PROC_FS */



/**
 *  rtnet_init()
 */
int __init rtnet_init(void)
{
    int err = 0;


    printk("\n*** RTnet " RTNET_PACKAGE_VERSION " - built on " __DATE__ " " __TIME__
           " ***\n\n");
    printk("RTnet: initialising real-time networking\n");

    if ((err = rtskb_pools_init()) != 0)
        goto err_out1;

#ifdef CONFIG_PROC_FS
    if ((err = rtnet_proc_register()) != 0)
        goto err_out2;
#endif

    /* initialize the Stack-Manager */
    if ((err = rt_stack_mgr_init(&STACK_manager)) != 0)
        goto err_out3;

    /* initialize the RTDEV-Manager */
    if ((err = rt_rtdev_mgr_init(&RTDEV_manager)) != 0)
        goto err_out4;

    rtnet_chrdev_init();

    if ((err = rtwlan_init()) != 0)
        goto err_out5;

    if ((err = rtpc_init()) != 0)
        goto err_out6;

    return 0;


err_out6:
    rtwlan_exit();

err_out5:
    rtnet_chrdev_release();
    rt_rtdev_mgr_delete(&RTDEV_manager);

err_out4:
    rt_stack_mgr_delete(&STACK_manager);

err_out3:
#ifdef CONFIG_PROC_FS
    rtnet_proc_unregister();

err_out2:
#endif
    rtskb_pools_release();

err_out1:
    return err;
}


/**
 *  rtnet_release()
 */
void __exit rtnet_release(void)
{
    rtpc_cleanup();

    rtwlan_exit();

    rtnet_chrdev_release();

    rt_stack_mgr_delete(&STACK_manager);
    rt_rtdev_mgr_delete(&RTDEV_manager);

    rtskb_pools_release();

#ifdef CONFIG_PROC_FS
    rtnet_proc_unregister();
#endif

    printk("RTnet: unloaded\n");
}


module_init(rtnet_init);
module_exit(rtnet_release);
