noinst_LIBRARIES = librt_client.a librt_client_parport.a librt_server.a
# noinst_PROGRAMS = showtime

librt_client_a_SOURCES = rt_client.c

librt_client_parport_a_SOURCES = rt_client_parport.c

librt_server_a_SOURCES = rt_server.c

# librt_client_a_INCLUDES = \
# 	@RTAI_KMOD_CFLAGS@ \
# 	-I$(top_srcdir)/include
# 	-I$(top_srcidr)/rtmac/include

# librt_client_parport_a_INCLUDES = \
# 	@RTAI_KMOD_CFLAGS@ \
# 	-I$(top_srcdir)/include
# 	-I$(top_srcidr)/rtmac/include

# librt_server_a_INCLUDES = \
# 	@RTAI_KMOD_CFLAGS@ \
# 	-I$(top_srcdir)/include
# 	-I$(top_srcidr)/rtmac/include

INCLUDES = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/rtmac/include

all-local: rt_client.o rt_client_parport.o rt_server.o #showtime