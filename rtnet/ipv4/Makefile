O_TARGET 	:= ipv4.o
MI_OBJS  	:= ipv4.o

IPv4_OBJS	:= arp.o ip_input.o ip_fragment.o ip_output.o af_inet.o protocol.o udp.o route.o icmp.o 

EXTRA_CFLAGS	:= -I../include

O_OBJS		:= $(IPv4_OBJS)

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C ../.. modules SUBDIRS=rtnet/ipv4
clean:
		make -C ../.. subclean SUBDIRS=rtnet/ipv4
endif

