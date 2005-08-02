/***
 *
 *  packet/af_packet.c
 *
 *  RTnet - real-time networking subsystem
 *  Copyright (C) 2003, 2004 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/if_arp.h> /* ARPHRD_ETHER */
#include <linux/sched.h>

#include <rtnet_iovec.h>
#include <rtnet_socket.h>
#include <stack_mgr.h>


/***
 *  rt_packet_rcv
 */
int rt_packet_rcv(struct rtskb *skb, struct rtpacket_type *pt)
{
    struct rtsocket *sock = (struct rtsocket *)(((u8 *)pt) -
                                ((u8 *)&((struct rtsocket *)0)->prot.packet));
    int             ifindex = sock->prot.packet.ifindex;
    void            (*callback_func)(struct rtdm_dev_context *, void *);
    void            *callback_arg;
    unsigned long   flags;


    if (((ifindex != 0) && (ifindex != skb->rtdev->ifindex)) ||
        (rtskb_acquire(skb, &sock->skb_pool) != 0))
        kfree_rtskb(skb);
    else {
        rtdev_reference(skb->rtdev);
        rtskb_queue_tail(&sock->incoming, skb);
        rtos_sem_up(&sock->pending_sem);

        rtos_spin_lock_irqsave(&sock->param_lock, flags);
        callback_func = sock->callback_func;
        callback_arg  = sock->callback_arg;
        rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

        if (callback_func)
            callback_func(rt_socket_context(sock), callback_arg);
    }
    return 0;
}



/***
 *  rt_packet_bind
 */
int rt_packet_bind(struct rtsocket *sock, const struct sockaddr *addr,
                   socklen_t addrlen)
{
    struct sockaddr_ll      *sll = (struct sockaddr_ll *)addr;
    struct rtpacket_type    *pt  = &sock->prot.packet.packet_type;
    int                     new_type;
    int                     ret;
    unsigned long           flags;


    if ((addrlen < (int)sizeof(struct sockaddr_ll)) ||
        (sll->sll_family != AF_PACKET))
        return -EINVAL;

    new_type = (sll->sll_protocol != 0) ? sll->sll_protocol : sock->protocol;

    rtos_spin_lock_irqsave(&sock->param_lock, flags);

    /* release exisiting binding */
    if ((pt->type != 0) && ((ret = rtdev_remove_pack(pt)) < 0)) {
        rtos_spin_unlock_irqrestore(&sock->param_lock, flags);
        return ret;
    }

    pt->type = new_type;
    sock->prot.packet.ifindex = sll->sll_ifindex;

    /* if protocol is non-zero, register the packet type */
    if (sock->protocol != 0) {
        pt->name        = "PACKET_SOCKET";
        pt->handler     = rt_packet_rcv;
        pt->err_handler = NULL;

        ret = rtdev_add_pack(pt);
    } else
        ret = 0;

    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    return ret;
}



/***
 *  rt_packet_getsockname
 */
int rt_packet_getsockname(struct rtsocket *sock, struct sockaddr *addr,
                          socklen_t *addrlen)
{
    struct sockaddr_ll  *sll = (struct sockaddr_ll*)addr;
    struct rtnet_device *rtdev;
    unsigned long       flags;


    if (*addrlen < sizeof(struct sockaddr_ll))
        return -EINVAL;

    rtos_spin_lock_irqsave(&sock->param_lock, flags);

    sll->sll_family   = AF_PACKET;
    sll->sll_ifindex  = sock->prot.packet.ifindex;
    sll->sll_protocol = sock->protocol;

    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    rtdev = rtdev_get_by_index(sll->sll_ifindex);
    if (rtdev != NULL) {
        sll->sll_hatype = rtdev->type;
        sll->sll_halen  = rtdev->addr_len;

        memcpy(sll->sll_addr, rtdev->dev_addr, rtdev->addr_len);

        rtdev_dereference(rtdev);
    } else {
        sll->sll_hatype = 0;
        sll->sll_halen  = 0;
    }

    *addrlen = sizeof(struct sockaddr_ll);

    return 0;
}



/***
 * rt_packet_socket - initialize a packet socket
 */
int rt_packet_socket(struct rtdm_dev_context *context,
                     rtdm_user_info_t *user_info, int protocol)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    int             ret;


    if ((ret = rt_socket_init(context)) != 0)
        return ret;

    sock->prot.packet.packet_type.type = protocol;
    sock->prot.packet.ifindex          = 0;

    /* if protocol is non-zero, register the packet type */
    if (protocol != 0) {
        sock->prot.packet.packet_type.name        = "PACKET_SOCKET";
        sock->prot.packet.packet_type.handler     = rt_packet_rcv;
        sock->prot.packet.packet_type.err_handler = NULL;

        if ((ret = rtdev_add_pack(&sock->prot.packet.packet_type)) < 0) {
            rt_socket_cleanup(context);
            return ret;
        }
    }

    sock->protocol = protocol;

    return 0;
}



/***
 *  rt_packet_close
 */
int rt_packet_close(struct rtdm_dev_context *context,
                    rtdm_user_info_t *user_info)
{
    struct rtsocket         *sock = (struct rtsocket *)&context->dev_private;
    struct rtpacket_type    *pt = &sock->prot.packet.packet_type;
    struct rtskb            *del;
    int                     ret = 0;
    unsigned long           flags;


    rtos_spin_lock_irqsave(&sock->param_lock, flags);

    if ((pt->type != 0) && ((ret = rtdev_remove_pack(pt)) == 0))
        pt->type = 0;

    rtos_spin_unlock_irqrestore(&sock->param_lock, flags);

    /* free packets in incoming queue */
    while ((del = rtskb_dequeue(&sock->incoming)) != NULL) {
        rtdev_dereference(del->rtdev);
        kfree_rtskb(del);
    }

    if (ret == 0)
        ret = rt_socket_cleanup(context);

    return ret;
}



/***
 *  rt_packet_ioctl
 */
int rt_packet_ioctl(struct rtdm_dev_context *context,
                    rtdm_user_info_t *user_info, int request, void *arg)
{
    struct rtsocket *sock = (struct rtsocket *)&context->dev_private;
    struct _rtdm_setsockaddr_args *setaddr = arg;
    struct _rtdm_getsockaddr_args *getaddr = arg;


    /* fast path for common socket IOCTLs */
    if (_IOC_TYPE(request) == RTIOC_TYPE_NETWORK)
        return rt_socket_common_ioctl(context, user_info, request, arg);

    switch (request) {
        case _RTIOC_BIND:
            return rt_packet_bind(sock, setaddr->addr, setaddr->addrlen);

        case _RTIOC_GETSOCKNAME:
            return rt_packet_getsockname(sock, getaddr->addr,
                                         getaddr->addrlen);

        default:
            return rt_socket_if_ioctl(context, user_info, request, arg);
    }
}



/***
 *  rt_packet_recvmsg
 */
ssize_t rt_packet_recvmsg(struct rtdm_dev_context *context,
                          rtdm_user_info_t *user_info, struct msghdr *msg,
                          int msg_flags)
{
    struct rtsocket     *sock = (struct rtsocket *)&context->dev_private;
    size_t              len   = rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    size_t              copy_len;
    size_t              real_len;
    struct rtskb        *skb;
    struct ethhdr       *eth;
    struct sockaddr_ll  *sll;
    int                 ret;
    nanosecs_t          timeout = sock->timeout;


    /* non-blocking receive? */
    if (test_bit(RT_SOCK_NONBLOCK, &context->context_flags) ||
        (msg_flags & MSG_DONTWAIT))
        timeout = -1;

    ret = rtos_sem_timeddown(&sock->pending_sem, timeout);
    if (unlikely(ret < 0)) {
        if (ret == -EWOULDBLOCK)
            ret = -EAGAIN;
        else if (ret != -ETIMEDOUT)
            ret = -ENOTSOCK;
        return ret;
    }

    skb = rtskb_dequeue_chain(&sock->incoming);
    RTNET_ASSERT(skb != NULL, return -EFAULT;);

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

    if ((msg_flags & MSG_PEEK) == 0) {
        rtdev_dereference(skb->rtdev);
        kfree_rtskb(skb);
    } else
        rtskb_queue_head(&sock->incoming, skb);

    return real_len;
}



/***
 *  rt_packet_sendmsg
 */
ssize_t rt_packet_sendmsg(struct rtdm_dev_context *context,
                          rtdm_user_info_t *user_info,
                          const struct msghdr *msg, int flags)
{
    struct rtsocket     *sock = (struct rtsocket *)&context->dev_private;
    size_t              len   = rt_iovec_len(msg->msg_iov, msg->msg_iovlen);
    struct sockaddr_ll  *sll  = (struct sockaddr_ll*)msg->msg_name;
    struct rtnet_device *rtdev;
    struct rtskb        *rtskb;
    int                 ret = 0;


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

    /* If an RTmac discipline is active, this becomes a pure sanity check to
       avoid writing beyond rtskb boundaries. The hard check is then performed
       upon rtdev_xmit() by the discipline's xmit handler. */
    if (len > rtdev->mtu) {
        ret = -EMSGSIZE;
        goto err;
    }

    if (sll->sll_halen != rtdev->addr_len) {
        ret = -EINVAL;
        goto err;
    }

    rtskb_reserve(rtskb, rtdev->hard_header_len);

    rt_memcpy_fromkerneliovec(rtskb_put(rtskb, len), msg->msg_iov, len);

    rtskb->rtdev    = rtdev;
    rtskb->priority = sock->priority;

    if (rtdev->hard_header) {
        ret = rtdev->hard_header(rtskb, rtdev, ntohs(sll->sll_protocol),
                                 sll->sll_addr, rtdev->dev_addr, rtskb->len);
        if (ret < 0)
            goto err;
    }

    if ((rtdev->flags & IFF_UP) != 0) {
        if ((ret = rtdev_xmit(rtskb)) == 0)
            ret = len;
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



static struct rtdm_device   packet_proto_dev = {
    struct_version:     RTDM_DEVICE_STRUCT_VER,

    device_flags:       RTDM_PROTOCOL_DEVICE,
    context_size:       sizeof(struct rtsocket),

    protocol_family:    PF_PACKET,
    socket_type:        SOCK_DGRAM,

    socket_rt:          rt_packet_socket,
    socket_nrt:         rt_packet_socket,

    ops: {
        close_rt:       rt_packet_close,
        close_nrt:      rt_packet_close,
        ioctl_rt:       rt_packet_ioctl,
        ioctl_nrt:      rt_packet_ioctl,
        recvmsg_rt:     rt_packet_recvmsg,
        sendmsg_rt:     rt_packet_sendmsg
    },

    device_class:       RTDM_CLASS_NETWORK,
    device_sub_class:   RTDM_SUBCLASS_RTNET,
    driver_name:        rtnet_rtdm_driver_name,
    peripheral_name:    "Packet Socket Interface",
    provider_name:      rtnet_rtdm_provider_name,

    proc_name:          "PACKET_DGRAM"
};



int rt_packet_proto_init(void)
{
    return rtdm_dev_register(&packet_proto_dev);
}



void rt_packet_proto_release(void)
{
    rtdm_dev_unregister(&packet_proto_dev, 1000);
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
