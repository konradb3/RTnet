/* ipv4/protocol.h
 *
 * RTnet - real-time networking subsystem
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
#ifndef __RTNET_PROTOCOL_H_
#define __RTNET_PROTOCOL_H_

#include <rtnet.h>
#include <rtskb.h>


#define MAX_RT_INET_PROTOCOLS   32

/***
 * transport layer protocol
 */
struct rtinet_protocol {
    char                *name;
    unsigned short      protocol;

    struct rtskb_queue  *(*get_pool)(struct rtskb *);
    int                 (*rcv_handler)(struct rtskb *);
    void                (*err_handler)(struct rtskb *);
    int                 (*init_socket)(struct rtsocket *sock);
};


extern struct rtinet_protocol *rt_inet_protocols[];

#define rt_inet_hashkey(id)  (id & (MAX_RT_INET_PROTOCOLS-1))
extern void rt_inet_add_protocol(struct rtinet_protocol *prot);
extern void rt_inet_del_protocol(struct rtinet_protocol *prot);
extern struct rtinet_protocol *rt_inet_get_protocol(int protocol);


#endif  /* __RTNET_PROTOCOL_H_ */
