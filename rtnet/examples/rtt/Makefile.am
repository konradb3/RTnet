noinst_LIBRARIES = librt_client.a librt_client_parport.a librt_server.a
noinst_PROGRAMS = linux_server showtime

librt_client_a_SOURCES = rt_client.c
librt_client_parport_a_SOURCES = rt_client_parport.c
librt_server_a_SOURCES = rt_server.c


librt_client_a_CFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

librt_client_parport_a_CFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

librt_server_a_CFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@


rt_client.o: librt_client.a
	$(LD) --whole-archive $< -r -o $@

rt_client_parport.o: librt_client_parport.a
	$(LD) --whole-archive $< -r -o $@

rt_server.o: librt_server.a
	$(LD) --whole-archive $< -r -o $@

all-local: rt_client.o rt_client_parport.o rt_server.o linux_server showtime
