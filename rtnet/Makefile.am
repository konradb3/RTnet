SUBDIRS = driver ethernet examples ipv4 lib rtmac tools

noinst_LIBRARIES = librtnet.a libnetproxy.a

librtnet_a_SOURCES = iovec.c \
		rtdev.c \
		rtdev_mgr.c \
		rtnet_dev.c \
		rtnet_init.c \
		rtnet_module.c \
		rtnet_syms.c \
		rtskb.c \
		socket.c \
		stack_mgr.c

RTNET_BUILTIN_OBJS = ethernet/eth.o \
		ipv4/ipv4.o \
		lib/crc32.o

libnetproxy_a_SOURCES = rtnetproxy.c

rtnet.o: librtnet.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(AR) x ../$<
	$(LD) -r -o $@ .extract/*.o $(RTNET_BUILTIN_OBJS)

INCLUDES = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/rtmac/include

all-local: rtnet.o rtnetproxy.o
