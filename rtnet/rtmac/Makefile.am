moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

SUBDIRS = examples

noinst_LIBRARIES = librtmac.a

librtmac_a_SOURCES = \
	rtmac_chrdev.c \
	rtmac_disc.c \
	rtmac_module.c \
	rtmac_proc.c \
	rtmac_proto.c \
	rtmac_tdma.c \
	rtmac_vnic.c \
	tdma_cleanup.c \
	tdma_event.c \
	tdma_ioctl.c \
	tdma_rx.c \
	tdma_task.c \
	tdma_timer.c

rtmac.o: librtmac.a
	$(LD) --whole-archive $< -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rtmac.o

install-exec-local: rtmac.o
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
