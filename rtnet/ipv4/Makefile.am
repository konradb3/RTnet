CROSS_COMPILE = @CROSS_COMPILE@

noinst_LIBRARIES = libipv4.a

libipv4_a_SOURCES = \
	af_inet.c \
	arp.c \
	icmp.c \
	ip_fragment.c \
	ip_input.c \
	ip_output.c \
	ip_sock.c \
	protocol.c \
	route.c \
	udp.c

ipv4.o: libipv4.a
	$(LD) --whole-archive $< -r -o $@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@RTAI_KMOD_CFLAGS@

all-local: ipv4.o