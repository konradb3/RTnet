/* tdma_ioctl.c
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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

#include <linux/delay.h>
#include <linux/netdevice.h>

#include <rtai.h>

#include <rtnet.h>
#include <rtmac.h>
#include <tdma.h>
#include <tdma_event.h>



int tdma_ioctl_client(struct rtnet_device *rtdev)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;
       
	memset(&info, 0, sizeof(struct tdma_info));
	
	info.rtdev = rtdev;

	return tdma_do_event(tdma, REQUEST_CLIENT, &info);
}


int tdma_ioctl_master(struct rtnet_device *rtdev, unsigned int cycle, unsigned int mtu)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;
       
	memset(&info, 0, sizeof(struct tdma_info));

	if ((cycle < 100) || (cycle > 4*1000*1000)) {
		rt_printk("RTmac: tdma: cycle must be between 100 us and 4 s\n"); 
		return -1;
	}
	if ((mtu < ETH_ZLEN - ETH_HLEN ) || (mtu > ETH_DATA_LEN)) {		// (mtu< 46 )||(mtu> 1500 )
		rt_printk("RTmac: tdma: mtu %d is out of bounds, must be between 46 and 1500 octets\n", mtu);
		return -1;
	}

	info.rtdev = rtdev;
	info.cycle = cycle;
	info.mtu = mtu;

	return tdma_do_event(tdma, REQUEST_MASTER, &info);
}


int tdma_ioctl_up(struct rtnet_device *rtdev)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;

	memset(&info, 0, sizeof(struct tdma_info));

	info.rtdev = rtdev;

	return tdma_do_event(tdma, REQUEST_UP, &info);
}


int tdma_ioctl_down(struct rtnet_device *rtdev)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;

	memset(&info, 0, sizeof(struct tdma_info));

	info.rtdev = rtdev;

	return tdma_do_event(tdma, REQUEST_DOWN, &info);
}


int tdma_ioctl_add(struct rtnet_device *rtdev, u32 ip_addr)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;

	memset(&info, 0, sizeof(struct tdma_info));

	info.rtdev = rtdev;
	info.ip_addr = ip_addr;
	
	return tdma_do_event(tdma, REQUEST_ADD_RT, &info);
}

int tdma_ioctl_remove(struct rtnet_device *rtdev, u32 ip_addr)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;

	memset(&info, 0, sizeof(struct tdma_info));

	info.rtdev = rtdev;
	info.ip_addr = ip_addr;
	
	return tdma_do_event(tdma, REQUEST_REMOVE_RT, &info);
}


int tdma_ioctl_mtu(struct rtnet_device *rtdev, unsigned int mtu)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;

	memset(&info, 0, sizeof(struct tdma_info));

	info.mtu = mtu;

	if ((mtu < ETH_ZLEN - ETH_HLEN ) || (mtu > ETH_DATA_LEN)) {		// (mtu< 46 )||(mtu> 1500 )
		rt_printk("RTmac: tdma: mtu %d is out of bounds, must be between 46 and 1500 octets\n", mtu);
		return -1;
	}

	return tdma_do_event(tdma, CHANGE_MTU, &info);
}


int tdma_ioctl_cycle(struct rtnet_device *rtdev, unsigned int cycle)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;

	memset(&info, 0, sizeof(struct tdma_info));

	if ((cycle < 1000) || (cycle > 4*1000*1000)) {
		rt_printk("RTmac: tdma: cycle must be between 1000 us and 4 s\n"); 
		return -1;
	}

	info.cycle = cycle;

	return tdma_do_event(tdma, CHANGE_CYCLE, &info);
}


int tdma_ioctl_offset(struct rtnet_device *rtdev, u32 ip_addr, unsigned int offset)
{
	struct rtmac_tdma *tdma = (struct rtmac_tdma *)rtdev->rtmac->priv;
	struct tdma_info info;

	memset(&info, 0, sizeof(struct tdma_info));

	info.offset = offset;
	info.ip_addr = ip_addr;

	return tdma_do_event(tdma, CHANGE_OFFSET, &info);
}
