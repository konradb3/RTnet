/***
 * rtnet/rtskb.c - rtskb implementation for rtnet
 *
 * Copyright (C) 2002 Ulrich Marx <marx@fet.uni-hannover.de>,
 *               2003 Jan Kiszka <jan.kiszka@web.de>
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

#include <rtdev.h>
#include <rtnet_internal.h>
#include <rtskb.h>

static unsigned int global_rtskbs = DEFAULT_GLOBAL_RTSKBS;
static unsigned int rtskb_max_size = DEFAULT_MAX_RTSKB_SIZE;
MODULE_PARM(global_rtskbs, "i");
MODULE_PARM(rtskb_max_size, "i");
MODULE_PARM_DESC(global_rtskbs, "Number of realtime socket buffers in global pool");
MODULE_PARM_DESC(rtskb_max_size, "Maximum size of an rtskb block (relevant for IP fragmentation)");

/**
 *  global pool
 */
struct rtskb_head global_pool;

/**
 *  statistics
 */
static unsigned int rtskb_pools=0;
static unsigned int rtskb_pools_max=0;
static unsigned int rtskb_amount=0;
static unsigned int rtskb_amount_max=0;



/***
 *  rtskb_copy_and_csum_bits
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
 *  rtskb_copy_and_csum_dev
 */
void rtskb_copy_and_csum_dev(const struct rtskb *skb, u8 *to)
{
    unsigned int csum;
    unsigned int csstart;

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
        unsigned int csstuff = csstart + skb->csum;

        *((unsigned short *)(to + csstuff)) = csum_fold(csum);
    }
}



/**
 *  skb_over_panic - private function
 *  @skb: buffer
 *  @sz: size
 *  @here: address
 *
 *  Out of line support code for rtskb_put(). Not user callable.
 */
void rtskb_over_panic(struct rtskb *skb, int sz, void *here)
{
    char *name;
    if ( skb->rtdev )
        name=skb->rtdev->name;
    else
        name="<NULL>";
    rt_printk("RTnet: rtskb_put :over: %p:%d put:%d dev:%s\n", here, skb->len, sz, name );
}



/**
 *  skb_under_panic - private function
 *  @skb: buffer
 *  @sz: size
 *  @here: address
 *
 *  Out of line support code for rtskb_push(). Not user callable.
 */
void rtskb_under_panic(struct rtskb *skb, int sz, void *here)
{
    char *name = "";
    if ( skb->rtdev )
        name=skb->rtdev->name;
    else
        name="<NULL>";

    rt_printk("RTnet: rtskb_push :under: %p:%d put:%d dev:%s\n", here, skb->len, sz, name);
}



/***
 *  new_rtskb - get a new rtskb from the memory manager
 *  @pool: pool to assign the rtskb to
 */
static inline int new_rtskb(struct rtskb_head *pool)
{
    struct rtskb *skb;
    unsigned int len = SKB_DATA_ALIGN(rtskb_max_size);


    ASSERT(pool != NULL, return -EINVAL;);

#ifndef CONFIG_RTNET_RTSKB_USE_KMALLOC
    /* default case, preserves possibility to create new sockets in real-time
     * note: CONFIG_RTAI_MM_VMALLOC must not be set!
     */
    if ( !(skb = rt_malloc(ALIGN_RTSKB_LEN + len)) ) {
        rt_printk("RTnet: rtskb allocation failed.\n");
        return -ENOMEM;
    }
#else
    if ( !(skb = kmalloc(ALIGN_RTSKB_LEN + len, GFP_KERNEL)) ) {
        rt_printk("RTnet: rtskb allocation failed.\n");
        return -ENOMEM;
    }
#endif

    /* fill the header with zero */
    memset(skb, 0, ALIGN_RTSKB_LEN);

    skb->pool = pool;
    skb->buf_start = ((char *)skb) + ALIGN_RTSKB_LEN;
    skb->buf_len = len;
    skb->buf_end = skb->buf_start+len-1;

    rtskb_queue_tail(pool, skb);

    rtskb_amount++;
    if (rtskb_amount > rtskb_amount_max)
        rtskb_amount_max = rtskb_amount;

    return 0;
}



/***
 *  dispose_rtskb - free the memory of an rtskb
 *  @skb: buffer to dispose
 */
static inline void dispose_rtskb(struct rtskb *skb)
{
    ASSERT(skb != NULL, return;);

    rt_free(skb);
    rtskb_amount--;
}



/***
 *  alloc_rtskb - allocate an rtskb from a pool
 *  @size: required buffer size (to check against maximum boundary)
 *  @pool: pool to take the rtskb from
 */
struct rtskb *alloc_rtskb(unsigned int size, struct rtskb_head *pool)
{
    struct rtskb *skb;


    ASSERT(size <= rtskb_max_size, return NULL;);

    skb = rtskb_dequeue(pool);
    if (!skb)
        return NULL;

    /* Load the data pointers. */
    skb->data = skb->buf_start;
    skb->tail = skb->buf_start;
    skb->end  = skb->buf_start + size;

    /* Set up other state */
    skb->len = 0;
    skb->data_len = 0;

    return skb;
}



/***
 *  kfree_rtskb
 *  @skb    rtskb
 */
void kfree_rtskb(struct rtskb *skb)
{
    ASSERT(skb != NULL, return;);
    ASSERT(skb->pool != NULL, return;);

    rtskb_queue_tail(skb->pool, skb);
}



/***
 *  rtskb_pool_init
 *  @pool: pool to be initialized
 *  @initial_size: number of rtskbs to allocate
 *  return: number of actually allocated rtskbs
 */
unsigned int rtskb_pool_init(struct rtskb_head *pool, unsigned int initial_size)
{
    unsigned int i;

    rtskb_queue_head_init(pool);

    for (i = 0; i < initial_size; i++) {
        if (new_rtskb(pool) != 0)
            break;
    }

    rtskb_pools++;
    if (rtskb_pools > rtskb_pools_max)
        rtskb_pools_max = rtskb_pools;

    return i;
}



/***
 *  rtskb_pool_release
 *  @pool: pool to release
 */
void rtskb_pool_release(struct rtskb_head *pool)
{
    struct rtskb *skb;

    while ( (skb = rtskb_dequeue(pool)) != NULL )
        dispose_rtskb(skb);

    rtskb_pools--;
}



unsigned int rtskb_pool_extend(struct rtskb_head *pool,
                               unsigned int add_rtskbs)
{
    unsigned int i;

    for (i = 0; i < add_rtskbs; i++) {
        if (new_rtskb(pool) != 0)
            break;
    }

    return i;
}



unsigned int rtskb_pool_shrink(struct rtskb_head *pool,
                               unsigned int rem_rtskbs)
{
    unsigned int i;
    struct rtskb *skb;

    for (i = 0; i < rem_rtskbs; i++) {
        if ( (skb = rtskb_dequeue(pool)) == NULL)
            break;
        dispose_rtskb(skb);
    }

    return i;
}



int rtskb_global_pool_init(void)
{
    if (rtskb_max_size < DEFAULT_MAX_RTSKB_SIZE)
        return -EINVAL;

    if (rtskb_pool_init(&global_pool, global_rtskbs) < global_rtskbs) {
        rtskb_pool_release(&global_pool);
        return -ENOMEM;
    }

    return 0;
}
