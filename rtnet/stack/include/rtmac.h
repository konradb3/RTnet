/***
 *
 *  include/rtmac.h
 *
 *  rtmac - real-time networking media access control subsystem
 *  Copyright (C) 2004-2006 Jan Kiszka <Jan.Kiszka@web.de>
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

#include <rtdm/rtdm.h>


/* sub-classes: RTDM_CLASS_RTMAC */
#define RTDM_SUBCLASS_TDMA          0
#define RTDM_SUBCLASS_UNMANAGED     1

#define RTIOC_TYPE_RTMAC            RTDM_CLASS_RTMAC


/* ** Common Cycle Event Types ** */
/* standard event, wake up once per cycle */
#define RTMAC_WAIT_ON_DEFAULT       0x00
/* wake up on media access of the station, may trigger multiple times per
   cycle */
#define RTMAC_WAIT_ON_XMIT          0x01

/* ** TDMA-specific Cycle Event Types ** */
/* tigger on on SYNC frame reception/transmission */
#define TDMA_WAIT_ON_SYNC           RTMAC_WAIT_ON_DEFAULT
#define TDMA_WAIT_ON_SOF            TDMA_WAIT_ON_SYNC /* legacy support */


/* RTMAC_RTIOC_WAITONCYCLE_EX control and status data */
struct rtmac_waitinfo {
    unsigned int    type; /* set to wait type before invoking the service */
    size_t          size; /* must be at least sizeof(struct rtmac_waitinfo) */
    unsigned long   cycle_no;
    uint64_t        cycle_start;
    int64_t         clock_offset;
};


/* RTmac Discipline IOCTLs */
#define RTMAC_RTIOC_TIMEOFFSET      _IOR(RTIOC_TYPE_RTMAC, 0x00, int64_t)
#define RTMAC_RTIOC_WAITONCYCLE     _IOW(RTIOC_TYPE_RTMAC, 0x01, unsigned int)
#define RTMAC_RTIOC_WAITONCYCLE_EX  _IOWR(RTIOC_TYPE_RTMAC, 0x02, \
                                          struct rtmac_waitinfo)

#endif /* __RTMAC_H_ */
