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

#define LxRTNET_IDX             7

#define RT_SOCKET               0
#define RT_SOCKET_CLOSE         1
#define RT_SOCKET_BIND          2
#define RT_SOCKET_CONNECT       3
#define RT_SOCKET_ACCEPT        4
#define RT_SOCKET_LISTEN        5
#define RT_SOCKET_SEND          6
#define RT_SOCKET_RECV          7
#define RT_SOCKET_SENDTO        8
#define RT_SOCKET_RECVFROM      9
#define RT_SOCKET_SENDMSG       10
#define RT_SOCKET_RECVMSG       11
#define RT_SOCKET_WRITE         12
#define RT_SOCKET_READ          13
#define RT_SOCKET_WRITEV        14
#define RT_SOCKET_READV         15
#define RT_SOCKET_GETSOCKNAME   16
#define RT_SOCKET_SETSOCKOPT    17
#define RT_SOCKET_IOCTL         18

/* discontinued */
/*#define RT_SSOCKET              40
#define RT_SSOCKET_CLOSE        41
#define RT_SSOCKET_BIND         42
#define RT_SSOCKET_CONNECT      43
#define RT_SSOCKET_ACCEPT       44
#define RT_SSOCKET_LISTEN       45
#define RT_SSOCKET_SEND         46
#define RT_SSOCKET_RECV         47
#define RT_SSOCKET_SENDTO       48
#define RT_SSOCKET_RECVFROM     49
#define RT_SSOCKET_SENDMSG      50
#define RT_SSOCKET_RECVMSG      51
#define RT_SSOCKET_WRITE        52
#define RT_SSOCKET_READ         53
#define RT_SSOCKET_WRITEV       54
#define RT_SSOCKET_READV        55
#define RT_SSOCKET_GETSOCKNAME  56
#define RT_SSOCKET_SETSOCKOPT   57*/

#ifndef __KERNEL__

#include <rtnet_config.h>

#ifdef HAVE_RTAI_DECLARE_H
#include <rtai_declare.h>
#endif

#ifdef HAVE_RTAI_LXRT_USER_H
#include <rtai_lxrt_user.h>
#endif

#include <rtai_lxrt.h>


#define SIZARG sizeof(arg)

#ifdef CONFIG_RTAI_24
#define RTAI_PROTO(type,name,arglist)   DECLARE type name arglist
#endif


RTAI_PROTO(int, rt_socket, (int family, int type, int protocol))
{
    struct { int family; int type; int protocol; } arg = { family, type, protocol };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_close, (int fd))
{
    struct { int fd; } arg = { fd };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_CLOSE, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_bind, (int fd, struct sockaddr *addr, socklen_t addrlen))
{
    struct {int fd; struct sockaddr *addr; socklen_t addrlen; } arg = { fd, addr, addrlen };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_BIND, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_connect, (int fd, const struct sockaddr *addr, socklen_t addrlen))
{
    struct {int fd; const struct sockaddr *addr; socklen_t addrlen; } arg = { fd, addr, addrlen };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_CONNECT, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_accept, (int fd, struct sockaddr *addr, socklen_t *addrlen))
{
    struct {int fd; struct sockaddr *addr; socklen_t *addrlen; } arg = { fd, addr, addrlen };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_ACCEPT, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_listen, (int fd, int backlog))
{
    struct {int fd; int backlog; } arg = { fd, backlog };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_LISTEN, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_send, (int fd, const void *buf, size_t len, int flags))
{
    struct {int fd; const void *buf; size_t len; int flags;} arg = { fd, buf, len, flags };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_SEND, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_recv, (int fd, void *buf, size_t len, int flags))
{
    struct {int fd; void *buf; size_t len; int flags;} arg = { fd, buf, len, flags };
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_RECV, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_sendto, (int fd, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen))
{
    struct {int fd; const void *buf; size_t len; int flags; const struct sockaddr *to; socklen_t tolen;} arg
        = {fd, buf, len, flags, to, tolen};
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_SENDTO, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_recvfrom, (int fd, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen))
{
    struct {int fd; void *buf; size_t len; int flags; struct sockaddr *from; socklen_t *fromlen;} arg
        = {fd, buf, len, flags, from, fromlen};
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_RECVFROM, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_sendmsg, (int fd, const struct msghdr *msg, int flags))
{
    struct {int fd; const struct msghdr *msg; int flags;} arg = {fd, msg, flags};
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_SENDMSG, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_recvmsg, (int fd, struct msghdr *msg, int flags))
{
    struct {int fd; struct msghdr *msg; int flags;} arg = {fd, msg, flags};
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_RECVMSG, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_getsockname, (int fd, struct sockaddr *addr, socklen_t addrlen))
{
    struct {int fd; struct sockaddr *addr; socklen_t addrlen;} arg = {fd, addr, addrlen};
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_GETSOCKNAME, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_setsockopt, (int fd, int level, int optname, const void *optval, socklen_t optlen))
{
    struct {int fd; int level; int optname; const void *optval; socklen_t optlen;} arg = {fd, level, optname, optval, optlen};
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_SETSOCKOPT, &arg).i[LOW];
}

RTAI_PROTO(int, rt_socket_ioctl, (int fd, int request, void *ioctl_arg))
{
    struct {int fd; int request; void *ioctl_arg;} arg = {fd, request, ioctl_arg};
    return (int)rtai_lxrt(LxRTNET_IDX, SIZARG, RT_SOCKET_IOCTL, &arg).i[LOW];
}


#define rt_bind                 rt_socket_bind
#define rt_listen               rt_socket_listen
#define rt_connect              rt_socket_connect
#define rt_accept               rt_socket_accept
#define rt_close                rt_socket_close
#define rt_sendto               rt_socket_sendto
#define rt_recvfrom             rt_socket_recvfrom
#define rt_setsockopt           rt_socket_setsockopt
#define rt_ioctl                rt_socket_ioctl


#endif	/* __KERNEL__ */
#endif	/* __LX_RTNET_H_ */
