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
#include <rtai.h>
#include <rtai_sched.h>
#include <rtai_fifos.h>

#include <rtdev.h>
#include <rtnet_internal.h>
#include <stack_mgr.h>


static struct rtskb_queue rxqueue;
static struct rtpacket_type *rt_packets[MAX_RT_PROTOCOLS];
static spinlock_t rt_packets_lock;



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

    hash=ntohs(pt->type) & (MAX_RT_PROTOCOLS-1);

    flags = rt_spin_lock_irqsave(&rt_packets_lock);

    if (rt_packets[hash] == NULL) {
        rt_packets[hash] = pt;

        pt->refcount = 0;

        rt_spin_unlock_irqrestore(flags, &rt_packets_lock);

        return 0;
    }
    else {
        rt_spin_unlock_irqrestore(flags, &rt_packets_lock);

        rt_printk("RTnet: protocol place %d is already in use\n", hash);
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


    ASSERT(pt != NULL, return -EINVAL;);

    if (pt->type == htons(ETH_P_ALL))
        return -EINVAL;

    hash=ntohs(pt->type) & (MAX_RT_PROTOCOLS-1);

    flags = rt_spin_lock_irqsave(&rt_packets_lock);

    if ((rt_packets[hash] != NULL) &&
        (rt_packets[hash]->type == pt->type)) {
        rt_packets[hash] = NULL;

        if (pt->refcount > 0)
            ret = -EAGAIN;

        rt_spin_unlock_irqrestore(flags, &rt_packets_lock);

        return ret;
    }
    else {
        rt_spin_unlock_irqrestore(flags, &rt_packets_lock);

        rt_printk("RTnet: protocol %s not found\n",
                (pt->name) ? (pt->name) : "<noname>");

        return -ENOENT;
    }
}



/***
 *  rt_mark_stack_mgr
 *
 */
void rt_mark_stack_mgr(struct rtnet_device *rtdev)
{
    rt_sem_signal(rtdev->stack_sem);
}



/***
 *  rtnetif_rx: will be called from the driver
 *  and send a message to rtdev-owned stack-manager
 *
 *  @skb - the packet
 */
void rtnetif_rx(struct rtskb *skb)
{
    struct rtnet_device *rtdev;
    unsigned long flags;


    ASSERT(skb != NULL, return;);
    ASSERT(skb->rtdev != NULL, return;);
    rtdev = skb->rtdev;

    flags = rt_spin_lock_irqsave(&rxqueue.lock);
    if (rtdev->rxqueue_len < DROPPING_RTSKB) {
        rtdev->rxqueue_len++;
        __rtskb_queue_tail(&rxqueue, skb);
        rt_spin_unlock_irqrestore(flags, &rxqueue.lock);
    }
    else {
        rt_spin_unlock_irqrestore(flags, &rxqueue.lock);
        rt_printk("RTnet: dropping packet in %s()\n",__FUNCTION__);
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


    rt_printk("RTnet: stack-mgr started\n");
    while(1) {
        rt_sem_wait(&mgr->sem);

        while (1) {
            flags = rt_spin_lock_irqsave(&rxqueue.lock);

            skb = __rtskb_dequeue(&rxqueue);
            if (!skb) {
                rt_spin_unlock_irqrestore(flags, &rxqueue.lock);
                break;
            }
            skb->rtdev->rxqueue_len--;
            rt_spin_unlock_irqrestore(flags, &rxqueue.lock);

            skb->nh.raw = skb->data;

            hash = ntohs(skb->protocol) & (MAX_RT_PROTOCOLS-1);

            flags = rt_spin_lock_irqsave(&rt_packets_lock);

            pt = rt_packets[hash];

            if ((pt != NULL) && (pt->type == skb->protocol)) {
                pt->refcount++;
                rt_spin_unlock_irqrestore(flags, &rt_packets_lock);

                pt->handler(skb, pt);

                flags = rt_spin_lock_irqsave(&rt_packets_lock);
                pt->refcount--;
                rt_spin_unlock_irqrestore(flags, &rt_packets_lock);
            } else {
                rt_spin_unlock_irqrestore(flags, &rt_packets_lock);

                rt_printk("RTnet: unknown layer-3 protocol\n");
                kfree_rtskb(skb);
            }
        }
    }
}



/***
 *  rt_stack_connect
 */
void rt_stack_connect (struct rtnet_device *rtdev, struct rtnet_mgr *mgr)
{
    rtdev->stack_sem=&mgr->sem;
}


/***
 *  rt_stack_disconnect
 */
void rt_stack_disconnect (struct rtnet_device *rtdev)
{
    rtdev->stack_sem=NULL;
}


/***
 *  rt_stack_mgr_start
 */
int rt_stack_mgr_start (struct rtnet_mgr *mgr)
{
    return (rt_task_resume(&mgr->task));
}



/***
 *  rt_stack_mgr_stop
 */
int rt_stack_mgr_stop (struct rtnet_mgr *mgr)
{
    return (rt_task_suspend(&mgr->task));
}



/***
 *  rt_stack_mgr_init
 */
int rt_stack_mgr_init (struct rtnet_mgr *mgr)
{
    int ret;
    int i;


    spin_lock_init(&rt_packets_lock);
    for (i=0; i<MAX_RT_PROTOCOLS; i++) rt_packets[i]=NULL;

    rtskb_queue_init(&rxqueue);

    rt_sem_init(&mgr->sem, 0);
    if ((ret=rt_task_init(&mgr->task, &do_stacktask, (int)mgr, 4096,
                           RTNET_STACK_PRIORITY, 0, 0)))
        return ret;
    if ((ret=rt_task_resume(&mgr->task)))
        return ret;

    return 0;
}



/***
 *  rt_stack_mgr_delete
 */
void rt_stack_mgr_delete (struct rtnet_mgr *mgr)
{
    rt_task_delete(&mgr->task);
    rt_sem_delete(&mgr->sem);
}
