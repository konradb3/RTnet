/* rtnet_init.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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
#ifndef __RTNET_INIT_H_
#define __RTNET_INIT_H_

#ifdef __KERNEL__

#include <rtdev.h>


extern struct rtnet_device *rt_alloc_etherdev(int sizeof_priv);
extern int rt_register_rtnetdev(struct rtnet_device *rtdev);
extern int rt_unregister_rtnetdev(struct rtnet_device *rtdev);


#endif  /* __KERNEL__ */

#endif  /* __RTNET_INIT_H_ */
