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

// $Log: ip_output.c,v $
// Revision 1.12  2003/08/20 16:26:25  kiszka
// * applied new pool mechanisms
//
// Revision 1.11  2003/07/18 15:22:39  kiszka
// * restructured rtmac to prepare separated discipline modules and virtual NICs
// * merged with Marc's autotool branch
//
// Revision 1.10  2003/06/30 16:18:00  kiszka
// * fixed RTmac-awareness of rt_ip_build_xmit_slow
//
// Revision 1.9  2003/06/24 13:09:46  kiszka
// * applied fragmentation patch by Mathias Koehrer
//
// Revision 1.8  2003/05/27 09:50:41  kiszka
// * applied new header file structure
//
// Revision 1.7  2003/05/17 19:28:11  hpbock
// rt_ip_build_xmit():
// deleted useless code
//
// Revision 1.6  2003/05/17 16:28:11  hpbock
// rtnet and rtnetproxy now use rtmac function hooks to send packets
// rtmac does not modify hard_start_xmit any more
//
// Revision 1.5  2003/05/16 19:31:52  hpbock
// big fat merge with pre-0-3-0
// compiles and hopefully also runs =8)
//
// Revision 1.4.2.1  2003/03/10 18:18:01  yamwong
// * Fixed bug: 680211 Using ifconfig on RTnet device crashes the system.
// * New device manager: decouple rtnet_device from net_device.
//   NOTE: Only 8139too-rt, eepro100-rt and tulip-rt were tested and known
//   to work.
// * Added new scripts for round trip examples.
//
// Revision 1.4  2003/02/12 07:49:15  hpbock
// rt_ip_build_xmit() returns -EAGAIN if packet could not be sent by rtdev_xmit()
//
// Revision 1.3  2003/02/06 14:34:06  hpbock
// skb was net freed, if rtdev->hard_header was NULL.
//

#include <net/checksum.h>

#include <rtnet_socket.h>
#include <ipv4/ip_fragment.h>
#include <ipv4/ip_input.h>
#include <ipv4/route.h>
#include <rtmac/rtmac_disc.h>


static u16 rt_ip_id_count = 0;

/***
 * Slow path for fragmented packets...
 */
int rt_ip_build_xmit_slow(struct rtsocket *sk,
        int getfrag(const void *, char *, unsigned int, unsigned int),
        const void *frag, unsigned length, struct rt_rtable *rt, int flags)
{
    int         err = 0;
    struct      rtskb *skb;
    struct      iphdr *iph;

    struct      rtnet_device *rtdev=rt->rt_dev;
    int         mtu = rtdev->mtu;
    unsigned    fragheaderlen, fragdatalen;
    unsigned    offset = 0;
    u16         msg_rt_ip_id;



    fragheaderlen = sizeof(struct iphdr); /* 20 byte... */

    fragdatalen  = ((mtu - fragheaderlen) & ~7 );

    /* Store id in local variable */
    msg_rt_ip_id = ++rt_ip_id_count;

    for (offset = 0; offset < length; offset += fragdatalen)
    {
        int fraglen; /* The length (IP, including ip-header) of this
                        very fragment */
        __u16 frag_off = offset >> 3 ;

        if (offset >= length - fragdatalen)
        {
            /* last fragment */
            fraglen = fragheaderlen + length - offset ;
        }
        else
        {
            fraglen = fragheaderlen + fragdatalen;
            frag_off |= IP_MF;
        }

        {
            int hh_len = (rtdev->hard_header_len+15)&~15;

            skb = alloc_rtskb(fraglen + hh_len + 15, &sk->skb_pool);
            if (skb==NULL)
                    goto no_rtskb;
            rtskb_reserve(skb, hh_len);
        }

        skb->dst=rt;
        skb->rtdev=rt->rt_dev;
        skb->nh.iph = iph = (struct iphdr *) rtskb_put(skb, fraglen);


        iph->version=4;
        iph->ihl=5;    /* 20 byte header - no options */
        iph->tos=sk->tos;
        iph->tot_len = htons(fraglen);
        iph->id=htons(msg_rt_ip_id);
        iph->frag_off = htons(frag_off);
        iph->ttl=255;
        iph->protocol=sk->protocol;
        iph->saddr=rt->rt_src;
        iph->daddr=rt->rt_dst;
        iph->check=0;
        iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

        if ( (err=getfrag(frag, ((char *)iph)+iph->ihl*4, offset,
                          fraglen - fragheaderlen)) )
            goto error;

        {
            unsigned char *d, *s;

            d=rt->rt_dst_mac_addr;
            s=rtdev->dev_addr;
        }


        if (!(rtdev->hard_header) ||
            (rtdev->hard_header(skb, rtdev, ETH_P_IP, rt->rt_dst_mac_addr,
                                rtdev->dev_addr, skb->len) < 0))
            goto error;

        err = rtdev_xmit(skb);

        if (err)
            return -EAGAIN;
    }
    return 0;

error:
    kfree_rtskb(skb);
no_rtskb:
    return err;
}



/***
 *  Fast path for unfragmented packets.
 */
int rt_ip_build_xmit(struct rtsocket *sk,
        int getfrag(const void *, char *, unsigned int, unsigned int),
        const void *frag, unsigned length, struct rt_rtable *rt, int flags)
{
    int     err=0;
    struct  rtskb *skb;
    int     df;
    struct  iphdr *iph;
    int     hh_len;

    struct  rtnet_device *rtdev=rt->rt_dev;

    /*
     *  Try the simple case first. This leaves fragmented frames, and by choice
     *  RAW frames within 20 bytes of maximum size(rare) to the long path
     */
    length += sizeof(struct iphdr);

    if (length > rtdev->mtu)
    {
        return rt_ip_build_xmit_slow(sk, getfrag, frag,
                        length - sizeof(struct iphdr), rt, flags);
    }

    df = htons(IP_DF);

    hh_len = (rtdev->hard_header_len+15)&~15;

    skb = alloc_rtskb(length+hh_len+15, &sk->skb_pool);
    if (skb==NULL)
       goto no_rtskb;

    rtskb_reserve(skb, hh_len);

    skb->dst=rt;
    skb->rtdev=rt->rt_dev;
    skb->nh.iph = iph = (struct iphdr *) rtskb_put(skb, length);

    iph->version=4;
    iph->ihl=5;
    iph->tos=sk->tos;
    iph->tot_len = htons(length);
    iph->id=htons(rt_ip_id_count++);
    iph->frag_off = df;
    iph->ttl=255;
    iph->protocol=sk->protocol;
    iph->saddr=rt->rt_src;
    iph->daddr=rt->rt_dst;
    iph->check=0;
    iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);

    if ( (err=getfrag(frag, ((char *)iph)+iph->ihl*4, 0, length-iph->ihl*4)) )
        goto error;

    if (!(rtdev->hard_header) ||
        (rtdev->hard_header(skb, rtdev, ETH_P_IP, rt->rt_dst_mac_addr,
                            rtdev->dev_addr, skb->len) < 0))
        goto error;

    err = rtdev_xmit(skb);

    if (err)
        return -EAGAIN;
    else
        return 0;

error:
    kfree_rtskb(skb);
no_rtskb:
    return err;
}



/***
 *  IP protocol layer initialiser
 */
static struct rtpacket_type ip_packet_type =
{
    name:       "IPv4",
    type:       __constant_htons(ETH_P_IP),
    handler:    &rt_ip_rcv,
    private:    (void*)1,
};



/***
 *  ip_init
 */
void rt_ip_init(void)
{
    rtdev_add_pack(&ip_packet_type);
    rt_ip_fragment_init();
}



/***
 *  ip_release
 */
void rt_ip_release(void)
{
    rt_ip_fragment_cleanup();
    rtdev_remove_pack(&ip_packet_type);
}
