subdirs		:= rtt mrtt

ALL_SUB_DIRS	:= $(subdirs)
MOD_SUB_DIRS	:= $(subdirs)
SUB_DIRS	:= $(subdirs)

MOD_LIST_NAME	:=
M_OBJS		+=

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../../.. modules SUBDIRS=rtnet/rtmac/examples
clean:
		make -C ../../.. subclean SUBDIRS=rtnet/rtmac/examples
endif

