/***
 *
 *  examples/raw-packets/raw-packets.c
 *
 *  sends Ethernet packets to another raw-packets instance
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
#include <linux/if_packet.h>

#include <rtai/task.h>
#include <rtnet.h>

static char *dest_mac_s = "FF:FF:FF:FF:FF:FF";
static int local_if = 1;
static int start_timer = 0;

MODULE_PARM(dest_mac_s, "s");
MODULE_PARM(local_if, "i");
MODULE_PARM(start_timer, "i");
MODULE_PARM_DESC(dest_mac_s, "destination MAC address (XX:XX:XX:XX:XX:XX)");
MODULE_PARM_DESC(local_if, "local interface for sending and receiving packets (1-n)");
MODULE_PARM_DESC(start_timer, "set to non-zero to start scheduling timer");

MODULE_LICENSE("GPL");

#define PROTOCOL    0x1234

#define CYCLE       1000*1000*1000   /* 1 s */
RT_TASK rt_xmit_task;
RT_TASK rt_recv_task;

static struct sockaddr_ll dest_addr;

static int sock;

static char buffer_out[] = "Hello, world! I'm sending Ethernet frames...";
static char buffer_in[1500];



void send_msg(void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov;


    while (1) {
        iov.iov_base = buffer_out;
        iov.iov_len  = sizeof(buffer_out);

        memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &dest_addr;
        msg.msg_namelen = sizeof(dest_addr);
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;

        printk("Sending message of %d bytes\n", sizeof(buffer_out));
        ret = rt_dev_sendmsg(sock, &msg, 0);
        if (ret != (int)sizeof(buffer_out))
            printk(" rt_dev_sendmsg() = %d!\n", ret);

        rt_task_wait_period();
    }
}



void recv_msg(void *arg)
{
    int ret;
    struct msghdr msg;
    struct iovec iov;
    struct sockaddr_ll addr;


    while (1) {
        iov.iov_base = buffer_in;
        iov.iov_len  = sizeof(buffer_in);

        memset(&msg, 0, sizeof(msg));
        msg.msg_name    = &addr;
        msg.msg_namelen = sizeof(addr);
        msg.msg_iov     = &iov;
        msg.msg_iovlen  = 1;

        ret = rt_dev_recvmsg(sock, &msg, 0);
        if (ret <= 0) {
            printk(" rt_dev_recvmsg() = %d\n", ret);
            return;
        } else {
            printk("received packet from %02X:%02X:%02X:%02X:%02X:%02X, "
                   "length: %d,\ncontent: \"%s\"\n",
                   addr.sll_addr[0], addr.sll_addr[1], addr.sll_addr[2],
                   addr.sll_addr[3], addr.sll_addr[4], addr.sll_addr[5],
                   ret, buffer_in);
        }
    }
}



int init_module(void)
{
    int ret;
    struct sockaddr_ll local_addr;


    /* set destination address */
    memset(&dest_addr, 0, sizeof(struct sockaddr_ll));
    dest_addr.sll_family   = AF_PACKET;
    dest_addr.sll_protocol = htons(PROTOCOL);
    dest_addr.sll_ifindex  = local_if;
    dest_addr.sll_halen    = 6;

    rt_eth_aton(dest_addr.sll_addr, dest_mac_s);

    printk("destination mac address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           dest_addr.sll_addr[0], dest_addr.sll_addr[1],
           dest_addr.sll_addr[2], dest_addr.sll_addr[3],
           dest_addr.sll_addr[4], dest_addr.sll_addr[5]);
    printk("local interface: %d\n", local_if);
    printk("start timer: %d\n", start_timer);

    /* create rt-socket */
    sock = rt_dev_socket(AF_PACKET, SOCK_DGRAM, htons(PROTOCOL));
    if (sock < 0) {
        printk(" rt_dev_socket() = %d!\n", sock);
        return sock;
    }

    /* bind the rt-socket to a port */
    memset(&local_addr, 0, sizeof(struct sockaddr_ll));
    local_addr.sll_family   = AF_PACKET;
    local_addr.sll_protocol = htons(PROTOCOL);
    local_addr.sll_ifindex  = local_if;
    ret = rt_dev_bind(sock, (struct sockaddr *)&local_addr,
                      sizeof(struct sockaddr_ll));
    if (ret < 0) {
        printk(" rt_dev_bind() = %d!\n", ret);
        goto cleanup_sock;
    }


    if (start_timer) {
        ret = rt_timer_start(TM_ONESHOT);
        if (ret != 0) {
            printk(" rt_timer_start = %d!\n", ret);
            goto cleanup_sock;
        }
    }

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
    if (start_timer)
        rt_timer_stop();

    /* Important: First close the socket! */
    while (rt_dev_close(sock) == -EAGAIN) {
        printk("raw-packets: Socket busy - waiting...\n");
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rt_task_delete(&rt_xmit_task);
    rt_task_delete(&rt_recv_task);
}
