noinst_LIBRARIES = libcrc32.a

libcrc32_a_SOURCES = crc32.c

AM_CPPFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: crc32.o
