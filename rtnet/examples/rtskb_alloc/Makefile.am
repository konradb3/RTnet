noinst_LIBRARIES = librtskb_alloc.a

librtskb_alloc_a_SOURCES = rtskb_alloc.c

AM_CPPFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: rtskb_alloc.o