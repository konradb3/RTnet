CROSS_COMPILE = @CROSS_COMPILE@

noinst_LIBRARIES = libfrag_ip.a

libfrag_ip_a_SOURCES = frag_ip.c


libfrag_ip_a_CFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@


frag_ip.o: libfrag_ip.a
	rm -rf .extract && mkdir .extract
	cd .extract && $(CROSS_COMPILE)$(AR) x ../$<
	$(CROSS_COMPILE)$(LD) -r -o $@ .extract/*.o


all-local: frag_ip.o

clean-local:
	rm -rf .extract
