/* tdma_event.h
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

#ifndef __TDMA_TIMER_H_
#define __TDMA_TIMER_H_
#ifdef __KERNEL__

struct timer_task_change_data {
	struct rtmac_tdma	*tdma;
	void (*task)		(int rtdev_id);
	unsigned int		cycle;
};

void tdma_timer_start(struct timer_list *ptimer, int timeout, void *data, TIMER_CALLBACK callback);

static void tdma_timer_expired_rt_add(void *data);
static void tdma_timer_expired_master_wait(void *data);
static void tdma_timer_expired_sent_conf(void *data);
static void tdma_timer_expired_task_change(void *data);
static void tdma_timer_expired_sent_test(void *data);
static void tdma_timer_expired_sent_ack(void *data);

#endif //__KERNEL__
#endif //__TDMA_EVENT_H_
