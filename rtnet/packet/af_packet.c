/* packet/af_packet.c
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

#include <linux/if_arp.h> /* ARPHRD_ETHER */

#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <stack_mgr.h>


int rt_packet_rcv(struct rtskb *skb, struct rtpacket_type *pt);



/***
 *  rt_packet_bind
 */
int rt_packet_bind(struct rtsocket *sock, struct sockaddr *addr,
                   socklen_t addrlen)
{
    struct sockaddr_ll   *sll = (struct sockaddr_ll *)addr;
    struct rtpacket_type *pt  = &sock->prot.packet.packet_type;
    int new_type;
    int ret;


    if ((addrlen < (int)sizeof(struct sockaddr_ll)) ||
        (sll->sll_family != AF_PACKET))
        return -EINVAL;

    new_type = (sll->sll_protocol != 0) ? sll->sll_protocol : sock->protocol;

    /* release exisiting binding */
    if (pt->type != 0) {
        if ((ret = rtdev_remove_pack(pt)) < 0)
            return ret;
    }

    pt->type = new_type;
    sock->prot.packet.ifindex = sll->sll_ifindex;

    /* if protocol is non-zero, register the packet type */
    if (sock->protocol != 0) {
        pt->name        = "PACKET_SOCKET";
        pt->handler     = rt_packet_rcv;
        pt->err_handler = NULL;

        return rtdev_add_pack(pt);
    } else
        return 0;
}



/***
 *  rt_packet_connect
 */
int rt_packet_connect(struct rtsocket *sock, const struct sockaddr *serv_addr,
                      socklen_t addrlen)
{
    return -EOPNOTSUPP;
}



/***
 *  rt_packet_listen
 */
int rt_packet_listen(struct rtsocket *sock, int backlog)
{
    return -EOPNOTSUPP;
}



/***
 *  rt_packet_accept
 */
int rt_packet_accept(struct rtsocket *sock, struct sockaddr *addr,
                     socklen_t *addrlen)
{
    return -EOPNOTSUPP;
}



/***
 *  rt_packet_recvmsg
 */
int rt_packet_recvmsg(struct rtsocket *sock, struct msghdr *msg, size_t len,
                      int flags)
{
    size_t copy_len, real_len;
    struct rtskb *skb;
    struct ethhdr *eth;
    struct sockaddr_ll *sll;
    int ret;


    /* block on receive event */
    if (((sock->flags & RT_SOCK_NONBLOCK) == 0) &&
        ((flags & MSG_DONTWAIT) == 0))
        while ((skb = rtskb_dequeue_chain(&sock->incoming)) == NULL) {
            if (RTOS_TIME_IS_ZERO(&sock->timeout)) {
                ret = rtos_event_wait_timeout(&sock->wakeup_event,
                                              &sock->timeout);
                if (ret == RTOS_EVENT_TIMEOUT)
                    return -ETIMEDOUT;
            } else
                ret = rtos_event_wait(&sock->wakeup_event);

            if (RTOS_EVENT_ERROR(ret))
                return -ENOTSOCK;
        }
    else {
        skb = rtskb_dequeue_chain(&sock->incoming);
        if (skb == NULL)
            return 0;
    }

    eth = skb->mac.ethernet;

    sll = msg->msg_name;

    /* copy the address */
    msg->msg_namelen = sizeof(*sll);
    if (sll != NULL) {
        sll->sll_family   = AF_PACKET;
        sll->sll_protocol = skb->protocol;
        sll->sll_ifindex  = skb->rtdev->ifindex;
        sll->sll_pkttype  = skb->pkt_type;

        /* Ethernet specific */
        sll->sll_hatype   = ARPHRD_ETHER;
        sll->sll_halen    = ETH_ALEN;
        memcpy(sll->sll_addr, eth->h_source, ETH_ALEN);
    }

    copy_len = real_len = skb->len;

    /* The data must not be longer than the available buffer size */
    if (copy_len > len) {
        copy_len = len;
        msg->msg_flags |= MSG_TRUNC;
    }

    /* copy the data */
    rt_memcpy_tokerneliovec(msg->msg_iov, skb->data, copy_len);

    if ((flags & MSG_PEEK) == 0)
        kfree_rtskb(skb);
    else
        rtskb_queue_head(&sock->incoming, skb);

    return real_len;
}



/***
 *  rt_packet_sendmsg
 */
int rt_packet_sendmsg(struct rtsocket *sock, const struct msghdr *msg,
                      size_t len, int flags)
{
    struct sockaddr_ll *sll = (struct sockaddr_ll*)msg->msg_name;
    struct rtnet_device *rtdev;
    struct rtskb *rtskb;
    int ret = 0;


    if (flags & MSG_OOB)   /* Mirror BSD error message compatibility */
        return -EOPNOTSUPP;

    /* a lot of sanity checks */
    if ((flags & ~MSG_DONTWAIT) ||
        (sll == NULL) || (msg->msg_namelen != sizeof(struct sockaddr_ll)) ||
        ((sll->sll_family != AF_PACKET) && (sll->sll_family != AF_UNSPEC)) ||
        (sll->sll_ifindex <= 0))
        return -EINVAL;

    if ((rtdev = rtdev_get_by_index(sll->sll_ifindex)) == NULL)
        return -ENODEV;

    rtskb = alloc_rtskb(rtdev->hard_header_len + len, &sock->skb_pool);
    if (rtskb == NULL) {
        ret = -ENOBUFS;
        goto out;
    }

    if ((len < 0) || (len > rtdev->mtu)) {
        ret = -EMSGSIZE;
        goto err;
    }

    if (sll->sll_halen != rtdev->addr_len) {
        ret = -EINVAL;
        goto err;
    }

    rtskb_reserve(rtskb, rtdev->hard_header_len);

    rt_memcpy_fromkerneliovec(rtskb_put(rtskb, len), msg->msg_iov, len);

    rtskb->rtdev = rtdev;

    if (rtdev->hard_header) {
        ret = rtdev->hard_header(rtskb, rtdev, ntohs(sll->sll_protocol),
                                 sll->sll_addr, rtdev->dev_addr, rtskb->len);
        if (ret < 0)
            goto err;
    }

    if ((rtdev->flags & IFF_UP) != 0) {
        if (rtdev_xmit(rtskb) == 0)
            ret = len;
        else
            ret = -EAGAIN;
    } else {
        ret = -ENETDOWN;
        goto err;
    }

out:
    rtdev_dereference(rtdev);
    return ret;

err:
    kfree_rtskb(rtskb);
    rtdev_dereference(rtdev);
    return ret;
}



/***
 *  rt_packet_close
 */
int rt_packet_close(struct rtsocket *sock)
{
    struct rtpacket_type *pt = &sock->prot.packet.packet_type;
    struct rtskb *del;
    int ret = 0;


    if (pt->type != 0) {
        if ((ret = rtdev_remove_pack(pt)) == 0)
            pt->type = 0;
    }

    /* free packets in incoming queue */
    while ((del = rtskb_dequeue(&sock->incoming)) != NULL)
        kfree_rtskb(del);

    return ret;
}



/***
 *  rt_packet_setsockopt
 */
int rt_packet_setsockopt(struct rtsocket *s, int level, int optname,
                         const void *optval, socklen_t optlen)
{
    return -ENOPROTOOPT;
}



/***
 *  rt_packet_rcv
 */
int rt_packet_rcv(struct rtskb *skb, struct rtpacket_type *pt)
{
    struct rtsocket *sock = (struct rtsocket *)(((u8 *)pt) -
                                ((u8 *)&((struct rtsocket *)0)->prot.packet));

    if (((sock->prot.packet.ifindex != 0) &&
         (sock->prot.packet.ifindex != skb->rtdev->ifindex)) ||
        (rtskb_acquire(skb, &sock->skb_pool) != 0))
        kfree_rtskb(skb);
    else {
        rtskb_queue_tail(&sock->incoming, skb);
        rtos_event_signal(&sock->wakeup_event);
        if (sock->wakeup != NULL)
            sock->wakeup(sock->fd, sock->wakeup_arg);
    }
    return 0;
}



static struct rtsocket_ops rt_packet_socket_ops = {
    bind:       &rt_packet_bind,
    connect:    &rt_packet_connect,
    listen:     &rt_packet_listen,
    accept:     &rt_packet_accept,
    recvmsg:    &rt_packet_recvmsg,
    sendmsg:    &rt_packet_sendmsg,
    close:      &rt_packet_close,
    setsockopt: &rt_packet_setsockopt
};


/***
 * rt_packet_socket - initialize a packet socket
 * @sock: socket structure
 * @protocol: protocol id
 */
int rt_packet_socket(struct rtsocket *sock, int protocol)
{
    int ret;


    /* only datagram-sockets */
    if (sock->type != SOCK_DGRAM)
        return -EAFNOSUPPORT;

    sock->prot.packet.packet_type.type = protocol;
    sock->prot.packet.ifindex          = 0;

    /* if protocol is non-zero, register the packet type */
    if (protocol != 0) {
        sock->prot.packet.packet_type.name        = "PACKET_SOCKET";
        sock->prot.packet.packet_type.handler     = rt_packet_rcv;
        sock->prot.packet.packet_type.err_handler = NULL;

        if ((ret = rtdev_add_pack(&sock->prot.packet.packet_type)) < 0)
            return ret;
    }

    sock->family   = PF_PACKET;
    sock->protocol = protocol;
    sock->ops      = &rt_packet_socket_ops;

    return sock->fd;
}



/**********************************************************
 * Utilities                                              *
 **********************************************************/

int hex2int(char hex_char)
{
    if ((hex_char >= '0') && (hex_char <= '9'))
        return hex_char - '0';
    else if ((hex_char >= 'a') && (hex_char <= 'f'))
        return hex_char - 'a' + 10;
    else if ((hex_char >= 'A') && (hex_char <= 'F'))
        return hex_char - 'A' + 10;
    else
        return -EINVAL;
}



int rt_eth_aton(char *addr_buf, const char *mac)
{
    int i = 0;
    int nibble;


    while (1) {
        if (*mac == 0)
            return -EINVAL;

        if ((nibble = hex2int(*mac++)) < 0)
            return nibble;
        *addr_buf = nibble << 4;

        if (*mac == 0)
            return -EINVAL;

        if ((nibble = hex2int(*mac++)) < 0)
            return nibble;
        *addr_buf++ |= nibble;

        if (++i == 6)
            break;

        if ((*mac == 0) || (*mac++ != ':'))
            return -EINVAL;

    }
    return 0;
}
