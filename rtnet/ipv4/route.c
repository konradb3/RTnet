/* route.c - routing tables
 *
 * Copyright (C) 1999    Lineo, Inc
 *               1999,2002 David A. Schleef <ds@schleef.org>
 *               2002, Ulrich Marx <marx@fet.uni-hannover.de>
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

/*
 * INET     An implementation of the TCP/IP protocol suite for the LINUX
 *          operating system.  INET is implemented using the  BSD Socket
 *          interface as the means of communication with the user level.
 *
 *          ROUTE - implementation of the IP router.
 *
 * Version: $Id: route.c,v 1.10 2003/11/18 14:32:33 kiszka Exp $
 *
 * Authors: Ross Biro, <bir7@leland.Stanford.Edu>
 *          Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *          Alan Cox, <gw4pts@gw4pts.ampr.org>
 *          Linus Torvalds, <Linus.Torvalds@helsinki.fi>
 *          Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>
 *
 * Fixes:
 *      Billa           :   added rt_ip_route_del_specific
 *
 *          This program is free software; you can redistribute it and/or
 *          modify it under the terms of the GNU General Public License
 *          as published by the Free Software Foundation; either version
 *          2 of the License, or (at your option) any later version.
 */


// Note:
// This file and arp.c need some revision. We should combine the specific
// routing table with the arp table (more precisely: drop the arp table!) and
// introduce lock protection. Some functions are unused, other should be
// combined. This code somehow smells...
// -JK-


#include <rtai.h>

#ifdef CONFIG_PROC_FS
#include <linux/stat.h>
#include <linux/proc_fs.h>

#include <rtai_proc_fs.h>
#endif /* CONFIG_PROC_FS */

#include <linux/in.h>
#include <ipv4/arp.h>
#include <ipv4/route.h>


#define RT_ROUT_TABLE_LEN 50


struct rt_rtable *rt_rtable_free_list;
struct rt_rtable rt_rtable_cache[RT_ROUT_TABLE_LEN];

struct rt_rtable *rt_rtables;
struct rt_rtable *rt_rtables_generic;


/***
 *  proc filesystem section "/proc/rtai/route"
 */
#ifdef CONFIG_PROC_FS
static int rt_route_read_proc(char *page, char **start, off_t off, int count,
                              int *eof, void *data)
{
    PROC_PRINT_VARS;
    struct rt_rtable *rt_entry;

    PROC_PRINT("specific routing table\n")
    PROC_PRINT("src\t\tdst\t\tdst_mask\tdst_mac\t\t\tdev\n");
    for (rt_entry=rt_rtables; rt_entry!=NULL; rt_entry=rt_entry->next) {
        union { unsigned long l; unsigned char c[4]; } src, dst, dst_mask;
        unsigned char *hw_dst;
        char dev_name[IFNAMSIZ+1];
        strncpy(dev_name, rt_entry->rt_dev->name, IFNAMSIZ);
        dev_name[IFNAMSIZ] = '\0';

        src.l=rt_entry->rt_src;
        dst.l=rt_entry->rt_dst;
        dst_mask.l=rt_entry->rt_dst_mask;
        hw_dst = rt_entry->rt_dst_mac_addr;

        PROC_PRINT("%d.%d.%d.%d\t%d.%d.%d.%d\t%d.%d.%d.%d\t"
                   "%02x:%02x:%02x:%02x:%02x:%02x\t%s\n",
                   src.c[0], src.c[1], src.c[2], src.c[3],
                   dst.c[0], dst.c[1], dst.c[2], dst.c[3],
                   dst_mask.c[0], dst_mask.c[1], dst_mask.c[2], dst_mask.c[3],
                   hw_dst[0], hw_dst[1], hw_dst[2], hw_dst[3], hw_dst[4],
                   hw_dst[5],
                   dev_name);
    }

    PROC_PRINT("\ngeneric routing table\n")
    PROC_PRINT("src\t\tdst\t\tdst_mask\tdst_mac\n");
    for (rt_entry=rt_rtables_generic; rt_entry!=NULL; rt_entry=rt_entry->next) {
        union { unsigned long l; unsigned char c[4]; } src, dst, dst_mask;
        unsigned char *hw_dst;

        src.l=rt_entry->rt_src;
        dst.l=rt_entry->rt_dst;
        dst_mask.l=rt_entry->rt_dst_mask;
        hw_dst = rt_entry->rt_dst_mac_addr;

        PROC_PRINT("%d.%d.%d.%d\t%d.%d.%d.%d\t%d.%d.%d.%d\t"
                   "%02x:%02x:%02x:%02x:%02x:%02x\n",
                   src.c[0], src.c[1], src.c[2], src.c[3],
                   dst.c[0], dst.c[1], dst.c[2], dst.c[3],
                   dst_mask.c[0], dst_mask.c[1], dst_mask.c[2], dst_mask.c[3],
                   hw_dst[0], hw_dst[1], hw_dst[2], hw_dst[3], hw_dst[4],
                   hw_dst[5]);
    }
    PROC_PRINT_DONE;
}



static int rt_route_proc_register(void)
{
    static struct proc_dir_entry *proc_rt_arp;

    proc_rt_arp = create_proc_entry("route", S_IFREG | S_IRUGO | S_IWUSR,
                                    rtai_proc_root);
    if (!proc_rt_arp) {
        rt_printk("Unable to initialize: /proc/rtai/route\n");
        return -1;
    }
    proc_rt_arp->read_proc = rt_route_read_proc;

    return 0;
}

static void rt_route_proc_unregister(void)
{
    remove_proc_entry ("route", rtai_proc_root);
}
#endif  /* CONFIG_PROC_FS */




/***
 *  rt_alloc
 *
 */
static struct rt_rtable *rt_alloc(void)
{
    struct rt_rtable *rt;

    rt=rt_rtable_free_list;
    if ( rt ) {
        rt_rtable_free_list=rt->next;
        rt->use_count=1;
        rt->next=NULL;
        rt->prev=NULL;
    } else {
        rt_printk("RTnet: no more routes\n");
    }

    return rt;
}




/***
 *  rt_ip_route_add: add a new route
 *
 *  @rtdev      the outgoing rtnet_device
 *  @addr       IPv4-Address
 *  @mask       Subnet-Mask
 *
 */
struct rt_rtable *rt_ip_route_add(struct rtnet_device *rtdev,
                                  u32 addr, u32 mask)
{
    struct rt_rtable *rt;

    rt=rt_alloc();
    if (!rt)
        return NULL;

    rt->rt_dst=addr;
    rt->rt_dst_mask=mask;
    rt->rt_src=rtdev->local_addr;
    rt->rt_dev=rtdev;

    if( rt_rtables_generic != NULL)
        rt_rtables_generic->prev = rt;
    rt->next=rt_rtables_generic;
    rt_rtables_generic=rt;

    return rt;
}




/***
 *  ip_route_add_specific
 */
struct rt_rtable *rt_ip_route_add_specific(struct rtnet_device *rtdev,
                                           u32 addr, unsigned char *hw_addr)
{
    struct rt_rtable *rt;

    rt = rt_alloc();
    if (!rt)
        return NULL;

    rt->rt_dst=addr;
    rt->rt_dst_mask=0xffffffff; /* it's specific, safer */
    rt->rt_src=rtdev->local_addr;
    rt->rt_dev=rtdev;
    rt->rt_ifindex=rtdev->ifindex;

    memcpy(rt->rt_dst_mac_addr, hw_addr, RT_ARP_ADDR_LEN);

    if( rt_rtables != NULL)
        rt_rtables->prev = rt;

    rt->next=rt_rtables;
    rt_rtables=rt;

    return rt;
}



/***
 *  ip_route_add_if_new - adds new specific route if it does not exist yet
 */
int rt_ip_route_add_if_new(struct rtnet_device *rtdev, u32 daddr, u32 saddr,
                           unsigned char *hw_addr)
{
    struct rt_rtable *rt;

    for (rt=rt_rtables;rt!=NULL;rt=rt->next) {
        if ( (rt->rt_dst==daddr) && (rt->rt_src==saddr) &&
             (rt->rt_dev==rtdev) ){
            return -EEXIST;
        }
    }

    rt = rt_alloc();
    if (!rt)
        return -ENOMEM;

    rt->rt_dst=daddr;
    rt->rt_dst_mask=0xffffffff; /* it's specific, safer */
    rt->rt_src=rtdev->local_addr;
    rt->rt_dev=rtdev;
    rt->rt_ifindex=rtdev->ifindex;

    memcpy(rt->rt_dst_mac_addr, hw_addr, RT_ARP_ADDR_LEN);

    if( rt_rtables != NULL)
        rt_rtables->prev = rt;

    rt->next=rt_rtables;
    rt_rtables=rt;

    return 0;
}



/***
 *  rt_ip_route_del: delete route
 *
 */
void rt_ip_route_del(struct rtnet_device *rtdev)
{
    struct rt_rtable *rt = rt_rtables_generic;
    struct rt_rtable *next;

    /* remove entries from the generic routing table */
    while(rt != NULL) {
        next = rt->next;

        if( rt->rt_dev == rtdev && rt->rt_src == rtdev->local_addr ) {
            struct rt_rtable *prev = rt->prev;

            if( prev != NULL )
                prev->next = next;
            if( next != NULL )
                next->prev = prev;

            memset(rt, 0, sizeof(struct rt_rtable));

            /* add rt_rtable to free list */
            rt->prev = NULL;
            rt->next = rt_rtable_free_list;
            rt_rtable_free_list = rt;

            /* if we deleted the first elemet, set head to next */
            if(rt == rt_rtables_generic)
                rt_rtables_generic = next;
        }

        rt = next;
    }

    /* now the same for the specific one */
    rt = rt_rtables;
    while(rt != NULL) {
        next = rt->next;

        if( rt->rt_dev == rtdev && rt->rt_src == rtdev->local_addr ) {
            struct rt_rtable *prev = rt->prev;

            if( prev != NULL )
                prev->next = next;
            if( next != NULL )
                next->prev = prev;

            memset(rt, 0, sizeof(struct rt_rtable));

            /* add rt_rtable to free list */
            rt->prev = NULL;
            rt->next = rt_rtable_free_list;
            rt_rtable_free_list = rt;

            /* if we deleted the first elemet, set head to next */
            if(rt == rt_rtables)
                rt_rtables = next;
        }

        rt = next;
    }


}



/***
 *  rt_ip_route_del_specific: delete an specific route
 *
 */
void rt_ip_route_del_specific(struct rtnet_device *rtdev, u32 addr)
{
    struct rt_rtable *rt = rt_rtables_generic;
    struct rt_rtable *next;

    /* remove entries from the specific routing table */
    rt = rt_rtables;
    while(rt != NULL) {
        next = rt->next;

        if( rt->rt_dev == rtdev && rt->rt_dst == addr ) {
            struct rt_rtable *prev = rt->prev;

            if( prev != NULL )
                prev->next = next;
            if( next != NULL )
                next->prev = prev;

            memset(rt, 0, sizeof(struct rt_rtable));

            /* add rt_rtable to free list */
            rt->prev = NULL;
            rt->next = rt_rtable_free_list;
            rt_rtable_free_list = rt;

            /* if we deleted the first elemet, set head to next */
            if(rt == rt_rtables)
                rt_rtables = next;
        }
        rt = next;
    }
}



/***
 *  rt_ip_route_find: find a route to destination
 *
 *  @daddr      destination-address
 */
struct rt_rtable *rt_ip_route_find(u32 daddr)
{
    struct rt_rtable *rt,*new_rt;

    for (rt=rt_rtables; rt; rt=rt->next) {
        if ( rt->rt_dst==daddr )
            return rt;
    }

    for(rt=rt_rtables_generic;rt;rt=rt->next){
        if(rt->rt_dst==(daddr&rt->rt_dst_mask))
            goto found;
    }
    return NULL;

found:
    new_rt=rt_alloc();
    new_rt->rt_dev=rt->rt_dev;
    new_rt->rt_src=rt->rt_src;
    new_rt->rt_dst=daddr;
    new_rt->rt_dst_mask=0xffffffff;

    memset(new_rt->rt_dst_mac_addr, 0, 6);
    return rt;
}



/***
 *  rt_ip_route_input:  for every incoming packet
 */
int rt_ip_route_input(struct rtskb *skb, u32 daddr, u32 saddr,
                      struct rtnet_device *rtdev)
{
    struct rt_rtable *rt;

    for (rt=rt_rtables;rt!=NULL;rt=rt->next) {
        if ( (rt->rt_dst==saddr) && (rt->rt_src==daddr) &&
             (rt->rt_dev==rtdev) ){
            skb->dst=rt;
            return 0;
        }
    }

    for (rt=rt_rtables_generic; rt!=NULL; rt=rt->next){
        __u32 mask = rt->rt_dst_mask;
        /* Check the saddr range and also if daddr fits directly or
         * if it is a IP broadcast: */
        if ( (rt->rt_dst==(saddr & mask)) &&
             (rt->rt_dev==rtdev) ) {
            /* Add new host-to-host routes */
            if (rt->rt_src == daddr) {
                skb->dst=rt;
                goto route;
            }
            /* Do not add routes for incoming broadcasts! */
            if ( ( (rt->rt_src & mask) == (daddr & mask) ) &&
                 ( (daddr | mask) == 0xffffffff ) ) {
                skb->dst=rt;
                return 0;
            }
        }
    }
    skb->dst = NULL;

    return -EHOSTUNREACH;

route:
    rt=rt_alloc();
    if (!rt)
        return 0;

    rt->rt_dst=saddr;
    rt->rt_dst_mask=0xffffffff; /* it's specific, safer */
    rt->rt_src=daddr;
    rt->rt_dev=rtdev;
    rt->rt_ifindex=rtdev->ifindex;

    memcpy(rt->rt_dst_mac_addr,skb->mac.ethernet->h_source,RT_ARP_ADDR_LEN);

    if( rt_rtables != NULL)
        rt_rtables->prev = rt;

    rt->next=rt_rtables;
    rt_rtables=rt;

    skb->dst=rt;

    return 0;
}



/***
 *  rt_ip_dev_find
 *
 */
#if 0
static struct rtnet_device *rt_ip_dev_find(u32 saddr)
{
    if (!saddr)
        return rtnet_devices;
    else {
        struct rtnet_device *rtdev;
        unsigned long flags;

        flags = rt_spin_lock_irqsave(&rtnet_devices_lock);
        for (rtdev=rtnet_devices; rtdev!=NULL; rtdev=rtdev->next) {
            if (saddr==rtdev->local_addr) {
                rt_spin_unlock_irqrestore(flags, &rtnet_devices_lock);
                return rtdev;
            }
        }
        rt_spin_unlock_irqrestore(flags, &rtnet_devices_lock);
        rt_printk("RTnet: rt_ip_dev_find() returning NULL\n");
        return NULL;
    }
}
#endif



/***
 *  rt_ip_route_output: for every outgoing packet
 */
int rt_ip_route_output(struct rt_rtable **rp, u32 daddr, u32 saddr)
{
    struct rt_rtable *rt;

    for (rt=rt_rtables; rt; rt=rt->next) {
        if ( (rt->rt_dst==daddr) &&
             ( (saddr == INADDR_ANY) || (rt->rt_src==saddr) ) ) {
            *rp=rt;
            return 0;
        }
    }

    /* we will find the right dev, but we haven't got the destination MAC....
    for (rt=rt_rtables_generic; rt; rt=rt->next) {
        if ( (rt->rt_dst==(daddr & rt->rt_dst_mask)) &&
            (rt->rt_src==saddr) ) {
            *rp=rt;
            return 0;
        }
    }
    */

    rt_printk("RTnet: Host %u.%u.%u.%u unreachable (from %u.%u.%u.%u)\n",
              NIPQUAD(daddr), NIPQUAD(saddr));

    return -EHOSTUNREACH;
}



/***
 *  rt_ip_routing_init: initialize
 *
 */
void rt_ip_routing_init(void)
{
    int i;

    (rt_rtable_cache)->prev=NULL;
    (rt_rtable_cache)->next=rt_rtable_cache+1;
    (rt_rtable_cache+RT_ROUT_TABLE_LEN-1)->prev=
        (rt_rtable_cache+RT_ROUT_TABLE_LEN-2);
    (rt_rtable_cache+RT_ROUT_TABLE_LEN-1)->next=NULL;
    for(i=1; i<RT_ROUT_TABLE_LEN-1; i++){
        (rt_rtable_cache+i)->prev=rt_rtable_cache+i-1;
        (rt_rtable_cache+i)->next=rt_rtable_cache+i+1;
    }

    rt_rtable_free_list=rt_rtable_cache;
    rt_rtables=rt_rtables_generic=NULL;
#ifdef CONFIG_PROC_FS
    rt_route_proc_register();
#endif
}



/***
 *  rt_ip_routing_realease
 *
 */
void rt_ip_routing_release(void)
{
#ifdef CONFIG_PROC_FS
    rt_route_proc_unregister();
#endif
}
