/***
 *
 *  tools/rtifconfig.c
 *  ifconfig replacement for RTnet
 *
 *  rtnet - real-time networking subsystem
 *  Copyright (C) 1999,2000 Zentropic Computing, LLC
 *                2004 Jan Kiszka <jan.kiszka@web.de>
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
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <netinet/in.h>

#include <rtnet_chrdev.h>
#include <tdma_chrdev.h>


#define PRINT_FLAG_ALL          1
#define PRINT_FLAG_INACTIVE     2


int                     f;
struct rtnet_core_cmd   cmd;
struct tdma_config      tdma_cfg;


void help(void)
{
    fprintf(stderr, "Usage:\n"
        "\trtifconfig [-a] [<dev>]\n"
        "\trtifconfig <dev> up [<addr> [netmask <mask>]] [[-]promisc]\n"
        "\trtifconfig <dev> down\n"
        "\trtifconfig <dev> mac client\n"
        "\trtifconfig <dev> mac master <cycle-time/us> [<mtu-size/byte>]\n"
        "\trtifconfig <dev> mac up\n"
        "\trtifconfig <dev> mac down\n"
        "\trtifconfig <dev> mac add <addr> <offset/us>\n"
        "\trtifconfig <dev> mac remove <addr>\n"
        "\trtifconfig <dev> mac cycle <time/us>\n"
        "\trtifconfig <dev> mac mtu <size/byte>\n"
        "\trtifconfig <dev> mac offset <addr> <offset/us>\n");

    exit(1);
}



void print_dev(void)
{
    struct in_addr  ip_addr;
    struct in_addr  broadcast_ip;
    unsigned int    flags;


    cmd.head.if_name[9] = 0;

    printf("%-9s Medium: ", cmd.head.if_name);

    if ((cmd.args.info.flags & IFF_LOOPBACK) != 0)
        printf("Local Loopback\n");
    else if (cmd.args.info.type == ARPHRD_ETHER)
        printf("Ethernet  Hardware address: "
               "%02X:%02X:%02X:%02X:%02X:%02X\n",
               cmd.args.info.dev_addr[0], cmd.args.info.dev_addr[1],
               cmd.args.info.dev_addr[2], cmd.args.info.dev_addr[3],
               cmd.args.info.dev_addr[4], cmd.args.info.dev_addr[5]);
    else
        printf("unknown (%X)\n", cmd.args.info.type);

    if (cmd.args.info.ip_addr != 0) {
        ip_addr.s_addr      = cmd.args.info.ip_addr;
        broadcast_ip.s_addr = cmd.args.info.broadcast_ip;
        printf("          IP address: %s  ", inet_ntoa(ip_addr));
        if (cmd.args.info.flags & IFF_BROADCAST)
            printf("Broadcast address: %s", inet_ntoa(broadcast_ip));
        printf("\n");
    }

    flags = cmd.args.info.flags &
        (IFF_UP | IFF_BROADCAST | IFF_LOOPBACK | IFF_RUNNING | IFF_PROMISC);
    printf("          %s%s%s%s%s%s MTU: %d\n\n",
           ((flags & IFF_UP) != 0) ? "UP " : "",
           ((flags & IFF_BROADCAST) != 0) ? "BROADCAST " : "",
           ((flags & IFF_LOOPBACK) != 0) ? "LOOPBACK " : "",
           ((flags & IFF_RUNNING) != 0) ? "RUNNING " : "",
           ((flags & IFF_PROMISC) != 0) ? "PROMISC " : "",
           (flags == 0) ? "[NO FLAGS] " : "", cmd.args.info.mtu);
}



void do_display(int print_flags)
{
    int i;
    int ret;


    if ((print_flags & PRINT_FLAG_ALL) != 0)
        for (i = 1; i <= MAX_RT_DEVICES; i++) {
            cmd.args.info.ifindex = i;

            ret = ioctl(f, IOC_RT_IFINFO, &cmd);
            if (ret == 0) {
                if (((print_flags & PRINT_FLAG_INACTIVE) != 0) ||
                    ((cmd.args.info.flags & IFF_RUNNING) != 0))
                    print_dev();
            } else if (errno != ENODEV) {
                perror("ioctl");
                exit(1);
            }
        }
    else {
        cmd.args.info.ifindex = 0;

        ret = ioctl(f, IOC_RT_IFINFO, &cmd);
        if (ret < 0) {
            perror("ioctl");
            exit(1);
        }

        print_dev();
    }

    exit(0);
}



void do_up(int argc, char *argv[])
{
    int             ret;
    int             i;
    struct in_addr  addr;
    __u32           ip_mask;

    if ((argc > 3) && (inet_aton(argv[3], &addr))) {
        i = 4;
        cmd.args.up.ip_addr = addr.s_addr;
        if (addr.s_addr == 0xFFFFFFFF) {
            fprintf(stderr, "Invalid IP address!\n");
            exit(1);
        }
    } else {
        i = 3;
        /* don't change ip settings */
        cmd.args.up.ip_addr = 0xFFFFFFFF;
    }

    /* set default netmask */
    if (ntohl(cmd.args.up.ip_addr) <= 0x7FFFFFFF)       /* 127.255.255.255  */
        ip_mask = 0x000000FF;                           /* 255.0.0.0        */
    else if (ntohl(cmd.args.up.ip_addr) <= 0xBFFFFFFF)  /* 191.255.255.255  */
        ip_mask = 0x0000FFFF;                           /* 255.255.0.0      */
    else
        ip_mask = 0x00FFFFFF;                           /* 255.255.255.0    */

    /* default: don't change flags */
    cmd.args.up.set_dev_flags   = 0;
    cmd.args.up.clear_dev_flags = 0;

    /* parse optional parameters */
    for ( ; i < argc; i++) {
        if (strcmp(argv[i], "netmask") == 0) {
            if ((++i >= argc) || (cmd.args.up.ip_addr == 0) ||
                (!inet_aton(argv[i], &addr)))
                help();
            ip_mask = addr.s_addr;
        } else if (strcmp(argv[i], "promisc") == 0) {
            cmd.args.up.set_dev_flags   |= IFF_PROMISC;
            cmd.args.up.clear_dev_flags &= ~IFF_PROMISC;
        } else if (strcmp(argv[i], "-promisc") == 0) {
            cmd.args.up.set_dev_flags   &= ~IFF_PROMISC;
            cmd.args.up.clear_dev_flags |= IFF_PROMISC;
        } else
            help();
    }

    cmd.args.up.broadcast_ip = cmd.args.up.ip_addr | (~ip_mask);

    ret = ioctl(f, IOC_RT_IFUP, &cmd);
    if (ret < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void do_down(int argc,char *argv[])
{
    int r;

    if (argc > 3)
        help();

    r = ioctl(f, IOC_RT_IFDOWN, &cmd);
    if (r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
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
    int r, offset;
    struct in_addr addr;

    if (argc < 6)
        help();

    inet_aton(argv[4], &addr);
    tdma_cfg.ip_addr = addr.s_addr;

    offset = atoi(argv[5]);
    tdma_cfg.offset = offset;

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
    f = open("/dev/rtnet", O_RDWR);

    if (f < 0) {
        perror("/dev/rtnet");
        exit(1);
    }

    if (argc == 1)
        do_display(PRINT_FLAG_ALL);

    if (strcmp(argv[1], "--help") == 0)
        help();

    if (strcmp(argv[1], "-a") == 0) {
        if (argc == 3) {
            strncpy(cmd.head.if_name, argv[2], IFNAMSIZ);
            do_display(PRINT_FLAG_INACTIVE);
        } else
            do_display(PRINT_FLAG_INACTIVE | PRINT_FLAG_ALL);
    } else
        strncpy(cmd.head.if_name, argv[1], IFNAMSIZ);

    if (argc < 3)
        do_display(0);

    if (strcmp(argv[2], "up") == 0)
        do_up(argc,argv);
    if (strcmp(argv[2], "down") == 0)
        do_down(argc,argv);
    if (strcmp(argv[2], "mac") == 0)
        do_mac(argc,argv);

    help();

    return 0;
}
