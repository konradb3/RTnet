moduledir = $(DESTDIR)/@RTNET_MODULE_DIR@

OPTDIRS =

if CONFIG_RTNET_TULIP
OPTDIRS += tulip
endif

SUBDIRS = $(OPTDIRS)

EXTRA_LIBRARIES = \
	lib3c59x-rt.a \
	lib8139too-rt.a \
	libeepro100-rt.a \
	libpcnet32-rt.a \
	libvia-rhine-rt.a \
	libnatsemi-rt.a \
	libloopback-rt.a \
	libmpc8260_fcc_enet-rt.a \
	libmpc8xx_enet-rt.a \
	libmpc8xx_fec-rt.a \
	libsmc91111-rt.a

OBJS =

if  CONFIG_RTNET_3C59X
lib3c59x_rt_a_SOURCES = 3c59x-rt.c
OBJS += 3c59x-rt.o
endif

if CONFIG_RTNET_8139
lib8139too_rt_a_SOURCES = 8139too-rt.c
OBJS += 8139too-rt.o
endif

if CONFIG_RTNET_EEPRO100
libeepro100_rt_a_SOURCES = eepro100-rt.c
OBJS += eepro100-rt.o
endif

if CONFIG_RTNET_PCNET32
libpcnet32_rt_a_SOURCES = pcnet32-rt.c
OBJS += pcnet32-rt.o
endif

if CONFIG_RTNET_VIA_RHINE
libvia_rhine_rt_a_SOURCES = via-rhine-rt.c
OBJS += via-rhine-rt.o
endif

if CONFIG_RTNET_NATSEMI
libnatsemi_rt_a_SOURCES = natsemi-rt.c
OBJS += natsemi-rt.o
endif

if CONFIG_RTNET_LOOPBACK
libloopback_rt_a_SOURCES = loopback-rt.c
OBJS += loopback-rt.o
endif

if CONFIG_RTNET_FCC_ENET
libmpc8260_fcc_enet_rt_a_SOURCES = mpc8260_fcc_enet-rt.c
OBJS += mpc8260_fcc_enet-rt.o
endif

if CONFIG_RTNET_SCC_ENET
libmpc8xx_enet_rt_a_SOURCES = mpc8xx_enet-rt.c
OBJS += mpc8xx_enet-rt.o
endif

if CONFIG_RTNET_FEC_ENET
libmpc8xx_fec_rt_a_SOURCES = mpc8xx_fec-rt.c
OBJS += mpc8xx_fec-rt.o
endif

if CONFIG_RTNET_SMC91111
libsmc91111_rt_a_SOURCES = smc91111-rt.c
OBJS += smc91111-rt.o
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_builddir)/include \
	-I$(top_srcdir)/lib \
	@RTAI_KMOD_CFLAGS@

all-local: $(OBJS)

install-exec-local: $(OBJS)
	$(mkinstalldirs) $(moduledir)
	for FILE in $^; do					\
		$(INSTALL_DATA) $$FILE $(moduledir);		\
	done
