/* rtskb_alloc.c
 *
 * rtnet - real-time networking subsystem
 * Copyright (C)  2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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
#include <linux/if_ether.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtskb.h>
#include <rtnet.h>

#define n 40
#define _max(a,b) (a>b)?a:b
#define _min(a,b) (a<b)?a:b

struct rtskb *rtskbs[n];
RTIME dt0, dt, time, t_max, t_min;

int rtskbs_alloc(void)
{
	int i;

	printk("RTSKB-ALLOC: start\n");

        rt_set_oneshot_mode();
        start_rt_timer_ns(10000000);
	
	t_min=90000;
	t_max=0;
	dt=time=0;
	dt0=0;

	for (i=0; i<n; i++) {
		time=rt_get_time();
		rtskbs[i]=alloc_rtskb(ETH_FRAME_LEN, &global_pool);
		dt0=rt_get_time()-time; 
		t_max=_max(t_max, dt0);
		t_min=_min(t_min, dt0);
		dt+=dt0;
	}
	printk("t_avg: %6dns \t t_min: %6dns \t t_max: %6dns \n", 
	       (int) (((int) count2nano(dt))/n), (int) count2nano(t_min), (int) count2nano(t_max));

	return 0;
}



void rtskbs_free(void)
{
	int i;

	t_min=90000;
	t_max=0;
	dt=time=0;
	dt0=0;

	for (i=n-1; i>=0; i--) {
		time=rt_get_time();
		kfree_rtskb(rtskbs[i]);
		dt0=rt_get_time()-time; 
		t_max=_max(t_max, dt0);
		t_min=_min(t_min, dt0);
		dt+=dt0;
	}
	printk("t_avg: %6dns \t t_min: %6dns \t t_max: %6dns \n", 
	       (int) (((int) count2nano(dt))/n), (int) count2nano(t_min), (int) count2nano(t_max));

        stop_rt_timer();

	printk("RTSKB-ALLOC: end\n");
}

module_init(rtskbs_alloc);
module_exit(rtskbs_free);

