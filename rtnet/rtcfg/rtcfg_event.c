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

#include <linux/kernel.h>
#include <linux/list.h>

#include <rtdev.h>
#include <rtnet_rtpc.h>
#include <ipv4/arp.h>
#include <ipv4/route.h>
#include <rtcfg/rtcfg.h>
#include <rtcfg/rtcfg_conn_event.h>
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


#ifdef CONFIG_RTCFG_DEBUG
const char *rtcfg_event[] = {
    "RTCFG_CMD_SERVER",
    "RTCFG_CMD_ADD_IP",
    "RTCFG_CMD_ADD_MAC",
    "RTCFG_CMD_DEL_IP",
    "RTCFG_CMD_DEL_MAC",
    "RTCFG_CMD_WAIT",
    "RTCFG_CMD_CLIENT",
    "RTCFG_CMD_ANNOUNCE",
    "RTCFG_FRM_STAGE_1_CFG",
    "RTCFG_FRM_ANNOUNCE_NEW",
    "RTCFG_FRM_ANNOUNCE_REPLY",
    "RTCFG_FRM_STAGE_2_CFG",
    "RTCFG_FRM_STAGE_2_CFG_FRAG",
    "RTCFG_FRM_ACK_CFG",
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
    "RTCFG_MAIN_CLIENT_2"
};

static int rtcfg_debug = RTCFG_DEFAULT_DEBUG_LEVEL;

#endif /* CONFIG_RTCFG_DEBUG */


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
    rtcfg_main_state_client_2
};


static int rtcfg_client_recv_announce(
    int ifindex, struct rtcfg_frm_event *frm_event);



static void rtcfg_queue_blocking_call(int ifindex, struct rt_proc_call *call)
{
    unsigned long       flags;
    struct rtcfg_device *rtcfg_dev = &device[ifindex];


    rtos_spin_lock_irqsave(&rtcfg_dev->event_calls_lock, flags);
    list_add_tail(&call->list_entry, &rtcfg_dev->event_calls);
    rtos_spin_unlock_irqrestore(&rtcfg_dev->event_calls_lock, flags);
}



static struct rt_proc_call *rtcfg_dequeue_blocking_call(int ifindex)
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
            if (ret < 0)
                return ret;

            rtcfg_next_main_state(ifindex, RTCFG_MAIN_SERVER_RUNNING);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            break;

        case RTCFG_CMD_CLIENT:
            rtos_res_lock(&rtcfg_dev->dev_lock);

            rtcfg_dev->client_addr_list     = cmd_event->args.client.addr_buf;
            cmd_event->args.client.addr_buf = NULL;

            rtcfg_dev->max_clients = cmd_event->args.client.max_clients;

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



static int rtcfg_main_state_server_running(int ifindex, RTCFG_EVENT event_id,
                                           void* event_data)
{
    struct rt_proc_call           *call;
    struct rtcfg_cmd              *cmd_event;
    struct rtcfg_frm_event        *frm_event;
    struct rtcfg_connection       *conn;
    struct rtcfg_connection       *new_conn;
    struct rtcfg_device           *rtcfg_dev = &device[ifindex];
    struct list_head              *entry;
    struct rtcfg_frm_announce_new *announce_new;
    struct rtcfg_frm_ack_cfg      *ack_cfg;
    struct rtnet_device           *rtdev;


    switch (event_id) {
        case RTCFG_CMD_ADD_IP:
            call      = (struct rt_proc_call *)event_data;
            cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

            rtos_res_lock(&rtcfg_dev->dev_lock);

            list_for_each(entry, &rtcfg_dev->conn_list) {
                conn = list_entry(entry, struct rtcfg_connection, entry);
                if (conn->addr.ip_addr == cmd_event->args.add.ip_addr) {
                    rtos_res_unlock(&rtcfg_dev->dev_lock);
                    return -EEXIST;
                }
            }

            new_conn = cmd_event->args.add.conn_buf;
            cmd_event->args.add.conn_buf = NULL;

            new_conn->ifindex      = ifindex;
            new_conn->state        = RTCFG_CONN_SEARCHING;
            new_conn->addr_type    = RTCFG_ADDR_IP;
            new_conn->addr.ip_addr = cmd_event->args.add.ip_addr;
            new_conn->cfg_offs     = 0;

            /* MAC address yet unknown -> set to broadcast address */
            memset(new_conn->mac_addr, 0xFF, sizeof(new_conn->mac_addr));

            list_add_tail(&new_conn->entry, &rtcfg_dev->conn_list);
            rtcfg_dev->clients++;

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            break;

        case RTCFG_CMD_WAIT:
            rtos_res_lock(&rtcfg_dev->dev_lock);

            if (rtcfg_dev->clients == rtcfg_dev->clients_found)
                rtpc_complete_call((struct rt_proc_call *)event_data, 0);
            else
                rtcfg_queue_blocking_call(ifindex,
                    (struct rt_proc_call *)event_data);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        case RTCFG_FRM_ANNOUNCE_NEW:
            frm_event = (struct rtcfg_frm_event *)event_data;

            if (frm_event->frm_size <
                    (int)sizeof(struct rtcfg_frm_announce_new)) {
                RTCFG_DEBUG(1, "RTcfg: received invalid announce_new frame\n");
                break;
            }

            announce_new =
                (struct rtcfg_frm_announce_new *)frm_event->frm_head;

            rtos_res_lock(&rtcfg_dev->dev_lock);

            list_for_each(entry, &rtcfg_dev->conn_list) {
                conn = list_entry(entry, struct rtcfg_connection, entry);

                switch (announce_new->addr_type) {
                    case RTCFG_ADDR_IP:
                        if ((conn->addr_type == RTCFG_ADDR_IP) &&
                            (*(u32 *)announce_new->addr ==
                                conn->addr.ip_addr)) {
                            /* save MAC address */
                            memcpy(conn->mac_addr,
                                   frm_event->addr->sll_addr, 6);

                            rtdev = rtdev_get_by_index(ifindex);
                            if (rtdev == NULL)
                                break;

                            /* update ARP and routing tables */
                            if (rt_ip_route_add_if_new(rtdev,
                                    conn->addr.ip_addr, rtdev->local_addr,
                                    frm_event->addr->sll_addr) == 0)
                                rt_arp_table_add(conn->addr.ip_addr,
                                    frm_event->addr->sll_addr);

                            rtdev_dereference(rtdev);

                            rtcfg_do_conn_event(conn, event_id, event_data);

                            goto out;
                        }
                        break;

                    case RTCFG_ADDR_MAC:
                        if (memcmp(conn->mac_addr,
                                   frm_event->addr->sll_addr, 6) == 0) {
                            rtcfg_do_conn_event(conn, event_id, event_data);

                            goto out;
                        }
                        break;
                }
            }

          out:
            rtos_res_unlock(&rtcfg_dev->dev_lock);

            break;

        case RTCFG_FRM_ACK_CFG:
            frm_event = (struct rtcfg_frm_event *)event_data;

            if (frm_event->frm_size <
                    (int)sizeof(struct rtcfg_frm_announce_new)) {
                RTCFG_DEBUG(1, "RTcfg: received invalid announce_new frame\n");
                break;
            }

            ack_cfg = (struct rtcfg_frm_ack_cfg *)frm_event->frm_head;

            rtos_res_lock(&rtcfg_dev->dev_lock);

            list_for_each(entry, &rtcfg_dev->conn_list) {
                conn = list_entry(entry, struct rtcfg_connection, entry);

                /* find the corresponding connection */
                if (memcmp(conn->mac_addr, frm_event->addr->sll_addr, 6) != 0)
                    continue;

                rtcfg_do_conn_event(conn, event_id, frm_event);

                if (rtcfg_dev->clients == rtcfg_dev->clients_found)
                    while (1) {
                        call = rtcfg_dequeue_blocking_call(ifindex);
                        if (call == NULL)
                            break;

                        cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

                        rtpc_complete_call(call,
                            (cmd_event->event_id == RTCFG_CMD_WAIT) ?
                                0 : -EINVAL);
                    }

                break;
            }

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_main_state_client_0(int ifindex, RTCFG_EVENT event_id,
                                     void* event_data)
{
    struct rtcfg_frm_stage_1_cfg *stage_1_cfg;
    struct rt_proc_call          *call;
    struct rtcfg_cmd             *cmd_event;
    struct rtcfg_frm_event       *frm_event;
    struct rtcfg_device          *rtcfg_dev = &device[ifindex];
    struct rtnet_device          *rtdev;
    u32                          daddr, saddr;
    u8                           addr_type;


    frm_event = (struct rtcfg_frm_event *)event_data;

    switch (event_id) {
        case RTCFG_FRM_STAGE_1_CFG:
            if (frm_event->frm_size <
                    (int)sizeof(struct rtcfg_frm_stage_1_cfg)) {
                RTCFG_DEBUG(1, "RTcfg: received invalid stage_1_cfg frame\n");
                break;
            }

            stage_1_cfg = (struct rtcfg_frm_stage_1_cfg *)frm_event->frm_head;

            addr_type = stage_1_cfg->addr_type;

            switch (stage_1_cfg->addr_type) {
                case RTCFG_ADDR_IP:
                    if (frm_event->frm_size <
                            (int)sizeof(struct rtcfg_frm_stage_1_cfg) +
                                2*RTCFG_ADDRSIZE_IP) {
                        RTCFG_DEBUG(1, "RTcfg: received invalid stage_1_cfg "
                                    "frame\n");
                        break;
                    }

                    rtdev = rtdev_get_by_index(ifindex);
                    if (rtdev == NULL)
                        break;

                    daddr = *(u32*)stage_1_cfg->client_addr;
                    stage_1_cfg = (struct rtcfg_frm_stage_1_cfg *)
                        (((u8 *)frm_event->frm_head) + RTCFG_ADDRSIZE_IP);

                    saddr = *(u32*)stage_1_cfg->server_addr;
                    stage_1_cfg = (struct rtcfg_frm_stage_1_cfg *)
                        (((u8 *)frm_event->frm_head) + 2*RTCFG_ADDRSIZE_IP);

                    /* directed to us? */
                    if (daddr != rtdev->local_addr) {
                        rtdev_dereference(rtdev);
                        break;
                    }

                    /* update ARP and routing tables */
                    if (rt_ip_route_add_if_new(rtdev, saddr, daddr,
                                               frm_event->addr->sll_addr) == 0)
                        rt_arp_table_add(saddr, frm_event->addr->sll_addr);

                    rtdev_dereference(rtdev);

                    /* fall trough */
                case RTCFG_ADDR_MAC:
                    rtos_res_lock(&rtcfg_dev->dev_lock);

                    rtcfg_dev->addr_type = addr_type;

                    memcpy(rtcfg_dev->srv_mac_addr,
                           frm_event->addr->sll_addr, 6);

                    while (1) {
                        call = rtcfg_dequeue_blocking_call(ifindex);
                        if (call == NULL)
                            break;

                        cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

                        rtpc_complete_call(call,
                            (cmd_event->event_id == RTCFG_CMD_CLIENT) ?
                                0 : -EINVAL);
                    }

                    rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_1);

                    rtos_res_unlock(&rtcfg_dev->dev_lock);

                    break;

                default:
                    RTCFG_DEBUG(1, "RTcfg: unknown addr_type %d in %s()\n",
                                stage_1_cfg->addr_type, __FUNCTION__);
                    return -EINVAL;
            }
            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            rtcfg_client_recv_announce(ifindex, frm_event);

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
    struct rtcfg_device    *rtcfg_dev = &device[ifindex];
    struct rtcfg_frm_event *frm_event;
    int                    ret;


    switch (event_id) {
        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            break;

        case RTCFG_CMD_ANNOUNCE:
            rtos_res_lock(&rtcfg_dev->dev_lock);

            ret = rtcfg_send_announce_new(ifindex);
            if (ret < 0) {
                rtos_res_unlock(&rtcfg_dev->dev_lock);
                return ret;
            }

            rtcfg_queue_blocking_call(ifindex,
                (struct rt_proc_call *)event_data);

            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ANNOUNCED);

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            return -CALL_PENDING;

        case RTCFG_FRM_ANNOUNCE_NEW:
            frm_event = (struct rtcfg_frm_event *)event_data;

            if (rtcfg_client_recv_announce(ifindex, frm_event) == 0)
                rtcfg_send_announce_reply(ifindex, frm_event->addr->sll_addr);

            break;

        case RTCFG_FRM_ANNOUNCE_REPLY:
            rtcfg_client_recv_announce(ifindex,
                                       (struct rtcfg_frm_event *)event_data);
            break;

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static void rtcfg_complete_cmd_announce(int ifindex)
{
    struct rt_proc_call *call;
    struct rtcfg_cmd    *cmd_event;


    while (1) {
        call = rtcfg_dequeue_blocking_call(ifindex);
        if (call == NULL)
            break;

        cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);

        rtpc_complete_call(call,
            (cmd_event->event_id == RTCFG_CMD_ANNOUNCE) ? 0 : -EINVAL);
    }
}



static int rtcfg_main_state_client_announced(int ifindex, RTCFG_EVENT event_id,
                                             void* event_data)
{
    struct rtcfg_frm_stage_2_cfg *stage_2_cfg;
    struct rtcfg_frm_event       *frm_event;
    struct rtcfg_device          *rtcfg_dev = &device[ifindex];


    frm_event = (struct rtcfg_frm_event *)event_data;

    switch (event_id) {
        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            break;

        case RTCFG_FRM_STAGE_2_CFG:
            if (frm_event->frm_size <
                    (int)sizeof(struct rtcfg_frm_stage_2_cfg)) {
                RTCFG_DEBUG(1, "RTcfg: received invalid stage_2_cfg frame\n");
                break;
            }

            stage_2_cfg = (struct rtcfg_frm_stage_2_cfg *)frm_event->frm_head;

            rtos_res_lock(&rtcfg_dev->dev_lock);

            rtcfg_dev->clients = ntohl(stage_2_cfg->clients);

            rtcfg_send_ack(ifindex);

            if (rtcfg_dev->clients == rtcfg_dev->clients_found) {
                rtcfg_complete_cmd_announce(ifindex);
                rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_2);
            } else {
                rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ALL_FRAMES);
            }

            rtos_res_unlock(&rtcfg_dev->dev_lock);

            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            if (rtcfg_client_recv_announce(ifindex, frm_event) == 0)
                rtcfg_send_announce_reply(ifindex, frm_event->addr->sll_addr);

            break;

        case RTCFG_FRM_ANNOUNCE_REPLY:
            rtcfg_client_recv_announce(ifindex, frm_event);
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
    switch (event_id) {
        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
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
    struct rtcfg_frm_event *frm_event = (struct rtcfg_frm_event *)event_data;


    switch (event_id) {
        case RTCFG_FRM_STAGE_1_CFG:
            /* ignore */
            break;

        case RTCFG_FRM_ANNOUNCE_NEW:
            if (rtcfg_client_recv_announce(ifindex, frm_event) == 0)
                rtcfg_send_announce_reply(ifindex, frm_event->addr->sll_addr);

            break;

        case RTCFG_FRM_ANNOUNCE_REPLY:
            rtcfg_client_recv_announce(ifindex, frm_event);
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
    switch (event_id) {
        case RTCFG_FRM_ANNOUNCE_NEW:
            /* TODO: update tables with new address */

        default:
            RTCFG_DEBUG(1, "RTcfg: unknown event %s for rtdev %d in %s()\n",
                        rtcfg_event[event_id], ifindex, __FUNCTION__);
            return -EINVAL;
    }
    return 0;
}



static int rtcfg_client_recv_announce(int ifindex,
                                      struct rtcfg_frm_event *frm_event)
{
    struct rtcfg_frm_announce *announce_frm;
    struct rtcfg_device       *rtcfg_dev = &device[ifindex];
    struct rtnet_device       *rtdev;
    u32                       saddr;
    u32                       i;


    announce_frm = (struct rtcfg_frm_announce *)frm_event->frm_head;

    if ((announce_frm->head.id == RTCFG_ID_ANNOUNCE_NEW) &&
        (frm_event->frm_size < (int)sizeof(struct rtcfg_frm_announce_new))) {
        RTCFG_DEBUG(1, "RTcfg: received invalid announce_new frame\n");
        return -EINVAL;
    } else if (frm_event->frm_size < (int)sizeof(struct rtcfg_frm_announce)) {
        RTCFG_DEBUG(1, "RTcfg: received invalid announce_reply frame\n");
        return -EINVAL;
    }

    switch (announce_frm->addr_type) {
        case RTCFG_ADDR_IP:
            rtdev = rtdev_get_by_index(ifindex);
            if (rtdev == NULL)
                return -ENODEV;

            saddr = *(u32 *)announce_frm->addr;

            /* update ARP and routing tables */
            if (rt_ip_route_add_if_new(rtdev, saddr, rtdev->local_addr,
                                       frm_event->addr->sll_addr) == 0)
                rt_arp_table_add(saddr, frm_event->addr->sll_addr);

            rtdev_dereference(rtdev);

            break;

        /* nothing to do yet
        case RTCFG_ADDR_MAC:
            break;*/
    }

    rtos_res_lock(&rtcfg_dev->dev_lock);

    for (i = 0; i < rtcfg_dev->clients_found; i++)
        if (memcmp(&rtcfg_dev->client_addr_list[i*6],
                   frm_event->addr->sll_addr, 6) == 0)
            goto out;

    if (rtcfg_dev->clients_found == rtcfg_dev->max_clients) {
        rtos_res_unlock(&rtcfg_dev->dev_lock);

        RTCFG_DEBUG(1, "RTcfg: %s() reports insufficient memory for storing "
                    "new client address\n", __FUNCTION__);
        return -ENOMEM;
    }

    memcpy(&rtcfg_dev->client_addr_list[rtcfg_dev->clients_found],
           frm_event->addr->sll_addr, 6);
    rtcfg_dev->clients_found++;

    if (rtcfg_dev->clients == rtcfg_dev->clients_found) {
        rtcfg_complete_cmd_announce(ifindex);

        if (rtcfg_dev->state == RTCFG_MAIN_CLIENT_ALL_FRAMES)
            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_2);
        else
            rtcfg_next_main_state(ifindex, RTCFG_MAIN_CLIENT_ALL_KNOWN);
    }

  out:
    rtos_res_unlock(&rtcfg_dev->dev_lock);

    return 0;
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
    struct list_head        *entry;
    struct list_head        *tmp;
    struct rt_proc_call     *call;


    for (i = 0; i < MAX_RT_DEVICES; i++) {
        rtcfg_dev = &device[i];

        rtos_task_delete(&rtcfg_dev->timer_task);
        rtos_res_lock_delete(&rtcfg_dev->dev_lock);

        list_for_each_safe(entry, tmp, &rtcfg_dev->conn_list)
            kfree(entry);

        if (rtcfg_dev->client_addr_list != NULL)
            kfree(rtcfg_dev->client_addr_list);

        while (1) {
            call = rtcfg_dequeue_blocking_call(i);
            if (call == NULL)
                break;

            rtpc_complete_call_nrt(call, -ENODEV);
        }
    }
}
