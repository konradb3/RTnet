/* ipv4/udp.c
 *
 * rtnet - real-time networking subsystem
 * Copyright (C) 1999,2000 Zentropic Computing, LLC
 *               2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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

#include <linux/socket.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <net/checksum.h>

#include <rtskb.h>
#include <rtnet_internal.h>
#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <ipv4/ip_output.h>
#include <ipv4/protocol.h>
#include <ipv4/route.h>


/***
 *	udp_sockets
 *	the registered sockets from any server
 */
struct rtsocket *udp_sockets;          
static unsigned short good_port=1024;

//rwlock_t udp_socket_base_lock = RW_LOCK_UNLOCKED;
//SEM udp_socket_sem;
spinlock_t  udp_socket_base_lock;

/***
 *	rt_udp_port_inuse
 */
static inline int rt_udp_port_inuse(u16 num)
{
	unsigned long flags;
	struct rtsocket *sk;

	flags = rt_spin_lock_irqsave(&udp_socket_base_lock);
	for (sk=udp_sockets; sk!=NULL; sk=sk->next) {
		if ( sk->sport==num )
			break;
	}
	rt_spin_unlock_irqrestore(flags, &udp_socket_base_lock);
	return (sk != NULL) ? 1 : 0;
}



/***
 *	rt_udp_good_port
 */
unsigned short rt_udp_good_port(void)
{
	unsigned short good;

	do {
		good=good_port++;
	} while (rt_udp_port_inuse(good));

	return good;
}



/***
 *	rt_udp_v4_lookup
 */
struct rtsocket *rt_udp_v4_lookup(u32 saddr, u16 sport, u32 daddr, u16 dport)
{
	unsigned long flags;
	struct rtsocket *sk;

	flags = rt_spin_lock_irqsave(&udp_socket_base_lock);
	for (sk=udp_sockets; sk!=NULL; sk=sk->next) {
		if (sk->sport==dport)
			break;
	}
	rt_spin_unlock_irqrestore(flags, &udp_socket_base_lock);
	return sk;
}



/***
 *	rt_udp_bind	- bind socket to local address
 *	@s:		socket
 *	@addr:		local address	
 */
int rt_udp_bind(struct rtsocket *s, struct sockaddr *addr, int addrlen)
{
	struct sockaddr_in *usin = (struct sockaddr_in *)addr;
	
	if ( (s->state!=TCP_CLOSE) || (addrlen<(int)sizeof(struct sockaddr_in)) )
		return -EINVAL;
	
	/* set the source-addr */	
	s->saddr=usin->sin_addr.s_addr;

	/* set source port, if not set by user */
	if( !(s->sport=usin->sin_port) )
		s->sport=htons(rt_udp_good_port());

	return 0;
}



/***
 *	rt_udp_connect
 */
int rt_udp_connect(struct rtsocket *s, struct sockaddr *serv_addr, int addrlen)
{
	struct sockaddr_in *usin = (struct sockaddr_in *) serv_addr;

	if ( (s->state!=TCP_CLOSE) || (addrlen < (int)sizeof(struct sockaddr_in)) ) 
	  	return -EINVAL;
	if ( (usin->sin_family) && (usin->sin_family!=AF_INET) ) {
		s->saddr=INADDR_ANY;
		s->daddr=INADDR_ANY;
		s->state=TCP_CLOSE;
	  	return -EAFNOSUPPORT;
	}
	s->state=TCP_ESTABLISHED;
	s->daddr=usin->sin_addr.s_addr;
	s->dport=usin->sin_port;

	rt_printk("connect socket to %x:%d\n", ntohl(s->daddr), ntohs(s->dport));

	return 0;
}



/***
 *	rt_udp_listen
 *	@s
 *	@backlog
 */
int rt_udp_listen(struct rtsocket *s, int backlog)
{
	/* UDP = connectionless */
	return 0;
}



/***
 *	rt_udp_accept
 */
int rt_udp_accept(struct rtsocket *s, struct sockaddr *client_addr, int *addr_len)
{
	/* UDP = connectionless */
	return 0;
}



/***
 *	rt_udp_recvmsg
 */
int rt_udp_recvmsg(struct rtsocket *s, struct msghdr *msg, int len)
{
	unsigned copied=0;
	struct rtskb *skb;	

	/* fetch packet from incomming queue */
	if ( (skb=rtskb_dequeue(&s->incoming))!=NULL ) {
		struct sockaddr_in *sin=msg->msg_name;
		/* copy the address */
		msg->msg_namelen=sizeof(*sin);
		if (sin) {
			sin->sin_family = AF_INET;
			sin->sin_port = skb->h.uh->source;
			sin->sin_addr.s_addr = skb->nh.iph->saddr;
  		}

		/* copy the data */
		copied = skb->len-sizeof(struct udphdr);
                /* The data must not be longer than the value of the parameter "len" in
                 * the socket recvmsg call */
                if (copied > msg->msg_iov->iov_len)
                {
                    copied = msg->msg_iov->iov_len;
                }
		rt_memcpy_tokerneliovec(msg->msg_iov, skb->h.raw+sizeof(struct udphdr), copied);

		kfree_rtskb(skb);
	}	
	return copied;
}



/***
 *	struct udpfakehdr 
 */
struct udpfakehdr 
{
	struct udphdr uh;
	u32 daddr;
	u32 saddr;
	struct iovec *iov;
	u32 wcheck;
};



/***
 *
 */
static int rt_udp_getfrag(const void *p, char * to, unsigned int offset, unsigned int fraglen) 
{
	struct udpfakehdr *ufh = (struct udpfakehdr *)p;

	if (offset==0) {

                /* Checksum of the complete data part of the UDP message: */
 		ufh->wcheck = csum_partial(ufh->iov->iov_base, ufh->iov->iov_len, ufh->wcheck);
	
		rt_memcpy_fromkerneliovec(to+sizeof(struct udphdr), ufh->iov,fraglen-sizeof(struct udphdr));
		
                /* Checksum of the udp header: */
 		ufh->wcheck = csum_partial((char *)ufh, sizeof(struct udphdr),ufh->wcheck);

		ufh->uh.check = csum_tcpudp_magic(ufh->saddr, ufh->daddr, ntohs(ufh->uh.len), IPPROTO_UDP, ufh->wcheck);
		
		if (ufh->uh.check == 0)
			ufh->uh.check = -1;

		memcpy(to, ufh, sizeof(struct udphdr));
		return 0;
	}
        
        rt_memcpy_fromkerneliovec(to, ufh->iov, fraglen);
        
	
	return 0;
}



/***
 *	rt_udp_sendmsg
 */
int rt_udp_sendmsg(struct rtsocket *s, const struct msghdr *msg, int len)
{
	int ulen = len + sizeof(struct udphdr);

	struct udpfakehdr ufh;
	struct rt_rtable *rt = NULL;

	u32 daddr;	
	u16 dport;
	int err;
	
	if ( (len<0) || (len>0xFFFF) )
		return -EMSGSIZE;
		
	if (msg->msg_flags & MSG_OOB)	/* Mirror BSD error message compatibility */
		return -EOPNOTSUPP;

	if (msg->msg_flags & ~(MSG_DONTROUTE|MSG_DONTWAIT|MSG_NOSIGNAL) )
	  	return -EINVAL;
		
	if ( (msg->msg_name) && (msg->msg_namelen==sizeof(struct sockaddr_in)) ) {
		struct sockaddr_in *usin = (struct sockaddr_in*) msg->msg_name;
		if ( (usin->sin_family!=AF_INET) && (usin->sin_family!=AF_UNSPEC) )
			return -EINVAL;
		daddr = usin->sin_addr.s_addr;
		dport = usin->sin_port;
	} else {
		if (s->state != TCP_ESTABLISHED)
			return -ENOTCONN;
		daddr = s->daddr;
		dport = s->dport;
  	}

#ifdef DEBUG
	rt_printk("sendmsg to %x:%d\n", ntohl(daddr), ntohs(dport));	
#endif
	if ( (daddr==0) || (dport==0) )
		return -EINVAL;

	err=rt_ip_route_output(&rt, daddr, s->saddr);
	if (err)
		goto out;
	
	/* we found a route, remeber the routing dest-addr could be the netmask */	
	ufh.saddr	= rt->rt_src; 
	ufh.daddr	= daddr;
	ufh.uh.source	= s->sport;
	ufh.uh.dest	= dport;
	ufh.uh.len 	= htons(ulen);
	ufh.uh.check 	= 0;
	ufh.iov 	= msg->msg_iov;
	ufh.wcheck 	= 0;
		
	err = rt_ip_build_xmit(s, rt_udp_getfrag, &ufh, ulen, rt, msg->msg_flags);

out:
	if (!err) {
		return len;
	}
	return err;
}



/***
 *	rt_udp_close
 */
void rt_udp_close(struct rtsocket *s,long timeout)
{
	unsigned long flags;
	struct rtsocket *prev=s->prev;
	struct rtsocket *next=s->next;
	struct rtskb *del;

	s->state=TCP_CLOSE;

	flags = rt_spin_lock_irqsave(&udp_socket_base_lock);

	if (prev) prev->next=next;
	if (next) next->prev=prev;
		
	/* free packets in incoming queue */
	while (0 != (del=rtskb_dequeue(&s->incoming))) {
		kfree_rtskb(del);
	}
		
	if (s==udp_sockets) {
		udp_sockets=udp_sockets->next;
	}

	s->next=NULL;
	s->prev=NULL;
		
	rt_spin_unlock_irqrestore(flags, &udp_socket_base_lock);
}



/***
 *	rt_udp_check
 */
static unsigned short rt_udp_check(struct udphdr *uh, int len, 
			  	   unsigned long saddr, 
				   unsigned long daddr, 
				   unsigned long base)
{
	return(csum_tcpudp_magic(saddr, daddr, len, IPPROTO_UDP, base));
}



/***
 *	rt_udp_rcv
 */
int rt_udp_rcv (struct rtskb *skb) 
{
  	struct udphdr *uh;
	unsigned short ulen;
	struct rtsocket *rtsk=NULL;
	
	u32 saddr = skb->nh.iph->saddr;
	u32 daddr = skb->nh.iph->daddr;
	
	uh=skb->h.uh;
	ulen = ntohs(uh->len);
	rtskb_trim(skb, ulen);

	if (uh->check == 0)
		skb->ip_summed = CHECKSUM_UNNECESSARY;
	else 
		if (skb->ip_summed == CHECKSUM_HW) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			if ( !rt_udp_check(uh, ulen, saddr, daddr, skb->csum) )
				goto drop;
			skb->ip_summed = CHECKSUM_NONE;
		}
		
	if (skb->ip_summed != CHECKSUM_UNNECESSARY)
		skb->csum = csum_tcpudp_nofold(saddr, daddr, ulen, IPPROTO_UDP, 0);

	/* find a socket */
	if ( (rtsk=rt_udp_v4_lookup(saddr, uh->source, daddr, uh->dest))==NULL )
		goto drop;
	else {
		/* rt_udp_deliver(rtsk, skb); */
		if (rtskb_queue_len(&rtsk->incoming)>=DROPPING_RTSKB)
			goto drop;
		else {
			__rtskb_queue_tail(&rtsk->incoming, skb);
			if ( rtsk->wakeup!=NULL )
				rtsk->wakeup(rtsk->fd, rtsk->private);
		}
	}

	return 0;

drop:
	kfree_rtskb(skb);
	return 0;
}


/***
 *	rt_udp_rcv_err
 */
void rt_udp_rcv_err (struct rtskb *skb) 
{
	rt_printk("RTnet: rt_udp_rcv err\n");
}


struct rtsocket_ops rt_udp_socket_ops = {
	bind:		&rt_udp_bind,
	connect:	&rt_udp_connect,
	listen:		&rt_udp_listen,
	accept:		&rt_udp_accept,
	recvmsg:	&rt_udp_recvmsg,
	sendmsg:	&rt_udp_sendmsg,
	close:		&rt_udp_close,
};


/***
 *	rt_udp_socket	- create a new UDP-Socket 
 *	@s:		socket
 */
int rt_udp_socket(struct rtsocket *s) 
{
	unsigned long flags;

	s->family=AF_INET;
	s->typ=SOCK_DGRAM;
	s->protocol=IPPROTO_UDP;
	s->ops = &rt_udp_socket_ops;
	
	/* add to udp-socket-list */
	s->prev=NULL;

//	rt_sem_wait(&udp_socket_sem);
//	write_lock(&udp_socket_base_lock);
	flags = rt_spin_lock_irqsave(&udp_socket_base_lock);
	s->next=udp_sockets;
	if (udp_sockets!=NULL)
		udp_sockets->prev=s;
	udp_sockets=s;
	rt_spin_unlock_irqrestore(flags, &udp_socket_base_lock);
//	write_unlock(&udp_socket_base_lock);
//	rt_sem_signal(&udp_socket_sem);
	
	return s->fd;
}



/***
 *	UDP-Initialisation	
 */
static struct rtinet_protocol udp_protocol = {
	protocol:	IPPROTO_UDP,
	handler:	&rt_udp_rcv,
	err_handler:	&rt_udp_rcv_err,
	socket:		&rt_udp_socket,
};

/***
 *	rt_udp_init
 */
void rt_udp_init(void)
{
//	rt_typed_sem_init(&udp_socket_sem, 0, BIN_SEM);
	spin_lock_init(&udp_socket_base_lock);

	udp_sockets=NULL;	
	rt_inet_add_protocol(&udp_protocol);
}

/***
 *	rt_udp_release
 */
void rt_udp_release(void)
{
	rt_inet_del_protocol(&udp_protocol);
	udp_sockets=NULL;

//	rt_sem_delete(&udp_socket_sem);
}
