/***
 *
 *  rtnet/module/rt_echo_client/rt_echo_client.c
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
static char *server_ip_s = "127.0.0.1";
int interval = 500; /* time between two sent packets in ms (1-1000) */
int packetsize = 58; /* packetsize exclusive headers (1-1400) */

MODULE_PARM (local_ip_s ,"s");
MODULE_PARM (server_ip_s,"s");
MODULE_PARM (interval, "i");
MODULE_PARM (packetsize,"i");
MODULE_PARM_DESC (local_ip_s, "rt_echo_client: local ip-address");
MODULE_PARM_DESC (server_ip_s, "rt_echo_client: server ip-address");

#define TICK_PERIOD	100000
RT_TASK rt_task;

#define RCV_PORT	35999
#define SRV_PORT	36000

static struct sockaddr_in server_addr;
static struct sockaddr_in local_addr;

static int sock;

#define BUFSIZE 1500
static char buffer[BUFSIZE], sendbuffer[BUFSIZE];
static RTIME tx_time;
static RTIME rx_time;

#define PRINT 0

unsigned long tsc1,tsc2;
unsigned long cnt = 0;
unsigned long sent=0, rcvd=0;

void *process(void * arg)
{
	int ret = 0;

	while(1) {

	        /* wait one period */ 
	        rt_task_wait_period();

                /* get time        */
		tx_time = rt_get_cpu_time_ns();

		memcpy (sendbuffer, &tx_time, sizeof(tx_time));

                /* send the time   */    
		ret=rt_socket_sendto
			(sock, &sendbuffer, packetsize, 0, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_in));
		if (ret) sent++;
	}
}



int echo_rcv(int s,void *arg)
{
	int			ret=0;
	struct msghdr		msg;
	struct iovec		iov;
	struct sockaddr_in	addr;
	

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
		
		/* get the time    */
		rx_time = rt_get_cpu_time_ns();
		memcpy (&tx_time, buffer, sizeof(RTIME));
		rcvd++;

		ret = rtf_put(PRINT, &rx_time, sizeof(RTIME));
		ret = rtf_put(PRINT, &tx_time, sizeof(RTIME));
		if (0 == ret) {
			rt_task_suspend(&rt_task);
			rt_printk("fifo full, sending task suspended\n");
		}

		/* copy the address */
		rcv.l = sin->sin_addr.s_addr;
	}

	return 0;
}


int init_module(void)
{
	int ret;
	RTIME tick_period;

	unsigned int local_ip  = rt_inet_aton(local_ip_s);
	unsigned int server_ip = rt_inet_aton(server_ip_s);

	if (interval < 1) interval = 1;
	if (interval > 1000) interval = 1000;

	if (packetsize < 1) packetsize = 1;
	if (packetsize > 5400) packetsize = 1400;

	printk ("***** start of rt_client ***** %s %s *****\n", __DATE__, __TIME__);
	printk ("local  ip address %s=%08x\n", local_ip_s, local_ip);
	printk ("server ip address %s=%08x\n", server_ip_s, server_ip);
	printk ("interval = %d\n", interval);
	printk ("packetsize = %d\n", packetsize);

	rtf_create(PRINT, 8000);

	/* create rt-socket */
	sock=rt_socket(AF_INET,SOCK_DGRAM,0);
	
	/* bind the rt-socket to local_addr */	
	memset(&local_addr, 0, sizeof(struct sockaddr_in));
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(RCV_PORT);
	local_addr.sin_addr.s_addr = local_ip;
	ret=rt_socket_bind(sock, (struct sockaddr *) &local_addr, sizeof(struct sockaddr_in));

	/* set server-addr */
	memset(&server_addr, 0, sizeof(struct sockaddr_in));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SRV_PORT);
	server_addr.sin_addr.s_addr = server_ip;

	/* set up receiving */
	rt_socket_callback(sock, echo_rcv, NULL);
	
        tick_period = start_rt_timer(nano2count(TICK_PERIOD));
        ret=rt_task_init(&rt_task,(void *)process,0,4096,10,0,NULL);
        ret=rt_task_make_periodic_relative_ns( &rt_task, 1000000, (RTIME) interval * 1000000);
	return 0;
}




void cleanup_module(void)
{
        /* stop timer         */ 
  	stop_rt_timer();

        /* rt_task_delete     */ 
  	rt_task_delete(&rt_task);

        /* close th rt-socket */
  	rt_socket_close(sock);

	/* destroy the fifo   */
	rtf_destroy(PRINT);

	printk ("packets sent:\t\t%10lu\npackets received:\t%10lu\npacketloss:\t\t%10lu%%\n", sent, rcvd, 100-((100*rcvd)/sent));
}
