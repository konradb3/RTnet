/***
 *
 *  rtnet/examples/select/rt_server.c
 *  test for implementation of rtnet_select()
 *  (c) 2004 by Hans-Peter Bock <rtnet@avaapgh.de>
 *
 *  based on rt_echo_client/rt_server (C) 2002 by
 *  Ulrich Marx <marx@kammer.uni-hannover.de>
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

/***
 * To test the select_rt() function of RTDM simply load this module with
 * the local IP address as parameter local_ip_s and send udp packets to
 * the ports 36000 or 45054 on the RTnet interface from a non realtime
 * host on the same network using the program netcat while watching the
 * system log messages of the RTnet host. For example:
 *
 * rtnet$ insmod ./rt_server local_ip_s="10.0.0.1"
 *
 * otherhost$ echo test | nc -u -q1 10.0.0.1 36000
 * otherhost$ echo test | nc -u -q1 10.0.0.1 45054
 *
 * rtnet$ rmmod rt_server
 * otherhost$ echo test | nc -u -q1 10.0.0.1 36000
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

#define MIN_LENGTH_IPv4 7
#define MAX_LENGTH_IPv4 15
static char *local_ip_s  = "192.168.6.4";

MODULE_PARM (local_ip_s ,"s");
MODULE_PARM_DESC (local_ip_s, "local ip-addr");
MODULE_LICENSE("GPL");

#define PRINT 0 /* real-time fifo */
RT_TASK rt_task;

#define RCV_PORT1	36000
#define RCV_PORT2	45054

static struct sockaddr_in local_addr1;
static struct sockaddr_in local_addr2;

static int sock1, sock2;
static volatile int exit_select;

#define BUFSIZE 1500
char buffer[BUFSIZE];


unsigned long rt_inet_aton(const char *ip)
{
    int p, n, c;
    union { unsigned long l; char c[4]; } u;
    p = n = 0;
    while ((c = *ip++)) {
	if (c != '.') {
	    n = n*10 + c-'0';
	} else {
	    if (n > 0xFF) {
		return 0;
	    }
	    u.c[p++] = n;
	    n = 0;
	}
    }
    u.c[3] = n;
    return u.l;
}


int packetsize = 58;

void* process(void * arg)
{
    fd_set readfds;
    FD_ZERO(&readfds);
    struct msghdr		msg;
    struct iovec		iov;
    struct sockaddr_in		addr;
    
    memset(&msg, 0, sizeof(msg));
    iov.iov_base=&buffer;
    iov.iov_len=BUFSIZE;
    msg.msg_name=&addr;
    msg.msg_namelen=sizeof(addr);
    msg.msg_iov=&iov;
    msg.msg_iovlen=1;
    msg.msg_control=NULL;
    msg.msg_controllen=0;
    
    /*
      FD_SET((sock1 & (MAX_FILDES - 1)), &readfds);
      FD_SET((sock2 & (MAX_FILDES - 1)), &readfds);
    */
    FD_SET(sock1, &readfds);
    FD_SET(sock2, &readfds);

    rt_printk(" --> selecting task started\n");
    while (0==exit_select) {
	int ret;
	ret = select_rt(0, sock1 > sock2 ? sock1 : sock2,
			&readfds, 0, 0);
	
	while (0<recvmsg_rt(sock1, &msg, MSG_DONTWAIT)) {
	    rt_printk("  # received data on socket 1\n");
	}
	
	while (0<recvmsg_rt(sock2, &msg, MSG_DONTWAIT)) {
	    rt_printk("  # received data on socket 2\n");
	}
    }
    rt_printk(" --> selecting task finished\n");
    exit_select=2;
    return NULL;
}


int init_module(void)
{
    unsigned int local_ip = rt_inet_aton(local_ip_s);
    int ret;

    exit_select=0;

    printk("*** test module for select_rt() *** 2004 by Hans-Peter Bock <rtnet@avaapgh.de>\n");
    printk("*** compiled on %s %s\n", __DATE__, __TIME__);

    printk("==> initializing module\n");
    printk("  * local ip address (%s)\n", local_ip_s);

    /* create rt-socket */
    printk("  * creating socket 1\n");	
    if ((sock1=socket_rt(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	printk("!!! socket not created\n");
	return -ENOMEM;
    }

    printk("  * creating socket 2\n");	
    if ((sock2=socket_rt(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
	printk("!!! socket not created, please reboot\n");
	return -ENOMEM;
    }
	
    /* bind the rt-socket to local_addr */	
    printk("  * binding socket 1 to local address:port\n");
    memset(&local_addr1, 0, sizeof(struct sockaddr_in));
    local_addr1.sin_family = AF_INET;
    local_addr1.sin_port = htons(RCV_PORT1);
    local_addr1.sin_addr.s_addr = local_ip;
    if ( (ret=bind_rt(sock1, (struct sockaddr *) &local_addr1, sizeof(struct sockaddr_in)))<0 ) {
	printk("!!! can't bind rtsocket, please reboot\n");
	return ret;
    }
	
    printk("  * binding socket 2 to local address:port\n");
    memset(&local_addr1, 0, sizeof(struct sockaddr_in));
    local_addr2.sin_family = AF_INET;
    local_addr2.sin_port = htons(RCV_PORT2);
    local_addr2.sin_addr.s_addr = local_ip;
    if ( (ret=bind_rt(sock2, (struct sockaddr *) &local_addr2, sizeof(struct sockaddr_in)))<0 ) {
	printk("!!! can't bind rtsocket, please reboot\n");
	return ret;
    }
    printk(" --> sockets created: %d / %d.\n", sock1, sock2);
	
    /* create print-fifo */
    rtf_create(PRINT, 3000);
    
    ret=rt_task_init(&rt_task, (void*) process, 0, 4096, 9, 0, NULL);
    if (0 == ret) {
	rt_task_resume (&rt_task);
    }
    printk("==> initialization complete\n");
    return ret;
}


void cleanup_module(void)
{
    int counter=0;
    printk("==> cleanup of module started\n");

    /* wait for selecting task to exit */
    exit_select=1;
    while (2 != exit_select) {
	printk("  * waiting for selecting task to exit (%d)\n", counter++);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1*HZ); /* wait a second */
    }
    
    /* close sockets */
    counter=0;
    while (close_rt(sock1) == -EAGAIN) {
	printk("  * waiting for socket 1 to be closed (%d)\n", counter++);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1*HZ); /* wait a second */
    }
    printk(" --> socket 1 closed\n");
    counter=0;
    while (close_rt(sock2) == -EAGAIN) {
	printk("  * waiting for socket 2 to be closed (%d)\n", counter++);
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(1*HZ); /* wait a second */
    }
    printk(" --> socket 2 closed\n");
	
    /* delete task and fifo */
    rt_task_delete(&rt_task);
    rtf_destroy(PRINT);
    printk("==> module unloaded\n\n");
}

/*
 * Local variables:
 * c-basic-offset: 4
 * End:
 */
