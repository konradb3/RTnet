/* rtmac_module.c
 *
 * rtmac - real-time networking media access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>,
 *               2003 Jan Kiszka <Jan.Kiszka@web.de>
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

#include <rtmac/rtmac_chrdev.h>
#include <rtmac/rtmac_proc.h>
#include <rtmac/rtmac_proto.h>
#include <rtmac/rtmac_vnic.h>

#include <rtmac/rtmac_disc.h>       /* legacy */
#include <rtmac/tdma/tdma_module.h> /* legacy */


static char *dev = "rteth0";
MODULE_PARM(dev, "s");
MODULE_PARM_DESC(dev, "RTmac: device to be rtnet started on");

int rtmac_init(void)
{
    struct rtnet_device *rtdev;
    int ret = 0;

    rt_printk("RTmac: init realtime media access control\n");

    rtmac_proto_init();

#ifdef CONFIG_PROC_FS
    ret = rtmac_proc_register();
    if (ret)
        goto error1;
#endif

    ret = rtmac_chrdev_init();
    if (ret)
        goto error2;

    ret = rtmac_vnic_module_init();
    if (ret)
        goto error3;

    /* legacy */
    {
        struct rtmac_disc *tdma;


        ret = tdma_init();
        if (ret)
        {
            rtmac_vnic_module_cleanup();
            goto error3;
        }

        tdma = rtmac_get_disc_by_name("TDMA1");

        rtdev = rtdev_get_by_name(dev);
        ret = rtmac_disc_attach(rtdev, tdma);
        rtdev_dereference(rtdev);
        if (ret)
        {
            tdma_release();
            rtmac_vnic_module_cleanup();
            goto error3;
        }
    } /* end of legacy */

    return 0;

error3:
    rtmac_chrdev_release();

error2:
#ifdef CONFIG_PROC_FS
    rtmac_proc_release();
#endif

error1:
    rtmac_proto_release();
    return ret;
}



void rtmac_release(void)
{
    struct rtnet_device *rtdev;


    rt_printk("RTmac: end realtime media access control\n");

    rtdev = rtdev_get_by_name(dev); /* legacy */
    rtmac_disc_detach(rtdev);       /* legacy */
    rtdev_dereference(rtdev);       /* legacy */
    tdma_release();                 /* legacy */

    rtmac_proto_release();
#ifdef CONFIG_PROC_FS
    rtmac_proc_release();
#endif
    rtmac_chrdev_release();
    rtmac_vnic_module_cleanup();
}



module_init(rtmac_init);
module_exit(rtmac_release);

MODULE_AUTHOR("Marc Kleine-Budde, Jan Kiszka");
MODULE_LICENSE("GPL");
