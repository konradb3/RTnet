/* rt2x00core.c
 *
 * Copyright (C) 2004 - 2005 rt2x00-2.0.0-b3 SourceForge Project
 *	                     <http://rt2x00.serialmonkey.com>
 *               2006        rtnet adaption by Daniel Gregorek 
 *                           <dxg@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Module: rt2x00core
 * Abstract: rt2x00 core routines.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <asm/io.h>

#include <rtnet_port.h>
#include <rtwlan_io.h>

#include "rt2x00.h"
#include "rt2x00dev.h"

#ifdef DRV_NAME
#undef DRV_NAME
#define DRV_NAME			"rt_rt2x00core"
#endif /* DRV_NAME */

static int rt2x00_radio_on(struct _rt2x00_device * device);
static int rt2x00_radio_off(struct _rt2x00_device * device);

static int cards[MAX_UNITS] = { [0 ... (MAX_UNITS-1)] = 1 };
MODULE_PARM(cards, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM_DESC(cards, "array of cards to be supported (e.g. 1,0,1)");

struct _rt2x00_device * rt2x00_device(struct rtnet_device * rtnet_dev) {

    return rtwlan_priv(rtnet_dev);
}
EXPORT_SYMBOL_GPL(rt2x00_device);


/*
 * Writes the pending configuration to the device
 */
static void rt2x00_update_config(struct _rt2x00_device *device) {

    struct _rt2x00_core	 * core = rt2x00_core(device);
    struct rtwlan_device * rtwlan = core->rtwlan;      
    u16			   update_flags = 0x0000;
    rtdm_lockctx_t context;

    if(!test_bit(DEVICE_ENABLED, &device->flags)
       && !test_bit(DEVICE_RADIO_ON, &device->flags))
        return;

    if(test_and_set_bit(DEVICE_CONFIG_UPDATE, &device->flags))
        return;

    rtdm_lock_get_irqsave(&rtwlan->lock, context);

    update_flags = core->config.update_flags;
    core->config.update_flags = 0;

    if(likely(update_flags))
        device->handler->dev_update_config(device, &core->config, update_flags);

    rtdm_lock_put_irqrestore(&rtwlan->lock, context);

    clear_bit(DEVICE_CONFIG_UPDATE, &device->flags);
}

/* 
 * Radio control.
 */
static int rt2x00_radio_on(struct _rt2x00_device * device) {

    int status = 0x00000000;

    if(test_bit(DEVICE_RADIO_ON, &device->flags)){
        WARNING("Radio already on.\n");
        return -ENOTCONN;
    }

    status = device->handler->dev_radio_on(device);
    if(status)
        return status;

    set_bit(DEVICE_RADIO_ON, &device->flags);

    return 0;

}

static int rt2x00_radio_off(struct _rt2x00_device * device) {

    if(!test_and_clear_bit(DEVICE_RADIO_ON, &device->flags)){
        WARNING("Radio already off.\n");
        return -ENOTCONN;
    }

    device->handler->dev_radio_off(device);

    return 0;
}

/*
 * user space io handler
 */
static int rt2x00_ioctl(struct rtnet_device * rtnet_dev, unsigned int request, void * arg) {

    struct _rt2x00_device * device = rt2x00_device(rtnet_dev);
    struct _rt2x00_core * core     = rt2x00_core(device);
    struct rtwlan_cmd * cmd;
    u8 rate, cck_rate, ofdm_rate;
    u32 address, value;
    cmd = (struct rtwlan_cmd *)arg;

    switch(request) {

    case IOC_RTWLAN_IFINFO:
        cmd->args.info.bitrate = core->config.bitrate;
        cmd->args.info.channel = core->config.channel;
        cmd->args.info.txpower = core->config.txpower;
        core->device->handler->dev_register_access(core->device, IOC_RTWLAN_BBPREAD, 0x11, &value);
        cmd->args.info.sensibility = value;
        cmd->args.info.mode    = core->rtwlan->mode;
        cmd->args.info.rx_packets = core->rtwlan->stats.rx_packets;
        cmd->args.info.tx_packets = core->rtwlan->stats.tx_packets;
        cmd->args.info.tx_retry   = core->rtwlan->stats.tx_retry;
        cmd->args.info.dropbcast = core->config.config_flags & CONFIG_DROP_BCAST ? 1 : 0;
        cmd->args.info.dropmcast =  core->config.config_flags & CONFIG_DROP_MCAST ? 1 : 0;
        break;
    case IOC_RTWLAN_BITRATE:
        rate = cmd->args.set.bitrate;
        ofdm_rate = ieee80211_is_ofdm_rate(rate);
        cck_rate = ieee80211_is_cck_rate(rate);
        DEBUG("bitrate=%d\n", rate);
        if(!(cck_rate ^ ofdm_rate))
            NOTICE("Rate %d is not CCK and not OFDM.\n", rate);
        core->config.bitrate = rate;
        core->config.update_flags |= UPDATE_BITRATE;
        break;
    case IOC_RTWLAN_CHANNEL:
        DEBUG("channel=%d\n", cmd->args.set.channel);
        core->config.channel = cmd->args.set.channel;
        core->config.update_flags |= UPDATE_CHANNEL;
        break;
    case IOC_RTWLAN_TXPOWER:
        core->config.txpower = cmd->args.set.txpower;
        core->config.update_flags |= UPDATE_TXPOWER;
        break;
    case IOC_RTWLAN_DROPBCAST:
        if(cmd->args.set.dropbcast) 
            core->config.config_flags |= CONFIG_DROP_BCAST;
        else 
            core->config.config_flags &= ~CONFIG_DROP_BCAST;
        core->config.update_flags |= UPDATE_PACKET_FILTER;
        break;
    case IOC_RTWLAN_DROPMCAST:
        if(cmd->args.set.dropmcast)
            core->config.config_flags |= CONFIG_DROP_MCAST;
        else
            core->config.config_flags &= ~CONFIG_DROP_MCAST;
        core->config.update_flags |= UPDATE_PACKET_FILTER;
        break;
    case IOC_RTWLAN_MODE:
        core->rtwlan->mode = cmd->args.set.mode;
        break;
    case IOC_RTWLAN_BBPSENS:
        value = cmd->args.set.bbpsens;
        if(value < 0)
            value = 0;
        if(value > 127)
            value = 127;
        core->device->handler->dev_register_access(core->device, IOC_RTWLAN_BBPWRITE, 0x11, &value);
        break;
    case IOC_RTWLAN_REGREAD:
    case IOC_RTWLAN_BBPREAD:
        address = cmd->args.reg.address;
        core->device->handler->dev_register_access(core->device, request, address, &value);
        cmd->args.reg.value = value;
        break;
    case IOC_RTWLAN_REGWRITE:
    case IOC_RTWLAN_BBPWRITE:
        address = cmd->args.reg.address;
        value = cmd->args.reg.value;
        core->device->handler->dev_register_access(core->device, request, address, &value) ;
        break;
    default:
        ERROR("Unknown request!\n");
        return -1;
    }

    if(request != IOC_RTWLAN_IFINFO)
        rt2x00_update_config(device);

    return 0;
}

/*
 * TX/RX related routines.
 */
static int rt2x00_start_xmit(struct rtskb *rtskb, struct rtnet_device *rtnet_dev) {

    struct _rt2x00_device	*device = rt2x00_device(rtnet_dev);
    struct _rt2x00_core		*core = rt2x00_core(device);
    u16				xmit_flags = 0x0000;
    u8				rate = 0x00;
    int retval=0;
    rtdm_lockctx_t context;

    if(!test_bit(DEVICE_RADIO_ON, &device->flags))
        return -ENOTCONN;

    rtdm_lock_get_irqsave(&core->rtwlan->lock, context);

    if (unlikely(rtskb && (core->rtwlan->mode != RTWLAN_MODE_MON))) {

        rtwlan_tx(rtskb, rtnet_dev);

        rate = core->config.bitrate;
        xmit_flags |= XMIT_START;
        xmit_flags |= XMIT_NEW_SEQUENCE;
        if(ieee80211_is_ofdm_rate(rate))
            xmit_flags |= XMIT_OFDM;

        /* Check if the packet should be acknowledged */
        if(core->rtwlan->mode == RTWLAN_MODE_ACK)
            xmit_flags |= XMIT_ACK;

        if(core->device->handler->dev_xmit_packet(core->device, rtskb, rate, xmit_flags)) {
            ERROR("Packet dropped !");
            retval = -1;
        }
    }

    rtdm_lock_put_irqrestore(&core->rtwlan->lock, context);
    dev_kfree_rtskb(rtskb);
  
    return retval;
}

/***
 *  rt2x00_open
 *  @rtdev
 */
static int rt2x00_open (struct rtnet_device *rtnet_dev) {

    struct _rt2x00_device *device = rt2x00_device(rtnet_dev);
    struct _rt2x00_core   * core  = rt2x00_core(device);
    int			  status = 0x00000000;

    DEBUG("Start.\n");

    if(test_and_set_bit(DEVICE_ENABLED, &device->flags)){
        ERROR("device already enabled.\n");
        return -EBUSY;
    }

    /*
     * Start rtnet interface.
     */
    rt_stack_connect(rtnet_dev, &STACK_manager);

    status = rt2x00_radio_on(device);
    if(status){
        clear_bit(DEVICE_ENABLED, &device->flags);
        ERROR("Couldn't activate radio.\n");
        return status;
    }

    rtnetif_start_queue(rtnet_dev);

    core->config.led_status = 1;
    core->config.update_flags |= UPDATE_LED_STATUS;

    RTNET_MOD_INC_USE_COUNT;

    rt2x00_update_config(device);
  
    DEBUG("Exit success.\n");

    return 0;
}


/***
 *  rt2x00_close
 *  @rtdev
 */
static int rt2x00_close (struct rtnet_device *rtnet_dev) {

    struct _rt2x00_device	*device = rt2x00_device(rtnet_dev);

    DEBUG("Start.\n");

    if(!test_and_clear_bit(DEVICE_ENABLED, &device->flags)){
        ERROR("device already disabled.\n");
        return -EBUSY;
    }

    rt2x00_radio_off(device);

    rtnetif_stop_queue(rtnet_dev);
    rt_stack_disconnect(rtnet_dev);

    RTNET_MOD_DEC_USE_COUNT;

    return 0;
}


/*
 * Initialization handlers.
 */
static void rt2x00_init_config(struct _rt2x00_core *core) {

    memset(&core->config.bssid, '\0', sizeof(core->config.bssid));

    core->config.channel = 1;
    core->config.bitrate = capabilities.bitrate[0];
    core->config.config_flags = 0;
    core->config.config_flags |= CONFIG_DROP_BCAST | CONFIG_DROP_MCAST;
    core->config.short_retry = 4;
    core->config.long_retry = 7;
    core->config.antenna_flags |= ANTENNA_TX_DIV | ANTENNA_RX_DIV;
    core->config.txpower = 100;
    core->config.plcp = 48;
    core->config.sifs = 10;
    core->config.slot_time = 20;
    core->rtwlan->mode = RTWLAN_MODE_ACK;
    core->config.update_flags = UPDATE_ALL_CONFIG;
}

struct rtnet_device * rt2x00_core_probe(struct _rt2x00_dev_handler * handler, 
					void * priv, 
					u32 sizeof_dev)
{
    struct rtnet_device	*rtnet_dev = NULL;
    struct _rt2x00_device	*device = NULL;
    struct _rt2x00_core	*core = NULL;
    static int cards_found = -1;
    int err;

    DEBUG("Start.\n");

    cards_found++;
    if (cards[cards_found] == 0)
        goto exit;

    rtnet_dev = rtwlan_alloc_dev(sizeof_dev + sizeof(*device) + sizeof(*core));
    if(!rtnet_dev)
        goto exit;

    rt_rtdev_connect(rtnet_dev, &RTDEV_manager);
    RTNET_SET_MODULE_OWNER(rtnet_dev);
    rtnet_dev->vers = RTDEV_VERS_2_0;


    device = rtwlan_priv(rtnet_dev);
    memset(device, 0x00, sizeof(*device));

    core = (void*)device + sizeof(*device);
    core->rtwlan = rtnetdev_priv(rtnet_dev);
    core->device = device;

    if (rtskb_pool_init(&core->rtwlan->skb_pool, RX_ENTRIES*2) < RX_ENTRIES*2) {
        rtskb_pool_release(&core->rtwlan->skb_pool);
        ERROR("rtskb_pool_init failed.\n");
        goto exit;
    }

    device->priv = (void*)core + sizeof(*core);
    device->owner = core;
    device->handler = handler;
    device->rtnet_dev = rtnet_dev;  

    /* Set configuration default values. */
    rt2x00_init_config(core);

    if(device->handler->dev_probe
       && device->handler->dev_probe(device, &core->config, priv)){
        ERROR("device probe failed.\n");
        goto exit;
    }
    /*  INFO("Device " MAC_FMT " detected.\n", MAC_ARG(device->net_dev->dev_addr)); */

    rtnet_dev->hard_start_xmit = rt2x00_start_xmit;
    rtnet_dev->open = &rt2x00_open;
    rtnet_dev->stop = &rt2x00_close;
    rtnet_dev->do_ioctl = &rt2x00_ioctl;
    rtnet_dev->hard_header = &rt_eth_header;

    if ((err = rt_register_rtnetdev(rtnet_dev)) != 0) {
        rtdev_free(rtnet_dev);
        ERROR("rtnet_device registration failed.\n");
        printk("err=%d\n", err);
        goto exit_dev_remove;
    }

    set_bit(DEVICE_AWAKE, &device->flags);

    /* Activate current configuration. */
    rt2x00_update_config(device);

    return rtnet_dev;

  exit_dev_remove:
    if(device->handler->dev_remove)
        device->handler->dev_remove(device);

  exit:
    return NULL; 
}
EXPORT_SYMBOL_GPL(rt2x00_core_probe);

void rt2x00_core_remove(struct rtnet_device * rtnet_dev) {

    struct rtwlan_device * rtwlan = rtnetdev_priv(rtnet_dev);

    rtskb_pool_release(&rtwlan->skb_pool);
    rt_unregister_rtnetdev(rtnet_dev);
    rt_rtdev_disconnect(rtnet_dev);

    rtdev_free(rtnet_dev);
  
}
EXPORT_SYMBOL_GPL(rt2x00_core_remove);

/*
 * RT2x00 core module information.
 */
static char version[] = DRV_NAME " - " DRV_VERSION;

int rt2x00_debug_level = 0x00000001;
EXPORT_SYMBOL_GPL(rt2x00_debug_level);

MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION("RTnet rt2500 PCI WLAN driver (Core Module)");
MODULE_LICENSE("GPL");

static int __init rt2x00_core_init(void) {
    printk(KERN_INFO "Loading module: %s\n", version);
    return 0; 
}

static void __exit rt2x00_core_exit(void) {
    printk(KERN_INFO "Unloading module: %s\n", version);
}

module_init(rt2x00_core_init);
module_exit(rt2x00_core_exit);
