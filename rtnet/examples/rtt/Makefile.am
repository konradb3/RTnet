CROSS_COMPILE = @CROSS_COMPILE@

noinst_LIBRARIES = librt_client.a librt_client_parport.a librt_server.a
noinst_PROGRAMS = linux_server showtime

librt_client_a_SOURCES = rt_client.c
librt_client_parport_a_SOURCES = rt_client_parport.c
librt_server_a_SOURCES = rt_server.c


librt_client_a_CFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

librt_client_parport_a_CFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

librt_server_a_CFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include


rt_client.o: librt_client.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o

rt_client_parport.o: librt_client_parport.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o

rt_server.o: librt_server.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o


all-local: rt_client.o rt_client_parport.o rt_server.o linux_server showtime

clean-local:
	rm -rf .extract