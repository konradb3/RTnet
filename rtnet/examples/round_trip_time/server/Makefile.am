noinst_LIBRARIES = librt_server.a

librt_server_a_SOURCES = rt_server.c

AM_CPPFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: rt_server.o