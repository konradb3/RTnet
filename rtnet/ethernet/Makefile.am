noinst_LIBRARIES = libeth.a

libeth_a_SOURCES = eth.c

INCLUDES = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: eth.o

