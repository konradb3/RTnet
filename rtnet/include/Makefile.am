nobase_include_HEADERS = \
		rtnet.h \
		rtmac.h

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(includedir)
	$(INSTALL_HEADER) rtnet_config.h $(DESTDIR)$(includedir)/rtnet_config.h

DISTCLEANFILES = rtnet_config.h
