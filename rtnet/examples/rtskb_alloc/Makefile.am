noinst_LIBRARIES = librtskb_alloc.a

librtskb_alloc_a_SOURCES = rtskb_alloc.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rtskb_alloc.o
