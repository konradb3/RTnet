/***
 *
 *  rtnet.h
 *
 *  RTnet - real-time networking subsystem
 *  Copyright (C) 1999, 2000 Zentropic Computing, LLC
 *                2002       Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2003-2005  Jan Kiszka <jan.kiszka@web.de>
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

#ifndef __RTNET_H_
#define __RTNET_H_

#include <rtdm/rtdm.h>


struct rtdm_dev_context;

struct rtnet_callback {
    void    (*func)(struct rtdm_dev_context *, void *);
    void    *arg;
};


/* sub-classes: RTDM_CLASS_NETWORK */
#define RTDM_SUBCLASS_RTNET     0

#define RTIOC_TYPE_NETWORK      RTDM_CLASS_NETWORK

/* RTnet-specific IOCTLs */
#define RTNET_RTIOC_XMITPARAMS  _IOW(RTIOC_TYPE_NETWORK, 0x10, unsigned int)
#define RTNET_RTIOC_PRIORITY    RTNET_RTIOC_XMITPARAMS  /* legacy */
#define RTNET_RTIOC_TIMEOUT     _IOW(RTIOC_TYPE_NETWORK, 0x11, int64_t)
#define RTNET_RTIOC_CALLBACK    _IOW(RTIOC_TYPE_NETWORK, 0x12, \
                                     struct rtnet_callback)
#define RTNET_RTIOC_NONBLOCK    _IOW(RTIOC_TYPE_NETWORK, 0x13, unsigned int)
#define RTNET_RTIOC_EXTPOOL     _IOW(RTIOC_TYPE_NETWORK, 0x14, unsigned int)
#define RTNET_RTIOC_SHRPOOL     _IOW(RTIOC_TYPE_NETWORK, 0x15, unsigned int)

/* socket transmission priorities */
#define SOCK_MAX_PRIO           0
#define SOCK_DEF_PRIO           SOCK_MAX_PRIO + \
                                    (SOCK_MIN_PRIO-SOCK_MAX_PRIO+1)/2
#define SOCK_MIN_PRIO           SOCK_NRT_PRIO - 1
#define SOCK_NRT_PRIO           31

/* socket transmission channels */
#define SOCK_DEF_RT_CHANNEL     0           /* default rt xmit channel     */
#define SOCK_DEF_NRT_CHANNEL    1           /* default non-rt xmit channel */
#define SOCK_USER_CHANNEL       2           /* first user-defined channel  */

/* argument construction for RTNET_RTIOC_XMITPARAMS */
#define SOCK_XMIT_PARAMS(priority, channel) ((priority) | ((channel) << 16))


/* function name wrappers */
#define socket_rt               rt_dev_socket
#define open_rt                 rt_dev_open
#define close_rt                rt_dev_close
#define ioctl_rt                rt_dev_ioctl
#define bind_rt                 rt_dev_bind
#define connect_rt              rt_dev_connect
#define listen_rt               rt_dev_listen
#define accept_rt               rt_dev_accept
#define recv_rt                 rt_dev_recv
#define recvfrom_rt             rt_dev_recvfrom
#define recvmsg_rt              rt_dev_recvmsg
#define send_rt                 rt_dev_send
#define sendto_rt               rt_dev_sendto
#define sendmsg_rt              rt_dev_sendmsg
#define getsockopt_rt           rt_dev_getsockopt
#define setsockopt_rt           rt_dev_setsockopt
#define getsockname_rt          rt_dev_getsockname
#define getpeername_rt          rt_dev_getpeername


#ifdef __KERNEL__

/* utils */
extern unsigned long rt_inet_aton(const char *ip);
extern int rt_eth_aton(char *addr_buf, const char *mac);

#endif  /* __KERNEL__ */

#endif  /* __RTNET_H_ */
