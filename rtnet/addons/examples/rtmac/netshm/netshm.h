/***
 *
 *  netshm.h
 *
 *  netshm - simple device providing a distributed pseudo shared memory
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

#ifndef __NETSHM_H_
#define __NETSHM_H_


#include <rtdm.h>


struct netshm_attach_args {
    void*   mem_start;
    size_t  mem_size;
    size_t  local_mem_offs;
    size_t  local_mem_size;
    int     recv_task_prio;
    int     xmit_prio;
};


#define RTIOC_TYPE_DSM          RTDM_CLASS_EXPERIMENTAL

#define NETSHM_RTIOC_ATTACH     _IOW(RTIOC_TYPE_DSM, 0x00, \
                                     struct netshm_attach_args)
#define NETSHM_RTIOC_CYCLE      _IO(RTIOC_TYPE_DSM, 0x01)

#endif  /* __NETSHM_H_ */
