/* rtskb.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>,
 *               2003 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/skbuff.h>

#include <rtnet_internal.h>


struct page;

struct rtskb_head;
struct rtsocket;
struct rtnet_device;

/***
 *  rtskb - realtime socket buffer
 */
struct rtskb {
    struct rtskb        *next;

    struct rtskb_head   *head;
    struct rtskb_head   *pool;

    unsigned int        priority;

    struct rtsocket     *sk;
    struct rtnet_device *rtdev;

    /* transport layer */
    union
    {
        struct tcphdr   *th;
        struct udphdr   *uh;
        struct icmphdr  *icmph;
        struct iphdr    *ipihdr;
        unsigned char   *raw;
    } h;

    /* network layer */
    union
    {
        struct iphdr    *iph;
        struct arphdr   *arph;
        unsigned char   *raw;
    } nh;

    /* link layer */
    union
    {
        struct ethhdr   *ethernet;
        unsigned char   *raw;
    } mac;

    unsigned short      protocol;
    unsigned char       pkt_type;

    unsigned int        csum;
    unsigned char       ip_summed;

    struct rt_rtable    *dst;

    unsigned int        len;
    unsigned int        data_len;
    unsigned int        buf_len;

    unsigned char       *buf_start;
    unsigned char       *buf_end;

    unsigned char       *data;
    unsigned char       *tail;
    unsigned char       *end;
    RTIME               rx;

#ifdef CONFIG_RTAI_MM_VMALLOC
    unsigned char       *buf_page_addr;
#endif
};

struct rtskb_head {
    struct rtskb        *first;
    struct rtskb        *last;

    unsigned int        qlen;
    spinlock_t          lock;
};

#define QUEUE_MAX_PRIO          0
#define QUEUE_MIN_PRIO          31

struct rtskb_prio_list {
    spinlock_t          lock;
    __u32               usage;
    unsigned int        qlen;
    struct rtskb_head   queue[QUEUE_MIN_PRIO+1];
};


/* default values for the module parameter */
#define DEFAULT_GLOBAL_RTSKBS   0       /* default number of rtskb's in global pool */
#define DEFAULT_DEVICE_RTSKBS   16      /* default additional rtskbs per network adapter */
#define DEFAULT_SOCKET_RTSKBS   16      /* default number of rtskb's in socket pools */
#define DEFAULT_MAX_RTSKB_SIZE  1544    /* maximum space, needed by pcnet32-rt */

#ifdef CONFIG_RTAI_MM_VMALLOC
    /* sanity check */
    #if PAGE_SIZE < DEFAULT_MAX_RTSKB_SIZE
        #error The page size is smaller than the rtskb size. This may prevent
        #error DMA mapping of non-continuous rtskbs. You have to disable
        #error CONFIG_RTAI_MM_VMALLOC in RTAI to compile RTnet for the selected
        #error architecture.
    #endif

    /* This macro calculates the kernel virtual address which can be passed to
     * pci_map_single(). */
    #define RTSKB_KVA(skb, addr) \
        (void*)((unsigned long)skb->buf_page_addr | ((unsigned long)addr & (PAGE_SIZE-1)))

    /* The rtskb structure and its data buffer are allocated as one chunk. If
     * vmalloc'ed memory is used for rtskb's, the buffer has to fit into a
     * single page to allow DMA mapping. This macro extends the rtskb structure
     * size in order to be able to adjust the buffer start appropriately. */
    #define ALIGN_RTSKB_STRUCT_LEN      SKB_DATA_ALIGN(sizeof(struct rtskb)) + \
        SKB_DATA_ALIGN(DEFAULT_MAX_RTSKB_SIZE)
#else
    /* Addresses already have correct format */
    #define RTSKB_KVA(skb, addr) (addr)

    /* This macro aligns the structure size to allow the data buffer alignment
     * according to DATA_BUF_ALING. */
    #define ALIGN_RTSKB_STRUCT_LEN      SKB_DATA_ALIGN(sizeof(struct rtskb))
#endif

extern unsigned int socket_rtskbs;      /* default number of rtskb's in socket pools */
extern unsigned int rtskb_max_size;     /* rtskb data buffer size */

extern unsigned int rtskb_pools;        /* current number of rtskb pools      */
extern unsigned int rtskb_pools_max;    /* maximum number of rtskb pools      */
extern unsigned int rtskb_amount;       /* current number of allocated rtskbs */
extern unsigned int rtskb_amount_max;   /* maximum number of allocated rtskbs */

extern void rtskb_over_panic(struct rtskb *skb, int len, void *here);
extern void rtskb_under_panic(struct rtskb *skb, int len, void *here);

extern struct rtskb *alloc_rtskb(unsigned int size, struct rtskb_head *pool);
#define dev_alloc_rtskb(len, pool)  alloc_rtskb(len, pool)

extern void kfree_rtskb(struct rtskb *skb);
#define dev_kfree_rtskb(a)  kfree_rtskb(a)


/***
 *  rtsk_queue_head_init - initialize the queue
 *  @list
 */
static inline void rtskb_queue_head_init(struct rtskb_head *list)
{
    spin_lock_init(&list->lock);
    list->first = NULL;
    list->last  = NULL;
    list->qlen = 0;
}

/***
 *  rtsk_prio_queue_head_init - initialize the prioritized queue
 *  @list
 */
static inline void rtskb_prio_list_init(struct rtskb_prio_list *list)
{
    memset(list, 0, sizeof(struct rtskb_prio_list));
    spin_lock_init(&list->lock);
}

/***
 *  rtsk_queue_len
 *  @list
 */
static inline int rtskb_queue_len(struct rtskb_head *list)
{
    return (list->qlen);
}

/***
 *  rtsk_queue_empty
 *  @list
 */
static inline int rtskb_queue_empty(struct rtskb_head *list)
{
    return (list->qlen == 0);
}

/***
 *  __rtskb_queue_head - queue a buffer at the list head (w/o locks)
 *  @list: list to use
 *  @skb: buffer to queue
 */
static inline void __rtskb_queue_head(struct rtskb_head *list,
                                      struct rtskb *skb)
{
    skb->head = list;
    skb->next = list->first;

    list->first = skb;
    if (list->qlen == 0)
        list->last = skb;
    list->qlen++;
}

/***
 *  rtskb_queue_head - queue a buffer at the list head (lock protected)
 *  @list: list to use
 *  @skb: buffer to queue
 */
static inline void rtskb_queue_head(struct rtskb_head *list, struct rtskb *skb)
{
    unsigned long flags;
    flags = rt_spin_lock_irqsave(&list->lock);
    __rtskb_queue_head(list, skb);
    rt_spin_unlock_irqrestore(flags, &list->lock);
}

/***
 *  rtskb_prio_queue_head - queue a buffer at the prioritized list head
 *  @list: list to use
 *  @skb: buffer to queue
 */
static inline void rtskb_prio_queue_head(struct rtskb_prio_list *list,
                                         struct rtskb *skb)
{
    unsigned long flags;

    ASSERT(skb->priority <= 31, skb->priority = 31;);

    flags = rt_spin_lock_irqsave(&list->lock);
    __rtskb_queue_head(&list->queue[skb->priority], skb);
    __set_bit(skb->priority, &list->usage);
    list->qlen++;
    rt_spin_unlock_irqrestore(flags, &list->lock);
}

/***
 *  __rtskb_queue_tail - queue a buffer at the list tail (w/o locks)
 *  @list: list to use
 *  @skb: buffer to queue
 */
static inline void __rtskb_queue_tail(struct rtskb_head *list,
                                      struct rtskb *skb)
{
    skb->head = list;
    skb->next = NULL;

    if (list->qlen == 0) {
        list->first = list->last = skb;
    } else {
        list->last->next = skb;
        list->last = skb;
    }
    list->qlen++;
}

/***
 *  rtskb_queue_tail - queue a buffer at the list tail (lock protected)
 *  @list: list to use
 *  @skb: buffer to queue
 */
static inline void rtskb_queue_tail(struct rtskb_head *list, struct rtskb *skb)
{
    unsigned long flags;
    flags = rt_spin_lock_irqsave(&list->lock);
    __rtskb_queue_tail(list, skb);
    rt_spin_unlock_irqrestore(flags, &list->lock);
}

/***
 *  rtskb_prio_queue_tail - queue a buffer at the prioritized list tail
 *  @list: list to use
 *  @skb: buffer to queue
 */
static inline void rtskb_prio_queue_tail(struct rtskb_prio_list *list,
                                         struct rtskb *skb)
{
    unsigned long flags;

    ASSERT(skb->priority <= 31, skb->priority = 31;);

    flags = rt_spin_lock_irqsave(&list->lock);
    __rtskb_queue_tail(&list->queue[skb->priority], skb);
    __set_bit(skb->priority, &list->usage);
    list->qlen++;
    rt_spin_unlock_irqrestore(flags, &list->lock);
}

/***
 *  __rtskb_dequeue - remove from the head of the queue (w/o locks)
 *  @list: list to dequeue from
 */
static inline struct rtskb *__rtskb_dequeue(struct rtskb_head *list)
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
 *  rtskb_dequeue - remove from the head of the queue (lock protected)
 *  @list: list to dequeue from
 */
static inline struct rtskb *rtskb_dequeue(struct rtskb_head *list)
{
    unsigned long flags;
    struct rtskb *result;

    flags = rt_spin_lock_irqsave(&list->lock);
    result = __rtskb_dequeue(list);
    rt_spin_unlock_irqrestore(flags, &list->lock);

    return result;
}

/***
 *  rtskb_prio_dequeue - remove from the head of the prioritized queue
 *  @list: list to dequeue from
 */
static inline struct rtskb *rtskb_prio_dequeue(struct rtskb_prio_list *list)
{
    unsigned long flags;
    int prio;
    struct rtskb *result = NULL;
    struct rtskb_head *sub_list;

    flags = rt_spin_lock_irqsave(&list->lock);
    if (list->usage) {
        prio     = ffz(~list->usage);
        sub_list = &list->queue[prio];
        result   = __rtskb_dequeue(sub_list);
        if (rtskb_queue_empty(sub_list))
            __change_bit(prio, &list->usage);
        list->qlen--;
    }
    rt_spin_unlock_irqrestore(flags, &list->lock);

    return result;
}

/***
 *  rtskb_queue_purge - clean the queue
 *  @list
 */
static inline void rtskb_queue_purge(struct rtskb_head *list)
{
    struct rtskb *skb;
    while ( (skb=rtskb_dequeue(list))!=NULL )
        kfree_rtskb(skb);
}

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

#define RTSKB_LINEAR_ASSERT(rtskb) \
    do { if (rtskb_is_nonlinear(rtskb)) BUG(); }  while (0)

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
    return skb->data+=len;
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

static inline struct rtskb *rtskb_padto(struct rtskb *rtskb, unsigned int len)
{
    ASSERT(len <= (unsigned int)(rtskb->buf_end + 1 - rtskb->data), return NULL;);

    memset(rtskb->data + rtskb->len, 0, len - rtskb->len);

    return rtskb;
}

static inline int rtskb_acquire(struct rtskb *rtskb,
                                struct rtskb_head *comp_pool)
{
    struct rtskb *comp_rtskb = rtskb_dequeue(comp_pool);

    if (!comp_rtskb)
        return -ENOMEM;

    comp_rtskb->pool = rtskb->pool;
    rtskb_queue_tail(comp_rtskb->pool, comp_rtskb);
    rtskb->pool = comp_pool;

    return 0;
}

extern struct rtskb_head global_pool;

extern unsigned int rtskb_pool_init(struct rtskb_head *pool,
                                    unsigned int initial_size);
extern void rtskb_pool_release(struct rtskb_head *pool);

extern unsigned int rtskb_pool_extend(struct rtskb_head *pool,
                                      unsigned int add_rtskbs);
extern unsigned int rtskb_pool_shrink(struct rtskb_head *pool,
                                      unsigned int rem_rtskbs);

extern int rtskb_global_pool_init(void);
#define rtskb_global_pool_release() rtskb_pool_release(&global_pool)

extern unsigned int rtskb_copy_and_csum_bits(const struct rtskb *skb,
                                             int offset, u8 *to, int len,
                                             unsigned int csum);
extern void rtskb_copy_and_csum_dev(const struct rtskb *skb, u8 *to);


#endif /* __KERNEL__ */

#endif  /* __RTSKB_H_ */
