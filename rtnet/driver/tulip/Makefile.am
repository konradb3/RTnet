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
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include \
	-I$(top_srcdir)/lib

tulip-rt.o: libtulip-rt.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o

all-local: tulip-rt.o

clean-local:
	rm -rf .extract