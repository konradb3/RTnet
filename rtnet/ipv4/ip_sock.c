/* ip_sock.c
 *
 * Copyright (C) 2002 Hans-Peter Bock <Hans-Peter.Bock@epost.de>
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
// Revision 1.2  2003/02/05 08:40:08  hpbock
// This file has been created by me (Hans-Peter Bock) - but I copied the Header from another file, so Ulrich Marx's name was still in it.
//

#include <rtnet.h>

int rt_ip_setsockopt (int fd, int optname, char *optval, int optlen) {
       int err;
       SOCKET *sk = rt_socket_lookup(fd);

       if (!sk) {
               return -EINVAL;
       }

       err=0;
       switch (optname) {
               case IP_TOS:
                       sk->tos = *optval;
                       break;
       }
       return err;
}
