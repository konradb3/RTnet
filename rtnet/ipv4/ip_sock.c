/* ip_sock.c
 *
 * Copyright (C) 2003 Hans-Peter Bock <hpbock@avaapgh.de>
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

// $Log: ip_sock.c,v $
// Revision 1.5  2003/08/20 16:41:53  kiszka
// * rt_ip_sockopt is now called from generic setsockopt function
//
// Revision 1.4  2003/05/27 09:50:41  kiszka
// * applied new header file structure
//
// Revision 1.3  2003/05/21 07:00:23  hpbock
// Corrected my email address.
//
// Revision 1.2  2003/02/05 08:40:08  hpbock
// This file has been created by me (Hans-Peter Bock) - but I copied the Header from another file, so Ulrich Marx's name was still in it.
//

#include <linux/errno.h>
#include <linux/socket.h>
#include <linux/in.h>

#include <rtnet.h>
#include <rtnet_socket.h>


int rt_ip_setsockopt(struct rtsocket *s, int level, int optname,
                     const void *optval, int optlen)
{
    int err = 0;

    if (optlen < sizeof(unsigned int))
        return -EINVAL;

    switch (optname) {
        case IP_TOS:
            s->tos = *(unsigned int *)optval;
            break;
        default:
            err = -ENOPROTOOPT;
            break;
    }

    return err;
}
