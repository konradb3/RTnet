/* rtnet_lxrt.h
 *
 * rtnet_lxrt - real-time networking in usermode
 * 
 * Copyright (C) 2002 Ulrich Marx <marx@fet.uni-hannover.de>
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
#ifndef __LX_RTNET_H__
#define __LX_RTNET_H__

#include <rtai_declare.h>

#define LxRTNET_IDX		 7

#define RT_SOCKET		 0
#define RT_SOCKET_CLOSE		 1
#define RT_SOCKET_BIND		 2
#define RT_SOCKET_CONNECT	 3
#define RT_SOCKET_ACCEPT	 4
#define RT_SOCKET_LISTEN	 5
#define RT_SOCKET_SEND		 6
#define RT_SOCKET_RECV		 7
#define RT_SOCKET_SENDTO	 8
#define RT_SOCKET_RECVFROM	 9
#define RT_SOCKET_SENDMSG	10
#define RT_SOCKET_RECVMSG	11
#define RT_SOCKET_WRITE		12
#define RT_SOCKET_READ		13
#define RT_SOCKET_WRITEV	14
#define RT_SOCKET_READV		15
#define RT_SOCKET_GETSOCKNAME	16
#define RT_SOCKET_CALLBACK	17

#define RT_SSOCKET		18
#define RT_SSOCKET_CLOSE	19
#define RT_SSOCKET_BIND		20
#define RT_SSOCKET_CONNECT	21
#define RT_SSOCKET_ACCEPT	22
#define RT_SSOCKET_LISTEN	23
#define RT_SSOCKET_SEND		24
#define RT_SSOCKET_RECV		25
#define RT_SSOCKET_SENDTO	26
#define RT_SSOCKET_RECVFROM	27
#define RT_SSOCKET_SENDMSG	28
#define RT_SSOCKET_RECVMSG	29
#define RT_SSOCKET_WRITE	30
#define RT_SSOCKET_READ		31
#define RT_SSOCKET_WRITEV	32
#define RT_SSOCKET_READV	33
#define RT_SSOCKET_GETSOCKNAME	34
#define RT_SSOCKET_CALLBACK	35


extern int rt_socket_init(int family, int type, int protocol);
extern int rt_socket_close(int fd);
extern int rt_socket_bind(int fd, struct sockaddr *addr, int addr_len);
extern int rt_socket_connect(int fd, struct sockaddr *addr, int addr_len);
extern int rt_socket_accept(int fd, struct sockaddr *addr, int *addrlen);
extern int rt_socket_listen(int fd, int backlog);
extern int rt_socket_send(int fd, void *buf, int len, unsigned int flags); 
extern int rt_socket_recv(int fd,void *buf,int len, unsigned int flags);
extern int rt_socket_sendto(int fd, void *buf, int len, unsigned int flags, struct sockaddr *to, int tolen);
extern int rt_socket_recvfrom(int fd, void *buf, int len, unsigned int flags,  struct sockaddr *from, int *fromlen);
extern int rt_socket_sendmsg(int fd, struct msghdr *msg, unsigned int flags);
extern int rt_socket_recvmsg(int fd, struct msghdr *msg, unsigned int flags);
extern int rt_socket_getsockname(int fd, struct sockaddr *addr, int addr_len);

#ifndef __KERNEL__

#include <stdarg.h>
#include <rtai_lxrt.h>

extern union rtai_lxrt_t rtai_lxrt(short int dynx, short int lsize, int srq, void *arg);

#define SIZARG sizeof(arg)

DECLARE int rt_socket(int family, int type, int protocol) 
{
	struct { int family; int type; int protocol; } arg = { family, type, protocol };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET, &arg).i[LOW];
}

DECLARE int rt_socket_close(int fd) 
{
	struct { int fd; } arg = { fd };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_CLOSE, &arg).i[LOW];
}

DECLARE int rt_socket_bind(int fd, struct sockaddr *addr, int addr_len)
{
	struct {int fd; struct sockaddr *addr; int addr_len; } arg = { fd, addr, addr_len };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_BIND, &arg).i[LOW];
}

DECLARE int rt_socket_connect(int fd, struct sockaddr *addr, int addr_len)
{
	struct {int fd; struct sockaddr *addr; int addr_len; } arg = { fd, addr, addr_len };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_CONNECT, &arg).i[LOW];
}

DECLARE int rt_socket_accept(int fd, struct sockaddr *addr, int *addr_len)
{
	struct {int fd; struct sockaddr *addr; int *addr_len; } arg = { fd, addr, addr_len };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_ACCEPT, &arg).i[LOW];
}

DECLARE int rt_socket_listen(int fd, int backlog)
{
	struct {int fd; int backlog; } arg = { fd, backlog };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_LISTEN, &arg).i[LOW];
}

DECLARE int rt_socket_send(int fd, void *buf, int len, unsigned int flags)
{
	struct {int fd; void *buf; int len; unsigned int flags;} arg = { fd, buf, len, flags };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_SEND, &arg).i[LOW];
}

DECLARE int rt_socket_recv(int fd, void *buf, int len, unsigned int flags)
{
	struct {int fd; void *buf; int len; unsigned int flags;} arg = { fd, buf, len, flags };
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_RECV, &arg).i[LOW];
}

DECLARE int rt_socket_sendto(int fd, void *buf, int len, unsigned int flags, struct sockaddr *to, int tolen)
{
	struct {int fd; void *buf; int len; unsigned int flags; struct sockaddr *to; int tolen;} arg 
		= {fd, buf, len, flags, to, tolen};
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_SENDTO, &arg).i[LOW];	
}

DECLARE int rt_socket_recvfrom(int fd, void *buf, int len, unsigned int flags, struct sockaddr *from, int *fromlen)
{
	fd = fd; buf=buf; len=len; flags=flags; from=from, fromlen=fromlen;
#warning function not yet implemented!
	return 0;
}

DECLARE int rt_socket_sendmsg(int fd, struct msghdr *msg, unsigned int flags)
{
	struct {int fd; struct msghdr *msg; unsigned int flags;} arg = {fd, msg, flags};
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_SENDMSG, &arg).i[LOW];	
}

DECLARE int rt_socket_recvmsg(int fd, struct msghdr *msg, unsigned int flags) 
{
	fd=fd; msg=msg; flags=flags;
#warning function not yet implemented!
	return 0;
}

DECLARE int rt_socket_getsockname(int fd, struct sockaddr *addr, int addr_len)
{
	struct {int fd; struct sockaddr *addr; int addr_len;} arg = {fd, addr, addr_len};
	return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_GETSOCKNAME, &arg).i[LOW];
} 

#endif	/* __KERNEL__ */
#endif	/* __LX_RTNET_H_ */
