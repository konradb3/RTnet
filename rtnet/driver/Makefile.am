OPTDIRS =

if CONFIG_RTNET_TULIP
OPTDIRS += tulip
endif

SUBDIRS = $(OPTDIRS)

EXTRA_LIBRARIES = \
	lib3c59x-rt.a \
	lib8139too-rt.a \
	libeepro100-rt.a \
	libloopback-rt.a \
	libmpc8260_fcc_enet-rt.a \
	libmpc8xx_enet-rt.a \
	libmpc8xx_fec-rt.a

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

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib \
	@RTAI_KMOD_CFLAGS@

all-local: $(OBJS)
