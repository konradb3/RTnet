noinst_LIBRARIES = libnetshm.a libnetshm_kerndemo.a

libnetshm_a_SOURCES = netshm.c
libnetshm_kerndemo_a_SOURCES = netshm_kerndemo.c


libnetshm_a_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

libnetshm_kerndemo_a_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@


netshm.o: libnetshm.a
	$(LD) --whole-archive $< -r -o $@

netshm_kerndemo.o: libnetshm_kerndemo.a
	$(LD) --whole-archive $< -r -o $@

all-local: netshm.o netshm_kerndemo.o
