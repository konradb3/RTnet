/* rtskb.h
 *
 * RTnet - real-time networking subsystem
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
#ifndef __RTSKB_H_
#define __RTSKB_H_

#ifdef __KERNEL__

#include <rtai.h>


struct rtskb_head;
struct rtsocket;
struct rtnet_device;

/***
 *	rtskb - realtime socket buffer
 */
struct rtskb {
	struct rtskb		*next;

	struct rtskb_head	*head;

	struct rtsocket		*sk;
	struct rtnet_device	*rtdev;

	/* transport layer */
	union
	{
		struct tcphdr	*th;
		struct udphdr	*uh;
		struct icmphdr	*icmph;
		struct iphdr	*ipihdr;
		unsigned char	*raw;
	} h;

	/* network layer */
	union
	{
		struct iphdr	*iph;
		struct arphdr	*arph;
		unsigned char	*raw;
	} nh;

	/* link layer */
	union
	{
		struct ethhdr	*ethernet;
		unsigned char	*raw;
	} mac;

	unsigned short		protocol;
	unsigned char		pkt_type;

  	unsigned int		users;
	unsigned char		cloned;
	
	unsigned int 		csum;
	unsigned char		ip_summed;
	
	struct rt_rtable	*dst;
	
	unsigned int		len;
	unsigned int		data_len;
	unsigned int		buf_len;	
	
	unsigned char		*buf_start;
	unsigned char		*buf_end;
	
	unsigned char		*data;
	unsigned char		*tail;
	unsigned char		*end;
	RTIME                   rx;
};

struct rtskb_head {
	struct rtskb	*first;
	struct rtskb	*last;

	unsigned int	qlen;
	spinlock_t	lock;
};


#if defined(CONFIG_RTNET_MM_VMALLOC)
	#define rt_mem_alloc(size)	vmalloc (size)
	#define rt_mem_free(addr)	vfree (addr)
#else
	#define rt_mem_alloc(size)	kmalloc (size, GFP_KERNEL)
	#define rt_mem_free(addr)	kfree (addr)
#endif


/****
 * The number of free rtskb's in pool is handled by two srq's. inc_pool_srq means 
 * creating rtskbs and dec_pool_srq is the opposite of it.
 */
//these are the new default values for the module parameter
#define DEFAULT_RTSKB_POOL_DEF	24	/* default number of rtskb's in pool				*/
#define DEFAULT_MIN_RTSKB_DEF	16	/* create new rtskb if less then DEFAULT_MIN_RTSKB in Pool	*/
#define DEFAULT_MAX_RTSKB_DEF	32	/* delete rtskb if more then DEFAULT_MAX_RTSKB in Pool		*/

extern unsigned int rtskb_pool_pool;		/* needed for /proc/... in rtnet_module.c */
extern unsigned int rtskb_pool_min;		
extern unsigned int rtskb_pool_max;		

extern unsigned int rtskb_amount;		/* current number of allocated rtskbs */
extern unsigned int rtskb_amount_max;		/* maximum number of allocated rtskbs */

extern void rtskb_over_panic(struct rtskb *skb, int len, void *here);
extern void rtskb_under_panic(struct rtskb *skb, int len, void *here);
extern struct rtskb *new_rtskb(void);
extern void dispose_rtskb(struct rtskb *skb);
extern struct rtskb *alloc_rtskb(unsigned int size);   
#define dev_alloc_rtskb(len)	alloc_rtskb(len)
extern void kfree_rtskb(struct rtskb *skb);
#define dev_kfree_rtskb(a)	kfree_rtskb(a)

extern struct rtskb *rtskb_clone(struct rtskb *skb,int gfp_mask);
extern struct rtskb *rtskb_copy(struct rtskb *skb,int gfp_mask);
extern struct rtskb *__rtskb_dequeue(struct rtskb_head *list);
extern struct rtskb *rtskb_dequeue(struct rtskb_head *list);

extern void __rtskb_queue_head(struct rtskb_head *list, struct rtskb *skb);
extern void rtskb_queue_head(struct rtskb_head *list,struct rtskb *buf);
extern void __rtskb_queue_tail(struct rtskb_head *list, struct rtskb *skb);
extern void rtskb_queue_tail(struct rtskb_head *list,struct rtskb *buf);

extern int rtskb_queue_len(struct rtskb_head *list);
extern int rtskb_queue_empty(struct rtskb_head *list);
extern void rtskb_queue_purge(struct rtskb_head *list);
extern void rtskb_queue_head_init(struct rtskb_head *list);

static inline int rtskb_is_nonlinear(const struct rtskb *skb)
{
	return skb->data_len;
}

static inline int rtskb_headlen(const struct rtskb *skb)
{
	return skb->len - skb->data_len;
}

static inline void rtskb_reserve(struct rtskb *skb, unsigned int len)
{
	skb->data+=len;
	skb->tail+=len;
}

#define RTSKB_LINEAR_ASSERT(rtskb) do { if (rtskb_is_nonlinear(rtskb)) BUG(); }  while (0)

static inline unsigned char *__rtskb_put(struct rtskb *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
 	RTSKB_LINEAR_ASSERT(skb);
	skb->tail+=len;
	skb->len+=len;
	return tmp;
}

static inline unsigned char *rtskb_put(struct rtskb *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	RTSKB_LINEAR_ASSERT(skb);
	skb->tail+=len;
	skb->len+=len;
	if(skb->tail>skb->buf_end) {
		rtskb_over_panic(skb, len, current_text_addr());
	}
	return tmp;
}

static inline unsigned char *__rtskb_push(struct rtskb *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	return skb->data;
}

static inline unsigned char *rtskb_push(struct rtskb *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	if (skb->data<skb->buf_start) {
		rtskb_under_panic(skb, len, current_text_addr());
	}
	return skb->data;
}

static inline char *__rtskb_pull(struct rtskb *skb, unsigned int len)
{
	skb->len-=len;
	if (skb->len < skb->data_len)
		BUG();
	return 	skb->data+=len;
}

static inline unsigned char *rtskb_pull(struct rtskb *skb, unsigned int len)
{	
	if (len > skb->len)
		return NULL;
		
	return __rtskb_pull(skb,len);
}


static inline void rtskb_trim(struct rtskb *skb, unsigned int len)
{
	if (skb->len>len) {
		skb->len = len;
		skb->tail = skb->data+len;
	}
}



extern struct rtskb_head rtskb_pool;
extern int rtskb_pool_init(void);
extern int rtskb_pool_release(void);

extern unsigned int rtskb_copy_and_csum_bits(const struct rtskb *skb, int offset, u8 *to, int len, unsigned int csum); 
extern void rtskb_copy_and_csum_dev(const struct rtskb *skb, u8 *to);


#endif /* __KERNEL__ */

#endif  /* __RTSKB_H_ */
