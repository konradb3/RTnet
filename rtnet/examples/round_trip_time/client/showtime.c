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

static void endme (int dummy) { end = 1; }

int main(int argc,char *argv[])
{
        int cmd0;
        union {unsigned long long l; unsigned char c[8];} t[2];
	unsigned int rtmin = -1, rtmax = 0, rtrip; /* -1 is the maximum value of an unsigned variable */

        signal (SIGINT, endme);

          
        if ((cmd0 = open("/dev/rtf0", O_RDONLY)) < 0) {
                fprintf(stderr, "Error opening /dev/rtf0\n");
                exit(1);
        }


        while(!end) {
                if (read (cmd0, t, 2 * sizeof (unsigned long long))
                    != 2*sizeof(unsigned long long))
                    break;
		
		rtrip = (unsigned int) ((t[0].l-t[1].l) / 1000);
		if (rtmin > rtrip) rtmin = rtrip;
		if (rtmax < rtrip) rtmax = rtrip;
		fprintf (stdout, "Roundtrip = %3uus (min: %3uus, max: %3uus)\n", rtrip, rtmin, rtmax);
        }

        return 0;
}
