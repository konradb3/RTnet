/***
 *
 *  ipv4/af_inet.c
 *
 *  rtnet - real-time networking subsystem
 *  Copyright (C) 1999,2000 Zentropic Computing, LLC
 *                2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 *                2004 Jan Kiszka <jan.kiszka@web.de>
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

#include <rtnet_internal.h>
#include <ipv4/arp.h>
#include <ipv4/icmp.h>
#include <ipv4/ip_output.h>
#include <ipv4/protocol.h>
#include <ipv4/route.h>
#include <ipv4/udp.h>


#ifdef CONFIG_PROC_FS
struct proc_dir_entry *ipv4_proc_root;
#endif


/***
 *	rt_inet_proto_init
 */
int rt_inet_proto_init(void)
{
    int i;
    int result;


#ifdef CONFIG_PROC_FS
    ipv4_proc_root = create_proc_entry("ipv4", S_IFDIR, rtnet_proc_root);
    if (!ipv4_proc_root) {
        /*ERRMSG*/printk("RTnet: unable to initialize /proc entry (ipv4)\n");
        return -1;
    }
#endif /* CONFIG_PROC_FS */

    /* Network-Layer */
    if ((result = rt_ip_routing_init()) < 0)
        goto err1;
    rt_ip_init();
    rt_arp_init();

    /* Transport-Layer */
    for (i=0; i<MAX_RT_INET_PROTOCOLS; i++)
        rt_inet_protocols[i]=NULL;

    rt_icmp_init();
    rt_udp_init();

    return 0;

  err1:
#ifdef CONFIG_PROC_FS
    remove_proc_entry("ipv4", rtnet_proc_root);
#endif /* CONFIG_PROC_FS */
    return result;
}



/***
 *  rt_inet_proto_release
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

#ifdef CONFIG_PROC_FS
    remove_proc_entry("ipv4", rtnet_proc_root);
#endif
}
