/* loopback.c
 *
 * Copyright (C) 2002 Ulrich Marx <marx@kammer.uni-hannover.de>
 * extended by Jose Carlos Billalabeitia and Jan Kiszka
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
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <linux/netdevice.h>

#include <rtnet_port.h>

/*#define DEBUG_LOOPBACK*/
/*#define DEBUG_LOOPBACK_DATA*/

MODULE_AUTHOR("Maintainer: Jan Kiszka <Jan.Kiszka@web.de>");
MODULE_DESCRIPTION("RTnet loopback driver");
MODULE_LICENSE("GPL");

static struct rtnet_device* rt_loopback_dev;

/***
 *  rt_loopback_open
 *  @rtdev
 */
static int rt_loopback_open (struct rtnet_device *rtdev)
{
    MOD_INC_USE_COUNT;

    rt_stack_connect(rtdev, &STACK_manager);
    rtnetif_start_queue(rtdev);

    return 0;
}


/***
 *  rt_loopback_close
 *  @rtdev
 */
static int rt_loopback_close (struct rtnet_device *rtdev)
{
    rtnetif_stop_queue(rtdev);
    rt_stack_disconnect(rtdev);

    MOD_DEC_USE_COUNT;

    return 0;
}


/***
 *  rt_loopback_xmit - begin packet transmission
 *  @skb: packet to be sent
 *  @dev: network device to which packet is sent
 *
 */
static int rt_loopback_xmit(struct rtskb *skb, struct rtnet_device *rtdev)
{
    /* make sure that critical fields are re-intialised */
    skb->chain_end = skb;

    /* parse the Ethernet header as usual */
    skb->protocol = rt_eth_type_trans(skb, rtdev);

#ifdef DEBUG_LOOPBACK
    {
        int i, cuantos;
        rtos_print("\n\nPACKET:");
        rtos_print("\nskb->protocol = %d", skb->protocol);
        rtos_print("\nskb->pkt_type = %d", skb->pkt_type);
        rtos_print("\nskb->csum = %d", skb->csum);
        rtos_print("\nskb->len = %d", skb->len);

        rtos_print("\n\nETHERNET HEADER:");
        rtos_print("\nMAC dest: "); for(i=0;i<6;i++){ rtos_print("0x%02X ", skb->buf_start[i+2]); }
        rtos_print("\nMAC orig: "); for(i=0;i<6;i++){ rtos_print("0x%02X ", skb->buf_start[i+8]); }
        rtos_print("\nPROTOCOL: "); for(i=0;i<2;i++){ rtos_print("0x%02X ", skb->buf_start[i+14]); }

        rtos_print("\n\nIP HEADER:");
        rtos_print("\nVERSIZE : "); for(i=0;i<1;i++){ rtos_print("0x%02X ", skb->buf_start[i+16]); }
        rtos_print("\nPRIORITY: "); for(i=0;i<1;i++){ rtos_print("0x%02X ", skb->buf_start[i+17]); }
        rtos_print("\nLENGTH  : "); for(i=0;i<2;i++){ rtos_print("0x%02X ", skb->buf_start[i+18]); }
        rtos_print("\nIDENT   : "); for(i=0;i<2;i++){ rtos_print("0x%02X ", skb->buf_start[i+20]); }
        rtos_print("\nFRAGMENT: "); for(i=0;i<2;i++){ rtos_print("0x%02X ", skb->buf_start[i+22]); }
        rtos_print("\nTTL     : "); for(i=0;i<1;i++){ rtos_print("0x%02X ", skb->buf_start[i+24]); }
        rtos_print("\nPROTOCOL: "); for(i=0;i<1;i++){ rtos_print("0x%02X ", skb->buf_start[i+25]); }
        rtos_print("\nCHECKSUM: "); for(i=0;i<2;i++){ rtos_print("0x%02X ", skb->buf_start[i+26]); }
        rtos_print("\nIP ORIGE: "); for(i=0;i<4;i++){ rtos_print("0x%02X ", skb->buf_start[i+28]); }
        rtos_print("\nIP DESTI: "); for(i=0;i<4;i++){ rtos_print("0x%02X ", skb->buf_start[i+32]); }

#ifdef DEBUG_LOOPBACK_DATA
        cuantos = (int)(*(unsigned short *)(skb->buf_start+18)) - 20;
        rtos_print("\n\nDATA (%d):", cuantos);
        rtos_print("\n:"); for(i=0;i<cuantos;i++){ rtos_print("0x%02X ", skb->buf_start[i+36]); }
#endif /* DEBUG_LOOPBACK_DATA */
    }
#endif /* DEBUG_LOOPBACK */

    rtnetif_rx(skb);
    rt_mark_stack_mgr(rtdev);

    return 0;
}


/***
 *  loopback_init
 */
static int __init loopback_init(void)
{
    int err;
    struct rtnet_device *rtdev;

    printk("initializing loopback...\n");

    if ((rtdev = rt_alloc_etherdev(0)) == NULL)
        return -ENODEV;

    rt_rtdev_connect(rtdev, &RTDEV_manager);
    SET_MODULE_OWNER(rtdev);

    strcpy(rtdev->name, "rtlo");

    rtdev->open = &rt_loopback_open;
    rtdev->stop = &rt_loopback_close;
    rtdev->hard_start_xmit = &rt_loopback_xmit;
    rtdev->flags |= IFF_LOOPBACK;

    if ((err = rt_register_rtnetdev(rtdev)) != 0)
    {
        rtdev_free(rtdev);
        return err;
    }

    rt_loopback_dev = rtdev;

    return 0;
}


/***
 *  loopback_cleanup
 */
static void __exit loopback_cleanup(void)
{
    struct rtnet_device *rtdev = rt_loopback_dev;

    printk("removing loopback...\n");

    rt_unregister_rtnetdev(rtdev);
    rt_rtdev_disconnect(rtdev);

    rtdev_free(rtdev);
}

module_init(loopback_init);
module_exit(loopback_cleanup);
