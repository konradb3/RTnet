/***
 *
 *  tools/tdmacfg.c
 *  Configuration tool for the RTmac/TDMA discipline
 *
 *  RTmac - real-time networking media access control subsystem
 *  Copyright (C) 2002       Marc Kleine-Budde <kleine-budde@gmx.de>,
 *                2003, 2004 Jan Kiszka <Jan.Kiszka@web.de>
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

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <tdma_chrdev.h>


static int                  f;
static struct tdma_config   tdma_cfg;


void help(void)
{
    fprintf(stderr, "Usage:\n"
        "\ttdmacfg <dev> master <cycle_period> [-b <backup_offset>]\n"
        "\t        [-c calibration_rounds] [-l calibration_log_file]\n"
        "\t        [-m max_calibration_requests] [-i max_slot_id]\n"
        "\ttdmacfg <dev> slave [-c calibration_rounds] "
            "[-l calibration_log_file]\n"
        "\t        [-i max_slot_id]\n"
        "\ttdmacfg <dev> slot <id> [<offset> [-p <phasing>/<period>] "
            "[-s <size>]]\n"
        "\ttdmacfg <dev> detach\n");

    exit(1);
}



int getintopt(int argc, int pos, char *argv[], int min)
{
    int result;


    if (pos >= argc)
        help();
    if ((sscanf(argv[pos], "%u", &result) != 1) || (result < min)) {
        fprintf(stderr, "invalid parameter: %s %s\n", argv[pos-1], argv[pos]);
        exit(1);
    }

    return result;
}



void do_master(int argc, char *argv[])
{
    int r;


    if (argc < 4)
        help();

    if ((sscanf(argv[3], "%u", &r) != 1) || (r <= 0)) {
        fprintf(stderr, "invalid cycle period: %s\n", argv[3]);
        exit(1);
    }
    tdma_cfg.args.master.cycle_period = ((__u64)r) * 1000;

    tdma_cfg.args.master.backup_sync_offset = 0;
    tdma_cfg.args.master.cal_rounds         = 100;
    tdma_cfg.args.master.max_cal_requests   = 64;
    tdma_cfg.args.master.max_slot_id        = 7;

    r = ioctl(f, TDMA_IOC_MASTER, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_slave(int argc, char *argv[])
{
    int r;


    if (argc < 3)
        help();

    tdma_cfg.args.slave.cal_rounds  = 100;
    tdma_cfg.args.slave.max_slot_id = 7;

    r = ioctl(f, TDMA_IOC_SLAVE, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_slot(int argc, char *argv[])
{
    int             r;
    unsigned int    ioc;


    if (argc < 4)
        help();

    if ((sscanf(argv[3], "%u", &r) != 1) || (r < 0)) {
        fprintf(stderr, "invalid slot id: %s\n", argv[3]);
        exit(1);
    }

    if (argc > 4) {
        tdma_cfg.args.set_slot.id = r;

        if ((sscanf(argv[4], "%u", &r) != 1) || (r < 0)) {
            fprintf(stderr, "invalid slot offset: %s\n", argv[4]);
            exit(1);
        }
        tdma_cfg.args.set_slot.offset = ((__u64)r) * 1000;

        tdma_cfg.args.set_slot.period  = 1;
        tdma_cfg.args.set_slot.phasing = 1;
        tdma_cfg.args.set_slot.size    = 0;

        ioc = TDMA_IOC_SET_SLOT;
    } else {
        tdma_cfg.args.remove_slot.id = r;

        ioc = TDMA_IOC_REMOVE_SLOT;
    }

    r = ioctl(f, ioc, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_detach(int argc, char *argv[])
{
    int r;


    if (argc != 3)
        help();

    r = ioctl(f, TDMA_IOC_DETACH, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



int main(int argc, char *argv[])
{
    if ((argc < 3) || (strcmp(argv[1], "--help") == 0))
        help();

    f = open("/dev/rtnet", O_RDWR);

    if (f < 0) {
        perror("/dev/rtnet");
        exit(1);
    }

    strncpy(tdma_cfg.head.if_name, argv[1], IFNAMSIZ);

    if (strcmp(argv[2], "master") == 0)
        do_master(argc, argv);
    if (strcmp(argv[2], "slave") == 0)
        do_slave(argc, argv);
    if (strcmp(argv[2], "slot") == 0)
        do_slot(argc, argv);
    if (strcmp(argv[2], "detach") == 0)
        do_detach(argc, argv);

    help();

    return 0;
}
