/***
 * rtnet/rtnet_syms.c - export kernel symbols
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
#include <linux/kernel.h>
#include <linux/module.h>

#include <rtnet.h>
#include <rtskb.h>
#include <rtnet_socket.h>
#include <rtdev_mgr.h>
#include <stack_mgr.h>
#include <ethernet/eth.h>
#include <ipv4/arp.h>
#include <ipv4/ip_input.h>
#include <ipv4/route.h>
#include <ipv4/protocol.h>


/****************************************************************************
 * socket.c                                                                 *
 ****************************************************************************/
EXPORT_SYMBOL(rt_socket);
EXPORT_SYMBOL(rt_socket_accept);
EXPORT_SYMBOL(rt_socket_bind);
EXPORT_SYMBOL(rt_socket_close);
EXPORT_SYMBOL(rt_socket_connect);
EXPORT_SYMBOL(rt_socket_listen);
EXPORT_SYMBOL(rt_socket_send);
EXPORT_SYMBOL(rt_socket_recv);
EXPORT_SYMBOL(rt_socket_sendto);
EXPORT_SYMBOL(rt_socket_recvfrom);
EXPORT_SYMBOL(rt_socket_sendmsg);
EXPORT_SYMBOL(rt_socket_recvmsg);
/*EXPORT_SYMBOL(rt_socket_getsockname);*/
EXPORT_SYMBOL(rt_socket_setsockopt);
EXPORT_SYMBOL(rt_socket_ioctl);
EXPORT_SYMBOL(rt_socket_callback);

/* DISCONTINUED! WILL BE REMOVED SOON */
#if 0
EXPORT_SYMBOL(rt_ssocket);
EXPORT_SYMBOL(rt_ssocket_bind);
EXPORT_SYMBOL(rt_ssocket_listen);
EXPORT_SYMBOL(rt_ssocket_connect);
EXPORT_SYMBOL(rt_ssocket_accept);
EXPORT_SYMBOL(rt_ssocket_close);
EXPORT_SYMBOL(rt_ssocket_writev);
EXPORT_SYMBOL(rt_ssocket_send);
EXPORT_SYMBOL(rt_ssocket_sendto);
EXPORT_SYMBOL(rt_ssocket_sendmsg);
EXPORT_SYMBOL(rt_ssocket_readv);
EXPORT_SYMBOL(rt_ssocket_recv);
EXPORT_SYMBOL(rt_ssocket_recvfrom);
EXPORT_SYMBOL(rt_ssocket_recvmsg);
EXPORT_SYMBOL(rt_ssocket_getsockname);
EXPORT_SYMBOL(rt_ssocket_callback);
#endif

/****************************************************************************
 * stack_mgr.c                                                              *
 ****************************************************************************/
EXPORT_SYMBOL(rtdev_add_pack);
EXPORT_SYMBOL(rtdev_remove_pack);

EXPORT_SYMBOL(rtnetif_rx);
EXPORT_SYMBOL(rt_mark_stack_mgr);
EXPORT_SYMBOL(rtnetif_tx);

EXPORT_SYMBOL(rt_stack_connect);
EXPORT_SYMBOL(rt_stack_disconnect);

/****************************************************************************
 * rtdev_mgr.c                                                              *
 ****************************************************************************/
EXPORT_SYMBOL(rtnetif_err_rx);
EXPORT_SYMBOL(rtnetif_err_tx);

EXPORT_SYMBOL(rt_rtdev_connect);
EXPORT_SYMBOL(rt_rtdev_disconnect);

/****************************************************************************
 * rtdev.c                                                                  *
 ****************************************************************************/
EXPORT_SYMBOL(rt_alloc_etherdev);
EXPORT_SYMBOL(rtdev_free);

EXPORT_SYMBOL(rtdev_alloc_name);

EXPORT_SYMBOL(rt_register_rtnetdev);
EXPORT_SYMBOL(rt_unregister_rtnetdev);

EXPORT_SYMBOL(rtdev_get_by_name);
EXPORT_SYMBOL(rtdev_get_by_index);
EXPORT_SYMBOL(rtdev_get_by_hwaddr);

EXPORT_SYMBOL(rtdev_xmit);
EXPORT_SYMBOL(rtdev_xmit_proxy);


/****************************************************************************
 * rtnet_module.c                                                             *
 ****************************************************************************/
EXPORT_SYMBOL(STACK_manager);
EXPORT_SYMBOL(RTDEV_manager);


/****************************************************************************
 * ethernet/eth.c                                                           *
 ****************************************************************************/
EXPORT_SYMBOL(rt_eth_header);
EXPORT_SYMBOL(rt_eth_type_trans);


/****************************************************************************
 * ipv4                                                                     *
 ****************************************************************************/
EXPORT_SYMBOL(rt_ip_route_add_if_new);
EXPORT_SYMBOL(rt_ip_route_output);
EXPORT_SYMBOL(rt_ip_register_fallback);

EXPORT_SYMBOL(rt_inet_aton);


/****************************************************************************
 * ipv4/arp.c                                                               *
 ****************************************************************************/
EXPORT_SYMBOL(rt_arp_table_lookup);
EXPORT_SYMBOL(rt_rarp_table_lookup);
EXPORT_SYMBOL(rt_arp_solicit);
EXPORT_SYMBOL(rt_arp_table_add);


/****************************************************************************
 * rtskb.c                                                                  *
 ****************************************************************************/
EXPORT_SYMBOL(rtskb_copy_and_csum_bits);
EXPORT_SYMBOL(rtskb_copy_and_csum_dev);

EXPORT_SYMBOL(rtskb_over_panic);
EXPORT_SYMBOL(rtskb_under_panic);

EXPORT_SYMBOL(alloc_rtskb);
EXPORT_SYMBOL(kfree_rtskb);

EXPORT_SYMBOL(rtskb_pool_init);
EXPORT_SYMBOL(rtskb_pool_release);
EXPORT_SYMBOL(global_pool);

EXPORT_SYMBOL(rtskb_acquire);


/****************************************************************************
 * packet                                                                   *
 ****************************************************************************/
EXPORT_SYMBOL(rt_eth_aton);
