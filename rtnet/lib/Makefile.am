noinst_LIBRARIES = libcrc32.a

libcrc32_a_SOURCES = crc32.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: crc32.o
