/***
 *
 *  netshm_kerndemo.c
 *
 *  netshm - simple device providing a distributed pseudo shared memory
 *  Copyright (C) 2004 Jan Kiszka <jan.kiszka@web.de>
 *
 *  kernel mode demonstration module
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <../netshm.h>


static char     *shm_name      = "myNETSHM";
static int      shm_size       = 1000;
static int      shm_local_size = 0;
static int      shm_local_offs = 0;

MODULE_PARM(shm_name, "s");
MODULE_PARM(shm_size, "i");
MODULE_PARM(shm_local_size, "i");
MODULE_PARM(shm_local_offs, "i");
MODULE_PARM_DESC(shm_name, "netshm device name");
MODULE_PARM_DESC(shm_size, "number of shared integer values");
MODULE_PARM_DESC(shm_local_size, "number of modifiable integer values");
MODULE_PARM_DESC(shm_local_offs, "first modifiable integer value");

MODULE_LICENSE("GPL");

static int      netshm;
static RT_TASK  rt_demo_task;
static int      *shared_mem;



void demo_task(int arg)
{
    int i = 1;
    int j;
    int sum;
    int ret;


    while (1) {
        if ((i & 127) == 0) {
            rt_printk("Cycle %d - memory snapshot "
                      "(every 100th and 101th value):\n", i);
            for (j = 0; j < shm_size; j += 100) {
                rt_printk(" 0x%08X, 0x%08X ", shared_mem[j], shared_mem[j+1]);
                if ((j % 300) == 200)
                    rt_printk("\n");
            }
            rt_printk("\n");
        }

        if (shm_local_size > 0) {
            shared_mem[((shm_local_offs + 99) / 100) * 100]++;

            for (j = 0, sum = 0; j < shm_size; j += 100)
                sum += shared_mem[j];

            shared_mem[((shm_local_offs + 99) / 100) * 100 + 1] = sum;
        }

        ret = ioctl_rt(netshm, NETSHM_RTIOC_CYCLE, NULL);
        if (ret < 0) {
            rt_printk("ioctl_rt(NETSHM_RTIOC_CYCLE) = %d\n", ret);
            break;
        }

        i++;
    }
}



int __init init_module(void)
{
    int                         ret;
    struct netshm_attach_args   params;


    shared_mem = vmalloc(shm_size * sizeof(int));
    if (shared_mem == NULL)
        return -ENOMEM;
    memset(shared_mem, 0, shm_size * sizeof(int));

    netshm = open_rt(shm_name, O_RDWR);
    if (netshm < 0) {
        printk("open_rt = %d!\n", netshm);
        vfree(shared_mem);
        return netshm;
    }

    params.mem_start      = shared_mem;
    params.mem_size       = shm_size * sizeof(int);
    params.local_mem_offs = shm_local_offs * sizeof(int);
    params.local_mem_size = shm_local_size * sizeof(int);
    params.recv_task_prio = -1; /*default */
    params.xmit_prio      = -1; /*default */
    ret = ioctl_rt(netshm, NETSHM_RTIOC_ATTACH, &params);
    if (ret < 0) {
        printk("ioctl_rt(NETSHM_RTIOC_ATTACH) = %d!\n", ret);
        close_rt(netshm);
        vfree(shared_mem);
        return ret;
    }

    ret = rt_task_init(&rt_demo_task, demo_task, 0, 4096, 9, 0, NULL);
    if (ret != 0) {
        printk("rt_task_init = %d!\n", ret);
        close_rt(netshm);
        vfree(shared_mem);
        return ret;
    }
    rt_task_resume(&rt_demo_task);

    printk("netshm kernel demo started\n"
           " shm_name       = %s\n"
           " shm_size       = %d (%d bytes)\n",
           shm_name,
           shm_size, shm_size*sizeof(int));
    if (shm_local_size > 0)
        printk(" shm_local_size = %d (%d bytes)\n"
               " shm_local_offs = %d (byte address %d)\n",
               shm_local_size, shm_local_size*sizeof(int),
               shm_local_offs, shm_local_offs*sizeof(int));
    else
        printk(" - observer mode -\n");

    return 0;
}



void cleanup_module(void)
{
    /* Important: First close the device! */
    while (close_rt(netshm) == -EAGAIN) {
        printk("netshm_kerndemo: Device busy - waiting...\n");
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rt_task_delete(&rt_demo_task);
    vfree(shared_mem);
}
