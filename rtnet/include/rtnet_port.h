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

#include <rtdev.h>
#include <rtdev_mgr.h>
#include <rtnet_sys.h>
#include <stack_mgr.h>
#include <ethernet/eth.h>


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
