/***
 *
 *  rtmac/examples/mrtt/rt_client.c
 *
 *  client part - sends packet, receives echo, passes them by fifo to
 *                userspace app (broadcast variant)
 *
 *  based on Ulrich Marx's module, adopted to rtmac
 *
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2002 Marc Kleine-Budde <kleine-budde@gmx.de>
 *
 *  rtnet - real-time networking example
 *  rtmac - real-time media access control example
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

#include <net/ip.h>

#include <rtnet_sys.h>
#include <rtnet.h>

static char *local_ip_s = "";
static char *broadcast_ip_s = "127.0.0.1";
static int cycle = 1*1000*1000; // = 1 s

struct mrtt_rx_packet {
    nanosecs_t  rx;
    nanosecs_t  tx;
    u32         ip_addr;
};

MODULE_PARM(local_ip_s ,"s");
MODULE_PARM(broadcast_ip_s,"s");
MODULE_PARM(cycle, "i");
MODULE_PARM_DESC(local_ip_s, "rt_echo_client: lokal ip-address");
MODULE_PARM_DESC(broadcast_ip_s, "rt_echo_client: broadcast ip-address");
MODULE_PARM_DESC(cycle, "cycletime in us");

MODULE_LICENSE("GPL");

rtos_task_t xmit_task;
rtos_task_t recv_task;

#define RCV_PORT    35999
#define SRV_PORT    36000

static struct sockaddr_in broadcast_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1500
static char buffer[BUFSIZE];

#define PRINT_FIFO  0
rtos_fifo_t print_fifo;


void process(void * arg)
{
    rtos_time_t time;
    nanosecs_t  tx_time;


    while(1) {
        rtos_get_time(&time);
        tx_time = rtos_time_to_nanosecs(&time);

        /* send the time   */
        sendto_rt(sock, &tx_time, sizeof(tx_time), 0,
                  (struct sockaddr *)&broadcast_addr,
                  sizeof(struct sockaddr_in));

        /* wait one period */
        rtos_task_wait_period();
    }
}



void echo_rcv(void *arg)
{
    int                     ret=0;
    struct msghdr           msg;
    struct iovec            iov;
    struct sockaddr_in      addr;
    struct mrtt_rx_packet   rx_packet;
    rtos_time_t             time;


    while (1) {
        iov.iov_base = &buffer;
        iov.iov_len = BUFSIZE;

        msg.msg_name = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;

        ret = recvmsg_rt(sock, &msg, 0);

        if ((ret > 0) && (msg.msg_namelen == sizeof(struct sockaddr_in))) {
            struct sockaddr_in *sin = msg.msg_name;

            /* get the time    */
            rtos_get_time(&time);
            rx_packet.rx = rtos_time_to_nanosecs(&time);
            memcpy(&rx_packet.tx, buffer, sizeof(rx_packet.tx));
            rx_packet.ip_addr = sin->sin_addr.s_addr;

            rtos_fifo_put(&print_fifo, &rx_packet,
                          sizeof(struct mrtt_rx_packet));
        } else
            break;
    }
}


int init_module(void)
{
    unsigned int    add_rtskbs = 30;
    rtos_time_t     period;
    int             ret;
    unsigned long   local_ip;
    unsigned long   broadcast_ip;


    if (strlen(local_ip_s) != 0)
        local_ip = rt_inet_aton(local_ip_s);
    else
        local_ip = INADDR_ANY;
    broadcast_ip = rt_inet_aton(broadcast_ip_s);

    rtos_fifo_create(&print_fifo, PRINT_FIFO, 40000);

    rtos_print("local     ip address %s=%8x\n", local_ip_s,
              (unsigned int)local_ip);
    rtos_print("broadcast ip address %s=%8x\n", broadcast_ip_s,
              (unsigned int)broadcast_ip);

    /* create rt-socket */
    rtos_print("create rtsocket\n");
    if ((sock = socket_rt(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        rtos_print("socket not created\n");
        return sock;
    }

    /* extend the socket pool */
    ret = ioctl_rt(sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != (int)add_rtskbs) {
        close_rt(sock);
        rtos_print("ioctl_rt(RTNET_RTIOC_EXTPOOL) = %d\n", ret);
        return -1;
    }

    /* bind the rt-socket to local_addr */
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RCV_PORT);
    local_addr.sin_addr.s_addr = local_ip;
    if ((ret = bind_rt(sock, (struct sockaddr *) &local_addr,
                       sizeof(struct sockaddr_in))) < 0) {
        close_rt(sock);
        rtos_print("can't bind rtsocket\n");
        return ret;
    }

    /* set server-addr */
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(SRV_PORT);
    broadcast_addr.sin_addr.s_addr = broadcast_ip;

    rtos_nanosecs_to_time(((nanosecs_t)cycle) * 1000, &period);
    rtos_task_init_periodic(&xmit_task, (void *)process, 0, 10, &period);

    rtos_task_init(&recv_task, (void *)echo_rcv, 0, 9);

    return 0;
}




void cleanup_module(void)
{
    /* Important: First close the socket! */
    while (close_rt(sock) == -EAGAIN) {
        rtos_print("rt_server: Socket busy - waiting...\n");
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rtos_fifo_destroy(&print_fifo);

    rtos_task_delete(&xmit_task);
    rtos_task_delete(&recv_task);
}
