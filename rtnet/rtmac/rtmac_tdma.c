/* rtmac_tdma.c
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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/netdevice.h>

#include <rtai.h>

#include <rtnet.h>
#include <rtmac.h>
#include <tdma.h>

__u32 tdma_debug = 4; //INT_MAX
MODULE_PARM(tdma_debug, "i");
MODULE_PARM_DESC(cards, "tdma debug level");




int tdma_init(struct rtnet_device *rtdev)
{
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma;

	rt_printk("RTmac: tdma: init time devision multiple access (tdma) for realtime stations\n");

	tdma = kmalloc(sizeof(struct rtmac_tdma), GFP_KERNEL);
	if (tdma == NULL) {
		rt_printk("RTmac: tdma: out of memory, cannot kmalloc rtmac->priv\n");
		return -1;	//FIXME: find better errno
	}
	memset(tdma, 0, sizeof(struct rtmac_tdma));

	rtmac->priv = tdma;
	tdma->rtmac = rtmac;

	/*
	 * init semas, they implement a producer consumer between the 
	 * sending realtime- and the driver-task
	 *
	 */
	rt_sem_init(&tdma->free, TDMA_MAX_TX_QUEUE);
	rt_sem_init(&tdma->full, 0);
	rt_sem_init(&tdma->client_tx, 0);
 
	/*
	 * init tx queue
	 *
	 */
	rtskb_queue_head_init(&tdma->tx_queue);

	/*
	 * init rt stuff
	 * - timer
	 * - list heads
	 *
	 */
	/* generic */

	/* master */
	init_timer(&tdma->rt_add_timer);
	INIT_LIST_HEAD(&tdma->rt_add_list);
	INIT_LIST_HEAD(&tdma->rt_list);
	INIT_LIST_HEAD(&tdma->rt_list_rate);

	init_timer(&tdma->task_change_timer);
	init_timer(&tdma->master_wait_timer);
	init_timer(&tdma->master_sent_conf_timer);
	init_timer(&tdma->master_sent_test_timer);

	rtskb_queue_head_init(&tdma->master_queue);


	/* client */
	init_timer(&tdma->client_sent_ack_timer);


	/*
	 * init nrt stuff
	 */
	//INIT_LIST_HEAD(&tdma->nrt_list);

	/*
	 * start timer
	 */
	rt_set_oneshot_mode();
	start_rt_timer(0);

	return 0;
}



int tdma_release(struct rtnet_device *rtdev)
{
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtmac->priv;

	rt_printk("RTmac: tdma: release\n");
	
	/*
	 * delete rt specific stuff
	 * - lists
	 *   * rt_add_list
	 *   * rt_list
	 *
	 * FIXME: all these thingies _should_ be clean...test them
	 */
	tdma_cleanup_master_rt_check(tdma);

	/*
	 * delete timers
	 */
	del_timer(&tdma->task_change_timer);
	del_timer(&tdma->rt_add_timer);
	del_timer(&tdma->master_wait_timer);
	del_timer(&tdma->master_sent_conf_timer);
	del_timer(&tdma->master_sent_test_timer);

	/*
	 * delete tx tasks sema
	 */
	rt_sem_delete(&tdma->free);
	rt_sem_delete(&tdma->full);
	rt_sem_delete(&tdma->client_tx);

	/*
	 * purge allocated space for tdma
	 */
	kfree(tdma);
	tdma = NULL;

	return 0;
}








int tdma_packet_tx(struct rtskb *skb, struct rtnet_device *rtdev)
{
	struct rtmac_device *rtmac = rtdev->rtmac;
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtmac->priv;

	int ret = 0;

	if (tdma->flags.mac_active == 0) {
		ret = rtdev_xmit(skb);
	} else {
		rt_sem_wait(&tdma->free);
		rtskb_queue_tail(&tdma->tx_queue, skb);
		rt_sem_signal(&tdma->full);
	}

	return ret;
}



int tdma_start(struct rtnet_device *rtdev)
{
	int ret;
    
	static struct rtmac_disc_ops tdma_disc_ops = {
		init:			&tdma_init,
		release:		&tdma_release,
	};

	static struct rtmac_ioctl_ops tdma_ioctl_ops = {
		client:			&tdma_ioctl_client,
		master:			&tdma_ioctl_master,
		up:			&tdma_ioctl_up,
		down:			&tdma_ioctl_down,
		add:			&tdma_ioctl_add,
		remove:			&tdma_ioctl_remove,
		cycle:			&tdma_ioctl_cycle,
		mtu:			&tdma_ioctl_mtu,
		offset:			&tdma_ioctl_offset,
	};

	static struct rtmac_disc_type disc_type = {
		packet_rx:		&tdma_packet_rx,
		rt_packet_tx:		&tdma_packet_tx,
		proxy_packet_tx:	&tdma_packet_tx,
		disc_ops:		&tdma_disc_ops,
		ioctl_ops:		&tdma_ioctl_ops,
	};

	if( !rtdev ) {
		rt_printk("RTmac: rtmac_tdma_start(struct rtnet_device *rtdev) called with rtdev=NULL\n");
		return -1; //FIXME: better code
	}

	ret = rtmac_disc_init(rtdev, &disc_type);

	return ret;
}



void tdma_stop(struct rtnet_device *rtdev)
{
	rtmac_disc_release(rtdev);
}
