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
#include <string.h>
#include <unistd.h>

#include <curses.h>

typedef unsigned int u32;
struct mrtt_rx_packet {
	long long	rx;
	long long	tx;
	u32		ip_addr;
};

struct stats {
	u32		ip;
	int		rtt,min,max;
	int		count;
};

#define MAX_STATIONS 100
struct stats stations[MAX_STATIONS];

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]


static int end = 0;

static void endme (int dummy) { end = 1; }

static struct stats *lookup(u32 ip)
{
	int i;

	for (i=0; i < MAX_STATIONS; i++) {
		if (stations[i].ip == ip)
			break;
		if (stations[i].ip == 0) {
			stations[i].ip = ip;
			stations[i].min = 10000000;
			break;
		}
	}
	if (i == MAX_STATIONS)
		return NULL;
	return &stations[i];
}

int main(int argc,char *argv[])
{
	struct mrtt_rx_packet rx_packet;
        int cmd0;
	struct stats *stats;

	memset(stations, 0, sizeof(stations));

	signal (SIGINT, endme);
          
        if ((cmd0 = open("/dev/rtf0", O_RDONLY)) < 0) {
		if ((cmd0 = open("/dev/rtf/0", O_RDONLY)) < 0) {
                	fprintf(stderr, "Error opening /dev/rtf/0 and /dev/rtf0\n");
			exit(1);
		}
        }

	initscr();      /* initialize the curses library */
	keypad(stdscr, TRUE);  /* enable keyboard mapping */
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */

	move(0,0);
	addstr("Showtime:");

        while(!end) {
                read (cmd0, &rx_packet, sizeof(struct mrtt_rx_packet));
		stats = lookup(rx_packet.ip_addr);
		if (!stats)
			continue;
		stats->rtt = (rx_packet.rx - rx_packet.tx)/1000;
		if (stats->rtt < stats->min)
			stats->min = stats->rtt;
		if (stats->rtt > stats->max)
			stats->max = stats->rtt;
		stats->count++;

		move(stats - stations + 1, 0);
		printw("%u.%u.%u.%u\t%dus, min=%dus, max=%dus, count=%d",
		       NIPQUAD(stats->ip),
		       stats->rtt, stats->min, stats->max, stats->count);
		refresh();
        }

	close(cmd0);
	endwin();

        return 0;
}
