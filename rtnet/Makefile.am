SUBDIRS = driver ethernet examples ipv4 lib packet rtmac tools

noinst_LIBRARIES = librtnet.a libnetproxy.a libnetlxrt.a

librtnet_a_SOURCES = iovec.c \
		rtdev.c \
		rtdev_mgr.c \
		rtnet_dev.c \
		rtnet_module.c \
		rtnet_syms.c \
		rtskb.c \
		socket.c \
		stack_mgr.c

RTNET_BUILTIN_OBJS = ethernet/eth.o \
		ipv4/ipv4.o \
		lib/crc32.o \
		packet/packet.o

libnetproxy_a_SOURCES = rtnetproxy.c

libnetlxrt_a_SOURCES = rtnet_lxrt.c

rtnet.o: librtnet.a $(RTNET_BUILTIN_OBJS)
	$(LD) --whole-archive $^ -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rtnet.o rtnetproxy.o rtnet_lxrt.o
