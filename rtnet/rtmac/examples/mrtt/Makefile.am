noinst_LIBRARIES = librt_client.a
noinst_PROGRAMS = showtime

librt_client_a_SOURCES = rt_client.c

librt_client_a_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

rt_client.o: librt_client.a
	$(LD) --whole-archive $< -r -o $@

showtime_LDADD = $(NCURSES_LIBS)

all-local: rt_client.o showtime
