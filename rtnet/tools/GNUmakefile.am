OPTPROGS =

if CONFIG_RTNET_RTCFG
OPTPROGS += rtcfg
endif

if CONFIG_RTNET_NOMAC
OPTPROGS += nomaccfg
endif

sbin_PROGRAMS = rtifconfig rtroute rtping $(OPTPROGS)

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include

all-local: rtifconfig rtroute rtping $(OPTPROGS)

install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(sbindir)
	$(INSTALL_SCRIPT) $(top_srcdir)/tools/rtnet $(DESTDIR)$(sbindir)
