/***
 *
 *  include/rtmac.h
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

#ifndef __RTMAC_H_
#define __RTMAC_H_

#include <rtdm.h>


#ifndef RTIOC_TYPE_RTMAC
#define RTIOC_TYPE_RTMAC        RTDM_CLASS_RTMAC
#endif

/* RTmac Discipline IOCTLs */
#define RTMAC_RTIOC_TIMEOFFSET  _IOR(RTIOC_TYPE_RTMAC, 0x00, __s64)
#define RTMAC_RTIOC_WAITONCYCLE _IOW(RTIOC_TYPE_RTMAC, 0x01, int)

/* Common Cycle Types */
#define RTMAC_WAIT_ON_DEFAULT   0x00
#define RTMAC_WAIT_ON_XMIT      0x01

/* TDMA-specific Cycle Types */
#define TDMA_WAIT_ON_SOF        0x10

#endif /* __RTMAC_H_ */
