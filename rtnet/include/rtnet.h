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


/* socket option names */
#define RT_SO_EXTPOOL   0xFF01
#define RT_SO_SHRPOOL   0xFF02
#define RT_SO_PRIORITY  0xFF03
#define RT_SO_NONBLOCK  0xFF04
#define RT_SO_TIMEOUT   0xFF05

/* socket priorities */
#define SOCK_MAX_PRIO   QUEUE_MAX_PRIO
#define SOCK_DEF_PRIO   QUEUE_MAX_PRIO+(QUEUE_MIN_PRIO-QUEUE_MAX_PRIO+1)/2
#define SOCK_MIN_PRIO   QUEUE_MIN_PRIO-1
#define SOCK_NRT_PRIO   QUEUE_MIN_PRIO


typedef size_t socklen_t;


/***
 *  rtsocket
 */

struct rtsocket;
typedef struct rtsocket SOCKET;



/* the external interface */
extern struct rtsocket *rt_socket_lookup(int s);

/* file descriptor interface */
extern int rt_socket            (int family, int type, int protocol);
extern int rt_socket_bind       (int s, struct sockaddr *my_addr, socklen_t addrlen);
extern int rt_socket_listen     (int s, int backlog);
extern int rt_socket_connect    (int s, const struct sockaddr *serv_addr, socklen_t addrlen);
extern int rt_socket_accept     (int s, struct sockaddr *addr, socklen_t *addrlen);
extern int rt_socket_close      (int s);
extern int rt_socket_writev     (int s, const struct iovec *vector, int count);
extern int rt_socket_send       (int s, const void *msg, size_t len, int flags);
extern int rt_socket_sendto     (int s, const void *msg, size_t len, int flags,
                                 const struct sockaddr *to, socklen_t tolen);
extern int rt_socket_sendmsg    (int s, const struct msghdr *msg, int flags);
extern int rt_socket_readv      (int s, const struct iovec *vector, int count);
extern int rt_socket_recv       (int s, void *buf, size_t len, int flags);
extern int rt_socket_recvfrom   (int s, void *buf, size_t len, int flags,
                                 struct sockaddr *from, socklen_t *fromlen);
extern int rt_socket_recvmsg    (int s, struct msghdr *msg, int flags);
extern int rt_socket_getsockname(int s, struct sockaddr *addr, socklen_t addrlen);
extern int rt_socket_callback   (int s, int (*func)(int,void *), void *arg);
extern int rt_socket_setsockopt (int s, int level, int optname,
                                 const void *optval, socklen_t optlen);

#define rt_bind                 rt_socket_bind
#define rt_listen               rt_socket_listen
#define rt_connect              rt_socket_connect
#define rt_accept               rt_socket_accept
#define rt_close                rt_socket_close
#define rt_sendto               rt_socket_sendto
#define rt_recvfrom             rt_socket_recvfrom
#define rt_setsockopt           rt_socket_setsockopt

/* static interface (does anyone actually use it???) */
extern int rt_ssocket(SOCKET* socket, int family, int type, int protocol);
extern int rt_ssocket_bind(SOCKET *socket, struct sockaddr *my_addr, socklen_t addrlen);
extern int rt_ssocket_listen(SOCKET *socket, int backlog);
extern int rt_ssocket_connect(SOCKET *socket, const struct sockaddr *serv_addr, socklen_t addrlen);
extern int rt_ssocket_accept(SOCKET *socket, struct sockaddr *addr, socklen_t *addrlen);
extern int rt_ssocket_close(SOCKET *socket);
extern int rt_ssocket_writev(SOCKET *socket, const struct iovec *vector, int count);
extern int rt_ssocket_send(SOCKET *socket, const void *msg, size_t len, int flags);
extern int rt_ssocket_sendto(SOCKET *socket, const void *msg, size_t len, int flags,
                             const struct sockaddr *to, socklen_t tolen);
extern int rt_ssocket_sendmsg(SOCKET *socket, const struct msghdr *msg, int flags);
extern int rt_ssocket_readv(SOCKET *socket, const struct iovec *vector, int count);
extern int rt_ssocket_recv(SOCKET *socket, void *buf, size_t len, int flags);
extern int rt_ssocket_recvfrom(SOCKET *socket, void *buf, size_t len, int flags,
                               struct sockaddr *from, socklen_t fromlen);
extern int rt_ssocket_recvmsg(SOCKET *socket, struct msghdr *msg, int flags);
extern int rt_ssocket_getsockname(SOCKET *socket, struct sockaddr *addr, socklen_t addr_len);
extern int rt_ssocket_callback(SOCKET *socket, int (*func)(int,void *), void *arg);

/* utils */
extern unsigned long rt_inet_aton(const char *ip);


#endif  /* __KERNEL__ */

#endif  /* __RTNET_H_ */
