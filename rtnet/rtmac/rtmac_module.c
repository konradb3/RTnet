/* rtmac_module.c
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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

#include <rtai.h>

#include <rtnet.h>
#include <rtmac.h>

static char *dev = "eth1";
MODULE_PARM(dev, "s");
MODULE_PARM_DESC(dev, "RTmac: device to be rtnet started on");

int rtmac_init(void)
{
	int ret = 0;

	rt_printk("RTmac: init realtime medium access control\n");

	ret = tdma_start(rtdev_get_by_name(dev));
	if (ret)
		return ret;

#ifdef CONFIG_PROC_FS
	ret = rtmac_proc_register();
	if (ret)
		return ret;
#endif
	ret = rtmac_chrdev_init();
	if (ret)
		return ret;

	return ret;
}



void rtmac_release(void)
{
	rt_printk("RTmac: end realtime medium access control\n");

	tdma_stop(rtdev_get_by_name(dev));

#ifdef CONFIG_PROC_FS
	rtmac_proc_release();
#endif

	rtmac_chrdev_release();
}



module_init(rtmac_init);
module_exit(rtmac_release);

MODULE_AUTHOR("Marc Kleine-Budde");
MODULE_LICENSE("GPL");
