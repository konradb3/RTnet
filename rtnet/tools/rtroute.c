/***
 *
 *  tools/rtroute.c
 *  manages IP host and network routes for RTnet
 *
 *  rtnet - real-time networking subsystem
 *  Copyright (C) 2004 Jan Kiszka <jan.kiszka@web.de>
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/ether.h>

#include <rtnet_chrdev.h>


int                     f;
struct rtnet_core_cmd   cmd;
struct in_addr          addr;


void help(void)
{
    fprintf(stderr, "Usage:\n"
        "\trtroute\n"
        "\trtroute solicit <addr> dev <dev>\n"
        "\trtroute add <addr> <hwaddr> dev <dev>\n"
        "\trtroute add <addr> netmask <mask> gw <gw-addr>\n"
        "\trtroute del <addr> [netmask <mask>]\n"
        );

    exit(1);
}



void print_routes(void)
{
    char        buf[4096];
    int         proc;
    size_t      size;
    const char  host_route[] = "/proc/rtnet/ipv4/host_route";
    const char  net_route[]  = "/proc/rtnet/ipv4/net_route";


    if ((proc = open(host_route, O_RDONLY)) < 0) {
        perror(host_route);
        exit(1);
    }

    printf("Host Routing Table\n");
    while ((size = read(proc, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, size);

    close(f);

    if ((proc = open(net_route, O_RDONLY)) < 0) {
        perror(net_route);
        exit(1);
    }

    printf("\nNetwork Routing Table\n");
    while ((size = read(proc, buf, sizeof(buf))) > 0)
        write(STDOUT_FILENO, buf, size);

    close(f);

    exit(0);
}



void route_solicit(int argc, char *argv[])
{
    int ret;


    if ((argc != 5) || (strcmp(argv[3], "dev") != 0))
        help();

    strncpy(cmd.head.if_name, argv[4], IFNAMSIZ);
    cmd.args.solicit.ip_addr = addr.s_addr;

    ret = ioctl(f, IOC_RT_HOST_ROUTE_SOLICIT, &cmd);
    if (ret < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void route_add(int argc, char *argv[])
{
    struct ether_addr   dev_addr;
    int                 ret;


    if (argc == 6) {
        /*** add host route ***/
        if ((ether_aton_r(argv[3], &dev_addr) == NULL) ||
            (strcmp(argv[4], "dev") != 0))
            help();

        cmd.args.addhost.ip_addr = addr.s_addr;
        memcpy(cmd.args.addhost.dev_addr, dev_addr.ether_addr_octet,
               sizeof(dev_addr.ether_addr_octet));
        strncpy(cmd.head.if_name, argv[5], IFNAMSIZ);

        ret = ioctl(f, IOC_RT_HOST_ROUTE_ADD, &cmd);
    } else if (argc == 7) {
        /*** add network route ***/
        if ((strcmp(argv[3], "netmask") != 0) || (strcmp(argv[5], "gw") != 0))
            help();

        cmd.args.addnet.net_addr = addr.s_addr;
        if (!inet_aton(argv[4], &addr))
            help();
        cmd.args.addnet.net_mask = addr.s_addr;
        if (!inet_aton(argv[6], &addr))
            help();
        cmd.args.addnet.gw_addr = addr.s_addr;

        ret = ioctl(f, IOC_RT_NET_ROUTE_ADD, &cmd);
    } else
        help();

    if (ret < 0) {
        perror("ioctl");
        exit(1);
    }

    exit(0);
}



void route_delete(int argc, char *argv[])
{
    int ret;


    if (argc == 3) {
        /*** delete host route ***/
        cmd.args.delhost.ip_addr = addr.s_addr;

        ret = ioctl(f, IOC_RT_HOST_ROUTE_DELETE, &cmd);
    } else if (argc == 5) {
        /*** delete network route ***/
        if (strcmp(argv[3], "netmask") != 0)
            help();

        cmd.args.delnet.net_addr = addr.s_addr;
        if (!inet_aton(argv[4], &addr))
            help();
        cmd.args.delnet.net_mask = addr.s_addr;

        ret = ioctl(f, IOC_RT_NET_ROUTE_DELETE, &cmd);
    } else
        help();

    if (ret < 0) {
        if (errno == ENOENT)
            fprintf(stderr, "Specified route not found\n");
        else
            perror("ioctl");
        exit(1);
    }

    exit(0);
}



int main(int argc, char *argv[])
{
    __u32       ip_addr;
    const char  rtnet_dev[] = "/dev/rtnet";


    if (argc == 1)
        print_routes();

    if ((strcmp(argv[1], "--help") == 0) || (argc < 3))
        help();

    f = open(rtnet_dev, O_RDWR);
    if (f < 0) {
        perror(rtnet_dev);
        exit(1);
    }

    /* second argument is always an IP address */
    if (!inet_aton(argv[2], &addr))
        help();

    if (strcmp(argv[1], "solicit") == 0)
        route_solicit(argc, argv);
    if (strcmp(argv[1], "add") == 0)
        route_add(argc, argv);
    if (strcmp(argv[1], "del") == 0)
        route_delete(argc, argv);

    help();

    return 0;
}
