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

#ifndef __TDMA_EVENT_H_
#define __TDMA_EVENT_H_

#ifdef __KERNEL__

/* tdma states */
typedef enum {
	TDMA_DOWN,

	TDMA_MASTER_DOWN,
	TDMA_MASTER_WAIT,
	TDMA_MASTER_SENT_CONF,
	TDMA_MASTER_SENT_TEST,

	TDMA_OTHER_MASTER,
	TDMA_CLIENT_DOWN,
	TDMA_CLIENT_ACK_CONF,
	TDMA_CLIENT_RCVD_ACK,
} TDMA_STATE;


/* tdma events */
typedef enum {
	REQUEST_MASTER,
	REQUEST_CLIENT,

	REQUEST_UP,
	REQUEST_DOWN,

	REQUEST_ADD_RT,
	REQUEST_REMOVE_RT,

	REQUEST_ADD_NRT,
	REQUEST_REMOVE_NRT,

	CHANGE_MTU,
	CHANGE_CYCLE,
	CHANGE_OFFSET,

	EXPIRED_ADD_RT,
	EXPIRED_MASTER_WAIT,
	EXPIRED_MASTER_SENT_CONF,
	EXPIRED_MASTER_SENT_TEST,
	EXPIRED_CLIENT_SENT_ACK,

	NOTIFY_MASTER,
	REQUEST_TEST,
	ACK_TEST,
	
	REQUEST_CONF,
	ACK_CONF,
	ACK_ACK_CONF,

	STATION_LIST,
	REQUEST_CHANGE_OFFSET,

	START_OF_FRAME,
} TDMA_EVENT;



typedef enum {
	RT_DOWN,
	RT_SENT_CONF,
	RT_RCVD_CONF,
	RT_SENT_TEST,
	RT_RCVD_TEST,
	RT_COMP_TEST,
	RT_CLIENT,
} TDMA_RT_STATE;




#endif //__KERNEL__

#endif //__TDMA_EVENT_H_
