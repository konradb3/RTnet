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

static int end = 0;
FILE *output;

static void endme (int dummy) { end = 1; }

int main(int argc,char *argv[])
{
        int cmd0;
        union {long long l; unsigned char c[8];} rx_time, tx_time;
	
	signal (SIGINT, endme);
	
	if (argc==2) {
		output = fopen(argv[1], "w");
		if (!output) {
			fprintf(stderr, "Error opening output file %s\n", argv[1]);
			exit(1);
		}
	}
          
        if ((cmd0 = open("/dev/rtf0", O_RDONLY)) < 0) {
		if ((cmd0 = open("/dev/rtf/0", O_RDONLY)) < 0) {
                	fprintf(stderr, "Error opening /dev/rtf/0 and /dev/rtf0\n");
			exit(1);
		}
        }

	fprintf(stdout, "working\n");
	
        while(!end) {
                read (cmd0, rx_time.c, sizeof (long long));
                read (cmd0, tx_time.c, sizeof (long long));
		
		if(output)
			fprintf(output, "%lld\n", (rx_time.l - tx_time.l));
		else
			fprintf(stdout, "Roundtrip = %lld us\n", (rx_time.l-tx_time.l)/1000);


		rx_time.l=0;
		tx_time.l=0;
        }

	close(cmd0);
	if (output) {
		fflush(NULL);
		fclose(output);
	}

        return 0;
}
