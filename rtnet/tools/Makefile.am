OPTPROGS =

if CONFIG_RTNET_RTCFG
OPTPROGS += rtcfg
endif

sbin_PROGRAMS = rtifconfig $(OPTPROGS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include

all-local: rtifconfig $(OPTPROGS)

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	$(INSTALL_SCRIPT) rtnet $(DESTDIR)$(sbindir)
