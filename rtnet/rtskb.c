/***
 * rtnet/rtskb.c - rtskb implementation for rtnet
 *
 * Copyright (C) 2002	U. Marx <marx@fet.uni-hannover.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <net/checksum.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtnet.h>
#include <rtnet_internal.h>

static int DEFAULT_RTSKB_POOL = DEFAULT_RTSKB_POOL_DEF;
static int DEFAULT_INC_RTSKB = DEFAULT_INC_RTSKB_DEF;
static int DEFAULT_DEC_RTSKB = DEFAULT_DEC_RTSKB_DEF;
MODULE_PARM(DEFAULT_RTSKB_POOL, "i");
MODULE_PARM(DEFAULT_INC_RTSKB, "i");
MODULE_PARM(DEFAULT_DEC_RTSKB, "i");
MODULE_PARM_DESC(DEFAULT_RTSKB_POOL, "number of Realtime Socket Buffers in pool");
MODULE_PARM_DESC(DEFAULT_INC_RTSKB, "low water mark");
MODULE_PARM_DESC(DEFAULT_DEC_RTSKB, "high water mark");

/**
 * struct rtskb_pool
 */
int inc_pool_srq;
int dec_pool_srq;
struct rtskb_head rtskb_pool;


#define RTSKB_CACHE	       	"rtskb"
kmem_cache_t			*rtskb_cache;

#define RTSKB_DATA_CACHE       	"rtskb_data"
kmem_cache_t			*rtskb_data_cache;



/***
 *	rtskb_copy_and_csum_bits
 */
unsigned int rtskb_copy_and_csum_bits(const struct rtskb *skb, int offset, u8 *to, int len, unsigned int csum)
{
	int copy;
	int start = skb->len - skb->data_len;
	int pos = 0;

	/* Copy header. */
	if ((copy = start-offset) > 0) {
		if (copy > len)
			copy = len;
		csum = csum_partial_copy_nocheck(skb->data+offset, to, copy, csum);
		if ((len -= copy) == 0)
			return csum;
		offset += copy;
		to += copy;
		pos = copy;
	}

	BUG();
	return csum;
}


/***
 *	rtskb_copy_and_csum_dev
 */
void rtskb_copy_and_csum_dev(const struct rtskb *skb, u8 *to)
{
	unsigned int csum;
	long csstart;

	if (skb->ip_summed == CHECKSUM_HW)
		csstart = skb->h.raw - skb->data;
	else
		csstart = skb->len - skb->data_len;

	if (csstart > skb->len - skb->data_len)
		BUG();

	memcpy(to, skb->data, csstart);

	csum = 0;
	if (csstart != skb->len)
		csum = rtskb_copy_and_csum_bits(skb, csstart, to+csstart, skb->len-csstart, 0);

	if (skb->ip_summed == CHECKSUM_HW) {
		long csstuff = csstart + skb->csum;

		*((unsigned short *)(to + csstuff)) = csum_fold(csum);
	}
}



/**
 *	skb_over_panic		- 	private function
 *	@skb: buffer
 *	@sz: size
 *	@here: address
 *
 *	Out of line support code for rtskb_put(). Not user callable.
 */
void rtskb_over_panic(struct rtskb *skb, int sz, void *here)
{
	char *name;
	if ( (skb->rtdev) && (dev_get_by_rtdev(skb->rtdev)) ) 
		name=dev_get_by_rtdev(skb->rtdev)->name;
	else 
		name="<NULL>";
	rt_printk("RTnet: rtskb_put :over: %p:%d put:%d dev:%s\n", here, skb->len, sz, name );
}



/**
 *	skb_under_panic		- 	private function
 *	@skb: buffer
 *	@sz: size
 *	@here: address
 *
 *	Out of line support code for rtskb_push(). Not user callable.
 */
void rtskb_under_panic(struct rtskb *skb, int sz, void *here)
{
	char *name = "";
	if ( (skb->rtdev) && (dev_get_by_rtdev(skb->rtdev)) ) 
		name=dev_get_by_rtdev(skb->rtdev)->name;
	else 
		name="<NULL>";
		
        rt_printk("RTnet: rtskb_push :under: %p:%d put:%d dev:%s\n", here, skb->len, sz, name);
}



/***
 *	rtskb_init		-	constructor for slab 
 *
 */
static inline void rtskb_init(void *p, kmem_cache_t *cache, unsigned long flags)
{
	struct rtskb *skb = p;
	memset (skb, 0, sizeof(struct rtskb));
}



/***
 *	rtskb_data_init		-	constructor for slab 
 *
 */
static inline void rtskb_data_init(void *p, kmem_cache_t *cache, unsigned long flags)
{
	unsigned char *skb_data = p;
	memset (skb_data, 0, SKB_DATA_ALIGN(ETH_FRAME_LEN));
}




/***
 *	new_rtskb		-	allocate an new rtskb-Buffer
 *	return:	buffer
 */
struct rtskb *new_rtskb(void)
{
	struct rtskb *skb;
	unsigned int len = SKB_DATA_ALIGN(ETH_FRAME_LEN);

	if ( !(skb = kmem_cache_alloc(rtskb_cache, GFP_ATOMIC)) ) {
		rt_printk("RTnet: allocate rtskb failed.\n");
		return NULL;
	}
	memset(skb, 0, sizeof(struct rtskb));

	if ( !(skb->buf_start = kmem_cache_alloc(rtskb_data_cache, GFP_ATOMIC)) ) {
		rt_printk("RTnet: allocate rtskb->buf_ptr failed.\n");
		kmem_cache_free(rtskb_cache, skb);
		return NULL;
	}

	memset(skb->buf_start, 0, len);
	skb->buf_len = len;
	skb->buf_end = skb->buf_start+len-1;

	return skb;
}



/***
 *	dispose_rtskb		-	deallocate the buffer
 *	@skb	rtskb
 */ 
void dispose_rtskb(struct rtskb *skb) 
{
	if ( skb ) {
		if (skb->buf_start)
			kmem_cache_free(rtskb_data_cache, skb->buf_start);
		kmem_cache_free(rtskb_cache, skb);
	}
}



/***
 *	alloc_rtskb
 *	@size: i will need it later.
 */
struct rtskb *alloc_rtskb(unsigned int size) 
{
	struct rtskb *skb; 

	if ( rtskb_pool.qlen>0 ) 
		skb = rtskb_dequeue(&rtskb_pool);
        else
	        skb = new_rtskb(); 

	/* Load the data pointers. */
	skb->data = skb->buf_start;
	skb->tail = skb->buf_start;
	skb->end  = skb->buf_start + size;
	
	/* Set up other state */
	skb->len = 0;
	skb->cloned = 0;
	skb->data_len = 0;

	skb->users = 1; 
		
	if ( rtskb_pool.qlen<DEFAULT_INC_RTSKB )
		rt_pend_linux_srq(inc_pool_srq);

	return (skb);
}



/***
 *	kfree_rtskb
 *	@skb	rtskb
 */
void kfree_rtskb(struct rtskb *skb) 
{
	if ( skb ) {
		skb->users = 0; 
		memset(skb->buf_start, 0, skb->buf_len);
		
		rtskb_queue_tail(&rtskb_pool, skb);
		
		if ( rtskb_pool.qlen>DEFAULT_DEC_RTSKB )
			rt_pend_linux_srq(dec_pool_srq);
	}
}



/***
 *	__rtskb_dequeue
 *	@list	rtskb_head
 */
struct rtskb *__rtskb_dequeue(struct rtskb_head *list)
{
	struct rtskb *result = NULL;

	if (list->qlen > 0) {
		result=list->first;
		list->first=result->next;
		result->next=NULL;
		list->qlen--;
	}

	return result;
}



/***
 *	rtskb_dequeue
 *	@list	rtskb_head
 */
struct rtskb *rtskb_dequeue(struct rtskb_head *list)
{
	unsigned long flags;
	struct rtskb *result;
	
	flags = rt_spin_lock_irqsave(&list->lock);
	result = __rtskb_dequeue(list);
	rt_spin_unlock_irqrestore(flags, &list->lock);

	return result;
}



/***
 *	__rtskb_queue_head
 *	@list
 *	@skb
 */	
void __rtskb_queue_head(struct rtskb_head *list, struct rtskb *skb)
{
	skb->head = list;
	skb->next = list->first;

	if ( !list->qlen ) {
		list->first = list->last = skb;
	} else {
		list->first = skb;
	}
	list->qlen++;
}



/***
 *	rtskb_queue_head
 *	@list
 *	@skb
 */	
void rtskb_queue_head(struct rtskb_head *list, struct rtskb *skb)
{
	unsigned long flags;

	flags = rt_spin_lock_irqsave(&list->lock);
	__rtskb_queue_head(list, skb);
	rt_spin_unlock_irqrestore(flags, &list->lock);
}



/***
 *	__rtsk_queue_tail
 *	@rtsk_head
 *	@skb
 */ 
void __rtskb_queue_tail(struct rtskb_head *list, struct rtskb *skb)
{
	skb->head = list;
	skb->next  = NULL;

	if ( !list->qlen ) {
		list->first = list->last = skb;
	} else {
		list->last->next = skb;
		list->last = skb;
	}
	list->qlen++;
}



/***
 *	rtskb_queue_tail
 *	@rtskb_head
 */
void rtskb_queue_tail(struct rtskb_head *list, struct rtskb *skb)
{
	unsigned long flags;
	flags = rt_spin_lock_irqsave(&list->lock);
	__rtskb_queue_tail(list, skb);	
	rt_spin_unlock_irqrestore(flags, &list->lock);
}



/***
 *	__rtskb_queue_purge
 *	@rtskb_head
 */
void __rtskb_queue_purge(struct rtskb_head *list)
{
	struct rtskb *skb;
	while ( (skb=__rtskb_dequeue(list))!=NULL )
		kfree_rtskb(skb);
}



/***
 *	rtskb_queue_purge	- clean the queue
 *	@rtskb_head
 */
void rtskb_queue_purge(struct rtskb_head *list)
{
	struct rtskb *skb;
	while ( (skb=rtskb_dequeue(list))!=NULL )
		kfree_rtskb(skb);
}



/***
 *	rtsk_queue_head_init	- initialize the queue
 *	@rtskb_head
 */
void rtskb_queue_head_init(struct rtskb_head *list)
{
	spin_lock_init(&list->lock);
	list->first = NULL;
	list->last  = NULL;
	list->qlen = 0;
}



/***
 *	rtsk_queue_len
 *	@rtskb_head
 */
int rtskb_queue_len(struct rtskb_head *list)
{
	return (list->qlen);
}



/***
 *	rtsk_queue_len
 *	@rtskb_head
 */
int rtskb_queue_empty(struct rtskb_head *list)
{
	return (list->qlen == 0);
}






/***
 *	inc_pool_handler
 */ 
void inc_pool_handler(void) 
{
	while ( rtskb_pool.qlen<DEFAULT_INC_RTSKB )
		rtskb_queue_tail(&rtskb_pool, new_rtskb());
}



/***
 *	dec_pool_handler
 */ 
void dec_pool_handler(void) 
{
	while ( rtskb_pool.qlen>DEFAULT_DEC_RTSKB )
		dispose_rtskb(rtskb_dequeue(&rtskb_pool));
}



/***
 *	rtskb_pool_init 
 */
int rtskb_pool_init(void) 
{
	int i, err = 0;
	
	rtskb_queue_head_init(&rtskb_pool);

	rtskb_cache = kmem_cache_create 
		(RTSKB_CACHE, sizeof (struct rtskb), 0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	if ( !rtskb_cache ) {
		rt_printk("RTnet: allocating 'rtskb_cache' failed.");
		return -ENOMEM;
	}
	rtskb_data_cache = kmem_cache_create 
		(RTSKB_DATA_CACHE, SKB_DATA_ALIGN(ETH_FRAME_LEN), 0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	if ( !rtskb_data_cache ) {
		rt_printk("RTnet: allocating 'rtskb_data_cache' failed.");
		return -ENOMEM;
	}

	for (i=0; i<DEFAULT_RTSKB_POOL; i++) {
		__rtskb_queue_tail(&rtskb_pool, new_rtskb());
	}

	if ( (inc_pool_srq=rt_request_srq (0, inc_pool_handler, 0)) < 0) {
		rt_printk("RTnet: allocating 'inc_pool_srq=%d' failed.\n", inc_pool_srq);
		return inc_pool_srq;
	}

	if ( (dec_pool_srq=rt_request_srq (0, dec_pool_handler, 0)) < 0) {
		rt_printk("RTnet: allocating 'inc_pool_srq=%d' failed.\n", inc_pool_srq);
		return inc_pool_srq;
	}

	return err;
}



/***
 *	rtskb_pool_release
 */
int rtskb_pool_release(void) 
{
	int err = 0;

	if ( rt_free_srq (dec_pool_srq)<0 ) {
		rt_printk("RTnet: deallocating 'dec_pool_srq=%d' failed.\n", dec_pool_srq);
		return dec_pool_srq;
	}

	if ( rt_free_srq (inc_pool_srq)<0 ) {
		rt_printk("RTnet: deallocating 'inc_pool_srq=%d' failed.\n", inc_pool_srq);
		return inc_pool_srq;
	}

	if ( rtskb_pool.qlen>0 ) {
		int i;		
		for (i=rtskb_pool.qlen; i>0; i--)
			dispose_rtskb(__rtskb_dequeue(&rtskb_pool));
	}

	if ( rtskb_data_cache && kmem_cache_destroy (rtskb_data_cache) ) {
		rt_printk("RTnet: deallocating 'rtskb_data_cache' failed.\n");
	}
	
	if ( rtskb_cache && kmem_cache_destroy (rtskb_cache) ) {
		rt_printk("RTnet: deallocating 'rtnet_skb_cache' failed.\n");
	}

	return err;
}






