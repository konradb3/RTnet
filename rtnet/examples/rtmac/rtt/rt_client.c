/***
 *
 *  rtmac/examples/rtt/rt_client.c
 *
 *  client part - sends packet, receives echo, passes them by fifo to userspace app
 *
 *  based on Ulrich Marx's module, adopted to rtmac
 *
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2002 Marc Kleine-Budde <kleine-budde@gmx.de>
 *                2004 Jan Kiszka <jan.kiszka@web.de>
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

#include <rtai.h>
#include <rtai_sched.h>

#include <rtnet.h>
/* for rtmac_dev */
#include <rtmac.h>

#include <rtai_fifos.h>

static char *local_ip_s  = "";
static char *server_ip_s = "127.0.0.1";
static int cycle = 1*1000*1000; /* = 1 s */
static char *rtmac_dev = "";

MODULE_PARM (local_ip_s ,"s");
MODULE_PARM (server_ip_s,"s");
MODULE_PARM (cycle, "i");
MODULE_PARM (rtmac_dev, "s");
MODULE_PARM_DESC (local_ip_s, "rt_echo_client: lokal ip-address");
MODULE_PARM_DESC (server_ip_s, "rt_echo_client: server ip-address");
MODULE_PARM_DESC (cycle, "cycle time in us or cycle counts");
MODULE_PARM_DESC (rtmac_dev,
    "RTmac device name to synchronise on its bus cycle (e.g. TDMA0)");

RT_TASK xmit_task;
RT_TASK recv_task;

#define RCV_PORT    35999
#define SRV_PORT    36000

static struct sockaddr_in server_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1500
static char buffer[BUFSIZE];
static RTIME tx_time;
static RTIME rx_time;

#define PRINT_FIFO  0

unsigned long tsc1,tsc2;
unsigned long cnt = 0;

static int tdma = -1;


void process(void* arg)
{
    int count;
    int wait_on = RTMAC_WAIT_ON_DEFAULT;

    while(1) {
        if (rtmac_dev[0]) {
            count = cycle;
            while (count != 0) {
                if (ioctl_rt(tdma, RTMAC_RTIOC_WAITONCYCLE, &wait_on) != 0) {
                    rt_printk("tdma_wait_sof() failed!");
                    return;
                }
                count--;
            }
        } else {
            rt_task_wait_period();
        }

        tx_time = rt_get_time_ns();

        /* send the time   */
        sendto_rt(sock, &tx_time, sizeof(RTIME), 0,
                  (struct sockaddr *)&server_addr,
                  sizeof(struct sockaddr_in));
    }
}



void echo_rcv(void *arg)
{
    int                 ret;
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;


    while (1) {
        iov.iov_base=&buffer;
        iov.iov_len=BUFSIZE;

        msg.msg_name=&addr;
        msg.msg_namelen=sizeof(addr);
        msg.msg_iov=&iov;
        msg.msg_iovlen=1;
        msg.msg_control=NULL;
        msg.msg_controllen=0;

        ret = recvmsg_rt(sock, &msg, 0);

        if ((ret > 0) && (msg.msg_namelen == sizeof(struct sockaddr_in))) {

            union { unsigned long l; unsigned char c[4]; } rcv;
            struct sockaddr_in *sin = msg.msg_name;

            /* get the time    */
            rx_time = rt_get_time_ns();
            memcpy (&tx_time, buffer, sizeof(RTIME));

            rtf_put(PRINT_FIFO, &rx_time, sizeof(RTIME));
            rtf_put(PRINT_FIFO, &tx_time, sizeof(RTIME));

            /* copy the address */
            rcv.l = sin->sin_addr.s_addr;
        } else
            break;
    }
}



int init_module(void)
{
    unsigned int add_rtskbs = 30;
    int ret;

    unsigned long local_ip;
    unsigned long server_ip;


    if (strlen(local_ip_s) != 0)
        local_ip = rt_inet_aton(local_ip_s);
    else
        local_ip = INADDR_ANY;
    server_ip = rt_inet_aton(server_ip_s);

    if (rtmac_dev[0] != '\0') {
        tdma = open_rt(rtmac_dev, O_RDONLY);
        if (tdma < 0) {
            rt_printk("You enabled rtmac_dev but device '%s' not found"
                " or no tdma attached.", rtmac_dev);
            return -ENODEV;
        }
    }

    rtf_create(PRINT_FIFO, 40000);

    rt_printk ("local  ip address %s=%8x\n", local_ip_s, (unsigned int) local_ip);
    rt_printk ("server ip address %s=%8x\n", server_ip_s, (unsigned int) server_ip);

    /* create rt-socket */
    rt_printk("create rtsocket\n");
    if ((sock = socket_rt(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        rt_printk("socket not created\n");
        return sock;
    }

    /* extend the socket pool */
    ret = ioctl_rt(sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != (int)add_rtskbs) {
        close_rt(sock);
        rt_printk("ioctl_rt(RTNET_RTIOC_EXTPOOL) = %d\n", ret);
        return -1;
    }

    /* bind the rt-socket to local_addr */
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RCV_PORT);
    local_addr.sin_addr.s_addr = local_ip;
    if ((ret = bind_rt(sock, (struct sockaddr *)&local_addr,
                       sizeof(struct sockaddr_in))) < 0) {
        close_rt(sock);
        rt_printk("can't bind rtsocket\n");
        return ret;
    }

    /* set server-addr */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SRV_PORT);
    server_addr.sin_addr.s_addr = server_ip;

    rt_task_init(&xmit_task,(void *)process,0,4096,10,0,NULL);
    if (rtmac_dev[0] != '\0') {
        rt_task_resume(&xmit_task);
    } else {
        rt_task_make_periodic_relative_ns(&xmit_task, 10 * 1000*1000, cycle * 1000);
    }

    rt_task_init(&recv_task,(void *)echo_rcv,0,4096,9,0,NULL);
    rt_task_resume(&recv_task);

    return 0;
}


void cleanup_module(void)
{
    /* Important: First close the socket! */
    while (close_rt(sock) == -EAGAIN) {
        printk("rt_server: Socket busy - waiting...\n");
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    while (close_rt(tdma) == -EAGAIN) {
        printk("rt_server: TDMA device busy - waiting...\n");
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rtf_destroy(PRINT_FIFO);

    rt_task_delete(&xmit_task);
    rt_task_delete(&recv_task);
}
