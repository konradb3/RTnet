/*
 * showtime - implement char device for user space communication
 * Copyright (C) 2002, Ulrich Marx <marx@fet.uni-hannover.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <unistd.h>

typedef unsigned int u32;
struct mrtt_rx_packet {
	long long	rx;
	long long	tx;
	u32		ip_addr;
};

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]


static int end = 0;

static void endme (int dummy) { end = 1; }

int main(int argc,char *argv[])
{
	struct mrtt_rx_packet rx_packet;
        int cmd0;
	

	signal (SIGINT, endme);
          
        if ((cmd0 = open("/dev/rtf0", O_RDONLY)) < 0) {
		if ((cmd0 = open("/dev/rtf/0", O_RDONLY)) < 0) {
                	fprintf(stderr, "Error opening /dev/rtf/0 and /dev/rtf0\n");
			exit(1);
		}
        }

        while(!end) {
                read (cmd0, &rx_packet, sizeof(struct mrtt_rx_packet));
		
		fprintf(stdout, "IP: %u.%u.%u.%u\t rtt: %lld us\n", NIPQUAD(rx_packet.ip_addr), (rx_packet.rx - rx_packet.tx)/1000);
		//fprintf (stdout, "Roundtrip = %lld us\n", (rx_time.l-tx_time.l)/1000);
        }

	close(cmd0);

        return 0;
}






