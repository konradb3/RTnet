/***
 * rtnet/socket.c - sockets implementation for rtnet
 *
 * Copyright (C) 1999      Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002      Ulrich Marx <marx@kammer.uni-hannover.de>
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
 *
 */
#include <linux/spinlock.h>

#include <linux/socket.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include <rtnet.h>
#include <rtnet_internal.h>
#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <ipv4/protocol.h>

SOCKET rt_sockets[RT_SOCKETS];
SOCKET *free_rtsockets;

//rwlock_t socket_base_lock = RW_LOCK_UNLOCKED;
//SEM socket_sem;
spinlock_t  socket_base_lock;

unsigned int rtsocket_fd = 1; 
#define new_rtsocket_fd() (rtsocket_fd++)

/************************************************************************
 *	internal socket functions					*
 ************************************************************************/

/***
 *	rt_socket_alloc
 */
SOCKET *rt_socket_alloc(void) 
{
	unsigned long flags;
	SOCKET *sock = free_rtsockets;

	if (!sock) 
		return ( NULL );
	else {

		flags = rt_spin_lock_irqsave(&socket_base_lock);
		free_rtsockets=free_rtsockets->next;
		if (free_rtsockets!=NULL)
			free_rtsockets->prev=NULL;
		rt_spin_unlock_irqrestore(flags, &socket_base_lock);

		sock->state=TCP_CLOSE;
		sock->next=NULL;
		return sock;
	}
}


/***
 *	rt_socket_release
 */
void rt_socket_release (SOCKET *sock) 
{
	unsigned long flags;

	memset (sock, 0, sizeof(SOCKET));

	flags = rt_spin_lock_irqsave(&socket_base_lock);
	sock->next=free_rtsockets;
	if (free_rtsockets->next)
		free_rtsockets->next->prev=sock;
	free_rtsockets=sock;
	rt_spin_unlock_irqrestore(flags, &socket_base_lock);
}


/***
 *	rt_scoket_lookup
 *	@fd - file descriptor 
 */
SOCKET *rt_socket_lookup (unsigned int fd) 
{
	int i;
	for (i=0; i<RT_SOCKETS; i++) {
		if (rt_sockets[i].fd==fd)
			return &rt_sockets[i];
	}
	return NULL;
}



/************************************************************************
 *	file discribtor socket interface				*
 ************************************************************************/
/***
 *	rt_socket	
 *	Create a new socket of type TYPE in domain DOMAIN (family), 
 *	using protocol PROTOCOL.  If PROTOCOL is zero, one is chosen 
 *	automatically. Returns a file descriptor for the new socket, 
 *	or errors < 0.
 */
int rt_socket(int family, int type, int protocol)
{
	SOCKET *sock = NULL;
	unsigned char hash;

	/* protol family (PF_INET) and adress family (AF_INET) only */
	if ( (family!=AF_INET) )
		return -EAFNOSUPPORT;

	/* only datagram-sockets */
	if ( type!=SOCK_DGRAM ) 
		return -EAFNOSUPPORT;

	/* allocate a new socket */
	if ( !(sock=rt_socket_alloc()) ) {
		rt_printk("RTnet: no more rt-sockets\n");
		return -ENOMEM;
	}

	/* create new file descriptor */
	sock->fd=new_rtsocket_fd();

	/* default UDP-Protocol */
	if (!protocol)
		hash = rt_inet_hashkey(IPPROTO_UDP);
	else 
		hash = rt_inet_hashkey(protocol);

	/* create the socket (call the socket creator) */
	if  (rt_inet_protocols[hash]) {
		int fd;
		fd = rt_inet_protocols[hash]->socket(sock);

		/* This is the right place to check if sock->ops is not NULL. */
		if (NULL == sock->ops) {
			rt_printk("%s:%s: sock-ops is NULL!\n", __FUNCTION__, __LINE__);
			/* Do something reasonable...
			 * (insert code here)
			 */
		}
		/* This is the right place to check if sock->ops->... are not NULL. */
		return fd;
	} else {
		rt_printk("RTnet: protocol with id %d not found\n", protocol);
		rt_socket_release(sock);
		return -ENOPROTOOPT;
	}
}

/***
 *	rt_sock_bind
 *	Bind a socket to a sockaddr
 */
int rt_socket_bind(int fd, struct sockaddr *my_addr, int addr_len)
{
	SOCKET *sock = rt_socket_lookup(fd);
	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->bind)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( sock->ops->bind(sock, my_addr, addr_len) );
}

/***
 *	rt_socket_listen
 */
int rt_socket_listen(int fd, int backlog)
{
	SOCKET *sock = rt_socket_lookup(fd);
	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->listen)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( sock->ops->listen(sock, backlog) );
}

/***
 *	rt_socket_connect
 */
int rt_socket_connect(int fd, struct sockaddr *serv_addr, int addr_len)
{
	SOCKET *sock = rt_socket_lookup(fd);
	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->connect)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( sock->ops->connect(sock, serv_addr, addr_len) );
}

/***
 *	rt_socket_accept
 */
int rt_socket_accept(int fd, struct sockaddr *client_addr, int *addr_len)
{
	SOCKET *sock = rt_socket_lookup(fd);
	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->accept)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( sock->ops->accept(sock, client_addr, addr_len) );
}

/***
 *	rt_socket_close
 */
int rt_socket_close(int fd)
{
	SOCKET *sock = rt_socket_lookup(fd);
	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->close)) /* There should be no socket without ops! */
		return -ENOTSOCK;

	sock->ops->close(sock,0);
	rt_socket_release(sock);
	return 0;
}

/***
 *	rt_socket_send
 */
int rt_socket_send(int fd, void *buf, int len, unsigned int flags)
{
	return rt_socket_sendto(fd, buf, len, flags, NULL, 0);
}

/***
 *	rt_socket_recv
 */
int rt_socket_recv(int fd, void *buf, int len, unsigned int flags)
{
	int fromlen=0;/*fix for null pointer dereference-NZG*/
        return rt_socket_recvfrom(fd, buf, len, flags, NULL, &fromlen);
}

/***
 *	rt_socket_sendto
 */
int rt_socket_sendto(int fd, void *buf, int len, unsigned int flags, struct sockaddr *to, int tolen)
{
	SOCKET *sock=rt_socket_lookup(fd);
	struct msghdr msg;
	struct iovec iov;

	if ( !sock ) {
		rt_printk("RTnet: socket %d not found\n", fd);
		return -ENOTSOCK;
	}

	if ((NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->sendmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;
		
	iov.iov_base=(void *)buf;
	iov.iov_len=len;
	
	msg.msg_name=to;
	msg.msg_namelen=tolen;
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;
	msg.msg_control=NULL;
	msg.msg_controllen=0;
	msg.msg_flags=flags;

	return sock->ops->sendmsg(sock, &msg, len);
}

/***
 *	rt_recvfrom
 *	@fd		filedescriptor
 *	@buf		buffer
 *	@len		length of buffer
 *	@flags		usermode -> kern 
 *			MSG_DONTROUTE: target is in lan
 *			MSG_DONTWAIT : if there no data to recieve, get out
 *			MSG_ERRQUEUE :
 *			kern->usermode
 *			MSG_TRUNC    :
 */
int rt_socket_recvfrom(int fd, void *buf, int len, unsigned int flags, struct sockaddr *from, int *fromlen)
{
	SOCKET *sock=rt_socket_lookup(fd);
	struct msghdr msg;
	struct iovec iov;
	int error=0;

	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->recvmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;

	iov.iov_base=buf;
	iov.iov_len=len;
	msg.msg_name=from;
	msg.msg_namelen=*fromlen; //sizeof(struct sockaddr);
	msg.msg_iov=&iov;
	msg.msg_iovlen=1;
	msg.msg_control=NULL;
	msg.msg_controllen=0;
	msg.msg_flags=flags;

	// rt_printk("rufe protokoll\n");
	error = sock->ops->recvmsg(sock, &msg, len);

	if ( (error>=0) && (*fromlen!=0) )
		*fromlen=msg.msg_namelen;

	// rt_printk("return %d\n", error);
	return error;
}

/***
 *	rt_socket_sendmsg
 */
int rt_socket_sendmsg(int fd, struct msghdr *msg, unsigned int flags)
{
	SOCKET *sock=rt_socket_lookup(fd);
	int len;
	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->sendmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	len=rt_iovec_len(msg->msg_iov,msg->msg_iovlen);
	return sock->ops->sendmsg(sock,msg,len);
}

/***
 *	rt_socket_recvmsg
 */
int rt_socket_recvmsg(int fd, struct msghdr *msg, unsigned int flags)
{
	SOCKET *sock=rt_socket_lookup(fd);
	int len;
	
	if ((NULL == sock) ||
	    (NULL == sock->ops) || /* This check shall be obsolete in the future! */
	    (NULL == sock->ops->recvmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;

	len=rt_iovec_len(msg->msg_iov,msg->msg_iovlen);
	return sock->ops->recvmsg(sock,msg,len);
}

/***
 *	rt_sock_getsockname
 */
int rt_socket_getsockname(int fd, struct sockaddr *addr, int addr_len)
{
	SOCKET *sock=rt_socket_lookup(fd);
	struct sockaddr_in *usin = (struct sockaddr_in *)addr;

	usin->sin_family=sock->family;
	usin->sin_addr.s_addr=sock->saddr;
	usin->sin_port=sock->sport;	

	return ( sizeof(struct sockaddr_in) );
}

/***
 *	rt_socket_callback
 */
int rt_socket_callback(int fd, int (*func)(int,void *), void *arg)
{
	SOCKET *sock=rt_socket_lookup(fd);
	sock->private=arg;
	sock->wakeup=func;
	return 0;
}



/************************************************************************
 *	static socket interface						*
 ************************************************************************/

/***
 *	rt_ssocket
 */
int rt_ssocket(SOCKET* socket, int family, int type, int protocol)
{
	if (!socket)
		return -ENOTSOCK;
	else {
		unsigned char hash;

		/* protol family (PF_INET) and adress family (AF_INET) only */
		if ( (family!=AF_INET) )
			return -EAFNOSUPPORT;

		/* only datagram-sockets */
		if ( type!=SOCK_DGRAM ) 
			return -EAFNOSUPPORT;

		/* create new file descriptor */
		socket->fd=new_rtsocket_fd();

		/* default UDP-Protocol */
		if (!protocol)
			hash = rt_inet_hashkey(IPPROTO_UDP);
		else 
			hash = rt_inet_hashkey(protocol);

		/* create the socket (call the socket creator) */
		if  ( (rt_inet_protocols[hash]) && (rt_inet_protocols[hash]->socket(socket)>0) )
			return 0;
		else {
			rt_printk("RTnet: protocol with id %d not found\n", protocol);
			rt_socket_release(socket);
			return -ENOPROTOOPT;
		}
	}
}

/***
 *	rt_ssocket_bind 
 */
int rt_ssocket_bind(SOCKET *socket, struct sockaddr *addr, int addr_len)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->bind)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( socket->ops->bind(socket, addr, addr_len) );
}

/***
 *	rt_ssocket_listen
 */
int rt_ssocket_listen(SOCKET *socket, int backlog)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->listen)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( socket->ops->listen(socket, backlog) );
}

/***
 *	rt_ssocket_connect
 */
int rt_ssocket_connect(SOCKET *socket, struct sockaddr *addr, int addr_len)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->connect)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( socket->ops->connect(socket, addr, addr_len) );
}

/***
 *	rt_ssocket_accept
 */
int rt_ssocket_accept(SOCKET *socket, struct sockaddr *addr, int *addr_len)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->accept)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	return ( socket->ops->accept(socket, addr, addr_len) );
}

/***
 *	rt_ssocket_close
 */
int rt_ssocket_close(SOCKET *socket)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->close)) /* There should be no socket without ops! */
		return -ENOTSOCK;

	socket->ops->close(socket, 0);
	return 0;
}

/***
 *	rt_ssocket_writev
 */
int rt_ssocket_writev(SOCKET *socket, struct iovec *iov, size_t count)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->sendmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	else {
		struct msghdr msg;

		msg.msg_name=NULL;
		msg.msg_namelen=0;
		msg.msg_iov=iov;
		msg.msg_iovlen=1;
		msg.msg_control=NULL;
		msg.msg_controllen=0;
		msg.msg_flags=0;

		return socket->ops->sendmsg(socket, &msg, count);
	}
}

/***
 *	rt_ssocket_send
 */
int rt_ssocket_send(SOCKET *socket, void *buf, int len, unsigned int flags)
{
	return rt_ssocket_sendto(socket, buf, len, flags, NULL, 0);
}

/***
 *	rt_ssocket_sendto
 */
int rt_ssocket_sendto(SOCKET *socket, void *buf, int len, unsigned int flags, struct sockaddr *to, int tolen)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->sendmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	else {
		struct msghdr msg;
		struct iovec iov;

		iov.iov_base=(void *)buf;
		iov.iov_len=len;
	
		msg.msg_name=to;
		msg.msg_namelen=tolen;
		msg.msg_iov=&iov;
		msg.msg_iovlen=1;
		msg.msg_control=NULL;
		msg.msg_controllen=0;
		msg.msg_flags=flags;

		return socket->ops->sendmsg(socket, &msg, len);
	}
}

/***
 *	rt_socket_sendmsg
 */
int rt_ssocket_sendmsg(SOCKET *socket, struct msghdr *msg, unsigned int flags)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->sendmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	else {
		int len=rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
		return socket->ops->sendmsg(socket, msg, len);
	}
}

/***
 *	rt_ssocket_readv
 */
int rt_ssocket_readv(SOCKET *socket, struct iovec *iov, size_t count)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->recvmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	else {
		struct msghdr msg;
		msg.msg_name=NULL;
		msg.msg_namelen=0; 
		msg.msg_iov=iov;
		msg.msg_iovlen=1;
		msg.msg_control=NULL;
		msg.msg_controllen=0;
		msg.msg_flags=0;
		return socket->ops->recvmsg(socket, &msg, count);
	}
}

/***
 *	rt_ssocket_recv
 */
int rt_ssocket_recv(SOCKET *socket, void *buf, int len, unsigned int flags)
{
	return rt_ssocket_recvfrom(socket, buf, len, flags, NULL, 0);
}

/***
 *	rt_ssocket_recfrom
 */
int rt_ssocket_recvfrom(SOCKET *socket, void *buf, int len, unsigned int flags, struct sockaddr *from, int fromlen)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->bind)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	else {
		struct msghdr msg;
		struct iovec iov;
		int error=0;

		iov.iov_base=buf;
		iov.iov_len=len;
		msg.msg_name=from;
		msg.msg_namelen=fromlen; //sizeof(struct sockaddr);
		msg.msg_iov=&iov;
		msg.msg_iovlen=1;
		msg.msg_control=NULL;
		msg.msg_controllen=0;
		msg.msg_flags=flags;
		error = socket->ops->recvmsg(socket, &msg, len);
		if ( (error>=0) && (fromlen!=0) )
			fromlen=msg.msg_namelen;
		return error;	
	}
}

/***
 *	rt_ssocket_recvmsg
 */
int rt_ssocket_recvmsg(SOCKET *socket, struct msghdr *msg, unsigned int flags)
{
	if ((NULL == socket) ||
	    (NULL == socket->ops) || /* This check shall be obsolete in the future! */
	    (NULL == socket->ops->recvmsg)) /* There should be no socket without ops! */
		return -ENOTSOCK;
	else {
		int len=rt_iovec_len(msg->msg_iov,msg->msg_iovlen);
		return socket->ops->recvmsg(socket, msg, len);
	}
}

/***
 *	rt_ssocket_getsocketname
 */
int rt_ssocket_getsockname(SOCKET *socket, struct sockaddr *addr, int addr_len)
{
	if ( !socket )
		return -ENOTSOCK;
	else {
		struct sockaddr_in *usin = (struct sockaddr_in *)addr;
		usin->sin_family=socket->family;
		usin->sin_addr.s_addr=socket->saddr;
		usin->sin_port=socket->sport;	

		return ( sizeof(struct sockaddr_in) );
	}
}

/***
 *	rt_ssocket_callback
 */
int rt_ssocket_callback(SOCKET *socket, int (*func)(int,void *), void *arg)
{
	if ( !socket )
		return -ENOTSOCK;
	else {
		socket->private=arg;
		socket->wakeup=func;

		return 0;
	}
}


/************************************************************************
 *	initialisation of rt-socket interface				*
 ************************************************************************/
/***
 *	rtsocket_init
 */
void rtsockets_init(void)
{
	int i;

	spin_lock_init(&socket_base_lock);

	/* initialise the first socket */ 	
	rt_sockets[0].prev=NULL;
	rt_sockets[0].next=&rt_sockets[1];
	rt_sockets[0].state=TCP_CLOSE;
	rtskb_queue_head_init(&rt_sockets[0].incoming);

	/* initialise the last socket */ 	
	rt_sockets[RT_SOCKETS-1].prev=&rt_sockets[RT_SOCKETS-2];
	rt_sockets[RT_SOCKETS-1].next=NULL;
	rt_sockets[RT_SOCKETS-1].state=TCP_CLOSE;
	rtskb_queue_head_init(&rt_sockets[RT_SOCKETS-1].incoming);

	for (i=1; i<RT_SOCKETS-1; i++) {
		rt_sockets[i].next=&rt_sockets[i+1];
		rt_sockets[i].prev=&rt_sockets[i-1];
		rt_sockets[i].state=TCP_CLOSE;
		rtskb_queue_head_init(&rt_sockets[i].incoming);
	}
	free_rtsockets=&rt_sockets[0];
}

/***
 *	rtsocket_release
 */
void rtsockets_release(void)
{
	free_rtsockets=NULL;
}
