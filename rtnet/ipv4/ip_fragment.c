/* ip_fragment.c
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

#include <rtskb.h>


struct rtskb *rt_ip_defrag(struct rtskb *skb)
{
	return skb;
#if 0
	struct iphdr *iph = skb->nh.iph;
	struct ipq *qp;
	struct net_device *dev;
	
	IP_INC_STATS_BH(IpReasmReqds);

	/* Start by cleaning up the memory. */
	if (atomic_read(&ip_frag_mem) > sysctl_ipfrag_high_thresh)
		ip_evictor();

	dev = skb->dev;

	/* Lookup (or create) queue header */
	if ((qp = ip_find(iph)) != NULL) {
		struct sk_buff *ret = NULL;

		spin_lock(&qp->lock);

		ip_frag_queue(qp, skb);

		if (qp->last_in == (FIRST_IN|LAST_IN) &&
		    qp->meat == qp->len)
			ret = ip_frag_reasm(qp, dev);

		spin_unlock(&qp->lock);
		ipq_put(qp);
		return ret;
	}

	IP_INC_STATS_BH(IpReasmFails);
#endif /* 0 */
	kfree_rtskb(skb);
	return NULL;
}

