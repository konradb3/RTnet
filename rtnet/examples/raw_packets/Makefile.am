CROSS_COMPILE = @CROSS_COMPILE@

noinst_LIBRARIES = libraw_packets.a

libraw_packets_a_SOURCES = raw_packets.c


libraw_packets_a_CFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@


raw_packets.o: libraw_packets.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o


all-local: raw_packets.o

clean-local:
	rm -rf .extract
