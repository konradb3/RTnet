/***
 *
 *  rtnet/socket.c - sockets implementation for rtnet
 *
 *  Copyright (C) 1999       Lineo, Inc
 *                1999, 2002 David A. Schleef <ds@schleef.org>
 *                2002       Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2003, 2004 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <asm/bitops.h>

#include <rtnet.h>
#include <rtnet_internal.h>
#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <ipv4/protocol.h>
#include <packet/af_packet.h>


#define SKB_POOL_CLOSED     RTDM_USER_CONTEXT_FLAG + 0

static unsigned int socket_rtskbs = DEFAULT_SOCKET_RTSKBS;
MODULE_PARM(socket_rtskbs, "i");
MODULE_PARM_DESC(socket_rtskbs, "Default number of realtime socket buffers in socket pools");

const char rtnet_rtdm_driver_name[] =
    "RTnet " RTNET_PACKAGE_VERSION;
const char rtnet_rtdm_provider_name[] =
    "(C) 1999-2004 RTnet Development Team, http://rtnet.sf.net";


/************************************************************************
 *  internal socket functions                                           *
 ************************************************************************/

/***
 *  rt_socket_init - initialises a new socket structure
 */
int rt_socket_init(struct rtdm_dev_context *context)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    unsigned int    pool_size;


    sock->priority =
        RTSKB_PRIO_VALUE(SOCK_DEF_PRIO, RTSKB_DEF_RT_CHANNEL);
    sock->callback_func = NULL;

    rtskb_queue_init(&sock->incoming);

    rtos_nanosecs_to_time(0, &sock->timeout);

    rtos_spin_lock_init(&sock->param_lock);
    rtos_event_sem_init(&sock->wakeup_event);

    if (test_bit(RTDM_CREATED_IN_NRT, &context->context_flags))
        pool_size = rtskb_pool_init(&sock->skb_pool, socket_rtskbs);
    else
        pool_size = rtskb_pool_init_rt(&sock->skb_pool, socket_rtskbs);
    atomic_set(&sock->pool_size, pool_size);

    if (pool_size < socket_rtskbs) {
        /* fix statistics */
        if (pool_size == 0)
            rtskb_pools--;

        rt_socket_cleanup(context);
        return -ENOMEM;
    }

    return 0;
}



/***
 *  rt_socket_cleanup - releases resources allocated for the socket
 */
int rt_socket_cleanup(struct rtdm_dev_context *context)
{
    struct rtsocket *sock  = (struct rtsocket *)&context->dev_private;
    unsigned int    rtskbs;
    unsigned long   flags;


    rtos_event_sem_delete(&sock->wakeup_event);

    rtos_spin_lock_irqsave(&sock->param_lock, flags);

    set_bit(SKB_POOL_CLOSED, &context->context_flags);
    rtskbs = atomic_read(&sock->pool_size);

    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    if (rtskbs > 0) {
        if (test_bit(RTDM_CREATED_IN_NRT, &context->context_flags)) {
            rtskbs = rtskb_pool_shrink(&sock->skb_pool, rtskbs);
            atomic_sub(rtskbs, &sock->pool_size);
            if (atomic_read(&sock->pool_size) > 0)
                return -EAGAIN;
            rtskb_pool_release(&sock->skb_pool);
        } else {
            rtskbs = rtskb_pool_shrink_rt(&sock->skb_pool, rtskbs);
            atomic_sub(rtskbs, &sock->pool_size);
            if (atomic_read(&sock->pool_size) > 0)
                return -EAGAIN;
            rtskb_pool_release_rt(&sock->skb_pool);
        }
    }

    return 0;
}



/***
 *  rt_socket_common_ioctl
 */
int rt_socket_common_ioctl(struct rtdm_dev_context *context, int call_flags,
                           int request, void *arg)
{
    struct rtsocket         *sock = (struct rtsocket *)&context->dev_private;
    int                     ret = 0;
    struct rtnet_callback   *callback = arg;
    unsigned int            rtskbs;
    unsigned long           flags;


    switch (request) {
        case RTNET_RTIOC_PRIORITY:
            sock->priority = *(unsigned int *)arg;
            break;

        case RTNET_RTIOC_TIMEOUT:
            rtos_spin_lock_irqsave(&sock->param_lock, flags);

            rtos_nanosecs_to_time(*(nanosecs_t *)arg, &sock->timeout);

            rtos_spin_unlock_irqrestore(&sock->param_lock, flags);
            break;

        case RTNET_RTIOC_CALLBACK:
            if (test_bit(RTDM_USER_MODE_CALL, &context->context_flags))
                return -EACCES;

            rtos_spin_lock_irqsave(&sock->param_lock, flags);

            sock->callback_func = callback->func;
            sock->callback_arg  = callback->arg;

            rtos_spin_unlock_irqrestore(&sock->param_lock, flags);
            break;

        case RTNET_RTIOC_NONBLOCK:
            if (*(unsigned int *)arg != 0)
                set_bit(RT_SOCK_NONBLOCK, &context->context_flags);
            else
                clear_bit(RT_SOCK_NONBLOCK, &context->context_flags);
            break;

        case RTNET_RTIOC_EXTPOOL:
            rtskbs = *(unsigned int *)arg;

            rtos_spin_lock_irqsave(&sock->param_lock, flags);

            if (test_bit(SKB_POOL_CLOSED, &context->context_flags)) {
                rtos_spin_unlock_irqrestore(&sock->param_lock, flags);
                return -EBADF;
            }
            atomic_add(rtskbs, &sock->pool_size);

            rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

            if (test_bit(RTDM_CREATED_IN_NRT, &context->context_flags)) {
                if (!(call_flags & RTDM_NRT_CALL))
                    return -EACCES;
                ret = rtskb_pool_extend(&sock->skb_pool, rtskbs);
            } else
                ret = rtskb_pool_extend_rt(&sock->skb_pool, rtskbs);
            atomic_sub(rtskbs-ret, &sock->pool_size);
            break;

        case RTNET_RTIOC_SHRPOOL:
            rtskbs = *(unsigned int *)arg;

            rtos_spin_lock_irqsave(&sock->param_lock, flags);

            if (test_bit(SKB_POOL_CLOSED, &context->context_flags)) {
                rtos_spin_unlock_irqrestore(&sock->param_lock, flags);
                return -EBADF;
            }
            atomic_sub(rtskbs, &sock->pool_size);

            rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

            if (test_bit(RTDM_CREATED_IN_NRT, &context->context_flags)) {
                if (!(call_flags & RTDM_NRT_CALL))
                    return -EACCES;
                ret = rtskb_pool_shrink(&sock->skb_pool, *(unsigned int *)arg);
            } else
                ret = rtskb_pool_shrink_rt(&sock->skb_pool,
                                           *(unsigned int *)arg);
            atomic_add(rtskbs-ret, &sock->pool_size);
            break;

        default:
            ret = -EOPNOTSUPP;
            break;
    }

    return ret;
}



/***
 *  rt_socket_if_ioctl
 */
int rt_socket_if_ioctl(struct rtdm_dev_context *context, int call_flags,
                       int request, void *arg)
{
    struct rtnet_device     *rtdev;
    struct ifreq            *cur_ifr;
    struct sockaddr_in      *sin;
    int                     i;
    int                     size;
    struct ifconf           *ifc = arg;
    struct ifreq            *ifr = arg;
    int                     ret = 0;


    switch (request) {
        case SIOCGIFCONF:
            size = 0;
            cur_ifr = ifc->ifc_req;

            for (i = 1; i <= MAX_RT_DEVICES; i++) {
                rtdev = rtdev_get_by_index(i);
                if (rtdev != NULL) {
                    if ((rtdev->flags & IFF_RUNNING) == 0) {
                        rtdev_dereference(rtdev);
                        continue;
                    }

                    size += sizeof(struct ifreq);
                    if (size > ifc->ifc_len) {
                        rtdev_dereference(rtdev);
                        size = ifc->ifc_len;
                        break;
                    }

                    strncpy(cur_ifr->ifr_name, rtdev->name,
                            IFNAMSIZ);
                    sin = (struct sockaddr_in *)&cur_ifr->ifr_addr;
                    sin->sin_family      = AF_INET;
                    sin->sin_addr.s_addr = rtdev->local_ip;

                    cur_ifr++;
                    rtdev_dereference(rtdev);
                }
            }

            ifc->ifc_len = size;
            break;

        case SIOCGIFFLAGS:
            rtdev = rtdev_get_by_name(ifr->ifr_name);
            if (rtdev == NULL)
                return -ENODEV;
            else {
                ifr->ifr_flags = rtdev->flags;
                rtdev_dereference(rtdev);
            }
            break;

        default:
            ret = -EOPNOTSUPP;
            break;
    }

    return ret;
}
