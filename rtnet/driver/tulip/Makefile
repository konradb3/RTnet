#
# Makefile for the Tulip ethernet driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...


EXTRA_CFLAGS := -I. -I../../include -g

M_OBJS += tulip-rt.o

#tulip_objs := tulip_core.o eeprom.o interrupt.o media.o \
#	   timer.o 		\
#	   21142.o pnic.o pnic2.o
tulip_objs := tulip_core.o eeprom.o interrupt.o media.o \
	   timer.o 		\
	   21142.o pnic.o pnic2.o


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else

all:
	make -C .. modules SUBDIRS=rtnet/driver/tulip
clean:
	make -C .. subclean SUBDIRS=rtnet/driver/tulip
depend:
	make -C .. depend SUBDIRS=rtnet/driver/tulip
all-dep: depend all
endif

tulip-rt.o: $(tulip_objs)
	$(LD) -r -o $@ $(tulip_objs)
