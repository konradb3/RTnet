/* rtmac_proto.c
 *
 * rtmac - real-time networking media access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>,
 *               2003 Jan Kiszka <Jan.Kiszka@web.de>
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


#include <rtai.h>

#include <rtmac/rtmac_disc.h>
#include <rtmac/rtmac_proto.h>



int rtmac_proto_rx(struct rtskb *skb, struct rtpacket_type *pt)
{
    struct rtmac_disc *disc = skb->rtdev->mac_disc;
    struct rtmac_hdr  *hdr;


    if (disc == NULL) {
        rt_printk("RTmac: received RTmac packet on unattached device %s\n",
                  skb->rtdev->name);
        return -1;
    }

    hdr = (struct rtmac_hdr *)skb->data;
    rtskb_pull(skb, sizeof(struct rtmac_hdr));

    if (hdr->ver != RTMAC_VERSION) {
        rt_printk("RTmac: received unsupported RTmac protocol version on "
                  "device %s\n", skb->rtdev->name);
        return -1;
    }

    if (disc->disc_type == hdr->type)
        return disc->packet_rx(skb);
    else
        return -1; /*rtmac_vnic_rx*/
}



static struct rtpacket_type rtmac_packet_type = {
    name:       "RTmac",
    type:       __constant_htons(ETH_RTMAC),
    handler:    rtmac_proto_rx,
    private:    (void *)1
};



void rtmac_proto_init(void)
{
    /*
     * install our layer 3 packet type
     */
    rtdev_add_pack(&rtmac_packet_type);
}


void rtmac_proto_release(void)
{
    /*
     * remove packet type from stack manager
     */
    rtdev_remove_pack(&rtmac_packet_type);
}
