A_TARG		:= 

ALL_SUB_DIRS	:= 
MOD_SUB_DIRS	:= 
SUB_DIRS	:= 

EXTRA_CFLAGS	:= -I../../include -I../../../include

MOD_LIST_NAME	:= MISC_MODULES
MI_OBJS		+= rt_event.o rt_show_events.o


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../../../.. modules SUBDIRS=rtnet/rtmac/examples/event
clean:
		make -C ../../../.. subclean SUBDIRS=rtnet/rtmac/examples/event
endif
