EXTRA_USER_CFLAGS	:= -I../../../lxrt -I../../include
EXTRA_USER_LIBS		:= -llxrt

A_TARG			:= client linux_server

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
	make -C ../../.. modules SUBDIRS=rtnet/examples/rtnet_lxrt
clean:
	make -C ../../..subclean SUBDIRS=rtnet/examples/rtnet_lxrt
endif

