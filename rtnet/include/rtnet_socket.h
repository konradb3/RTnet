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

#include <linux/init.h>
#include <linux/socket.h>

#include <rtdev.h>
#include <rtnet.h>
#include <rtnet_sys.h>
#include <stack_mgr.h>


#define RT_SOCKETS          64  /* only increase with care (lockup delays!),
                                 * must not be greater then 255 */
#define RT_SOCK_NONBLOCK    0x0001


struct rtsocket_ops {
    int  (*bind)        (struct rtsocket *s, struct sockaddr *my_addr,
                         socklen_t addrlen);
    int  (*connect)     (struct rtsocket *s, const struct sockaddr *serv_addr,
                         socklen_t addrlen);
    int  (*getsockname) (struct rtsocket *s, struct sockaddr *addr,
                         socklen_t *addrlen);
    int  (*listen)      (struct rtsocket *s, int backlog);
    int  (*accept)      (struct rtsocket *s, struct sockaddr *addr,
                         socklen_t *addrlen);
    int  (*recvmsg)     (struct rtsocket *s, struct msghdr *msg,
                         size_t total_len, int flags);
    int  (*sendmsg)     (struct rtsocket *s, const struct msghdr *msg,
                         size_t total_len, int flags);
    int  (*close)       (struct rtsocket *s);
    int  (*setsockopt)  (struct rtsocket *s, int level, int optname,
                         const void *optval, socklen_t optlen);
};

struct rtsocket {
    int                 fd;         /* file descriptor
                                     * bit 0-7: index in rt_sockets
                                     * bit 8-30: instance id */
    struct rtsocket     *prev;      /* previous socket in list */
    struct rtsocket     *next;      /* next socket in list */
    atomic_t            refcount;

    unsigned short      family;
    unsigned short      type;
    unsigned short      protocol;

    unsigned char       state;

    struct rtsocket_ops *ops;

    struct rtskb_queue  skb_pool;
    int                 rt_pool;
    unsigned int        pool_size;

    struct rtskb_queue  incoming;

    unsigned int        priority;
    unsigned int        flags;      /* see RT_SOCK_xxx defines  */
    rtos_time_t         timeout;    /* receive timeout, 0 for infinite */

    int                 (*wakeup)(int s,void *arg); /* callback function */
    void                *wakeup_arg; /* argument of callback function */
    rtos_event_t        wakeup_event; /* for blocking calls */

    union {
        /* IP specific */
        struct {
            u32         saddr;      /* source ip-addr */
            u16         sport;      /* source port */
            u32         daddr;      /* destination ip-addr */
            u16         dport;      /* destination port */
            u8          tos;
        } inet;

        /* packet socket specific */
        struct {
            struct rtpacket_type packet_type;
            int                  ifindex;
        } packet;
    } prot;
};

#define rt_socket_reference(sock)   atomic_inc(&(sock)->refcount)
#define rt_socket_dereference(sock) atomic_dec(&(sock)->refcount)

extern void __init rtsockets_init(void);


#endif  /* __KERNEL__ */

#endif  /* __RTNET_SOCKET_H_ */
