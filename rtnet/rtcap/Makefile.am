moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

noinst_LIBRARIES = librtcap.a

librtcap_a_SOURCES = rtcap.c

AM_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rtcap.o

install-exec-local: rtcap.o
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
