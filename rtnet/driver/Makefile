ifeq ($(CONFIG_PCI), y)
subdirs		= tulip
endif

ALL_SUB_DIRS	:= $(subdirs)
MOD_SUB_DIRS	:= $(subdirs)
SUB_DIRS	:= 

M_OBJS		+= loopback-rt.o
ifeq ($(CONFIG_PCI), y)
M_OBJS		+= 8139too-rt.o eepro100-rt.o 3c59x-rt.o
endif
ifeq ($(CONFIG_RTAI_RTNET_SCC_ENET), y)
M_OBJS		+= mpc8xx_enet-rt.o
endif
ifeq ($(CONFIG_RTAI_RTNET_FEC_ENET), y)
M_OBJS		+= mpc8xx_fec-rt.o
endif
ifeq ($(CONFIG_RTAI_RTNET_FCC_ENET), y)
M_OBJS		+= mpc8260_fcc_enet-rt.o
endif
MI_OBJS		:=
MIX_OBJS	+= 

EXTRA_CFLAGS	:= -I../lib -I../include -I../rtmac/include

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
	make -C ../.. modules SUBDIRS=rtnet/driver
clean:
	make -C ../.. subclean SUBDIRS=rtnet/driver
endif



