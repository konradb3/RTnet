/* include/rtmac_tdma.h
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

#ifndef __TDMA_H_
#define __TDMA_H_

#ifdef __KERNEL__

#include <rtai.h>
#include <rtai_sched.h>

#include <rtdev.h>
#include <rtmac/tdma/tdma.h>


static inline struct rtmac_tdma *tdma_get_by_name(const char *name)
{
	struct rtnet_device *rtdev = rtdev = rtdev_get_by_name(name);
	if (rtdev && rtdev->rtmac && rtdev->rtmac->priv)
		return (struct rtmac_tdma *) rtdev->rtmac->priv;
	return NULL;
}

static inline int tdma_wait_sof(struct rtmac_tdma *tdma)
{
	return (rt_sem_wait(&tdma->client_tx) != 0xFFFF) ? 0 : -1;
}

static inline RTIME tdma_get_delta_t(struct rtmac_tdma *tdma)
{
	return tdma->delta_t;
}


#endif /* __KERNEL__ */

#endif /* __TDMA_H_ */
