/***
 *
 *  rtcfg/rtcfg_conn_event.c
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2003, 2004 Jan Kiszka <jan.kiszka@web.de>
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

#include <rtcfg/rtcfg.h>
#include <rtcfg/rtcfg_conn_event.h>
#include <rtcfg/rtcfg_event.h>
#include <rtcfg/rtcfg_frame.h>


/****************************** states ***************************************/
static int rtcfg_conn_state_searching(
    struct rtcfg_connection *conn, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_conn_state_stage_1(
    struct rtcfg_connection *conn, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_conn_state_stage_2(
    struct rtcfg_connection *conn, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_conn_state_ready(
    struct rtcfg_connection *conn, RTCFG_EVENT event_id, void* event_data);


#ifdef CONFIG_RTNET_RTCFG_DEBUG
const char *rtcfg_conn_state[] = {
    "RTCFG_CONN_SEARCHING",
    "RTCFG_CONN_STAGE_1",
    "RTCFG_CONN_STAGE_2",
    "RTCFG_CONN_READY"
};

extern char *rtcfg_event[];
#endif /* CONFIG_RTNET_RTCFG_DEBUG */


static void rtcfg_conn_client_configured(struct rtcfg_connection *conn);
static void rtcfg_conn_check_cfg_timeout(struct rtcfg_connection *conn);



static int (*state[])(struct rtcfg_connection *conn, RTCFG_EVENT event_id,
                      void* event_data) =
{
    rtcfg_conn_state_searching,
    rtcfg_conn_state_stage_1,
    rtcfg_conn_state_stage_2,
    rtcfg_conn_state_ready
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
    struct rtcfg_device       *rtcfg_dev = &device[conn->ifindex];
    struct rtskb              *rtskb = (struct rtskb *)event_data;
    struct rtcfg_frm_announce *announce_new;
    int                       packets;


    switch (event_id) {
        case RTCFG_FRM_ANNOUNCE_NEW:
            memcpy(&conn->last_frame, &rtskb->time_stamp, sizeof(rtos_time_t));

            announce_new = (struct rtcfg_frm_announce *)rtskb->data;

            conn->flags = announce_new->flags;
            if (announce_new->burstrate < conn->burstrate)
                conn->burstrate = announce_new->burstrate;

            rtcfg_next_conn_state(conn, RTCFG_CONN_STAGE_1);

            rtcfg_dev->stations_found++;
            if ((conn->flags & RTCFG_FLAG_READY) != 0)
                rtcfg_dev->stations_ready++;

            if (((conn->flags & RTCFG_FLAG_STAGE_2_DATA) != 0) &&
                (conn->stage2_file != NULL)) {
                packets = conn->burstrate - 1;

                rtcfg_send_stage_2(conn, 1);

                while ((conn->cfg_offs < conn->stage2_file->size) &&
                       (packets > 0)) {
                    rtcfg_send_stage_2_frag(conn);
                    packets--;
                }
            } else {
                rtcfg_send_stage_2(conn, 0);
                conn->flags &= ~RTCFG_FLAG_STAGE_2_DATA;
            }

            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for conn %p in %s()\n",
                        rtcfg_event[event_id], conn, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_conn_state_stage_1(struct rtcfg_connection *conn,
                                    RTCFG_EVENT event_id, void* event_data)
{
    struct rtskb             *rtskb = (struct rtskb *)event_data;
    struct rtcfg_frm_ack_cfg *ack_cfg;
    int                      packets;


    switch (event_id) {
        case RTCFG_FRM_ACK_CFG:
            memcpy(&conn->last_frame, &rtskb->time_stamp, sizeof(rtos_time_t));

            ack_cfg = (struct rtcfg_frm_ack_cfg *)rtskb->data;
            conn->cfg_offs = ntohl(ack_cfg->ack_len);

            if ((conn->flags & RTCFG_FLAG_STAGE_2_DATA) != 0) {
                if (conn->cfg_offs >= conn->stage2_file->size) {
                    rtcfg_conn_client_configured(conn);
                    rtcfg_next_conn_state(conn,
                        ((conn->flags & RTCFG_FLAG_READY) != 0) ?
                        RTCFG_CONN_READY : RTCFG_CONN_STAGE_2);
                } else {
                    packets = conn->burstrate;
                    while ((conn->cfg_offs < conn->stage2_file->size) &&
                        (packets > 0)) {
                        rtcfg_send_stage_2_frag(conn);
                        packets--;
                    }
                }
            } else {
                rtcfg_conn_client_configured(conn);
                rtcfg_next_conn_state(conn,
                    ((conn->flags & RTCFG_FLAG_READY) != 0) ?
                    RTCFG_CONN_READY : RTCFG_CONN_STAGE_2);
            }

            break;

        case RTCFG_TIMER:
            rtcfg_conn_check_cfg_timeout(conn);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for conn %p in %s()\n",
                        rtcfg_event[event_id], conn, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_conn_state_stage_2(struct rtcfg_connection *conn,
                                    RTCFG_EVENT event_id, void* event_data)
{
    struct rtskb        *rtskb = (struct rtskb *)event_data;
    struct rtcfg_device *rtcfg_dev = &device[conn->ifindex];
    struct rt_proc_call *call;
    struct rtcfg_cmd    *cmd_event;


    switch (event_id) {
        case RTCFG_FRM_READY:
            memcpy(&conn->last_frame, &rtskb->time_stamp, sizeof(rtos_time_t));

            rtcfg_next_conn_state(conn, RTCFG_CONN_READY);

            conn->flags |= RTCFG_FLAG_READY;
            rtcfg_dev->stations_ready++;

            if (rtcfg_dev->stations_ready == rtcfg_dev->other_stations)
                while (1) {
                    call = rtcfg_dequeue_blocking_call(conn->ifindex);
                    if (call == NULL)
                        break;

                    cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

                    rtpc_complete_call(call,
                        (cmd_event->event_id == RTCFG_CMD_READY) ?
                            0 : -EINVAL);
                }

            break;

        case RTCFG_TIMER:
            rtcfg_conn_check_cfg_timeout(conn);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for conn %p in %s()\n",
                        rtcfg_event[event_id], conn, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_conn_state_ready(struct rtcfg_connection *conn,
                                  RTCFG_EVENT event_id, void* event_data)
{
    switch (event_id) {
        case RTCFG_TIMER:
            /* TODO */
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for conn %p in %s()\n",
                        rtcfg_event[event_id], conn, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static void rtcfg_conn_client_configured(struct rtcfg_connection *conn)
{
    struct rtcfg_device *rtcfg_dev = &device[conn->ifindex];
    struct rt_proc_call *call;
    struct rtcfg_cmd    *cmd_event;


    rtcfg_dev->clients_configured++;
    if (rtcfg_dev->clients_configured == rtcfg_dev->other_stations)
        while (1) {
            call = rtcfg_dequeue_blocking_call(conn->ifindex);
            if (call == NULL)
                break;

            cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

            rtpc_complete_call(call,
                (cmd_event->event_id == RTCFG_CMD_WAIT) ?
                    0 : -EINVAL);
        }
}



static void rtcfg_conn_check_cfg_timeout(struct rtcfg_connection *conn)
{
    rtos_time_t         now;
    rtos_time_t         deadline;
    struct rtnet_device *rtdev;


    if (RTOS_TIME_IS_ZERO(&conn->cfg_timeout))
        return;

    rtos_get_time(&now);
    rtos_time_sum(&deadline, &conn->last_frame, &conn->cfg_timeout);

    if (!RTOS_TIME_IS_BEFORE(&now, &deadline)) {
        rtcfg_next_conn_state(conn, RTCFG_CONN_SEARCHING);
        conn->cfg_offs = 0;
        conn->flags    = 0;

        if (conn->addr_type == RTCFG_ADDR_IP) {
            /* MAC address yet unknown -> use broadcast address */
            rtdev = rtdev_get_by_index(conn->ifindex);
            if (rtdev == NULL)
                return;
            memcpy(conn->mac_addr, rtdev->broadcast, MAX_ADDR_LEN);
            rtdev_dereference(rtdev);
        }
    }
}
