noinst_LIBRARIES = librt_server.a

librt_server_a_SOURCES = rt_server.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rt_server.o
