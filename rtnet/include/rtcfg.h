/***
 *
 *  include/rtcfg.h
 *
 *  Real-Time Configuration Distribution Protocol
 *
 *  Copyright (C) 2004 Jan Kiszka <jan.kiszka@web.de>
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

#ifndef __RTCFG_H_
#define __RTCFG_H_

#include <rtnet_chrdev.h>


#define NEED_STAGE1_FILE        1
#define NEED_STAGE2_FILE        2


typedef enum {
    RTCFG_CMD_SERVER,
    RTCFG_CMD_ADD_IP,
    RTCFG_CMD_ADD_MAC,
    RTCFG_CMD_DEL_IP,
    RTCFG_CMD_DEL_MAC,
    RTCFG_CMD_WAIT,
    RTCFG_CMD_CLIENT,
    RTCFG_CMD_ANNOUNCE,

    /* internal usage only */
    RTCFG_FRM_STAGE_1_CFG,
    RTCFG_FRM_ANNOUNCE_NEW,
    RTCFG_FRM_ANNOUNCE_REPLY,
    RTCFG_FRM_STAGE_2_CFG,
    RTCFG_FRM_STAGE_2_CFG_FRAG,
    RTCFG_FRM_ACK_CFG,
    RTCFG_FRM_HEARTBEAT
} RTCFG_EVENT;

struct rtcfg_file {
    ino_t       inode;
    size_t      size;
    time_t      modtime;
    void        *buffer;
    size_t      frag_size;
};

struct rtcfg_connection;

struct rtcfg_cmd {
    struct rtnet_ioctl_head head;

    union {
        struct {
            unsigned int period;
            unsigned int burstrate;
            unsigned int heartbeat;
            unsigned int threshold;
        } server;

        struct {
            struct rtcfg_file stage1_file;
            struct rtcfg_file stage2_file;
            __u32             ip_addr;
            __u8              mac_addr[MAX_ADDR_LEN];

            /* internal usage only */
            struct rtcfg_connection *conn_buf;
        } add;

        struct {
            __u32 ip_addr;
            __u8  mac_addr[MAX_ADDR_LEN];

            /* internal usage only */
            struct rtcfg_connection *conn_buf;
        } del;

        struct {
            unsigned int timeout;
        } wait;

        struct {
            unsigned int timeout;
            void         *buffer;
            size_t       buffer_size;
            unsigned int max_clients;

            /* internal usage only */
            __u8 *addr_buf;
        } client;

        struct {
            unsigned int timeout;
            void         *buffer;
            size_t       buffer_size;
            unsigned int burstrate;
        } announce;
    } args;

    /* internal usage only */
    int         ifindex;
    RTCFG_EVENT event_id;
};


#define RTCFG_IOC_SERVER        _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_SERVER,  \
                                     sizeof(struct rtcfg_cmd))
#define RTCFG_IOC_ADD_IP        _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_ADD_IP,  \
                                     sizeof(struct rtcfg_cmd))
#define RTCFG_IOC_ADD_MAC       _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_ADD_MAC, \
                                     sizeof(struct rtcfg_cmd))
#define RTCFG_IOC_DEL_IP        _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_DEL_IP,  \
                                     sizeof(struct rtcfg_cmd))
#define RTCFG_IOC_DEL_MAC       _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_DEL_MAC, \
                                     sizeof(struct rtcfg_cmd))
#define RTCFG_IOC_WAIT          _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_WAIT,    \
                                     sizeof(struct rtcfg_cmd))
#define RTCFG_IOC_CLIENT        _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_CLIENT,  \
                                     sizeof(struct rtcfg_cmd))
#define RTCFG_IOC_ANNOUNCE      _IOW(RTNET_IOC_TYPE_RTCFG, RTCFG_CMD_ANNOUNCE,\
                                     sizeof(struct rtcfg_cmd))

#endif /* __RTCFG_H_ */
