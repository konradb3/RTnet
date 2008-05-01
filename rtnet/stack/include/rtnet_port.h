/* include/rtnet_port.h
 *
 * RTnet - real-time networking subsystem
 * Copyright (C) 2003      Wittawat Yamwong
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
#ifndef __RTNET_PORT_H_
#define __RTNET_PORT_H_

#ifdef __KERNEL__

#include <linux/bitops.h>
#include <linux/moduleparam.h>

#include <rtdev.h>
#include <rtdev_mgr.h>
#include <rtnet_sys.h>
#include <stack_mgr.h>
#include <ethernet/eth.h>


#ifndef compat_pci_register_driver
# if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#  define compat_pci_register_driver(drv) \
	(pci_register_driver(drv) <= 0 ? -EINVAL : 0)
# else
#  define compat_pci_register_driver(drv) \
	pci_register_driver(drv)
# endif
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
# define pci_dma_sync_single_for_device     pci_dma_sync_single
# define pci_dma_sync_single_for_cpu        pci_dma_sync_single
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
# define kmem_cache                         kmem_cache_s
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#include <linux/pci.h>
/* only matches directly on vendor and device ID */
static inline int pci_dev_present(const struct pci_device_id *ids)
{
	while (ids->vendor) {
		if (pci_find_device(ids->vendor, ids->device, NULL))
			return 1;
		ids++;
	}
	return 0;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,8)
# define proc_dointvec(a, b, c, d, e, f)    proc_dointvec(a, b, c, d, e)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,10)
# define compat_pci_restore_state(a, b)     pci_restore_state(a, b)
# define compat_pci_save_state(a, b)        pci_save_state(a, b)
# define compat_module_int_param_array(name, count) \
    MODULE_PARM(name, "1-" __MODULE_STRING(count) "i")
#else
# define compat_pci_restore_state(a, b)     pci_restore_state(a)
# define compat_pci_save_state(a, b)        pci_save_state(a)
# define compat_module_int_param_array(name, count) \
    module_param_array(name, int, NULL, 0444)
#endif

static inline void rtnetif_start_queue(struct rtnet_device *rtdev)
{
    clear_bit(__LINK_STATE_XOFF, &rtdev->state);
}

static inline void rtnetif_wake_queue(struct rtnet_device *rtdev)
{
    if (test_and_clear_bit(__LINK_STATE_XOFF, &rtdev->state))
    /*TODO __netif_schedule(dev); */ ;
}

static inline void rtnetif_stop_queue(struct rtnet_device *rtdev)
{
    set_bit(__LINK_STATE_XOFF, &rtdev->state);
}

static inline int rtnetif_queue_stopped(struct rtnet_device *rtdev)
{
    return test_bit(__LINK_STATE_XOFF, &rtdev->state);
}

static inline int rtnetif_running(struct rtnet_device *rtdev)
{
    return test_bit(__LINK_STATE_START, &rtdev->state);
}

static inline void rtnetif_carrier_on(struct rtnet_device *rtdev)
{
    clear_bit(__LINK_STATE_NOCARRIER, &rtdev->state);
    /*
    if (netif_running(dev))
        __netdev_watchdog_up(dev);
    */
}

static inline void rtnetif_carrier_off(struct rtnet_device *rtdev)
{
    set_bit(__LINK_STATE_NOCARRIER, &rtdev->state);
}

static inline int rtnetif_carrier_ok(struct rtnet_device *rtdev)
{
    return !test_bit(__LINK_STATE_NOCARRIER, &rtdev->state);
}


#endif /* __KERNEL__ */

#endif /* __RTNET_PORT_H_ */
