/***
 *
 *  stack/stack_mgr.c - Stack-Manager
 *
 *  Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2005, 2006 Jan Kiszka <jan.kiszka@web.de>
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

#include <linux/moduleparam.h>

#include <rtdev.h>
#include <rtnet_internal.h>
#include <rtskb_fifo.h>
#include <stack_mgr.h>


static unsigned int stack_mgr_prio = RTNET_DEF_STACK_PRIORITY;
module_param(stack_mgr_prio, uint, 0444);
MODULE_PARM_DESC(stack_mgr_prio, "Priority of the stack manager task");


#if (CONFIG_RTNET_RX_FIFO_SIZE & (CONFIG_RTNET_RX_FIFO_SIZE-1)) != 0
#error CONFIG_RTNET_RX_FIFO_SIZE must be power of 2!
#endif
static DECLARE_RTSKB_FIFO(rx, CONFIG_RTNET_RX_FIFO_SIZE);

struct list_head    rt_packets[RTPACKET_HASH_TBL_SIZE];
rtdm_lock_t         rt_packets_lock = RTDM_LOCK_UNLOCKED;



/***
 *  rtdev_add_pack:         add protocol (Layer 3)
 *  @pt:                    the new protocol
 */
int rtdev_add_pack(struct rtpacket_type *pt)
{
    int                     hash;
    int                     ret = 0;
    rtdm_lockctx_t          context;


    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    INIT_LIST_HEAD(&pt->list_entry);
    pt->refcount = 0;

    hash = ntohs(pt->type) & RTPACKET_HASH_KEY_MASK;

    rtdm_lock_get_irqsave(&rt_packets_lock, context);
    list_add_tail(&pt->list_entry, &rt_packets[hash]);
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

    if (unlikely(rtskb_fifo_insert_inirq(&rx.fifo, skb) < 0)) {
        rtdm_printk("RTnet: dropping packet in %s()\n", __FUNCTION__);
        kfree_rtskb(skb);
        rtdev_dereference(rtdev);
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
    int                     err;


    while (rtdm_event_wait(mgr_event) == 0)
        while (1) {
          next_packet:
            /* we are the only reader => no locking required */
            skb = __rtskb_fifo_remove(&rx.fifo);
            if (!skb)
                break;

            rtdev = skb->rtdev;

            rtcap_report_incoming(skb);

            skb->nh.raw = skb->data;

            hash = ntohs(skb->protocol) & RTPACKET_HASH_KEY_MASK;

            rtdm_lock_get_irqsave(&rt_packets_lock, context);

            list_for_each_entry(pt_entry, &rt_packets[hash], list_entry)
                if (pt_entry->type == skb->protocol) {
                    pt_entry->refcount++;
                    rtdm_lock_put_irqrestore(&rt_packets_lock, context);

                    err = pt_entry->handler(skb, pt_entry);

                    rtdm_lock_get_irqsave(&rt_packets_lock, context);
                    pt_entry->refcount--;
                    rtdm_lock_put_irqrestore(&rt_packets_lock, context);

                    rtdev_dereference(rtdev);
                    if (!err)
                        goto next_packet;
                }

            rtdm_lock_put_irqrestore(&rt_packets_lock, context);

            /* don't warn if running in promiscuous mode (RTcap...?) */
            if ((rtdev->flags & IFF_PROMISC) == 0)
                rtdm_printk("RTnet: no one cared for packet with layer 3 "
                            "protocol type 0x%04x\n", ntohs(skb->protocol));

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


    rtskb_fifo_init(&rx.fifo, CONFIG_RTNET_RX_FIFO_SIZE);

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
