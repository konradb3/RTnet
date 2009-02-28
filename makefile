KCONFIG_DIR=./scripts/kconfig

ifneq ($(MAKECMDGOALS),help)
ifeq ($(rtnet_srctree),)
ifneq ($(MAKEFILE_LIST),)
# Since 3.80, we can find out which Makefile is currently processed,
# and infere the location of the source tree using MAKEFILE_LIST.
rtnet_srctree := $(dir $(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST)))
else
ifeq ($(rtnet_srctree),)
rtnet_srctree := $(shell test -d config && pwd)
ifeq ($(rtnet_srctree),)
$(error Please specify the location of your source tree: make rtnet_srctree=...)
endif
endif
endif
endif
build_alias := $(shell $(rtnet_srctree)/config/autoconf/config.guess)
host_alias := $(shell  $(rtnet_srctree)/scripts/arch2host.sh $(ARCH) $(build_alias))
override ARCH := $(shell echo $(host_alias)|cut -f1 -d-|sed -e s/^i.86/i386/ -e s/^powerpc/ppc/)
endif

ifeq ($(MAKECMDGOALS),)
confgui = $(shell test $(TERM) = emacs && echo xconfig || echo menuconfig)
forceconf = $(shell test -r .rtnet_config -a -r .cfok || echo $(confgui))
endif

override rtnet_srctree := $(shell cd $(rtnet_srctree) && pwd)

all:: $(forceconf)
	@/bin/true

xconfig: qconf config-script

gconfig: gconf config-script

mconfig menuconfig: mconf config-script

config: conf config-script

oldconfig: oldconf config-script

config-script: config.status

reconfig::
	@touch .rtnet_config

reconfig:: config-script

config.status: .rtnet_config
	@test -r config.status && recf=yes || recf=no ; \
	eval `grep ^CONFIG_RTNET_INSTALLDIR $<`; \
	$(rtnet_srctree)/configure \
	CROSS_COMPILE=$(CROSS_COMPILE) \
	CC="$(CROSS_COMPILE)$(CC)" \
	CXX="$(CROSS_COMPILE)$(CXX)" \
	LD="$(CROSS_COMPILE)$(LD)" \
	AR="$(CROSS_COMPILE)$(AR)" \
	RANLIB=$(CROSS_COMPILE)ranlib \
	STRIP=$(CROSS_COMPILE)strip \
	NM=$(CROSS_COMPILE)nm \
	--build=$(build_alias) \
	--host=$(host_alias) \
	--with-kconfig-file=$< \
	--prefix=$$CONFIG_RTNET_INSTALLDIR; \
	if test $$? = 0; then \
	   touch .cfok ; \
	   if test x$$recf = xyes ; then \
	      touch .cfchanged ; \
	   fi ; \
	else \
	   rm -f .cfok ; false; \
	fi

qconf: $(KCONFIG_DIR)
	@$(MAKE) -C $(KCONFIG_DIR) \
	-f $(rtnet_srctree)/$(KCONFIG_DIR)/Makefile.kconfig xconfig \
	srctree=$(rtnet_srctree) ARCH=$(ARCH)

gconf: $(KCONFIG_DIR)
	@$(MAKE) -C $(KCONFIG_DIR) \
	-f $(rtnet_srctree)/$(KCONFIG_DIR)/Makefile.kconfig gconfig \
	srctree=$(rtnet_srctree) ARCH=$(ARCH)

mconf: $(KCONFIG_DIR)
	@$(MAKE) -C $(KCONFIG_DIR) \
	-f $(rtnet_srctree)/$(KCONFIG_DIR)/Makefile.kconfig menuconfig \
	srctree=$(rtnet_srctree) ARCH=$(ARCH)

conf: $(KCONFIG_DIR)
	@$(MAKE) -C $(KCONFIG_DIR) \
	-f $(rtnet_srctree)/$(KCONFIG_DIR)/Makefile.kconfig config \
	srctree=$(rtnet_srctree) ARCH=$(ARCH)

oldconf: $(KCONFIG_DIR)
	@$(MAKE) -C $(KCONFIG_DIR) \
	-f $(rtnet_srctree)/$(KCONFIG_DIR)/Makefile.kconfig oldconfig \
	srctree=$(rtnet_srctree) ARCH=$(ARCH)

defconfig: $(KCONFIG_DIR)
	@$(MAKE) -C $(KCONFIG_DIR) \
	-f $(rtnet_srctree)/$(KCONFIG_DIR)/Makefile.kconfig defconfig \
	srctree=$(rtnet_srctree) ARCH=$(ARCH)

$(KCONFIG_DIR):
	@test -d $@ || mkdir -p $@

help:
	@echo ; echo "This is RTnet's build bootstrapping Makefile. In order to build RTnet," ; \
	echo 'you first need to configure it. Proceed as follows:' ; \
	echo ; echo '$$ cd $$your_builddir' ; echo ; \
	echo '# Configuration using a KDE-based, GTK-based or Dialog-based GUI' ; \
	echo '$$ make -f $$rtnet_srcdir/makefile {xconfig,gconfig,menuconfig}' ; \
	echo '                            OR,' ; \
	echo '# Configuration using a text-based interface' ; \
	echo '$$ make -f $$rtnet_srcdir/makefile {config,oldconfig}' ; \
	echo ; echo 'In case a configuration error occurs, re-run the command above to fix the' ; \
	echo 'faulty parameter. Once the configuration is successful, type:' ; echo ; \
	echo '$$ make [all]' ; \
	echo ; echo "To change the configuration from now on, simply run:"; echo ; \
	echo '$$ make {xconfig,gconfig,menuconfig,config}' ; echo

clean distclean:
	if test -r $(KCONFIG_DIR)/Makefile.kconfig ; then \
	$(MAKE) -C $(KCONFIG_DIR) -f Makefile.kconfig clean ; fi
	if test -r GNUmakefile ; then \
	$(MAKE) -f GNUmakefile $@ ; else \
	$(MAKE) -C $(KCONFIG_DIR) -f $(rtnet_srctree)/$(KCONFIG_DIR)/Makefile.kconfig clean ; \
	fi
	@find . -name autom4te.cache | xargs rm -fr

all %::
	@if test -r GNUmakefile ; then \
	$(MAKE) -f GNUmakefile $@ ; else \
	echo "*** Please configure RTnet first (running 'make help' in RTnet's toplevel dir might help)." ; \
	exit 1; fi

.PHONY: config-script reconfig xconfig gconfig mconfig menuconfig config oldconfig qconf mconf conf clean distclean help
