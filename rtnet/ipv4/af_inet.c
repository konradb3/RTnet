/* af_inet.c
 *
 * rtnet - real-time networking subsystem
 * Copyright (C) 1999,2000 Zentropic Computing, LLC
 *               2002 Ulrich Marx <marx@kammer.uni-hannover.de>
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

#include <ipv4/arp.h>
#include <ipv4/icmp.h>
#include <ipv4/ip_output.h>
#include <ipv4/protocol.h>
#include <ipv4/route.h>
#include <ipv4/udp.h>


/***
 *	rt_inet_proto_init
 */
void rt_inet_proto_init(void)
{
	int i;

	/* Network-Layer */
	rt_ip_routing_init();
	rt_ip_init();
	rt_arp_init();
	
	/* Transport-Layer */
	for (i=0; i<MAX_RT_INET_PROTOCOLS; i++) 
		rt_inet_protocols[i]=NULL;

	rt_icmp_init();
	rt_udp_init();
}



/***
 *	rt_inet_proto_release
 */
void rt_inet_proto_release(void)
{
	/* Transport-Layer */
	rt_udp_release();
	rt_icmp_release();

	/* Network-Layer */
	rt_arp_release();
	rt_ip_release();
	rt_ip_routing_release();
}
