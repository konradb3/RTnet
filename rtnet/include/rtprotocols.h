/***
 * rtnet/rtprotocols.h - implementation of rt-protocols
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
#ifndef __RTnet_INTERNAL_H__
#define __RTnet_INTERNAL_H__


/***
 * network-layer 
 * 
 */
#define MAX_NETWORK_PROTOCOLS 	16
//struct rtnetwork_protocol;
typedef struct rtnetwork_protocol NETWORK_PROTOCOL;
struct rtnetwork_protocol {
	char			*name;
	unsigned short		type;
	struct net_device 	*dev;

	int			(*handler) 
				(struct rtskb *, struct rtnet_device *, NETWORK_PROTOCOL *);
	int			(*err_handler)
				(struct rtskb *, struct rtnet_device *, NETWORK_PROTOCOL *);

	void			*private;
};


/***
 * transport-layer 
 *
 */
#define MAX_TRANSPORT_PROTOCOLS	32
struct rttransport_protocol {
	char			*name;
	unsigned short		protocol;
	
	int			(*handler)	(struct rtskb *);
	void			(*err_handler)	(struct rtskb *);
	int			(*socket)	(SOCKET *sock);

	void			*private;
};
typedef struct rttransport_protocol TRANSPORT_PROTOCOL;


/****************************************************************************************
 * ipv4/protocol.c									*
 ****************************************************************************************/
extern TRANSPORT_PROTOCOL *rt_tps[];

#define rt_tp_hashkey(id)  (id & (MAX_TRANSPORT_PROTOCOLS-1))
extern void rt_add_tp(TRANSPORT_PROTOCOL *prot);
extern void rt_del_tp(TRANSPORT_PROTOCOL *prot);
extern TRANSPORT_PROTOCOL *rt_get_tp(int protocol);

/****************************************************************************************
 * ipv6/protocol.c									*
 ****************************************************************************************/

#endif __RTnet_INTERNAL_H__
