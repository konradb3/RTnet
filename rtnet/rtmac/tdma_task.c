/* rtmac_task.c
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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

#include <linux/netdevice.h>

#include <rtai.h>

#include <rtnet.h>
#include <rtmac.h>
#include <tdma.h>
#include <tdma_event.h>

void tdma_task_shutdown(struct rtmac_tdma *tdma)
{
	if (tdma->flags.task_active == 1) {
		tdma->flags.shutdown_task = 1;

		/*
		 * unblock all tasks by deleting semas
		 */
		//rt_sem_delete(&tdma->client_tx);	//tdma_task_client()
		rt_sem_delete(&tdma->free);		//tdma_packet_tx()

		/*
		 * re-init semas
		 */
		//rt_sem_init(&tdma->free, TDMA_MAX_TX_QUEUE);
		rt_sem_init(&tdma->client_tx, 0);
	}
}



int tdma_task_change(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle)
{
	int ret = 0;

	/*
	 * shutdown the task
	 */
	tdma_task_shutdown(tdma);
	
	ret = tdma_timer_start_task_change(tdma, task, cycle, TDMA_NOTIFY_TASK_CYCLE);

	return ret;
}



int tdma_task_change_con(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle)
{
	struct rtmac_device *rtmac = tdma->rtmac;
	struct rtnet_device *rtdev = rtmac->rtdev;
	int ret = 0;

	if (tdma->flags.task_active) {
		rt_printk("RTmac: tdma: "__FUNCTION__"() task was not shutted down.\n");
		rt_task_delete(&tdma->tx_task);
	}

	ret = rt_task_init(&tdma->tx_task, task, (int)rtdev, 4096, TDMA_PRIO_TX_TASK, 0, 0);
	
	if (cycle != 0)
		ret = rt_task_make_periodic_relative_ns(&tdma->tx_task, 1000*1000, cycle);
	else
		ret = rt_task_resume(&tdma->tx_task);

	if (ret != 0)
		rt_printk("RTmac: tdma: "__FUNCTION__"() not successful\n");
	else
		TDMA_DEBUG(2, "RTmac: tdma: "__FUNCTION__"() succsessfull\n");


	tdma->flags.task_active = 1;
	return ret;
}



void tdma_task_notify(int rtdev_id)
{
	struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtmac->priv;

	struct rtskb *skb;
	void *data;

	while (tdma->flags.shutdown_task == 0) {
		/*
		 * alloc message
		 */
		skb = tdma_make_msg(rtdev, NULL, NOTIFY_MASTER, &data);

		/*
		 * wait 'till begin of next period
		 */
		rt_task_wait_period();

		/*
		 * transmit packet
		 */
		rtdev_xmit(skb);
	}

	TDMA_DEBUG(2, "RTmac: tdma: "__FUNCTION__"() shutdown complete\n");
	tdma->flags.task_active = 0;
	tdma->flags.shutdown_task = 0;
}





void tdma_task_config(int rtdev_id)
{
	struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtmac->priv;
	struct rtskb *skb;
	struct tdma_test_msg *test_msg;
	void *data = &test_msg;
	struct tdma_rt_entry *rt_entry;
	struct list_head *lh;
	int i, max;

	max = TDMA_MASTER_MAX_TEST;

	list_for_each(lh, &tdma->rt_list) {
		rt_entry = list_entry(lh, struct tdma_rt_entry, list);

		if (rt_entry->state != RT_RCVD_CONF )
			goto out;

		TDMA_DEBUG(4, "RTmac: tdma: "__FUNCTION__"() sending %d test packets to %u.%u.%u.%u\n",
			   max, NIPQUAD(rt_entry->arp->ip_addr));

		for (i = 0; i <= max; i++) {
			if (!(rt_entry->state == RT_RCVD_CONF || rt_entry->state == RT_RCVD_TEST))
				goto out;

			TDMA_DEBUG(6, "RTmac: tdma: "__FUNCTION__"() sending test packet #%d to %u.%u.%u.%u\n",
				   i, NIPQUAD(rt_entry->arp->ip_addr));

			/*
			 * alloc skb, and put counter and time into it....
			 */
			skb = tdma_make_msg(rtdev, rt_entry->arp->hw_addr, REQUEST_TEST, data);
			rt_entry->counter = test_msg->counter = i;
			rt_entry->state = RT_SENT_TEST;
			rt_entry->tx = test_msg->tx = rt_get_time();

			/*
			 * transmit packet
			 */
			rtdev_xmit(skb);

			/*
			 * wait
			 */
			rt_task_wait_period();
		}
	}
	
	TDMA_DEBUG(3, "RTmac: tdma: "__FUNCTION__"() shutdown complete\n");
	tdma->flags.task_active = 0;
	tdma->flags.shutdown_task = 0;

	tdma_timer_start_sent_test(tdma, TDMA_MASTER_WAIT_TEST);
	tdma_next_state(tdma, TDMA_MASTER_SENT_TEST);

	return;

 out:
	rt_printk("RTmac: tdma: *** WARNING *** "__FUNCTION__"() received not ACK from station %d, IP %u.%u.%u.%u, going into DOWN state\n",
		  rt_entry->station, NIPQUAD(rt_entry->arp->ip_addr));
	tdma_cleanup_master_rt(tdma);
	tdma_next_state(tdma, TDMA_DOWN);

	tdma->flags.task_active = 0;
	tdma->flags.shutdown_task = 0;
	TDMA_DEBUG(2, "RTmac: tdma: "__FUNCTION__"() shutdown complete\n");
}





void tdma_task_master(int rtdev_id)
{
	struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtmac->priv;
	struct rtskb *skb;
	void *data;

	while (tdma->flags.shutdown_task == 0) {
		/*
		 * make, resp. get master skb
		 */
		//FIXME: rtskb_queue_empty(tdma->master_queue) enable to send real msgs...

		skb = tdma_make_msg(rtdev, NULL, START_OF_FRAME, &data);

		rt_task_wait_period();
	
		rtmac->packet_tx(skb, skb->rtdev);
	
		/*
		 * get client skb out of queue and send it
		 */

		if (rt_sem_wait_if(&tdma->full) >= 1) {
			skb = rtskb_dequeue(&tdma->tx_queue);
			rt_sem_signal(&tdma->free);

			rtmac->packet_tx(skb, skb->rtdev);
		}

	}
	TDMA_DEBUG(2, "RTmac: tdma: "__FUNCTION__"() shutdown complete\n");
	tdma->flags.task_active = 0;
	tdma->flags.shutdown_task = 0;
}




void tdma_task_client(int rtdev_id)
{
	struct rtnet_device *rtdev = (struct rtnet_device *)rtdev_id;
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtmac->priv;
	struct rtskb *skb;
       
	while(tdma->flags.shutdown_task == 0) {
		rt_sem_wait(&tdma->client_tx);

		rt_sleep_until(tdma->wakeup);

		if (rt_sem_wait_if(&tdma->full) >= 1) {
			skb = rtskb_dequeue(&tdma->tx_queue);
			rt_sem_signal(&tdma->free);
			
			rtmac->packet_tx(skb, skb->rtdev);
		}
	}

	TDMA_DEBUG(2, "RTmac: tdma: "__FUNCTION__"() shutdown complete\n");
	tdma->flags.task_active = 0;
	tdma->flags.shutdown_task = 0;
}
