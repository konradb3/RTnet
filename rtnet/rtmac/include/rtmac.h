/* include/rtmac.h
 *
 * rtmac - real-time networking medium access control subsystem
 * Copyright (C) 2002 Marc Kleine-Budde <kleine-budde@gmx.de>
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

#ifndef __RTMAC_H_
#define __RTMAC_H_

#ifdef __KERNEL__

#define RTMAC_VERSION	0x1
#define ETH_RTMAC	0x9021

typedef enum {
	TDMA = 1,
} DISC_TYPE;


struct rtmac_device {
	struct rtnet_device		*rtdev;
	void				*priv;

	struct rtmac_ioctl_ops		*ioctl_ops;
	struct rtmac_disc_ops		*disc_ops;
	struct rtmac_disc_type		*disc_type;
};


struct rtmac_hdr {
	u16				type;
	u8				ver;
	u8				flags;

	u16				len;
	u16				res;		// reserved for future use :)
} __attribute__ ((packed));


/***
 * rtmac_proc.c
 */
#define RTMAC_PROC_NAME "rtmac"

extern int rtmac_proc_register(void);
extern void rtmac_proc_release(void);
extern int rtmac_proc_read(char *page, char **start, off_t off, int count, int *eof, void *data);





/***
 * rtmac_chrdev.c
 */
struct rtmac_ioctl_ops {
	int (*client)			(struct rtnet_device *rtdev);
	int (*master)			(struct rtnet_device *rtdev, unsigned int cycle, unsigned int mtu);
	int (*up)			(struct rtnet_device *rtdev);
	int (*down)			(struct rtnet_device *rtdev);
	int (*add)			(struct rtnet_device *rtdev, u32 ip_addr);
	int (*remove)			(struct rtnet_device *rtdev, u32 ip_addr);
	int (*add_nrt)			(struct rtnet_device *rtdev, u32 ip_addr);
	int (*remove_nrt)		(struct rtnet_device *rtdev, u32 ip_addr);
	int (*cycle)			(struct rtnet_device *rtdev, unsigned int cycle);
	int (*mtu)			(struct rtnet_device *rtdev, unsigned int mtu);
	int (*offset)			(struct rtnet_device *rtdev, u32 ip_addr, unsigned int offset);
};

extern int rtmac_chrdev_init(void);
extern void rtmac_chrdev_release(void);
extern int rtmac_ioctl_client(struct rtnet_device *rtdev);
extern int rtmac_ioctl_master(struct rtnet_device *rtdev, unsigned int cycle, unsigned int mtu);
extern int rtmac_ioctl_up(struct rtnet_device *rtdev);
extern int rtmac_ioctl_down(struct rtnet_device *rtdev);
extern int rtmac_ioctl_add(struct rtnet_device *rtdev, u32 ip_addr);
extern int rtmac_ioctl_remove(struct rtnet_device *rtdev, u32 ip_addr);
extern int rtmac_ioctl_add_nrt(struct rtnet_device *rtdev, u32 ip_addr);
extern int rtmac_ioctl_remove_nrt(struct rtnet_device *rtdev, u32 ip_addr);
extern int rtmac_ioctl_cycle(struct rtnet_device *rtdev, unsigned int cycle);
extern int rtmac_ioctl_mtu(struct rtnet_device *rtdev, unsigned int mtu);
extern int rtmac_ioctl_offset(struct rtnet_device *rtdev, u32 ip_addr, unsigned int offset);





/***
 * rtmac_module.c
 */
extern int rtmac_init(void);
extern void rtmac_release(void);





/***
 * rtmac_disc.c
 */
struct rtmac_disc_ops {
	int (*init)			(struct rtnet_device *rtdev);
	int (*release)			(struct rtnet_device *rtdev);
	// FIXME: get_mtu + get_cycle
};

struct rtmac_disc_type {
	int (*packet_rx)		(struct rtskb *skb, struct rtnet_device *rtdev, struct rtpacket_type *pt);
	int (*rt_packet_tx)		(struct rtskb *skb, struct rtnet_device *rtdev);
	int (*proxy_packet_tx)		(struct rtskb *skb, struct rtnet_device *rtdev);
	struct rtmac_disc_ops		*disc_ops;
	struct rtmac_ioctl_ops		*ioctl_ops;
};

extern int rtmac_disc_init(struct rtnet_device *rtdev, struct rtmac_disc_type *disc);
extern int rtmac_disc_release(struct rtnet_device *rtdev);

extern int tdma_start(struct rtnet_device *rtdev);
extern void tdma_stop(struct rtnet_device *rtdev);



#endif // __KERNEL__

/*
 * rtmac_chrdev.c
 *
 * user interface for /dev/rtmac
 *
 *
 */
#define RTMAC_MINOR		241

#define RTMAC_IOC_MAGIC		0xFD // FIXME: change IOCTL numbers

#define RTMAC_IOC_CLIENT	200
#define RTMAC_IOC_MASTER	201
#define RTMAC_IOC_UP		202
#define RTMAC_IOC_DOWN		203
#define RTMAC_IOC_ADD		204
#define RTMAC_IOC_REMOVE	205
#define RTMAC_IOC_ADD_NRT	206
#define RTMAC_IOC_REMOVE_NRT	207
#define RTMAC_IOC_CYCLE		208
#define RTMAC_IOC_MTU		209
#define RTMAC_IOC_OFFSET	210 //FIXME: hex or dec numbers?


struct rtmac_config {
	char		if_name[16];	
	u32 		ip_addr;
	unsigned int	cycle;
	unsigned int	mtu;
	unsigned int	offset;
};

#endif //__RTMAC_H_
//EOF
