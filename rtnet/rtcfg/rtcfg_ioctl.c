/***
 *
 *  rtcfg/rtcfg_ioctl.c
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

#include <rtcfg.h>
#include <rtnet_rtpc.h>
#include <rtcfg/rtcfg_event.h>
#include <rtcfg/rtcfg_frame.h>


int rtcfg_event_handler(struct rt_proc_call *call)
{
    struct rtcfg_cmd *cmd_event;


    cmd_event = rtpc_get_priv(call, struct rtcfg_cmd);
    return rtcfg_do_main_event(cmd_event->ifindex, cmd_event->event_id, call);
}



void cleanup_cmd_add(struct rt_proc_call *call)
{
    struct rtcfg_cmd *cmd;
    void             *conn_buf;


    cmd      = rtpc_get_priv(call, struct rtcfg_cmd);
    conn_buf = cmd->args.add.conn_buf;
    if (conn_buf != NULL)
        kfree(conn_buf);
}



void cleanup_cmd_client(struct rt_proc_call *call)
{
    struct rtcfg_cmd *cmd;
    void             *addr_buf;


    cmd      = rtpc_get_priv(call, struct rtcfg_cmd);
    addr_buf = cmd->args.client.addr_buf;
    if (addr_buf != NULL)
        kfree(addr_buf);
}



int rtcfg_ioctl(struct rtnet_device *rtdev, unsigned int request, unsigned long arg)
{
    struct rtcfg_cmd        cmd;
    struct rtcfg_connection *conn_buf;
    u8                      *addr_buf;
    int                     ret;


    ret = copy_from_user(&cmd, (void *)arg, sizeof(cmd));
    if (ret != 0)
        return -EFAULT;

    cmd.ifindex  = rtdev->ifindex;
    cmd.event_id = _IOC_NR(request);

    switch (request) {
        case RTCFG_IOC_SERVER:
            ret = rtpc_dispatch_call(rtcfg_event_handler, 0, &cmd,
                                     sizeof(cmd), NULL);
            break;

        case RTCFG_IOC_ADD_IP:
            conn_buf = kmalloc(sizeof(struct rtcfg_connection), GFP_KERNEL);
            if (conn_buf == NULL)
                return -ENOMEM;
            cmd.args.add.conn_buf = conn_buf;

            ret = rtpc_dispatch_call(rtcfg_event_handler, 0, &cmd,
                                     sizeof(cmd), cleanup_cmd_add);
            break;

        case RTCFG_IOC_WAIT:
            ret = rtpc_dispatch_call(rtcfg_event_handler,
                                     cmd.args.wait.timeout, &cmd,
                                     sizeof(cmd), NULL);
            break;

        case RTCFG_IOC_CLIENT:
            addr_buf = kmalloc(RTCFG_MAX_ADDRSIZE*cmd.args.client.max_clients,
                               GFP_KERNEL);
            if (addr_buf == NULL)
                return -ENOMEM;
            cmd.args.client.addr_buf = addr_buf;

            ret = rtpc_dispatch_call(rtcfg_event_handler,
                                     cmd.args.client.timeout, &cmd,
                                     sizeof(cmd), cleanup_cmd_client);
            break;

        case RTCFG_IOC_ANNOUNCE:
            ret = rtpc_dispatch_call(rtcfg_event_handler,
                                     cmd.args.announce.timeout, &cmd,
                                     sizeof(cmd), NULL);
            break;

        default:
            ret = -ENOTTY;
    }

    return ret;
}



struct rtnet_ioctls rtcfg_ioctls = {
    service_name:   "RTcfg",
    ioctl_type:     RTNET_IOC_TYPE_RTCFG,
    handler:        rtcfg_ioctl
};
