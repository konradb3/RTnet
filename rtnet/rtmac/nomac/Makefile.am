moduledir = $(DESTDIR)@RTNET_MODULE_DIR@

SUBDIRS =

noinst_LIBRARIES = libnomac.a

libnomac_a_SOURCES = \
	nomac_dev.c \
	nomac_ioctl.c \
	nomac_module.c \
	nomac_proto.c

nomac.o: libnomac.a
	$(LD) --whole-archive $< -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: nomac.o

install-exec-local: nomac.o
	$(mkinstalldirs) $(moduledir)
	$(INSTALL_DATA) $^ $(moduledir)
