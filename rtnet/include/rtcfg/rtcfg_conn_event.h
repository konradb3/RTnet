/***
 *
 *  include/rtcfg/rtcfg_conn_event.h
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003 Jan Kiszka <jan.kiszka@web.de>
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

#ifndef __RTCFG_CONN_EVENT_H_
#define __RTCFG_CONN_EVENT_H_

#include <rtcfg/rtcfg_event.h>


int rtcfg_do_conn_event(struct rtcfg_connection *conn, RTCFG_EVENT event_id,
                        void* event_data);

#endif /* __RTCFG_CONN_EVENT_H_ */
