A_TARG		:= showtime

ALL_SUB_DIRS	:= 
MOD_SUB_DIRS	:= 
SUB_DIRS	:= 

EXTRA_CFLAGS	:= -I../../include -I../../../include

MOD_LIST_NAME	:= MISC_MODULES
MI_OBJS		+= rt_client.o


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../../../.. modules SUBDIRS=rtnet/rtmac/examples/mrtt
clean:
		make -C ../../../.. subclean SUBDIRS=rtnet/rtmac/examples/mrtt
endif
