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

#include <rtnet.h>
#include <rtnet_internal.h>

/***
 *	rt_mark_stack_mgr 
 *
 */
void rt_mark_stack_mgr(struct rtnet_device *rtdev) 
{
 	struct rtnet_msg msg;

	if (rtdev) {
		msg.msg_type=Rx_PACKET;
		msg.rtdev=rtdev;
		rt_mbx_send_if(msg.rtdev->stack_mbx, &msg, sizeof (struct rtnet_msg));
	}

}



/***
 *	rtnetif_rx: will be called from the driver
 *	and send a message to rtdev-owned stack-manager
 *
 *	@skb - the packet
 */
void rtnetif_rx(struct rtskb *skb)
{
	struct rtnet_device *rtdev;
	if (skb && (rtdev=skb->rtdev)) {
		if (rtskb_queue_len(&rtdev->rxqueue) < DROPPING_RTSKB) {
			rtskb_queue_tail(&rtdev->rxqueue, skb);
		} else {
			rt_printk("RTnet: dropping packet in %s()\n",__FUNCTION__);
			kfree_rtskb(skb);
		}
	} else {
		rt_printk("RTnet: called %s() with skb=<NULL>\n",__FUNCTION__);
	}
}




/***
 *	rtnetif_tx: will be called from the  driver
 *	and send a message to rtdev-owned stack-manager
 *
 *	@rtdev - the network-device
 */
void rtnetif_tx(struct rtnet_device *rtdev)
{
  //	rt_sem_signal(&(rtdev->txsem));
}




char *msg = "neue Reihe\n";

/***
 *      do_stacktask
 */
static void do_stacktask(int mgr_id)
{
        struct rtnet_msg msg;
	struct rtnet_mgr *mgr = (struct rtnet_mgr *)mgr_id;

        rt_printk("RTnet: stack-mgr started\n");
        while(1) {
		rt_mbx_receive(&(mgr->mbx), &msg, sizeof(struct rtnet_msg));
		
                if ( (msg.rtdev) && (msg.msg_type==Rx_PACKET) ) {
			while ( !rtskb_queue_empty(&msg.rtdev->rxqueue) ) {
	                        struct rtskb *skb = rtskb_dequeue(&msg.rtdev->rxqueue);
		                if ( skb ) {
			                unsigned short hash = ntohs(skb->protocol) & (MAX_RT_PROTOCOLS-1);
				        struct rtpacket_type *pt = rt_packets[hash];
					skb->nh.raw = skb->data;
	                                if (pt) {
		                                pt->handler (skb, skb->rtdev, pt);
			                } else {
						rt_printk("RTnet: undefined Layer-3-Protokoll\n");
						kfree_rtskb(skb);
					}
				}
                        }
                }

        }
}



/***
 *	rt_stack_connect
 */
void rt_stack_connect (struct rtnet_device *rtdev, struct rtnet_mgr *mgr)
{
	rtdev->stack_mbx=&(mgr->mbx);
} 


/***
 *	rt_stack_disconnect
 */
void rt_stack_disconnect (struct rtnet_device *rtdev)
{
	rtdev->stack_mbx=NULL;
} 


/***
 *	rt_stack_mgr_start
 */
int rt_stack_mgr_start (struct rtnet_mgr *mgr)
{
	return (rt_task_resume(&(mgr->task)));
}



/***
 *	rt_stack_mgr_stop
 */
int rt_stack_mgr_stop (struct rtnet_mgr *mgr)
{
	return (rt_task_suspend(&(mgr->task)));
}



/***
 *	rt_stack_mgr_init
 */
int rt_stack_mgr_init (struct rtnet_mgr *mgr)
{
	int ret = 0;

	if ( (ret=rt_mbx_init (&(mgr->mbx), sizeof(struct rtnet_msg))) )
		return ret;
	if ( (ret=rt_task_init(&(mgr->task), &do_stacktask, (int)mgr, 4096, RTNET_STACK_PRIORITY, 0, 0)) )
		return ret;
	if ( (ret=rt_task_resume(&(mgr->task))) )
		return ret;
		
	return (ret);
}



/***
 *	rt_stack_mgr_delete 
 */
void rt_stack_mgr_delete (struct rtnet_mgr *mgr)
{
	rt_task_suspend(&(mgr->task));
	rt_task_delete(&(mgr->task));
	rt_mbx_delete(&(mgr->mbx));
}











