/***
 *
 *  examples/xenomai/posix/rttcp-client.c
 *
 *  Simple RTNet TCP client - sends packet to a server
 *
 *  Copyright (C) 2009 Vladimir Zapolskiy <vladimir.zapolskiy@siemens.com>
 *
 *  RTnet - real-time networking example
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2, as
 *  published by the Free Software Foundation.
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

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <limits.h>

#include <rtnet.h>

char *dest_ip_s = "127.0.0.1";
char *local_ip_s = "";
unsigned int cycle = 500000; /* 500 ms */

#define RCV_PORT                35999
#define XMT_PORT                36000
#define DEFAULT_LOOPS           10
#define DEFAULT_ADD_BUFFERS     30

int add_rtskbs = DEFAULT_ADD_BUFFERS;

pthread_t sender_task = 0;

struct conn_t {
    int nloops;
    int sock;
    struct sockaddr_in dest_addr;
    struct sockaddr_in local_addr;
};

const char msg[] = "Hello";

void *sender(void *arg)
{
    struct conn_t *connection = (struct conn_t *)arg;
    int sock = connection->sock;
    int ret, i, sopt_len;
    struct timeval  tv;
    struct timespec sleep_period;

    sopt_len = 1;
    if ((ret = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &sopt_len,
                          sizeof(sopt_len))) < 0) {
        perror("set SO_KEEPALIVE socket option");
        return NULL;
    }

    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    if ((ret = setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv,
                          sizeof(tv))) < 0) {
        perror("set SO_SNDTIMEO socket option");
        return NULL;
    }

    if ((ret = bind(sock, (struct sockaddr*)&connection->local_addr,
        sizeof(struct sockaddr_in))) < 0)
    {
        perror("bind socket");
        return NULL;
    }

    if ((ret = connect(sock, (struct sockaddr*)&connection->dest_addr,
        sizeof(struct sockaddr_in))) < 0)
    {
        perror("connect to server");
        return NULL;
    }

    sleep_period.tv_nsec = cycle * 1000;

    for (i = 1; i <= connection->nloops; i++) {
        clock_gettime(CLOCK_MONOTONIC, &sleep_period);

        sleep_period.tv_nsec += cycle * 1000;
        if (sleep_period.tv_nsec >= 1000000000) {
            sleep_period.tv_nsec = 0;
            sleep_period.tv_sec++;
        }

        ret = write(sock, msg, sizeof(msg));
        if (ret <= 0) {
            if (ret == 0)
                printf("connection closed by peer\n");
            else
                perror("write to socket");
            return NULL;
        }
        printf("%d: wrote %d bytes to socket\n", i, ret);

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &sleep_period, NULL);
    }

    return NULL;
}

int main(int argc, char** argv)
{
    struct conn_t connection = { .nloops = DEFAULT_LOOPS };
    struct sched_param param;
    pthread_attr_t attr;
    int local_port = RCV_PORT;
    int ret;

    while (1) {
        switch (getopt(argc, argv, "d:l:p:n:")) {
            case 'd':
                dest_ip_s = optarg;
                break;

            case 'l':
                local_ip_s = optarg;
                break;

            case 'p':
                local_port = atoi(optarg);
                break;

            case 'n':
                connection.nloops = atoi(optarg);
                break;

            case -1:
                goto end_of_opt;

            default:
                printf("usage: %s [-d <dest_ip>] [-l <local_ip>] "
                       "[-p <local port>] [-n <number of loops>]\n", argv[0]);
                return 0;
        }
    }
 end_of_opt:

    connection.dest_addr.sin_family = AF_INET;
    connection.dest_addr.sin_port   = htons(XMT_PORT);
    if (dest_ip_s[0])
        inet_aton(dest_ip_s, &connection.dest_addr.sin_addr);
    else
        connection.dest_addr.sin_addr.s_addr = INADDR_ANY;

    connection.local_addr.sin_family = AF_INET;
    connection.local_addr.sin_port = htons(local_port);
    if (local_ip_s[0])
        inet_aton(local_ip_s, &connection.local_addr.sin_addr);
    else
        connection.local_addr.sin_addr.s_addr = INADDR_ANY;

    mlockall(MCL_CURRENT|MCL_FUTURE);

    printf("destination ip address: %s = %08x\n",
           dest_ip_s[0] ? dest_ip_s : "SENDER",
           connection.dest_addr.sin_addr.s_addr);
    printf("local ip address: %s = %08x\n",
           local_ip_s[0] ? local_ip_s : "INADDR_ANY",
           connection.local_addr.sin_addr.s_addr);
    printf("port: %d\n", local_port);

    /* create rt-socket */
    if ((connection.sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket create");
        return 1;
    }

    /* extend the socket pool (optional, will fail with non-RT sockets) */
    ret = ioctl(connection.sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != DEFAULT_ADD_BUFFERS)
        perror("ioctl(RTNET_RTIOC_EXTPOOL)");

    pthread_attr_init(&attr);
    pthread_attr_setinheritsched(&attr, 1);
    pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
    param.sched_priority = 80;
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(&sender_task, &attr, &sender, &connection);
    if (ret) {
        perror("start real-time task");
        return 1;
    }

    pthread_join(sender_task, NULL);
    close(connection.sock);

    return 0;
}
