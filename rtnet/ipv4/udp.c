/***
 *
 *  ipv4/udp.c - UDP implementation for RTnet
 *
 *  Copyright (C) 1999, 2000 Zentropic Computing, LLC
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
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <net/checksum.h>

#include <rtskb.h>
#include <rtnet_internal.h>
#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <ipv4/ip_fragment.h>
#include <ipv4/ip_output.h>
#include <ipv4/ip_sock.h>
#include <ipv4/protocol.h>
#include <ipv4/route.h>
#include <ipv4/udp.h>


/***
 *  This structure is used to register a UDP socket for reception. All
 +  structures are kept in the port_registry array to increase the cache
 *  locality during the critical port lookup in rt_udp_v4_lookup().
 */
struct udp_socket {
    u16             sport;      /* local port */
    u32             saddr;      /* local ip-addr */
    struct rtsocket *sock;
};

/***
 *  Automatic port number assignment

 *  The automatic assignment of port numbers to unbound sockets is realised as
 *  a simple addition of two values:
 *   - the socket ID (lower 8 bits of file descriptor) which is set during
 *     initialisation and left unchanged afterwards
 *   - the start value auto_port_start which is a module parameter

 *  auto_port_mask, also a module parameter, is used to define the range of
 *  port numbers which are used for automatic assignment. Any number within
 *  this range will be rejected when passed to bind_rt().

 */
static unsigned int         auto_port_start = 1024;
static unsigned int         auto_port_mask  = ~(RT_UDP_SOCKETS-1);
static int                  free_ports      = RT_UDP_SOCKETS;
static u32                  port_bitmap[(RT_UDP_SOCKETS + 31) / 32];
static struct udp_socket    port_registry[RT_UDP_SOCKETS];
static rtos_spinlock_t      udp_socket_base_lock;

MODULE_PARM(auto_port_start, "i");
MODULE_PARM(auto_port_mask, "i");
MODULE_PARM_DESC(auto_port_start, "Start of automatically assigned port range");
MODULE_PARM_DESC(auto_port_mask,
                 "Mask that defines port range for automatic assignment");


/***
 *  rt_udp_v4_lookup
 */
static inline struct rtsocket *rt_udp_v4_lookup(u32 daddr, u16 dport)
{
    unsigned long   flags;
    int             index;
    int             bit;
    int             bitmap_index;
    u32             bitmap;
    struct rtsocket *sock;


    for (bitmap_index = 0; bitmap_index < ((RT_UDP_SOCKETS + 31) / 32);
         bitmap_index++) {
        bit    = 0;
        index  = bitmap_index * 32;

        rtos_spin_lock_irqsave(&udp_socket_base_lock, flags);

        bitmap = port_bitmap[bitmap_index];
        while (bitmap != 0) {
            if (test_bit(bit, &bitmap)) {
                if ((port_registry[index].sport == dport) &&
                    ((port_registry[index].saddr == INADDR_ANY) ||
                     (port_registry[index].saddr == daddr))) {
                    sock = port_registry[index].sock;
                    rt_socket_reference(sock);

                    rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);

                    return sock;
                }
                clear_bit(bit, &bitmap);
            }
            index++;
            bit++;
        }

        rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);
    }

    return NULL;
}



/***
 *  rt_udp_bind - bind socket to local address
 *  @s:     socket
 *  @addr:  local address
 */
int rt_udp_bind(struct rtsocket *sock, const struct sockaddr *addr,
                socklen_t addrlen)
{
    struct sockaddr_in  *usin = (struct sockaddr_in *)addr;
    unsigned long       flags;
    int                 index;


    if ((addrlen < (int)sizeof(struct sockaddr_in)) ||
        ((usin->sin_port & auto_port_mask) == auto_port_start))
        return -EINVAL;

    if ((index = sock->prot.inet.reg_index) < 0)
        /* socket is being closed */
        return -EBADF;

    rtos_spin_lock_irqsave(&udp_socket_base_lock, flags);

    if (sock->prot.inet.state != TCP_CLOSE) {
        rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);
        return -EINVAL;
    }

    /* set the source-addr */
    sock->prot.inet.saddr = usin->sin_addr.s_addr;

    /* set source port, if not set by user */
    if ((sock->prot.inet.sport = usin->sin_port) == 0)
        sock->prot.inet.sport = index + auto_port_start;

    port_registry[index].sport = sock->prot.inet.sport;
    port_registry[index].saddr = sock->prot.inet.saddr;

    rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);

    return 0;
}



/***
 *  rt_udp_connect
 */
int rt_udp_connect(struct rtsocket *sock, const struct sockaddr *serv_addr,
                   socklen_t addrlen)
{
    struct sockaddr_in  *usin = (struct sockaddr_in *) serv_addr;
    unsigned long       flags;
    int                 index;


    if (usin->sin_family == AF_UNSPEC) {
        if ((index = sock->prot.inet.reg_index) < 0)
            /* socket is being closed */
            return -EBADF;

        rtos_spin_lock_irqsave(&udp_socket_base_lock, flags);

        sock->prot.inet.saddr = INADDR_ANY;
        /* Note: The following line differs from standard stacks, and we also
                 don't remove the socket from the port list. Might get fixed in
                 the future... */
        sock->prot.inet.sport = index + auto_port_start;
        sock->prot.inet.daddr = INADDR_ANY;
        sock->prot.inet.dport = 0;
        sock->prot.inet.state = TCP_CLOSE;

        rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);
    } else {
        if ((addrlen < (int)sizeof(struct sockaddr_in)) ||
            (usin->sin_family != AF_INET))
            return -EINVAL;

        rtos_spin_lock_irqsave(&udp_socket_base_lock, flags);

        if (sock->prot.inet.state != TCP_CLOSE) {
            rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);
            return -EINVAL;
        }

        sock->prot.inet.state = TCP_ESTABLISHED;
        sock->prot.inet.daddr = usin->sin_addr.s_addr;
        sock->prot.inet.dport = usin->sin_port;

        rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);
    }

    return 0;
}



/***
 *  rt_udp_socket - create a new UDP-Socket
 *  @s: socket
 */
int rt_udp_socket(struct rtdm_dev_context *context, int call_flags)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    int             ret;
    int             i;
    int             index;
    unsigned long   flags;


    if ((ret = rt_socket_init(context)) != 0)
        return ret;

    sock->protocol        = IPPROTO_UDP;
    sock->prot.inet.saddr = INADDR_ANY;
    sock->prot.inet.state = TCP_CLOSE;
#ifdef CONFIG_RTNET_RTDM_SELECT
    sock->wakeup_select   = NULL;
#endif /* CONFIG_RTNET_RTDM_SELECT */

    rtos_spin_lock_irqsave(&udp_socket_base_lock, flags);

    /* enforce maximum number of UDP sockets */
    if (free_ports == 0) {
        rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);
        return -EAGAIN;
    }
    free_ports--;

    /* find free auto-port in bitmap */
    for (i = 0; i < sizeof(port_bitmap)/4; i++)
        if (port_bitmap[i] != 0xFFFFFFFF)
            break;
    index = ffz(port_bitmap[i]);
    set_bit(index, &port_bitmap[i]);
    index += i*32;
    sock->prot.inet.reg_index = index;
    sock->prot.inet.sport     = index + auto_port_start;

    /* register UDP socket */
    port_registry[index].sport = sock->prot.inet.sport;
    port_registry[index].saddr = INADDR_ANY;
    port_registry[index].sock  = sock;

    rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);

    return 0;
}



/***
 *  rt_udp_close
 */
int rt_udp_close(struct rtdm_dev_context *context, int call_flags)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    struct rtskb    *del;
    int             port;
    unsigned long   flags;


    rtos_spin_lock_irqsave(&udp_socket_base_lock, flags);

    sock->prot.inet.state = TCP_CLOSE;

    if (sock->prot.inet.reg_index >= 0) {
        port = sock->prot.inet.reg_index;
        clear_bit(port % 32, &port_bitmap[port / 32]);

        sock->prot.inet.reg_index = -1;
    }

    rtos_spin_unlock_irqrestore(&udp_socket_base_lock, flags);

    /* cleanup already collected fragments */
    rt_ip_frag_invalidate_socket(sock);

    /* free packets in incoming queue */
    while ((del = rtskb_dequeue(&sock->incoming)) != NULL)
        kfree_rtskb(del);

    return rt_socket_cleanup(context);
}



int rt_udp_ioctl(struct rtdm_dev_context *context, int call_flags, int request,
                 void *arg)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    struct rtdm_setsockaddr_args *setaddr = arg;


    /* fast path for common socket IOCTLs */
    if (_IOC_TYPE(request) == RTIOC_TYPE_NETWORK)
        return rt_socket_common_ioctl(context, call_flags, request, arg);

    switch (request) {
        case RTIOC_BIND:
            return rt_udp_bind(sock, setaddr->addr, setaddr->addrlen);

        case RTIOC_CONNECT:
            return rt_udp_connect(sock, setaddr->addr, setaddr->addrlen);

        default:
            return rt_ip_ioctl(context, call_flags, request, arg);
    }
}



/***
 *  rt_udp_recvmsg
 */
ssize_t rt_udp_recvmsg(struct rtdm_dev_context *context, int call_flags,
                       struct msghdr *msg, int msg_flags)
{
    struct rtsocket     *sock = (struct rtsocket *)&context->dev_private;
    size_t              len   = rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    struct rtskb        *skb;
    struct rtskb        *first_skb;
    size_t              copied = 0;
    size_t              block_size;
    size_t              data_len;
    struct udphdr       *uh;
    struct sockaddr_in  *sin;
    int                 ret;
    unsigned long       flags;
    rtos_time_t         timeout;


    /* block on receive event */
    if (!test_bit(RT_SOCK_NONBLOCK, &context->context_flags) &&
        ((msg_flags & MSG_DONTWAIT) == 0))
        while ((skb = rtskb_dequeue_chain(&sock->incoming)) == NULL) {
            rtos_spin_lock_irqsave(&sock->param_lock, flags);
            memcpy(&timeout, &sock->timeout, sizeof(timeout));
            rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

            if (!RTOS_TIME_IS_ZERO(&timeout)) {
                ret = rtos_event_sem_wait_timed(&sock->wakeup_event, &timeout);
                if (ret == RTOS_EVENT_TIMEOUT)
                    return -ETIMEDOUT;
            } else
                ret = rtos_event_sem_wait(&sock->wakeup_event);

            if (RTOS_EVENT_ERROR(ret))
                return -ENOTSOCK;
        }
    else {
        skb = rtskb_dequeue_chain(&sock->incoming);
        if (skb == NULL)
            return -EAGAIN;
    }

    uh = skb->h.uh;
    data_len = ntohs(uh->len) - sizeof(struct udphdr);
    sin = msg->msg_name;

    /* copy the address */
    msg->msg_namelen = sizeof(*sin);
    if (sin) {
        sin->sin_family      = AF_INET;
        sin->sin_port        = uh->source;
        sin->sin_addr.s_addr = skb->nh.iph->saddr;
    }

    /* remove the UDP header */
    __rtskb_pull(skb, sizeof(struct udphdr));

    first_skb = skb;

    /* iterate over all IP fragments */
    do {
        rtskb_trim(skb, data_len);

        block_size = skb->len;
        copied += block_size;
        data_len -= block_size;

        /* The data must not be longer than the available buffer size */
        if (copied > len) {
            block_size -= copied - len;
            copied = len;
            msg->msg_flags |= MSG_TRUNC;

            /* copy the data */
            rt_memcpy_tokerneliovec(msg->msg_iov, skb->data, block_size);

            break;
        }

        /* copy the data */
        rt_memcpy_tokerneliovec(msg->msg_iov, skb->data, block_size);

        /* next fragment */
        skb = skb->next;
    } while (skb != NULL);

    /* did we copied all bytes? */
    if (data_len > 0)
        msg->msg_flags |= MSG_TRUNC;

    if ((msg_flags & MSG_PEEK) == 0)
        kfree_rtskb(first_skb);
    else {
        __rtskb_push(first_skb, sizeof(struct udphdr));
        rtskb_queue_head(&sock->incoming, first_skb);
    }

    return copied;
}



/***
 *  struct udpfakehdr
 */
struct udpfakehdr
{
    struct udphdr uh;
    u32 daddr;
    u32 saddr;
    struct iovec *iov;
    int iovlen;
    u32 wcheck;
};



/***
 *
 */
static int rt_udp_getfrag(const void *p, char *to, unsigned int offset, unsigned int fraglen)
{
    struct udpfakehdr *ufh = (struct udpfakehdr *)p;
    int i;

    // We should optimize this function a bit (copy+csum...)!
    if (offset==0) {
        /* Checksum of the complete data part of the UDP message: */
        for (i = 0; i < ufh->iovlen; i++) {
            ufh->wcheck = csum_partial(ufh->iov[i].iov_base, ufh->iov[i].iov_len,
                                       ufh->wcheck);
        }

        rt_memcpy_fromkerneliovec(to + sizeof(struct udphdr), ufh->iov,
                                  fraglen - sizeof(struct udphdr));

        /* Checksum of the udp header: */
        ufh->wcheck = csum_partial((char *)ufh, sizeof(struct udphdr), ufh->wcheck);

        ufh->uh.check = csum_tcpudp_magic(ufh->saddr, ufh->daddr, ntohs(ufh->uh.len),
                                          IPPROTO_UDP, ufh->wcheck);

        if (ufh->uh.check == 0)
            ufh->uh.check = -1;

        memcpy(to, ufh, sizeof(struct udphdr));
        return 0;
    }

    rt_memcpy_fromkerneliovec(to, ufh->iov, fraglen);

    return 0;
}



/***
 *  rt_udp_sendmsg
 */
ssize_t rt_udp_sendmsg(struct rtdm_dev_context *context, int call_flags,
                       const struct msghdr *msg, int msg_flags)
{
    struct rtsocket     *sock = (struct rtsocket *)&context->dev_private;
    size_t              len   = rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    int                 ulen  = len + sizeof(struct udphdr);
    struct sockaddr_in  *usin;
    struct udpfakehdr   ufh;
    struct dest_route   rt;
    u32                 saddr;
    u32                 daddr;
    u16                 dport;
    int                 err;
    unsigned long       flags;


    if ((len < 0) || (len > 0xFFFF-sizeof(struct iphdr)-sizeof(struct udphdr)))
        return -EMSGSIZE;

    if (msg_flags & MSG_OOB)   /* Mirror BSD error message compatibility */
        return -EOPNOTSUPP;

    if (msg_flags & ~(MSG_DONTROUTE|MSG_DONTWAIT) )
        return -EINVAL;

    if ((msg->msg_name) && (msg->msg_namelen==sizeof(struct sockaddr_in))) {
        usin = (struct sockaddr_in*) msg->msg_name;

        if ((usin->sin_family != AF_INET) && (usin->sin_family != AF_UNSPEC))
            return -EINVAL;

        daddr = usin->sin_addr.s_addr;
        dport = usin->sin_port;

        rtos_spin_lock_irqsave(&sock->param_lock, flags);
    } else {
        rtos_spin_lock_irqsave(&sock->param_lock, flags);

        if (sock->prot.inet.state != TCP_ESTABLISHED)
            return -ENOTCONN;

        daddr = sock->prot.inet.daddr;
        dport = sock->prot.inet.dport;
    }
    saddr         = sock->prot.inet.saddr;
    ufh.uh.source = sock->prot.inet.sport;

    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    if ((daddr | dport) == 0)
        return -EINVAL;

    /* get output route */
    err = rt_ip_route_output(&rt, daddr);
    if (err)
        return err;

    /* check if specified source address fits */
    if ((saddr != INADDR_ANY) && (saddr != rt.rtdev->local_ip)) {
        rtdev_dereference(rt.rtdev);
        return -EHOSTUNREACH;
    }

    /* we found a route, remember the routing dest-addr could be the netmask */
    ufh.saddr     = rt.rtdev->local_ip;
    ufh.daddr     = daddr;
    ufh.uh.dest   = dport;
    ufh.uh.len    = htons(ulen);
    ufh.uh.check  = 0;
    ufh.iov       = msg->msg_iov;
    ufh.iovlen    = msg->msg_iovlen;
    ufh.wcheck    = 0;

    err = rt_ip_build_xmit(sock, rt_udp_getfrag, &ufh, ulen, &rt, msg_flags);

    rtdev_dereference(rt.rtdev);

    if (!err)
        return len;
    else
        return err;
}

#ifdef CONFIG_RTNET_RTDM_SELECT
/***
 *  rt_udp_poll
 */
unsigned int rt_udp_poll(struct rtdm_dev_context *context) /* , poll_table *wait) */
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    unsigned int mask = 0;

    /* rtdm_poll_wait(context, sock->wqe_in, wait) */
    /* rtdm_poll_wait(context, sock->wqe_out, wait) */

    /* if data is available (sock.incoming!=NULL), bit-or mask with POLLIN */
    if (NULL != sock->incoming.first)	{
	mask |= POLLIN;
    }

#warning check that sending really does not block
    mask |= POLLOUT;

    return mask;
}

/***
 *  rt_udp_pollwait
 * The right position for this function is in rtdm! (A poll function should be implemented here instead.)
 */
ssize_t rt_udp_pollwait(struct rtdm_dev_context *context, wait_queue_primitive_t *sem)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    unsigned long       flags;

    rtos_spin_lock_irqsave(&sock->param_lock, flags);

    /* check for available data / free buffers */
    /* call poll_wait() */

#warning there could already be a pointer to a semaphore
    sock->wakeup_select = sem;
    /* the linux select() calls poll_freewait with a poll_table, who knows alls registered waitqueues;
     * so in linux the wait-queue belongs to the socket and not like the semaphore to the select-process */
    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    /* a meaningfull value should be returned */
    return 0;
}

/***
 *  rt_udp_pollfree
 */
ssize_t rt_udp_pollfree(struct rtdm_dev_context *context)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    unsigned long       flags;

    rtos_spin_lock_irqsave(&sock->param_lock, flags);

    sock->wakeup_select = NULL;

    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    /* a meaningfull value should be returned */
    return 0;
}
#endif /* CONFIG_RTNET_RTDM_SELECT */


/***
 *  rt_udp_check
 */
static inline unsigned short rt_udp_check(struct udphdr *uh, int len,
                                          unsigned long saddr,
                                          unsigned long daddr,
                                          unsigned long base)
{
    return(csum_tcpudp_magic(saddr, daddr, len, IPPROTO_UDP, base));
}



struct rtsocket *rt_udp_dest_socket(struct rtskb *skb)
{
    struct udphdr           *uh   = skb->h.uh;
    unsigned short          ulen  = ntohs(uh->len);
    u32                     saddr = skb->nh.iph->saddr;
    u32                     daddr = skb->nh.iph->daddr;
    struct rtnet_device*    rtdev = skb->rtdev;


    if (uh->check == 0)
        skb->ip_summed = CHECKSUM_UNNECESSARY;
/* ip_summed (yet) never equals CHECKSUM_HW
    else
        if (skb->ip_summed == CHECKSUM_HW) {
            skb->ip_summed = CHECKSUM_UNNECESSARY;

            if ( !rt_udp_check(uh, ulen, saddr, daddr, skb->csum) )
                return NULL;

            skb->ip_summed = CHECKSUM_NONE;
        }*/

    if (skb->ip_summed != CHECKSUM_UNNECESSARY)
        skb->csum = csum_tcpudp_nofold(saddr, daddr, ulen, IPPROTO_UDP, 0);

    /* patch broadcast daddr */
    if (daddr == rtdev->broadcast_ip)
        daddr = rtdev->local_ip;

    /* find the destination socket */
    skb->sk = rt_udp_v4_lookup(daddr, uh->dest);

    return skb->sk;
}



/***
 *  rt_udp_rcv
 */
int rt_udp_rcv (struct rtskb *skb)
{
    struct rtsocket *sock = skb->sk;
    void            (*callback_func)(struct rtdm_dev_context *, void *);
    void            *callback_arg;
    unsigned long   flags;


    rtskb_queue_tail(&sock->incoming, skb);
    rtos_event_sem_signal(&sock->wakeup_event);

    rtos_spin_lock_irqsave(&sock->param_lock, flags);
#ifdef CONFIG_RTNET_RTDM_SELECT
    if (sock->wakeup_select != NULL) {
	wq_wakeup(sock->wakeup_select);
    }
#endif /* CONFIG_RTNET_RTDM_SELECT */
    callback_func = sock->callback_func;
    callback_arg  = sock->callback_arg;
    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    if (callback_func)
        callback_func(rt_socket_context(sock), callback_arg);

    return 0;
}



/***
 *  rt_udp_rcv_err
 */
void rt_udp_rcv_err (struct rtskb *skb)
{
    rtos_print("RTnet: rt_udp_rcv err\n");
}



/***
 *  UDP-Initialisation
 */
static struct rtinet_protocol udp_protocol = {
    protocol:       IPPROTO_UDP,
    dest_socket:    &rt_udp_dest_socket,
    rcv_handler:    &rt_udp_rcv,
    err_handler:    &rt_udp_rcv_err,
    init_socket:    &rt_udp_socket
};



/***
 *  rt_udp_init
 */
void __init rt_udp_init(void)
{
    if ((auto_port_start < 0) || (auto_port_start >= 0x10000 - RT_UDP_SOCKETS))
        auto_port_start = 1024;
    auto_port_start = htons(auto_port_start & (auto_port_mask & 0xFFFF));
    auto_port_mask  = htons(auto_port_mask | 0xFFFF0000);

    rtos_spin_lock_init(&udp_socket_base_lock);
    rt_inet_add_protocol(&udp_protocol);
}



/***
 *  rt_udp_release
 */
void rt_udp_release(void)
{
    rt_inet_del_protocol(&udp_protocol);
}
