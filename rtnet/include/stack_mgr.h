/* stack_mgr.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 2002, Ulrich Marx <marx@fet.uni-hannover.de>
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
#ifndef __STACK_MGR_H_
#define __STACK_MGR_H_

#ifdef __KERNEL__

#include <rtnet_internal.h>


extern void rt_stack_connect (struct rtnet_device *rtdev, struct rtnet_mgr *mgr);
extern void rt_stack_disconnect (struct rtnet_device *rtdev);
extern int rt_stack_mgr_init (struct rtnet_mgr *mgr);
extern void rt_stack_mgr_delete (struct rtnet_mgr *mgr);
extern int rt_stack_mgr_start (struct rtnet_mgr *mgr);
extern int rt_stack_mgr_stop (struct rtnet_mgr *mgr);

extern void rtnetif_rx(struct rtskb *skb);
extern void rtnetif_tx(struct rtnet_device *rtdev);
extern void rtnetif_err_tx(struct rtnet_device *rtdev);
extern void rtnetif_err_rx(struct rtnet_device *rtdev);
extern void rt_mark_stack_mgr(struct rtnet_device *rtdev);


#endif /* __KERNEL__ */

#endif  /* __STACK_MGR_H_ */
