subdirs		:= round_trip_time rtskb_alloc rtt
ifdef CONFIG_RTAI_LXRT
subdirs		+= rtnet_lxrt
endif

ALL_SUB_DIRS	:= $(subdirs)
MOD_SUB_DIRS	:= $(subdirs)
SUB_DIRS	:= $(subdirs)

MOD_LIST_NAME	:=
M_OBJS		+=

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../.. modules SUBDIRS=rtnet/examples
clean:
		make -C ../.. subclean SUBDIRS=rtnet/examples
endif

