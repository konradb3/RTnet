/***
 *
 *  examples/event/rt_event.c
 *
 *  Example for tdma-based RTmac, global time and cycle based
 *  packet transmission.
 *
 *  Copyright (C) 2003 Jan Kiszka <Jan.Kiszka@Uweb.de>
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/io.h>

#include <net/ip.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtnet.h>
#include <tdma.h>

#define DATA            io
#define STATUS          io+1
#define CONTROL         io+2

#define SYNC_PORT       40000
#define REPORT_PORT     40001


static int io  = 0x378;
static int irq = 7;
MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");

static char* rteth_dev = "rteth0";
static char* my_ip     = "";
static char* dest_ip   = "10.255.255.255";
MODULE_PARM(rteth_dev, "s");
MODULE_PARM(my_ip, "s");
MODULE_PARM(dest_ip, "s");

static unsigned long      irq_count = 0;
static struct rtmac_tdma* tdma;
static int                sock;
static struct sockaddr_in dest_addr;
static RT_TASK            task;
static SEM                event_sem;
static RTIME              time_stamp;


void irq_handler(void)
{
    time_stamp = rt_get_time_ns() + tdma_get_delta_t(tdma);
    irq_count++;

    rt_sem_signal(&event_sem);
}



void event_handler(int arg)
{
    struct {
        RTIME         time_stamp;
        unsigned long count;
    } packet;


    while (1)
    {
        rt_sem_wait(&event_sem);


        rt_disable_irq(irq);

        packet.time_stamp = time_stamp;
        packet.count      = irq_count;        

        rt_enable_irq(irq);


        tdma_wait_sof(tdma);


        rt_socket_sendto(sock, &packet, sizeof(packet), 0,
                         (struct sockaddr*)&dest_addr, sizeof(struct sockaddr_in));
    }
}



int sync_callback(int socket, void* arg)
{
    struct msghdr      msg;


    irq_count = 0;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name       = NULL;
    msg.msg_namelen    = 0;
    msg.msg_iov        = NULL;
    msg.msg_iovlen     = 0;
    msg.msg_control    = NULL;
    msg.msg_controllen = 0;

    rt_socket_recvmsg(socket, &msg, 0);

    return 0;
}



int init_module(void)
{
    struct sockaddr_in local_addr;


    printk("rt_event is using the following parameters:\n"
           "    io      = 0x%04X\n"
           "    irq     = %d\n"
           "    my_ip   = %s\n"
           "    dest_ip = %s\n", io, irq, my_ip, dest_ip);


    tdma = tdma_get_by_name(rteth_dev);
    if (!tdma)
    {
        printk("ERROR: RTmac/TDMA not loaded!\n");
        return 1;
    }


    sock = rt_socket(AF_INET,SOCK_DGRAM,0);

    if (my_ip[0] == '\0')
    {
        printk("ERROR: my_ip must be specified!\n");
        return 1;
    }
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(SYNC_PORT);
    local_addr.sin_addr.s_addr = rt_inet_aton(my_ip);
    rt_socket_bind(sock, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_in));

    memset(&dest_addr, 0, sizeof(struct sockaddr_in));
    dest_addr.sin_family      = AF_INET;
    dest_addr.sin_port        = htons(REPORT_PORT);
    dest_addr.sin_addr.s_addr = rt_inet_aton(dest_ip);

    rt_socket_callback(sock, sync_callback, NULL);


    rt_task_init(&task, event_handler, 0, 4096, 10, 0, NULL);
    rt_sem_init(&event_sem, 0);


    outb(0x10, CONTROL);
    if (rt_request_global_irq(irq, irq_handler) != 0)
    {
        printk("ERROR: irq not available!\n");
        goto Cleanup1;
    }
    rt_startup_irq(irq);
    rt_enable_irq(irq);


    rt_task_resume(&task);

    return 0;

Cleanup1:
    rt_task_delete(&task);
    rt_sem_delete(&event_sem);

    return 1;
}



void cleanup_module(void)
{
    rt_disable_irq(irq);
    rt_shutdown_irq(irq);
    rt_free_global_irq(irq);

    rt_task_delete(&task);
    rt_sem_delete(&event_sem);

    rt_socket_close(sock);
}
