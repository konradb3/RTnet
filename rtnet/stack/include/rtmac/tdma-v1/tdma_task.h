/* include/rtmac/tdma/tdma_task.h
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

#ifndef __TDMA_TASK_H_
#define __TDMA_TASK_H_

#ifdef __KERNEL__

#include <rtmac/tdma-v1/tdma.h>


extern void tdma_task_shutdown(struct rtmac_tdma *tdma);
extern int tdma_task_change(struct rtmac_tdma *tdma, void (*task) (int rtdev_id), unsigned int cycle);
extern int tdma_task_change_con(struct rtmac_tdma *tdma, void (*task)(int rtdev_id), unsigned int cycle);

extern void tdma_task_notify(int rtdev_id);
extern void tdma_task_config(int rtdev_id);
extern void tdma_task_master(int rtdev_id);
extern void tdma_task_client(int rtdev_id);


#endif //__KERNEL__

#endif //__TDMA_TASK_H_
