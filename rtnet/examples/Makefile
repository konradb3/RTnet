subdirs		:= round_trip_time rtnet_lxrt rtskb_alloc rtt

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

