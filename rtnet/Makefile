subdirs		:= tools driver ipv4 ethernet lib examples rtmac

ALL_SUB_DIRS	:= $(subdirs)
MOD_SUB_DIRS	:= $(subdirs)
SUB_DIRS	:= ipv4 ethernet lib

M_OBJS 		:= rtnet.o rtnetproxy.o
ifdef CONFIG_RTAI_LXRT
M_OBJS 		+= rtnet_lxrt.o
endif
MI_OBJS      	:= stack_mgr.o rtdev_mgr.o rtnet_module.o rtskb.o rtnet_dev.o socket.o rtdev.o iovec.o rtnet_init.o 
MIX_OBJS 	:= rtnet_syms.o

EXTRA_CFLAGS 	:= -I./lib -I./include -I./rtmac/include -g

rtnet-objs   	:= $(MI_OBJS) $(MIX_OBJS) $(join $(SUB_DIRS),$(SUB_DIRS:%=/%.o))


ifdef TOPDIR
include $(TOPDIR)/Rules.make
else
all:		clean	# absolutely weird, but unfortunately necessary because of broken dependencies, so if you are bothered by it please do not just delete this but fix the dependencies; patches are always welcome =:)
		make -C .. modules SUBDIRS=rtnet
clean:
		make -C .. subclean SUBDIRS=rtnet
endif


rtnet.o: 	$(MI_OBJS) $(MIX_OBJS) $(patsubst %,_modsubdir_%,$(MOD_SUB_DIRS))
		$(LD) -r -o $@ $(rtnet-objs)

tags:
		etags ../*/*/*/*/*.[ch] ../*/*/*/*.[ch] ../*/*/*.[ch] ../*/*.[ch]
