/***
 *
 *  rtcfg/rtcfg_module.c
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003 Jan Kiszka <jan.kiszka@web.de>
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

#include <rtai.h>
#include <rtai_sched.h>

#include <rtdev.h>
#include <ipv4/protocol.h>
#include <rtcfg/rtcfg_frame.h>
#include <rtcfg/rtcfg_ui.h>


static char *ips[8]    = {"", "", "", "", "", "", "", ""};
static char *dev       = "rteth0";
static int start_timer = 1;
static int timeout     = 120000;

MODULE_PARM(ips, "1-8s");
MODULE_PARM(dev, "s");
MODULE_PARM(start_timer, "i");
MODULE_PARM(timeout, "i");
MODULE_PARM_DESC(ips, "list of client IPs, if empty, run RTcfg as client");
MODULE_PARM_DESC(dev, "network device to use");
MODULE_PARM_DESC(start_timer, "set to zero if scheduler already runs");
MODULE_PARM_DESC(timeout, "timeout for waiting on other stations");

MODULE_LICENSE("GPL");



int rtcfg_init(void)
{
    int ret;
    int i;
    int client = 1;
    struct rtnet_device *rtdev;
    int ifindex;


    printk("RTcfg: init real-time configuration distribution protocol\n");

    if (start_timer) {
        rt_set_oneshot_mode();
        start_rt_timer(0);
    }

    ret = rtcfg_init_ui();
    if (ret != 0)
        return ret;

    rtcfg_init_state_machines();

    ret = rtcfg_init_frames();
    if (ret != 0) {
        rtcfg_cleanup_state_machines();
        rtcfg_cleanup_ui();
        return ret;
    }


    rtdev = rtdev_get_by_name(dev);
    if (rtdev == NULL) {
        rtcfg_cleanup_frames();
        rtcfg_cleanup_state_machines();
        rtcfg_cleanup_ui();

        printk("RTcfg: device %s not found\n", dev);
        return 1;
    }
    ifindex = rtdev->ifindex;
    rtdev_dereference(rtdev);

    for (i = 0; i < 8; i++)
        if (*ips[i] != 0) {
            if (client) {
                ret = rtcfg_cmd_server(ifindex);
                if (ret < 0) {
                    printk("rtcfg_cmd_server(): %d\n", ret);
                    return 0;
                }
                client = 0;
            }
            printk("IP %d: %s\n", i, ips[i]);
            ret = rtcfg_cmd_add_ip(ifindex, rt_inet_aton(ips[i]));
            if (ret < 0) {
                printk("rtcfg_cmd_add_ip(): %d\n", ret);
                return 0;
            }
        }

    if (client) {
        ret = rtcfg_cmd_client(ifindex, timeout);
        if (ret < 0) {
            printk("rtcfg_cmd_client(): %d\n", ret);
            return 0;
        }
        ret = rtcfg_cmd_announce(ifindex, timeout);
        if (ret < 0) {
            printk("rtcfg_cmd_announce(): %d\n", ret);
            return 0;
        }
    } else {
        rtcfg_cmd_wait(ifindex, timeout);
        if (ret < 0) {
            printk("rtcfg_cmd_wait(): %d\n", ret);
            return 0;
        }
    }

    return 0;
}



void rtcfg_cleanup(void)
{
    if (start_timer)
        stop_rt_timer();

    rtcfg_cleanup_frames();
    rtcfg_cleanup_state_machines();
    rtcfg_cleanup_ui();

    printk("RTcfg: unloaded\n");
}



module_init(rtcfg_init);
module_exit(rtcfg_cleanup);
