CROSS_COMPILE = @CROSS_COMPILE@

noinst_LIBRARIES = librt_client.a
noinst_PROGRAMS = showtime

librt_client_a_SOURCES = rt_client.c

librt_client_a_CFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

rt_client.o: librt_client.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o

showtime_LDADD = $(NCURSES_LIBS)

all-local: rt_client.o showtime

clean-local:
	rm -rf .extract
