/***
 *
 *  rtcfg/rtcfg_event.c
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

#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/vmalloc.h>

#include <rtdev.h>
#include <ipv4/route.h>
#include <rtcfg/rtcfg.h>
#include <rtcfg/rtcfg_conn_event.h>
#include <rtcfg/rtcfg_file.h>
#include <rtcfg/rtcfg_frame.h>
#include <rtcfg/rtcfg_timer.h>


/****************************** states ***************************************/
static int rtcfg_main_state_off(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_server_running(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_client_0(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_client_1(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_client_announced(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_client_all_known(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_client_all_frames(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_client_2(
    int ifindex, RTCFG_EVENT event_id, void* event_data);
static int rtcfg_main_state_client_ready(
    int ifindex, RTCFG_EVENT event_id, void* event_data);


#ifdef CONFIG_RTNET_RTCFG_DEBUG
const char *rtcfg_event[] = {
    "RTCFG_CMD_SERVER",
    "RTCFG_CMD_ADD_IP",
    "RTCFG_CMD_ADD_MAC",
    "RTCFG_CMD_DEL_IP",
    "RTCFG_CMD_DEL_MAC",
    "RTCFG_CMD_WAIT",
    "RTCFG_CMD_CLIENT",
    "RTCFG_CMD_ANNOUNCE",
    "RTCFG_CMD_READY",
    "RTCFG_CMD_DOWN",
    "RTCFG_FRM_STAGE_1_CFG",
    "RTCFG_FRM_ANNOUNCE_NEW",
    "RTCFG_FRM_ANNOUNCE_REPLY",
    "RTCFG_FRM_STAGE_2_CFG",
    "RTCFG_FRM_STAGE_2_CFG_FRAG",
    "RTCFG_FRM_ACK_CFG",
    "RTCFG_FRM_READY",
    "RTCFG_FRM_HEARTBEAT"
};

static const char *rtcfg_main_state[] = {
    "RTCFG_MAIN_OFF",
    "RTCFG_MAIN_SERVER_RUNNING",
    "RTCFG_MAIN_CLIENT_0",
    "RTCFG_MAIN_CLIENT_1",
    "RTCFG_MAIN_CLIENT_ANNOUNCED",
    "RTCFG_MAIN_CLIENT_ALL_KNOWN",
    "RTCFG_MAIN_CLIENT_ALL_FRAMES",
    "RTCFG_MAIN_CLIENT_2",
    "RTCFG_MAIN_CLIENT_READY"
};

static int rtcfg_debug = RTCFG_DEFAULT_DEBUG_LEVEL;
#endif /* CONFIG_RTNET_RTCFG_DEBUG */


struct rtcfg_device device[MAX_RT_DEVICES];

static int (*state[])(int ifindex, RTCFG_EVENT event_id, void* event_data) =
{
    rtcfg_main_state_off,
    rtcfg_main_state_server_running,
    rtcfg_main_state_client_0,
    rtcfg_main_state_client_1,
    rtcfg_main_state_client_announced,
    rtcfg_main_state_client_all_known,
    rtcfg_main_state_client_all_frames,
    rtcfg_main_state_client_2,
    rtcfg_main_state_client_ready
};


static int rtcfg_server_add(struct rtcfg_cmd *cmd_event,
                            unsigned int addr_type);
static int rtcfg_server_recv_announce(int ifindex, struct rtskb *rtskb);
static int rtcfg_server_recv_ack(int ifindex, struct rtskb *rtskb);
static int rtcfg_server_recv_ready(int ifindex, struct rtskb *rtskb);

static int rtcfg_client_get_frag(int ifindex, struct rt_proc_call *call);
static void rtcfg_client_recv_stage_1(int ifindex, struct rtskb *rtskb);
static int rtcfg_client_recv_announce(int ifindex, struct rtskb *rtskb);
static void rtcfg_client_recv_stage_2_cfg(int ifindex, struct rtskb *rtskb);
static void rtcfg_client_recv_stage_2_frag(int ifindex, struct rtskb *rtskb);
static int rtcfg_client_recv_ready(int ifindex, struct rtskb *rtskb);

static void rtcfg_complete_cmd(int ifindex, RTCFG_EVENT event_id, int result);



static void rtcfg_queue_blocking_call(int ifindex, struct rt_proc_call *call)
{
    unsigned long       flags;
    struct rtcfg_device *rtcfg_dev = &device[ifindex];


    rtos_spin_lock_irqsave(&rtcfg_dev->event_calls_lock, flags);
    list_add_tail(&call->list_entry, &rtcfg_dev->event_calls);
    rtos_spin_unlock_irqrestore(&rtcfg_dev->event_calls_lock, flags);
}



struct rt_proc_call *rtcfg_dequeue_blocking_call(int ifindex)
{
    unsigned long flags;
    struct rt_proc_call *call;
    struct rtcfg_device *rtcfg_dev = &device[ifindex];


    rtos_spin_lock_irqsave(&rtcfg_dev->event_calls_lock, flags);
    if (!list_empty(&rtcfg_dev->event_calls)) {
        call = (struct rt_proc_call *)rtcfg_dev->event_calls.next;
        list_del(&call->list_entry);
    } else
        call = NULL;
    rtos_spin_unlock_irqrestore(&rtcfg_dev->event_calls_lock, flags);

    return call;
}



int rtcfg_do_main_event(int ifindex, RTCFG_EVENT event_id, void* event_data)
{
    int main_state = device[ifindex].state;


    RTCFG_DEBUG(3, "RTcfg: %s() rtdev=%d, event=%s, state=%s\n", __FUNCTION__,
                ifindex, rtcfg_event[event_id], rtcfg_main_state[main_state]);

    return (*state[main_state])(ifindex, event_id, event_data);
}



static void rtcfg_next_main_state(int ifindex, RTCFG_MAIN_STATE state)
{
    RTCFG_DEBUG(4, "RTcfg: next main state=%s \n", rtcfg_main_state[state]);

    device[ifindex].state = state;
}



static int rtcfg_main_state_off(int ifindex, RTCFG_EVENT event_id,
                                void* event_data)
{
    struct rtcfg_device     *rtcfg_dev = &device[ifindex];
    struct rt_proc_call     *call      = (struct rt_proc_call *)event_data;
    struct rtcfg_cmd        *cmd_event;
    rtos_time_t             period;
    int                     ret;

    cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);
    switch (event_id) {
        case RTCFG_CMD_SERVER:
            rtos_nanosecs_to_time(
                ((nanosecs_t)cmd_event->args.server.period)*1000000, &period);

            rtos_res_lock(&rtcfg_dev->dev_lock);

            ret = rtos_task_init_periodic(&rtcfg_dev->timer_task, rtcfg_timer,
                                          ifindex, RTOS_LOWEST_RT_PRIORITY,
                                          &period);
            if (ret < 0) {
                rtos_res_unlock(&rtcfg_dev->dev_lock);
                return ret;
            }

            rtcfg_dev->flags = cmd_event->args.server.flags & RTCFG_FLAG_READY;
            rtcfg_dev->burstrate = cmd_event->args.server.burstrate;

            rtcfg_next_main_state(ifindex, RTCFG_MAIN_SERVER_RUNNING);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            break;

        case RTCFG_CMD_CLIENT:
            rtos_res_lock(&rtcfg_dev->dev_lock);

            rtcfg_dev->station_addr_list = cmd_event->args.client.station_buf;
            cmd_event->args.client.station_buf = NULL;

            rtcfg_dev->max_stations   = cmd_event->args.client.max_stations;
            rtcfg_dev->other_stations = -1;

            rtcfg_queue_blocking_call(ifindex, call);

            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_0);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



/*** Server States ***/

static int rtcfg_main_state_server_running(int ifindex, RTCFG_EVENT event_id,
                                           void* event_data)
{
    struct rt_proc_call *call;
    struct rtcfg_cmd    *cmd_event;
    struct rtcfg_device *rtcfg_dev;
    struct rtskb        *rtskb;
    struct rtnet_device *rtdev;


    switch (event_id) {
        case RTCFG_CMD_ADD_IP:
            call      = (struct rt_proc_call *)event_data;
            cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

            /* MAC address yet unknown -> use broadcast address */
            rtdev = rtdev_get_by_index(cmd_event->ifindex);
            if (rtdev == NULL)
                return -ENODEV;
            memcpy(cmd_event->args.add.mac_addr, rtdev->broadcast,
                   MAX_ADDR_LEN);
            rtdev_dereference(rtdev);

            return rtcfg_server_add(cmd_event, RTCFG_ADDR_IP);

        case RTCFG_CMD_ADD_IP_MAC:
            call      = (struct rt_proc_call *)event_data;
            cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

            return rtcfg_server_add(cmd_event, RTCFG_ADDR_IP);

        case RTCFG_CMD_ADD_MAC:
            call      = (struct rt_proc_call *)event_data;
            cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

            return rtcfg_server_add(cmd_event, RTCFG_ADDR_MAC);

        case RTCFG_CMD_WAIT:
            call = (struct rt_proc_call *)event_data;

            rtcfg_dev = &device[ifindex];
            rtos_res_lock(&rtcfg_dev->dev_lock);

            if (rtcfg_dev->clients_configured == rtcfg_dev->other_stations)
                rtpc_complete_call(call, 0);
            else
                rtcfg_queue_blocking_call(ifindex, call);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        case RTCFG_CMD_READY:
            call = (struct rt_proc_call *)event_data;

            rtcfg_dev = &device[ifindex];
            rtos_res_lock(&rtcfg_dev->dev_lock);

            if (rtcfg_dev->stations_ready == rtcfg_dev->other_stations)
                rtpc_complete_call(call, 0);
            else
                rtcfg_queue_blocking_call(ifindex, call);

            if ((rtcfg_dev->flags & RTCFG_FLAG_READY) == 0) {
                rtcfg_dev->flags |= RTCFG_FLAG_READY;
                rtcfg_send_ready(ifindex);
            }

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        case RTCFG_FRM_ANNOUNCE_NEW:
            rtskb = (struct rtskb *)event_data;
            return rtcfg_server_recv_announce(ifindex, rtskb);

        case RTCFG_FRM_ACK_CFG:
            rtskb = (struct rtskb *)event_data;
            return rtcfg_server_recv_ack(ifindex, rtskb);

        case RTCFG_FRM_READY:
            rtskb = (struct rtskb *)event_data;
            return rtcfg_server_recv_ready(ifindex, rtskb);

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



/*** Client States ***/

static int rtcfg_main_state_client_0(int ifindex, RTCFG_EVENT event_id,
                                     void* event_data)
{
    struct rtskb *rtskb = (struct rtskb *)event_data;


    switch (event_id) {
        case RTCFG_FRM_STAGE_1_CFG:
            rtcfg_client_recv_stage_1(ifindex, rtskb);
            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            if (rtcfg_client_recv_announce(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);
            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_READY:
            if (rtcfg_client_recv_ready(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_main_state_client_1(int ifindex, RTCFG_EVENT event_id,
                                     void* event_data)
{
    struct rtcfg_device *rtcfg_dev = &device[ifindex];
    struct rtskb        *rtskb;
    struct rt_proc_call *call;
    struct rtcfg_cmd    *cmd_event;
    int                 ret;


    switch (event_id) {
        case RTCFG_CMD_CLIENT:
            /* second trial (buffer was probably too small) */
            rtos_res_lock(&rtcfg_dev->dev_lock);

            rtcfg_queue_blocking_call(ifindex,
                (struct rt_proc_call *)event_data);

            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_0);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        case RTCFG_CMD_ANNOUNCE:
            call      = (struct rt_proc_call *)event_data;
            cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

            if (cmd_event->args.announce.burstrate == 0)
                return -EINVAL;

            rtos_res_lock(&rtcfg_dev->dev_lock);

            rtcfg_queue_blocking_call(ifindex,
                (struct rt_proc_call *)event_data);

            rtcfg_dev->flags = cmd_event->args.announce.flags &
                (RTCFG_FLAG_STAGE_2_DATA | RTCFG_FLAG_READY);
            if (cmd_event->args.announce.burstrate < rtcfg_dev->burstrate)
                rtcfg_dev->burstrate = cmd_event->args.announce.burstrate;

            ret = rtcfg_send_announce_new(ifindex);
            if (ret < 0) {
                rtos_res_unlock(&rtcfg_dev->dev_lock);
                return ret;
            }

            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ANNOUNCED);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        case RTCFG_FRM_ANNOUNCE_NEW:
            rtskb = (struct rtskb *)event_data;

            if (rtcfg_client_recv_announce(ifindex, rtskb) == 0) {
                rtcfg_send_announce_reply(ifindex,
                                          rtskb->mac.ethernet->h_source);
                rtos_res_unlock(&device[ifindex].dev_lock);
            }

            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_ANNOUNCE_REPLY:
            rtskb = (struct rtskb *)event_data;

            if (rtcfg_client_recv_announce(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);

            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_READY:
            rtskb = (struct rtskb *)event_data;
            if (rtcfg_client_recv_ready(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);
            break;

        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            kfree_rtskb((struct rtskb *)event_data);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_main_state_client_announced(int ifindex, RTCFG_EVENT event_id,
                                             void* event_data)
{
    struct rtskb        *rtskb = (struct rtskb *)event_data;
    struct rtcfg_device *rtcfg_dev;
    struct rt_proc_call *call;


    switch (event_id) {
        case RTCFG_CMD_ANNOUNCE:
            call = (struct rt_proc_call *)event_data;
            return rtcfg_client_get_frag(ifindex, call);

        case RTCFG_FRM_STAGE_2_CFG:
            rtcfg_client_recv_stage_2_cfg(ifindex, rtskb);
            break;

        case RTCFG_FRM_STAGE_2_CFG_FRAG:
            rtcfg_client_recv_stage_2_frag(ifindex, rtskb);
            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            if (rtcfg_client_recv_announce(ifindex, rtskb) == 0) {
                rtcfg_send_announce_reply(ifindex,
                                          rtskb->mac.ethernet->h_source);

                rtcfg_dev = &device[ifindex];
                if (rtcfg_dev->stations_found == rtcfg_dev->other_stations)
                    rtcfg_next_main_state(ifindex,
                        RTCFG_MAIN_CLIENT_ALL_KNOWN);

                rtos_res_unlock(&rtcfg_dev->dev_lock);
            }
            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_ANNOUNCE_REPLY:
            if (rtcfg_client_recv_announce(ifindex, rtskb) == 0) {
                rtcfg_dev = &device[ifindex];
                if (rtcfg_dev->stations_found == rtcfg_dev->other_stations)
                    rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ALL_KNOWN);

                rtos_res_unlock(&rtcfg_dev->dev_lock);
            }
            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_READY:
            if (rtcfg_client_recv_ready(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);
            break;

        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            kfree_rtskb(rtskb);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }

    return 0;
}



static int rtcfg_main_state_client_all_known(int ifindex, RTCFG_EVENT event_id,
                                             void* event_data)
{
    struct rtskb *rtskb = (struct rtskb *)event_data;
    struct rt_proc_call *call;


    switch (event_id) {
        case RTCFG_CMD_ANNOUNCE:
            call = (struct rt_proc_call *)event_data;
            return rtcfg_client_get_frag(ifindex, call);

        case RTCFG_FRM_STAGE_2_CFG_FRAG:
            rtcfg_client_recv_stage_2_frag(ifindex, rtskb);
            break;

        case RTCFG_FRM_READY:
            if (rtcfg_client_recv_ready(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);
            break;

        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            /* TODO: update tables with new address */

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_main_state_client_all_frames(int ifindex,
                                              RTCFG_EVENT event_id,
                                              void* event_data)
{
    struct rtskb        *rtskb = (struct rtskb *)event_data;
    struct rtcfg_device *rtcfg_dev;


    switch (event_id) {
        case RTCFG_FRM_ANNOUNCE_NEW:
            if (rtcfg_client_recv_announce(ifindex, rtskb) == 0) {
                rtcfg_send_announce_reply(ifindex,
                                          rtskb->mac.ethernet->h_source);

                rtcfg_dev = &device[ifindex];
                if (rtcfg_dev->stations_found == rtcfg_dev->other_stations) {
                    rtcfg_complete_cmd(ifindex, RTCFG_CMD_ANNOUNCE, 0);

                    rtcfg_next_main_state(ifindex,
                        ((rtcfg_dev->flags & RTCFG_FLAG_READY) != 0) ?
                        RTCFG_MAIN_CLIENT_READY : RTCFG_MAIN_CLIENT_2);
                }

                rtos_res_unlock(&rtcfg_dev->dev_lock);
            }
            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_ANNOUNCE_REPLY:
            if (rtcfg_client_recv_announce(ifindex, rtskb) == 0) {
                rtcfg_dev = &device[ifindex];
                if (rtcfg_dev->stations_found == rtcfg_dev->other_stations) {
                    rtcfg_complete_cmd(ifindex, RTCFG_CMD_ANNOUNCE, 0);

                    rtcfg_next_main_state(ifindex,
                        ((rtcfg_dev->flags & RTCFG_FLAG_READY) != 0) ?
                        RTCFG_MAIN_CLIENT_READY : RTCFG_MAIN_CLIENT_2);
                }

                rtos_res_unlock(&rtcfg_dev->dev_lock);
            }
            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_READY:
            if (rtcfg_client_recv_ready(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);
            break;

        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            kfree_rtskb(rtskb);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}


static int rtcfg_main_state_client_2(int ifindex, RTCFG_EVENT event_id,
                                     void* event_data)
{
    struct rtcfg_device *rtcfg_dev;
    struct rt_proc_call *call;
    struct rtskb        *rtskb;


    switch (event_id) {
        case RTCFG_CMD_READY:
            call = (struct rt_proc_call *)event_data;

            rtcfg_dev = &device[ifindex];
            rtos_res_lock(&rtcfg_dev->dev_lock);

            if (rtcfg_dev->stations_ready == rtcfg_dev->other_stations)
                rtpc_complete_call(call, 0);
            else
                rtcfg_queue_blocking_call(ifindex, call);

            if ((rtcfg_dev->flags & RTCFG_FLAG_READY) == 0) {
                rtcfg_dev->flags |= RTCFG_FLAG_READY;
                rtcfg_send_ready(ifindex);
            }
            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_READY);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        case RTCFG_FRM_READY:
            rtskb = (struct rtskb *)event_data;
            if (rtcfg_client_recv_ready(ifindex, rtskb) == 0)
                rtos_res_unlock(&device[ifindex].dev_lock);
            break;

        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            kfree_rtskb((struct rtskb *)event_data);
            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            /* TODO: update tables with new address */

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_main_state_client_ready(int ifindex, RTCFG_EVENT event_id,
                                         void* event_data)
{
    struct rtskb        *rtskb = (struct rtskb *)event_data;
    struct rtcfg_device *rtcfg_dev;


    switch (event_id) {
        case RTCFG_FRM_READY:
            if (rtcfg_client_recv_ready(ifindex, rtskb) == 0) {
                rtcfg_dev = &device[ifindex];
                if (rtcfg_dev->stations_ready == rtcfg_dev->other_stations)
                    rtcfg_complete_cmd(ifindex, RTCFG_CMD_READY, 0);

                rtos_res_unlock(&rtcfg_dev->dev_lock);
            }
            break;

        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            kfree_rtskb(rtskb);
            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            /* TODO: update tables with new address */

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



/*** Server Command Event Handlers ***/

static int rtcfg_server_add(struct rtcfg_cmd *cmd_event,
                            unsigned int addr_type)
{
    struct rtcfg_connection *conn;
    struct rtcfg_connection *new_conn;
    struct list_head        *entry;
    struct rtcfg_device     *rtcfg_dev = &device[cmd_event->ifindex];


    new_conn = cmd_event->args.add.conn_buf;
    memset(new_conn, 0, sizeof(struct rtcfg_connection));

    new_conn->ifindex      = cmd_event->ifindex;
    new_conn->state        = RTCFG_CONN_SEARCHING;
    new_conn->addr_type    = addr_type;
    new_conn->addr.ip_addr = cmd_event->args.add.ip_addr;
    new_conn->stage1_data  = cmd_event->args.add.stage1_data;
    new_conn->stage1_size  = cmd_event->args.add.stage1_size;
    new_conn->burstrate    = rtcfg_dev->burstrate;

    memcpy(new_conn->mac_addr, cmd_event->args.add.mac_addr, MAX_ADDR_LEN);

    /* get stage 2 file */
    if (cmd_event->args.add.stage2_file != NULL) {
        if (cmd_event->args.add.stage2_file->buffer != NULL) {
            new_conn->stage2_file = cmd_event->args.add.stage2_file;
            rtcfg_add_file(new_conn->stage2_file);

            cmd_event->args.add.stage2_file = NULL;
        } else {
            new_conn->stage2_file =
                rtcfg_get_file(cmd_event->args.add.stage2_file->name);
            if (new_conn->stage2_file == NULL)
                return 1;
        }
    }

    rtos_res_lock(&rtcfg_dev->dev_lock);

    list_for_each(entry, &rtcfg_dev->conn_list) {
        conn = list_entry(entry, struct rtcfg_connection, entry);

        if (((addr_type == RTCFG_ADDR_IP) &&
             (conn->addr.ip_addr == cmd_event->args.add.ip_addr)) ||
            ((addr_type == RTCFG_ADDR_MAC) &&
             (memcmp(conn->mac_addr, cmd_event->args.add.mac_addr,
                     MAX_ADDR_LEN) == 0))) {
            rtos_res_unlock(&rtcfg_dev->dev_lock);

            if (rtcfg_release_file(new_conn->stage2_file) == 0) {
                /* Note: This assignment cannot overwrite a valid file pointer.
                 * Effectively, it will only be executed when
                 * new_conn->stage2_file is the pointer originally passed by
                 * rtcfg_ioctl. But checking this assumptions does not cause
                 * any harm :o)
                 */
                RTNET_ASSERT(cmd_event->args.add.stage2_file == NULL, ;);

                cmd_event->args.add.stage2_file = new_conn->stage2_file;
            }

            return -EEXIST;
        }
    }

    list_add_tail(&new_conn->entry, &rtcfg_dev->conn_list);
    rtcfg_dev->other_stations++;

    rtos_res_unlock(&rtcfg_dev->dev_lock);

    cmd_event->args.add.conn_buf    = NULL;
    cmd_event->args.add.stage1_data = NULL;

    return 0;
}



/*** Server Frame Event Handlers ***/

static int rtcfg_server_recv_announce(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_device       *rtcfg_dev = &device[ifindex];
    struct list_head          *entry;
    struct rtcfg_frm_announce *announce_new;
    struct rtcfg_connection   *conn;
    struct rtnet_device       *rtdev;


    if (rtskb->len < sizeof(struct rtcfg_frm_announce)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid announce_new frame\n");
        return -EINVAL;
    }

    announce_new = (struct rtcfg_frm_announce *)rtskb->data;

    rtos_res_lock(&rtcfg_dev->dev_lock);

    list_for_each(entry, &rtcfg_dev->conn_list) {
        conn = list_entry(entry, struct rtcfg_connection, entry);

        switch (announce_new->addr_type) {
            case RTCFG_ADDR_IP:
                if ((conn->addr_type == RTCFG_ADDR_IP) &&
                    (*(u32 *)announce_new->addr ==
                        conn->addr.ip_addr)) {
                    /* save MAC address - Ethernet-specific! */
                    memcpy(conn->mac_addr, rtskb->mac.ethernet->h_source,
                           ETH_ALEN);

                    rtdev = rtskb->rtdev;

                    /* update routing table */
                    rt_ip_route_add_host(conn->addr.ip_addr, conn->mac_addr,
                                         rtdev);

                    /* remove IP address */
                    __rtskb_pull(rtskb, RTCFG_ADDRSIZE_IP);

                    rtcfg_do_conn_event(conn, RTCFG_FRM_ANNOUNCE_NEW, rtskb);

                    goto out;
                }
                break;

            case RTCFG_ADDR_MAC:
                /* Ethernet-specific! */
                if (memcmp(conn->mac_addr, rtskb->mac.ethernet->h_source,
                           ETH_ALEN) == 0) {
                    rtcfg_do_conn_event(conn, RTCFG_FRM_ANNOUNCE_NEW, rtskb);

                    goto out;
                }
                break;
        }
    }

out:
    rtos_res_unlock(&rtcfg_dev->dev_lock);

    kfree_rtskb(rtskb);
    return 0;
}



static int rtcfg_server_recv_ack(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_device     *rtcfg_dev = &device[ifindex];
    struct list_head        *entry;
    struct rtcfg_connection *conn;


    if (rtskb->len < sizeof(struct rtcfg_frm_ack_cfg)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid ack_cfg frame\n");
        return -EINVAL;
    }

    rtos_res_lock(&rtcfg_dev->dev_lock);

    list_for_each(entry, &rtcfg_dev->conn_list) {
        conn = list_entry(entry, struct rtcfg_connection, entry);

        /* find the corresponding connection - Ethernet-specific! */
        if (memcmp(conn->mac_addr,
                   rtskb->mac.ethernet->h_source, ETH_ALEN) != 0)
            continue;

        rtcfg_do_conn_event(conn, RTCFG_FRM_ACK_CFG, rtskb);

        break;
    }

    rtos_res_unlock(&rtcfg_dev->dev_lock);

    kfree_rtskb(rtskb);
    return 0;
}



static int rtcfg_server_recv_ready(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_device     *rtcfg_dev = &device[ifindex];
    struct list_head        *entry;
    struct rtcfg_connection *conn;


    if (rtskb->len < sizeof(struct rtcfg_frm_ready)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid ready frame\n");
        return -EINVAL;
    }

    rtos_res_lock(&rtcfg_dev->dev_lock);

    list_for_each(entry, &rtcfg_dev->conn_list) {
        conn = list_entry(entry, struct rtcfg_connection, entry);

        /* find the corresponding connection - Ethernet-specific! */
        if (memcmp(conn->mac_addr,
                   rtskb->mac.ethernet->h_source, ETH_ALEN) != 0)
            continue;

        rtcfg_do_conn_event(conn, RTCFG_FRM_READY, rtskb);

        break;
    }

    rtos_res_unlock(&rtcfg_dev->dev_lock);

    kfree_rtskb(rtskb);
    return 0;
}



/*** Client Command Event Handlers ***/

static int rtcfg_client_get_frag(int ifindex, struct rt_proc_call *call)
{
    struct rtcfg_device *rtcfg_dev = &device[ifindex];
    struct rtcfg_cmd    *cmd_event;


    cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

    rtos_res_lock(&rtcfg_dev->dev_lock);

    if ((rtcfg_dev->flags & RTCFG_FLAG_STAGE_2_DATA) == 0) {
        rtos_res_unlock(&rtcfg_dev->dev_lock);
        return -EINVAL;
    }

    rtcfg_send_ack(ifindex);

    if (rtcfg_dev->cfg_offs >= rtcfg_dev->cfg_len) {
        if (rtcfg_dev->stations_found == rtcfg_dev->other_stations) {
            rtpc_complete_call(call, 0);

            rtcfg_next_main_state(ifindex,
                ((rtcfg_dev->flags & RTCFG_FLAG_READY) != 0) ?
                RTCFG_MAIN_CLIENT_READY : RTCFG_MAIN_CLIENT_2);
        } else {
            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ALL_FRAMES);
            rtcfg_queue_blocking_call(ifindex, call);
        }
    } else
        rtcfg_queue_blocking_call(ifindex, call);

    rtos_res_unlock(&rtcfg_dev->dev_lock);

    return -CALL_PENDING;
}



/*** Client Frame Event Handlers ***/

static void rtcfg_client_recv_stage_1(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_frm_stage_1_cfg *stage_1_cfg;
    struct rt_proc_call          *call;
    struct rtcfg_cmd             *cmd_event;
    struct rtcfg_device          *rtcfg_dev = &device[ifindex];
    struct rtnet_device          *rtdev, *tmp;
    u32                          daddr, saddr, mask, bcast;
    u8                           addr_type;
    int                          ret;


    if (rtskb->len < sizeof(struct rtcfg_frm_stage_1_cfg)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid stage_1_cfg frame\n");
        kfree_rtskb(rtskb);
        return;
    }

    stage_1_cfg = (struct rtcfg_frm_stage_1_cfg *)rtskb->data;
    __rtskb_pull(rtskb, sizeof(struct rtcfg_frm_stage_1_cfg));

    addr_type = stage_1_cfg->addr_type;

    switch (stage_1_cfg->addr_type) {
        case RTCFG_ADDR_IP:
            if (rtskb->len < sizeof(struct rtcfg_frm_stage_1_cfg) +
                    2*RTCFG_ADDRSIZE_IP) {
                RTCFG_DEBUG(1, "RTcfg: received invalid stage_1_cfg "
                            "frame\n");
                kfree_rtskb(rtskb);
                break;
            }

            rtdev = rtskb->rtdev;

            daddr = *(u32*)stage_1_cfg->client_addr;
            stage_1_cfg = (struct rtcfg_frm_stage_1_cfg *)
                (((u8 *)stage_1_cfg) + RTCFG_ADDRSIZE_IP);

            saddr = *(u32*)stage_1_cfg->server_addr;
            stage_1_cfg = (struct rtcfg_frm_stage_1_cfg *)
                (((u8 *)stage_1_cfg) + RTCFG_ADDRSIZE_IP);

            __rtskb_pull(rtskb, 2*RTCFG_ADDRSIZE_IP);

            /* Broadcast: IP is used to address client */
            if (rtskb->pkt_type == PACKET_BROADCAST) {
                /* directed to us? */
                if (daddr != rtdev->local_ip) {
                    kfree_rtskb(rtskb);
                    break;
                }
            /* Unicast: IP address is assigned by the server */
            } else {
                /* default netmask */
                if (ntohl(daddr) <= 0x7FFFFFFF)         /* 127.255.255.255  */
                    mask = 0x000000FF;                  /* 255.0.0.0        */
                else if (ntohl(daddr) <= 0xBFFFFFFF)    /* 191.255.255.255  */
                    mask = 0x0000FFFF;                  /* 255.255.0.0      */
                else
                    mask = 0x00FFFFFF;                  /* 255.255.255.0    */
                bcast = daddr | (~mask);

                rt_ip_route_del_all(rtdev); /* cleanup routing table */

                rtdev->local_ip     = daddr;
                rtdev->broadcast_ip = bcast;

                if ((tmp = rtdev_get_loopback()) != NULL) {
                    rt_ip_route_add_host(rtdev->local_ip,
                                         tmp->dev_addr, tmp);
                    rtdev_dereference(tmp);
                }

                if (rtdev->flags & IFF_BROADCAST)
                    rt_ip_route_add_host(daddr, rtdev->broadcast, rtdev);
            }

            /* update routing table */
            rt_ip_route_add_host(saddr, rtskb->mac.ethernet->h_source, rtdev);

            /* fall through */
        case RTCFG_ADDR_MAC:
            rtos_res_lock(&rtcfg_dev->dev_lock);

            rtcfg_dev->addr_type = addr_type;

            /* Ethernet-specific */
            memcpy(rtcfg_dev->srv_mac_addr, rtskb->mac.ethernet->h_source,
                   ETH_ALEN);

            rtcfg_dev->burstrate = stage_1_cfg->burstrate;

            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_1);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            while (1) {
                call = rtcfg_dequeue_blocking_call(ifindex);
                if (call == NULL)
                    break;

                cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

                if (cmd_event->event_id == RTCFG_CMD_CLIENT) {
                    ret = 0;

                    /* note: only the first pending call gets data */
                    if ((rtskb != NULL) &&
                        (cmd_event->args.client.buffer_size > 0)) {
                        ret = ntohs(stage_1_cfg->cfg_len);

                        cmd_event->args.client.rtskb = rtskb;
                        rtskb = NULL;
                    }
                } else
                    ret = -EINVAL;

                rtpc_complete_call(call, ret);
            }

            if (rtskb)
                kfree_rtskb(rtskb);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown addr_type %d in %s()\n",
                        stage_1_cfg->addr_type, __FUNCTION__);
            kfree_rtskb(rtskb);
    }
}



static int rtcfg_add_to_station_list(struct rtcfg_device *rtcfg_dev,
                                     u8 *mac_addr, u8 flags)
{
   if (rtcfg_dev->stations_found == rtcfg_dev->max_stations) {
        RTCFG_DEBUG(1, "RTcfg: insufficient memory for storing new station "
                    "address\n");
        return -ENOMEM;
    }

    /* Ethernet-specific! */
    memcpy(&rtcfg_dev->station_addr_list[rtcfg_dev->stations_found].mac_addr,
           mac_addr, ETH_ALEN);

    rtcfg_dev->station_addr_list[rtcfg_dev->stations_found].flags = flags;

    rtcfg_dev->stations_found++;
    if ((flags & RTCFG_FLAG_READY) != 0)
        rtcfg_dev->stations_ready++;

    return 0;
}



/* Notes:
 *  o rtcfg_client_recv_announce does not release the passed rtskb.
 *  o If no error occured, rtcfg_client_recv_announce returns without releasing
 *    the devices lock.
 */
static int rtcfg_client_recv_announce(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_frm_announce *announce_frm;
    struct rtcfg_device       *rtcfg_dev = &device[ifindex];
    struct rtnet_device       *rtdev;
    u32                       saddr;
    u32                       i;
    int                       result;


    announce_frm = (struct rtcfg_frm_announce *)rtskb->data;

    if (rtskb->len < sizeof(struct rtcfg_frm_announce)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid announce frame (id: %d)\n",
                    announce_frm->head.id);
        return -EINVAL;
    }

    switch (announce_frm->addr_type) {
        case RTCFG_ADDR_IP:
            if (rtskb->len < sizeof(struct rtcfg_frm_announce) +
                    RTCFG_ADDRSIZE_IP) {
                RTCFG_DEBUG(1, "RTcfg: received invalid announce frame "
                            "(id: %d)\n", announce_frm->head.id);
                return -EINVAL;
            }

            rtdev = rtskb->rtdev;

            saddr = *(u32 *)announce_frm->addr;

            /* update routing table */
            rt_ip_route_add_host(saddr, rtskb->mac.ethernet->h_source, rtdev);

            announce_frm = (struct rtcfg_frm_announce *)
                (((u8 *)announce_frm) + RTCFG_ADDRSIZE_IP);

            break;

        /* nothing to do yet
        case RTCFG_ADDR_MAC:
            break;*/
    }

    rtos_res_lock(&rtcfg_dev->dev_lock);

    for (i = 0; i < rtcfg_dev->stations_found; i++)
        /* Ethernet-specific! */
        if (memcmp(rtcfg_dev->station_addr_list[i].mac_addr,
                   rtskb->mac.ethernet->h_source, ETH_ALEN) == 0)
            return 0;

    result = rtcfg_add_to_station_list(rtcfg_dev,
        rtskb->mac.ethernet->h_source, announce_frm->flags);
    if (result < 0)
        rtos_res_unlock(&rtcfg_dev->dev_lock);

    return result;
}



static void rtcfg_client_queue_frag(int ifindex, struct rtskb *rtskb,
                                    size_t data_len)
{
    struct rtcfg_device *rtcfg_dev = &device[ifindex];
    struct rt_proc_call *call;
    struct rtcfg_cmd    *cmd_event;
    int                 result;


    rtskb_trim(rtskb, data_len);

    if (rtcfg_dev->stage2_chain == NULL)
        rtcfg_dev->stage2_chain = rtskb;
    else {
        rtcfg_dev->stage2_chain->chain_end->next = rtskb;
        rtcfg_dev->stage2_chain->chain_end = rtskb;
    }

    rtcfg_dev->cfg_offs  += data_len;
    rtcfg_dev->chain_len += data_len;

    if ((rtcfg_dev->cfg_offs >= rtcfg_dev->cfg_len) ||
        (++rtcfg_dev->packet_counter == rtcfg_dev->burstrate)) {

        while (1) {
            call = rtcfg_dequeue_blocking_call(ifindex);
            if (call == NULL)
                break;

            cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

            result = 0;

            /* note: only the first pending call gets data */
            if (rtcfg_dev->stage2_chain != NULL) {
                result = rtcfg_dev->chain_len;
                cmd_event->args.announce.rtskb = rtcfg_dev->stage2_chain;
                rtcfg_dev->stage2_chain = NULL;
            }

            rtpc_complete_call(call,
                (cmd_event->event_id == RTCFG_CMD_ANNOUNCE) ?
                result : -EINVAL);
        }

        rtcfg_dev->packet_counter = 0;
        rtcfg_dev->chain_len      = 0;
    }
}



static void rtcfg_client_recv_stage_2_cfg(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_frm_stage_2_cfg *stage_2_cfg;
    struct rtcfg_device          *rtcfg_dev = &device[ifindex];
    size_t                       data_len;


    if (rtskb->len < sizeof(struct rtcfg_frm_stage_2_cfg)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid stage_2_cfg frame\n");
        kfree_rtskb(rtskb);
        return;
    }

    stage_2_cfg = (struct rtcfg_frm_stage_2_cfg *)rtskb->data;
    __rtskb_pull(rtskb, sizeof(struct rtcfg_frm_stage_2_cfg));

    rtos_res_lock(&rtcfg_dev->dev_lock);

    /* add server to station list */
    if (rtcfg_add_to_station_list(rtcfg_dev,
            rtskb->mac.ethernet->h_source, stage_2_cfg->flags) < 0) {
        rtos_res_unlock(&rtcfg_dev->dev_lock);
        RTCFG_DEBUG(1, "RTcfg: unable to process stage_2_cfg frage\n");
        kfree_rtskb(rtskb);
        return;
    }

    rtcfg_dev->other_stations = ntohl(stage_2_cfg->stations);
    rtcfg_dev->cfg_len        = ntohl(stage_2_cfg->cfg_len);
    data_len = MIN(rtcfg_dev->cfg_len, rtskb->len);

    if (((rtcfg_dev->flags & RTCFG_FLAG_STAGE_2_DATA) != 0) &&
        (data_len > 0)) {
        rtcfg_client_queue_frag(ifindex, rtskb, data_len);
        rtskb = NULL;

        if (rtcfg_dev->stations_found == rtcfg_dev->other_stations)
            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ALL_KNOWN);
    } else {
        rtcfg_send_ack(ifindex);

        if (rtcfg_dev->stations_found == rtcfg_dev->other_stations) {
            rtcfg_complete_cmd(ifindex, RTCFG_CMD_ANNOUNCE, 0);

            rtcfg_next_main_state(ifindex,
                ((rtcfg_dev->flags & RTCFG_FLAG_READY) != 0) ?
                RTCFG_MAIN_CLIENT_READY : RTCFG_MAIN_CLIENT_2);
        } else
            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ALL_FRAMES);
    }

    rtos_res_unlock(&rtcfg_dev->dev_lock);

    if (rtskb != NULL)
        kfree_rtskb(rtskb);
}



static void rtcfg_client_recv_stage_2_frag(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_frm_stage_2_cfg_frag *stage_2_frag;
    struct rtcfg_device               *rtcfg_dev = &device[ifindex];
    size_t                            data_len;


    if (rtskb->len < sizeof(struct rtcfg_frm_stage_2_cfg_frag)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid stage_2_cfg_frag frame\n");
        kfree_rtskb(rtskb);
        return;
    }

    stage_2_frag = (struct rtcfg_frm_stage_2_cfg_frag *)rtskb->data;
    __rtskb_pull(rtskb, sizeof(struct rtcfg_frm_stage_2_cfg_frag));

    rtos_res_lock(&rtcfg_dev->dev_lock);

    data_len = MIN(rtcfg_dev->cfg_len - rtcfg_dev->cfg_offs, rtskb->len);

    if ((rtcfg_dev->flags & RTCFG_FLAG_STAGE_2_DATA) == 0) {
        RTCFG_DEBUG(1, "RTcfg: unexpected stage 2 fragment, we did not "
                    "request any data!\n");

    } else if (rtcfg_dev->cfg_offs != ntohl(stage_2_frag->frag_offs)) {
        RTCFG_DEBUG(1, "RTcfg: unexpected stage 2 fragment (expected: %d, "
                    "received: %d)\n", rtcfg_dev->cfg_offs,
                    ntohl(stage_2_frag->frag_offs));

        rtcfg_send_ack(ifindex);
        rtcfg_dev->packet_counter = 0;
    } else {
        rtcfg_client_queue_frag(ifindex, rtskb, data_len);
        rtskb = NULL;
    }

    rtos_res_unlock(&rtcfg_dev->dev_lock);

    if (rtskb != NULL)
        kfree_rtskb(rtskb);
}



/* Notes:
 *  o If no error occured, rtcfg_client_recv_ready returns without releasing
 *    the devices lock.
 */
static int rtcfg_client_recv_ready(int ifindex, struct rtskb *rtskb)
{
    struct rtcfg_frm_ready *ready_frm;
    struct rtcfg_device    *rtcfg_dev = &device[ifindex];
    u32                    i;


    ready_frm = (struct rtcfg_frm_ready *)rtskb->data;

    if (rtskb->len < sizeof(struct rtcfg_frm_ready)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid ready frame\n");
        kfree_rtskb(rtskb);
        return -EINVAL;
    }

    rtos_res_lock(&rtcfg_dev->dev_lock);

    for (i = 0; i < rtcfg_dev->stations_found; i++)
        /* Ethernet-specific! */
        if (memcmp(rtcfg_dev->station_addr_list[i].mac_addr,
                   rtskb->mac.ethernet->h_source, ETH_ALEN) == 0) {
            if ((rtcfg_dev->station_addr_list[i].flags &
                 RTCFG_FLAG_READY) == 0) {
                rtcfg_dev->station_addr_list[i].flags |=
                    RTCFG_FLAG_READY;
                rtcfg_dev->stations_ready++;
            }
            break;
        }

    kfree_rtskb(rtskb);
    return 0;
}



static void rtcfg_complete_cmd(int ifindex, RTCFG_EVENT event_id, int result)
{
    struct rt_proc_call *call;
    struct rtcfg_cmd    *cmd_event;


    while (1) {
        call = rtcfg_dequeue_blocking_call(ifindex);
        if (call == NULL)
            break;

        cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

        rtpc_complete_call(call,
            (cmd_event->event_id == event_id) ? result : -EINVAL);
    }
}



void rtcfg_init_state_machines(void)
{
    int                 i;
    struct rtcfg_device *rtcfg_dev;


    memset(device, 0, sizeof(device));

    for (i = 0; i < MAX_RT_DEVICES; i++) {
        rtcfg_dev = &device[i];
        rtcfg_dev->state = RTCFG_MAIN_OFF;

        INIT_LIST_HEAD(&rtcfg_dev->conn_list);
        rtos_res_lock_init(&rtcfg_dev->dev_lock);

        INIT_LIST_HEAD(&rtcfg_dev->event_calls);
        rtos_spin_lock_init(&rtcfg_dev->event_calls_lock);
    }
}



void rtcfg_cleanup_state_machines(void)
{
    int                     i;
    struct rtcfg_device     *rtcfg_dev;
    struct rtcfg_connection *conn;
    struct list_head        *entry;
    struct list_head        *tmp;
    struct rt_proc_call     *call;


    for (i = 0; i < MAX_RT_DEVICES; i++) {
        rtcfg_dev = &device[i];

        rtos_task_delete(&rtcfg_dev->timer_task);
        rtos_res_lock_delete(&rtcfg_dev->dev_lock);

        list_for_each_safe(entry, tmp, &rtcfg_dev->conn_list) {
            conn = list_entry(entry, struct rtcfg_connection, entry);

            if (conn->stage1_data != NULL)
                kfree(conn->stage1_data);

            if ((conn->stage2_file != NULL) &&
                (rtcfg_release_file(conn->stage2_file) == 0)){
                vfree(conn->stage2_file->buffer);
                kfree(conn->stage2_file);
            }

            kfree(entry);
        }

        if (rtcfg_dev->station_addr_list != NULL)
            kfree(rtcfg_dev->station_addr_list);

        if (rtcfg_dev->stage2_chain != NULL)
            kfree_rtskb(rtcfg_dev->stage2_chain);

        while (1) {
            call = rtcfg_dequeue_blocking_call(i);
            if (call == NULL)
                break;

            rtpc_complete_call_nrt(call, -ENODEV);
        }
    }
}
