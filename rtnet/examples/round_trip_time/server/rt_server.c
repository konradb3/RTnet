/***
 *
 *  examples/round_trip_time/sever/rt_server.c
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

#include <rtai.h>
#include <rtai_sched.h>
#include <rtdm_driver.h>

#include <rtnet.h>


#define MIN_LENGTH_IPv4 7
#define MAX_LENGTH_IPv4 15
static char *local_ip_s  = "";
static char *client_ip_s = "127.0.0.1";

MODULE_PARM (local_ip_s ,"s");
MODULE_PARM (client_ip_s,"s");
MODULE_PARM_DESC (local_ip_s, "local ip-addr");
MODULE_PARM_DESC (client_ip_s, "client ip-addr");

MODULE_LICENSE("GPL");

#define RCV_PORT	36000
#define SRV_PORT	35999

static struct sockaddr_in client_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1400
char buffer[BUFSIZE];


void echo_rcv(struct rtdm_dev_context *context, void *arg)
{
    int                 ret;
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;


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
        iov.iov_base=&buffer;
        iov.iov_len=BUFSIZE;
        msg.msg_name=&client_addr;
        msg.msg_namelen=sizeof(client_addr);
        msg.msg_iov=&iov;
        msg.msg_iovlen=1;
        msg.msg_control=NULL;
        msg.msg_controllen=0;

        context->ops->sendmsg_rt(context, 0, &msg, 0);
    }
}



int init_module(void)
{
    int                     ret;
    unsigned int            local_ip;
    unsigned int            client_ip = rt_inet_aton(client_ip_s);
    struct rtnet_callback   callback  = {echo_rcv, NULL};


    if (strlen(local_ip_s) != 0)
        local_ip = rt_inet_aton(local_ip_s);
    else
        local_ip = INADDR_ANY;

    printk ("local  ip address %s=%08x\n", local_ip_s, local_ip);
    printk ("client ip address %s=%08x\n", client_ip_s, client_ip);

    /* create rt-socket */
    printk("create rtsocket\n");
    if ((sock = socket_rt(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        printk("socket not created\n");
        return -ENOMEM;
    }

    /* bind the rt-socket to local_addr */
    printk("bind rtsocket to local address:port\n");
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RCV_PORT);
    local_addr.sin_addr.s_addr = local_ip;
    if ((ret = bind_rt(sock, (struct sockaddr *)&local_addr,
                       sizeof(struct sockaddr_in))) < 0) {
        printk("can't bind rtsocket\n");
        close_rt(sock);
        return ret;
    }

    /* set client-addr */
    printk("connect rtsocket to client address:port\n");
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(SRV_PORT);
    client_addr.sin_addr.s_addr = client_ip;
    if ((ret = connect_rt(sock, (struct sockaddr *)&client_addr,
                          sizeof(struct sockaddr_in))) < 0) {
        printk("can't connect rtsocket\n");
        close_rt(sock);
        return ret;
    }

    /* set up callback handler */
    ioctl_rt(sock, RTNET_RTIOC_CALLBACK, &callback);

    return 0;
}




void cleanup_module(void)
{
    while (close_rt(sock) == -EAGAIN) {
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }
}
