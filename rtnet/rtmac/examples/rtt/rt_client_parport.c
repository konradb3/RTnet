/***
 *
 *  rtmac/examples/rtt/rt_client_parport.c
 *
 *  client part - sends packet triggered by external IRQ, receives echo,
 *                and passes them by fifo to userspace app
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

#include <rtnet_config.h>

#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_fifos.h>

#ifdef HAVE_RTAI_SEM_H
#include <rtai_sem.h>
#endif

#include <rtnet.h>

static char *local_ip_s  = "";
static char *server_ip_s = "127.0.0.1";

MODULE_PARM (local_ip_s ,"s");
MODULE_PARM (server_ip_s,"s");
MODULE_PARM_DESC (local_ip_s, "rt_echo_client: lokal  ip-address");
MODULE_PARM_DESC (server_ip_s, "rt_echo_client: server ip-address");

MODULE_PARM(parport, "i");
MODULE_PARM(parirq, "i");

RT_TASK rt_task;

#define RCV_PORT    35999
#define SRV_PORT    36000

static struct sockaddr_in server_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1500
static char buffer[BUFSIZE];
static RTIME tx_time;
static RTIME rx_time;

SEM tx_sem;

#define EXT_TRIG

static int parport = 0x378;
static int parirq  = 7;

#define PAR_DATA    parport
#define PAR_STATUS  PAR_DATA+1
#define PAR_CONTROL PAR_DATA+2

#define KHZ0_1      0x00
#define KHZ1        0x83
#define KHZ10       0x03
#define KHZ100      0x01

#define PRINT_FIFO 0

unsigned long tsc1,tsc2;
unsigned long cnt = 0;


static void parport_irq_handler(void)
{
    outb(0xF7, PAR_DATA);
    rt_sem_signal(&tx_sem);
}



void *process(void* arg)
{
    int ret = 0;

    while(1) {
        rt_sem_wait(&tx_sem);

        tx_time = rt_get_time_ns();

        /* send the time   */
        ret=rt_socket_sendto(sock, &tx_time, sizeof(RTIME), 0,
                             (struct sockaddr *) &server_addr,
                             sizeof(struct sockaddr_in));
    }
}



int echo_rcv(int s,void *arg)
{
    int                 ret=0;
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;


    memset(&msg,0,sizeof(msg));
    iov.iov_base=&buffer;
    iov.iov_len=BUFSIZE;

    msg.msg_name=&addr;
    msg.msg_namelen=sizeof(addr);
    msg.msg_iov=&iov;
    msg.msg_iovlen=1;
    msg.msg_control=NULL;
    msg.msg_controllen=0;

    ret=rt_socket_recvmsg(sock, &msg, 0);

    if ( (ret>0) && (msg.msg_namelen==sizeof(struct sockaddr_in)) ) {

        union { unsigned long l; unsigned char c[4]; } rcv;
        struct sockaddr_in *sin = msg.msg_name;

        outb(0xEF, PAR_DATA);
        outb(0xFF, PAR_DATA);

        /* get the time    */
        rx_time = rt_get_time_ns();
        memcpy (&tx_time, buffer, sizeof(RTIME));

        rtf_put(PRINT_FIFO, &rx_time, sizeof(RTIME));
        rtf_put(PRINT_FIFO, &tx_time, sizeof(RTIME));

        /* copy the address */
        rcv.l = sin->sin_addr.s_addr;
    }

    return 0;
}



int init_module(void)
{
    unsigned int nonblock = 1;
    unsigned int add_rtskbs = 30;
    int ret;

    unsigned long local_ip;
    unsigned long server_ip;


    if (strlen(local_ip_s) != 0)
        local_ip = rt_inet_aton(local_ip_s);
    else
        local_ip = INADDR_ANY;
    server_ip = rt_inet_aton(server_ip_s);

    rtf_create(PRINT_FIFO, 40000);
    rt_sem_init(&tx_sem, 0);

    rt_printk ("local  ip address %s=%8x\n", local_ip_s, (unsigned int) local_ip);
    rt_printk ("server ip address %s=%8x\n", server_ip_s, (unsigned int) server_ip);

    /* create rt-socket */
    rt_printk("create rtsocket\n");
    if ((sock=rt_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        rt_printk("socket not created\n");
        return sock;
    }

    /* switch to non-blocking */
    ret = rt_setsockopt(sock, SOL_SOCKET, RT_SO_NONBLOCK, &nonblock, sizeof(nonblock));
    rt_printk("rt_setsockopt(RT_SO_NONBLOCK) = %d\n", ret);

    /* extend the socket pool */
    ret = rt_setsockopt(sock, SOL_SOCKET, RT_SO_EXTPOOL, &add_rtskbs, sizeof(add_rtskbs));
    if (ret != (int)add_rtskbs) {
        rt_socket_close(sock);
        rt_printk("rt_setsockopt(RT_SO_EXTPOOL) = %d\n", ret);
        return -1;
    }

    /* bind the rt-socket to local_addr */
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(RCV_PORT);
    local_addr.sin_addr.s_addr = local_ip;
    if ( (ret=rt_socket_bind(sock, (struct sockaddr *) &local_addr, sizeof(struct sockaddr_in)))<0 ) {
        rt_socket_close(sock);
        rt_printk("can't bind rtsocket\n");
        return ret;
    }

    /* set server-addr */
    memset(&server_addr, 0, sizeof(struct sockaddr_in));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SRV_PORT);
    server_addr.sin_addr.s_addr = server_ip;

    /* set up receiving */
    rt_socket_callback(sock, echo_rcv, NULL);

    ret=rt_task_init(&rt_task,(void *)process,0,4096,10,0,NULL);

    rt_task_resume(&rt_task);

    rt_request_global_irq(parirq, parport_irq_handler);
    rt_startup_irq(parirq);

    outb(0xFF, PAR_DATA);
    outb(0x14 + KHZ0_1, PAR_CONTROL);

    return ret;
}



void cleanup_module(void)
{
    rt_shutdown_irq(parirq);
    rt_free_global_irq(parirq);

    outb(0, PAR_CONTROL);

    /* Important: First close the socket! */
    while (rt_socket_close(sock) == -EAGAIN) {
        printk("rt_server: Socket busy - waiting...\n");
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rt_printk("rt_task_delete() = %d\n",rt_task_delete(&rt_task));

    rt_sem_delete(&tx_sem);

    rtf_destroy(PRINT_FIFO);
}
