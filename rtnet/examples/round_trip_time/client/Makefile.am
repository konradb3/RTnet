noinst_LIBRARIES = librt_client.a
noinst_PROGRAMS = showtime

librt_client_a_SOURCES = rt_client.c

INCLUDES = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: showtime rt_client.o