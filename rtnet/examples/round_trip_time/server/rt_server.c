/***
 *
 *  rtnet/module/rt_echo_client/rt_server.c
 *  module that tests echo capability of server
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *
 *  rtnet - real-time networking example
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
#include <rtai_fifos.h>

#include <rtnet.h>


#define MIN_LENGTH_IPv4 7
#define MAX_LENGTH_IPv4 15
static char *local_ip_s  = "127.0.0.1";
static char *client_ip_s = "127.0.0.1";

MODULE_PARM (local_ip_s ,"s");
MODULE_PARM (client_ip_s,"s");
MODULE_PARM_DESC (local_ip_s, "local ip-addr");
MODULE_PARM_DESC (client_ip_s, "client ip-addr");

#define TICK_PERIOD	100000
#define PRINT 0   
RT_TASK rt_task;
SEM	tx_sem;

#define RCV_PORT	36000
#define SRV_PORT	35999

static struct sockaddr_in client_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1500
char buffer[BUFSIZE];
char tx_msg[BUFSIZE];


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

void *process(void * arg)
{
	int ret = 0;
	while(1) {
	        rt_sem_wait(&tx_sem);
	     	ret=rt_socket_sendto
	      	    (sock, &tx_msg, packetsize, 0, (struct sockaddr *) &client_addr, sizeof (struct sockaddr_in));
	}
}



int echo_rcv(int s,void *arg)
{
	int			ret=0;
	struct msghdr		msg;
	struct iovec		iov;
	struct sockaddr_in	addr;

	memset(&msg, 0, sizeof(msg));
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
		if ((0 < ret) && (ret <= 1400))
			packetsize = ret;
		memcpy(&tx_msg, &buffer, packetsize);
		rt_sem_signal(&tx_sem);
	}

	return 0;
}



int init_module(void)
{
	int ret;
	struct rtsocket *socket;

	unsigned int local_ip  = rt_inet_aton(local_ip_s);
	unsigned int client_ip = rt_inet_aton(client_ip_s);


	printk ("local  ip address %s=%08x\n", local_ip_s, local_ip);
	printk ("client ip address %s=%08x\n", client_ip_s, client_ip);

	/* create rt-socket */
	printk("create rtsocket\n");	
	if ( !(sock=rt_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) ) {
		printk("socket not created\n");
		return -ENOMEM;
	}
	
	/* bind the rt-socket to local_addr */	
	printk("bind rtsocket to local address:port\n");
	memset(&local_addr, 0, sizeof(struct sockaddr_in));
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(RCV_PORT);
	local_addr.sin_addr.s_addr = local_ip;
	if ( (ret=rt_socket_bind(sock, (struct sockaddr *) &local_addr, sizeof(struct sockaddr_in)))<0 ) {
		printk("can't bind rtsocket\n");
		return ret;
	}
	
	/* set client-addr */
	printk("connect rtsocket to client address:port\n");
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(SRV_PORT);
	client_addr.sin_addr.s_addr = client_ip;
	if ( (ret=rt_socket_connect(sock, (struct sockaddr *) &client_addr, sizeof(struct sockaddr_in)))<0 ) {
		printk("can't connect rtsocket\n");
		return ret;
	}

	/* get socket-structure for printing */
	if ( (socket=rt_socket_lookup(sock)) ) {
		printk("src  addr: %08x:%04x\n", socket->saddr, socket->sport);
		printk("dest addr: %08x:%04x\n", socket->daddr, socket->dport);
	}

	/* set up receiving */
	rt_socket_callback(sock, echo_rcv, NULL);

	/* initialize semaphore */
	rt_sem_init(&tx_sem, 0);

	/* create print-fifo */
	rtf_create (PRINT, 3000);

	ret=rt_task_init(&rt_task,(void *)process,0,4096,0,0,NULL);
	rt_task_resume (&rt_task);

	return ret;
}




void cleanup_module(void)
{
	rt_task_delete(&rt_task);
	rtf_destroy(PRINT);
	rt_sem_delete(&tx_sem);

  	rt_socket_close(sock);
}
