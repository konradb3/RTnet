all:
	@echo $(RTEXT_LINUX_DIR) $(ARCH)
	$(MAKE) -s -C $(RTEXT_LINUX_DIR) CC=$(CC) $(RTEXT_LINUX_DIR)/include/linux/modversions.h
	$(MAKE) -s -C $(RTEXT_LINUX_DIR) CC=$(CC) ARCH=$(ARCH) SUBDIRS=$(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi) modules
                                                                                
modules:
	@echo RTEXT_KMOD_CFLAGS="\"$(CFLAGS)\""

