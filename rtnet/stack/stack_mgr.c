/* stack_mgr.c - Stack-Manager
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

#include <rtdev.h>
#include <rtnet_internal.h>
#include <stack_mgr.h>


static struct rtskb_queue rxqueue;

struct list_head    rt_packets[RTPACKET_HASH_TBL_SIZE];
rtos_spinlock_t     rt_packets_lock = RTOS_SPIN_LOCK_UNLOCKED;



/***
 *  rtdev_add_pack:         add protocol (Layer 3)
 *  @pt:                    the new protocol
 */
int rtdev_add_pack(struct rtpacket_type *pt)
{
    struct rtpacket_type    *pt_entry;
    int                     hash;
    int                     ret = 0;
    unsigned long           flags;


    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    INIT_LIST_HEAD(&pt->list_entry);
    pt->refcount = 0;

    hash = ntohs(pt->type) & RTPACKET_HASH_KEY_MASK;

    rtos_spin_lock_irqsave(&rt_packets_lock, flags);

    list_for_each_entry(pt_entry, &rt_packets[hash], list_entry) {
        if (unlikely(pt_entry->type == pt->type)) {
            ret = -EADDRNOTAVAIL;
            goto unlock_out;
        }
    }
    list_add_tail(&pt->list_entry, &rt_packets[hash]);

  unlock_out:
    rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

    return ret;
}



/***
 *  rtdev_remove_pack:  remove protocol (Layer 3)
 *  @pt:                protocol
 */
int rtdev_remove_pack(struct rtpacket_type *pt)
{
    unsigned long   flags;
    int             ret = 0;


    RTNET_ASSERT(pt != NULL, return -EINVAL;);

    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    rtos_spin_lock_irqsave(&rt_packets_lock, flags);

    if (pt->refcount > 0)
        ret = -EAGAIN;
    else
        list_del(&pt->list_entry);

    rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

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

    rtos_spin_lock(&rxqueue.lock);
    if (rtdev->rxqueue_len < DROPPING_RTSKB) {
        rtdev->rxqueue_len++;
        __rtskb_queue_tail(&rxqueue, skb);
        rtos_spin_unlock(&rxqueue.lock);
    }
    else {
        rtos_spin_unlock(&rxqueue.lock);
        rtos_print("RTnet: dropping packet in %s()\n", __FUNCTION__);
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
 *      do_stacktask
 */
static void do_stacktask(int mgr_id)
{
    struct rtnet_mgr        *mgr = (struct rtnet_mgr *)mgr_id;
    struct rtskb            *skb;
    unsigned short          hash;
    struct rtpacket_type    *pt_entry;
    unsigned long           flags;
    struct rtnet_device     *rtdev;


    rtos_print("RTnet: stack-mgr started\n");
    while(1) {
        if (RTOS_EVENT_ERROR(rtos_event_sem_wait(&mgr->event)))
            return;

        while (1) {
          next_packet:
            rtos_spin_lock_irqsave(&rxqueue.lock, flags);

            skb = __rtskb_dequeue(&rxqueue);
            if (!skb) {
                rtos_spin_unlock_irqrestore(&rxqueue.lock, flags);
                break;
            }
            rtdev = skb->rtdev;
            rtdev->rxqueue_len--;
            rtos_spin_unlock_irqrestore(&rxqueue.lock, flags);

            rtcap_report_incoming(skb);

            skb->nh.raw = skb->data;

            hash = ntohs(skb->protocol) & RTPACKET_HASH_KEY_MASK;

            rtos_spin_lock_irqsave(&rt_packets_lock, flags);

            list_for_each_entry(pt_entry, &rt_packets[hash], list_entry)
                if (pt_entry->type == skb->protocol) {
                    pt_entry->refcount++;
                    rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

                    pt_entry->handler(skb, pt_entry);

                    rtos_spin_lock_irqsave(&rt_packets_lock, flags);
                    pt_entry->refcount--;
                    rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

                    rtdev_dereference(rtdev);
                    goto next_packet;
                }

            rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

            /* don't warn if running in promiscuous mode (RTcap...?) */
            if ((rtdev->flags & IFF_PROMISC) == 0)
                rtos_print("RTnet: unknown layer-3 protocol\n");

            kfree_rtskb(skb);
            rtdev_dereference(rtdev);
        }
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

    rtos_event_sem_init(&mgr->event);

    return rtos_task_init(&mgr->task, do_stacktask, (int)mgr,
                          RTNET_STACK_PRIORITY);
}



/***
 *  rt_stack_mgr_delete
 */
void rt_stack_mgr_delete (struct rtnet_mgr *mgr)
{
    rtos_event_sem_delete(&mgr->event);
    rtos_task_delete(&mgr->task);
}
