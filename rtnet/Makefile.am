moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

SUBDIRS = driver ethernet examples ipv4 lib packet rtcfg rtmac tools

noinst_LIBRARIES = librtnet.a libnetproxy.a

EXTRA_LIBRARIES = librtnet_lxrt.a

librtnet_a_SOURCES = \
	iovec.c \
	rtdev.c \
	rtdev_mgr.c \
	rtnet_chrdev.c \
	rtnet_module.c \
	rtnet_rtpc.c \
	rtnet_syms.c \
	rtskb.c \
	socket.c \
	stack_mgr.c

libnetproxy_a_SOURCES = rtnetproxy.c

RTNET_BUILTIN_OBJS = \
	ethernet/eth.o \
	ipv4/ipv4.o \
	lib/crc32.o \
	packet/packet.o

OBJS =

if CONFIG_NEWLXRT
librtnet_lxrt_a_SOURCES = rtnet_lxrt.c
OBJS += rtnet_lxrt.o
endif

rtnet.o: librtnet.a $(RTNET_BUILTIN_OBJS)
	$(LD) --whole-archive $^ -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rtnet.o rtnetproxy.o $(OBJS)

install-exec-local: rtnet.o rtnetproxy.o $(OBJS)
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
