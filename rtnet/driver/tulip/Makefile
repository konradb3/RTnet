
MI_OBJS += 	21142.o eeprom.o interrupt.o media.o pnic.i pnic2.o timer.o\
		tulip_core.o 


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
	make -C ../../.. modules SUBDIRS=rtnet/driver/tulip
clean:
	make -C ../../.. subclean SUBDIRS=rtnet/driver/tulip
endif

