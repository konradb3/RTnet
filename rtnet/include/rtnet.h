/* rtnet.h
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
#ifndef __RTNET_H_
#define __RTNET_H_

#ifdef __KERNEL__

#include <linux/socket.h>


/***
 *	rtsocket
 */
 
struct rtsocket;
typedef struct rtsocket SOCKET;



/* the external interface */
extern struct rtsocket *rt_socket_lookup(int fd);

/* file descriptor interface */
extern int rt_socket		(int family, int type, int protocol);
extern int rt_socket_bind	(int fd, struct sockaddr *my_addr, int addr_len);
extern int rt_socket_listen	(int fd, int backlog);
extern int rt_socket_connect	(int fd, struct sockaddr *addr, int addr_len);
extern int rt_socket_accept	(int fd, struct sockaddr *addr, int *addrlen);
extern int rt_socket_close	(int fd);
extern int rt_socket_writev	(int fd, struct iovec *vector, size_t count);
extern int rt_socket_send	(int fd, void *buf, int len, unsigned int flags);
extern int rt_socket_sendto	(int fd, void *buf, int len, unsigned int flags, 
				struct sockaddr *to, int tolen);
extern int rt_socket_sendmsg	(int fd, struct msghdr *msg, unsigned int flags);
extern int rt_socket_readv	(int fd, struct iovec *vector, size_t count);
extern int rt_socket_recv	(int fd, void *buf, int len, unsigned int flags);
extern int rt_socket_recvfrom	(int fd, void *buf, int len, unsigned int flags, 
				struct sockaddr *from, int *fromlen);
extern int rt_socket_recvmsg	(int s, struct msghdr *msg, unsigned int flags);
extern int rt_socket_getsockname(int fd, struct sockaddr *addr, int addr_len);
extern int rt_socket_callback	(int fd, int (*func)(int,void *), void *arg);

extern int rt_ip_setsockopt (int fd, int optname, char *optval, int optlen);

#define rt_bind		rt_socket_bind
#define rt_listen	rt_socket_listen
#define rt_connect	rt_socket_connect
#define rt_accept	rt_socket_accept
#define rt_close	rt_socket_close
#define rt_sendto	rt_socket_sendto
#define rt_recvfrom	rt_socket_recvfrom
#define rt_setsockopt	rt_ip_setsockopt

/* static interface */
extern int rt_ssocket(SOCKET* socket, int family, int type, int protocol);
extern int rt_ssocket_bind(SOCKET *socket, struct sockaddr *my_addr, int addr_len);
extern int rt_ssocket_listen(SOCKET *socket, int backlog);
extern int rt_ssocket_connect(SOCKET *socket, struct sockaddr *addr, int addr_len);
extern int rt_ssocket_accept(SOCKET *socket, struct sockaddr *addr, int *addrlen);
extern int rt_ssocket_close(SOCKET *socket);
extern int rt_ssocket_writev(SOCKET *socket, struct iovec *vector, size_t count);
extern int rt_ssocket_send(SOCKET *socket, void *buf, int len, unsigned int flags);
extern int rt_ssocket_sendto(SOCKET *socket, void *buf, int len, unsigned int flags, 
				struct sockaddr *to, int tolen);
extern int rt_ssocket_sendmsg(SOCKET *socket, struct msghdr *msg, unsigned int flags);
extern int rt_ssocket_readv(SOCKET *socket, struct iovec *vector, size_t count);
extern int rt_ssocket_recv(SOCKET *socket, void *buf, int len, unsigned int flags);
extern int rt_ssocket_recvfrom(SOCKET *socket, void *buf, int len, unsigned int flags, 
				struct sockaddr *from, int fromlen);
extern int rt_ssocket_recvmsg(SOCKET *socket, struct msghdr *msg, unsigned int flags);
extern int rt_ssocket_getsockname(SOCKET *socket, struct sockaddr *addr, int addr_len);
extern int rt_ssocket_callback(SOCKET *socket, int (*func)(int,void *), void *arg);

/* utils */
extern unsigned long rt_inet_aton(const char *ip);


#endif  /* __KERNEL__ */

#endif  /* __RTNET_H_ */
