/***
 *
 *  include/ipv4/udp.h
 *
 *  RTnet - real-time networking subsystem
 *  Copyright (C) 1999, 2000 Zentropic Computing, LLC
 *                2002       Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2004, 2005 Jan Kiszka <jan.kiszka@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __RTNET_UDP_H_
#define __RTNET_UDP_H_

#include <linux/init.h>

#include <ipv4/protocol.h>


/* Maximum number of active udp sockets
   Only increase with care (look-up delays!), must be power of 2 */
#define RT_UDP_SOCKETS      64


extern struct rtinet_protocol udp_protocol;


extern int rt_udp_close(struct rtdm_dev_context *context,
                        rtdm_user_info_t *user_info);
extern int rt_udp_ioctl(struct rtdm_dev_context *context,
                        rtdm_user_info_t *user_info,
                        int request, void *arg);
extern ssize_t rt_udp_recvmsg(struct rtdm_dev_context *context,
                              rtdm_user_info_t *user_info,
                              struct msghdr *msg, int flags);
extern ssize_t rt_udp_sendmsg(struct rtdm_dev_context *context,
                              rtdm_user_info_t *user_info,
                              const struct msghdr *msg, int flags);
#ifdef CONFIG_RTNET_RTDM_SELECT
extern unsigned int rt_udp_poll(struct rtdm_dev_context *context); /* , poll_table *wait) */
extern ssize_t rt_udp_pollwait(struct rtdm_dev_context *context, wait_queue_primitive_t *sem);
extern ssize_t rt_udp_pollfree(struct rtdm_dev_context *context);
#endif /* CONFIG_RTNET_RTDM_SELECT */

extern void __init rt_udp_init(void);
extern void rt_udp_release(void);


#endif  /* __RTNET_UDP_H_ */
