/* rtmac_rx.c
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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


#include <linux/netdevice.h>

#include <rtnet.h>
#include <rtmac.h>
#include <tdma.h>
#include <tdma_event.h>

int tdma_packet_rx(struct rtskb *skb, struct rtnet_device *rtdev, struct rtpacket_type *pt)
{
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtmac->priv;
	
	struct rtmac_hdr *rtmac_ptr;
	struct tdma_hdr *tdma_ptr;
	TDMA_EVENT event;

	int ret = 0;

	/*
	 * set pointers in skb
	 *
	 * network layer pointer (->nh) to rtmac header
	 * transport layer pointer (->h) to tdma header
	 * data pointer (->data) to beginning of data
	 */	
	skb->nh.raw = skb->data;
	rtmac_ptr = (struct rtmac_hdr *)skb->nh.raw;
	skb->data += sizeof(struct rtmac_hdr);

	skb->h.raw = skb->data;
	tdma_ptr = (struct tdma_hdr *)skb->h.raw;
	skb->data += sizeof(struct tdma_hdr);


	/*
	 * test if the received packet is a valid tdma packet...
	 */
	if (rtmac_ptr->type != __constant_htons(ETH_TDMA) || rtmac_ptr->ver != RTMAC_VERSION) {
		rt_printk("RTmac: tdma: received packet on interface %s is not tdma ;(\n",
			  rtdev->name);
		kfree_rtskb(skb);
		return -1;
	}

	event = ntohl(tdma_ptr->msg);

	ret = tdma_do_event(tdma, event, (void *)skb);

	/*
	 * dispose socket buffer
	 */
	kfree_rtskb(skb);

	return ret;
}
