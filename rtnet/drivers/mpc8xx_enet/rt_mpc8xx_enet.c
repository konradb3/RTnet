/*
 * BK Id: SCCS/s.enet.c 1.24 01/19/02 03:07:14 dan
 */
/*
 * Ethernet driver for Motorola MPC8xx.
 * Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
 *
 * I copied the basic skeleton from the lance driver, because I did not
 * know how to write the Linux driver, but I did know how the LANCE worked.
 *
 * This version of the driver is somewhat selectable for the different
 * processor/board combinations.  It works for the boards I know about
 * now, and should be easily modified to include others.  Some of the
 * configuration information is contained in <asm/commproc.h> and the
 * remainder is here.
 *
 * Buffer descriptors are kept in the CPM dual port RAM, and the frame
 * buffers are in the host memory.
 *
 * Right now, I am very watseful with the buffers.  I allocate memory
 * pages and then divide them into 2K frame buffers.  This way I know I
 * have buffers large enough to hold one frame within one buffer descriptor.
 * Once I get this working, I will use 64 or 128 byte CPM buffers, which
 * will be much more memory efficient and will easily handle lots of
 * small packets.
 *
 * Ported to RTnet.
 * Copyright (c) 2003 Wolfgang Grandegger (wg@denx.de)
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>

#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/commproc.h>

#include <rtnet_port.h>

MODULE_AUTHOR("Maintainer: Wolfgang Grandegger <wg@denx.de>");
MODULE_DESCRIPTION("RTnet MPC8xx SCC Ethernet driver");
MODULE_LICENSE("GPL");

static unsigned int rx_pool_size =  0;
MODULE_PARM(rx_pool_size, "i");
MODULE_PARM_DESC(rx_pool_size, "Receive buffer pool size");

static unsigned int rtnet_scc = 1; /* SCC1 */
MODULE_PARM(rtnet_scc, "i");
MODULE_PARM_DESC(rtnet_scc, "SCCx port for RTnet, x=1..3 (default=1)");

#define printk(fmt,args...)	rtdm_printk ("RTnet: " fmt ,##args)

#if 0
#define RT_DEBUG(fmt,args...)	rtdm_printk (fmt ,##args)
#else
#define RT_DEBUG(fmt,args...)
#endif

/*
 *				Theory of Operation
 *
 * The MPC8xx CPM performs the Ethernet processing on SCC1.  It can use
 * an aribtrary number of buffers on byte boundaries, but must have at
 * least two receive buffers to prevent constant overrun conditions.
 *
 * The buffer descriptors are allocated from the CPM dual port memory
 * with the data buffers allocated from host memory, just like all other
 * serial communication protocols.  The host memory buffers are allocated
 * from the free page pool, and then divided into smaller receive and
 * transmit buffers.  The size of the buffers should be a power of two,
 * since that nicely divides the page.  This creates a ring buffer
 * structure similar to the LANCE and other controllers.
 *
 * Like the LANCE driver:
 * The driver runs as two independent, single-threaded flows of control.  One
 * is the send-packet routine, which enforces single-threaded use by the
 * cep->tx_busy flag.  The other thread is the interrupt handler, which is
 * single threaded by the hardware and other software.
 *
 * The send packet thread has partial control over the Tx ring and the
 * 'cep->tx_busy' flag.  It sets the tx_busy flag whenever it's queuing a Tx
 * packet. If the next queue slot is empty, it clears the tx_busy flag when
 * finished otherwise it sets the 'lp->tx_full' flag.
 *
 * The MBX has a control register external to the MPC8xx that has some
 * control of the Ethernet interface.  Information is in the manual for
 * your board.
 *
 * The RPX boards have an external control/status register.  Consult the
 * programming documents for details unique to your board.
 *
 * For the TQM8xx(L) modules, there is no control register interface.
 * All functions are directly controlled using I/O pins.  See <asm/commproc.h>.
 */

/* The transmitter timeout
 */
#define TX_TIMEOUT	(2*HZ)

/* The number of Tx and Rx buffers.  These are allocated from the page
 * pool.  The code may assume these are power of two, so it is best
 * to keep them that size.
 * We don't need to allocate pages for the transmitter.  We just use
 * the skbuffer directly.
 */
#define CPM_ENET_RX_PAGES	4
#define CPM_ENET_RX_FRSIZE	2048
#define CPM_ENET_RX_FRPPG	(PAGE_SIZE / CPM_ENET_RX_FRSIZE)
#define RX_RING_SIZE		(CPM_ENET_RX_FRPPG * CPM_ENET_RX_PAGES)
#define TX_RING_SIZE		8	/* Must be power of two */
#define TX_RING_MOD_MASK	7	/*   for this to work */

/* The CPM stores dest/src/type, data, and checksum for receive packets.
 */
#define PKT_MAXBUF_SIZE		1518
#define PKT_MINBUF_SIZE		64
#define PKT_MAXBLR_SIZE		1520

/* The CPM buffer descriptors track the ring buffers.  The rx_bd_base and
 * tx_bd_base always point to the base of the buffer descriptors.  The
 * cur_rx and cur_tx point to the currently available buffer.
 * The dirty_tx tracks the current buffer that is being sent by the
 * controller.  The cur_tx and dirty_tx are equal under both completely
 * empty and completely full conditions.  The empty/ready indicator in
 * the buffer descriptor determines the actual condition.
 */
struct scc_enet_private {
	/* The addresses of a Tx/Rx-in-place packets/buffers. */
	struct rtskb *tx_skbuff[TX_RING_SIZE];
	struct  rtskb_queue skb_pool;
	ushort	skb_cur;
	ushort	skb_dirty;

	/* CPM dual port RAM relative addresses.
	*/
	cbd_t	*rx_bd_base;		/* Address of Rx and Tx buffers. */
	cbd_t	*tx_bd_base;
	cbd_t	*cur_rx, *cur_tx;		/* The next free ring entry */
	cbd_t	*dirty_tx;	/* The ring entries to be free()ed. */
	scc_t	*sccp;

	/* Virtual addresses for the receive buffers because we can't
	 * do a __va() on them anymore.
	 */
	unsigned char *rx_vaddr[RX_RING_SIZE];
	struct	net_device_stats stats;
	uint	tx_full;
	rtdm_lock_t lock;
	rtdm_irq_t irq_handle;
};

static int scc_enet_open(struct rtnet_device *rtdev);
static int scc_enet_start_xmit(struct rtskb *skb, struct rtnet_device *rtdev);
static int scc_enet_rx(struct rtnet_device *rtdev, int *packets, nanosecs_t *time_stamp);
static int scc_enet_interrupt(rtdm_irq_t *irq_handle);
static int scc_enet_close(struct rtnet_device *rtdev);

#ifdef ORIGINAL_VERSION
static struct net_device_stats *scc_enet_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
#endif

#ifndef ORIGINAL_VERSION
static struct rtnet_device *rtdev_root = NULL;
#endif

/* Typically, 860(T) boards use SCC1 for Ethernet, and other 8xx boards
 * use SCC2. Some even may use SCC3.
 * This is easily extended if necessary.
 * These values are set when the driver is initialized.
 */
static int CPM_CR_ENET;
static int PROFF_ENET;
static int SCC_ENET;
static int CPMVEC_ENET;

static int
scc_enet_open(struct rtnet_device *rtdev)
{
        MOD_INC_USE_COUNT;

	/* I should reset the ring buffers here, but I don't yet know
	 * a simple way to do that.
	 */
	rtnetif_start_queue(rtdev);

	return 0;					/* Always succeed */
}

static int
scc_enet_start_xmit(struct rtskb *skb, struct rtnet_device *rtdev)
{
	struct scc_enet_private *cep = (struct scc_enet_private *)rtdev->priv;
	volatile cbd_t	*bdp;
	rtdm_lockctx_t context;


	RT_DEBUG(__FUNCTION__": ...\n");

	/* Fill in a Tx ring entry */
	bdp = cep->cur_tx;

#ifndef final_version
	if (bdp->cbd_sc & BD_ENET_TX_READY) {
		/* Ooops.  All transmit buffers are full.  Bail out.
		 * This should not happen, since cep->tx_busy should be set.
		 */
		rtdm_printk("%s: tx queue full!.\n", rtdev->name);
		return 1;
	}
#endif

	/* Clear all of the status flags.
	 */
	bdp->cbd_sc &= ~BD_ENET_TX_STATS;

	/* If the frame is short, tell CPM to pad it.
	*/
	if (skb->len <= ETH_ZLEN)
		bdp->cbd_sc |= BD_ENET_TX_PAD;
	else
		bdp->cbd_sc &= ~BD_ENET_TX_PAD;

	/* Set buffer length and buffer pointer.
	*/
	bdp->cbd_datlen = skb->len;
	bdp->cbd_bufaddr = __pa(skb->data);

	/* Save skb pointer.
	*/
	cep->tx_skbuff[cep->skb_cur] = skb;

	cep->stats.tx_bytes += skb->len;
	cep->skb_cur = (cep->skb_cur+1) & TX_RING_MOD_MASK;
	
	/* Prevent interrupts from changing the Tx ring from underneath us. */
	// *** RTnet ***
#if 0
	rtdm_irq_disable(&cep->irq_handle);
	rtdm_lock_get(&cep->lock);
#else
	rtdm_lock_get_irqsave(&cep->lock, context);
#endif

	/* Get and patch time stamp just before the transmission */
	if (skb->xmit_stamp)
		*skb->xmit_stamp = cpu_to_be64(rtdm_clock_read() + *skb->xmit_stamp);

	/* Push the data cache so the CPM does not get stale memory
	 * data.
	 */
	flush_dcache_range((unsigned long)(skb->data),
			   (unsigned long)(skb->data + skb->len));


	/* Send it on its way.  Tell CPM its ready, interrupt when done,
	 * its the last BD of the frame, and to put the CRC on the end.
	 */
	bdp->cbd_sc |= (BD_ENET_TX_READY | BD_ENET_TX_INTR | BD_ENET_TX_LAST | BD_ENET_TX_TC);
#if 0
	dev->trans_start = jiffies;
#endif

	/* If this was the last BD in the ring, start at the beginning again.
	*/
	if (bdp->cbd_sc & BD_ENET_TX_WRAP)
		bdp = cep->tx_bd_base;
	else
		bdp++;

	if (bdp->cbd_sc & BD_ENET_TX_READY) {
	        rtnetif_stop_queue(rtdev);
		cep->tx_full = 1;
	}

	cep->cur_tx = (cbd_t *)bdp;

	// *** RTnet ***
#if 0
	rtdm_lock_put(&cep->lock);
	rtdm_irq_enable(&cep->irq_handle);
#else
	rtdm_lock_put_irqrestore(&cep->lock, context);
#endif

	return 0;
}

#ifdef ORIGINAL_VERSION
static void
scc_enet_timeout(struct net_device *dev)
{
	struct scc_enet_private *cep = (struct scc_enet_private *)dev->priv;

	printk("%s: transmit timed out.\n", dev->name);
	cep->stats.tx_errors++;
#ifndef final_version
	{
		int	i;
		cbd_t	*bdp;
		printk(" Ring data dump: cur_tx %p%s cur_rx %p.\n",
		       cep->cur_tx, cep->tx_full ? " (full)" : "",
		       cep->cur_rx);
		bdp = cep->tx_bd_base;
		for (i = 0 ; i < TX_RING_SIZE; i++, bdp++)
			printk("%04x %04x %08x\n",
			       bdp->cbd_sc,
			       bdp->cbd_datlen,
			       bdp->cbd_bufaddr);
		bdp = cep->rx_bd_base;
		for (i = 0 ; i < RX_RING_SIZE; i++, bdp++)
			printk("%04x %04x %08x\n",
			       bdp->cbd_sc,
			       bdp->cbd_datlen,
			       bdp->cbd_bufaddr);
	}
#endif
	if (!cep->tx_full)
		netif_wake_queue(dev);
}
#endif /* ORIGINAL_VERSION */

/* The interrupt handler.
 * This is called from the CPM handler, not the MPC core interrupt.
 */
static int scc_enet_interrupt(rtdm_irq_t *irq_handle)
{
	struct rtnet_device *rtdev = rtdm_irq_get_arg(irq_handle, struct rtnet_device);
	int packets = 0;
	struct	scc_enet_private *cep;
	volatile cbd_t	*bdp;
	ushort	int_events;
	int	must_restart;
	nanosecs_t time_stamp = rtdm_clock_read();


	cep = (struct scc_enet_private *)rtdev->priv;

	/* Get the interrupt events that caused us to be here.
	*/
	int_events = cep->sccp->scc_scce;
	cep->sccp->scc_scce = int_events;
	must_restart = 0;

	/* Handle receive event in its own function.
	*/
	if (int_events & SCCE_ENET_RXF) {
		scc_enet_rx(rtdev, &packets, &time_stamp);
	}

	/* Check for a transmit error.  The manual is a little unclear
	 * about this, so the debug code until I get it figured out.  It
	 * appears that if TXE is set, then TXB is not set.  However,
	 * if carrier sense is lost during frame transmission, the TXE
	 * bit is set, "and continues the buffer transmission normally."
	 * I don't know if "normally" implies TXB is set when the buffer
	 * descriptor is closed.....trial and error :-).
	 */

	/* Transmit OK, or non-fatal error.  Update the buffer descriptors.
	*/
	if (int_events & (SCCE_ENET_TXE | SCCE_ENET_TXB)) {
	    rtdm_lock_get(&cep->lock);
	    bdp = cep->dirty_tx;
	    while ((bdp->cbd_sc&BD_ENET_TX_READY)==0) {
		RT_DEBUG(__FUNCTION__": Tx ok\n");
		if ((bdp==cep->cur_tx) && (cep->tx_full == 0))
		    break;

		if (bdp->cbd_sc & BD_ENET_TX_HB)	/* No heartbeat */
			cep->stats.tx_heartbeat_errors++;
		if (bdp->cbd_sc & BD_ENET_TX_LC)	/* Late collision */
			cep->stats.tx_window_errors++;
		if (bdp->cbd_sc & BD_ENET_TX_RL)	/* Retrans limit */
			cep->stats.tx_aborted_errors++;
		if (bdp->cbd_sc & BD_ENET_TX_UN)	/* Underrun */
			cep->stats.tx_fifo_errors++;
		if (bdp->cbd_sc & BD_ENET_TX_CSL)	/* Carrier lost */
			cep->stats.tx_carrier_errors++;


		/* No heartbeat or Lost carrier are not really bad errors.
		 * The others require a restart transmit command.
		 */
		if (bdp->cbd_sc &
		    (BD_ENET_TX_LC | BD_ENET_TX_RL | BD_ENET_TX_UN)) {
			must_restart = 1;
			cep->stats.tx_errors++;
		}

		cep->stats.tx_packets++;

		/* Deferred means some collisions occurred during transmit,
		 * but we eventually sent the packet OK.
		 */
		if (bdp->cbd_sc & BD_ENET_TX_DEF)
			cep->stats.collisions++;

		/* Free the sk buffer associated with this last transmit.
		*/
		dev_kfree_rtskb(cep->tx_skbuff[cep->skb_dirty]);
		cep->skb_dirty = (cep->skb_dirty + 1) & TX_RING_MOD_MASK;

		/* Update pointer to next buffer descriptor to be transmitted.
		*/
		if (bdp->cbd_sc & BD_ENET_TX_WRAP)
			bdp = cep->tx_bd_base;
		else
			bdp++;

		/* I don't know if we can be held off from processing these
		 * interrupts for more than one frame time.  I really hope
		 * not.  In such a case, we would now want to check the
		 * currently available BD (cur_tx) and determine if any
		 * buffers between the dirty_tx and cur_tx have also been
		 * sent.  We would want to process anything in between that
		 * does not have BD_ENET_TX_READY set.
		 */

		/* Since we have freed up a buffer, the ring is no longer
		 * full.
		 */
		if (cep->tx_full) {
			cep->tx_full = 0;
			if (rtnetif_queue_stopped(rtdev))
				rtnetif_wake_queue(rtdev);
		}

		cep->dirty_tx = (cbd_t *)bdp;
	    }

	    if (must_restart) {
		volatile cpm8xx_t *cp;

		/* Some transmit errors cause the transmitter to shut
		 * down.  We now issue a restart transmit.  Since the
		 * errors close the BD and update the pointers, the restart
		 * _should_ pick up without having to reset any of our
		 * pointers either.
		 */
		cp = cpmp;
		cp->cp_cpcr =
		    mk_cr_cmd(CPM_CR_ENET, CPM_CR_RESTART_TX) | CPM_CR_FLG;
		while (cp->cp_cpcr & CPM_CR_FLG);
	    }
	    rtdm_lock_put(&cep->lock);
	}

	/* Check for receive busy, i.e. packets coming but no place to
	 * put them.  This "can't happen" because the receive interrupt
	 * is tossing previous frames.
	 */
	if (int_events & SCCE_ENET_BSY) {
		cep->stats.rx_dropped++;
		rtdm_printk("CPM ENET: BSY can't happen.\n");
	}

	if (packets > 0)
		rt_mark_stack_mgr(rtdev);
	return RTDM_IRQ_ENABLE;
}

/* During a receive, the cur_rx points to the current incoming buffer.
 * When we update through the ring, if the next incoming buffer has
 * not been given to the system, we just set the empty indicator,
 * effectively tossing the packet.
 */
static int
scc_enet_rx(struct rtnet_device *rtdev, int* packets, nanosecs_t *time_stamp)
{
	struct	scc_enet_private *cep;
	volatile cbd_t	*bdp;
	ushort	pkt_len;
	struct	rtskb *skb;

	RT_DEBUG(__FUNCTION__": ...\n");

	cep = (struct scc_enet_private *)rtdev->priv;

	/* First, grab all of the stats for the incoming packet.
	 * These get messed up if we get called due to a busy condition.
	 */
	bdp = cep->cur_rx;

    for (;;) {

	if (bdp->cbd_sc & BD_ENET_RX_EMPTY)
		break;

#ifndef final_version
	/* Since we have allocated space to hold a complete frame, both
	 * the first and last indicators should be set.
	 */
	if ((bdp->cbd_sc & (BD_ENET_RX_FIRST | BD_ENET_RX_LAST)) !=
		(BD_ENET_RX_FIRST | BD_ENET_RX_LAST))
			printk("CPM ENET: rcv is not first+last\n");
#endif

	/* Frame too long or too short.
	*/
	if (bdp->cbd_sc & (BD_ENET_RX_LG | BD_ENET_RX_SH))
		cep->stats.rx_length_errors++;
	if (bdp->cbd_sc & BD_ENET_RX_NO)	/* Frame alignment */
		cep->stats.rx_frame_errors++;
	if (bdp->cbd_sc & BD_ENET_RX_CR)	/* CRC Error */
		cep->stats.rx_crc_errors++;
	if (bdp->cbd_sc & BD_ENET_RX_OV)	/* FIFO overrun */
		cep->stats.rx_crc_errors++;

	/* Report late collisions as a frame error.
	 * On this error, the BD is closed, but we don't know what we
	 * have in the buffer.  So, just drop this frame on the floor.
	 */
	if (bdp->cbd_sc & BD_ENET_RX_CL) {
		cep->stats.rx_frame_errors++;
	}
	else {

		/* Process the incoming frame.
		*/
		cep->stats.rx_packets++;
		pkt_len = bdp->cbd_datlen;
		cep->stats.rx_bytes += pkt_len;

		/* This does 16 byte alignment, much more than we need.
		 * The packet length includes FCS, but we don't want to
		 * include that when passing upstream as it messes up
		 * bridging applications.
		 */
		skb = dev_alloc_rtskb(pkt_len-4, &cep->skb_pool);
		if (skb == NULL) {
			printk("%s: Memory squeeze, dropping packet.\n", rtdev->name);
			cep->stats.rx_dropped++;
		}
		else {
			skb->rtdev = rtdev;
			rtskb_put(skb,pkt_len-4); /* Make room */
			memcpy(skb->data,
			       (unsigned char *)__va(bdp->cbd_bufaddr),
			       pkt_len-4);
			skb->protocol=rt_eth_type_trans(skb,rtdev);
			skb->time_stamp = *time_stamp;
			rtnetif_rx(skb);
			(*packets)++;
		}
	}

	/* Clear the status flags for this buffer.
	*/
	bdp->cbd_sc &= ~BD_ENET_RX_STATS;

	/* Mark the buffer empty.
	*/
	bdp->cbd_sc |= BD_ENET_RX_EMPTY;

	/* Update BD pointer to next entry.
	*/
	if (bdp->cbd_sc & BD_ENET_RX_WRAP)
		bdp = cep->rx_bd_base;
	else
		bdp++;

    }
	cep->cur_rx = (cbd_t *)bdp;

	return 0;
}

static int
scc_enet_close(struct rtnet_device *rtdev)
{
	/* Don't know what to do yet.
	*/
	rtnetif_stop_queue(rtdev);

	MOD_DEC_USE_COUNT;

	return 0;
}

#ifdef ORIGINAL_VERSION
static struct net_device_stats *scc_enet_get_stats(struct net_device *dev)
{
	struct scc_enet_private *cep = (struct scc_enet_private *)dev->priv;

	return &cep->stats;
}
#endif

#ifdef ORIGINAL_VERSION
/* Set or clear the multicast filter for this adaptor.
 * Skeleton taken from sunlance driver.
 * The CPM Ethernet implementation allows Multicast as well as individual
 * MAC address filtering.  Some of the drivers check to make sure it is
 * a group multicast address, and discard those that are not.  I guess I
 * will do the same for now, but just remove the test if you want
 * individual filtering as well (do the upper net layers want or support
 * this kind of feature?).
 */

static void set_multicast_list(struct net_device *dev)
{
	struct	scc_enet_private *cep;
	struct	dev_mc_list *dmi;
	u_char	*mcptr, *tdptr;
	volatile scc_enet_t *ep;
	int	i, j;
	cep = (struct scc_enet_private *)dev->priv;

	/* Get pointer to SCC area in parameter RAM.
	*/
	ep = (scc_enet_t *)dev->base_addr;

	if (dev->flags&IFF_PROMISC) {
	  
		/* Log any net taps. */
		printk("%s: Promiscuous mode enabled.\n", dev->name);
		cep->sccp->scc_pmsr |= SCC_PMSR_PRO;
	} else {

		cep->sccp->scc_pmsr &= ~SCC_PMSR_PRO;

		if (dev->flags & IFF_ALLMULTI) {
			/* Catch all multicast addresses, so set the
			 * filter to all 1's.
			 */
			ep->sen_gaddr1 = 0xffff;
			ep->sen_gaddr2 = 0xffff;
			ep->sen_gaddr3 = 0xffff;
			ep->sen_gaddr4 = 0xffff;
		}
		else {
			/* Clear filter and add the addresses in the list.
			*/
			ep->sen_gaddr1 = 0;
			ep->sen_gaddr2 = 0;
			ep->sen_gaddr3 = 0;
			ep->sen_gaddr4 = 0;

			dmi = dev->mc_list;

			for (i=0; i<dev->mc_count; i++) {
				
				/* Only support group multicast for now.
				*/
				if (!(dmi->dmi_addr[0] & 1))
					continue;

				/* The address in dmi_addr is LSB first,
				 * and taddr is MSB first.  We have to
				 * copy bytes MSB first from dmi_addr.
				 */
				mcptr = (u_char *)dmi->dmi_addr + 5;
				tdptr = (u_char *)&ep->sen_taddrh;
				for (j=0; j<6; j++)
					*tdptr++ = *mcptr--;

				/* Ask CPM to run CRC and set bit in
				 * filter mask.
				 */
				cpmp->cp_cpcr = mk_cr_cmd(CPM_CR_ENET, CPM_CR_SET_GADDR) | CPM_CR_FLG;
				/* this delay is necessary here -- Cort */
				udelay(10);
				while (cpmp->cp_cpcr & CPM_CR_FLG);
			}
		}
	}
}
#endif /* ORIGINAL_VERSION */

/* Initialize the CPM Ethernet on SCC.  If EPPC-Bug loaded us, or performed
 * some other network I/O, a whole bunch of this has already been set up.
 * It is no big deal if we do it again, we just have to disable the
 * transmit and receive to make sure we don't catch the CPM with some
 * inconsistent control information.
 */
int __init scc_enet_init(void)
{
	struct rtnet_device *rtdev = NULL;
	struct scc_enet_private *cep;
	int i, j, k;
	unsigned char	*eap, *ba;
	dma_addr_t	mem_addr;
	bd_t		*bd;
	volatile	cbd_t		*bdp;
	volatile	cpm8xx_t	*cp;
	volatile	scc_t		*sccp;
	volatile	scc_enet_t	*ep;
	volatile	immap_t		*immap;

	cp = cpmp;	/* Get pointer to Communication Processor */

	immap = (immap_t *)(mfspr(IMMR) & 0xFFFF0000);	/* and to internal registers */

	bd = (bd_t *)__res;

	/* Configure the SCC parameters (this has formerly be done 
	 * by macro definitions).
	 */
	switch (rtnet_scc) {
	case 3:
		CPM_CR_ENET = CPM_CR_CH_SCC3;
		PROFF_ENET  = PROFF_SCC3;
		SCC_ENET    = 2;		/* Index, not number! */
		CPMVEC_ENET = CPMVEC_SCC3;
		break;
	case 2:
		CPM_CR_ENET = CPM_CR_CH_SCC2;
		PROFF_ENET  = PROFF_SCC2;
		SCC_ENET    = 1;		/* Index, not number! */
		CPMVEC_ENET = CPMVEC_SCC2;
		break;
	case 1:
		CPM_CR_ENET = CPM_CR_CH_SCC1;
		PROFF_ENET  = PROFF_SCC1;
		SCC_ENET    = 0;		/* Index, not number! */
		CPMVEC_ENET = CPMVEC_SCC1;
		break;
	default:
		printk(KERN_ERR "enet: SCC%d doesn't exit (check rtnet_scc)\n");
		return -1;
	}

	/* Allocate some private information and create an Ethernet device instance.
	*/
	rtdev = rtdev_root = rt_alloc_etherdev(sizeof(struct scc_enet_private));
	if (rtdev == NULL) {
		printk(KERN_ERR "enet: Could not allocate ethernet device.\n");
		return -1;
	}
	rtdev_alloc_name(rtdev, "rteth%d");
	rt_rtdev_connect(rtdev, &RTDEV_manager);
	RTNET_SET_MODULE_OWNER(rtdev);
	rtdev->vers = RTDEV_VERS_2_0;

	cep = (struct scc_enet_private *)rtdev->priv;
	rtdm_lock_init(&cep->lock);

	/* Get pointer to SCC area in parameter RAM.
	*/
	ep = (scc_enet_t *)(&cp->cp_dparam[PROFF_ENET]);

	/* And another to the SCC register area.
	*/
	sccp = (volatile scc_t *)(&cp->cp_scc[SCC_ENET]);
	cep->sccp = (scc_t *)sccp;		/* Keep the pointer handy */

	/* Disable receive and transmit in case EPPC-Bug started it.
	*/
	sccp->scc_gsmrl &= ~(SCC_GSMRL_ENR | SCC_GSMRL_ENT);

	/* Cookbook style from the MPC860 manual.....
	 * Not all of this is necessary if EPPC-Bug has initialized
	 * the network.
	 * So far we are lucky, all board configurations use the same
	 * pins, or at least the same I/O Port for these functions.....
	 * It can't last though......
	 */

#if (defined(PA_ENET_RXD) && defined(PA_ENET_TXD))
	/* Configure port A pins for Txd and Rxd.
	*/
	immap->im_ioport.iop_papar |=  (PA_ENET_RXD | PA_ENET_TXD);
	immap->im_ioport.iop_padir &= ~(PA_ENET_RXD | PA_ENET_TXD);
	immap->im_ioport.iop_paodr &=                ~PA_ENET_TXD;
#elif (defined(PB_ENET_RXD) && defined(PB_ENET_TXD))
	/* Configure port B pins for Txd and Rxd.
	*/
	immap->im_cpm.cp_pbpar |=  (PB_ENET_RXD | PB_ENET_TXD);
	immap->im_cpm.cp_pbdir &= ~(PB_ENET_RXD | PB_ENET_TXD);
	immap->im_cpm.cp_pbodr &=		 ~PB_ENET_TXD;
#else
#error Exactly ONE pair of PA_ENET_[RT]XD, PB_ENET_[RT]XD must be defined
#endif

#if defined(PC_ENET_LBK)
	/* Configure port C pins to disable External Loopback
	 */
	immap->im_ioport.iop_pcpar &= ~PC_ENET_LBK;
	immap->im_ioport.iop_pcdir |=  PC_ENET_LBK;
	immap->im_ioport.iop_pcso  &= ~PC_ENET_LBK;
	immap->im_ioport.iop_pcdat &= ~PC_ENET_LBK;	/* Disable Loopback */
#endif	/* PC_ENET_LBK */

	/* Configure port C pins to enable CLSN and RENA.
	*/
	immap->im_ioport.iop_pcpar &= ~(PC_ENET_CLSN | PC_ENET_RENA);
	immap->im_ioport.iop_pcdir &= ~(PC_ENET_CLSN | PC_ENET_RENA);
	immap->im_ioport.iop_pcso  |=  (PC_ENET_CLSN | PC_ENET_RENA);

	/* Configure port A for TCLK and RCLK.
	*/
	immap->im_ioport.iop_papar |=  (PA_ENET_TCLK | PA_ENET_RCLK);
	immap->im_ioport.iop_padir &= ~(PA_ENET_TCLK | PA_ENET_RCLK);

	/* Configure Serial Interface clock routing.
	 * First, clear all SCC bits to zero, then set the ones we want.
	 */
	cp->cp_sicr &= ~SICR_ENET_MASK;
	cp->cp_sicr |=  SICR_ENET_CLKRT;

	/* Manual says set SDDR, but I can't find anything with that
	 * name.  I think it is a misprint, and should be SDCR.  This
	 * has already been set by the communication processor initialization.
	 */

	/* Allocate space for the buffer descriptors in the DP ram.
	 * These are relative offsets in the DP ram address space.
	 * Initialize base addresses for the buffer descriptors.
	 */
	i = m8xx_cpm_dpalloc(sizeof(cbd_t) * RX_RING_SIZE);
	ep->sen_genscc.scc_rbase = i;
	cep->rx_bd_base = (cbd_t *)&cp->cp_dpmem[i];

	i = m8xx_cpm_dpalloc(sizeof(cbd_t) * TX_RING_SIZE);
	ep->sen_genscc.scc_tbase = i;
	cep->tx_bd_base = (cbd_t *)&cp->cp_dpmem[i];

	cep->dirty_tx = cep->cur_tx = cep->tx_bd_base;
	cep->cur_rx = cep->rx_bd_base;

	/* Issue init Rx BD command for SCC.
	 * Manual says to perform an Init Rx parameters here.  We have
	 * to perform both Rx and Tx because the SCC may have been
	 * already running.
	 * In addition, we have to do it later because we don't yet have
	 * all of the BD control/status set properly.
	cp->cp_cpcr = mk_cr_cmd(CPM_CR_ENET, CPM_CR_INIT_RX) | CPM_CR_FLG;
	while (cp->cp_cpcr & CPM_CR_FLG);
	 */

	/* Initialize function code registers for big-endian.
	*/
	ep->sen_genscc.scc_rfcr = SCC_EB;
	ep->sen_genscc.scc_tfcr = SCC_EB;

	/* Set maximum bytes per receive buffer.
	 * This appears to be an Ethernet frame size, not the buffer
	 * fragment size.  It must be a multiple of four.
	 */
	ep->sen_genscc.scc_mrblr = PKT_MAXBLR_SIZE;

	/* Set CRC preset and mask.
	*/
	ep->sen_cpres = 0xffffffff;
	ep->sen_cmask = 0xdebb20e3;

	ep->sen_crcec = 0;	/* CRC Error counter */
	ep->sen_alec = 0;	/* alignment error counter */
	ep->sen_disfc = 0;	/* discard frame counter */

	ep->sen_pads = 0x8888;	/* Tx short frame pad character */
	ep->sen_retlim = 15;	/* Retry limit threshold */

	ep->sen_maxflr = PKT_MAXBUF_SIZE;   /* maximum frame length register */
	ep->sen_minflr = PKT_MINBUF_SIZE;  /* minimum frame length register */

	ep->sen_maxd1 = PKT_MAXBLR_SIZE;	/* maximum DMA1 length */
	ep->sen_maxd2 = PKT_MAXBLR_SIZE;	/* maximum DMA2 length */

	/* Clear hash tables.
	*/
	ep->sen_gaddr1 = 0;
	ep->sen_gaddr2 = 0;
	ep->sen_gaddr3 = 0;
	ep->sen_gaddr4 = 0;
	ep->sen_iaddr1 = 0;
	ep->sen_iaddr2 = 0;
	ep->sen_iaddr3 = 0;
	ep->sen_iaddr4 = 0;

	/* Set Ethernet station address.
	 */
	eap = (unsigned char *)&(ep->sen_paddrh);
#ifdef CONFIG_FEC_ENET
	/* We need a second MAC address if FEC is used by Linux */
	for (i=5; i>=0; i--)
		*eap++ = rtdev->dev_addr[i] = (bd->bi_enetaddr[i] | 
					     (i==3 ? 0x80 : 0));
#else
	for (i=5; i>=0; i--)
		*eap++ = rtdev->dev_addr[i] = bd->bi_enetaddr[i];
#endif

	ep->sen_pper = 0;	/* 'cause the book says so */
	ep->sen_taddrl = 0;	/* temp address (LSB) */
	ep->sen_taddrm = 0;
	ep->sen_taddrh = 0;	/* temp address (MSB) */

	/* Now allocate the host memory pages and initialize the
	 * buffer descriptors.
	 */
	bdp = cep->tx_bd_base;
	for (i=0; i<TX_RING_SIZE; i++) {

		/* Initialize the BD for every fragment in the page.
		*/
		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	bdp = cep->rx_bd_base;
	k = 0;
	for (i=0; i<CPM_ENET_RX_PAGES; i++) {

		/* Allocate a page.
		*/
		ba = (unsigned char *)consistent_alloc(GFP_KERNEL, PAGE_SIZE, &mem_addr);

		/* Initialize the BD for every fragment in the page.
		*/
		for (j=0; j<CPM_ENET_RX_FRPPG; j++) {
			bdp->cbd_sc = BD_ENET_RX_EMPTY | BD_ENET_RX_INTR;
			bdp->cbd_bufaddr = mem_addr;
			cep->rx_vaddr[k++] = ba;
			mem_addr += CPM_ENET_RX_FRSIZE;
			ba += CPM_ENET_RX_FRSIZE;
			bdp++;
		}
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* Let's re-initialize the channel now.  We have to do it later
	 * than the manual describes because we have just now finished
	 * the BD initialization.
	 */
	cp->cp_cpcr = mk_cr_cmd(CPM_CR_ENET, CPM_CR_INIT_TRX) | CPM_CR_FLG;
	while (cp->cp_cpcr & CPM_CR_FLG);

	cep->skb_cur = cep->skb_dirty = 0;

	sccp->scc_scce = 0xffff;	/* Clear any pending events */

	/* Enable interrupts for transmit error, complete frame
	 * received, and any transmit buffer we have also set the
	 * interrupt flag.
	 */
	sccp->scc_sccm = (SCCE_ENET_TXE | SCCE_ENET_RXF | SCCE_ENET_TXB);

	/* Install our interrupt handler.
	*/
	rtdev->irq = CPM_IRQ_OFFSET + CPMVEC_ENET;
	rt_stack_connect(rtdev, &STACK_manager);
	if ((i = rtdm_irq_request(&cep->irq_handle, rtdev->irq,
				  scc_enet_interrupt, 0, "rt_mpc8xx_enet", rtdev))) {
		printk(KERN_ERR "Couldn't request IRQ %d\n", rtdev->irq);
		rtdev_free(rtdev);
		return i;
	}
	rtdm_irq_enable(&cep->irq_handle);
	

	/* Set GSMR_H to enable all normal operating modes.
	 * Set GSMR_L to enable Ethernet to MC68160.
	 */
	sccp->scc_gsmrh = 0;
	sccp->scc_gsmrl = (SCC_GSMRL_TCI | SCC_GSMRL_TPL_48 | SCC_GSMRL_TPP_10 | SCC_GSMRL_MODE_ENET);

	/* Set sync/delimiters.
	*/
	sccp->scc_dsr = 0xd555;

	/* Set processing mode.  Use Ethernet CRC, catch broadcast, and
	 * start frame search 22 bit times after RENA.
	 */
	sccp->scc_pmsr = (SCC_PMSR_ENCRC | SCC_PMSR_NIB22);

	/* It is now OK to enable the Ethernet transmitter.
	 * Unfortunately, there are board implementation differences here.
	 */
#if   (!defined (PB_ENET_TENA) &&  defined (PC_ENET_TENA))
	immap->im_ioport.iop_pcpar |=  PC_ENET_TENA;
	immap->im_ioport.iop_pcdir &= ~PC_ENET_TENA;
#elif ( defined (PB_ENET_TENA) && !defined (PC_ENET_TENA))
	cp->cp_pbpar |= PB_ENET_TENA;
	cp->cp_pbdir |= PB_ENET_TENA;
#else
#error Configuration Error: define exactly ONE of PB_ENET_TENA, PC_ENET_TENA
#endif

#if defined(CONFIG_RPXLITE) || defined(CONFIG_RPXCLASSIC)
	/* And while we are here, set the configuration to enable ethernet.
	*/
	*((volatile uint *)RPX_CSR_ADDR) &= ~BCSR0_ETHLPBK;
	*((volatile uint *)RPX_CSR_ADDR) |=
			(BCSR0_ETHEN | BCSR0_COLTESTDIS | BCSR0_FULLDPLXDIS);
#endif

#ifdef CONFIG_BSEIP
	/* BSE uses port B and C for PHY control.
	*/
	cp->cp_pbpar &= ~(PB_BSE_POWERUP | PB_BSE_FDXDIS);
	cp->cp_pbdir |= (PB_BSE_POWERUP | PB_BSE_FDXDIS);
	cp->cp_pbdat |= (PB_BSE_POWERUP | PB_BSE_FDXDIS);

	immap->im_ioport.iop_pcpar &= ~PC_BSE_LOOPBACK;
	immap->im_ioport.iop_pcdir |= PC_BSE_LOOPBACK;
	immap->im_ioport.iop_pcso &= ~PC_BSE_LOOPBACK;
	immap->im_ioport.iop_pcdat &= ~PC_BSE_LOOPBACK;
#endif

#ifdef CONFIG_FADS
	cp->cp_pbpar |= PB_ENET_TENA;
	cp->cp_pbdir |= PB_ENET_TENA;

	/* Enable the EEST PHY.
	*/
	*((volatile uint *)BCSR1) &= ~BCSR1_ETHEN;
#endif

	rtdev->base_addr = (unsigned long)ep;

	/* The CPM Ethernet specific entries in the device structure. */
	rtdev->open = scc_enet_open;
	rtdev->hard_start_xmit = scc_enet_start_xmit;
	rtdev->stop = scc_enet_close;
	rtdev->hard_header = &rt_eth_header;

	if (!rx_pool_size)
		rx_pool_size = RX_RING_SIZE * 2;
	if (rtskb_pool_init(&cep->skb_pool, rx_pool_size) < rx_pool_size) {
		rtdm_irq_disable(&cep->irq_handle);
		rtdm_irq_free(&cep->irq_handle);
		rtskb_pool_release(&cep->skb_pool);
		rtdev_free(rtdev);
		return -ENOMEM;
	}

	if ((i = rt_register_rtnetdev(rtdev))) {
		printk(KERN_ERR "Couldn't register rtdev\n");
		rtdm_irq_disable(&cep->irq_handle);
		rtdm_irq_free(&cep->irq_handle);
		rtskb_pool_release(&cep->skb_pool);
		rtdev_free(rtdev);
		return i;
	}

	/* And last, enable the transmit and receive processing.
	*/
	sccp->scc_gsmrl |= (SCC_GSMRL_ENR | SCC_GSMRL_ENT);

	printk("%s: CPM ENET Version 0.2 on SCC%d, irq %d, addr %02x:%02x:%02x:%02x:%02x:%02x\n", 
	       rtdev->name, SCC_ENET+1, rtdev->irq,
	       rtdev->dev_addr[0], rtdev->dev_addr[1], rtdev->dev_addr[2],
	       rtdev->dev_addr[3], rtdev->dev_addr[4], rtdev->dev_addr[5]);
	
	return 0;
}

static void __exit scc_enet_cleanup(void)
{
	struct rtnet_device *rtdev = rtdev_root;
	struct scc_enet_private *cep = (struct scc_enet_private *)rtdev->priv;
	volatile cpm8xx_t *cp = cpmp;
	volatile scc_enet_t *ep;

	if (rtdev) {
		rtdm_irq_disable(&cep->irq_handle);
		rtdm_irq_free(&cep->irq_handle);

		ep = (scc_enet_t *)(&cp->cp_dparam[PROFF_ENET]);
		m8xx_cpm_dpfree(ep->sen_genscc.scc_rbase);
		m8xx_cpm_dpfree(ep->sen_genscc.scc_tbase);

		rt_stack_disconnect(rtdev);
		rt_unregister_rtnetdev(rtdev);
		rt_rtdev_disconnect(rtdev);

		printk("%s: unloaded\n", rtdev->name);
		rtskb_pool_release(&cep->skb_pool);
		rtdev_free(rtdev);
		rtdev_root = NULL;
	}
}

module_init(scc_enet_init);
module_exit(scc_enet_cleanup);
