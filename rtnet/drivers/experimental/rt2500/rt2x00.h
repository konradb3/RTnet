/* rt2x00.h
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
  Module: rt2x00
  Abstract: rt2x00 global information.
  Supported chipsets: RT2460, RT2560 & RT2570.
*/

/* This file is a rtnet adaptation from rt2x00.h */

#ifndef RT2X00_H
#define RT2X00_H

#include <linux/netdevice.h>
#include <linux/wireless.h>

#include <rtnet_port.h>
#include <rtwlan_io.h>
#include <rtwlan.h>

#define MAX_UNITS 2

/*
 * Module information.
 */
#define DRV_NAME			"rt2x00"
#define DRV_VERSION			"0.1"
#define DRV_AUTHOR			"Daniel Gregorek <dxg@gmx.de>"
#define CONFIG_RT2X00_DEBUG

/*
 * Debug defines.
 * The debug variable will be exported by the device specific module.
 * For this reason this variable must be set to extern to make it accessible
 * to the core module as well.
 */
#ifdef CONFIG_RT2X00_DEBUG
extern int rt2x00_debug_level;
#define DEBUG_PRINTK(__message...)	do{ if(rt2x00_debug_level) rtdm_printk(__message); }while(0)
#else /* CONFIG_RT2X00_DEBUG */
#define DEBUG_PRINTK(__message...)	do{  }while(0)
#endif /* CONFIG_RT2X00_DEBUG */

/*
 * Various debug levels.
 * PANIC and ERROR indicates serious problems within the module,
 * these should never be ignored and thus we will always print the message.
 */
#define PANIC(__message,__args...)	rtdm_printk(KERN_PANIC DRV_NAME "->%s: Panic - " __message,__FUNCTION__,##__args);
#define ERROR(__message,__args...)	rtdm_printk(KERN_ERR DRV_NAME "->%s: Error - " __message,__FUNCTION__,##__args);
#define WARNING(__message,__args...)	DEBUG_PRINTK(KERN_WARNING DRV_NAME "->%s: Warning - " __message,__FUNCTION__,##__args);
#define NOTICE(__message,__args...)	DEBUG_PRINTK(KERN_NOTICE DRV_NAME "->%s: Notice - " __message,__FUNCTION__,##__args);
#define INFO(__message,__args...)	DEBUG_PRINTK(KERN_INFO DRV_NAME "->%s: Info - " __message,__FUNCTION__,##__args);
#define DEBUG(__message,__args...)	DEBUG_PRINTK(KERN_DEBUG DRV_NAME "->%s: Debug - " __message,__FUNCTION__,##__args);

/*
 * RT2x00 ring types.
 */

/*
 * Ring names.
 */
#define RING_RX				0x01	/* Ring used for receiving packets. */
#define RING_TX				0x02	/* Ring used for transmitting normal packets. */

/*
 * Ring sizes.
 */
#define DATA_FRAME_SIZE			2432
#define MGMT_FRAME_SIZE			256

/*
 * RT2x00 xmit flags.
 */
#define XMIT_IFS_SIFS			0x0001
#define XMIT_IFS_BACKOFF		0x0002
#define XMIT_IFS_NEW_BACKOFF		0x0004
#define XMIT_IFS_NONE			0x0008
#define XMIT_NEW_SEQUENCE		0x0010
#define XMIT_ACK			0x0020
#define XMIT_TIMESTAMP			0x0040
#define XMIT_RTS			0x0080
#define XMIT_OFDM			0x0100
#define XMIT_LONG_RETRY			0x0200
#define XMIT_MORE_FRAGS			0x0400
#define XMIT_SHORT_PREAMBLE		0x0800
#define XMIT_START			0x1000

/*
 * RT2x00 Statistics flags.
 */
#define STATS_TX_RESULT			0x01
#define STATS_TX_RETRY_COUNT		0x02
#define STATS_RX_CRC			0x10
#define STATS_RX_PHYSICAL		0x20
#define STATS_RX_QUALITY		0x40
#define STATS_RX_DROP			0x80

/*
 * TX result flags.
 */
#define TX_SUCCESS			0
#define TX_SUCCESS_RETRY		1
#define TX_FAIL_RETRY			2
#define TX_FAIL_INVALID			3
#define TX_FAIL_OTHER			4

/*
 * Channel type defines.
 */
#define CHANNEL_OFDM			0x01
#define CHANNEL_UNII_LOW		0x02
#define CHANNEL_HIPERLAN2		0x04
#define CHANNEL_UNII_HIGH		0x08

#define CHANNEL_OFDM_MIN		1
#define CHANNEL_OFDM_MAX		14
#define CHANNEL_UNII_LOW_MIN		36
#define CHANNEL_UNII_LOW_MAX		64
#define CHANNEL_HIPERLAN2_MIN		100
#define CHANNEL_HIPERLAN2_MAX		140
#define CHANNEL_UNII_HIGH_MIN		149
#define CHANNEL_UNII_HIGH_MAX		161

/*
 * Device 802.11abg capabilities.
 */
static struct _rt2x00_capabilities{
    u8				txpower[6];
    u8				bitrate[12];
} __attribute__ ((packed)) capabilities = {
    /*
     * tx-power.
     */
    .txpower = {
          3, 12, 25, 50, 75, 100,
      },

    /*
     * Bitrates
     */
    .bitrate = {
         2, 4, 11, 22,						/* CCK. */
         12, 18, 24, 36, 48, 72, 96, 108,			/* OFDM. */
     },
};

struct _rt2x00_config{

    u32				config_flags;
#define CONFIG_DROP_BCAST           0x0001
#define CONFIG_DROP_MCAST           0x0002

    u8				antenna_flags;
#define ANTENNA_TX			0x0f
#define ANTENNA_TX_A			0x01
#define ANTENNA_TX_B			0x02
#define ANTENNA_TX_DIV			0x03
#define ANTENNA_RX			0xf0
#define ANTENNA_RX_A			0x10
#define ANTENNA_RX_B			0x20
#define ANTENNA_RX_DIV			0x30

    u8				bssid[ETH_ALEN];
    u8				short_retry;
    u8				long_retry;

    u8                          channel;
    u8                          bitrate;	/* 0.5Mbit/sec */
    u8				txpower;	/* % */

    u16				sensitivity;	/* RSSI */
    u16				rts_threshold;

    /*
     * LED status
     */
    u8				led_status;

    u16				__pad2;		/* For alignment only. */

    /*
     * Duration values in us.
     */
    u8				beacon;
    u8				plcp;
    u8				sifs;
    u8				slot_time;

    /*
     * The sequence number for outgoing packets is stored here.
     * May only be changed by SEQUENCE_INC().
     */
    u16				sequence;

    /*
     * Configuration values that have to be updated to device.
     */
    u16				update_flags;
#define UPDATE_ALL_CONFIG		0xffff
#define UPDATE_BSSID			0x0001
#define UPDATE_PACKET_FILTER		0x0002
#define UPDATE_CHANNEL			0x0004
#define UPDATE_BITRATE			0x0008
#define UPDATE_RETRY			0x0010
#define UPDATE_TXPOWER			0x0020
#define UPDATE_ANTENNA			0x0040
#define UPDATE_DURATION			0x0080
#define UPDATE_PREAMBLE			0x0100
#define UPDATE_SENSITIVITY		0x0200
#define UPDATE_LED_STATUS		0x0400

}  __attribute__ ((packed));

/*
 * The _rt2x00_device acts as layer of communication
 * between device specific module and core module.
 */
struct _rt2x00_device {

    /*
     * RT2x00 device status flags (atomic read/write access).
     */
    unsigned long			flags;
#define DEVICE_ENABLED			0		/* Device has been opened. */
#define DEVICE_AWAKE			1		/* Device is not suspended. */
#define DEVICE_RADIO_ON			2		/* Device antenna is enabled. */
#define DEVICE_SCANNING			3		/* Device is currently scanning. */
#define DEVICE_CONFIG_UPDATE		4		/* Device is updating configuration. */
#define DEVICE_RESTORE_LINK		5		/* Device should be re-opened after resume. */
#define DEVICE_CAP_802_11A		6		/* Device has 802.11a capabilities. */
#define DEVICE_CAP_802_11B		7		/* Device has 802.11b capabilities. */
#define DEVICE_CAP_802_11G		8		/* Device has 802.11g capabilities. */

    /*
     * Device handler.
     */
    struct _rt2x00_dev_handler	*handler;

    /*
     * Pointer to core structure of the main module.
     * structure address may only be acquired by rt2x00_core function.
     */
    void				*owner;

    /*
     * Pointer to device specific structure.
     * structure address may only be acquired by rt2x00_dev function.
     */
    void				*priv;

    /*
     * Net device we belong to.
     */
    struct rtnet_device		*rtnet_dev;
}  __attribute__ ((packed));


struct _rt2x00_core {

    struct _rt2x00_device *device;

    /*
     * RTWLAN stack structure.
     */
    struct rtwlan_device	*rtwlan;

    /*
     * Device configuration.
     */
    struct _rt2x00_config	config;

} __attribute__ ((packed));

/*
 * Device specific handlers.
 */
struct _rt2x00_dev_handler {

    /*
     * Device specific module.
     */
    struct module	*dev_module;

    /*
     * Initialization handlers.
     */
    int (*dev_probe)(struct _rt2x00_device *device, struct _rt2x00_config *config, void *priv);
    int (*dev_remove)(struct _rt2x00_device *device);

    /*
     * Radio control.
     */
    int (*dev_radio_on)(struct _rt2x00_device *device);
    int (*dev_radio_off)(struct _rt2x00_device *device);

    /*
     * Configuration handlers.
     */
    int (*dev_update_config)(struct _rt2x00_device *device, struct _rt2x00_config *config, u16 update_flags);

    /*
     * xmit handler.
     */
    int (*dev_xmit_packet)(struct _rt2x00_device *device, struct rtskb *rtskb, u16 rate, u16 xmit_flags);

    /*
     * Handler for direct access to register from core.
     */
    int (*dev_register_access)(struct _rt2x00_device * device, int request, u32 address, u32 * value);

}  __attribute__ ((packed));

static inline void * rt2x00_priv(const struct _rt2x00_device *device) {

    return device->priv;
}

static inline void*
rt2x00_core(const struct _rt2x00_device *device) {

    return device->owner;
}

/*
 * Duration calculations
 * The rate variable passed is: 2 * real_rate (in Mb/s).
 * Therefore length has to be multiplied with 8 to convert bytes to bits and  mulltiply the length
 * with 2 to compensate for the difference between real_rate and the rate variable.
 */
#define ACK_SIZE				14
#define IEEE80211_HEADER			24

static inline u16
get_duration(const unsigned int size, const u8 rate) {

    return ((size * 8 * 2) / rate);
}

static inline u16
get_duration_res(const unsigned int size, const u8 rate) {

    return ((size * 8 * 2) % rate);
}

static inline u16
get_preamble(const struct _rt2x00_config *config) {

    return 144;
}

/*
 * RT2x00 core module functions that can be used in the device specific modules.
 */
extern struct rtnet_device * rt2x00_core_probe(struct _rt2x00_dev_handler *rthandler, void *priv, u32 sizeof_dev);
extern void rt2x00_core_remove(struct rtnet_device *rtnet_dev);
extern void rt2x00_rx(struct _rt2x00_device *device, struct sk_buff *skb);
extern void rt2x00_tx(struct _rt2x00_device *device, u8 ring_type);
extern void rt2x00_update_stats(struct _rt2x00_device *device, u8 flags, u8 value);
extern struct _rt2x00_device* rt2x00_device(struct rtnet_device *rtnet_dev);

#endif


