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
static struct rtpacket_type *rt_packets[MAX_RT_PROTOCOLS];
static rtos_spinlock_t rt_packets_lock = RTOS_SPIN_LOCK_UNLOCKED;



/***
 *  rtdev_add_pack:         add protocol (Layer 3)
 *  @pt:                    the new protocol
 */
int rtdev_add_pack(struct rtpacket_type *pt)
{
    int hash;
    unsigned long flags;


    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    hash = ntohs(pt->type) & (MAX_RT_PROTOCOLS-1);

    rtos_spin_lock_irqsave(&rt_packets_lock, flags);

    if (rt_packets[hash] == NULL) {
        rt_packets[hash] = pt;

        pt->refcount = 0;

        rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

        return 0;
    }
    else {
        rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

        rtos_print("RTnet: protocol place %d is already in use\n", hash);
        return -EADDRNOTAVAIL;
    }
}



/***
 *  rtdev_remove_pack:  remove protocol (Layer 3)
 *  @pt:                protocol
 */
int rtdev_remove_pack(struct rtpacket_type *pt)
{
    int hash;
    unsigned long flags;
    int ret = 0;


    RTNET_ASSERT(pt != NULL, return -EINVAL;);

    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    hash = ntohs(pt->type) & (MAX_RT_PROTOCOLS-1);

    rtos_spin_lock_irqsave(&rt_packets_lock, flags);

    if ((rt_packets[hash] != NULL) &&
        (rt_packets[hash]->type == pt->type)) {
        rt_packets[hash] = NULL;

        if (pt->refcount > 0)
            ret = -EAGAIN;

        rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

        return ret;
    }
    else {
        rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

        rtos_print("RTnet: protocol %s not found\n",
                   (pt->name) ? (pt->name) : "<noname>");

        return -ENOENT;
    }
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
    struct rtpacket_type    *pt;
    unsigned long           flags;
    struct rtnet_device     *rtdev;


    rtos_print("RTnet: stack-mgr started\n");
    while(1) {
        rtos_event_wait(&mgr->event);

        while (1) {
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

            hash = ntohs(skb->protocol) & (MAX_RT_PROTOCOLS-1);

            rtos_spin_lock_irqsave(&rt_packets_lock, flags);

            pt = rt_packets[hash];

            if ((pt != NULL) && (pt->type == skb->protocol)) {
                pt->refcount++;
                rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

                pt->handler(skb, pt);

                rtos_spin_lock_irqsave(&rt_packets_lock, flags);
                pt->refcount--;
                rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);
            } else {
                rtos_spin_unlock_irqrestore(&rt_packets_lock, flags);

                /* don't warn if running in promiscuous mode (RTcap...?) */
                if ((rtdev->flags & IFF_PROMISC) == 0)
                    rtos_print("RTnet: unknown layer-3 protocol\n");

                kfree_rtskb(skb);
            }

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
    rtskb_queue_init(&rxqueue);

    rtos_event_init(&mgr->event);

    return rtos_task_init(&mgr->task, do_stacktask, (int)mgr,
                          RTNET_STACK_PRIORITY);
}



/***
 *  rt_stack_mgr_delete
 */
void rt_stack_mgr_delete (struct rtnet_mgr *mgr)
{
    rtos_task_delete(&mgr->task);
    rtos_event_delete(&mgr->event);
}
