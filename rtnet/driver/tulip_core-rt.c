/* tulip_core-rt.c: A DEC 21x4x-family ethernet driver for Linux. */

/*
        Maintained by Jeff Garzik <jgarzik@mandrakesoft.com>
        Copyright 2000,2001  The Linux Kernel Team
        Written/copyright 1994-2001 by Donald Becker.

        This software may be used and distributed according to the terms
        of the GNU General Public License, incorporated herein by reference.

        Please refer to Documentation/DocBook/tulip.{pdf,ps,html}
        for more information on this driver, or visit the project
        Web page at http://sourceforge.net/projects/tulip/

*/

#define DRV_NAME        "tulip"
#define DRV_VERSION     "1.1.0"
#define DRV_RELDATE     "Dec 11, 2001"

#include <linux/config.h>
#include <linux/module.h>
#include "tulip.h"
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <asm/unaligned.h>
#include <asm/uaccess.h>

#include <crc32.h>
#include <rtnet.h>


/***
 * Set the bus performance register.
 *       Typical: Set 16 longword cache alignment, no burst limit.
 *       Cache alignment bits 15:14           Burst length 13:8
 *               0000    No  alignment  0x00000000 unlimited              0800 8 longwords
 *               4000    8   longwords            0100 1 longword         1000 16 longwords
 *               8000    16  longwords            0200 2 longwords        2000 32 longwords
 *               C000    32  longwords           0400 4 longwords
 *       Warning: many older 486 systems are broken and require setting 0x00A04800
 *          8 longword cache alignment, 8 longword burst.
 *       ToDo: Non-Intel setting could be better.
 */
#if defined(__alpha__) || defined(__ia64__)
static int csr0 = 0x01A00000 | 0xE000;
#elif defined(__i386__) || defined(__powerpc__)
static int csr0 = 0x01A00000 | 0x8000;
#elif defined(__arm__) || defined(__sh__)
static int csr0 = 0x01A00000 | 0x4800;
#else
#warning Processor architecture undefined!
static int csr0 = 0x00A00000 | 0x4800;
#endif




/***
 * This table use during operation for capabilities and media timer.
 *
 * It is indexed via the values in 'enum chips'
 */
struct tulip_chip_table tulip_tbl[] = {
  { }, /* placeholder for array, slot unused currently */
  { }, /* placeholder for array, slot unused currently */

  /* DC21140 */
  { "Digital DS21140 Tulip", 128, 0x0001ebef,
        HAS_MII | HAS_MEDIA_TABLE | CSR12_IN_SROM | HAS_PCI_MWI, tulip_timer },

  /* DC21142, DC21143 */
  { "Digital DS21143 Tulip", 128, 0x0801fbff,
        HAS_MII | HAS_MEDIA_TABLE | ALWAYS_CHECK_MII | HAS_ACPI | HAS_NWAY
        | HAS_INTR_MITIGATION | HAS_PCI_MWI, t21142_timer },

  /* LC82C168 */
  { "Lite-On 82c168 PNIC", 256, 0x0001fbef,
        HAS_MII | HAS_PNICNWAY, pnic_timer },

  /* MX98713 */
  { "Macronix 98713 PMAC", 128, 0x0001ebef,
        HAS_MII | HAS_MEDIA_TABLE | CSR12_IN_SROM, mxic_timer },

  /* MX98715 */
  { "Macronix 98715 PMAC", 256, 0x0001ebef,
        HAS_MEDIA_TABLE, mxic_timer },

  /* MX98725 */
  { "Macronix 98725 PMAC", 256, 0x0001ebef,
        HAS_MEDIA_TABLE, mxic_timer },

  /* AX88140 */
  { "ASIX AX88140", 128, 0x0001fbff,
        HAS_MII | HAS_MEDIA_TABLE | CSR12_IN_SROM | MC_HASH_ONLY
        | IS_ASIX, tulip_timer },

  /* PNIC2 */
  { "Lite-On PNIC-II", 256, 0x0801fbff,
        HAS_MII | HAS_NWAY | HAS_8023X | HAS_PCI_MWI, pnic2_timer },

  /* COMET */
  { "ADMtek Comet", 256, 0x0001abef,
        MC_HASH_ONLY | COMET_MAC_ADDR, comet_timer },

  /* COMPEX9881 */
  { "Compex 9881 PMAC", 128, 0x0001ebef,
        HAS_MII | HAS_MEDIA_TABLE | CSR12_IN_SROM, mxic_timer },

  /* I21145 */
  { "Intel DS21145 Tulip", 128, 0x0801fbff,
        HAS_MII | HAS_MEDIA_TABLE | ALWAYS_CHECK_MII | HAS_ACPI
        | HAS_NWAY | HAS_PCI_MWI, t21142_timer },

  /* DM910X */
  { "Davicom DM9102/DM9102A", 128, 0x0001ebef,
        HAS_MII | HAS_MEDIA_TABLE | CSR12_IN_SROM | HAS_ACPI,
        tulip_timer },
};

static struct pci_device_id tulip_pci_tbl[] __devinitdata = {
        { 0x1011, 0x0009, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DC21140 },
        { 0x1011, 0x0019, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DC21143 },
        { 0x11AD, 0x0002, PCI_ANY_ID, PCI_ANY_ID, 0, 0, LC82C168 },
        { 0x10d9, 0x0512, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MX98713 },
        { 0x10d9, 0x0531, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MX98715 },
/*      { 0x10d9, 0x0531, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MX98725 },*/
        { 0x125B, 0x1400, PCI_ANY_ID, PCI_ANY_ID, 0, 0, AX88140 },
        { 0x11AD, 0xc115, PCI_ANY_ID, PCI_ANY_ID, 0, 0, PNIC2 },
        { 0x1317, 0x0981, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x1317, 0x0985, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x1317, 0x1985, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x13D1, 0xAB02, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x13D1, 0xAB03, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x104A, 0x0981, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x104A, 0x2774, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x11F6, 0x9881, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMPEX9881 },
        { 0x8086, 0x0039, PCI_ANY_ID, PCI_ANY_ID, 0, 0, I21145 },
        { 0x1282, 0x9100, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DM910X },
        { 0x1282, 0x9102, PCI_ANY_ID, PCI_ANY_ID, 0, 0, DM910X },
        { 0x1113, 0x1216, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { 0x1113, 0x1217, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MX98715 },
        { 0x1113, 0x9511, PCI_ANY_ID, PCI_ANY_ID, 0, 0, COMET },
        { } /* terminate list */
};
MODULE_DEVICE_TABLE(pci, tulip_pci_tbl);




static int __devinit 
rt_tulip_init_one (struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct rtnet_device *rtdev;

        static int board_idx	= -1;
        int chip_idx		= ent->driver_data;

        struct tulip_private *tp;
        /* See note below on the multiport cards. */
        static unsigned char last_phys_addr[6] = {0x00, 'L', 'i', 'n', 'u', 'x'};
        static int last_irq;
        static int multiport_cnt;       /* For four-port boards w/one EEPROM */
        u8 chip_rev;
        int i, irq;
        unsigned short sum;
        u8 ee_data[EEPROM_SIZE];
        struct net_device *dev;
        long ioaddr;
        unsigned int t2104x_mode = 0;
        unsigned int eeprom_missing = 0;
        unsigned int force_csr0 = 0;


	/***
         *	Lan media wire a tulip chip to a wan interface. 
	 *	Needs a very different driver (lmc driver)
         */
	if (pdev->subsystem_vendor == PCI_VENDOR_ID_LMC) {
		printk("RTnet: skipping LMC card.\n");
		return -ENODEV;
	}

        if (pdev->vendor == 0x1282 && pdev->device == 0x9100)
        {
                u32 dev_rev;
                /* Read Chip revision */
                pci_read_config_dword(pdev, PCI_REVISION_ID, &dev_rev);
                if(dev_rev < 0x02000030) {
                        printk("RTnet: skipping early DM9100 with Crc bug (use dmfe)\n");
                        return -ENODEV;
                }
        }

        /*
         *      Looks for early PCI chipsets where people report hangs
         *      without the workarounds being on.
         */

        /* Intel Saturn. Switch to 8 long words burst, 8 long word cache aligned
           Aries might need this too. The Saturn errata are not pretty reading but
           thankfully its an old 486 chipset.
        */
        if (pci_find_device(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82424, NULL)) {
                csr0 = MRL | MRM | (8 << BurstLenShift) | (1 << CALShift);
                force_csr0 = 1;
        }

        /* The dreaded SiS496 486 chipset. Same workaround as above. */
        if (pci_find_device(PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_496, NULL)) {
                csr0 = MRL | MRM | (8 << BurstLenShift) | (1 << CALShift);
                force_csr0 = 1;
        }

        /* bugfix: the ASIX must have a burst limit or horrible things happen. */
        if (chip_idx == AX88140) {
                if ((csr0 & 0x3f00) == 0)
                        csr0 |= 0x2000;
        }

        /* PNIC doesn't have MWI/MRL/MRM... */
        if (chip_idx == LC82C168)
                csr0 &= ~0xfff10000; /* zero reserved bits 31:20, 16 */

        /* DM9102A has troubles with MRM & clear reserved bits 24:22, 20, 16, 7:1 */
        if (pdev->vendor == 0x1282 && pdev->device == 0x9102)
                csr0 &= ~0x01f100ff;

	
	if ( (i=pci_enable_device(pdev)) ) {
                printk ("RTnet: Cannot enable tulip board #%d, aborting\n", board_idx);
                return i;
        }
        ioaddr = pci_resource_start (pdev, 0);
        irq = pdev->irq;

	rtdev=rt_dev_alloc(sizeof(struct tulip_private));

}






