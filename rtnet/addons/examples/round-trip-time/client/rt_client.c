/***
 *
 *  examples/round_trip_time/client/rt_client.c
 *
 *  module that demonstrates an echo message scenario
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2004 Jan Kiszka <jan.kiszka@web.de>
 *
 *  RTnet - real-time networking example
 *
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
#include <rtdm_driver.h>
#include <rtnet.h>

static char *local_ip_s  = "";
static char *server_ip_s = "127.0.0.1";
int interval = 500; /* time between two sent packets in ms (1-1000) */
int packetsize = 58; /* packetsize exclusive headers (1-1400) */

MODULE_PARM(local_ip_s ,"s");
MODULE_PARM(server_ip_s,"s");
MODULE_PARM(interval, "i");
MODULE_PARM(packetsize,"i");
MODULE_PARM_DESC(local_ip_s, "rt_echo_client: local ip-address");
MODULE_PARM_DESC(server_ip_s, "rt_echo_client: server ip-address");

#ifdef CONFIG_RTOS_STARTSTOP_TIMER
static int start_timer = 0;
MODULE_PARM(start_timer, "i");
MODULE_PARM_DESC(start_timer, "set to non-zero to start scheduling timer");
#endif

MODULE_LICENSE("GPL");

#define TICK_PERIOD     100000
rtos_task_t rt_task;

#define RCV_PORT        35999
#define SRV_PORT        36000

static struct sockaddr_in server_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1500
static char buffer[BUFSIZE], sendbuffer[BUFSIZE];
static nanosecs_t tx_time;
static nanosecs_t rx_time;

#define PRINT_FIFO 0
static rtos_fifo_t print_fifo;

static unsigned long sent;
static unsigned long rcvd;


void process(void *arg)
{
    int         ret = 0;
    rtos_time_t time;


    while (1) {
        /* wait one period */
        rtos_task_wait_period();

        /* get time        */
        rtos_get_time(&time);
        tx_time = rtos_time_to_nanosecs(&time);

        memcpy(sendbuffer, &tx_time, sizeof(tx_time));

        /* send the time   */
        ret = sendto_rt(sock, &sendbuffer, packetsize, 0,
                        (struct sockaddr *)&server_addr,
                        sizeof(struct sockaddr_in));
        if (ret)
            sent++;
    }
}



void echo_rcv(struct rtdm_dev_context *context, void *arg)
{
    int                 ret=0;
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;
    rtos_time_t         time;


    iov.iov_base=&buffer;
    iov.iov_len=BUFSIZE;

    msg.msg_name=&addr;
    msg.msg_namelen=sizeof(addr);
    msg.msg_iov=&iov;
    msg.msg_iovlen=1;
    msg.msg_control=NULL;
    msg.msg_controllen=0;

    /* This demonstrates the fast path to the RTnet API for kernel modules.
       Note that this method depends on using the correct RTDM version. An
       alternative is to take the file descriptor from a global variable or
       pass it in the callback argument "arg" and then call the official API
       functions. */
    ret = context->ops->recvmsg_rt(context, 0, &msg, 0);

    if ((ret > 0) && (msg.msg_namelen == sizeof(struct sockaddr_in))) {

        union { unsigned long l; unsigned char c[4]; } rcv;
        struct sockaddr_in *sin = msg.msg_name;

        /* get the time    */
        rtos_get_time(&time);
        rx_time = rtos_time_to_nanosecs(&time);
        rcvd++;

        rtos_fifo_put(&print_fifo, &rx_time, sizeof(rx_time));
        rtos_fifo_put(&print_fifo, buffer, sizeof(nanosecs_t));

        /* copy the address */
        rcv.l = sin->sin_addr.s_addr;
    }
}


int init_module(void)
{
    int                     ret = 0;
    rtos_time_t             cycle;
    unsigned int            local_ip;
    unsigned int            server_ip = rt_inet_aton(server_ip_s);
    struct rtnet_callback   callback  = {echo_rcv, NULL};


    if (strlen(local_ip_s) != 0)
        local_ip = rt_inet_aton(local_ip_s);
    else
        local_ip = INADDR_ANY;

    if (interval < 1) interval = 1;
    if (interval > 1000) interval = 1000;

    if (packetsize < 1) packetsize = 1;
    if (packetsize > 1400) packetsize = 1400;

    printk("***** start of rt_client ***** %s %s *****\n",
           __DATE__, __TIME__);
    printk("local  ip address %s=%08x\n", local_ip_s, local_ip);
    printk("server ip address %s=%08x\n", server_ip_s, server_ip);
    printk("interval = %d\n", interval);
    printk("packetsize = %d\n", packetsize);
#ifdef CONFIG_RTOS_STARTSTOP_TIMER
    printk("start timer %d\n", start_timer);
#endif

    rtos_fifo_create(&print_fifo, PRINT_FIFO, 8000);

    /* create rt-socket */
    if ((sock = socket_rt(AF_INET,SOCK_DGRAM,0)) < 0) {
        printk("socket not created\n");
        return -ENOMEM;
    }

    /* bind the rt-socket to local_addr */
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RCV_PORT);
    local_addr.sin_addr.s_addr = local_ip;
    if ( (ret = bind_rt(sock, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in))) < 0) {
        printk("can't bind rtsocket\n");
        close_rt(sock);
        return ret;
    }

    /* set server-addr */
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SRV_PORT);
    server_addr.sin_addr.s_addr = server_ip;

    /* set up callback handler */
    ioctl_rt(sock, RTNET_RTIOC_CALLBACK, &callback);

#ifdef CONFIG_RTOS_STARTSTOP_TIMER
    if (start_timer) {
        rtos_timer_start_oneshot();
    }
#endif

    rtos_nanosecs_to_time(((nanosecs_t)interval) * 1000000, &cycle);
    rtos_task_init_periodic(&rt_task, (void *)process, 0, 10, &cycle);

    return 0;
}




void cleanup_module(void)
{
#ifdef CONFIG_RTOS_STARTSTOP_TIMER
    if (start_timer)
        rtos_timer_stop();
#endif

    while (close_rt(sock) == -EAGAIN) {
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    /* rt_task_delete     */
    rtos_task_delete(&rt_task);

    /* destroy the fifo   */
    rtos_fifo_destroy(&print_fifo);

    printk("packets sent:\t\t%10lu\npackets received:\t%10lu\npacketloss:\t\t"
           "%10lu%%\n", sent, rcvd, 100-((100*rcvd)/sent));
}
