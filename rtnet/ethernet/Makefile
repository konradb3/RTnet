O_TARGET 	:= ethernet.o
MI_OBJS  	:= ethernet.o

EXTRA_CFLAGS    := -DDEBUG -I../include

OBJS		:= eth.o

O_OBJS		:= $(OBJS)

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../.. modules SUBDIRS=rtnet/ethernet
clean:
		make -C ../.. subclean SUBDIRS=rtnet/ethernet
endif




