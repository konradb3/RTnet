noinst_LIBRARIES = librt_client.a
noinst_PROGRAMS = showtime

librt_client_a_SOURCES = rt_client.c

librt_client_a_CFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

rt_client.o: librt_client.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(AR) x ../$<
	$(LD) -r -o $@ .extract/*.o

showtime_LDADD = $(NCURSES_LIBS)

all-local: rt_client.o showtime

clean-local:
	rm -rf .extract