/***
 *
 *  examples/event/rt_show_event.c
 *
 *  Collects and displays jitter of reported events.
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

#include <net/ip.h>

#include <rtnet_config.h>

#include <rtai.h>
#include <rtai_sched.h>

#ifdef HAVE_RTAI_SEM_H
#include <rtai_sem.h>
#endif

#include <rtnet.h>


#define SYNC_PORT       40000
#define REPORT_PORT     40001


static char* my_ip   = "";
static char* dest_ip = "10.255.255.255";
MODULE_PARM(my_ip, "s");
MODULE_PARM(dest_ip, "s");

MODULE_LICENSE("GPL");

static int                sock;
static RT_TASK            task;
static SEM                rx_sem;
static struct sockaddr_in local_addr;
static unsigned long      cur_index  = 0;
static RTIME              cur_min    = 0;
static RTIME              cur_max    = 0;
static unsigned long      max_jitter = 0;


int recv_callback(int socket, void* arg)
{
    rt_sem_signal(&rx_sem);

    return 0;
}



void recv_handler(int arg)
{
    struct msghdr      msg;
    struct iovec       iov;
    struct sockaddr_in addr;
    struct {
        RTIME         time_stamp;
        unsigned long count;
    } event;
    unsigned long      time_hi;
    unsigned long      time_lo;
    /*char*              addr_bytes = (char*)&addr.sin_addr.s_addr;*/
    unsigned long      jitter;


    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SYNC_PORT);
    addr.sin_addr.s_addr = rt_inet_aton(dest_ip);
    rt_socket_sendto(sock, NULL, 0, 0, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));

    while (1)
    {
        rt_sem_wait(&rx_sem);

        memset(&msg, 0, sizeof(msg));
        iov.iov_base       = &event;
        iov.iov_len        = sizeof(event);
        msg.msg_name       = &addr;
        msg.msg_namelen    = sizeof(addr);
        msg.msg_iov        = &iov;
        msg.msg_iovlen     = 1;
        msg.msg_control    = NULL;
        msg.msg_controllen = 0;

        if (rt_socket_recvmsg(sock, &msg, 0) > 0)
        {
            time_hi = ulldiv(event.time_stamp, 1000000, &time_lo);
            /*rt_printk("%d.%d.%d.%d reports event no. #%lu at %lu.%06lu ms global time\n",
                      addr_bytes[0], addr_bytes[1], addr_bytes[2], addr_bytes[3],
                      event.count, time_hi, time_lo);*/

            if (event.count == cur_index)
            {
                if (event.time_stamp < cur_min)
                    cur_min = event.time_stamp;
                else if (event.time_stamp > cur_max)
                    cur_max = event.time_stamp;
            }
            else
            {
                jitter = (unsigned long)cur_max - cur_min;
                if (jitter > max_jitter)
                {
                    max_jitter = jitter;
                    rt_printk("new worst-case synchronization jitter: %lu us\n", jitter/1000);
                }

                cur_min = event.time_stamp;
                cur_max = event.time_stamp;
                cur_index = event.count;
            }
        }
    }
}



int init_module(void)
{
    sock = rt_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family      = AF_INET;
    local_addr.sin_port        = htons(REPORT_PORT);
    local_addr.sin_addr.s_addr =
        (strlen(my_ip) != 0) ? rt_inet_aton(my_ip) : INADDR_ANY;

    if (rt_socket_bind(sock, (struct sockaddr*)&local_addr, sizeof(struct sockaddr_in)) < 0)
    {
        rt_printk("ERROR: Can't bind socket!\n");
        return 1;
    }

    rt_socket_callback(sock, recv_callback, NULL);

    rt_sem_init(&rx_sem, 0);

    rt_task_init(&task, recv_handler, 0, 4096, 11, 0, NULL);
    rt_task_resume(&task);

    return 0;
}



void cleanup_module(void)
{
    while (rt_socket_close(sock) == -EAGAIN) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rt_task_delete(&task);
    rt_sem_delete(&rx_sem);

    printk("worst-case synchronization jitter was: %lu us\n", max_jitter/1000);
}
