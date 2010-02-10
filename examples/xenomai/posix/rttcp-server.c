/***
 *
 *  examples/xenomai/posix/rttcp-server.c
 *
 *  Simple RTNet TCP server - listens and sends back a packet
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
#include <unistd.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <limits.h>

#include <rtnet.h>

char *local_ip_s  = "";

#define RCV_PORT            36000
#define MAX_STRLENGTH       128
#define DEFAULT_ADD_BUFFERS 30

int add_rtskbs = DEFAULT_ADD_BUFFERS;

pthread_t receiver_task = 0;

struct conn_t {
    int sock;
    struct sockaddr_in client_addr;
    struct sockaddr_in local_addr;
};

void* receiver(void* arg)
{
    struct conn_t *connection = (struct conn_t*)arg;
    socklen_t len = sizeof(struct sockaddr_in);
    int sock = connection->sock;
    struct timeval tv;
    fd_set readset;
    int cnt = 0;
    char chr;
    int ret;

    /* bind the rt-socket to local_addr */
    connection->local_addr.sin_family = AF_INET;
    connection->local_addr.sin_port   = htons(RCV_PORT);
    if (bind(sock, (struct sockaddr *)&connection->local_addr,
             sizeof(struct sockaddr_in)) < 0) {
        perror("bind socket");
        return NULL;
    }

    /* Backlog is ignored, current realization just transmit state to LISTEN */
    if (listen(sock, 1) < 0) {
        perror("listen on socket");
        return NULL;
    }

    /* Warning, no new socket descriptor, only one connection */
    sock = accept(sock, (struct sockaddr *)&connection->client_addr, &len);
    if (sock < 0) {
        perror("accept connection");
        return NULL;
    }
    printf("connection from %s:%d\n",
           inet_ntoa(connection->client_addr.sin_addr),
           ntohs(connection->client_addr.sin_port));

    while (1) {
        FD_ZERO(&readset);
        FD_SET(sock, &readset);
        tv.tv_sec = 5;
        tv.tv_usec = 0;

        ret = select(sock + 1, &readset, NULL, NULL, &tv);
        if (ret <= 0) {
            if (ret == 0)
                fprintf(stderr, "timeout during select()\n");
            else
                perror("error on select()");
            return NULL;
        }

        if (FD_ISSET(sock, &readset)) {
            ret = read(sock, &chr, 1);
            if (ret <= 0) {
                if (ret == 0)
                    printf("connection closed\n");
                else
                    perror("error on read()");
                return NULL;
            }

            printf("%d: received %d bytes, message: %c\n", cnt++, ret, chr);
        }
    }
    return NULL;
}

int main(int argc, char** argv)
{
    struct conn_t connection;
    struct sched_param param;
    pthread_attr_t attr;
    int ret;

    while (1) {
        switch (getopt(argc, argv, "l:")) {
            case 'l':
                local_ip_s = optarg;
                break;

            case -1:
                goto end_of_opt;

            default:
                printf("usage: %s [-l <local_ip>]\n", argv[0]);
                return 0;
        }
    }
 end_of_opt:

    if (local_ip_s[0])
        inet_aton(local_ip_s, &connection.local_addr.sin_addr);
    else
        connection.local_addr.sin_addr.s_addr = INADDR_ANY;

    mlockall(MCL_CURRENT|MCL_FUTURE);

    printf("local ip address: %s = %08x\n",
           local_ip_s[0] ? local_ip_s : "INADDR_ANY",
           connection.local_addr.sin_addr.s_addr);

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
    param.sched_priority = 20;
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(&receiver_task, NULL, &receiver, &connection);
    if (ret) {
        perror("start real-time task");
        return 1;
    }

    pthread_join(receiver_task, NULL);
    close(connection.sock);

    return 0;
}
