/***
 * rtnet/socket.c - sockets implementation for rtnet
 *
 * Copyright (C) 1999      Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002      Ulrich Marx <marx@kammer.uni-hannover.de>
 *               2003      Jan Kiszka <jan.kiszka@web.de>
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
 *
 */

#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/tcp.h>

#include <rtnet.h>
#include <rtnet_internal.h>
#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <ipv4/protocol.h>
#include <packet/af_packet.h>


static unsigned int socket_rtskbs = DEFAULT_SOCKET_RTSKBS;
MODULE_PARM(socket_rtskbs, "i");
MODULE_PARM_DESC(socket_rtskbs, "Default number of realtime socket buffers in socket pools");

static struct rtsocket rt_sockets[RT_SOCKETS];
static struct rtsocket *free_rtsockets;
static rtos_spinlock_t socket_base_lock = RTOS_SPIN_LOCK_UNLOCKED;


/************************************************************************
 *  internal socket functions                                           *
 ************************************************************************/

int rt_socket_release(struct rtsocket *sock);

/***
 *  rt_socket_alloc
 */
static inline struct rtsocket *rt_socket_alloc(void)
{
    unsigned long    flags;
    struct rtsocket  *sock;


    rtos_spin_lock_irqsave(&socket_base_lock, flags);

    sock = free_rtsockets;
    if (!sock) {
        rtos_spin_unlock_irqrestore(&socket_base_lock, flags);
        rtos_print("RTnet: no more rt-sockets\n");
        return NULL;
    }
    free_rtsockets = (struct rtsocket *)free_rtsockets->list_entry.next;

    atomic_set(&sock->refcount, 1);

    rtos_spin_unlock_irqrestore(&socket_base_lock, flags);

    sock->priority = SOCK_DEF_PRIO;
    sock->state    = TCP_CLOSE;
    sock->wakeup   = NULL;

    rtskb_queue_init(&sock->incoming);

    rtos_event_sem_init(&sock->wakeup_event);

    /* detect if running in Linux context */
    if (rtos_in_rt_context()) {
        sock->rt_pool = 1;
        sock->pool_size = rtskb_pool_init_rt(&sock->skb_pool, socket_rtskbs);
    } else
        sock->pool_size = rtskb_pool_init(&sock->skb_pool, socket_rtskbs);

    if (sock->pool_size < socket_rtskbs) {
        /* fix statistics */
        if (sock->pool_size == 0)
            rtskb_pools--;

        rt_socket_release(sock);
        return NULL;
    }

    return sock;
}



/***
 *  rt_socket_release
 */
int rt_socket_release(struct rtsocket *sock)
{
    unsigned long flags;
    unsigned int rtskbs = sock->pool_size;


    rtos_event_sem_delete(&sock->wakeup_event);

    if (sock->pool_size > 0)
    {
        if (sock->rt_pool) {
            rtskbs = rtskb_pool_shrink_rt(&sock->skb_pool, rtskbs);
            if ((sock->pool_size -= rtskbs) > 0) {
                rt_socket_dereference(sock);
                return -EAGAIN;
            }
            rtskb_pool_release_rt(&sock->skb_pool);
        } else {
            rtskbs = rtskb_pool_shrink(&sock->skb_pool, rtskbs);
            if ((sock->pool_size -= rtskbs) > 0) {
                rt_socket_dereference(sock);
                return -EAGAIN;
            }
            rtskb_pool_release(&sock->skb_pool);
        }
    }

    rtos_spin_lock_irqsave(&socket_base_lock, flags);

    if (!atomic_dec_and_test(&sock->refcount)) {
        rtos_spin_unlock_irqrestore(&socket_base_lock, flags);
        return -EAGAIN;
    }

    sock->list_entry.next = (struct list_head *)free_rtsockets;
    free_rtsockets = sock;

    /* invalidate file descriptor id */
    sock->fd = (sock->fd + 0x100) & 0x7FFFFFFF;

    rtos_spin_unlock_irqrestore(&socket_base_lock, flags);

    return 0;
}



/***
 *  rt_scoket_lookup
 *  @fd - file descriptor
 */
struct rtsocket *rt_socket_lookup(int fd)
{
    struct rtsocket *sock = NULL;
    unsigned long flags;
    unsigned int index;


    if ((index = fd & 0xFF) < RT_SOCKETS) {
        rtos_spin_lock_irqsave(&socket_base_lock, flags);

        if (rt_sockets[index].fd == fd) {
            sock = &rt_sockets[index];
            rt_socket_reference(sock);
        }

        rtos_spin_unlock_irqrestore(&socket_base_lock, flags);
    }
    return sock;
}



/************************************************************************
 *  file discriptor socket interface                                    *
 ************************************************************************/

/***
 *  rt_socket
 *  Create a new socket of type TYPE in domain DOMAIN (family),
 *  using protocol PROTOCOL.  If PROTOCOL is zero, one is chosen
 *  automatically. Returns a file descriptor for the new socket,
 *  or errors < 0.
 */
int rt_socket(int family, int type, int protocol)
{
    struct rtsocket *sock = NULL;
    int ret;


    /* allocate a new socket */
    if ((sock = rt_socket_alloc()) == NULL)
        return -ENOMEM;

    sock->type = type;

    switch (family) {
        /* protocol family PF_INET */
        case PF_INET:
            ret = rt_inet_socket(sock, protocol);
            break;

        case PF_PACKET:
            ret = rt_packet_socket(sock, protocol);
            break;

        default:
            ret = -EAFNOSUPPORT;
    }

    if (ret < 0)
        rt_socket_release(sock);
    else
        rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_sock_bind
 *  Bind a socket to a sockaddr
 */
int rt_socket_bind(int s, struct sockaddr *my_addr, socklen_t addrlen)
{
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    ret = sock->ops->bind(sock, my_addr, addrlen);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_socket_listen
 */
int rt_socket_listen(int s, int backlog)
{
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    ret = sock->ops->listen(sock, backlog);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_socket_connect
 */
int rt_socket_connect(int s, const struct sockaddr *serv_addr, socklen_t addrlen)
{
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    ret = sock->ops->connect(sock, serv_addr, addrlen);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_socket_accept
 */
int rt_socket_accept(int s, struct sockaddr *addr, socklen_t *addrlen)
{
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    ret = sock->ops->accept(sock, addr, addrlen);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_socket_close
 */
int rt_socket_close(int s)
{
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    if ((ret = sock->ops->close(sock)) < 0) {
        rt_socket_dereference(sock);
        return ret;
    } else
        return rt_socket_release(sock);
}



/***
 *  rt_socket_send
 */
int rt_socket_send(int s, const void *msg, size_t len, int flags)
{
    return rt_socket_sendto(s, msg, len, flags, NULL, 0);
}



/***
 *  rt_socket_recv
 */
int rt_socket_recv(int s, void *buf, size_t len, int flags)
{
    return rt_socket_recvfrom(s, buf, len, flags, NULL, NULL);
}



/***
 *  rt_socket_sendto
 */
int rt_socket_sendto(int s, const void *msg, size_t len, int flags,
                     const struct sockaddr *to, socklen_t tolen)
{
    struct msghdr msg_hdr;
    struct iovec iov;
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    iov.iov_base=(void *)msg;
    iov.iov_len=len;

    msg_hdr.msg_name=(void*)to;
    msg_hdr.msg_namelen=tolen;
    msg_hdr.msg_iov=&iov;
    msg_hdr.msg_iovlen=1;

    ret = sock->ops->sendmsg(sock, &msg_hdr, len, flags);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_recvfrom
 *  @s          socket descriptor
 *  @buf        buffer
 *  @len        length of buffer
 *  @flags      usermode -> kern
 *                  MSG_DONTROUTE: target is in lan
 *                  MSG_DONTWAIT : if there no data to recieve, get out
 *                  MSG_ERRQUEUE :
 *              kern->usermode
 *                  MSG_TRUNC    :
 */
int rt_socket_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    struct msghdr msg_hdr;
    struct iovec iov;
    int error=0;
    struct rtsocket *sock;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    iov.iov_base = buf;
    iov.iov_len  = len;

    msg_hdr.msg_name    = from;
    msg_hdr.msg_namelen = (from != NULL) ? *fromlen : 0;
    msg_hdr.msg_iov     = &iov;
    msg_hdr.msg_iovlen  = 1;

    error = sock->ops->recvmsg(sock, &msg_hdr, len, flags);

    rt_socket_dereference(sock);

    if ((error >= 0) && (from != NULL))
        *fromlen = msg_hdr.msg_namelen;

    return error;
}



/***
 *  rt_socket_sendmsg
 */
int rt_socket_sendmsg(int s, const struct msghdr *msg, int flags)
{
    size_t total_len;
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    total_len = rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    ret = sock->ops->sendmsg(sock, msg, total_len, flags);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_socket_recvmsg
 */
int rt_socket_recvmsg(int s, struct msghdr *msg, int flags)
{
    size_t total_len;
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    total_len = rt_iovec_len(msg->msg_iov,msg->msg_iovlen);
    ret = sock->ops->recvmsg(sock, msg, total_len, flags);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_sock_getsockname
 */
int rt_socket_getsockname(int s, struct sockaddr *name, socklen_t *namelen)
{
    struct rtsocket *sock;
    int ret;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    ret = sock->ops->getsockname(sock, name, namelen);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_socket_callback
 */
int rt_socket_callback(int s, int (*func)(int,void *), void *arg)
{
    struct rtsocket *sock;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    sock->wakeup_arg = arg;
    sock->wakeup     = func;

    rt_socket_dereference(sock);
    return 0;
}



/***
 *  rt_socket_setsockopt
 */
int rt_socket_setsockopt(int s, int level, int optname, const void *optval,
                         socklen_t optlen)
{
    int ret = 0;
    struct rtsocket *sock;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    if (level == SOL_SOCKET) {
        if (optlen < sizeof(unsigned int)) {
            rt_socket_dereference(sock);
            return -EINVAL;
        }

        switch (optname) {
            case RT_SO_EXTPOOL:
                if (sock->rt_pool)
                    ret = rtskb_pool_extend_rt(&sock->skb_pool,
                                               *(unsigned int *)optval);
                else
                    ret = rtskb_pool_extend(&sock->skb_pool,
                                            *(unsigned int *)optval);
                sock->pool_size += ret;
                break;

            case RT_SO_SHRPOOL:
                if (sock->rt_pool)
                    ret = rtskb_pool_shrink_rt(&sock->skb_pool,
                                               *(unsigned int *)optval);
                else
                    ret = rtskb_pool_shrink(&sock->skb_pool,
                                            *(unsigned int *)optval);
                sock->pool_size -= ret;
                break;

            case RT_SO_PRIORITY:
                sock->priority = *(unsigned int *)optval;
                break;

            case RT_SO_NONBLOCK:
                if (*(unsigned int *)optval != 0)
                    sock->flags |= RT_SOCK_NONBLOCK;
                else
                    sock->flags &= ~RT_SOCK_NONBLOCK;
                break;

            case RT_SO_TIMEOUT:
                if (optlen < sizeof(nanosecs_t))
                    ret = -EINVAL;
                else
                    rtos_nanosecs_to_time(*(nanosecs_t *)optval,
                                          &sock->timeout);
                break;

            default:
                ret = -ENOPROTOOPT;
                break;
        }
    } else
        ret = sock->ops->setsockopt(sock, level, optname, optval, optlen);

    rt_socket_dereference(sock);
    return ret;
}



/***
 *  rt_socket_ioctl
 */
int rt_socket_ioctl(int s, int request, void *arg)
{
    int ret = 0;
    struct rtsocket *sock;
    union {
        struct ifconf ifc;
        struct ifreq ifr;
    } *args = arg;
    struct rtnet_device *rtdev;
    struct ifreq *cur_ifr;
    struct sockaddr_in *sin;
    int i;
    int size;


    if ((sock = rt_socket_lookup(s)) == NULL)
        return -ENOTSOCK;

    switch (request) {
        case SIOCGIFCONF:
            size = 0;
            cur_ifr = args->ifc.ifc_req;

            for (i = 1; i <= MAX_RT_DEVICES; i++) {
                rtdev = rtdev_get_by_index(i);
                if (rtdev != NULL) {
                    if ((rtdev->flags & IFF_RUNNING) == 0) {
                        rtdev_dereference(rtdev);
                        continue;
                    }

                    size += sizeof(struct ifreq);
                    if (size > args->ifc.ifc_len) {
                        rtdev_dereference(rtdev);
                        size = args->ifc.ifc_len;
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

            args->ifc.ifc_len = size;
            break;

        case SIOCGIFFLAGS:
            rtdev = rtdev_get_by_name(args->ifr.ifr_name);
            if (rtdev == NULL)
                ret = -ENODEV;
            else {
                args->ifr.ifr_flags = rtdev->flags;
                rtdev_dereference(rtdev);
            }
            break;

        default:
            ret = -EOPNOTSUPP;
    }

    rt_socket_dereference(sock);
    return ret;
}



/************************************************************************
 *  initialisation of rt-socket interface                               *
 ************************************************************************/

/***
 *  rtsocket_init
 */
void __init rtsockets_init(void)
{
    int i;


    /* initialise the first socket */
    rt_sockets[0].list_entry.next = (struct list_head *)&rt_sockets[1];
    rt_sockets[0].state           = TCP_CLOSE;
    rt_sockets[0].fd              = 0;

    /* initialise the last socket */
    rt_sockets[RT_SOCKETS-1].list_entry.next = NULL;
    rt_sockets[RT_SOCKETS-1].state           = TCP_CLOSE;
    rt_sockets[RT_SOCKETS-1].fd              = RT_SOCKETS-1;

    for (i = 1; i < RT_SOCKETS-1; i++) {
        rt_sockets[i].list_entry.next = (struct list_head *)&rt_sockets[i+1];
        rt_sockets[i].state           = TCP_CLOSE;
        rt_sockets[i].fd              = i;
    }
    free_rtsockets=&rt_sockets[0];
}
