noinst_LIBRARIES = libpacket.a

libpacket_a_SOURCES = \
	af_packet.c

packet.o: libpacket.a
	$(LD) --whole-archive $< -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: packet.o
