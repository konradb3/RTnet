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
#include <rtai_fifos.h>

#include <rtnet.h>

//#define USE_CALLBACKS

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

#ifdef USE_CALLBACKS
SEM     tx_sem;
#endif

#define RCV_PORT    36000
#define XMT_PORT    35999

static struct sockaddr_in client_addr;
static struct sockaddr_in local_addr;

static int sock;

char buffer[sizeof(RTIME)];
char tx_msg[65536];




#ifdef USE_CALLBACKS

void *process(void* arg)
{
    while(1) {
        rt_sem_wait(&tx_sem);
        rt_socket_sendto(sock, &tx_msg, reply_size, 0, (struct sockaddr *) &client_addr,
                         sizeof (struct sockaddr_in));
    }
}



int echo_rcv(int s,void *arg)
{
    int                 ret=0;
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;


    memset(&msg, 0, sizeof(msg));
    iov.iov_base=&buffer;
    iov.iov_len=sizeof(RTIME);
    msg.msg_name=&addr;
    msg.msg_namelen=sizeof(addr);
    msg.msg_iov=&iov;
    msg.msg_iovlen=1;
    msg.msg_control=NULL;
    msg.msg_controllen=0;

    ret=rt_socket_recvmsg(sock, &msg, 0);
    if ( (ret>0) && (msg.msg_namelen==sizeof(struct sockaddr_in)) ) {
        memcpy(&tx_msg, &buffer, 8);
        rt_sem_signal(&tx_sem);
    }

    return 0;
}

#else

void process(void* arg)
{
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;
    int                 ret;

    while(1) {
        memset(&msg, 0, sizeof(msg));
        iov.iov_base=&buffer;
        iov.iov_len=sizeof(RTIME);
        msg.msg_name=&addr;
        msg.msg_namelen=sizeof(addr);
        msg.msg_iov=&iov;
        msg.msg_iovlen=1;
        msg.msg_control=NULL;
        msg.msg_controllen=0;

        ret=rt_socket_recvmsg(sock, &msg, 0);
        if ( (ret <= 0) || (msg.msg_namelen != sizeof(struct sockaddr_in)) )
            return;

        memcpy(&tx_msg, &buffer, sizeof(RTIME));

        rt_socket_sendto(sock, &tx_msg, reply_size, 0, (struct sockaddr *) &client_addr,
                         sizeof (struct sockaddr_in));
    }
}

#endif



int init_module(void)
{
#ifdef USE_CALLBACKS
    unsigned int nonblock = 1;
#endif
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
    if ((sock=rt_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        rt_printk("socket not created\n");
        return sock;
    }

    /* bind the rt-socket to local_addr */
    rt_printk("bind rtsocket to local address:port\n");
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RCV_PORT);
    local_addr.sin_addr.s_addr = local_ip;
    if ( (ret=rt_socket_bind(sock, (struct sockaddr *) &local_addr, sizeof(struct sockaddr_in)))<0 ) {
        rt_socket_close(sock);
        rt_printk("can't bind rtsocket\n");
        return ret;
    }

    /* set client-addr */
    rt_printk("connect rtsocket to client address:port\n");
    memset(&client_addr, 0, sizeof(struct sockaddr_in));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(XMT_PORT);
    client_addr.sin_addr.s_addr = client_ip;
    if ( (ret=rt_socket_connect(sock, (struct sockaddr *) &client_addr, sizeof(struct sockaddr_in)))<0 ) {
        rt_socket_close(sock);
        rt_printk("can't connect rtsocket\n");
        return ret;
    }

#ifdef USE_CALLBACKS
    /* switch to non-blocking */
    ret = rt_setsockopt(sock, SOL_SOCKET, RT_SO_NONBLOCK, &nonblock, sizeof(nonblock));
    rt_printk("rt_setsockopt(RT_SO_NONBLOCK) = %d\n", ret);

    /* set up receiving */
    rt_socket_callback(sock, echo_rcv, NULL);

    /* initialize semaphore */
    rt_sem_init(&tx_sem, 0);
#endif

    /* extend the socket pool */
    ret = rt_setsockopt(sock, SOL_SOCKET, RT_SO_EXTPOOL, &add_rtskbs, sizeof(add_rtskbs));
    if (ret != (int)add_rtskbs) {
        rt_socket_close(sock);
        rt_printk("rt_setsockopt(RT_SO_EXTPOOL) = %d\n", ret);
        return -1;
    }

    ret = rt_task_init(&rt_task,(void *)process,0,4096,10,0,NULL);
    ret = rt_task_resume(&rt_task);

    return ret;
}




void cleanup_module(void)
{
    /* Important: First close the socket! */
    while (rt_socket_close(sock) == -EAGAIN) {
        printk("rt_server: Not all buffers freed yet - waiting...\n");
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rt_task_delete(&rt_task);
#ifdef USE_CALLBACKS
    rt_sem_delete(&tx_sem);
#endif
}
