subdirs		:= examples

ALL_SUB_DIRS	:= $(subdirs)
MOD_SUB_DIRS	:= $(subdirs)
SUB_DIRS	:= 

M_OBJS		+= rtmac.o
MI_OBJS		:= rtmac_module.o rtmac_chrdev.o rtmac_proc.o rtmac_disc.o rtmac_tdma.o tdma_ioctl.o tdma_event.o tdma_timer.o tdma_task.o tdma_rx.o tdma_cleanup.o
MIX_OBJS	+= 

EXTRA_CFLAGS	:= -I../lib -I../include -I../rtmac/include -g

rtmac-objs	:= rtmac_module.o rtmac_chrdev.o rtmac_proc.o rtmac_disc.o rtmac_tdma.o tdma_ioctl.o tdma_event.o tdma_timer.o tdma_task.o tdma_rx.o tdma_cleanup.o


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
	make -C ../.. modules SUBDIRS=rtnet/rtmac
clean:
	make -C ../.. subclean SUBDIRS=rtnet/rtmac
endif


rtmac.o:	$(MI_OBJS) $($MIX_OBJS)
		$(LD) -r -o $@ $(rtmac-objs)
