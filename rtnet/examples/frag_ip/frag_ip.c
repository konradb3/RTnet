/***
 *
 *  examples/frag_ip/frag_ip.c
 *
 *  sends fragmented IP packets to another frag_ip instance
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

#include <rtnet.h>


static char *dest_ip_s = "127.0.0.1";
static unsigned int size = 65505;
static int start_timer = 0;
static unsigned int add_rtskbs = 30;

MODULE_PARM(dest_ip_s, "s");
MODULE_PARM(size, "i");
MODULE_PARM(start_timer, "i");
MODULE_PARM(add_rtskbs, "i");
MODULE_PARM_DESC(dest_ip_s, "destination IP address");
MODULE_PARM_DESC(size, "message size (0-65505)");
MODULE_PARM_DESC(start_timer, "set to non-zero to start scheduling timer");
MODULE_PARM_DESC(add_rtskbs, "number of additional rtskbs (default: 30)");

MODULE_LICENSE("GPL");

#define CYCLE       1000*1000*1000   /* 1 s */
RT_TASK rt_task;

#define PORT        37000

static struct sockaddr_in dest_addr;

static int sock;

static char buffer_out[64*1024];
static char buffer_in[64*1024];



void send_msg(void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov[2];
    unsigned short msgsize = size;


    while(1)
    {
        iov[0].iov_base = &msgsize;
        iov[0].iov_len  = sizeof(msgsize);
        iov[1].iov_base = buffer_out;
        iov[1].iov_len  = size;

        memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov     = iov;
        msg.msg_iovlen  = 2;

        rt_printk("Sending message of %d+2 bytes\n", size);
        ret = rt_socket_sendmsg(sock, &msg, 0);
        if (ret != (int)(sizeof(msgsize) + size))
            rt_printk(" rt_socket_sendmsg() = %d!\n", ret);

        rt_task_wait_period();
    }
}



int recv_msg(int s, void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov[2];
    unsigned short msgsize = size;
    struct sockaddr_in addr;


    iov[0].iov_base = &msgsize;
    iov[0].iov_len  = sizeof(msgsize);
    iov[1].iov_base = buffer_in;
    iov[1].iov_len  = size;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name    = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov     = iov;
    msg.msg_iovlen  = 2;

    ret = rt_recvmsg(sock, &msg, MSG_DONTWAIT);
    if (ret <= 0)
        rt_printk(" rt_recvmsg() = %d\n", ret);
    else
    {
        unsigned long ip = ntohl(addr.sin_addr.s_addr);

        rt_printk("received packet from %d.%d.%d.%d, length: %d+2, encoded "
                  "length: %d,\n flags: %X, content %s\n", ip >> 24,
                  (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
                  ret-sizeof(msgsize), msgsize, msg.msg_flags,
                  (memcmp(buffer_in, buffer_out, ret-sizeof(msgsize)) == 0) ?
                  "ok" : "corrupted");
    }

    return 0;
}



int init_module(void)
{
    int ret;
    unsigned int i;
    struct sockaddr_in local_addr;
    unsigned long dest_ip = rt_inet_aton(dest_ip_s);

    if (size > 65505)
        size = 65505;

    rt_printk("destination ip address %s=%08x\n", dest_ip_s,
              (unsigned int)dest_ip);
    rt_printk("size %d\n", size);
    rt_printk("start timer %d\n", start_timer);

    /* fill output buffer with test pattern */
    for (i = 0; i < sizeof(buffer_out); i++)
        buffer_out[i] = i & 0xFF;

    /* create rt-socket */
    sock = rt_socket(AF_INET,SOCK_DGRAM,0);
    if (sock < 0)
    {
        rt_printk(" rt_socket() = %d!\n", sock);
        return sock;
    }

    /* extend the socket pool */
    ret = rt_setsockopt(sock, SOL_SOCKET, RT_SO_EXTPOOL, &add_rtskbs,
                        sizeof(add_rtskbs));
    if (ret != (int)add_rtskbs) {
        rt_printk("rt_setsockopt(RT_SO_EXTPOOL) = %d\n", ret);
        rt_close(sock);
        return -1;
    }

    /* bind the rt-socket to a port */
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    ret = rt_bind(sock, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in));
    if (ret < 0)
    {
        rt_printk(" rt_socket_bind() = %d!\n", ret);
        rt_close(sock);
        return ret;
    }

    /* set destination address */
    memset(&dest_addr, 0, sizeof(struct sockaddr_in));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = dest_ip;

    /* install callback handler */
    rt_socket_callback(sock, recv_msg, NULL);

    if (start_timer)
    {
        rt_set_oneshot_mode();
        start_rt_timer(0);
    }

    ret = rt_task_init(&rt_task,(void *)send_msg, 0, 4096, 10, 0, NULL);
    if (ret != 0)
    {
        rt_printk(" rt_task_init() = %d!\n", ret);
        rt_close(sock);
        return ret;
    }

    ret = rt_task_make_periodic_relative_ns(&rt_task, 0, CYCLE);
    if (ret != 0)
    {
        rt_printk(" rt_task_make_periodic_relative_ns() = %d!\n", ret);
        rt_close(sock);
        return ret;
    }

    return 0;
}




void cleanup_module(void)
{
    if (start_timer)
        stop_rt_timer();

    rt_task_delete(&rt_task);

    while (rt_socket_close(sock) == -EAGAIN) {
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }
}
