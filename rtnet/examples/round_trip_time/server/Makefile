ALL_SUB_DIRS	:= 
MOD_SUB_DIRS	:= 
SUB_DIRS	:= 

EXTRA_CFLAGS	:= -I../../../include

MOD_LIST_NAME	:= MISC_MODULES
MI_OBJS		+= rt_server.o 


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../../../.. modules SUBDIRS=rtnet/examples/round_trip_time/server
clean:
		make -C ../../../.. subclean SUBDIRS=rtnet/examples/round_trip_time/server
endif
