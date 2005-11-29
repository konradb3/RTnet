/***
 *
 *  include/tdma_chrdev.h
 *
 *  RTmac - real-time networking media access control subsystem
 *  Copyright (C) 2002      Marc Kleine-Budde <kleine-budde@gmx.de>,
 *                2003-2005 Jan Kiszka <Jan.Kiszka@web.de>
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

#ifndef __TDMA_CHRDEV_H_
#define __TDMA_CHRDEV_H_

#include <inttypes.h>

#include <rtnet_chrdev.h>


#define MIN_SLOT_SIZE       60


struct tdma_config {
    struct rtnet_ioctl_head head;

    union {
        struct {
            uint64_t        cycle_period;
            uint64_t        backup_sync_offset;
            unsigned int    cal_rounds;
            unsigned int    max_cal_requests;
            unsigned int    max_slot_id;
        } master;

        struct {
            unsigned int    cal_rounds;
            unsigned int    max_slot_id;
        } slave;

        struct {
            int             id;
            uint64_t        offset;
            unsigned int    period;
            unsigned int    phasing;
            unsigned int    size;
            int             joint_slot;
            unsigned int    cal_timeout;
            uint64_t        *cal_results;
        } set_slot;

        struct {
            int             id;
        } remove_slot;
    } args;
};


#define TDMA_IOC_MASTER                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 0, \
                                             struct tdma_config)
#define TDMA_IOC_SLAVE                  _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 1, \
                                             struct tdma_config)
#define TDMA_IOC_CAL_RESULT_SIZE        _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 2, \
                                             struct tdma_config)
#define TDMA_IOC_SET_SLOT               _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 3, \
                                             struct tdma_config)
#define TDMA_IOC_REMOVE_SLOT            _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 4, \
                                             struct tdma_config)
#define TDMA_IOC_DETACH                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 5, \
                                             struct tdma_config)

#endif /* __TDMA_CHRDEV_H_ */
