subdirs		:= tools driver ipv4 ethernet lib examples rtmac

ALL_SUB_DIRS	:= $(subdirs)
MOD_SUB_DIRS	:= $(subdirs)
SUB_DIRS	:= ipv4 ethernet lib

M_OBJS 		:= rtnet.o rtnet_lxrt.o
MI_OBJS      	:= stack_mgr.o rtdev_mgr.o rtnet_module.o rtskb.o rtnet_dev.o socket.o rtdev.o iovec.o rtnet_init.o
MIX_OBJS 	:= rtnet_syms.o

EXTRA_CFLAGS 	:= -I./lib -I./include -I./rtmac/include -g

rtnet-objs   	:= $(MI_OBJS) $(MIX_OBJS) $(join $(SUB_DIRS),$(SUB_DIRS:%=/%.o))


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:
		make -C .. modules SUBDIRS=rtnet
clean:
		make -C .. subclean SUBDIRS=rtnet
endif


rtnet.o: 	$(MI_OBJS) $(MIX_OBJS) $(patsubst %,_modsubdir_%,$(MOD_SUB_DIRS))
		$(LD) -r -o $@ $(rtnet-objs)

tags:
		etags ../*/*/*/*/*.[ch] ../*/*/*/*.[ch] ../*/*/*.[ch] ../*/*.[ch]
