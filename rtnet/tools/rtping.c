/***
 *
 *  tools/rtping.c
 *  sends real-time ICMP echo requests
 *
 *  rtnet - real-time networking subsystem
 *  Copyright (C) 2004 Jan Kiszka <jan.kiszka@web.de>
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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <netinet/in.h>

#include <ipv4.h>


int             f;
struct ipv4_cmd cmd;
struct in_addr  addr;
int             count    = 0;
int             delay    = 1000;
unsigned int    sent     = 0;
unsigned int    received = 0;


void help(void)
{
    fprintf(stderr, "Usage:\n"
        "\trtping [-c count] [-i interval] [-s packetsize] <addr>\n"
        );

    exit(1);
}



int getintopt(int argc, int pos, char *argv[], int min)
{
    int result;


    if (pos >= argc)
        help();
    if ((sscanf(argv[pos], "%u", &result) != 1) || (result < min)) {
        fprintf(stderr, "invalid parameter: %s %s\n", argv[pos-1], argv[pos]);
        exit(1);
    }

    return result;
}



void print_statistics()
{
    printf("\n--- %s rtping statistics ---\n"
           "%d packets transmitted, %d received, %d%% packet loss\n",
           inet_ntoa(addr.s_addr), sent, received,
           100 - ((received * 100) / sent));
    exit(0);
}



void terminate(int signal)
{
    print_statistics();
}



void ping(int signal)
{
    int ret;


    cmd.args.ping.ip_addr = addr.s_addr;
    sent++;

    ret = ioctl(f, IOC_RT_PING, &cmd);
    if (ret < 0) {
        if (errno == ETIME)
            goto done;
        perror("ioctl");
        exit(1);
    }

    received++;
    printf("%d bytes from %s: icmp_seq=%d time=%.1f us\n",
           ret, inet_ntoa(cmd.args.ping.ip_addr), cmd.args.ping.sequence,
           (float)cmd.args.ping.rtt / (float)1000);

  done:
    if (cmd.args.ping.sequence++ == count)
        print_statistics();
}



int main(int argc, char *argv[])
{
    const char          rtnet_dev[] = "/dev/rtnet";
    struct timeval      time;
    struct itimerval    timer = {{0, 0}, {0, 1}};
    int                 i;


    if (argc < 2)
        help();

    gettimeofday(&time, NULL);
    cmd.args.ping.id       = time.tv_usec & 0xFFFF;
    cmd.args.ping.sequence = 1;
    cmd.args.ping.msg_size = 56;
    cmd.args.ping.timeout  = 500;

    for (i = 1; i < argc-1; i++) {
        if (strcmp(argv[i], "-c") == 0)
            count = getintopt(argc, ++i, argv, 1);
        else if (strcmp(argv[i], "-i") == 0)
            delay = getintopt(argc, ++i, argv, 1);
        else if (strcmp(argv[i], "-s") == 0) {
            cmd.args.ping.msg_size = getintopt(argc, ++i, argv, 0);
            if (cmd.args.ping.msg_size > 1472)
                cmd.args.ping.msg_size = 1472;
        } else
            help();
    }

    f = open(rtnet_dev, O_RDWR);
    if (f < 0) {
        perror(rtnet_dev);
        exit(1);
    }

    if (!inet_aton(argv[i], &addr))
        help();

    printf("Real-time PING %s %d(%d) bytes of data.\n",
           inet_ntoa(addr.s_addr), cmd.args.ping.msg_size,
           cmd.args.ping.msg_size + 28);

    signal(SIGINT, terminate);
    signal(SIGALRM, ping);
    timer.it_interval.tv_sec  = delay / 1000;
    timer.it_interval.tv_usec = (delay % 1000) * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);

    while (1) pause();
}
