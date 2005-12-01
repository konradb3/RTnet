/***
 *
 *  stack/stack_mgr.c - Stack-Manager
 *
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2005 Jan Kiszka <jan.kiszka@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <rtdev.h>
#include <rtnet_internal.h>
#include <stack_mgr.h>


static unsigned int stack_mgr_prio = RTNET_DEF_STACK_PRIORITY;
MODULE_PARM(stack_mgr_prio, "i");
MODULE_PARM_DESC(stack_mgr_prio, "Priority of the stack manager task");


static struct rtskb_queue rxqueue;

struct list_head    rt_packets[RTPACKET_HASH_TBL_SIZE];
rtdm_lock_t         rt_packets_lock = RTDM_LOCK_UNLOCKED;



/***
 *  rtdev_add_pack:         add protocol (Layer 3)
 *  @pt:                    the new protocol
 */
int rtdev_add_pack(struct rtpacket_type *pt)
{
    struct rtpacket_type    *pt_entry;
    int                     hash;
    int                     ret = 0;
    rtdm_lockctx_t          context;


    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    INIT_LIST_HEAD(&pt->list_entry);
    pt->refcount = 0;

    hash = ntohs(pt->type) & RTPACKET_HASH_KEY_MASK;

    rtdm_lock_get_irqsave(&rt_packets_lock, context);

    list_for_each_entry(pt_entry, &rt_packets[hash], list_entry) {
        if (unlikely(pt_entry->type == pt->type)) {
            ret = -EADDRNOTAVAIL;
            goto unlock_out;
        }
    }
    list_add_tail(&pt->list_entry, &rt_packets[hash]);

  unlock_out:
    rtdm_lock_put_irqrestore(&rt_packets_lock, context);

    return ret;
}



/***
 *  rtdev_remove_pack:  remove protocol (Layer 3)
 *  @pt:                protocol
 */
int rtdev_remove_pack(struct rtpacket_type *pt)
{
    rtdm_lockctx_t  context;
    int             ret = 0;


    RTNET_ASSERT(pt != NULL, return -EINVAL;);

    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    rtdm_lock_get_irqsave(&rt_packets_lock, context);

    if (pt->refcount > 0)
        ret = -EAGAIN;
    else
        list_del(&pt->list_entry);

    rtdm_lock_put_irqrestore(&rt_packets_lock, context);

    return ret;
}



/***
 *  rtnetif_rx: will be called from the driver interrupt handler
 *  (IRQs disabled!) and send a message to rtdev-owned stack-manager
 *
 *  @skb - the packet
 */
void rtnetif_rx(struct rtskb *skb)
{
    struct rtnet_device *rtdev;


    RTNET_ASSERT(skb != NULL, return;);
    RTNET_ASSERT(skb->rtdev != NULL, return;);

    rtdev = skb->rtdev;
    rtdev_reference(rtdev);

    rtdm_lock_get(&rxqueue.lock);
    if (rtdev->rxqueue_len < DROPPING_RTSKB) {
        rtdev->rxqueue_len++;
        __rtskb_queue_tail(&rxqueue, skb);
        rtdm_lock_put(&rxqueue.lock);
    }
    else {
        rtdm_lock_put(&rxqueue.lock);
        rtdm_printk("RTnet: dropping packet in %s()\n", __FUNCTION__);
        kfree_rtskb(skb);
    }
}




/***
 *  rtnetif_tx: will be called from the  driver
 *  and send a message to rtdev-owned stack-manager
 *
 *  @rtdev - the network-device
 */
void rtnetif_tx(struct rtnet_device *rtdev)
{
}



/***
 *      stackmgr_task
 */
static void stackmgr_task(void *arg)
{
    rtdm_event_t            *mgr_event = &((struct rtnet_mgr *)arg)->event;
    struct rtskb            *skb;
    unsigned short          hash;
    struct rtpacket_type    *pt_entry;
    rtdm_lockctx_t          context;
    struct rtnet_device     *rtdev;


    while (rtdm_event_wait(mgr_event) == 0)
        while (1) {
          next_packet:
            rtdm_lock_get_irqsave(&rxqueue.lock, context);

            skb = __rtskb_dequeue(&rxqueue);
            if (!skb) {
                rtdm_lock_put_irqrestore(&rxqueue.lock, context);
                break;
            }
            rtdev = skb->rtdev;
            rtdev->rxqueue_len--;
            rtdm_lock_put_irqrestore(&rxqueue.lock, context);

            rtcap_report_incoming(skb);

            skb->nh.raw = skb->data;

            hash = ntohs(skb->protocol) & RTPACKET_HASH_KEY_MASK;

            rtdm_lock_get_irqsave(&rt_packets_lock, context);

            list_for_each_entry(pt_entry, &rt_packets[hash], list_entry)
                if (pt_entry->type == skb->protocol) {
                    pt_entry->refcount++;
                    rtdm_lock_put_irqrestore(&rt_packets_lock, context);

                    pt_entry->handler(skb, pt_entry);

                    rtdm_lock_get_irqsave(&rt_packets_lock, context);
                    pt_entry->refcount--;
                    rtdm_lock_put_irqrestore(&rt_packets_lock, context);

                    rtdev_dereference(rtdev);
                    goto next_packet;
                }

            rtdm_lock_put_irqrestore(&rt_packets_lock, context);

            /* don't warn if running in promiscuous mode (RTcap...?) */
            if ((rtdev->flags & IFF_PROMISC) == 0)
                rtdm_printk("RTnet: unknown layer-3 protocol\n");

            kfree_rtskb(skb);
            rtdev_dereference(rtdev);
        }
}



/***
 *  rt_stack_connect
 */
void rt_stack_connect (struct rtnet_device *rtdev, struct rtnet_mgr *mgr)
{
    rtdev->stack_event = &mgr->event;
}


/***
 *  rt_stack_disconnect
 */
void rt_stack_disconnect (struct rtnet_device *rtdev)
{
    rtdev->stack_event = NULL;
}


/***
 *  rt_stack_mgr_init
 */
int rt_stack_mgr_init (struct rtnet_mgr *mgr)
{
    int i;


    rtskb_queue_init(&rxqueue);

    for (i = 0; i < RTPACKET_HASH_TBL_SIZE; i++)
        INIT_LIST_HEAD(&rt_packets[i]);

    rtdm_event_init(&mgr->event, 0);

    return rtdm_task_init(&mgr->task, "rtnet-stack", stackmgr_task, mgr,
                          stack_mgr_prio, 0);
}



/***
 *  rt_stack_mgr_delete
 */
void rt_stack_mgr_delete (struct rtnet_mgr *mgr)
{
    rtdm_event_destroy(&mgr->event);
    rtdm_task_join_nrt(&mgr->task, 100);
}


EXPORT_SYMBOL(rtdev_add_pack);
EXPORT_SYMBOL(rtdev_remove_pack);

EXPORT_SYMBOL(rtnetif_rx);
EXPORT_SYMBOL(rt_mark_stack_mgr);
EXPORT_SYMBOL(rtnetif_tx);

EXPORT_SYMBOL(rt_stack_connect);
EXPORT_SYMBOL(rt_stack_disconnect);

EXPORT_SYMBOL(rt_packets);
EXPORT_SYMBOL(rt_packets_lock);
