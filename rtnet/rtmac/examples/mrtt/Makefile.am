noinst_LIBRARIES = librt_client.a
# noinst_PROGRAMS = showtime

librt_client_a_SOURCES = rt_client.c

# librt_client_a_INCLUDES = \
# 	@RTAI_KMOD_CFLAGS@ \
# 	-I$(top_srcdir)/include
# 	-I$(top_srcidr)/rtmac/include

INCLUDES = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include
	-I$(top_srcidr)/rtmac/include

all-local: rt_client.o #showtime