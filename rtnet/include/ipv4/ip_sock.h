/* ipv4/ip_sock.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 2003 Jan Kiszka <jan.kiszka@web.de>
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
#ifndef __RTNET_IP_SOCK_H_
#define __RTNET_IP_SOCK_H_


extern int rt_ip_setsockopt(struct rtsocket *s, int level, int optname,
                            const void *optval, socklen_t optlen);
extern int rt_ip_getsockname(struct rtsocket *s, struct sockaddr *addr,
                             socklen_t *addrlen);


#endif  /* __RTNET_IP_SOCK_H_ */
