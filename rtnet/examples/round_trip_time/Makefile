subdirs		:= client server 
ALL_SUB_DIRS	:= $(subdirs)
MOD_SUB_DIRS	:= $(subdirs)
SUB_DIRS	:= $(subdirs) 

EXTRA_CFLAGS	:=

MOD_LIST_NAME	:= MISC_MODULES

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../../.. modules SUBDIRS=rtnet/examples/round_trip_time
clean:
		make -C ../../.. subclean SUBDIRS=rtnet/examples/round_trip_time
endif
