moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

OPTDIRS =

if CONFIG_RTNET_NOMAC
OPTDIRS += nomac
endif

SUBDIRS = tdma $(OPTDIRS)

noinst_LIBRARIES = librtmac.a

librtmac_a_SOURCES = \
	rtmac_disc.c \
	rtmac_module.c \
	rtmac_proc.c \
	rtmac_proto.c \
	rtmac_syms.c \
	rtmac_vnic.c

rtmac.o: librtmac.a
	$(LD) --whole-archive $< -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: rtmac.o

install-exec-local: rtmac.o
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
