noinst_LIBRARIES = libfrag_ip.a

libfrag_ip_a_SOURCES = frag_ip.c


libfrag_ip_a_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@


frag_ip.o: libfrag_ip.a
	$(LD) --whole-archive $< -r -o $@

all-local: frag_ip.o