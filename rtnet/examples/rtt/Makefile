A_TARG		:= showtime linux_server

EXTRA_CFLAGS	:= -I../../include -I../../../include

MOD_LIST_NAME	:= MISC_MODULES
MI_OBJS		+= rt_client.o rt_server.o rt_client_parport.o


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../../.. modules SUBDIRS=rtnet/examples/rtt
clean:
		make -C ../../.. subclean SUBDIRS=rtnet/examples/rtt
endif
