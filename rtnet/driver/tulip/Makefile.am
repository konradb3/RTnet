CROSS_COMPILE = @CROSS_COMPILE@

noinst_LIBRARIES = libtulip-rt.a

libtulip_rt_a_SOURCES = \
	tulip_core.c \
	eeprom.c \
	interrupt.c \
	media.c \
	timer.c \
	21142.c \
	pnic.c \
	pnic2.c

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib \
	@RTAI_KMOD_CFLAGS@

tulip-rt.o: libtulip-rt.a
	$(LD) --whole-archive $< -r -o $@

all-local: tulip-rt.o
