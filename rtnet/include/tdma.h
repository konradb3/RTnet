/* include/rtmac_tdma.h
 *
 * rtmac - real-time networking media access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>,
 *               2003 Jan Kiszka <Jan.Kiszka@web.de>
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

#ifndef __TDMA_H_
#define __TDMA_H_

#include <rtnet_chrdev.h>


#ifdef __KERNEL__

#include <rtai.h>
#include <rtai_sched.h>

#include <rtmac/tdma/tdma.h>


static inline struct rtmac_tdma *tdma_get_by_device(const char *devname)
{
    struct rtnet_device *rtdev = rtdev_get_by_name(devname);

    if (rtdev) {
        /* This line is not a solution, it's a workaround. We need a revision of
         * this API in the future. */
        rtdev_dereference(rtdev);

        if (rtdev->mac_disc &&
            (rtdev->mac_disc->disc_type == __constant_htons(ETH_TDMA)) &&
            (rtdev->mac_priv))
            return (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    }
    return NULL;
}

static inline int tdma_wait_sof(struct rtmac_tdma *tdma)
{
    return (rt_sem_wait(&tdma->client_tx) != 0xFFFF) ? 0 : -1;
}

static inline RTIME tdma_get_delta_t(struct rtmac_tdma *tdma)
{
    RTIME delta_t;
    unsigned long flags;

    flags = rt_spin_lock_irqsave(&tdma->delta_t_lock);
    delta_t = tdma->delta_t;
    rt_spin_unlock_irqrestore(flags, &tdma->delta_t_lock);

    return delta_t;
}

#endif /* __KERNEL__ */


struct tdma_config {
    struct rtnet_ioctl_head head;

    __u32           ip_addr;
    unsigned int    cycle;
    unsigned int    mtu;
    unsigned int    offset;
};


#define TDMA_IOC_CLIENT                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 0, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_MASTER                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 1, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_UP                     _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 2, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_DOWN                   _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 3, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_ADD                    _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 4, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_REMOVE                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 5, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_CYCLE                  _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 6, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_MTU                    _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 7, \
                                             sizeof(struct tdma_config))
#define TDMA_IOC_OFFSET                 _IOW(RTNET_IOC_TYPE_RTMAC_TDMA, 8, \
                                             sizeof(struct tdma_config))


#endif /* __TDMA_H_ */
