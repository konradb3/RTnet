/* tdma_ioctl.c
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

#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/netdevice.h>

#include <tdma_chrdev.h>
#include <rtmac/tdma-v1/tdma_event.h>
#include <rtmac/tdma-v1/tdma_module.h>


static inline int tdma_ioctl_client(struct rtnet_device *rtdev)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;
    int                 ret;


    if (rtdev->mac_priv == NULL) {
        ret = rtmac_disc_attach(rtdev, &tdma_disc);
        if (ret < 0)
            return ret;
    }

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    info.rtdev = rtdev;

    return tdma_do_event(tdma, REQUEST_CLIENT, &info);
}



static inline int tdma_ioctl_master(struct rtnet_device *rtdev, unsigned int cycle, unsigned int mtu)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;
    int                 ret;


    if (rtdev->mac_priv == NULL) {
        ret = rtmac_disc_attach(rtdev, &tdma_disc);
        if (ret < 0)
            return ret;
    }

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    if ((cycle < 100) || (cycle > 4*1000*1000)) {
        printk("RTmac: tdma: cycle must be between 100 us and 4 s\n");
        return -EINVAL;
    }
    if ((mtu < ETH_ZLEN - ETH_HLEN ) || (mtu > ETH_DATA_LEN)) {		// (mtu< 46 )||(mtu> 1500 )
        printk("RTmac: tdma: mtu %d is out of bounds, must be between 46 and 1500 octets\n", mtu);
        return -EINVAL;
    }

    info.rtdev = rtdev;
    info.cycle = cycle;
    info.mtu = mtu;

    return tdma_do_event(tdma, REQUEST_MASTER, &info);
}



static inline int tdma_ioctl_up(struct rtnet_device *rtdev)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    info.rtdev = rtdev;

    return tdma_do_event(tdma, REQUEST_UP, &info);
}



static inline int tdma_ioctl_down(struct rtnet_device *rtdev)
{
    struct rtmac_tdma   *tdma;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    return rtmac_disc_detach(rtdev);
}



static inline int tdma_ioctl_add(struct rtnet_device *rtdev, u32 ip_addr, unsigned int offset)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    info.rtdev = rtdev;
    info.ip_addr = ip_addr;
    info.offset = offset;

    return tdma_do_event(tdma, REQUEST_ADD_RT, &info);
}



static inline int tdma_ioctl_remove(struct rtnet_device *rtdev, u32 ip_addr)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    info.rtdev = rtdev;
    info.ip_addr = ip_addr;

    return tdma_do_event(tdma, REQUEST_REMOVE_RT, &info);
}



static inline int tdma_ioctl_cycle(struct rtnet_device *rtdev, unsigned int cycle)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    if ((cycle < 1000) || (cycle > 4*1000*1000)) {
        printk("RTmac: tdma: cycle must be between 1000 us and 4 s\n");
        return -1;
    }

    info.cycle = cycle;

    return tdma_do_event(tdma, CHANGE_CYCLE, &info);
}



static inline int tdma_ioctl_mtu(struct rtnet_device *rtdev, unsigned int mtu)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    info.mtu = mtu;

    if ((mtu < ETH_ZLEN - ETH_HLEN ) || (mtu > ETH_DATA_LEN)) {		// (mtu< 46 )||(mtu> 1500 )
        printk("RTmac: tdma: mtu %d is out of bounds, must be between 46 and 1500 octets\n", mtu);
        return -1;
    }

    return tdma_do_event(tdma, CHANGE_MTU, &info);
}



static inline int tdma_ioctl_offset(struct rtnet_device *rtdev, u32 ip_addr, unsigned int offset)
{
    struct rtmac_tdma   *tdma;
    struct tdma_info    info;


    if (rtdev->mac_priv == NULL)
        return -ENOTTY;

    tdma = (struct rtmac_tdma *)rtdev->mac_priv->disc_priv;
    if (tdma->magic != TDMA_MAGIC)
        return -ENOTTY;

    memset(&info, 0, sizeof(struct tdma_info));

    info.offset = offset;
    info.ip_addr = ip_addr;

    return tdma_do_event(tdma, CHANGE_OFFSET, &info);
}



int tdma_ioctl(struct rtnet_device *rtdev, unsigned int request, unsigned long arg)
{
    struct tdma_config  cfg;
    int                 ret;


    ret = copy_from_user(&cfg, (void *)arg, sizeof(cfg));
    if (ret != 0)
        return -EFAULT;

    down(&rtdev->nrt_sem);

    switch (request) {
        case TDMA_IOC_CLIENT:
            ret = tdma_ioctl_client(rtdev);
            break;

        case TDMA_IOC_MASTER:
            ret = tdma_ioctl_master(rtdev, cfg.cycle, cfg.mtu);
            break;

        case TDMA_IOC_UP:
            ret = tdma_ioctl_up(rtdev);
            break;

        case TDMA_IOC_DOWN:
            ret = tdma_ioctl_down(rtdev);
            break;

        case TDMA_IOC_ADD:
            ret = tdma_ioctl_add(rtdev, cfg.ip_addr, cfg.offset);
            break;

        case TDMA_IOC_REMOVE:
            ret = tdma_ioctl_remove(rtdev, cfg.ip_addr);
            break;

        case TDMA_IOC_CYCLE:
            ret = tdma_ioctl_cycle(rtdev, cfg.cycle);
            break;

        case TDMA_IOC_MTU:
            ret = tdma_ioctl_mtu(rtdev, cfg.mtu);
            break;

        case TDMA_IOC_OFFSET:
            ret = tdma_ioctl_offset(rtdev, cfg.ip_addr, cfg.offset);
            break;

        default:
            ret = -ENOTTY;
    }

    up(&rtdev->nrt_sem);

    return ret;
}
