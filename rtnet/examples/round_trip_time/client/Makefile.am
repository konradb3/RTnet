noinst_LIBRARIES = librt_client.a
noinst_PROGRAMS = showtime

librt_client_a_SOURCES = rt_client.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: showtime rt_client.o
