/***
 * rtnet/rtnet_internal.h - internal declarations
 *
 * Copyright (C) 1999      Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002      Ulrich Marx <marx@kammer.uni-hannover.de>
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
#ifndef __RTNET_INTERNAL_H__
#define __RTNET_INTERNAL_H__

/****************************************************************************************
 * iovec.c										*
 ****************************************************************************************/
extern int rt_iovec_len(struct iovec *iov,int iovlen);
extern void rt_memcpy_tokerneliovec(struct iovec *iov, unsigned char *kdata, int len);
extern int  rt_memcpy_fromkerneliovec(unsigned char *kdata,struct iovec *iov,int len);


#endif //__RTNET_INTERNAL_H__







