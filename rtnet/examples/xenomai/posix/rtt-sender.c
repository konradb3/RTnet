/***
 *
 *  examples/xenomai/posix/rtt-requester.c
 *
 *  Round-Trip Time Requester - sends packet, receives echo, evaluates
 *                              and displays per-station round-trip times
 *
 *  Based on Ulrich Marx's module, adopted to RTmac and later ported over
 *  user space POSIX.
 *
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2002 Marc Kleine-Budde <kleine-budde@gmx.de>
 *                2006 Jan Kiszka <jan.kiszka@web.de>
 *
 *  RTnet - real-time networking example
 *  RTmac - real-time media access control example
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
#include <mqueue.h>
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

char *dest_ip_s = "127.0.0.1";
char *local_ip_s = "";
unsigned int cycle = 50000; /* 50 ms */

pthread_t xmit_thread;
pthread_t recv_thread;

#define RCV_PORT                35999
#define XMT_PORT                36000

#define DEFAULT_ADD_BUFFERS     30

struct sockaddr_in dest_addr;

int sock;
mqd_t mq;

#define BUFSIZE 1500
union {
    char            data[BUFSIZE];
    struct timespec tx_date;
} packet;

struct station_stats {
    struct in_addr  addr;
    long long       last, min, max;
    unsigned long   count;
};

struct packet_stats {
    struct in_addr  addr;
    long long       rtt;
};

#define MAX_STATIONS 100
static struct station_stats station[MAX_STATIONS];


static struct station_stats *lookup_stats(struct in_addr addr)
{
    int i;

    for (i = 0; i < MAX_STATIONS; i++) {
        if (station[i].addr.s_addr == addr.s_addr)
            break;
        if (station[i].addr.s_addr == 0) {
            station[i].addr = addr;
            station[i].min  = LONG_MAX;
            station[i].max  = LONG_MIN;
            break;
        }
    }
    if (i == MAX_STATIONS)
        return NULL;
    return &station[i];
}


void *transmitter(void *arg)
{
    struct sched_param  param = { .sched_priority = 80 };
    struct timespec     next_period;
    struct timespec     tx_date;


    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    clock_gettime(CLOCK_MONOTONIC, &next_period);

    while(1) {
        next_period.tv_nsec += cycle * 1000;
        if (next_period.tv_nsec >= 1000000000) {
            next_period.tv_nsec = 0;
            next_period.tv_sec++;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);

        clock_gettime(CLOCK_MONOTONIC, &tx_date);

        /* transmit the request packet containing the local time */
        if (sendto(sock, &tx_date, sizeof(tx_date), 0,
                   (struct sockaddr *)&dest_addr,
                   sizeof(struct sockaddr_in)) < 0) {
            if (errno == EBADF)
                printf("terminating transmitter thread\n");
            else
                perror("sendto failed");
            return NULL;
        }
    }
}


void *receiver(void *arg)
{
    struct sched_param  param = { .sched_priority = 82 };
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;
    struct timespec     rx_date;
    struct packet_stats stats;
    int                 ret;


    msg.msg_name       = &addr;
    msg.msg_namelen    = sizeof(addr);
    msg.msg_iov        = &iov;
    msg.msg_iovlen     = 1;
    msg.msg_control    = NULL;
    msg.msg_controllen = 0;

    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    while (1) {
        iov.iov_base = &packet;
        iov.iov_len  = sizeof(packet);

        ret = recvmsg(sock, &msg, 0);
        if (ret <= 0) {
            printf("terminating receiver thread\n");
            return NULL;
        }

        clock_gettime(CLOCK_MONOTONIC, &rx_date);
        stats.rtt = rx_date.tv_sec * 1000000000LL + rx_date.tv_nsec;
        stats.rtt -= packet.tx_date.tv_sec * 1000000000LL +
            packet.tx_date.tv_nsec;
        stats.addr = addr.sin_addr;

        mq_send(mq, (char *)&stats, sizeof(stats), 0);
    }
}


void catch_signal(int sig)
{
    mq_close(mq);
}


int main(int argc, char *argv[])
{
    struct sched_param param = { .sched_priority = 1 };
    struct sockaddr_in local_addr;
    int add_rtskbs = DEFAULT_ADD_BUFFERS;
    pthread_attr_t thattr;
    char mqname[32];
    struct mq_attr mqattr;
    int stations = 0;
    int ret;


    while (1) {
        switch (getopt(argc, argv, "d:l:c:b:")) {
            case 'd':
                dest_ip_s = optarg;
                break;

            case 'l':
                local_ip_s = optarg;
                break;

            case 'c':
                cycle = atoi(optarg);
                break;

            case 'b':
                add_rtskbs = atoi(optarg);

            case -1:
                goto end_of_opt;

            default:
                printf("usage: %s [-d <dest_ip>] [-l <local_ip>] "
                       "[-c <cycle_microsecs>] [-b <add_buffers>]\n",
                       argv[0]);
                return 0;
        }
    }
 end_of_opt:

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port   = htons(XMT_PORT);
    if (dest_ip_s[0])
        inet_aton(dest_ip_s, &dest_addr.sin_addr);
    else
        dest_addr.sin_addr.s_addr = INADDR_ANY;

    if (local_ip_s[0])
        inet_aton(local_ip_s, &local_addr.sin_addr);
    else
        local_addr.sin_addr.s_addr = INADDR_ANY;

    signal(SIGTERM, catch_signal);
    signal(SIGINT, catch_signal);
    signal(SIGHUP, catch_signal);
    mlockall(MCL_CURRENT|MCL_FUTURE);

    printf("destination ip address: %s = %08x\n",
           dest_ip_s[0] ? dest_ip_s : "SENDER", dest_addr.sin_addr.s_addr);
    printf("local ip address: %s = %08x\n",
           local_ip_s[0] ? local_ip_s : "INADDR_ANY", local_addr.sin_addr.s_addr);
    printf("cycle: %d us\n", cycle);

    /* create rt-socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        perror("socket cannot be created");
        return 1;
    }

    /* bind the rt-socket to local_addr */
    local_addr.sin_family = AF_INET;
    local_addr.sin_port   = htons(RCV_PORT);
    if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("cannot bind to local ip/port");
        close(sock);
        return 1;
    }

    /* extend the socket pool */
    ret = ioctl(sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != add_rtskbs)
        perror("WARNING: ioctl(RTNET_RTIOC_EXTPOOL)");

    /* create statistics message queue */
    snprintf(mqname, sizeof(mqname), "/rtt-sender-%d", getpid());
    mqattr.mq_flags   = 0;
    mqattr.mq_maxmsg  = 100;
    mqattr.mq_msgsize = sizeof(struct packet_stats);
    mq = mq_open(mqname, O_RDWR | O_CREAT | O_EXCL, 0600, &mqattr);
    if (mq == (mqd_t)-1) {
        perror("opening mqueue failed");
        close(sock);
        return 1;
    }

    /* create transmitter rt-thread */
    pthread_attr_init(&thattr);
    pthread_attr_setdetachstate(&thattr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&thattr, PTHREAD_STACK_MIN);
    ret = pthread_create(&recv_thread, &thattr, &receiver, NULL);
    if (ret) {
        errno = ret; perror("pthread_create(receiver) failed");
        close(sock);
        mq_close(mq);
        return 1;
    }

    /* create receiver rt-thread */
    ret = pthread_create(&xmit_thread, &thattr, &transmitter, NULL);
    if (ret) {
        errno = ret; perror("pthread_create(transmitter) failed");
        close(sock);
        mq_close(mq);
        pthread_kill(recv_thread, SIGHUP);
        pthread_join(recv_thread, NULL);
        return 1;
    }

    pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

    while (1) {
        struct packet_stats pack;
        struct station_stats *pstat;
        int nr;

        ret = mq_receive(mq, (char *)&pack, sizeof(pack), NULL);
        if (ret < (int)sizeof(pack))
            break;

        pstat = lookup_stats(pack.addr);
        if (!pstat)
            continue;

        pstat->last = pack.rtt;
        if (pstat->last < pstat->min)
            pstat->min = pstat->last;
        if (pstat->last > pstat->max)
            pstat->max = pstat->last;
        pstat->count++;

        nr = pstat - &station[0];
        if (nr >= stations) {
            stations = nr+1;
            printf("\n");
        }

        printf("\033[%dA%s\t%9.3f us, min=%9.3f us, max=%9.3f us, count=%ld\n",
               stations-nr, inet_ntoa(pack.addr), (float)pstat->last/1000,
               (float)pstat->min/1000, (float)pstat->max/1000, pstat->count);
        for (nr = stations-nr-1; nr > 0; nr --)
            printf("\n");
    }

    /* This call also leaves primary mode, required for socket cleanup. */
    printf("shutting down\n");

    /* Note: The following loop is no longer required since Xenomai 2.4,
     *       plain close works as well. */
    while ((close(sock) < 0) && (errno == EAGAIN)) {
        printf("socket busy - waiting...\n");
        sleep(1);
    }

    pthread_join(xmit_thread, NULL);
    pthread_kill(recv_thread, SIGHUP);
    pthread_join(recv_thread, NULL);

    return 0;
}
