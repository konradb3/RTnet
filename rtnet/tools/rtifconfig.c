/*
    tools/rtifconfig.c
    ifconfig replacement for RTnet

    rtnet - real-time networking subsystem
    Copyright (C) 1999,2000 Zentropic Computing, LLC
                  2004 Jan Kiszka <jan.kiszka@web.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <rtnet_chrdev.h>
#include <tdma.h>


int f;
struct rtnet_core_cfg   cfg;
struct tdma_config      tdma_cfg;


void help(void)
{
    fprintf(stderr, "Usage:\n"
        "\trtifconfig [<dev>]\n"
        "\trtifconfig <dev> up <addr> [netmask <mask>]\n"
        "\trtifconfig <dev> down\n"
        "\trtifconfig <dev> route\n"
        "\trtifconfig <dev> route solicit <addr>\n"
        "\trtifconfig <dev> route delete <addr>\n"
        "\trtifconfig <dev> mac client\n"
        "\trtifconfig <dev> mac master <cycle-time/us> [<mtu-size/byte>]\n"
        "\trtifconfig <dev> mac up\n"
        "\trtifconfig <dev> mac down\n"
        "\trtifconfig <dev> mac add <addr>\n"
        "\trtifconfig <dev> mac remove <addr>\n"
        "\trtifconfig <dev> mac cycle <time/us>\n"
        "\trtifconfig <dev> mac mtu <size/byte>\n"
        "\trtifconfig <dev> mac offset <addr> <offset/us>\n");

    exit(1);
}



void do_display(void)
{
    char buff[1024];
    int linenr;
    FILE *fp;

    char *name = "/proc/rtai/route";

    if ((fp = fopen(name, "r")) == NULL) {
        fprintf(stderr, "rtifconfig: cannot open file %s:%s.\n", name, strerror(errno));
        help ();
    }

    /* Read the lines in the file. */
    linenr = 0;
    while (fgets(buff, sizeof(buff), fp)) {
        ++linenr;
        if ((buff[0] == '#') || (buff[0] == '\0'))
            continue;
        fprintf(stdout, buff);
    }
    fclose(fp);
    exit(0);
}



void do_up(int argc, char *argv[])
{
    int r;
    struct in_addr addr;

    if (argc < 4)
        help();

    inet_aton(argv[3], &addr);
    cfg.ip_addr = addr.s_addr;
    if (argc > 4) {
        if ((argc != 6) || (strcmp(argv[4], "netmask") != 0))
            help();

        inet_aton(argv[5], &addr);
        cfg.ip_mask = addr.s_addr;
    } else {
        if (strcmp(argv[1], "rtlo") == 0)           /* loopback device? */
            /* RTnet's routing system still needs this work around      */
            cfg.ip_mask = 0xFFFFFFFF;
        else if (ntohl(cfg.ip_addr) <= 0x7FFFFFFF)  /* 127.255.255.255  */
            cfg.ip_mask = 0x000000FF;               /* 255.0.0.0        */
        else if (ntohl(cfg.ip_addr) <= 0xBFFFFFFF)  /* 191.255.255.255  */
            cfg.ip_mask = 0x0000FFFF;               /* 255.255.0.0      */
        else
            cfg.ip_mask = 0x00FFFFFF;               /* 255.255.255.0    */
    }

    cfg.ip_netaddr   = cfg.ip_addr&cfg.ip_mask;
    cfg.ip_broadcast = cfg.ip_addr|(~cfg.ip_mask);

    r = ioctl(f, IOC_RT_IFUP, &cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_down(int argc,char *argv[])
{
    int r;

    r = ioctl(f, IOC_RT_IFDOWN, &cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_route_solicit(int argc, char *argv[])
{
    int r;
    struct in_addr addr;

    if (argc < 5)
        help();

    inet_aton(argv[4], &addr);
    cfg.ip_addr = addr.s_addr;

    r = ioctl(f, IOC_RT_ROUTE_SOLICIT, &cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



/***
 * do_route_delete:     delete route
 */
void do_route_delete(int argc, char *argv[])
{
    int r;
    struct in_addr addr;

    if (argc < 5)
        help();

    inet_aton(argv[4], &addr);
    cfg.ip_addr = addr.s_addr;

    r = ioctl(f, IOC_RT_ROUTE_DELETE, &cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_route(int argc, char *argv[])
{
    if (argc < 4)
        do_display();

    if (strcmp(argv[3], "solicit") == 0)
        do_route_solicit(argc, argv);
    if (strcmp(argv[3], "delete") == 0)
        do_route_delete(argc, argv);

    help();
}



void do_mac_display(void)
{
    fprintf(stderr, "fixme\n");
    exit(0);
}



void do_mac_client(int argc, char *argv[])
{
    int r;

    r = ioctl(f, TDMA_IOC_CLIENT, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac_master(int argc, char *argv[])
{
    int r, cycle, mtu = ETH_ZLEN - ETH_HLEN; /* 46 = min octets in payload */

    if (argc < 5)
        help();
    cycle = atoi(argv[4]);

    if (argc >= 6)
        mtu = atoi(argv[5]);

    tdma_cfg.cycle = cycle;
    tdma_cfg.mtu = mtu;

    r = ioctl(f, TDMA_IOC_MASTER, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }


    exit(0);
}



void do_mac_up(int argc, char *argv[])
{
    int r;

    r = ioctl(f, TDMA_IOC_UP, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac_down(int argc, char *argv[])
{
    int r;

    r = ioctl(f, TDMA_IOC_DOWN, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac_add(int argc, char *argv[])
{
    int r;
    struct in_addr addr;

    if (argc < 5)
        help();

    inet_aton(argv[4], &addr);
    tdma_cfg.ip_addr = addr.s_addr;

    r = ioctl(f, TDMA_IOC_ADD, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac_remove(int argc, char *argv[])
{
    int r;
    struct in_addr addr;

    if (argc < 5)
        help();

    inet_aton(argv[4], &addr);
    tdma_cfg.ip_addr = addr.s_addr;

    r = ioctl(f, TDMA_IOC_REMOVE, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac_cycle(int argc, char *argv[])
{
    int r, cycle;

    if (argc < 5)
        help();

    cycle = atoi(argv[4]);
    tdma_cfg.cycle = cycle;

    r = ioctl(f, TDMA_IOC_CYCLE, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac_mtu(int argc, char *argv[])
{
    int r, mtu;

    if (argc < 5)
        help();

    mtu = atoi(argv[4]);
    tdma_cfg.mtu = mtu;

    r = ioctl(f, TDMA_IOC_MTU, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac_offset(int argc, char *argv[])
{
    int r, offset;
    struct in_addr addr;

    if (argc < 6)
        help();

    inet_aton(argv[4], &addr);
    offset = atoi(argv[5]);

    tdma_cfg.ip_addr = addr.s_addr;
    tdma_cfg.offset = offset;

    r = ioctl(f, TDMA_IOC_OFFSET, &tdma_cfg);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_mac(int argc, char *argv[])
{
    memset(&tdma_cfg, 0, sizeof(tdma_cfg));
    strncpy(tdma_cfg.head.if_name, argv[1], 15);

    if (argc < 4)
        do_mac_display();

    if (!strcmp(argv[3], "client"))
        do_mac_client(argc, argv);
    if (!strcmp(argv[3], "master"))
        do_mac_master(argc, argv);
    if (!strcmp(argv[3], "up"))
        do_mac_up(argc, argv);
    if (!strcmp(argv[3], "down"))
        do_mac_down(argc, argv);
    if (!strcmp(argv[3], "add"))
        do_mac_add(argc, argv);
    if (!strcmp(argv[3], "remove"))
        do_mac_remove(argc, argv);
    if (!strcmp(argv[3], "cycle"))
        do_mac_cycle(argc, argv);
    if (!strcmp(argv[3], "mtu"))
        do_mac_mtu(argc, argv);
    if (!strcmp(argv[3], "offset"))
        do_mac_offset(argc, argv);

    help();
}



int main(int argc, char *argv[])
{
    if (argc == 1)
        do_display();

    if (strcmp(argv[1], "--help") == 0)
        help();

    if (argc < 3)
        do_display();

    f = open("/dev/rtnet", O_RDWR);

    if (f < 0) {
        perror("/dev/rtnet");
        exit(1);
    }

    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.head.if_name, argv[1], IFNAMSIZ);

    if (strcmp(argv[2], "up") == 0)
        do_up(argc,argv);
    if (strcmp(argv[2], "down") == 0)
        do_down(argc,argv);
    if (strcmp(argv[2], "route") == 0)
        do_route(argc,argv);
    if (strcmp(argv[2], "mac") == 0)
        do_mac(argc,argv);

    help();

    return 0;
}
