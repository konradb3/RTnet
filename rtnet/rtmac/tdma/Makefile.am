moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

SUBDIRS =

noinst_LIBRARIES = libtdma.a

libtdma_a_SOURCES = \
	tdma_module.c \
	tdma_cleanup.c \
	tdma_event.c \
	tdma_ioctl.c \
	tdma_rx.c \
	tdma_task.c \
	tdma_timer.c

tdma.o: libtdma.a
	$(LD) --whole-archive $< -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: tdma.o

install-exec-local: tdma.o
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
