/***
 *
 *  rtmac/tdma/tdma_dev.c
 *
 *  rtmac - real-time networking media access control subsystem
 *  Copyright (C) 2004 Jan Kiszka <Jan.Kiszka@web.de>
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

#include <linux/list.h>

#include <rtmac.h>
#include <rtmac/tdma/tdma.h>


static int tdma_dev_openclose(void)
{
    return 0;
}



static int tdma_dev_ioctl(struct rtdm_dev_context *context, int call_flags,
                          int request, void *arg)
{
    struct rtmac_tdma   *tdma;
    nanosecs_t          delta_t;
    unsigned long       flags;


    tdma = (struct rtmac_tdma *)((char *)context->device -
        (char *)&((struct rtmac_tdma *)0)->api_device);

    switch (request) {
        case RTMAC_RTIOC_TIMEOFFSET:
            rtos_spin_lock_irqsave(&tdma->delta_t_lock, flags);
            delta_t = tdma->delta_t;
            rtos_spin_unlock_irqrestore(&tdma->delta_t_lock, flags);

            *(__s64 *)arg = delta_t;
            return 0;

        case RTMAC_RTIOC_WAITONCYCLE:
            if (call_flags & RTDM_NRT_CALL)
                return -EACCES;

            if ((*(int *)arg != RTMAC_WAIT_ON_DEFAULT) &&
                (*(int *)arg != TDMA_WAIT_ON_SOF))
                return -EINVAL;

            if (RTOS_EVENT_ERROR(rtos_event_wait(&tdma->client_tx)))
                return -ENODEV;
            return 0;

        default:
            return -ENOTTY;
    }
}



int tdma_dev_init(struct rtnet_device *rtdev, struct rtmac_tdma *tdma)
{
    char    *pos;


    tdma->api_device.struct_version = RTDM_DEVICE_STRUCT_VER;

    tdma->api_device.device_flags = RTDM_NAMED_DEVICE;
    tdma->api_device.context_size = 0;

    strcpy(tdma->api_device.device_name, "TDMA");
    for (pos = rtdev->name + strlen(rtdev->name) - 1;
        (pos >= rtdev->name) && ((*pos) >= '0') && (*pos <= '9'); pos--);
    strncat(tdma->api_device.device_name+4, pos+1, IFNAMSIZ-4);

    tdma->api_device.open_rt  =
        (int (*)(struct rtdm_dev_context *, int, int))tdma_dev_openclose;
    tdma->api_device.open_nrt =
        (int (*)(struct rtdm_dev_context *, int, int))tdma_dev_openclose;

    tdma->api_device.ops.close_rt =
        (int (*)(struct rtdm_dev_context *, int))tdma_dev_openclose;
    tdma->api_device.ops.close_nrt =
        (int (*)(struct rtdm_dev_context *, int))tdma_dev_openclose;

    tdma->api_device.ops.ioctl_rt = tdma_dev_ioctl;
    tdma->api_device.ops.ioctl_nrt = tdma_dev_ioctl;

    tdma->api_device.proc_name = tdma->api_device.device_name;

    tdma->api_device.device_class     = RTDM_CLASS_RTMAC;
    tdma->api_device.device_sub_class = RTDM_SUBCLASS_TDMA;
    tdma->api_device.driver_name      = "RTmac/TDMA (RTnet " RTNET_VERSION ")";
    tdma->api_device.peripheral_name  = "TDMA API";
    tdma->api_device.provider_name    =
        "(C) 2002-2004 RTnet Development Team, http://rtnet.sf.net";

    return rtdm_dev_register(&tdma->api_device);
}



int tdma_dev_release(struct rtmac_tdma *tdma)
{
    return rtdm_dev_unregister(&tdma->api_device);
}
