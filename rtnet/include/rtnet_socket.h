/* rtnet_socket.h
 *
 * RTnet - real-time networking subsystem
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
 */
#ifndef __RTNET_SOCKET_H_
#define __RTNET_SOCKET_H_

#ifdef __KERNEL__

#include <linux/socket.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtdev.h>
#include <rtnet.h>


#define RT_SOCKETS          64

#define RT_SOCK_NONBLOCK    0x0001


struct rtsocket_ops {
    int  (*bind)        (struct rtsocket *s, struct sockaddr *my_addr,
                         socklen_t addrlen);
    int  (*connect)     (struct rtsocket *s, const struct sockaddr *serv_addr,
                         socklen_t addrlen);
    int  (*listen)      (struct rtsocket *s, int backlog);
    int  (*accept)      (struct rtsocket *s, struct sockaddr *addr,
                         socklen_t *addrlen);
    int  (*recvmsg)     (struct rtsocket *s, struct msghdr *msg,
                         size_t total_len, int flags);
    int  (*sendmsg)     (struct rtsocket *s, const struct msghdr *msg,
                         size_t total_len, int flags);
    void (*close)       (struct rtsocket *s, long timeout);
    int  (*setsockopt)  (struct rtsocket *s, int level, int optname,
                         const void *optval, socklen_t optlen);
};

struct rtsocket {
    struct rtsocket     *prev;
    struct rtsocket     *next;      /* next socket in list */

    int                 fd;         /* file descriptor */

    unsigned short      family;
    unsigned short      typ;
    unsigned short      protocol;

    unsigned char       state;

    struct rtsocket_ops *ops;

    struct rtskb_head   skb_pool;
    struct rtskb_head   incoming;

    unsigned int        flags;      /* see RT_SOCK_xxx defines  */
    RTIME               timeout;    /* receive timeout, 0 for infinite */

    u32                 saddr;      /* source ip-addr */
    u16                 sport;      /* source port */

    u32                 daddr;      /* destination ip-addr */
    u16                 dport;      /* destination port */

    int                 (*wakeup)(int s,void *arg); /* socket wakeup-func */
    SEM                 wakeup_sem; /* for blocking calls */

    void                *private;

    unsigned int        priority;
    u8                  tos;
};

extern void rtsockets_init(void);
extern void rtsockets_release(void);

extern SOCKET *rt_socket_alloc(void);
extern void rt_socket_release(SOCKET *sock);


#endif  /* __KERNEL__ */

#endif  /* __RTNET_SOCKET_H_ */
