/***
 *
 *  include/rtnet_sys.h
 *
 *  RTnet - real-time networking subsystem
 *          RTOS abstraction layer
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

#ifndef __RTNET_SYS_H_
#define __RTNET_SYS_H_

#include <rtnet_config.h>

#include <linux/time.h>
#include <linux/types.h>


typedef __s64   nanosecs_t;     /* used for time calculations and I/O */


/* RTAI support */
#if defined(CONFIG_RTAI_24) || defined(CONFIG_RTAI_30)
#include <rtnet_sys_rtai.h>
#endif


#endif /* __RTNET_SYS_H_ */
