SUBDIRS = examples

noinst_LIBRARIES = librtmac.a

librtmac_a_SOURCES = \
	rtmac_chrdev.c \
	rtmac_disc.c \
	rtmac_module.c \
	rtmac_proc.c \
	rtmac_proto.c \
	rtmac_tdma.c \
	tdma_cleanup.c \
	tdma_event.c \
	tdma_ioctl.c \
	tdma_rx.c \
	tdma_task.c \
	tdma_timer.c

rtmac.o: librtmac.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(AR) x ../$<
	$(LD) -r -o $@ .extract/*.o

AM_CPPFLAGS = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: rtmac.o

clean-local:
	rm -rf .extract