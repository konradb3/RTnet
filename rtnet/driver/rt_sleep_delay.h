#define rt_sleep_delay(delay) {\
	while ( ((oneshot_timer ? rdtsc(): rt_times.tick_time) + delay)>rt_time_h ) {\
		rt_schedule();\
	}\
