#ifndef _RTNET_PORT_H_
#define _RTNET_PORT_H_

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

#endif
