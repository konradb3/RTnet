/* rtnetproxy.c: a network driver that uses the RTAI rtnet driver to
 * transport IP data from/to Linux kernel mode.
 * This allows the usage of TCP/IP from linux space using via the RTNET 
 * network adapter.
 *
 *
 * Usage:
 * 
 * insmod rtnetproxy.o    (only after having rtnet up and running)
 *
 * ifconfig rtproxy up IP_ADDRESS netmask NETMASK
 *
 * Use it like any other network device from linux.
 *
 * Restrictions:
 * Only IPV4 based protocols are supported, UDP and ICMP can be send out
 * but not received - as these are handled directly by rtnet!
 * 
 *
 * 
 * Based on the linux net driver dummy.c by Nick Holloway
 *
 *
 * Changelog:
 *
 * 08-Nov-2002  Mathias Koehrer - Clear separation between rtai context and
 *                                standard linux driver context.
 *                                Data exchange via ringbuffers.
 *                                A RTAI thread is used for rtnet transmission.
 * 
 * 05-Nov-2002  Mathias Koehrer - Initial version! 
 *                                Development based on rtnet 0.2.6, 
 *                                rtai-24.1.10, kernel 2.4.19
 *                               
 * 
 * Mathias Koehrer - mathias_koehrer@yahoo.de
*/

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>

#include <linux/if_ether.h> /* For the statistics structure. */
#include <linux/if_arp.h>   /* For ARPHRD_ETHER */


#include <rtnet.h>
#include <rtnet_internal.h>

/* **************************************************************************
 *  SKB ringbuffers for exchanging data between rtnet and kernel:
 * ************************************************************************ */
#define SKB_RINGBUFFER_SIZE 16
typedef struct 
{
    int wr;        // Offset to wr: Increment before write!
    int rd;        // Offset to rd: Increment before read!
    void *ptr[SKB_RINGBUFFER_SIZE];
} skb_exch_ringbuffer_t;

/* Stores "new" skbs to be used by rtnet: */
static skb_exch_ringbuffer_t  ring_skb_kernel_rtnet; 

/* Stores "used" skbs to be freed by the kernel: */
static skb_exch_ringbuffer_t  ring_skb_rtnet_kernel;

/* Stores "new" rtskbs to be used by the kernel: */
static skb_exch_ringbuffer_t  ring_rtskb_rtnet_kernel; 

/* Stores "used" rtskbs to be freed by rtnet: */
static skb_exch_ringbuffer_t  ring_rtskb_kernel_rtnet;

/* Spinlock for protected code areas... */
static spinlock_t skb_spinlock = SPIN_LOCK_UNLOCKED;

/* handle for rtai srq: */
static int rtnetproxy_srq = 0;

/* Thread for transmission */
static RT_TASK rtnetproxy_thread;

/* ***********************************************************************
 * Returns the next pointer from the ringbuffer or zero if nothing is
 * available 
 * ********************************************************************* */
static void *read_from_ringbuffer(skb_exch_ringbuffer_t *pRing)
{
    void *ret = 0;
    unsigned int flags = rt_spin_lock_irqsave(&skb_spinlock);
    if (pRing->rd != pRing->wr)
    {
        pRing->rd = (pRing->rd + 1) % SKB_RINGBUFFER_SIZE;
        ret = pRing->ptr[pRing->rd];
    }
    rt_spin_unlock_irqrestore(flags, &skb_spinlock);
    return ret;
}

/* ************************************************************************
 * Puts p at the end of the ringbuffer.
 * Returns p on success. If there is no space in the ringbuffer,
 * zwei is returned and p is not queued! 
 * *********************************************************************** */
static void *write_to_ringbuffer(skb_exch_ringbuffer_t *pRing, void *p)
{
    void *ret;
    int tmpwr;
    unsigned int flags = rt_spin_lock_irqsave(&skb_spinlock);
    tmpwr = (pRing->wr + 1) % SKB_RINGBUFFER_SIZE;
    if (pRing->rd != tmpwr)
    {
        pRing->wr = tmpwr;
        pRing->ptr[tmpwr] = p;
        ret = p;
    }
    else
    {
        ret = 0;
    }
    rt_spin_unlock_irqrestore(flags, &skb_spinlock);
    return ret;
}



/* ************************************************************************
 * net_device specific stuff:
 * ************************************************************************ */
static struct net_device dev_rtnetproxy;

static int rtnetproxy_xmit(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *rtnetproxy_get_stats(struct net_device *dev);



/* ************************************************************************
 * ************************************************************************
 *   T R A N S M I T 
 * ************************************************************************
 * ************************************************************************ */

/* ************************************************************************
 * This functions runs in rtai context.
 * It is called from rtnetproxy_user_srq whenever there is frame to sent out
 * Copy the standard linkx sk_buff buffer to a rtnet buffer and send it out 
 * using rtnet functions. 
 * ************************************************************************ */
static inline void send_data_out(struct sk_buff *skb)
{

    struct rtskb *rtskb;  
    struct rt_rtable *rt;
    
    struct skb_data_format 
    {
        struct ethhdr ethhdr;
        char   reserved[12]; /* Ugly but it works... All the not-interesting header bytes */
        u32    ip_src;
        u32    ip_dst;
    } __attribute__ ((packed));  /* Important to have this structure packed!
                                  * It represents the ethernet frame on the line and
                                  * thus no spaces are allowed! */
    
    struct skb_data_format *pData;
    int rc;
    
    /* Copy the data from the standard sk_buff to the realtime sk_buff:
     * Both have the same length. */
    rtskb = alloc_rtskb(skb->len);
    if (NULL == rtskb) {
        return;
    }

    memcpy(rtskb->data, skb->data, skb->len);
    rtskb->len = skb->len;

    pData = (struct skb_data_format*) rtskb->data;

    /* Determine the device to use: Only ip routing is used here.
     * Non-ip protocols are not supported... */
    rc = rt_ip_route_output(&rt, pData->ip_dst, pData->ip_src);
    if (rc == 0)
    {
        struct rtnet_device *rtdev = rt->rt_dev;
        struct net_device *dev = dev_get_by_rtdev(rtdev);
        rtskb->dst = rt;
        rtskb->rtdev = rt->rt_dev;

        /* Fill in the ethernet headers: There is already space for the header
         * but they contain zeros only => Fill it */
        memcpy(pData->ethhdr.h_source, dev->dev_addr, dev->addr_len);
        memcpy(pData->ethhdr.h_dest, rt->rt_dst_mac_addr, dev->addr_len);

        /* Call the actual transmit function (this function is semaphore 
         * protected): */
        rtdev_xmit(rtskb);
        /* The rtskb is freed somewhere deep in the driver... 
         * No need to do it here. */
    }
    else
    {
        /* Routing failed => Free rtskb here... */
        kfree_rtskb(rtskb);
    }

}

/* ************************************************************************
 * This is a RTAI thread. It will be activated (resumed) by the 
 * function "rtnetproxy_xmit" (in linux context)
 * whenever new frames have to be sent out. 
 * ************************************************************************ */
static void rtnetproxy_transmit_thread(int arg)
{
    while (1)
    {
        struct sk_buff *skb;

        /* Send out all frames in the ringbuffer that have not been sent yet */
        while ((skb = read_from_ringbuffer(&ring_skb_kernel_rtnet)) != 0)
        {
            send_data_out(skb);
            /* Place the "used" skb in the ringbuffer back to kernel */
            write_to_ringbuffer(&ring_skb_rtnet_kernel, skb);
        }
        /* Suspend self! Will be activated with next frame to send... */
        rt_task_suspend(&rtnetproxy_thread);
    }
}


/* ************************************************************************
 *  hard_xmit
 *
 *  This function runs in linux kernel context and is executed whenever
 *  there is a frame to be sent out.
 * ************************************************************************ */
static int rtnetproxy_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct net_device_stats *stats = dev->priv;

    if (write_to_ringbuffer(&ring_skb_kernel_rtnet, skb))
    {
        stats->tx_packets++;
        stats->tx_bytes+=skb->len;
    }
    else
    {
        /* No space in the ringbuffer... */
        printk("rtnetproxy_xmit - no space in queue\n");
        dev_kfree_skb(skb);  /* Free the standard skb. */
    }
    
    /* Signal rtnet that there are packets waiting to be processed... 
     * Resume the transmission thread (function rtnetproxy_transmit_thread)
     * */
    rt_task_resume(&rtnetproxy_thread);

    /* Delete all "used" skbs that already have been processed... */
    {
        struct sk_buff *del_skb;
        while ((del_skb = read_from_ringbuffer(&ring_skb_rtnet_kernel)) != 0)
        {
            dev_kfree_skb(del_skb);  /* Free the standard skb. */
        }
    }
    return 0;
}


/* ************************************************************************
 * ************************************************************************
 *   R E C E I V E
 * ************************************************************************
 * ************************************************************************ */


/* ************************************************************************
 * This function runs in rtai context.
 *
 * It is called from inside rtnet whenever a packet has been received that
 * has to be processed by rtnetproxy.
 * ************************************************************************ */
static int rtnetproxy_recv(struct rtskb *rtskb)
{

    /* Place the rtskb in the ringbuffer: */
    if (write_to_ringbuffer(&ring_rtskb_rtnet_kernel, rtskb))
    {
        /* Switch over to kernel context: */
        rt_pend_linux_srq(rtnetproxy_srq);
    }
    else
    {
        /* No space in ringbuffer => Free rtskb here... */
        rt_printk("rtnetproxy_recv: No space in queue\n");
        kfree_rtskb(rtskb);
    }

    /* Delete all "used" rtskb entries from the ringbuffer */
    {
        struct rtskb *del;
        while ((del=read_from_ringbuffer(&ring_rtskb_kernel_rtnet))!=0)
        {
            kfree_rtskb(del);
        }
    }
    return 0;
}


/* ************************************************************************
 * This function runs in kernel mode.
 * It is activated from rtnetproxy_rtai_srq whenever rtnet received a 
 * frame to be processed by rtnetproxy.
 * ************************************************************************ */
static inline void rtnetproxy_kernel_recv(struct rtskb *rtskb)
{
    struct sk_buff *skb;
    struct net_device *dev = &dev_rtnetproxy;
    struct net_device_stats *stats = dev->priv;
    
#define IP_OVERHEAD 34
    int len = rtskb->len + IP_OVERHEAD;

    /* Copy the realtime skb (rtskb) to the standard skb: */
    skb = dev_alloc_skb(len+2);
    skb_reserve(skb, 2);

    memcpy(skb_put(skb, len), rtskb->data-IP_OVERHEAD, len);

    
    /* Set some relevant entries in the skb: */
    skb->protocol=eth_type_trans(skb,dev);
    skb->dev=dev;
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->pkt_type = PACKET_HOST;  /* Extremely important! Why?!? */

    dev->last_rx = jiffies;
    stats->rx_bytes+=skb->len;
    stats->rx_packets++;
   
    
    netif_rx(skb);  /* pass it to the received stuff */
        
}

/* ************************************************************************
 * This function runs in kernel mode.
 * It is activated from rtnetproxy_recv whenever rtnet received a frame to
 * be processed by rtnetproxy.
 * ************************************************************************ */
static void rtnetproxy_rtai_srq(void)
{
    struct rtskb *rtskb;

    while ( (rtskb = read_from_ringbuffer(&ring_rtskb_rtnet_kernel)) != 0)
    {
        rtnetproxy_kernel_recv(rtskb);
        /* Place "used" rtskb in backqueue... */
        write_to_ringbuffer(&ring_rtskb_kernel_rtnet, rtskb);
    }
}

/* ************************************************************************
 * ************************************************************************
 *   G E N E R A L
 * ************************************************************************
 * ************************************************************************ */

/* fake multicast ability */
static void set_multicast_list(struct net_device *dev)
{
}

#ifdef CONFIG_NET_FASTROUTE
static int rtnetproxy_accept_fastpath(struct net_device *dev, struct dst_entry *dst)
{
    return -1;
}
#endif

/* ************************************************************************
 *  device init
 * ************************************************************************ */
static int __init rtnetproxy_init(struct net_device *dev)
{
    /* Initialize the device structure. */

    dev->priv = kmalloc(sizeof(struct net_device_stats), GFP_KERNEL);
    if (dev->priv == NULL)
        return -ENOMEM;
    memset(dev->priv, 0, sizeof(struct net_device_stats));

    dev->get_stats = rtnetproxy_get_stats;
    dev->hard_start_xmit = rtnetproxy_xmit;
    dev->set_multicast_list = set_multicast_list;
#ifdef CONFIG_NET_FASTROUTE
    dev->accept_fastpath = rtnetproxy_accept_fastpath;
#endif

    /* Fill in device structure with ethernet-generic values. */
    ether_setup(dev);
    dev->tx_queue_len = 0;
    dev->flags |= IFF_NOARP;
    dev->flags &= ~IFF_MULTICAST;

    return 0;
}

/* ************************************************************************
 *  get_stats
 * ************************************************************************ */
static struct net_device_stats *rtnetproxy_get_stats(struct net_device *dev)
{
    return dev->priv;
}

/* ************************************************************************
 * ************************************************************************
 *   I N I T
 * ************************************************************************
 * ************************************************************************ */
static int __init rtnetproxy_init_module(void)
{
    int err;

    dev_rtnetproxy.init = rtnetproxy_init;
    SET_MODULE_OWNER(&dev_rtnetproxy);

    /* Define the name for this unit */
    err=dev_alloc_name(&dev_rtnetproxy,"rtproxy");

    if(err<0)
        return err;
    err = register_netdev(&dev_rtnetproxy);
    if (err<0)
        return err;

    /* Initialize the ringbuffers: */
    memset(&ring_rtskb_kernel_rtnet, 0, sizeof(ring_rtskb_kernel_rtnet));
    memset(&ring_rtskb_rtnet_kernel, 0, sizeof(ring_rtskb_rtnet_kernel));
    memset(&ring_skb_kernel_rtnet, 0, sizeof(ring_skb_kernel_rtnet));
    memset(&ring_skb_rtnet_kernel, 0, sizeof(ring_skb_rtnet_kernel));

    /* Init the task for transmission */
    rt_task_init(&rtnetproxy_thread, rtnetproxy_transmit_thread, 0, 2000, 
                  RT_LOWEST_PRIORITY, 0, 0);
        
    /* Register srq */
    rtnetproxy_srq = rt_request_srq(0, rtnetproxy_rtai_srq, 0);
    
    /* rtNet stuff: */
    rt_ip_register_fallback(rtnetproxy_recv);

    printk("rtnetproxy installed as \"%s\"\n", dev_rtnetproxy.name);

    return 0;
}


static void __exit rtnetproxy_cleanup_module(void)
{

    /* Unregister the fallback at rtnet */
    rt_ip_register_fallback(0);

    /* free the rtai srq */
    rt_free_srq(rtnetproxy_srq);

    rt_task_suspend(&rtnetproxy_thread);
    rt_task_delete(&rtnetproxy_thread);
   
    /* Free the ringbuffers... */
    {
        struct sk_buff *del_skb;  /* standard skb */
        while ((del_skb = read_from_ringbuffer(&ring_skb_rtnet_kernel)) != 0)
        {
            dev_kfree_skb(del_skb);
        }
        while ((del_skb = read_from_ringbuffer(&ring_skb_kernel_rtnet)) != 0)
        {
            dev_kfree_skb(del_skb); 
        }
    }
    {
        struct rtskb *del; /* rtnet skb */
        while ((del=read_from_ringbuffer(&ring_rtskb_kernel_rtnet))!=0)
        {
            kfree_rtskb(del); // Although this is kernel mode, freeing should work...
        }
        while ((del=read_from_ringbuffer(&ring_rtskb_rtnet_kernel))!=0)
        {
            kfree_rtskb(del); // Although this is kernel mode, freeing should work...
        }
    }

    /* Unregister the net device: */
    unregister_netdev(&dev_rtnetproxy);
    kfree(dev_rtnetproxy.priv);

    memset(&dev_rtnetproxy, 0, sizeof(dev_rtnetproxy));
    dev_rtnetproxy.init = rtnetproxy_init;
}

module_init(rtnetproxy_init_module);
module_exit(rtnetproxy_cleanup_module);
MODULE_LICENSE("GPL");
