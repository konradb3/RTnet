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

#include <rtnet_sys.h>
#include <rtnet.h>

static char *local_ip_s  = "";
static char *server_ip_s = "127.0.0.1";

MODULE_PARM (local_ip_s ,"s");
MODULE_PARM (server_ip_s,"s");
MODULE_PARM_DESC (local_ip_s, "rt_echo_client: lokal  ip-address");
MODULE_PARM_DESC (server_ip_s, "rt_echo_client: server ip-address");

MODULE_PARM(parport, "i");
MODULE_PARM(parirq, "i");

rtos_task_t xmit_task;
rtos_task_t recv_task;

#define RCV_PORT    35999
#define SRV_PORT    36000

static struct sockaddr_in server_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1500
static char buffer[BUFSIZE];

rtos_event_sem_t tx_sem;

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
rtos_fifo_t print_fifo;
rtos_irq_t  irq_handle;


static RTOS_IRQ_HANDLER_PROTO(parport_irq_handler)
{
    outb(0xF7, PAR_DATA);
    rtos_event_sem_signal(&tx_sem);
    RTOS_IRQ_RETURN_HANDLED();
}



void process(void* arg)
{
    rtos_time_t time;
    nanosecs_t  tx_time;


    while(1) {
        rtos_event_sem_wait(&tx_sem);

        rtos_get_time(&time);
        tx_time = rtos_time_to_nanosecs(&time);

        /* send the time   */
        if (sendto_rt(sock, &tx_time, sizeof(tx_time), 0,
                      (struct sockaddr *)&server_addr,
                      sizeof(struct sockaddr_in)) < 0)
            break;
    }
}



void echo_rcv(void *arg)
{
    int                 ret;
    struct msghdr       msg;
    struct iovec        iov;
    struct sockaddr_in  addr;
    rtos_time_t         time;
    nanosecs_t          rx_time;


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

            outb(0xEF, PAR_DATA);
            outb(0xFF, PAR_DATA);

            /* get the time    */
            rtos_get_time(&time);
            rx_time = rtos_time_to_nanosecs(&time);

            rtos_fifo_put(&print_fifo, &rx_time, sizeof(rx_time));
            rtos_fifo_put(&print_fifo, buffer, sizeof(nanosecs_t));

            /* copy the address */
            rcv.l = sin->sin_addr.s_addr;
        }
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

    rtos_fifo_create(&print_fifo, PRINT_FIFO, 40000);
    rtos_event_sem_init(&tx_sem);

    rtos_print("local  ip address %s=%8x\n", local_ip_s, (unsigned int) local_ip);
    rtos_print("server ip address %s=%8x\n", server_ip_s, (unsigned int) server_ip);

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
    if ((ret = bind_rt(sock, (struct sockaddr *)&local_addr,
                       sizeof(struct sockaddr_in))) < 0) {
        close_rt(sock);
        rtos_print("can't bind rtsocket\n");
        return ret;
    }

    /* set server-addr */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SRV_PORT);
    server_addr.sin_addr.s_addr = server_ip;

    rtos_task_init(&xmit_task,(void *)process,0,10);

    rtos_task_init(&recv_task,(void *)echo_rcv,0,9);

    rtos_irq_request(&irq_handle, parirq, parport_irq_handler, NULL);

    outb(0xFF, PAR_DATA);
    outb(0x14 + KHZ0_1, PAR_CONTROL);

    return ret;
}



void cleanup_module(void)
{
    rtos_irq_free(&irq_handle);

    outb(0, PAR_CONTROL);

    /* Important: First close the socket! */
    while (close_rt(sock) == -EAGAIN) {
        rtos_print("rt_server: Socket busy - waiting...\n");
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rtos_event_sem_delete(&tx_sem);

    rtos_task_delete(&xmit_task);
    rtos_task_delete(&recv_task);

    rtos_fifo_destroy(&print_fifo);
}
