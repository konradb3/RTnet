/* ipv4/udp.c
 *
 * rtnet - real-time networking subsystem
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


/***
 *  udp_sockets
 *  the registered sockets from any server
 */
LIST_HEAD(udp_sockets);
static rtos_res_lock_t  udp_socket_base_lock;

/***
 *  Automatic port number assignment
 *  The automatic assignment of port numbers to unbound sockets is realised as
 *  a simple addition of two values:
 *   - the socket ID (lower 8 bits of file descriptor) which is set during
 *     initialisation and left unchanged afterwards
 *   - the start value auto_port_start which is a module parameter
 *  auto_port_mask, also a module parameter, is used to define the range of
 *  port numbers which are used for automatic assignment. Any number within
 *  this range be rejected when passed to rt_bind(). This restriction allows a
 *  lock-free implementation of the port assignment.
 */
static unsigned int     auto_port_start = 1024;
static unsigned int     auto_port_mask  = ~(RT_SOCKETS-1);

MODULE_PARM(auto_port_start, "i");
MODULE_PARM(auto_port_mask, "i");
MODULE_PARM_DESC(auto_port_start, "Start of automatically assigned port range");
MODULE_PARM_DESC(auto_port_mask,
                 "Mask that defines port range for automatic assignment");


/***
 *  rt_udp_v4_lookup
 */
struct rtsocket *rt_udp_v4_lookup(u32 daddr, u16 dport)
{
    struct list_head *entry;
    struct rtsocket  *sk = NULL;

    rtos_res_lock(&udp_socket_base_lock);

    list_for_each(entry, &udp_sockets) {
        sk = list_entry(entry, struct rtsocket, list_entry);
        if ((sk->prot.inet.sport == dport) &&
            ((sk->prot.inet.saddr == INADDR_ANY) ||
             (sk->prot.inet.saddr == daddr))) {
            rt_socket_reference(sk);
            break;
        }
    }

    rtos_res_unlock(&udp_socket_base_lock);

    return sk;
}



/***
 *  rt_udp_bind - bind socket to local address
 *  @s:     socket
 *  @addr:  local address
 */
int rt_udp_bind(struct rtsocket *s, struct sockaddr *addr, socklen_t addrlen)
{
    struct sockaddr_in *usin = (struct sockaddr_in *)addr;

    if ((s->state!=TCP_CLOSE) || (addrlen<(int)sizeof(struct sockaddr_in)) ||
        ((usin->sin_port & auto_port_mask) == auto_port_start))
        return -EINVAL;

    /* set the source-addr */
    s->prot.inet.saddr = usin->sin_addr.s_addr;

    /* set source port, if not set by user */
    if((s->prot.inet.sport = usin->sin_port) == 0)
        s->prot.inet.sport = htons(auto_port_start + (s->fd & (RT_SOCKETS-1)));

    return 0;
}



/***
 *  rt_udp_connect
 */
int rt_udp_connect(struct rtsocket *s, const struct sockaddr *serv_addr, socklen_t addrlen)
{
    struct sockaddr_in *usin = (struct sockaddr_in *) serv_addr;

    if ( (s->state!=TCP_CLOSE) || (addrlen < (int)sizeof(struct sockaddr_in)) )
        return -EINVAL;
    if ( (usin->sin_family) && (usin->sin_family!=AF_INET) ) {
        s->prot.inet.saddr = INADDR_ANY;
        s->prot.inet.daddr = INADDR_ANY;
        s->state           = TCP_CLOSE;
        return -EAFNOSUPPORT;
    }
    s->state           = TCP_ESTABLISHED;
    s->prot.inet.daddr = usin->sin_addr.s_addr;
    s->prot.inet.dport = usin->sin_port;

#ifdef DEBUG
    rtos_print("connect socket to %x:%d\n", ntohl(s->prot.inet.daddr),
               ntohs(s->prot.inet.dport));
#endif

    return 0;
}



/***
 *  rt_udp_listen
 */
int rt_udp_listen(struct rtsocket *s, int backlog)
{
    /* UDP = connectionless */
    return 0;
}



/***
 *  rt_udp_accept
 */
int rt_udp_accept(struct rtsocket *s, struct sockaddr *addr, socklen_t *addrlen)
{
    /* UDP = connectionless */
    return 0;
}



/***
 *  rt_udp_recvmsg
 */
int rt_udp_recvmsg(struct rtsocket *s, struct msghdr *msg, size_t len, int flags)
{
    size_t copied = 0;
    struct rtskb *skb, *first_skb;
    size_t block_size;
    struct udphdr *uh;
    size_t data_len;
    struct sockaddr_in *sin;
    int ret;


    /* block on receive event */
    if (((s->flags & RT_SOCK_NONBLOCK) == 0) && ((flags & MSG_DONTWAIT) == 0))
        while ((skb = rtskb_dequeue_chain(&s->incoming)) == NULL) {
            if (!RTOS_TIME_IS_ZERO(&s->timeout)) {
                ret = rtos_event_sem_wait_timed(&s->wakeup_event, &s->timeout);
                if (ret == RTOS_EVENT_TIMEOUT)
                    return -ETIMEDOUT;
            } else
                ret = rtos_event_sem_wait(&s->wakeup_event);

            if (RTOS_EVENT_ERROR(ret))
                return -ENOTSOCK;
        }
    else {
        skb = rtskb_dequeue_chain(&s->incoming);
        if (skb == NULL)
            return 0;
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

    if ((flags & MSG_PEEK) == 0)
        kfree_rtskb(first_skb);
    else {
        __rtskb_push(first_skb, sizeof(struct udphdr));
        rtskb_queue_head(&s->incoming, first_skb);
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
int rt_udp_sendmsg(struct rtsocket *s, const struct msghdr *msg, size_t len, int flags)
{
    int ulen = len + sizeof(struct udphdr);

    struct udpfakehdr ufh;
    struct rt_rtable *rt = NULL;

    u32 daddr;
    u16 dport;
    int err;

    if ((len < 0) || (len > 0xFFFF-sizeof(struct iphdr)-sizeof(struct udphdr)))
        return -EMSGSIZE;

    if (flags & MSG_OOB)   /* Mirror BSD error message compatibility */
        return -EOPNOTSUPP;

    if (flags & ~(MSG_DONTROUTE|MSG_DONTWAIT) )
        return -EINVAL;

    if ((msg->msg_name) && (msg->msg_namelen==sizeof(struct sockaddr_in))) {
        struct sockaddr_in *usin = (struct sockaddr_in*) msg->msg_name;

        if ((usin->sin_family!=AF_INET) && (usin->sin_family!=AF_UNSPEC))
            return -EINVAL;

        daddr = usin->sin_addr.s_addr;
        dport = usin->sin_port;
    } else {
        if (s->state != TCP_ESTABLISHED)
            return -ENOTCONN;

        daddr = s->prot.inet.daddr;
        dport = s->prot.inet.dport;
    }

#ifdef DEBUG
    rtos_print("sendmsg to %x:%d\n", ntohl(daddr), ntohs(dport));
#endif
    if ((daddr==0) || (dport==0))
        return -EINVAL;

    err = rt_ip_route_output(&rt, daddr, s->prot.inet.saddr);
    if (err)
        goto out;

    /* we found a route, remember the routing dest-addr could be the netmask */
    ufh.saddr     = rt->rt_src;
    ufh.daddr     = daddr;
    ufh.uh.source = s->prot.inet.sport;
    ufh.uh.dest   = dport;
    ufh.uh.len    = htons(ulen);
    ufh.uh.check  = 0;
    ufh.iov       = msg->msg_iov;
    ufh.iovlen    = msg->msg_iovlen;
    ufh.wcheck    = 0;

    err = rt_ip_build_xmit(s, rt_udp_getfrag, &ufh, ulen, rt, flags);

out:
    if (!err)
        return len;
    else
        return err;
}



/***
 *  rt_udp_close
 */
int rt_udp_close(struct rtsocket *s)
{
    struct rtskb *del;


    s->state=TCP_CLOSE;

    rtos_res_lock(&udp_socket_base_lock);
    list_del(&s->list_entry);
    rtos_res_unlock(&udp_socket_base_lock);

    /* cleanup already collected fragments */
    rt_ip_frag_invalidate_socket(s);

    /* free packets in incoming queue */
    while ((del = rtskb_dequeue(&s->incoming)) != NULL)
        kfree_rtskb(del);

    return 0;
}



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
    struct udphdr   *uh   = skb->h.uh;
    unsigned short  ulen  = ntohs(uh->len);
    u32             saddr = skb->nh.iph->saddr;
    u32             daddr = skb->nh.iph->daddr;


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

    /* find the destination socket */
    skb->sk = rt_udp_v4_lookup(daddr, uh->dest);

    return skb->sk;
}



/***
 *  rt_udp_rcv
 */
int rt_udp_rcv (struct rtskb *skb)
{
    struct rtsocket *rtsk = skb->sk;


    rtskb_queue_tail(&rtsk->incoming, skb);
    rtos_event_sem_signal(&rtsk->wakeup_event);
    if (rtsk->wakeup != NULL)
        rtsk->wakeup(rtsk->fd, rtsk->wakeup_arg);

    return 0;
}



/***
 *  rt_udp_rcv_err
 */
void rt_udp_rcv_err (struct rtskb *skb)
{
    rtos_print("RTnet: rt_udp_rcv err\n");
}



static struct rtsocket_ops rt_udp_socket_ops = {
    bind:        rt_udp_bind,
    connect:     rt_udp_connect,
    listen:      rt_udp_listen,
    accept:      rt_udp_accept,
    recvmsg:     rt_udp_recvmsg,
    sendmsg:     rt_udp_sendmsg,
    close:       rt_udp_close,
    setsockopt:  rt_ip_setsockopt,
    getsockname: rt_ip_getsockname
};



/***
 *  rt_udp_socket - create a new UDP-Socket
 *  @s: socket
 */
int rt_udp_socket(struct rtsocket *s)
{
    s->family          = PF_INET;
    s->protocol        = IPPROTO_UDP;
    s->ops             = &rt_udp_socket_ops;
    s->prot.inet.saddr = INADDR_ANY;
    s->prot.inet.sport = htons(auto_port_start + (s->fd & (RT_SOCKETS-1)));

    /* add to udp-socket-list */
    rtos_res_lock(&udp_socket_base_lock);
    list_add_tail(&s->list_entry, &udp_sockets);
    rtos_res_unlock(&udp_socket_base_lock);

    return s->fd;
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
    auto_port_start &= (auto_port_mask & 0xFFFF);
    auto_port_mask  |= 0xFFFF0000;

    rtos_res_lock_init(&udp_socket_base_lock);
    rt_inet_add_protocol(&udp_protocol);
}



/***
 *  rt_udp_release
 */
void rt_udp_release(void)
{
    rt_inet_del_protocol(&udp_protocol);
    rtos_res_lock_delete(&udp_socket_base_lock);
}
