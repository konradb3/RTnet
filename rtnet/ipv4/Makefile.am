noinst_LIBRARIES = libipv4.a

libipv4_a_SOURCES = af_inet.c \
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
	rm -rf .extract && mkdir .extract
	cd .extract && $(AR) x ../$<
	$(LD) -r -o $@ .extract/*.o

INCLUDES = \
	@RTAI_KMOD_CFLAGS@ \
	-I$(top_srcdir)/include

all-local: ipv4.o

clean-local:
	rm -rf .extract