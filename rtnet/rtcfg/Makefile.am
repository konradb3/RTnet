moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

noinst_LIBRARIES = librtcfg.a

librtcfg_a_SOURCES = \
	rtcfg_module.c \
	rtcfg_event.c \
	rtcfg_conn_event.c \
	rtcfg_ioctl.c \
	rtcfg_frame.c \
	rtcfg_timer.c \
	rtcfg_file.c

AM_CFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

rtcfg.o: librtcfg.a
	$(LD) --whole-archive $< -r -o $@

all-local: rtcfg.o

install-exec-local: rtcfg.o
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
