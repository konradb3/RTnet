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


/* RTDM_API_VER < 5 is lacking generic time types */
#if !(defined RTDM_API_VER) || (RTDM_API_VER < 5)

#ifndef __KERNEL__
#include <stdint.h>
#endif /* !__KERNEL__ */

typedef uint64_t                nanosecs_abs_t;
typedef int64_t                 nanosecs_rel_t;

#define RTDM_TIMEOUT_INFINITE   0
#define RTDM_TIMEOUT_NONE       (-1)

#endif /* !RTDM_API_VER */


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
/* RTNET_RTIOC_NONBLOCK         _IOW(RTIOC_TYPE_NETWORK, 0x13, unsigned int)
 * This IOCTL is no longer supported (and it was buggy anyway).
 * Use RTNET_RTIOC_TIMEOUT with any negative timeout value instead. */
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


#ifdef __KERNEL__

/* utility functions */

/* provided by rt_ipv4 */
unsigned long rt_inet_aton(const char *ip);

/* provided by rt_packet */
int rt_eth_aton(unsigned char *addr_buf, const char *mac);

#endif  /* __KERNEL__ */

#endif  /* __RTNET_H_ */
