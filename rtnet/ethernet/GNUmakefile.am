noinst_LIBRARIES = libeth.a

libeth_a_SOURCES = eth.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: eth.o

