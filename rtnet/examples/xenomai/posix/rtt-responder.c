/***
 *
 *  examples/xenomai/posix/rtt-responder.c
 *
 *  Round-Trip Time Responder - listens and sends back a packet
 *
 *  Based on Ulrich Marx's module, later ported over user space POSIX.
 *
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2002 Marc Kleine-Budde <kleine-budde@gmx.de>
 *                2004, 2006 Jan Kiszka <jan.kiszka@web.de>
 *
 *  RTnet - real-time networking example
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

#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <arpa/inet.h>

#include <rtnet.h>

char *dest_ip_s = "";
char *local_ip_s  = "";
unsigned int reply_size = 0;

pthread_t rt_thread;

#define RCV_PORT    36000
#define XMT_PORT    35999

struct sockaddr_in dest_addr;

int sock;

char buffer[65536];


void *responder(void* arg)
{
    struct sched_param  param = { .sched_priority = 81 };
    struct msghdr       rx_msg;
    struct iovec        iov;
    ssize_t             ret;


    if (dest_addr.sin_addr.s_addr == INADDR_ANY) {
        rx_msg.msg_name    = &dest_addr;
        rx_msg.msg_namelen = sizeof(dest_addr);
    } else {
        rx_msg.msg_name    = NULL;
        rx_msg.msg_namelen = 0;
    }
    rx_msg.msg_namelen     = sizeof(struct sockaddr_in);
    rx_msg.msg_iov         = &iov;
    rx_msg.msg_iovlen      = 1;
    rx_msg.msg_control     = NULL;
    rx_msg.msg_controllen  = 0;

    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    while(1) {
        iov.iov_base = &buffer;
        iov.iov_len  = sizeof(buffer);

        ret = recvmsg(sock, &rx_msg, 0);
        if (ret <= 0) {
            printf("terminating responder thread\n");
            return NULL;
        }

        sendto(sock, &buffer, reply_size ? : ret, 0,
               (struct sockaddr *)&dest_addr,
               sizeof(struct sockaddr_in));
    }
}


void catch_signal(int sig)
{
}


int main(int argc, char *argv[])
{
    struct sockaddr_in local_addr;
    int add_rtskbs = 30;
    pthread_attr_t thattr;
    int ret;


    while (1) {
        switch (getopt(argc, argv, "d:l:s:")) {
            case 'd':
                dest_ip_s = optarg;
                break;

            case 'l':
                local_ip_s = optarg;
                break;

            case 's':
                reply_size = atoi(optarg);
                break;

            case -1:
                goto end_of_opt;

            default:
                printf("usage: %s [-d <dest_ip>] [-l <local_ip>] "
                       "[-s <reply_size>]\n", argv[0]);
                return 0;
        }
    }
 end_of_opt:

    if (dest_ip_s[0]) {
        inet_aton(dest_ip_s, &dest_addr.sin_addr);
        dest_addr.sin_port = htons(XMT_PORT);
    } else
        dest_addr.sin_addr.s_addr = INADDR_ANY;

    if (local_ip_s[0])
        inet_aton(local_ip_s, &local_addr.sin_addr);
    else
        local_addr.sin_addr.s_addr = INADDR_ANY;

    if (reply_size > 65505)
        reply_size = 65505;
    else if (reply_size < sizeof(struct timespec))
        reply_size = sizeof(struct timespec);

    signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);
    signal(SIGHUP, catch_signal);
    mlockall(MCL_CURRENT|MCL_FUTURE);

    printf("destination ip address: %s = %08x\n",
           dest_ip_s[0] ? dest_ip_s : "SENDER", dest_addr.sin_addr.s_addr);
    printf("local ip address: %s = %08x\n",
           local_ip_s[0] ? local_ip_s : "INADDR_ANY", local_addr.sin_addr.s_addr);
    printf("reply size: %d\n", reply_size);

    /* create rt-socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket cannot be created");
        return 1;
    }

    /* bind the rt-socket to local_addr */
    local_addr.sin_family = AF_INET;
    local_addr.sin_port   = htons(RCV_PORT);
    if ((ret = bind(sock, (struct sockaddr *)&local_addr,
                    sizeof(struct sockaddr_in))) < 0) {
        close(sock);
        perror("cannot bind to local ip/port");
        return 1;
    }

    /* extend the socket pool */
    ret = ioctl(sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != add_rtskbs)
        printf("WARNING: ioctl(RTNET_RTIOC_EXTPOOL) = %d\n", ret);

    /* create reply rt-thread */
    pthread_attr_init(&thattr);
    pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&thattr, PTHREAD_STACK_MIN);
    ret = pthread_create(&rt_thread, &thattr, &responder, NULL);
    if (ret) {
        close(sock);
        errno = ret; perror("pthread_create failed");
        return 1;
    }

    pause();

    /* Important: First close the socket! */
    while ((close(sock) < 0) && (errno == EAGAIN)) {
        printf("socket busy - waiting...\n");
        sleep(1);
    }

    pthread_kill(rt_thread, SIGHUP);
    pthread_join(rt_thread, NULL);

    return 0;
}
