/* ip_fragment.c
 *
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 * Extended 2003 by Mathias Koehrer <mathias_koehrer@yahoo.de>
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


#include <net/checksum.h>
#include <rtdev.h>
#include <rtnet_internal.h>

#include <linux/ip.h>
#include <linux/in.h>


/* This defined sets the number incoming fragmented IP messages that
 * can be handled parallel.
 * */
#define COLLECTOR_COUNT 4

#define GARBAGE_COLLECT_LIMIT 50

struct collector_str
{
    int   in_use;
    __u32 saddr;
    __u32 daddr;
    __u16 id;
    __u8  protocol;

    struct rtskb *skb;
    unsigned int collected;
    unsigned int last_access;
};
static struct collector_str collector[COLLECTOR_COUNT];

static unsigned int counter = 0;

static SEM collector_sem;



/*
 * Return a pointer to the collector that holds the message that
 * fits to the iphdr (param iph).
 * If no collector can be found, a new one is created.
 * */
static struct collector_str * get_collector(struct iphdr *iph)
{
    int i;
    struct collector_str *p_coll;

    rt_sem_wait(&collector_sem);

    /* Search in existing collectors... */
    for (i=0; i < COLLECTOR_COUNT; i++)
    {
        p_coll = &collector[i];
        if ( p_coll->in_use  &&    
             iph->saddr == p_coll->saddr  &&
             iph->daddr == p_coll->daddr  &&
             iph->id    == p_coll->id     && 
             iph->protocol == p_coll->protocol )
        {
            goto success_out;
        }
    }

    /* Nothing found. Create a new one... */
    for (i=0; i < COLLECTOR_COUNT; i++)
    {
        if ( ! collector[i].in_use )
        {
            collector[i].in_use = 1;

            p_coll = &collector[i];
            p_coll->last_access = counter;
            p_coll = &collector[i];
            p_coll->collected = 0;
            if (!(p_coll->skb = alloc_rtskb(rtskb_max_size, &global_pool)))
            {
                collector[i].in_use = 0;
                goto error_out;
            }
            p_coll->saddr = iph->saddr;
            p_coll->daddr = iph->daddr;
            p_coll->id    = iph->id;
            p_coll->protocol = iph->protocol;
            goto success_out;
        }
    }

error_out:
    rt_sem_signal(&collector_sem);
    rt_printk("RTnet: IP fragmentation - no collector available\n");
    return NULL;

success_out:
    rt_sem_signal(&collector_sem);
    return p_coll;
}


static void cleanup_collector(void)
{
    int i;
    for (i=0; i < COLLECTOR_COUNT; i++)
    {
        if ( collector[i].in_use &&
             collector[i].skb )
        {
            collector[i].in_use = 0;
            kfree_rtskb(collector[i].skb);
            collector[i].skb = NULL;
        }
    }
}

/*
 * This is a very simple version of a garbage collector.
 * Whenver the last access to any of the collectors is a while ago,
 * the collector will be freed... 
 * Under normal conditions, it should never be necessary to collect
 * the garbage. 
 * */
static void garbage_collect(void)
{
    /* Kick off all collectors that are not in use anymore... */
    int i;
    for (i=0; i < COLLECTOR_COUNT; i++)
    {
        if ( collector[i].in_use && 
             (counter - collector[i].last_access > GARBAGE_COLLECT_LIMIT))
        {
            rt_printk("RTnet: IP fragmentation garbage collection (saddr:%x, daddr:%x)\n",
                        collector[i].saddr, collector[i].daddr);
            kfree_rtskb(collector[i].skb);
            collector[i].skb = NULL;
            collector[i].in_use = 0;
        }
    }
}


/* 
 * This function returns an rtskb that contains the complete, accumulated IP message.
 * If not all fragments of the IP message have been received yet, it returns NULL 
 * */
struct rtskb *rt_ip_defrag(struct rtskb *skb)
{
    unsigned int offset;
    unsigned int end;
    unsigned int flags;
    unsigned int ihl;

    struct iphdr *iph = skb->nh.iph;
    struct collector_str *p_coll = 0;


    counter++;

    /* Check, if there is already a "collector" for this connection: */
    p_coll = get_collector(iph);
    if (! p_coll )
    {
        /* Not able to create a collector.
         * Stop and discard skb */
        kfree_rtskb(skb);
        return NULL;
    }
    p_coll->last_access = counter;


    /* Write the data to the collector */
    offset = ntohs(iph->frag_off);
    flags = offset & ~IP_OFFSET;
    offset &= IP_OFFSET;
    offset <<= 3;   /* offset is in 8-byte chunks */
    ihl = iph->ihl * 4;
    end = offset + skb->len ;

    if (end > rtskb_max_size)
    {
        struct rtskb *temp_skb = p_coll->skb;

        rt_printk("RTnet: discarding incoming IP fragment (offset %i, end:%i)\n", 
                  offset, end);
        kfree_rtskb(skb);
        p_coll->skb = NULL;
        p_coll->in_use = 0;
        kfree_rtskb(temp_skb);
        return NULL;
    }
  
    /* Copy data: */
    memcpy(p_coll->skb->buf_start + ihl + offset, skb->data + ihl, skb->len - ihl);
    p_coll->collected += skb->len - ihl;


    /* Is this the final fragment? */
    if ((flags & IP_MF) == 0)
    {
        /* Determine complete skb length (including header) */
        p_coll->skb->data_len = offset + skb->len;
    }

    /* Is this the first fragment? */
    if (offset == 0)
    {
        /* Copy the header to the collector skb */
        memcpy(p_coll->skb->buf_start, skb->data, ihl);
        p_coll->collected += ihl;

        /* Set the pointers in the collector skb: */
        p_coll->skb->data =   p_coll->skb->buf_start;

        p_coll->skb->nh.iph = (struct iphdr*) p_coll->skb->buf_start;
        p_coll->skb->h.raw  = p_coll->skb->buf_start + ihl;
    }


    /* skb is no longer needed. Free it. */
    kfree_rtskb(skb);


    /* All data bytes received? */
    if (p_coll->collected == p_coll->skb->data_len)
    {
        /* Return p_coll->skb */
        struct rtskb *ret_skb = p_coll->skb;
        ret_skb->nh.iph->tot_len = htons(ret_skb->data_len);
        p_coll->skb = NULL;
        p_coll->in_use = 0;

        garbage_collect();
        return ret_skb;
    }


    /* Not all bytes received, return NULL */
    return NULL;
}
  
void rt_ip_fragment_init(void)
{
    int i;
    rt_typed_sem_init(&collector_sem, 1, BIN_SEM);

    for (i=0; i < COLLECTOR_COUNT; i++)
    {
        collector[i].in_use = 0;
        collector[i].skb = NULL;
    }
}

void rt_ip_fragment_cleanup(void)
{
    rt_sem_delete(&collector_sem);
    cleanup_collector();
}
