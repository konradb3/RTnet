/* ipv4/icmp.c
 *
 * rtnet - real-time networking subsystem
 * Copyright (C) 1999,2000 Zentropic Computing, LLC
 *               2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *               2002 Vinay Sridhara <vinaysridhara@yahoo.com>
 *               2003 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/types.h>
#include <linux/icmp.h>
#include <net/checksum.h>

#include <rtskb.h>
#include <rtnet_socket.h>
#include <ipv4/icmp.h>
#include <ipv4/ip_output.h>
#include <ipv4/ip_sock.h>
#include <ipv4/protocol.h>
#include <ipv4/route.h>


struct rt_icmp_control
{
    void    (*handler)(struct rtskb *skb);
    short   error;      /* This ICMP is classed as an error message */
};


/**
 *  Socket for icmp replies
 *  It is not part of the socket pool. It may furthermore be used concurrently
 *  by multiple tasks because all fields are static excect skb_pool, but that
 *  is spin lock protected.
 */
static struct rtsocket reply_socket;

/**
 *  icmp_sockets
 *  the registered sockets from any server
 */
static struct rtsocket  *icmp_sockets;
static rtos_spinlock_t  icmp_socket_base_lock = RTOS_SPIN_LOCK_UNLOCKED;

static struct rtsocket_ops rt_icmp_socket_ops = {
    bind:       NULL,
    connect:    NULL,
    listen:     NULL,
    accept:     NULL,
    recvmsg:    NULL,
    sendmsg:    NULL,
    close:      NULL,
    setsockopt: &rt_ip_setsockopt
};

static struct rt_icmp_control rt_icmp_pointers[NR_ICMP_TYPES+1];

/***
 * Structure for sending the icmp packets
 */

struct icmp_bxm
{
    struct rtskb *skb;
    int offset;
    int data_len;

    unsigned int csum;

    struct{
        struct icmphdr  icmph;
        __u32           times[3];
    }data;
    int head_len;
    struct ip_options replyopts;
    unsigned char optbuf[40];
};



/***
 *  rt_icmp_discard - dummy function
 */
static void rt_icmp_discard(struct rtskb *skb)
{
}



/***
 *  rt_icmp_unreach - dummy function
 */
static void rt_icmp_unreach(struct rtskb *skb)
{
}



/***
 *  rt_icmp_redirect - dummy function
 */
static void rt_icmp_redirect(struct rtskb *skb)
{
}



static int rt_icmp_glue_bits(const void *p, char *to, unsigned int offset, unsigned int fraglen)
{
    struct icmp_bxm *icmp_param = (struct icmp_bxm *)p;
    struct icmphdr *icmph;
    unsigned long csum;
    unsigned int header_len;

    header_len = sizeof(struct icmphdr);

    if(offset)
    {
        icmp_param->csum = rtskb_copy_and_csum_bits(icmp_param->skb,
                icmp_param->offset+(offset-icmp_param->head_len),
                to, fraglen, icmp_param->csum);
        return 0;
    }

    csum = csum_partial_copy_nocheck((void *)&icmp_param->data,
            to, icmp_param->head_len,
            icmp_param->csum);

    csum = rtskb_copy_and_csum_bits(icmp_param->skb,
                    icmp_param->offset,
                    to+icmp_param->head_len,
                    fraglen-icmp_param->head_len,
                    csum);

    icmph = (struct icmphdr *)to;

    icmph->checksum = csum_fold(csum);

    return 0;
}



/***
 *  common reply function
 */
static void rt_icmp_reply(struct icmp_bxm *icmp_param, struct rtskb *skb)
{
    struct ipcm_cookie  ipc;
    struct rt_rtable    *rt = NULL;
    u32                 saddr;
    u32                 daddr;
    int                 err;

    saddr = skb->nh.iph->daddr;

    rt = (struct rt_rtable*)skb->dst;

    RTNET_ASSERT(rt != NULL,
                 rtos_print("RTnet: rt_icmp_reply() error in route table\n");
                 return;);

    icmp_param->data.icmph.checksum = 0;
    icmp_param->csum = 0;


    daddr = ipc.addr = rt->rt_dst;
    ipc.opt = &icmp_param->replyopts;

    err = rt_ip_route_output(&rt, daddr, saddr);

    if (err)
        return;

    err = rt_ip_build_xmit(&reply_socket, rt_icmp_glue_bits, icmp_param,
            icmp_param->data_len+sizeof(struct icmphdr),
            rt, MSG_DONTWAIT);

    RTNET_ASSERT(err == 0,
                 rtos_print("RTnet: rt_icmp_reply() error in xmit\n"););
}



/***
 *  rt_icmp_echo -
 */
static void rt_icmp_echo(struct rtskb *skb)
{
    struct icmp_bxm icmp_param;

    icmp_param.data.icmph = *skb->h.icmph;
    icmp_param.data.icmph.type = ICMP_ECHOREPLY;
    icmp_param.skb = skb;
    icmp_param.offset = 0;
    icmp_param.data_len = skb->len;
    icmp_param.head_len = sizeof(struct icmphdr);

    rt_icmp_reply(&icmp_param, skb);

    return;
}



/***
 *  rt_icmp_timestamp -
 */
static void rt_icmp_timestamp(struct rtskb *skb)
{
}



/***
 *  rt_icmp_address -
 */
static void rt_icmp_address(struct rtskb *skb)
{
}



/***
 *  rt_icmp_address_reply
 */
static void rt_icmp_address_reply(struct rtskb *skb)
{
}



/***
 *  rt_icmp_socket
 */
int rt_icmp_socket(struct rtsocket *sock)
{
    unsigned long flags;

    sock->family    = AF_INET;
    sock->type      = SOCK_DGRAM;
    sock->protocol  = IPPROTO_ICMP;
    sock->ops       = &rt_icmp_socket_ops;

    /* add to icmp-socket-list */
    rtos_spin_lock_irqsave(&icmp_socket_base_lock, flags);
    sock->next = icmp_sockets;
    if (icmp_sockets != NULL)
        icmp_sockets->prev = sock;
    icmp_sockets = sock;
    rtos_spin_unlock_irqrestore(&icmp_socket_base_lock, flags);

    return sock->fd;
}



static struct rt_icmp_control rt_icmp_pointers[NR_ICMP_TYPES+1] =
{
    /* ECHO REPLY (0) */
    { rt_icmp_discard,          0 },
    { rt_icmp_discard,          1 },
    { rt_icmp_discard,          1 },

    /* DEST UNREACH (3) */
    { rt_icmp_unreach,          1 },

    /* SOURCE QUENCH (4) */
    { rt_icmp_unreach,          1 },

    /* REDIRECT (5) */
    { rt_icmp_redirect,         1 },
    { rt_icmp_discard,          1 },
    { rt_icmp_discard,          1 },

    /* ECHO (8) */
    { rt_icmp_echo,             0 },
    { rt_icmp_discard,          1 },
    { rt_icmp_discard,          1 },

    /* TIME EXCEEDED (11) */
    { rt_icmp_unreach,          1 },

    /* PARAMETER PROBLEM (12) */
    { rt_icmp_unreach,          1 },

    /* TIMESTAMP (13) */
    { rt_icmp_timestamp,        0 },

    /* TIMESTAMP REPLY (14) */
    { rt_icmp_discard,          0 },

    /* INFO (15) */
    { rt_icmp_discard,          0 },

    /* INFO REPLY (16) */
    { rt_icmp_discard,          0 },

    /* ADDR MASK (17) */
    { rt_icmp_address,          0 },

    /* ADDR MASK REPLY (18) */
    { rt_icmp_address_reply,    0 }
};



/***
 *  rt_icmp_dest_pool
 */
struct rtsocket *rt_icmp_dest_socket(struct rtskb *skb)
{
    /* Note that the socket's refcount is not used by this protocol.
     * The socket returned here is static and not part of the global pool. */
    return &reply_socket;
}



/***
 *  rt_icmp_rcv
 */
int rt_icmp_rcv(struct rtskb *skb)
{
    struct icmphdr *icmpHdr = skb->h.icmph;
    unsigned int length = skb->len;

    if (length < sizeof(struct icmphdr))
    {
        rtos_print("RTnet: improper length in icmp packet\n");
        goto cleanup;
    }

    if (ip_compute_csum((unsigned char *)icmpHdr, length))
    {
        rtos_print("RTnet: invalid checksum in icmp packet %d\n", length);
        goto cleanup;
    }

    if (!rtskb_pull(skb, sizeof(struct icmphdr)))
    {
        rtos_print("RTnet: pull failed %p\n", (skb->sk));
        goto cleanup;
    }


    if (icmpHdr->type > NR_ICMP_TYPES)
    {
        rtos_print("RTnet: invalid icmp type\n");
        goto cleanup;
    }

    /* sane packet, process it */

    (rt_icmp_pointers[icmpHdr->type].handler)(skb);

cleanup:
    kfree_rtskb(skb);
    return 0;
}



/***
 *  rt_icmp_rcv_err
 */
void rt_icmp_rcv_err(struct rtskb *skb)
{
    rtos_print("RTnet: rt_icmp_rcv err\n");
}



/***
 *  ICMP-Initialisation
 */
static struct rtinet_protocol icmp_protocol = {
    protocol:       IPPROTO_ICMP,
    dest_socket:    &rt_icmp_dest_socket,
    rcv_handler:    &rt_icmp_rcv,
    err_handler:    &rt_icmp_rcv_err,
    init_socket:    &rt_icmp_socket
};



/***
 *  rt_icmp_init
 */
void __init rt_icmp_init(void)
{
    unsigned int skbs;

    reply_socket.protocol = IPPROTO_ICMP;
    reply_socket.prot.inet.tos = 0;
    reply_socket.priority = RT_ICMP_REPLY_PRIO;

    /* create the rtskb pool */
    skbs = rtskb_pool_init(&reply_socket.skb_pool, ICMP_REPLY_POOL_SIZE);
    if (skbs < ICMP_REPLY_POOL_SIZE)
        printk("RTnet: allocated only %d icmp rtskbs\n", skbs);

    icmp_sockets=NULL;
    rt_inet_add_protocol(&icmp_protocol);
}



/***
 *  rt_icmp_release
 */
void rt_icmp_release(void)
{
    rt_inet_del_protocol(&icmp_protocol);
    icmp_sockets=NULL;
    rtskb_pool_release(&reply_socket.skb_pool);
}
