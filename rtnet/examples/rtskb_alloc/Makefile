ALL_SUB_DIRS	:= 
MOD_SUB_DIRS	:= 
SUB_DIRS	:= 

EXTRA_CFLAGS	:= -I../../include

MOD_LIST_NAME	:= MISC_MODULES
MI_OBJS		+= rtskb_alloc.o 


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../../.. modules SUBDIRS=rtnet/examples/rtskb_alloc
clean:
		make -C ../../.. subclean SUBDIRS=rtnet/examples/rtskb_alloc
endif







