/* ipv4/icmp.h
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
#ifndef __RTNET_ICMP_H_
#define __RTNET_ICMP_H_

#include <ipv4/protocol.h>


#define ICMP_REPLY_POOL_SIZE        8

extern struct rtinet_protocol icmp_protocol;
extern void rt_icmp_init(void);
extern void rt_icmp_release(void);


#endif  /* __RTNET_ICMP_H_ */
