/* rtnetproxy.c: a Linux network driver that uses the RTnet driver to
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/init.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <net/sock.h>
#include <net/ip.h>

#include <linux/if_ether.h> /* For the statistics structure. */
#include <linux/if_arp.h>   /* For ARPHRD_ETHER */


#include <rtdev.h>
#include <rtskb.h>
#include <rtnet_sys.h>
#include <ipv4/ip_input.h>
#include <ipv4/route.h>


static struct net_device *dev_rtnetproxy;

/* **************************************************************************
 *  SKB pool management (JK):
 * ************************************************************************ */
#define DEFAULT_PROXY_RTSKBS        32

static unsigned int proxy_rtskbs = DEFAULT_PROXY_RTSKBS;
module_param(proxy_rtskbs, uint, 0444);
MODULE_PARM_DESC(proxy_rtskbs, "Number of realtime socket buffers in proxy pool");

static struct rtskb_queue rtskb_pool;


/* **************************************************************************
 *  SKB ringbuffers for exchanging data between rtnet and kernel:
 * ************************************************************************ */
#define SKB_RINGBUFFER_SIZE 16
typedef struct {
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
static rtdm_lock_t skb_spinlock = RTDM_LOCK_UNLOCKED;

/* handle for non-real-time signal */
static rtdm_nrtsig_t rtnetproxy_signal;

/* Thread for transmission */
static rtdm_task_t rtnetproxy_thread;

static rtdm_sem_t rtnetproxy_sem;

#ifdef CONFIG_RTNET_ADDON_PROXY_ARP
static char* rtdev_attach = "rteth0";
module_param(rtdev_attach, charp, 0444);
MODULE_PARM_DESC(rtdev_attach, "Attach to the specified RTnet device");

struct rtnet_device *rtnetproxy_rtdev;
#endif

/* ***********************************************************************
 * Returns the next pointer from the ringbuffer or zero if nothing is
 * available
 * ********************************************************************* */
static void *read_from_ringbuffer(skb_exch_ringbuffer_t *pRing)
{
    void            *ret = 0;
    rtdm_lockctx_t  context;


    rtdm_lock_get_irqsave(&skb_spinlock, context);
    if (pRing->rd != pRing->wr) {
        pRing->rd = (pRing->rd + 1) % SKB_RINGBUFFER_SIZE;
        ret = pRing->ptr[pRing->rd];
    }
    rtdm_lock_put_irqrestore(&skb_spinlock, context);
    return ret;
}

/* ************************************************************************
 * Puts p at the end of the ringbuffer.
 * Returns p on success. If there is no space in the ringbuffer,
 * zwei is returned and p is not queued!
 * *********************************************************************** */
static void *write_to_ringbuffer(skb_exch_ringbuffer_t *pRing, void *p)
{
    void            *ret;
    int             tmpwr;
    rtdm_lockctx_t  context;


    rtdm_lock_get_irqsave(&skb_spinlock, context);
    tmpwr = (pRing->wr + 1) % SKB_RINGBUFFER_SIZE;
    if (pRing->rd != tmpwr) {
        pRing->wr = tmpwr;
        pRing->ptr[tmpwr] = p;
        ret = p;
    } else
        ret = 0;

    rtdm_lock_put_irqrestore(&skb_spinlock, context);
    return ret;
}



/* ************************************************************************
 * ************************************************************************
 *   T R A N S M I T
 * ************************************************************************
 * ************************************************************************ */

/* ************************************************************************
 * This functions runs in rtai context.
 * It is called from rtnetproxy_transmit_thread whenever there is frame to
 * sent out. Copies the standard linux sk_buff buffer to a rtnet buffer and
 * sends it out using rtnet functions.
 * ************************************************************************ */
static inline void send_data_out(struct sk_buff *skb)
{

    struct rtskb        *rtskb;
#ifndef CONFIG_RTNET_ADDON_PROXY_ARP
    struct dest_route   rt;
    int rc;
#endif

    struct skb_data_format {
        struct ethhdr ethhdr;
        char   reserved[12]; /* Ugly but it works... All the not-interesting header bytes */
        u32    ip_src;
        u32    ip_dst;
    } __attribute__ ((packed));  /* Important to have this structure packed!
                                  * It represents the ethernet frame on the line and
                                  * thus no spaces are allowed! */

    struct skb_data_format *pData;

    /* Copy the data from the standard sk_buff to the realtime sk_buff:
     * Both have the same length. */
    rtskb = alloc_rtskb(skb->len, &rtskb_pool);
    if (NULL == rtskb)
        return;

    memcpy(rtskb->data, skb->data, skb->len);
    rtskb->len = skb->len;

    pData = (struct skb_data_format*) rtskb->data;

#ifdef CONFIG_RTNET_ADDON_PROXY_ARP
    rtdev_reference(rtnetproxy_rtdev);

    rtskb->rtdev = rtnetproxy_rtdev;

    /* Call the actual transmit function */
    rtdev_xmit_proxy(rtskb);

    rtdev_dereference(rtnetproxy_rtdev);

#else /* !CONFIG_RTNET_ADDON_PROXY_ARP */
    /* Determine the device to use: Only ip routing is used here.
     * Non-ip protocols are not supported... */
    rc = rt_ip_route_output(&rt, pData->ip_dst, INADDR_ANY);
    if (rc == 0) {
        struct rtnet_device *rtdev = rt.rtdev;


        /* check if IP source address fits */
        if (rtdev->local_ip != pData->ip_src) {
            rtdev_dereference(rtdev);
            kfree_rtskb(rtskb);
            return;
        }

        rtskb->rtdev = rtdev;

        /* Fill in the ethernet headers: There is already space for the header
         * but they contain zeros only => Fill it */
        memcpy(pData->ethhdr.h_source, rtdev->dev_addr, rtdev->addr_len);
        memcpy(pData->ethhdr.h_dest, rt.dev_addr, rtdev->addr_len);

        /* Call the actual transmit function */
        rtdev_xmit_proxy(rtskb);

        /* The rtskb is freed somewhere deep in the driver...
         * No need to do it here. */

        rtdev_dereference(rtdev);
    } else
        /* Routing failed => Free rtskb here... */
        kfree_rtskb(rtskb);

#endif /* CONFIG_RTNET_ADDON_PROXY_ARP */
}

/* ************************************************************************
 * This is a RTAI thread. It will be activated (resumed) by the
 * functions "rtnetproxy_xmit" or "rtnetproxy_kernel_recv" (in linux context)
 * whenever new frames have to be sent out or if the
 * "used" rtskb ringbuffer is full.
 * ************************************************************************ */
static void rtnetproxy_transmit_thread(void *arg)
{
    struct sk_buff *skb;
    struct rtskb *del;

    while (1) {
        /* Free all "used" rtskbs in ringbuffer */
        while ((del=read_from_ringbuffer(&ring_rtskb_kernel_rtnet)) != 0)
            kfree_rtskb(del);

        /* Send out all frames in the ringbuffer that have not been sent yet */
        while ((skb = read_from_ringbuffer(&ring_skb_kernel_rtnet)) != 0) {
            send_data_out(skb);
            /* Place the "used" skb in the ringbuffer back to kernel */
            write_to_ringbuffer(&ring_skb_rtnet_kernel, skb);
        }
        /* Will be activated with next frame to send... */
        rtdm_sem_down(&rtnetproxy_sem);
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
    struct sk_buff *del_skb;
    int ret = NETDEV_TX_OK;

    if (write_to_ringbuffer(&ring_skb_kernel_rtnet, skb)) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
        dev->stats.tx_packets++;
        dev->stats.tx_bytes+=skb->len;
#endif
        /* Signal rtnet that there are packets waiting to be processed. */
        rtdm_sem_up(&rtnetproxy_sem);
    } else
        /* No space in the ringbuffer... */
        ret = NETDEV_TX_BUSY;

    /* Delete all "used" skbs that already have been processed... */
    while ((del_skb = read_from_ringbuffer(&ring_skb_rtnet_kernel)) != 0)
        dev_kfree_skb(del_skb);  /* Free the standard skb. */

    return ret;
}


/* ************************************************************************
 * ************************************************************************
 *   R E C E I V E
 * ************************************************************************
 * ************************************************************************ */


/* ************************************************************************
 * This function runs in real-time context.
 *
 * It is called from inside rtnet whenever a packet has been received that
 * has to be processed by rtnetproxy.
 * ************************************************************************ */
static void rtnetproxy_recv(struct rtskb *rtskb)
{
    /* Acquire rtskb (JK) */
    if (rtskb_acquire(rtskb, &rtskb_pool) != 0) {
        rtdm_printk("rtnetproxy_recv: No free rtskb in pool\n");
        kfree_rtskb(rtskb);
        return;
    }

    /* Place the rtskb in the ringbuffer: */
    if (write_to_ringbuffer(&ring_rtskb_rtnet_kernel, rtskb)) {
        /* Switch over to kernel context: */
        rtdm_nrtsig_pend(&rtnetproxy_signal);
    } else {
        /* No space in ringbuffer => Free rtskb here... */
        rtdm_printk("rtnetproxy_recv: No space in queue\n");
        kfree_rtskb(rtskb);
    }
}


/* ************************************************************************
 * This function runs in kernel mode.
 * It is activated from rtnetproxy_signal_handler whenever rtnet received a
 * frame to be processed by rtnetproxy.
 * ************************************************************************ */
static inline void rtnetproxy_kernel_recv(struct rtskb *rtskb)
{
    struct sk_buff *skb;
    struct net_device *dev = dev_rtnetproxy;

    int header_len = rtskb->rtdev->hard_header_len;
    int len        = rtskb->len + header_len;

    /* Copy the realtime skb (rtskb) to the standard skb: */
    skb = dev_alloc_skb(len+2);
    skb_reserve(skb, 2);

    memcpy(skb_put(skb, len), rtskb->data-header_len, len);


    /* Set some relevant entries in the skb: */
    skb->protocol=eth_type_trans(skb,dev);
    skb->dev=dev;
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->pkt_type = PACKET_HOST;  /* Extremely important! Why?!? */

    /* the rtskb stamp is useless (different clock), get new one */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,14)
    __net_timestamp(skb);
#else
    do_gettimeofday(&skb->stamp);
#endif

    dev->last_rx = jiffies;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
    dev->stats.rx_bytes+=skb->len;
    dev->stats.rx_packets++;
#endif

    netif_rx(skb);  /* pass it to the received stuff */

}

/* ************************************************************************
 * This function runs in kernel mode.
 * It is activated from rtnetproxy_recv whenever rtnet received a frame to
 * be processed by rtnetproxy.
 * ************************************************************************ */
static void rtnetproxy_signal_handler(rtdm_nrtsig_t nrtsig, void *arg)
{
    struct rtskb *rtskb;

    while ( (rtskb = read_from_ringbuffer(&ring_rtskb_rtnet_kernel)) != 0) {
        rtnetproxy_kernel_recv(rtskb);
        /* Place "used" rtskb in backqueue... */
        while (0 == write_to_ringbuffer(&ring_rtskb_kernel_rtnet, rtskb))
            rtdm_sem_up(&rtnetproxy_sem);
    }

    /* Signal rtnet that there are "used" rtskbs waiting to be processed...
     * Resume the rtnetproxy_thread to recycle "used" rtskbs
     * */
    rtdm_sem_up(&rtnetproxy_sem);
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

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops rtnetproxy_netdev_ops = {
    .ndo_start_xmit         = rtnetproxy_xmit,
    .ndo_set_multicast_list = set_multicast_list,
};
#endif /* HAVE_NET_DEVICE_OPS */

/* ************************************************************************
 *  device init
 * ************************************************************************ */
static void __init rtnetproxy_init(struct net_device *dev)
{
    /* Fill in device structure with ethernet-generic values. */
    ether_setup(dev);

    dev->tx_queue_len = 0;
#ifdef CONFIG_RTNET_ADDON_PROXY_ARP
    memcpy(dev->dev_addr, rtnetproxy_rtdev->dev_addr, MAX_ADDR_LEN);
#else
    dev->flags |= IFF_NOARP;
#endif
    dev->flags &= ~IFF_MULTICAST;

#ifdef HAVE_NET_DEVICE_OPS
    dev->netdev_ops      = &rtnetproxy_netdev_ops;
#else /* !HAVE_NET_DEVICE_OPS */
    dev->hard_start_xmit = rtnetproxy_xmit;
    dev->set_multicast_list = set_multicast_list;
#ifdef CONFIG_NET_FASTROUTE
    dev->accept_fastpath = rtnetproxy_accept_fastpath;
#endif
#endif /* !HAVE_NET_DEVICE_OPS */
}

/* ************************************************************************
 * ************************************************************************
 *   I N I T
 * ************************************************************************
 * ************************************************************************ */
static int __init rtnetproxy_init_module(void)
{
    int err;

#ifdef CONFIG_RTNET_ADDON_PROXY_ARP
    if ((rtnetproxy_rtdev = rtdev_get_by_name(rtdev_attach)) == NULL) {
	printk("Couldn't attach to %s\n", rtdev_attach);
	return -EINVAL;
    }
    rtdev_dereference(rtnetproxy_rtdev);
    printk("RTproxy attached to %s\n", rtdev_attach);
#endif

    /* Initialize the proxy's rtskb pool (JK) */
    if (rtskb_pool_init(&rtskb_pool, proxy_rtskbs) < proxy_rtskbs) {
        rtskb_pool_release(&rtskb_pool);
        return -ENOMEM;
    }

    dev_rtnetproxy = alloc_netdev(0, "rtproxy", rtnetproxy_init);
    if (!dev_rtnetproxy) {
        rtskb_pool_release(&rtskb_pool);
        return -ENOMEM;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    SET_MODULE_OWNER(dev_rtnetproxy);
#endif

    err = register_netdev(dev_rtnetproxy);
    if (err < 0) {
        rtskb_pool_release(&rtskb_pool);
        return err;
    }

    /* Initialize the ringbuffers: */
    memset(&ring_rtskb_kernel_rtnet, 0, sizeof(ring_rtskb_kernel_rtnet));
    memset(&ring_rtskb_rtnet_kernel, 0, sizeof(ring_rtskb_rtnet_kernel));
    memset(&ring_skb_kernel_rtnet, 0, sizeof(ring_skb_kernel_rtnet));
    memset(&ring_skb_rtnet_kernel, 0, sizeof(ring_skb_rtnet_kernel));

    /* Init the task for transmission */
    rtdm_sem_init(&rtnetproxy_sem, 0);
    rtdm_task_init(&rtnetproxy_thread, "rtnetproxy",
                   rtnetproxy_transmit_thread, 0,
                   RTDM_TASK_LOWEST_PRIORITY, 0);

    /* Register non-real-time signal */
    rtdm_nrtsig_init(&rtnetproxy_signal, rtnetproxy_signal_handler, NULL);

    /* Register with RTnet */
    rt_ip_fallback_handler = rtnetproxy_recv;

    printk("rtnetproxy installed as \"%s\"\n", dev_rtnetproxy->name);

    return 0;
}


static void __exit rtnetproxy_cleanup_module(void)
{
    struct sk_buff *del_skb;  /* standard skb */
    struct rtskb *del; /* rtnet skb */

    /* Unregister the fallback at rtnet */
    rt_ip_fallback_handler = NULL;

    /* free the non-real-time signal */
    rtdm_nrtsig_destroy(&rtnetproxy_signal);

    rtdm_task_destroy(&rtnetproxy_thread);
    rtdm_sem_destroy(&rtnetproxy_sem);

    /* Free the ringbuffers... */
    while ((del_skb = read_from_ringbuffer(&ring_skb_rtnet_kernel)) != 0)
        dev_kfree_skb(del_skb);

    while ((del_skb = read_from_ringbuffer(&ring_skb_kernel_rtnet)) != 0)
        dev_kfree_skb(del_skb);

    while ((del=read_from_ringbuffer(&ring_rtskb_kernel_rtnet))!=0)
        kfree_rtskb(del);

    while ((del=read_from_ringbuffer(&ring_rtskb_rtnet_kernel))!=0)
        kfree_rtskb(del);

    /* Unregister the net device: */
    unregister_netdev(dev_rtnetproxy);

    rtskb_pool_release(&rtskb_pool);
}

module_init(rtnetproxy_init_module);
module_exit(rtnetproxy_cleanup_module);
MODULE_LICENSE("GPL");
