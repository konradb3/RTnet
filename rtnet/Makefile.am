moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

OPTDIRS =

if CONFIG_RTNET_RTCAP
OPTDIRS += rtcap
endif

if CONFIG_RTNET_RTCFG
OPTDIRS += rtcfg
endif

SUBDIRS = driver ethernet examples ipv4 lib packet rtmac tools include $(OPTDIRS)

noinst_LIBRARIES = librtnet.a

EXTRA_LIBRARIES = librtnet_lxrt.a libnetproxy.a

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

if CONFIG_RTNET_PROXY
libnetproxy_a_SOURCES = rtnetproxy.c
OBJS += rtnetproxy.o
endif

rtnet.o: librtnet.a $(RTNET_BUILTIN_OBJS)
	$(LD) --whole-archive $^ -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rtnet.o $(OBJS)

install-exec-local: rtnet.o $(OBJS)
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
