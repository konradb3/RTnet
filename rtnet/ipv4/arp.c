/* arp.c - Adress Resolution for rtnet
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
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/netdevice.h>
#include <linux/in.h>

#include <rtai.h>
#include <rtai_sched.h>

#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>
#include <rtai_proc_fs.h>
#endif /* CONFIG_PROC_FS */

#include <rtnet.h>
#include <rtnet_internal.h>
#include <../rtmac/include/rtmac.h>

/***
 *	arp_send:	Create and send an arp packet. If (dest_hw == NULL),
 *			we create a broadcast message.
 */
void rt_arp_send(int type, 
		 int ptype, 
		 u32 dest_ip, 
		 struct rtnet_device *rtdev, 
		 u32 src_ip, 
		 unsigned char *dest_hw, 
		 unsigned char *src_hw,
		 unsigned char *target_hw)
{
	struct rtskb *skb;
	struct arphdr *arp;
	unsigned char *arp_ptr;
	
	if ( rtdev->flags & IFF_NOARP )
		return;

	if ( !(skb=alloc_rtskb(sizeof(struct arphdr)+ 2*(rtdev->addr_len+4)+rtdev->hard_header_len+15)) )
		return;

	rtskb_reserve(skb, (rtdev->hard_header_len+15)&~15);

	skb->nh.raw = skb->data;
	arp = (struct arphdr *) rtskb_put(skb,sizeof(struct arphdr) + 2*(rtdev->addr_len+4));

	skb->rtdev = rtdev;
	skb->protocol = __constant_htons (ETH_P_ARP);
	if (src_hw == NULL)
		src_hw = rtdev->dev_addr;
	if (dest_hw == NULL)
		dest_hw = rtdev->broadcast;

	/*
	 *	Fill the device header for the ARP frame
	 */
	if (rtdev->hard_header &&
	    rtdev->hard_header(skb,rtdev,ptype,dest_hw,src_hw,skb->len) < 0)
		goto out;

	arp->ar_hrd = htons(rtdev->type);
	arp->ar_pro = __constant_htons(ETH_P_IP);
	arp->ar_hln = rtdev->addr_len;
	arp->ar_pln = 4;
	arp->ar_op = htons(type);

	arp_ptr=(unsigned char *)(arp+1);

	memcpy(arp_ptr, src_hw, rtdev->addr_len);
	arp_ptr+=rtdev->addr_len;

	memcpy(arp_ptr, &src_ip,4);
	arp_ptr+=4;

	if (target_hw != NULL)
		memcpy(arp_ptr, target_hw, rtdev->addr_len);
	else
		memset(arp_ptr, 0, rtdev->addr_len);
	arp_ptr+=rtdev->addr_len;

	memcpy(arp_ptr, &dest_ip, 4);


	/* send the frame */
	if ((skb->rtdev->rtmac) && /* This code lines are crappy! */
	    (skb->rtdev->rtmac->disc_type) &&
	    (skb->rtdev->rtmac->disc_type->rt_packet_tx)) {
	    skb->rtdev->rtmac->disc_type->rt_packet_tx(skb, skb->rtdev);
	} else {
	    if (rtdev_xmit_if(skb)) { /* If xmit fails, free rtskb. */
	        goto out;
	    }
	}

	return;

out:
	kfree_rtskb(skb);
}


/***
 *	arp_rcv:	Receive an arp request by the device layer.
 */
int rt_arp_rcv(struct rtskb *skb, struct rtnet_device *rtdev, struct rtpacket_type *pt)
{
	struct arphdr *arp = skb->nh.arph;
	unsigned char *arp_ptr= (unsigned char *)(arp+1);
	unsigned char *sha, *tha;
	u32 sip, tip;
	u16 dev_type = rtdev->type;

/*
 *	The hardware length of the packet should match the hardware length
 *	of the device.  Similarly, the hardware types should match.  The
 *	device should be ARP-able.  Also, if pln is not 4, then the lookup
 *	is not from an IP number.  We can't currently handle this, so toss
 *	it. 
 */  
	if (arp->ar_hln != rtdev->addr_len    || 
	    rtdev->flags & IFF_NOARP ||
	    skb->pkt_type == PACKET_OTHERHOST ||
	    skb->pkt_type == PACKET_LOOPBACK ||
	    arp->ar_pln != 4)
		goto out;

	switch (dev_type) {
	default:	
		if ( arp->ar_pro != __constant_htons(ETH_P_IP) &&
		     htons(dev_type) != arp->ar_hrd )
			goto out;
		break;
	case ARPHRD_ETHER:
		/*
		 * ETHERNET devices will accept ARP hardware types of either
		 * 1 (Ethernet) or 6 (IEEE 802.2).
		 */
		if (arp->ar_hrd != __constant_htons(ARPHRD_ETHER) &&
		    arp->ar_hrd != __constant_htons(ARPHRD_IEEE802)) {
			goto out;
		}
		if (arp->ar_pro != __constant_htons(ETH_P_IP)) {
			goto out;
		}
		break;
	}

	/* Understand only these message types */
	if (arp->ar_op != __constant_htons(ARPOP_REPLY) &&
	    arp->ar_op != __constant_htons(ARPOP_REQUEST))
		goto out;

/*
 *	Extract fields
 */
	sha=arp_ptr;
	arp_ptr += rtdev->addr_len;
	memcpy(&sip, arp_ptr, 4);

	arp_ptr += 4;
	tha=arp_ptr;
	arp_ptr += rtdev->addr_len;
	memcpy(&tip, arp_ptr, 4);

/* 
 *	Check for bad requests for 127.x.x.x and requests for multicast
 *	addresses.  If this is one such, delete it.
 */
	if (LOOPBACK(tip) || MULTICAST(tip))
		goto out;


	if (dev_type == ARPHRD_DLCI)
		sha = rtdev->broadcast;

/*
 *  Process entry.  The idea here is we want to send a reply if it is a
 *  request for us or if it is a request for someone else that we hold
 *  a proxy for.  We want to add an entry to our cache if it is a reply
 *  to us or if it is a request for our address.  
 *  (The assumption for this last is that if someone is requesting our 
 *  address, they are probably intending to talk to us, so it saves time 
 *  if we cache their address.  Their address is also probably not in 
 *  our cache, since ours is not in their cache.)
 * 
 *  Putting this another way, we only care about replies if they are to
 *  us, in which case we add them to the cache.  For requests, we care
 *  about those for us and those for our proxies.  We reply to both,
 *  and in the case of requests for us we add the requester to the arp 
 *  cache.
 */

	if ( rt_ip_route_input(skb, tip, sip, rtdev)==0 ) {
		rt_arp_table_add(sip, sha);
		if ( arp->ar_op==__constant_htons(ARPOP_REQUEST) )
			rt_arp_send(ARPOP_REPLY,ETH_P_ARP,sip,rtdev,tip,sha,rtdev->dev_addr,sha);
	}

out:
	kfree_rtskb(skb);
	return 0;
}

struct rt_arp_table_struct *free_arp_list;
struct rt_arp_table_struct *arp_list;
struct rt_arp_table_struct rt_arp_table_list[RT_ARP_TABLE_LEN];


/***
 *	proc filesystem section 
 */ 
#ifdef CONFIG_PROC_FS
static int rt_arp_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data) 
{
	PROC_PRINT_VARS;
	struct rt_arp_table_struct *arp_entry;
	
	PROC_PRINT("IPaddress\t\tHWaddress\n");
	for (arp_entry=arp_list; arp_entry!=NULL; arp_entry=arp_entry->next) {
		union { unsigned long l; unsigned char c[4]; } u;
		unsigned char *a;
		u.l=arp_entry->ip_addr;
		a = arp_entry->hw_addr;
		
		PROC_PRINT("%d.%d.%d.%d\t\t%02x:%02x:%02x:%02x:%02x:%02x\n", 
			   u.c[0], u.c[1], u.c[2], u.c[3], 
			   a[0], a[1], a[2], a[3], a[4], a[5]);
	}	
	PROC_PRINT_DONE;
}

static int rt_arp_proc_register(void)
{
	static struct proc_dir_entry *proc_rt_arp;
  
	proc_rt_arp = create_proc_entry("arp", S_IFREG | S_IRUGO | S_IWUSR, rtai_proc_root);
	if (!proc_rt_arp) {
		rt_printk("RTnet: unable to initialize proc-file for arp\n");
		return -1;
	}
	proc_rt_arp->read_proc = rt_arp_read_proc;

	return 0;
}       

static void rt_arp_proc_unregister(void) 
{
	remove_proc_entry ("arp", rtai_proc_root);
}
#endif	/* CONFIG_PROC_FS */



/***
 *	rt_arp_table_lookup
 */
struct rt_arp_table_struct *rt_arp_table_lookup(u32 ip_addr)
{
	struct rt_arp_table_struct *arp_entry;

	//	rt_sem_wait(&arp_sem);

	for(arp_entry=arp_list;arp_entry;arp_entry=arp_entry->next)
		if(arp_entry->ip_addr==ip_addr)
			return arp_entry;

	//	rt_sem_signal(&arp_sem);

	return NULL;
}



struct rt_arp_table_struct *rt_rarp_table_lookup(char *hw_addr)
{
	struct rt_arp_table_struct *arp_entry;

	//rt_sem_wait(&arp_sem);

	for (arp_entry = arp_list; arp_entry; arp_entry = arp_entry->next)
		if (memcmp(arp_entry->hw_addr, hw_addr, RT_ARP_ADDR_LEN) == 0)
		    return arp_entry;
	
	//rt_sem_signal(&arp_sem);

	return NULL;
}


/***
 *	rt_arp_table_add
 */
void rt_arp_table_add(u32 ip_addr, unsigned char *hw_addr)
{
	struct rt_arp_table_struct *arp_entry=rt_arp_table_lookup(ip_addr);

	//	rt_sem_wait(&arp_sem);
	
	if (arp_entry == NULL) {
		arp_entry=free_arp_list;
		if (!arp_entry) {
			rt_printk("RTnet: "__FUNCTION__"(): no free arp entries\n");
			return;
		}

		arp_entry->ip_addr=ip_addr;
		memcpy(arp_entry->hw_addr,hw_addr,RT_ARP_ADDR_LEN);

		free_arp_list=free_arp_list->next;
	
		arp_entry->next=arp_list;
		if (arp_list) 
			arp_list->prev=arp_entry;
		arp_list=arp_entry;
		// Billa: for the rt_arp_table_del() not to crash
		arp_list->prev=NULL;
	}
	//	rt_sem_signal(&arp_sem);
}



/***
 *	rt_arp_table_del
 */
void rt_arp_table_del(u32 ip_addr)
{
	struct rt_arp_table_struct *list = arp_list;
	struct rt_arp_table_struct *next;
	//	rt_sem_wait(&arp_sem);

	while(list != NULL) {
		next = list->next;

		if (list->ip_addr==ip_addr) {
			struct rt_arp_table_struct *prev = list->prev;
			
			if ( prev!=NULL )
				prev->next=next;
			if ( next!=NULL )
				next->prev=prev;
				
			memset(list, 0, sizeof(struct rt_arp_table_struct));
			
			// add to free list;
			list->prev=NULL;
			list->next=free_arp_list;
			free_arp_list=list;

			// if we delete the first element, set head to next
			if(list == arp_list)
				arp_list = next;
		}
		
		list = next;
	}

	//	rt_sem_signal(&arp_sem);
}



/***
 *	rt_arp_table_solicit
 */
int rt_arp_solicit(struct rtnet_device *rtdev,u32 target)
{
	u32 saddr=rtdev->local_addr;
	rt_arp_send(ARPOP_REQUEST, ETH_P_ARP, target, rtdev, saddr, NULL, NULL, NULL);

	return 0;
}



static struct rtpacket_type arp_packet_type = {
	name:		"ARPv4",
	type:		__constant_htons(ETH_P_ARP),
	dev:		NULL,
	handler:	&rt_arp_rcv,
	private:	(void*) 1, /* understand shared skbs */
};



/***
 *	rt_arp_init
 */
void rt_arp_init(void)
{
	int i;
	rtdev_add_pack(&arp_packet_type);
	
	(rt_arp_table_list+RT_ARP_TABLE_LEN-1)->prev=(rt_arp_table_list+RT_ARP_TABLE_LEN-2);
	(rt_arp_table_list+RT_ARP_TABLE_LEN-1)->next=NULL;
	(rt_arp_table_list)->prev=NULL;
	(rt_arp_table_list)->next=(rt_arp_table_list+1);
	
	for (i=1; i<RT_ARP_TABLE_LEN-1; i++) {
		(rt_arp_table_list+i)->prev=(rt_arp_table_list+i-1);
 		(rt_arp_table_list+i)->next=(rt_arp_table_list+i+1);
	}
	free_arp_list=rt_arp_table_list;
	arp_list=NULL;

	//	rt_typed_sem_init(&arp_sem, 1, CNT_SEM);
	
#ifdef CONFIG_PROC_FS
	rt_arp_proc_register();
#endif 
}



/***
 *	rt_arp_release
 */
void rt_arp_release(void)
{
	free_arp_list=arp_list=NULL;
	rtdev_remove_pack(&arp_packet_type);

#ifdef CONFIG_PROC_FS
	rt_arp_proc_unregister();
#endif 
}














