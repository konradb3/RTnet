noinst_LIBRARIES = librt_event.a librt_show_events.a

librt_event_a_SOURCES = rt_event.c

librt_show_events_a_SOURCES = rt_show_events.c

AM_CPPFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: rt_event.o rt_show_events.o