/***
 *
 *  examples/fusion/frag_ip.c
 *
 *  sends fragmented IP packets to another frag_ip instance - fusion version
 *
 *  Copyright (C) 2003-2005 Jan Kiszka <jan.kiszka@web.de>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rtai/task.h>
#include <rtnet.h>

static char *dest_ip_s = "127.0.0.1";
static unsigned int size = 65505;
static unsigned int add_rtskbs = 75;

static int start_timer = 0;

#define CYCLE       1000*1000*1000   /* 1 s */
RT_TASK rt_xmit_task;
RT_TASK rt_recv_task;

#define PORT        37000

static struct sockaddr_in dest_addr;

static int sock;

static char buffer_out[64*1024];
static char buffer_in[64*1024];


#ifndef XNTHREAD_SPARE0
#define XNTHREAD_SPARE0 0x10000000
#endif

#define rt_printf(args, ...)                                \
{                                                           \
    printf(args, ## __VA_ARGS__);                           \
    rt_task_set_mode(T_PRIMARY, 0, NULL);                   \
}


void xmit_msg(void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov[2];
    unsigned short msgsize = size;


    rt_task_set_periodic(NULL, TM_NOW, CYCLE);
    while (1) {
        iov[0].iov_base = &msgsize;
        iov[0].iov_len  = sizeof(msgsize);
        iov[1].iov_base = buffer_out;
        iov[1].iov_len  = size;

        memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov     = iov;
        msg.msg_iovlen  = 2;

        rt_printf("Sending message of %d+2 bytes\n", size);
        ret = sendmsg_rt(sock, &msg, 0);
        if (ret != (int)(sizeof(msgsize) + size))
            rt_printf(" rt_sendmsg() = %d!\n", ret);

        rt_task_wait_period();
    }
}



void recv_msg(void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov[2];
    unsigned short msgsize = size;
    struct sockaddr_in addr;


    while (1) {
        iov[0].iov_base = &msgsize;
        iov[0].iov_len  = sizeof(msgsize);
        iov[1].iov_base = buffer_in;
        iov[1].iov_len  = size;

        memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov     = iov;
        msg.msg_iovlen  = 2;

        ret = recvmsg_rt(sock, &msg, 0);
        if (ret <= 0) {
            rt_printf(" rt_recvmsg() = %d\n", ret);
            return;
        } else {
            unsigned long ip = ntohl(addr.sin_addr.s_addr);

            rt_printf("received packet from %lu.%lu.%lu.%lu, length: %d+2, "
                "encoded length: %d,\n flags: %X, content %s\n", ip >> 24,
                (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
                ret-sizeof(msgsize), msgsize, msg.msg_flags,
                (memcmp(buffer_in, buffer_out, ret-sizeof(msgsize)) == 0) ?
                    "ok" : "corrupted");
        }
    }
}



void catch_signal(int sig)
{
}


int main(int argc, char *argv[])
{
    int ret;
    unsigned int i;
    struct sockaddr_in local_addr;
    struct in_addr dest_ip;


    while (1) {
        switch (getopt(argc, argv, "d:s:t")) {
            case 'd':
                dest_ip_s = optarg;
                break;

            case 's':
                size = atoi(optarg);
                break;

            case 't':
                start_timer = 1;
                break;

            case -1:
                goto end_of_opt;

            default:
                printf("usage: %s -d <dest_ip> -s <size> -t\n", argv[0]);
                return 0;
        }
    }
 end_of_opt:

    inet_aton(dest_ip_s, &dest_ip);

    if (size > 65505)
        size = 65505;

    signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);
    mlockall(MCL_CURRENT|MCL_FUTURE);

    printf("destination ip address %s=%08x\n", dest_ip_s, dest_ip.s_addr);
    printf("size %d\n", size);
    printf("start timer %d\n", start_timer);

    /* fill output buffer with test pattern */
    for (i = 0; i < sizeof(buffer_out); i++)
        buffer_out[i] = i & 0xFF;

    /* create rt-socket */
    sock = socket_rt(AF_INET,SOCK_DGRAM,0);
    if (sock < 0) {
        printf(" socket_rt() = %d!\n", sock);
        return sock;
    }

    /* extend the socket pool */
    ret = ioctl_rt(sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != (int)add_rtskbs) {
        printf(" ioctl_rt(RT_IOC_SO_EXTPOOL) = %d\n", ret);
        close_rt(sock);
        return -1;
    }

    /* bind the rt-socket to a port */
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    ret = bind_rt(sock, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        printf(" bind_rt() = %d!\n", ret);
        close_rt(sock);
        return ret;
    }

    /* set destination address */
    memset(&dest_addr, 0, sizeof(struct sockaddr_in));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr = dest_ip;

    if (start_timer) {
        rt_timer_start(TM_ONESHOT);
    }

    ret = rt_task_create(&rt_recv_task, "Receiver", 0, 10, 0);
    if (ret != 0) {
        printf(" rt_task_create(recv) = %d!\n", ret);
        close_rt(sock);
        return ret;
    }

    ret = rt_task_create(&rt_xmit_task, "Sender", 0, 9, 0);
    if (ret != 0) {
        printf(" rt_task_create(xmit) = %d!\n", ret);
        close_rt(sock);
        rt_task_delete(&rt_recv_task);
        return ret;
    }

    rt_task_start(&rt_recv_task, recv_msg, NULL);
    rt_task_start(&rt_xmit_task, xmit_msg, NULL);

    pause();

    if (start_timer)
        rt_timer_stop();

    /* Important: First close the socket! */
    while (close_rt(sock) == -EAGAIN) {
        printf("frag-ip: Socket busy - waiting...\n");
        sleep(1);
    }

    rt_task_delete(&rt_xmit_task);
    rt_task_delete(&rt_recv_task);

    return 0;
}
