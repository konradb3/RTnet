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

int rt_ip_local_deliver_finish(struct rtskb *skb)
{
    int ihl  = skb->nh.iph->ihl*4;
    int hash = rt_inet_hashkey(skb->nh.iph->protocol);
    int ret  = 0;

    struct rtinet_protocol *ipprot = rt_inet_protocols[hash];

    __rtskb_pull(skb, ihl);

    /* Point into the IP datagram, just past the header. */
    skb->h.raw = skb->data;

    if ( ipprot )
        ret = ipprot->handler(skb);
    else {
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
        }
    }

    return ret;
}

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
int rt_ip_local_deliver(struct rtskb *skb)
{
    /*
     *  Reassemble IP fragments.
     */
    if (skb->nh.iph->frag_off & htons(IP_MF|IP_OFFSET)) {
        skb = rt_ip_defrag(skb);
        if (!skb)
            return 0;
    }
    return rt_ip_local_deliver_finish(skb);
}


/***
 *  rt_ip_rcv
 */
int rt_ip_rcv(struct rtskb *skb, struct rtpacket_type *pt)
{
    struct iphdr *iph;

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

    {
        __u32 len = ntohs(iph->tot_len); 
        if ( (skb->len<len) || (len<((__u32)iph->ihl<<2)) )
            goto drop;

        rtskb_trim(skb, len);
    }

    if (skb->dst == NULL)
        if ( rt_ip_route_input(skb, iph->daddr, iph->saddr, skb->rtdev) )
            goto drop; 

    /* ip_local_deliver */
    if (skb->nh.iph->frag_off & htons(IP_MF|IP_OFFSET)) {
        skb = rt_ip_defrag(skb);
        if (!skb)
            return 0;
    }
    return rt_ip_local_deliver_finish(skb);

drop:
    kfree_rtskb(skb);
    return NET_RX_DROP;
}
