/* ip_input.c
 *
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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

#include <net/checksum.h>
#include <net/ip.h>

#include <rtskb.h>
#include <ipv4/ip_fragment.h>
#include <ipv4/protocol.h>
#include <ipv4/route.h>

static int (*ip_fallback_handler)(struct rtskb *skb) = 0;


/* This function can be used to register a fallback handler for incoming
 * ip frames. Typically this is done to move over to the standard linux
 * ip protocol (e.g. for handling TCP).
 * By calling this function with the argument set to zero, the function is
 * unregistered.
 * Note: Only one function can be registered! */
int rt_ip_register_fallback( int (*callback)(struct rtskb *skb))
{
    ip_fallback_handler = callback;
    return 0;
}



/***
 *  rt_ip_local_deliver
 */
static inline int rt_ip_local_deliver(struct rtskb *skb)
{
    struct iphdr *iph       = skb->nh.iph;
    unsigned short protocol = iph->protocol;
    struct rtinet_protocol *ipprot;
    struct rtskb_queue *cmppool;
    int ret;


    ipprot = rt_inet_protocols[rt_inet_hashkey(protocol)];

    /* Check if we are supporting the protocol */
    if ((ipprot != NULL) && (ipprot->protocol == protocol))
    {
        __rtskb_pull(skb, iph->ihl*4);

        /* Point into the IP datagram, just past the header. */
        skb->h.raw = skb->data;

        /* Reassemble IP fragments */
        if (iph->frag_off & htons(IP_MF|IP_OFFSET)) {
            skb = rt_ip_defrag(skb, ipprot);
            if (!skb)
                return 0;
        } else {
            /* Acquire the rtskb at the expense of the protocol pool */
            cmppool = ipprot->get_pool(skb);
            if ((cmppool == NULL) || (rtskb_acquire(skb, cmppool) != 0)) {
                kfree_rtskb(skb);
                return 0;
            }
        }

        /* deliver the packet to the next layer*/
        ret = ipprot->rcv_handler(skb);
    } else {
        /* If a fallback handler for IP protocol has been installed,
         * call it! */
        if (ip_fallback_handler) {
            ret = ip_fallback_handler(skb);
            if (ret) {
                rt_printk("RTnet: fallback handler failed\n");
            }
        } else {
            rt_printk("RTnet: no protocol found\n");
            kfree_rtskb(skb);
            ret = 0;
        }
    }

    return ret;
}



/***
 *  rt_ip_rcv
 */
int rt_ip_rcv(struct rtskb *skb, struct rtpacket_type *pt)
{
    struct iphdr *iph;
    __u32 len;

    /* When the interface is in promisc. mode, drop all the crap
     * that it receives, do not try to analyse it.
     */
    if (skb->pkt_type == PACKET_OTHERHOST)
        goto drop;

    iph = skb->nh.iph;

    /*
     *  RFC1122: 3.1.2.2 MUST silently discard any IP frame that fails the checksum.
     *
     *  Is the datagram acceptable?
     *
     *  1.  Length at least the size of an ip header
     *  2.  Version of 4
     *  3.  Checksums correctly. [Speed optimisation for later, skip loopback checksums]
     *  4.  Doesn't have a bogus length
     */
    if (iph->ihl < 5 || iph->version != 4)
        goto drop;

    if ( ip_fast_csum((u8 *)iph, iph->ihl)!=0 )
        goto drop;

    len = ntohs(iph->tot_len);
    if ( (skb->len<len) || (len<((__u32)iph->ihl<<2)) )
        goto drop;

    rtskb_trim(skb, len);

    if (skb->dst == NULL)
        if ( rt_ip_route_input(skb, iph->daddr, iph->saddr, skb->rtdev) )
            goto drop;

    return rt_ip_local_deliver(skb);

drop:
    kfree_rtskb(skb);
    return NET_RX_DROP;
}
