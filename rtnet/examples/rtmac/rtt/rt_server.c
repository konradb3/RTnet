/***
 *
 *  rtmac/examples/rtt/rt_server.c
 *
 *  server part - listens and sends back a packet
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

static char *local_ip_s  = "";
static char *client_ip_s = "127.0.0.1";
static unsigned int reply_size = sizeof(RTIME);

MODULE_PARM(local_ip_s, "s");
MODULE_PARM(client_ip_s, "s");
MODULE_PARM(reply_size, "i");
MODULE_PARM_DESC(local_ip_s, "local ip-addr");
MODULE_PARM_DESC(client_ip_s, "client ip-addr");
MODULE_PARM_DESC(reply_size, "size of the reply message (8-65507)");

MODULE_LICENSE("GPL");

RT_TASK rt_task;

#define RCV_PORT    36000
#define XMT_PORT    35999

static struct sockaddr_in client_addr;
static struct sockaddr_in local_addr;

static int sock;

char buffer[sizeof(RTIME)];
char tx_msg[65536];




void process(void* arg)
{
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;
    int                 ret;

    while(1) {
        iov.iov_base=&buffer;
        iov.iov_len=sizeof(RTIME);
        msg.msg_name=&addr;
        msg.msg_namelen=sizeof(addr);
        msg.msg_iov=&iov;
        msg.msg_iovlen=1;
        msg.msg_control=NULL;
        msg.msg_controllen=0;

        ret = recvmsg_rt(sock, &msg, 0);
        if ( (ret <= 0) || (msg.msg_namelen != sizeof(struct sockaddr_in)) )
            return;

        memcpy(&tx_msg, &buffer, sizeof(RTIME));

        sendto_rt(sock, &tx_msg, reply_size, 0, (struct sockaddr *) &client_addr,
                  sizeof (struct sockaddr_in));
    }
}



int init_module(void)
{
    unsigned int add_rtskbs = 30;
    int ret;

    unsigned long local_ip;
    unsigned long client_ip;


    if (strlen(local_ip_s) != 0)
        local_ip = rt_inet_aton(local_ip_s);
    else
        local_ip = INADDR_ANY;
    client_ip = rt_inet_aton(client_ip_s);
    if (reply_size < sizeof(RTIME))
        reply_size = sizeof(RTIME);

    rt_printk("local  ip address %s(%8x):%d\n", local_ip_s, (unsigned int) local_ip, RCV_PORT);
    rt_printk("client ip address %s(%8x):%d\n", client_ip_s, (unsigned int) client_ip, XMT_PORT);
    rt_printk("reply message size=%d\n", reply_size);

    /* create rt-socket */
    rt_printk("create rtsocket\n");
    if ((sock = socket_rt(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        rt_printk("socket not created\n");
        return sock;
    }

    /* bind the rt-socket to local_addr */
    rt_printk("bind rtsocket to local address:port\n");
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RCV_PORT);
    local_addr.sin_addr.s_addr = local_ip;
    if ((ret = bind_rt(sock, (struct sockaddr *)&local_addr,
                       sizeof(struct sockaddr_in))) < 0) {
        close_rt(sock);
        rt_printk("can't bind rtsocket\n");
        return ret;
    }

    /* set client-addr */
    rt_printk("connect rtsocket to client address:port\n");
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(XMT_PORT);
    client_addr.sin_addr.s_addr = client_ip;
    if ((ret = connect_rt(sock, (struct sockaddr *)&client_addr,
                          sizeof(struct sockaddr_in))) < 0) {
        close_rt(sock);
        rt_printk("can't connect rtsocket\n");
        return ret;
    }

    /* extend the socket pool */
    ret = ioctl_rt(sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != (int)add_rtskbs) {
        close_rt(sock);
        rt_printk("ioctl_rt(RTNET_RTIOC_EXTPOOL) = %d\n", ret);
        return -1;
    }

    ret = rt_task_init(&rt_task,(void *)process,0,4096,10,0,NULL);
    ret = rt_task_resume(&rt_task);

    return ret;
}




void cleanup_module(void)
{
    /* Important: First close the socket! */
    while (close_rt(sock) == -EAGAIN) {
        printk("rt_server: Not all buffers freed yet - waiting...\n");
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rt_task_delete(&rt_task);
}
