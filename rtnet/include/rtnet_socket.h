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


#define RT_SOCKETS      64

struct rtsocket_ops {
    int  (*bind)        (struct rtsocket *s, struct sockaddr *my_addr,
                         int addr_len);
    int  (*connect)     (struct rtsocket *s, struct sockaddr *serv_addr,
                         int addr_len);
    int  (*listen)      (struct rtsocket *s, int backlog);
    int  (*accept)      (struct rtsocket *s, struct sockaddr *client_addr,
                         int *addr_len);
    int  (*recvmsg)     (struct rtsocket *s, struct msghdr *msg, int len);
    int  (*sendmsg)     (struct rtsocket *s, const struct msghdr *msg, int len);
    void (*close)       (struct rtsocket *s, long timeout);
    int  (*setsockopt)  (struct rtsocket *s, int level, int optname,
                         const void *optval, int optlen);
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

    unsigned char       connected;  /* connect any socket!  */

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
