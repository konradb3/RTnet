A_TARG := rtifconfig

EXTRA_USER_CFLAGS	:= -I../include -I../rtmac/include

ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
	make -C ../.. modules SUBDIRS=rtnet/tools
clean:
	make -C ../.. subclean SUBDIRS=rtnet/tools
endif

