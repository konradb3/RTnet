/* ipv4/icmp.c
 *
 * rtnet - real-time networking subsystem
 * Copyright (C) 1999,2000 Zentropic Computing, LLC
 *               2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *               2002 Vinay Sridhara <vinaysridhara@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/types.h>
#include <linux/icmp.h>
#include <net/checksum.h>

#include <rtskb.h>
#include <rtnet_socket.h>
#include <ipv4/ip_output.h>
#include <ipv4/protocol.h>
#include <ipv4/route.h>


struct rt_icmp_control
{
	void (*handler)(struct rtskb *skb);
	short	error;				/* This ICMP is classed as an error message */
};


/***
 *	icmp_sockets
 *	the registered sockets from any server
 */
struct rtsocket *icmp_sockets;          
//static unsigned short good_port=1024;

spinlock_t icmp_socket_base_lock;

struct rtsocket_ops rt_icmp_socket_ops = {
	bind:		NULL,
	connect:	NULL,
	listen:		NULL,
	accept:		NULL,
	recvmsg:	NULL,
	sendmsg:	NULL,
	close:		NULL,
#if 0        
	bind:		&rt_udp_bind,
	connect:	&rt_udp_connect,
	listen:		&rt_udp_listen,
	accept:		&rt_udp_accept,
	recvmsg:	&rt_udp_recvmsg,
	sendmsg:	&rt_udp_sendmsg,
	close:		&rt_udp_close,
#endif
};

static struct rt_icmp_control rt_icmp_pointers[NR_ICMP_TYPES+1];

/***
 * Structure for sending the icmp packets
 */

struct icmp_bxm
{
	struct rtskb *skb;
	int offset;
	int data_len;

	unsigned int csum;

	struct{
		struct icmphdr icmph;
		__u32		times[3];
	}data;
	int head_len;
	struct ip_options replyopts;
	unsigned char optbuf[40];
};

/***
 *	rt_icmp_discard - dummy function
 */
static void rt_icmp_discard(struct rtskb *skb)
{
}


/***
 *	rt_icmp_unreach - dummy function
 */
static void rt_icmp_unreach(struct rtskb *skb)
{
}


/***
 *	rt_icmp_redirect - dummy function
 */
static void rt_icmp_redirect(struct rtskb *skb)
{
}

static int rt_icmp_glue_bits(const void *p, char *to, unsigned int offset, unsigned int fraglen)
{
	struct icmp_bxm *icmp_param = (struct icmp_bxm *)p;
	struct icmphdr *icmph;
	unsigned long csum;
	unsigned int header_len;

	header_len = sizeof(struct icmphdr);

	if(offset)
	{
		icmp_param->csum = rtskb_copy_and_csum_bits(icmp_param->skb, 
				icmp_param->offset+(offset-icmp_param->head_len),
				to, fraglen, icmp_param->csum);
		return 0;
	}

	csum = csum_partial_copy_nocheck((void *)&icmp_param->data,
			to, icmp_param->head_len,
			icmp_param->csum);

	csum = rtskb_copy_and_csum_bits(icmp_param->skb,
					icmp_param->offset,
					to+icmp_param->head_len,
					fraglen-icmp_param->head_len, 
					csum);

	icmph = (struct icmphdr *)to;

	icmph->checksum = csum_fold(csum);

	return 0;
}




/***
 *  common reply function
 */
static void rt_icmp_reply(struct icmp_bxm *icmp_param, struct rtskb *skb)
{
	/* just create a dummy socket */
	struct rtsocket *sk = NULL;

	struct ipcm_cookie ipc;
	struct rt_rtable *rt = NULL;
	u32 daddr;
	int err;

	/* just create a dummy socket for the reply */
	/* we dont have to worry too much about the socket here coz */
	/* in the xmit function socket is used only to find the protocol type */
	sk = rt_socket_alloc();

	if(sk)
	{
		sk->family = AF_INET;
		sk->typ = SOCK_DGRAM;
		sk->protocol = IPPROTO_ICMP;
		sk->ops = &rt_icmp_socket_ops;
		sk->prev = NULL;
		sk->next = NULL;
		sk->saddr = skb->nh.iph->daddr;
	}
	else
	{
		rt_printk("RTnet : could not allocate a socket\n");
		goto error;
	}

	rt = (struct rt_rtable*)skb->dst;

	if(rt == NULL)
	{
		/* NO ROUTE FOR THE REPLY PACKET */
		/* SHOULD NOT BE THE CASE */
		/* DEBUGGING NECESSARY SOMEWHERE */

		rt_printk("RTnet : error in route table\n");
		goto error;
	}

	if(!sk)
	{
		/* WHAT ???? */
		rt_printk("RTnet : socket is NULL %p\n", icmp_sockets);
		goto error;
	}

	icmp_param->data.icmph.checksum = 0;
	icmp_param->csum = 0;


	daddr = ipc.addr = rt->rt_dst;
	ipc.opt = &icmp_param->replyopts;

	err = rt_ip_route_output(&rt, daddr, sk->saddr);

	if(err)
	{
		rt_printk("RTnet : error in route daddr %x saddr %x\n", daddr, sk->saddr);
		goto error;
	}

	err = rt_ip_build_xmit(sk, rt_icmp_glue_bits, icmp_param, 
			icmp_param->data_len+sizeof(struct icmphdr),
			rt, MSG_DONTWAIT);

	if(err)
	{
		rt_printk("RTnet : error in xmit\n");
		goto error;
	}

	//rt_printk("RTnet : sent a reply successfully\n");
	//
	rt_socket_release(sk);

	return;

error:
	rt_printk("RTnet : error \n");
	return;
}


/***
 *	rt_icmp_echo - 
 */
static void rt_icmp_echo(struct rtskb *skb)
{
	struct icmp_bxm icmp_param;

	icmp_param.data.icmph = *skb->h.icmph;
	icmp_param.data.icmph.type = ICMP_ECHOREPLY;
	icmp_param.skb = skb;
	icmp_param.offset = 0;
	icmp_param.data_len = skb->len;
	icmp_param.head_len = sizeof(struct icmphdr);

	//rt_printk("RTnet : replying to an echo request\n");

	rt_icmp_reply(&icmp_param, skb);

	return;
}


/***
 *	rt_icmp_timestamp - 
 */
static void rt_icmp_timestamp(struct rtskb *skb)
{
}


/***
 *	rt_icmp_address - 
 */
static void rt_icmp_address(struct rtskb *skb)
{
}


/***
 *	rt_icmp_address_reply
 */
static void rt_icmp_address_reply(struct rtskb *skb)
{
}


/***
 *	rt_icmp_socket
 */
int rt_icmp_socket(struct rtsocket *sock) 
{

	unsigned long flags;
	
	sock->family=AF_INET;
	sock->typ=SOCK_DGRAM;
	sock->protocol=IPPROTO_ICMP;
	sock->ops = &rt_icmp_socket_ops;

	/* add to udp-socket-list */
	sock->prev = NULL;

	flags = rt_spin_lock_irqsave(&icmp_socket_base_lock);
	sock->next=icmp_sockets;
	if (icmp_sockets!=NULL)
		icmp_sockets->prev=sock;
	sock->prev=NULL;
	icmp_sockets=sock;
	rt_spin_unlock_irqrestore(flags, &icmp_socket_base_lock);

	rt_printk("RTnet : into rt_icmp_socket\n");
	
	return sock->fd;
}


static struct rt_icmp_control rt_icmp_pointers[NR_ICMP_TYPES+1] = 
{
	/* ECHO REPLY (0) */
	{ rt_icmp_discard,	0 },
	{ rt_icmp_discard,	1 },
	{ rt_icmp_discard,	1 },

	/* DEST UNREACH (3) */
	{ rt_icmp_unreach,	1 },

	/* SOURCE QUENCH (4) */
	{ rt_icmp_unreach,	1 },

	/* REDIRECT (5) */
	{ rt_icmp_redirect,	1 },
	{ rt_icmp_discard,	1 },
	{ rt_icmp_discard,	1 },

	/* ECHO (8) */
	{ rt_icmp_echo,	0 },
	{ rt_icmp_discard,	1 },
	{ rt_icmp_discard,	1 },

	/* TIME EXCEEDED (11) */
	{ rt_icmp_unreach,	1 },

	/* PARAMETER PROBLEM (12) */
	{ rt_icmp_unreach,	1 },

	/* TIMESTAMP (13) */
	{ rt_icmp_timestamp,	0  },

	/* TIMESTAMP REPLY (14) */
	{ rt_icmp_discard,	0 },

	/* INFO (15) */
	{ rt_icmp_discard,	0 },

	/* INFO REPLY (16) */
	{ rt_icmp_discard,	0 },

	/* ADDR MASK (17) */
	{ rt_icmp_address,	0  },

	/* ADDR MASK REPLY (18) */
	{ rt_icmp_address_reply, 0 }
};



/***
 *	rt_icmp_rcv
 */
int rt_icmp_rcv(struct rtskb *skb)
{
	struct icmphdr *icmpHdr = skb->h.icmph;
	int length = ntohs(skb->nh.iph->tot_len) - (skb->nh.iph->ihl*4);

	if(length < sizeof(struct icmphdr))
	{
		rt_printk("RTnet : improper length in icmp packet\n");
		goto error;
	}

	if(ip_compute_csum((unsigned char *)icmpHdr, length))
	{
		rt_printk("RTnet : invalid checksum in icmp packet %d\n", 
									length);
		goto error;
	}

	if(!rtskb_pull(skb, sizeof(struct icmphdr)))
	{
		rt_printk("RTnet : pull failed %p\n", (skb->sk));
		goto error;
	}


	if(icmpHdr->type > NR_ICMP_TYPES)
	{
		rt_printk("RTnet : invalid icmp type\n");
		goto error;
	}
	
	/* SANE PACKET ... PROCESS IT */

	length = length - sizeof(struct icmphdr);

	(rt_icmp_pointers[icmpHdr->type].handler)(skb);
	
error:
	kfree_rtskb(skb);
	return 0;
}

/***
 *	rt_icmp_rcv_err
 */
void rt_icmp_rcv_err (struct rtskb *skb) 
{
	rt_printk("RTnet: rt_icmp_rcv err\n");
}

/***
 *	ICMP-Initialisation	
 */
static struct rtinet_protocol icmp_protocol = {
	protocol:	IPPROTO_ICMP,
	handler:	&rt_icmp_rcv,
	err_handler:	&rt_icmp_rcv_err,
	socket:		&rt_icmp_socket,
};


/***
 *	rt_icmp_init
 */
void rt_icmp_init(void)
{
	spin_lock_init(&icmp_socket_base_lock);
	icmp_sockets=NULL;
	rt_inet_add_protocol(&icmp_protocol);
}

/***
 *	rt_icmp_release
 */
void rt_icmp_release(void)
{
	rt_inet_del_protocol(&icmp_protocol);
	icmp_sockets=NULL;
}
