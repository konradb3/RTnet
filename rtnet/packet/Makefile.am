CROSS_COMPILE = @CROSS_COMPILE@

noinst_LIBRARIES = libpacket.a

libpacket_a_SOURCES = \
	af_packet.c

packet.o: libpacket.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: packet.o

clean-local:
	rm -rf .extract
