/* tdma_rx.c
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

#include <rtmac/tdma/tdma_event.h>


int tdma_packet_rx(struct rtskb *skb)
{
    struct rtnet_device *rtdev = skb->rtdev;
    struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;

    struct tdma_hdr *tdma_ptr;
    TDMA_EVENT event;

    int ret = 0;

    tdma_ptr = (struct tdma_hdr *)skb->data;
    rtskb_pull(skb, sizeof(struct tdma_hdr));

    event = ntohl(tdma_ptr->msg);

    ret = tdma_do_event(tdma, event, (void *)skb);

    /*
     * dispose socket buffer
     */
    kfree_rtskb(skb);

    return ret;
}
