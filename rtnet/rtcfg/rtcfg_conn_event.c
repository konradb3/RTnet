/***
 *
 *  rtcfg/rtcfg_conn_event.c
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

#include <linux/kernel.h>

#include <rtai.h>
#include <rtai_sched.h>

#include <rtcfg/rtcfg.h>
#include <rtcfg/rtcfg_event.h>
#include <rtcfg/rtcfg_frame.h>


/****************************** states ***************************************/
static int rtcfg_conn_state_searching(
    struct rtcfg_connection *conn, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_conn_state_conn_1(
    struct rtcfg_connection *conn, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_conn_state_established(
    struct rtcfg_connection *conn, RTCFG_EVENT event_id, void* event_data);


#ifdef CONFIG_RTCFG_DEBUG

const char *rtcfg_conn_state[] = {
    "RTCFG_CONN_SEARCHING",
    "RTCFG_CONN_1",
    "RTCFG_CONN_ESTABLISHED"
};

extern char *rtcfg_event[];

#endif /* CONFIG_RTCFG_DEBUG */


static int (*state[])(struct rtcfg_connection *conn, RTCFG_EVENT event_id,
                      void* event_data) =
{
    rtcfg_conn_state_searching,
    rtcfg_conn_state_conn_1,
    rtcfg_conn_state_established
};



int rtcfg_do_conn_event(struct rtcfg_connection *conn, RTCFG_EVENT event_id,
                        void* event_data)
{
    int conn_state = conn->state;


    RTCFG_DEBUG(3, "RTcfg: %s() conn=%p, event=%s, state=%s\n", __FUNCTION__,
                conn, rtcfg_event[event_id], rtcfg_conn_state[conn_state]);

    return (*state[conn_state])(conn, event_id, event_data);
}



static void rtcfg_next_conn_state(struct rtcfg_connection *conn,
                                  RTCFG_CONN_STATE state)
{
    RTCFG_DEBUG(4, "RTcfg: next connection state=%s \n",
                rtcfg_conn_state[state]);

    conn->state = state;
}



static int rtcfg_conn_state_searching(struct rtcfg_connection *conn,
                                      RTCFG_EVENT event_id, void* event_data)
{
    switch (event_id) {
        case RTCFG_FRM_ANNOUNCE_NEW:
            rtcfg_next_conn_state(conn, RTCFG_CONN_1);

            rtcfg_send_stage_2(conn);

            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for conn %p in %s()\n",
                        rtcfg_event[event_id], conn, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_conn_state_conn_1(struct rtcfg_connection *conn,
                                   RTCFG_EVENT event_id, void* event_data)
{
    switch (event_id) {
        case RTCFG_FRM_ACK_CFG:
            /* ... */

            rtcfg_next_conn_state(conn, RTCFG_CONN_ESTABLISHED);
            device[conn->ifindex].clients_found++;

            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for conn %p in %s()\n",
                        rtcfg_event[event_id], conn, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_conn_state_established(struct rtcfg_connection *conn,
                                        RTCFG_EVENT event_id, void* event_data)
{
    switch (event_id) {

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for conn %p in %s()\n",
                        rtcfg_event[event_id], conn, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}
