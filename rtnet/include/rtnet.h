 /* rtnet.h
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
#ifndef __RTNET_H_
#define __RTNET_H_

#ifdef __KERNEL__
#include <rtai.h>
#include <rtai_sched.h>
#include <linux/in.h>
#include <linux/netdevice.h>
#endif /* __KERNEL__ */

/* some configurables */

#define RTNET_PROC_NAME		"rtnet"
#define RTNET_STACK_PRIORITY	1
#define RTNET_RTDEV_PRIORITY	5
#define DROPPING_RTSKB		20

/****************************************************************************************
 * ipv4/arp.c										*
 ****************************************************************************************/
#define RT_ARP_ADDR_LEN  6
#define RT_ARP_TABLE_LEN 20

#ifdef __KERNEL__

struct rtskb;
struct rtskb_head;
struct rtnet_device;

extern struct rtnet_mgr STACK_manager;
extern struct rtnet_mgr RTDEV_manager;
extern struct rtnet_mgr RTMAC_manager;

/***
 *	rtskb - realtime socket buffer
 */
struct rtskb {
	struct rtskb		*next;

	struct rtskb_head	*head;

	struct rtsocket		*sk;
	struct rtnet_device	*rtdev;

	/* transport layer */
	union
	{
		struct tcphdr	*th;
		struct udphdr	*uh;
		struct icmphdr	*icmph;
		struct iphdr	*ipihdr;
		unsigned char	*raw;
	} h;

	/* network layer */
	union
	{
		struct iphdr	*iph;
		struct arphdr	*arph;
		unsigned char	*raw;
	} nh;

	/* link layer */
	union
	{
		struct ethhdr	*ethernet;
		unsigned char	*raw;
	} mac;

	unsigned short		protocol;
	unsigned char		pkt_type;

  	unsigned int		users;
	unsigned char		cloned;
	
	unsigned int 		csum;
	unsigned char		ip_summed;
	
	struct rt_rtable	*dst;
	
	unsigned int		len;
	unsigned int		data_len;
	unsigned int		buf_len;	
	
	unsigned char		*buf_start;
	unsigned char		*buf_end;
	
	unsigned char		*data;
	unsigned char		*tail;
	unsigned char		*end;
	RTIME                   rx;
};

struct rtskb_head {
	struct rtskb	*first;
	struct rtskb	*last;

	unsigned int	qlen;
	spinlock_t	lock;
};


/***
 *	rtsocket
 */
struct rtsocket_ops {
	int  (*bind)	(struct rtsocket *s, struct sockaddr *my_addr, int addr_len);
	int  (*connect)	(struct rtsocket *s, struct sockaddr *serv_addr, int addr_len);
	int  (*listen)	(struct rtsocket *s, int backlog);
	int  (*accept)	(struct rtsocket *s, struct sockaddr *client_addr, int *addr_len);
	int  (*recvmsg)	(struct rtsocket *s, struct msghdr *msg, int len);
	int  (*sendmsg)	(struct rtsocket *s, const struct msghdr *msg, int len);
	void (*close)	(struct rtsocket *s, long timeout);
};

struct rtsocket {
	struct rtsocket		*prev;
	struct rtsocket		*next;				/* next socket in list	*/

	int			fd;				/* file descriptor 	*/

	unsigned short		family;
	unsigned short		typ;
	unsigned short		protocol;
	
	unsigned char		state;

	struct rtsocket_ops	*ops;
	
	struct rtskb_head	incoming;
	
       	unsigned char		connected;			/* connect any socket!  */

	u32			saddr;				/* source ip-addr	*/
	u16			sport;				/* source port		*/

	u32			daddr;				/* destination ip-addr	*/
	u16			dport;				/* destination port	*/

	int			(*wakeup)(int s,void *arg);	/* socket wakeup-func	*/

	void 			*private;

	u8			tos;
};
typedef struct rtsocket SOCKET;



/*** 
 * network-layer-protocol (Layer-3-Protokoll) 
 */
#define MAX_RT_PROTOCOLS 	16
struct rtpacket_type {
	char			*name;
	unsigned short		type;
	struct net_device 	*dev;

	int			(*handler) 
				(struct rtskb *, struct rtnet_device *, struct rtpacket_type *);
	int			(*err_handler)
				(struct rtskb *, struct rtnet_device *, struct rtpacket_type *);

	void			*private;
};

/***
 * transport-layer-protocol 
 */
#define MAX_RT_INET_PROTOCOLS	32
struct rtinet_protocol {
	char			*name;
	unsigned short		protocol;
	
	int			(*handler)    (struct rtskb *);
	void			(*err_handler)(struct rtskb *);
	int			(*socket)
				(struct rtsocket *sock);

	void			*private;
};



/***
 *	rtnet_device 
 */
struct rtnet_device {
	/* Many field are borrowed from struct net_device in <linux/netdevice.h> - WY */
	char			name[IFNAMSIZ];

	unsigned long		rmem_end;	/* shmem "recv" end	*/
	unsigned long		rmem_start;	/* shmem "recv" start	*/
	unsigned long		mem_end;	/* shared mem end	*/
	unsigned long		mem_start;	/* shared mem start	*/
	unsigned long		base_addr;	/* device I/O address	*/
	unsigned int		irq;		/* device IRQ number	*/

	/*
	 *	Some hardware also needs these fields, but they are not
	 *	part of the usual set specified in Space.c.
	 */
	unsigned char		if_port;	/* Selectable AUI, TP,..*/
	unsigned char		dma;		/* DMA channel		*/

	unsigned long		state;
	int			ifindex;

	struct rtnet_device	*next;
	struct net_device	*ldev;		/* can be used by rtnetproxy */

	struct module		*owner;

	__u32			local_addr;	/* in network order	*/

	struct rtsocket		*protocols;

	SEM			txsem;		/* tx-Semaphore		*/
	struct rtskb_head	rxqueue;	/* rx-queue		*/

	unsigned short		flags;	/* interface flags (a la BSD)	*/
	unsigned short		gflags;
	unsigned int		mtu;		/* eth = 1536, tr = 4... */
	unsigned short		type;		/* interface hardware type	*/
	unsigned short		hard_header_len;	/* hardware hdr length	*/
	void			*priv;		/* pointer to private data	*/
	int			features;	/* NETIF_F_* */

	/* Interface address info. */
	unsigned char		broadcast[MAX_ADDR_LEN];	/* hw bcast add	*/
	unsigned char		dev_addr[MAX_ADDR_LEN];	/* hw address	*/
	unsigned char		addr_len;	/* hardware address length	*/

	struct dev_mc_list	*mc_list;	/* Multicast mac addresses	*/
	int			mc_count;	/* Number of installed mcasts	*/
	int			promiscuity;
	int			allmulti;

	MBX			*stack_mbx;
	MBX			*rtdev_mbx;
	struct rtmac_device	*rtmac;

	int			(*open)(struct rtnet_device *rtdev);
	int			(*stop)(struct rtnet_device *rtdev);
	int			(*hard_header) (struct rtskb *,struct rtnet_device *,unsigned short type,void *daddr,void *saddr,unsigned int len);
	int			(*rebuild_header)(struct rtskb *);
	int			(*hard_start_xmit)(struct rtskb *skb,struct rtnet_device *dev);
	int			(*hw_reset)(struct rtnet_device *rtdev);
};



/*
 *  we have two routing tables, the generic routing table and the
 *  specific routing table.  The specific routing table is the table
 *  actually used to route outgoing packets.  The generic routing
 *  table is used to discover specific routes when needed.
 */
struct rt_rtable {
	struct rt_rtable	*prev;
	struct rt_rtable	*next;

	unsigned int 		use_count;

	__u32			rt_dst;
	__u32			rt_dst_mask;
	char			rt_dst_mac_addr[6];

	__u32			rt_src;
	
	int			rt_ifindex;

	struct rtnet_device 	*rt_dev;
};



struct rt_arp_table_struct {
	struct rt_arp_table_struct	*next;
	struct rt_arp_table_struct	*prev;
	
	u32				ip_addr;
	char				hw_addr[RT_ARP_ADDR_LEN];
};

/****************************************************************************************
 * stack_mgr.c	(MODULE)								*
 ****************************************************************************************/


typedef struct rt_msg ALARM_MSG;
typedef struct rt_msg RX_MSG;

/*
extern RT_TASK stack_mgr;
extern MBX     stack_mbx;
extern RT_TASK alarm_mgr;
extern MBX     alarm_mbx;
*/

/****************************************************************************************
 * rtskb.c										*
 ****************************************************************************************/
#if defined(CONFIG_RTNET_MM_VMALLOC)
	#define rt_mem_alloc(size)	vmalloc (size)
	#define rt_mem_free(addr)	vfree (addr)
#else
	#define rt_mem_alloc(size)	kmalloc (size, GFP_KERNEL)
	#define rt_mem_free(addr)	kfree (addr)
#endif


/****
 * The number of free rtskb's in pool is handled by two srq's. inc_pool_srq means 
 * creating rtskbs and dec_pool_srq is the opposite of it.
 */
//these are the new default values for the module parameter
#define DEFAULT_RTSKB_POOL_DEF	24	/* default number of rtskb's in pool				*/
#define DEFAULT_MIN_RTSKB_DEF	16	/* create new rtskb if less then DEFAULT_MIN_RTSKB in Pool	*/
#define DEFAULT_MAX_RTSKB_DEF	32	/* delete rtskb if more then DEFAULT_MAX_RTSKB in Pool		*/

extern int rtskb_pool_pool;		/* needed for /proc/... in rtnet_module.c	*/
extern int rtskb_pool_min;		
extern int rtskb_pool_max;		

extern int rtskb_amount;			/* current number of allocated rtskbs */
extern int rtskb_amount_max;			/* maximum number of allocated rtskbs */

extern void rtskb_over_panic(struct rtskb *skb, int len, void *here);
extern void rtskb_under_panic(struct rtskb *skb, int len, void *here);
extern struct rtskb *new_rtskb(void);
extern void dispose_rtskb(struct rtskb *skb);
extern struct rtskb *alloc_rtskb(unsigned int size);   
#define dev_alloc_rtskb(len)	alloc_rtskb(len)
extern void kfree_rtskb(struct rtskb *skb);
#define dev_kfree_rtskb(a)	kfree_rtskb(a)

extern struct rtskb *rtskb_clone(struct rtskb *skb,int gfp_mask);
extern struct rtskb *rtskb_copy(struct rtskb *skb,int gfp_mask);
extern struct rtskb *__rtskb_dequeue(struct rtskb_head *list);
extern struct rtskb *rtskb_dequeue(struct rtskb_head *list);

extern void __rtskb_queue_head(struct rtskb_head *list, struct rtskb *skb);
extern void rtskb_queue_head(struct rtskb_head *list,struct rtskb *buf);
extern void __rtskb_queue_tail(struct rtskb_head *list, struct rtskb *skb);
extern void rtskb_queue_tail(struct rtskb_head *list,struct rtskb *buf);

extern int rtskb_queue_len(struct rtskb_head *list);
extern int rtskb_queue_empty(struct rtskb_head *list);
extern void rtskb_queue_purge(struct rtskb_head *list);
extern void rtskb_queue_head_init(struct rtskb_head *list);

static inline int rtskb_is_nonlinear(const struct rtskb *skb)
{
	return skb->data_len;
}

static inline int rtskb_headlen(const struct rtskb *skb)
{
	return skb->len - skb->data_len;
}

static inline void rtskb_reserve(struct rtskb *skb, unsigned int len)
{
	skb->data+=len;
	skb->tail+=len;
}

#define RTSKB_LINEAR_ASSERT(rtskb) do { if (rtskb_is_nonlinear(rtskb)) BUG(); }  while (0)

static inline unsigned char *__rtskb_put(struct rtskb *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
 	RTSKB_LINEAR_ASSERT(skb);
	skb->tail+=len;
	skb->len+=len;
	return tmp;
}

static inline unsigned char *rtskb_put(struct rtskb *skb, unsigned int len)
{
	unsigned char *tmp=skb->tail;
	RTSKB_LINEAR_ASSERT(skb);
	skb->tail+=len;
	skb->len+=len;
	if(skb->tail>skb->buf_end) {
		rtskb_over_panic(skb, len, current_text_addr());
	}
	return tmp;
}

static inline unsigned char *__rtskb_push(struct rtskb *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	return skb->data;
}

static inline unsigned char *rtskb_push(struct rtskb *skb, unsigned int len)
{
	skb->data-=len;
	skb->len+=len;
	if (skb->data<skb->buf_start) {
		rtskb_under_panic(skb, len, current_text_addr());
	}
	return skb->data;
}

static inline char *__rtskb_pull(struct rtskb *skb, unsigned int len)
{
	skb->len-=len;
	if (skb->len < skb->data_len)
		BUG();
	return 	skb->data+=len;
}

static inline unsigned char *rtskb_pull(struct rtskb *skb, unsigned int len)
{	
	if (len > skb->len)
		return NULL;
		
	return __rtskb_pull(skb,len);
}


static inline void rtskb_trim(struct rtskb *skb, unsigned int len)
{
	if (skb->len>len) {
		skb->len = len;
		skb->tail = skb->data+len;
	}
}



extern struct rtskb_head rtskb_pool;
extern int rtskb_pool_init(void);
extern int rtskb_pool_release(void);

extern unsigned int rtskb_copy_and_csum_bits(const struct rtskb *skb, int offset, u8 *to, int len, unsigned int csum); 
extern void rtskb_copy_and_csum_dev(const struct rtskb *skb, u8 *to);



/****************************************************************************************
 * rtnet_init.c										*
 ****************************************************************************************/
extern struct rtnet_device *rt_alloc_etherdev(int sizeof_priv);
extern int rt_register_rtnetdev(struct rtnet_device *rtdev);
extern int rt_unregister_rtnetdev(struct rtnet_device *rtdev);



/****************************************************************************************
 * rtnet_dev.c										*
 ****************************************************************************************/
extern void rtnet_chrdev_init(void);
extern void rtnet_chrdev_release(void);



/****************************************************************************************
 * stack_mgr.c										*
 ****************************************************************************************/
enum RTnet_MSG {
	Rx_PACKET = 1,
	Tx_PACKET = 2,
};

struct rtnet_msg {
	int			msg_type;
	struct rtnet_device	*rtdev;
};

struct rtnet_mgr {
	RT_TASK	task;
	MBX	mbx;
};

extern void rt_stack_connect (struct rtnet_device *rtdev, struct rtnet_mgr *mgr);
extern void rt_stack_disconnect (struct rtnet_device *rtdev);
extern int rt_stack_mgr_init (struct rtnet_mgr *mgr);
extern void rt_stack_mgr_delete (struct rtnet_mgr *mgr);
extern int rt_stack_mgr_start (struct rtnet_mgr *mgr);
extern int rt_stack_mgr_stop (struct rtnet_mgr *mgr);

extern void rt_rtdev_connect (struct rtnet_device *rtdev, struct rtnet_mgr *mgr);
extern void rt_rtdev_disconnect (struct rtnet_device *rtdev);
extern int rt_rtdev_mgr_init (struct rtnet_mgr *mgr);
extern void rt_rtdev_mgr_delete (struct rtnet_mgr *mgr);
extern int rt_rtdev_mgr_start (struct rtnet_mgr *mgr);
extern int rt_rtdev_mgr_stop (struct rtnet_mgr *mgr);

extern void rtnetif_rx(struct rtskb *skb);
extern void rtnetif_tx(struct rtnet_device *rtdev);
extern void rtnetif_err_tx(struct rtnet_device *rtdev);
extern void rtnetif_err_rx(struct rtnet_device *rtdev);
extern void rt_mark_stack_mgr(struct rtnet_device *rtdev);


/****************************************************************************************
 * rtdev_mgr.c										*
 ****************************************************************************************/
extern void rtnetif_err_rx(struct rtnet_device *rtdev);
extern void rtnetif_err_tx(struct rtnet_device *rtdev);


/****************************************************************************************
 * dev.c										*
 ****************************************************************************************/
extern struct rtnet_device *rtnet_devices;
extern rwlock_t rtnet_devices_lock;

extern struct rtpacket_type *rt_packets[];

extern void rtdev_add_pack(struct rtpacket_type *pt);
extern void rtdev_remove_pack(struct rtpacket_type *pt);

extern void rtdev_alloc_name (struct rtnet_device *rtdev, const char *name_mask);
extern int rtdev_new_index(void);
extern struct rtnet_device *rtdev_alloc(int sizeof_priv);
extern void rtdev_free(struct rtnet_device *rtdev);

#define dev_get_by_rtdev(rtdev)	(rtdev->dev)
extern struct rtnet_device *rtdev_get_by_name(const char *if_name);
extern struct rtnet_device *rtdev_get_by_dev(struct net_device *dev);
extern struct rtnet_device *rtdev_get_by_index(int ifindex);
extern struct rtnet_device *rtdev_get_by_hwaddr(unsigned short type,char *ha);

extern int rtdev_xmit(struct rtskb *skb);
extern int rtdev_xmit_if(struct rtskb *skb);

extern int rtnet_dev_init(void);
extern int rtnet_dev_release(void);

extern int rtdev_open(struct rtnet_device *rtdev);
extern int rtdev_close(struct rtnet_device *rtdev);

/****************************************************************************************
 * ipv4/protocol.c									*
 ****************************************************************************************/
extern struct rtinet_protocol *rt_inet_protocols[];

#define rt_inet_hashkey(id)  (id & (MAX_RT_INET_PROTOCOLS-1))
extern void rt_inet_add_protocol(struct rtinet_protocol *prot);
extern void rt_inet_del_protocol(struct rtinet_protocol *prot);
extern struct rtinet_protocol *rt_inet_get_protocol(int protocol);
extern unsigned long rt_inet_aton(const char *ip);

/****************************************************************************************
 * ipv4/arp.c										*
 ****************************************************************************************/
extern struct rt_arp_table_struct *free_arp_list;
extern struct rt_arp_table_struct *arp_list;

extern int rt_arp_solicit(struct rtnet_device *dev,u32 target);
extern void rt_arp_table_add(u32 ip_addr, unsigned char *hw_addr);
extern void rt_arp_table_del(u32 ip_addr);
extern void rt_arp_table_init(void);
extern void rt_arp_init(void);
extern void rt_arp_release(void);
extern struct rt_arp_table_struct *rt_arp_table_lookup(u32 ip_addr);
extern struct rt_arp_table_struct *rt_rarp_table_lookup(char *hw_addr);

extern void rt_arp_table_display(void);


/****************************************************************************************
 * ipv4/ip_fragment.c									*
 ****************************************************************************************/
extern struct rtskb *rt_ip_defrag(struct rtskb *skb);


/****************************************************************************************
 * ipv4/ip_input.c									*
 ****************************************************************************************/
extern int rt_ip_local_deliver_finish(struct rtskb *skb);
extern int rt_ip_local_deliver(struct rtskb *skb);
extern int rt_ip_rcv(struct rtskb *skb, struct rtnet_device *rtdev, struct rtpacket_type *pt);
extern int rt_ip_register_fallback( int (*callback)(struct rtskb *skb));

/****************************************************************************************
 * ipv4/ip_output.c									*
 ****************************************************************************************/
extern int rt_ip_build_xmit(struct rtsocket *sk, 
		            int getfrag (const void *, char *, unsigned int, unsigned int),
			    const void *frag, unsigned length, struct rt_rtable *rt, int flags);
extern void rt_ip_init(void);
extern void rt_ip_release(void);


/****************************************************************************************
 * ipv4/ip_sock.c									*
 ****************************************************************************************/
extern int rt_ip_setsockopt (int fd, int optname, char *optval, int optlen);


/****************************************************************************************
 * ipv4/udp.c										*
 ****************************************************************************************/
extern struct rtinet_protocol udp_protocol;
extern void rt_udp_init(void);
extern void rt_udp_release(void);

/****************************************************************************************
 * ipv4/icmp.c										*
 ****************************************************************************************/
extern struct rtinet_protocol icmp_protocol;
extern void rt_icmp_init(void);
extern void rt_icmp_release(void);



/****************************************************************************************
 * ipv4/route.c										*
 ****************************************************************************************/
extern struct rt_rtable *rt_rtables;
extern struct rt_rtable *rt_rtables_generic;

extern struct rt_rtable *rt_ip_route_add(struct rtnet_device *rtdev, u32 addr, u32 mask);
extern struct rt_rtable *rt_ip_route_add_specific(struct rtnet_device *rtdev, u32 addr, unsigned char *hw_addr);
extern void rt_ip_route_del(struct rtnet_device *rtdev);
extern void rt_ip_route_del_specific(struct rtnet_device *rtdev, u32 addr);
extern struct rt_rtable *rt_ip_route_find(u32 daddr);

extern int rt_ip_route_input(struct rtskb *skb, u32 daddr, u32 saddr, struct rtnet_device *rtdev);
extern int rt_ip_route_output(struct rt_rtable **rp, u32 daddr, u32 saddr);

extern void rt_ip_routing_init(void);
extern void rt_ip_routing_release(void);



/****************************************************************************************
 * ipv4/af_inet.c									*
 ****************************************************************************************/
extern void rt_inet_proto_init(void);
extern void rt_inet_proto_release(void);
extern void rt_inet_init(void);
extern void rt_inet_release(void);



/****************************************************************************************
 * ethernet/eth.c									*
 ****************************************************************************************/
extern int rt_eth_header(struct rtskb *skb,struct rtnet_device *rtdev, 
			 unsigned short type,void *daddr,void *saddr,unsigned int len);
extern unsigned short rt_eth_type_trans(struct rtskb *skb, struct rtnet_device *dev);

/****************************************************************************************
 * lib/crc32.c										*
 ****************************************************************************************/
extern int init_crc32(void);
extern void cleanup_crc32(void);

/****************************************************************************************
 * socket.c										*
 ****************************************************************************************/
/* the external interface */
#define RT_SOCKETS	64
extern struct rtsocket rt_sockets[];
extern struct rtsocket *rt_socket_alloc(void);
extern void rt_socket_release(struct rtsocket *sock);
extern struct rtsocket *rt_socket_lookup(unsigned int fd);

/* file descriptor interface */
extern int rt_socket		(int family, int type, int protocol);
extern int rt_socket_bind	(int fd, struct sockaddr *my_addr, int addr_len);
extern int rt_socket_listen	(int fd, int backlog);
extern int rt_socket_connect	(int fd, struct sockaddr *addr, int addr_len);
extern int rt_socket_accept	(int fd, struct sockaddr *addr, int *addrlen);
extern int rt_socket_close	(int fd);
extern int rt_socket_writev	(int fd, struct iovec *vector, size_t count);
extern int rt_socket_send	(int fd, void *buf, int len, unsigned int flags);
extern int rt_socket_sendto	(int fd, void *buf, int len, unsigned int flags, 
				struct sockaddr *to, int tolen);
extern int rt_socket_sendmsg	(int fd, struct msghdr *msg, unsigned int flags);
extern int rt_socket_readv	(int fd, struct iovec *vector, size_t count);
extern int rt_socket_recv	(int fd, void *buf, int len, unsigned int flags);
extern int rt_socket_recvfrom	(int fd, void *buf, int len, unsigned int flags, 
				struct sockaddr *from, int *fromlen);
extern int rt_socket_recvmsg	(int s, struct msghdr *msg, unsigned int flags);
extern int rt_socket_getsockname(int fd, struct sockaddr *addr, int addr_len);
extern int rt_socket_callback	(int fd, int (*func)(int,void *), void *arg);

#define rt_bind		rt_socket_bind
#define rt_listen	rt_socket_listen
#define rt_connect	rt_socket_connect
#define rt_accept	rt_socket_accept
#define rt_close	rt_socket_close
#define rt_sendto	rt_socket_sendto
#define rt_recvfrom	rt_socket_recvfrom

/* static interface */
extern int rt_ssocket(SOCKET* socket, int family, int type, int protocol);
extern int rt_ssocket_bind(SOCKET *socket, struct sockaddr *my_addr, int addr_len);
extern int rt_ssocket_listen(SOCKET *socket, int backlog);
extern int rt_ssocket_connect(SOCKET *socket, struct sockaddr *addr, int addr_len);
extern int rt_ssocket_accept(SOCKET *socket, struct sockaddr *addr, int *addrlen);
extern int rt_ssocket_close(SOCKET *socket);
extern int rt_ssocket_writev(SOCKET *socket, struct iovec *vector, size_t count);
extern int rt_ssocket_send(SOCKET *socket, void *buf, int len, unsigned int flags);
extern int rt_ssocket_sendto(SOCKET *socket, void *buf, int len, unsigned int flags, 
				struct sockaddr *to, int tolen);
extern int rt_ssocket_sendmsg(SOCKET *socket, struct msghdr *msg, unsigned int flags);
extern int rt_ssocket_readv(SOCKET *socket, struct iovec *vector, size_t count);
extern int rt_ssocket_recv(SOCKET *socket, void *buf, int len, unsigned int flags);
extern int rt_ssocket_recvfrom(SOCKET *socket, void *buf, int len, unsigned int flags, 
				struct sockaddr *from, int fromlen);
extern int rt_ssocket_recvmsg(SOCKET *socket, struct msghdr *msg, unsigned int flags);
extern int rt_ssocket_getsockname(SOCKET *socket, struct sockaddr *addr, int addr_len);
extern int rt_ssocket_callback(SOCKET *socket, int (*func)(int,void *), void *arg);

extern void rtsockets_init(void);
extern void rtsockets_release(void);


#endif  /* __KERNEL__ */


/****************************************************************************************
 * rtnet_dev.c 										*
 ****************************************************************************************/
/* user interface for /dev/rtnet */
#define RTNET_MINOR			240

#define IOC_RT_IFUP			100
#define IOC_RT_IFDOWN			101
#define IOC_RT_IF			102
#define IOC_RT_ROUTE_ADD		103
#define IOC_RT_ROUTE_SOLICIT		104
#define IOC_RT_ROUTE_DELETE		105
#define IOC_RT_ROUTE_GET		106


struct rtnet_config{
	char if_name[16];
	int len;

	u32 ip_addr;
	u32 ip_mask;
	u32 ip_netaddr;
	u32 ip_broadcast;
};


#endif  /* __RTNET_H_ */

