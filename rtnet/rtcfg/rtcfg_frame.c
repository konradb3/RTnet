/***
 *
 *  rtcfg/rtcfg_frame.c
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
#include <linux/if_ether.h>

#include <rtnet.h>
#include <rtcfg/rtcfg.h>
#include <rtcfg/rtcfg_frame.h>


#define RTCFG_PROTOCOL      0x9022


static int          rtcfg_sock;
static rtos_task_t  rx_task;



static void rtcfg_rx_handler(int arg)
{
    char                   buf[ETH_DATA_LEN];
    struct sockaddr_ll     addr;
    struct rtcfg_frm_event event = {0, (struct rtcfg_frm_head *)&buf, &addr};
    socklen_t              addr_len;


    while (1) {
        addr_len = sizeof(addr);

        event.frm_size =
            rt_socket_recvfrom(rtcfg_sock, buf, sizeof(buf), 0,
                               (struct sockaddr *)&addr, &addr_len);

        if (event.frm_size < (int)sizeof(struct rtcfg_frm_head)) {
            RTCFG_DEBUG(1, "RTcfg: error while receiving frames in %s(), "
                        "result=%d\n", __FUNCTION__, event.frm_size);
            return;
        }

        rtcfg_do_main_event(addr.sll_ifindex,
                            event.frm_head->id + RTCFG_FRM_STAGE_1_CFG,
                            &event);
    }
}



int rtcfg_send_stage_1(struct rtcfg_connection *conn)
{
    struct msghdr                msg;
    struct iovec                 iov;
    struct sockaddr_ll           addr;
    struct rtcfg_frm_stage_1_cfg *stage_1_frm;
    char                         buf[sizeof(struct rtcfg_frm_stage_1_cfg) +
                                     2*RTCFG_MAX_ADDRSIZE];
    size_t                       frm_size;
    struct rtnet_device          *rtdev;


    stage_1_frm = (struct rtcfg_frm_stage_1_cfg *)buf;
    frm_size    = sizeof(struct rtcfg_frm_stage_1_cfg);

    stage_1_frm->head.id      = RTCFG_ID_STAGE_1_CFG;
    stage_1_frm->head.version = 0;
    stage_1_frm->addr_type    = conn->addr_type;

    if (conn->addr_type == RTCFG_ADDR_IP) {
        *(u32*)stage_1_frm->client_addr = conn->addr.ip_addr;

        stage_1_frm = (struct rtcfg_frm_stage_1_cfg *)
            (buf + RTCFG_ADDRSIZE_IP);

        rtdev = rtdev_get_by_index(conn->ifindex);
        if (rtdev == NULL)
            return -ENODEV;

        *(u32*)stage_1_frm->server_addr = rtdev->local_addr;

        rtdev_dereference(rtdev);

        stage_1_frm = (struct rtcfg_frm_stage_1_cfg *)
            (buf + 2*RTCFG_ADDRSIZE_IP);
        frm_size += 2*RTCFG_ADDRSIZE_IP;
    }

    stage_1_frm->burst_rate = 1; /* TODO: set to reasonable value */
    stage_1_frm->cfg_len    = htons(0);

    addr.sll_family   = PF_PACKET;
    addr.sll_protocol = htons(RTCFG_PROTOCOL);
    addr.sll_ifindex  = conn->ifindex;
    addr.sll_halen    = 6;
    memcpy(addr.sll_addr, conn->mac_addr, 6);

    iov.iov_base = buf;
    iov.iov_len  = frm_size;

    msg.msg_name    = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    return rt_socket_sendmsg(rtcfg_sock, &msg, 0);
}



int rtcfg_send_stage_2(struct rtcfg_connection *conn)
{
    struct msghdr                msg;
    struct iovec                 iov;
    struct rtcfg_frm_stage_2_cfg *stage_2_frm;
    char                         buf[sizeof(struct rtcfg_frm_stage_2_cfg)];
    size_t                       frm_size;
    struct sockaddr_ll           addr;


    stage_2_frm = (struct rtcfg_frm_stage_2_cfg *)buf;
    frm_size    = sizeof(struct rtcfg_frm_stage_2_cfg);

    stage_2_frm->head.id          = RTCFG_ID_STAGE_2_CFG;
    stage_2_frm->head.version     = 0;
    stage_2_frm->clients          = htonl(device[conn->ifindex].clients - 1);
    stage_2_frm->heartbeat_period = htons(0);
    stage_2_frm->cfg_len          = htonl(0);

    addr.sll_family   = PF_PACKET;
    addr.sll_protocol = htons(RTCFG_PROTOCOL);
    addr.sll_ifindex  = conn->ifindex;
    addr.sll_halen    = 6;
    memcpy(addr.sll_addr, conn->mac_addr, 6);

    iov.iov_base = buf;
    iov.iov_len  = frm_size;

    msg.msg_name    = &addr;
    msg.msg_namelen = sizeof(addr);
    msg.msg_iov     = &iov;
    msg.msg_iovlen  = 1;

    return rt_socket_sendmsg(rtcfg_sock, &msg, 0);
}



int rtcfg_send_announce_new(int ifindex)
{
    struct rtcfg_frm_announce_new *announce_frm;
    char                          buf[sizeof(struct rtcfg_frm_announce_new) +
                                      RTCFG_MAX_ADDRSIZE];
    size_t                        frm_size;
    struct rtnet_device           *rtdev;
    struct sockaddr_ll            addr;


    announce_frm = (struct rtcfg_frm_announce_new *)buf;
    frm_size     = sizeof(struct rtcfg_frm_announce_new);

    announce_frm->head.id      = RTCFG_ID_ANNOUNCE_NEW;
    announce_frm->head.version = 0;
    announce_frm->addr_type    = device[ifindex].addr_type;

    if (announce_frm->addr_type == RTCFG_ADDR_IP) {
        rtdev = rtdev_get_by_index(ifindex);
        if (rtdev == NULL)
            return -ENODEV;

        *(u32*)announce_frm->addr = rtdev->local_addr;

        rtdev_dereference(rtdev);

        announce_frm =
            (struct rtcfg_frm_announce_new *)(buf+RTCFG_ADDRSIZE_IP);
        frm_size += RTCFG_ADDRSIZE_IP;
    }

    announce_frm->get_cfg    = 0;
    announce_frm->burst_rate = 1; /* TODO: set to reasonable value */

    addr.sll_family   = PF_PACKET;
    addr.sll_protocol = htons(RTCFG_PROTOCOL);
    addr.sll_ifindex  = ifindex;
    addr.sll_halen    = 6;
    memset(addr.sll_addr, 0xFF, 6); /* send as broadcast */

    return rt_socket_sendto(rtcfg_sock, buf, frm_size, 0,
                            (struct sockaddr *)&addr, sizeof(addr));
}



int rtcfg_send_announce_reply(int ifindex, u8 *dest_mac_addr)
{
    struct rtcfg_frm_announce *announce_frm;
    char                      buf[sizeof(struct rtcfg_frm_announce) +
                                  RTCFG_MAX_ADDRSIZE];
    size_t                    frm_size;
    struct rtnet_device       *rtdev;
    struct sockaddr_ll        addr;


    announce_frm = (struct rtcfg_frm_announce *)buf;
    frm_size     = sizeof(struct rtcfg_frm_announce);

    announce_frm->head.id      = RTCFG_ID_ANNOUNCE_REPLY;
    announce_frm->head.version = 0;
    announce_frm->addr_type    = device[ifindex].addr_type;

    if (announce_frm->addr_type == RTCFG_ADDR_IP) {
        rtdev = rtdev_get_by_index(ifindex);
        if (rtdev == NULL)
            return -ENODEV;

        *(u32*)announce_frm->addr = rtdev->local_addr;

        rtdev_dereference(rtdev);

        frm_size += RTCFG_ADDRSIZE_IP;
    }

    addr.sll_family   = PF_PACKET;
    addr.sll_protocol = htons(RTCFG_PROTOCOL);
    addr.sll_ifindex  = ifindex;
    addr.sll_halen    = 6;
    memcpy(addr.sll_addr, dest_mac_addr, 6);

    return rt_socket_sendto(rtcfg_sock, buf, frm_size, 0,
                            (struct sockaddr *)&addr, sizeof(addr));
}



int rtcfg_send_ack(int ifindex)
{
    struct rtcfg_frm_ack_cfg *ack_frm;
    char                     buf[sizeof(struct rtcfg_frm_ack_cfg)];
    struct sockaddr_ll       addr;


    ack_frm = (struct rtcfg_frm_ack_cfg *)buf;

    ack_frm->head.id      = RTCFG_ID_ACK_CFG;
    ack_frm->head.version = 0;
    ack_frm->ack_len      = htonl(device[ifindex].cfg_offs);

    addr.sll_family   = PF_PACKET;
    addr.sll_protocol = htons(RTCFG_PROTOCOL);
    addr.sll_ifindex  = ifindex;
    addr.sll_halen    = 6;
    memcpy(addr.sll_addr, device[ifindex].srv_mac_addr, 6);

    return rt_socket_sendto(rtcfg_sock, buf, sizeof(struct rtcfg_frm_ack_cfg),
                            0, (struct sockaddr *)&addr, sizeof(addr));
}



int __init rtcfg_init_frames(void)
{
    int ret;


    rtcfg_sock = rt_socket(PF_PACKET, SOCK_DGRAM, htons(RTCFG_PROTOCOL));
    if (rtcfg_sock < 0) {
        return rtcfg_sock;
    }

    ret = rtos_task_init(&rx_task, rtcfg_rx_handler, 0,
                         RTOS_LOWEST_RT_PRIORITY);
    if (ret < 0)
        rt_socket_close(rtcfg_sock);
        return ret;

    return 0;
}



void rtcfg_cleanup_frames(void)
{
    while (rt_socket_close(rtcfg_sock) == -EAGAIN) {
        RTCFG_DEBUG(3, "RTcfg: waiting for socket to be closed\n");
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(1*HZ); /* wait a second */
    }

    rtos_task_delete(&rx_task);
}
