/***
 *
 *  examples/frag_ip/frag_ip.c
 *
 *  sends fragmented IP packets to another frag_ip instance
 *
 *  Copyright (C) 2003, 2004 Jan Kiszka <jan.kiszka@web.de>
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
#include <linux/net.h>
#include <linux/socket.h>
#include <linux/in.h>

#include <native/task.h>
#include <rtnet.h>

#include <rtnet_config.h>   /* required for rt_task_wait_period() changes */

static char *dest_ip_s = "127.0.0.1";
static unsigned int size = 65505;
static unsigned int add_rtskbs = 75;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10)
module_param(dest_ip_s, charp, 0);
#else /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10) */
MODULE_PARM(dest_ip_s, "s");
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,10) */
module_param(size, uint, 0);
module_param(add_rtskbs, uint, 0);
MODULE_PARM_DESC(dest_ip_s, "destination IP address");
MODULE_PARM_DESC(size, "message size (0-65505)");
MODULE_PARM_DESC(add_rtskbs, "number of additional rtskbs (default: 75)");

MODULE_LICENSE("GPL");

#define CYCLE       1000*1000*1000   /* 1 s */
RT_TASK rt_xmit_task;
RT_TASK rt_recv_task;

#define PORT        37000

static struct sockaddr_in dest_addr;

static int sock;

static char buffer_out[64*1024];
static char buffer_in[64*1024];



void send_msg(void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov[2];
    unsigned short msgsize = size;


    while(1) {
        iov[0].iov_base = &msgsize;
        iov[0].iov_len  = sizeof(msgsize);
        iov[1].iov_base = buffer_out;
        iov[1].iov_len  = size;

        memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov     = iov;
        msg.msg_iovlen  = 2;

        printk("Sending message of %d+2 bytes\n", size);
        ret = rt_dev_sendmsg(sock, &msg, 0);
        if (ret != (int)(sizeof(msgsize) + size))
            printk(" rt_dev_sendmsg() = %d!\n", ret);

#ifdef CONFIG_XENO_2_0x /* imported via rtnet_config.h */
        rt_task_wait_period(); /* old signature */
#else /* Xenomai 2.1 and later */
        rt_task_wait_period(NULL);
#endif /* CONFIG_XENO_2_0x */
    }
}



void recv_msg(void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov[2];
    unsigned short msgsize = size;
    struct sockaddr_in addr;


    while(1) {
        iov[0].iov_base = &msgsize;
        iov[0].iov_len  = sizeof(msgsize);
        iov[1].iov_base = buffer_in;
        iov[1].iov_len  = size;

        memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov     = iov;
        msg.msg_iovlen  = 2;

        ret = rt_dev_recvmsg(sock, &msg, 0);
        if (ret <= 0) {
            printk(" rt_dev_recvmsg() = %d\n", ret);
            return;
        } else {
            unsigned long ip = ntohl(addr.sin_addr.s_addr);

            printk("received packet from %lu.%lu.%lu.%lu, length: %d+2, "
                   "encoded length: %d,\n flags: %X, content %s\n", ip >> 24,
                   (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
                   ret-sizeof(msgsize), msgsize, msg.msg_flags,
                   (memcmp(buffer_in, buffer_out, ret-sizeof(msgsize)) == 0) ?
                       "ok" : "corrupted");
        }
    }
}



int init_module(void)
{
    int ret;
    unsigned int i;
    struct sockaddr_in local_addr;
    unsigned long dest_ip = rt_inet_aton(dest_ip_s);

    if (size > 65505)
        size = 65505;

    printk("destination ip address %s=%08x\n", dest_ip_s,
           (unsigned int)dest_ip);
    printk("size %d\n", size);

    /* fill output buffer with test pattern */
    for (i = 0; i < sizeof(buffer_out); i++)
        buffer_out[i] = i & 0xFF;

    /* create rt-socket */
    sock = rt_dev_socket(AF_INET,SOCK_DGRAM,0);
    if (sock < 0) {
        printk(" rt_dev_socket() = %d!\n", sock);
        return sock;
    }

    /* extend the socket pool */
    ret = rt_dev_ioctl(sock, RTNET_RTIOC_EXTPOOL, &add_rtskbs);
    if (ret != (int)add_rtskbs) {
        printk(" rt_dev_ioctl(RT_IOC_SO_EXTPOOL) = %d\n", ret);
        goto cleanup_sock;
    }

    /* bind the rt-socket to a port */
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = INADDR_ANY;
    ret = rt_dev_bind(sock, (struct sockaddr *)&local_addr,
                      sizeof(struct sockaddr_in));
    if (ret < 0) {
        printk(" rt_dev_bind() = %d!\n", ret);
        goto cleanup_sock;
    }

    /* set destination address */
    memset(&dest_addr, 0, sizeof(struct sockaddr_in));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    dest_addr.sin_addr.s_addr = dest_ip;

    /* You may have to start the system timer manually
     * on older Xenomai versions (2.0.x):
     * rt_timer_start(TM_ONESHOT); */

    ret = rt_task_create(&rt_recv_task, "recv_task", 0, 9, 0);
    if (ret != 0) {
        printk(" rt_task_create(rt_recv_task) = %d!\n", ret);
        goto cleanup_sock;
    }

    ret = rt_task_start(&rt_recv_task, recv_msg, NULL);
    if (ret != 0) {
        printk(" rt_task_start(rt_recv_task) = %d!\n", ret);
        goto cleanup_recv_task;
    }

    ret = rt_task_create(&rt_xmit_task, "xmit_task", 0, 10, 0);
    if (ret != 0) {
        printk(" rt_task_create(rt_xmit_task) = %d!\n", ret);
        goto cleanup_recv_task;
    }

    ret = rt_task_set_periodic(&rt_xmit_task, TM_INFINITE, CYCLE);
    if (ret == 0) {
        printk(" rt_task_set_periodic(rt_xmit_task) = %d!\n", ret);
        goto cleanup_xmit_task;
    }

    ret = rt_task_start(&rt_xmit_task, send_msg, NULL);
    if (ret != 0) {
        printk(" rt_task_start(rt_xmit_task) = %d!\n", ret);
        goto cleanup_xmit_task;
    }

    return 0;


 cleanup_xmit_task:
    rt_task_delete(&rt_xmit_task);

 cleanup_recv_task:
    rt_task_delete(&rt_recv_task);

 cleanup_sock:
    rt_dev_close(sock);

    return ret;
}



void cleanup_module(void)
{
    /* In case you started it in this module, see comment above.
     * rt_timer_stop(); */

    /* Important: First close the socket! */
    while (rt_dev_close(sock) == -EAGAIN) {
        printk("frag-ip: Socket busy - waiting...\n");
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rt_task_delete(&rt_xmit_task);
    rt_task_delete(&rt_recv_task);
}
