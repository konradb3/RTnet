/***
 *
 *  examples/xenomai/tdma-api.c
 *
 *  waits on the SYNC frame arrival/transmission and prints both local and
 *  global time - Xenomai version
 *
 *  Copyright (C) 2006 Jan Kiszka <jan.kiszka@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/mman.h>

#include <native/task.h>

#include <rtnet.h>
#include <rtmac.h>

int fd;

void terminate(int signal)
{
    rt_dev_close(fd);
    exit(0);
}

int main(int argc, char *argv[])
{
    char                    *device_name = "TDMA0";
    RT_TASK                 task;
    struct rtmac_waitinfo   waitinfo;
    int                     err;

    mlockall(MCL_CURRENT | MCL_FUTURE);

    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);

    if (argc > 1)
        device_name = argv[1];
    fd = rt_dev_open(device_name, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "failed to open %s, error code %d\n", device_name,
                fd);
        exit(1);
    }

    rt_task_shadow(&task, "tdma-api", 50, 0);

    waitinfo.type = TDMA_WAIT_ON_SYNC;
    waitinfo.size = sizeof(waitinfo);

    while (1) {
        do
        {
            err = rt_dev_ioctl(fd, RTMAC_RTIOC_WAITONCYCLE_EX, &waitinfo);
            if (err) {
                fprintf(stderr, "failed to issue RTMAC_RTIOC_WAITONCYCLE_EX, "
                        "error code %d\n", err);
                rt_dev_close(fd);
                exit(1);
            }
        } while (waitinfo.cycle_no%100 != 0);

        /* You should not call printf in time-critical code, this is only
           for demostration purpose. */
        printf("cycle #%ld, start %.9f s, offset %lld ns\n",
               waitinfo.cycle_no,
               (waitinfo.cycle_start+waitinfo.clock_offset)/1000000000.0,
               waitinfo.clock_offset);
    }
}
