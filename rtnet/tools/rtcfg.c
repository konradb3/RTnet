/***
 *
 *  tools/rtcfg.c
 *
 *  Real-Time Configuration Distribution Protocol
 *
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

#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/netdevice.h>
#include <netinet/ether.h>
#include <netinet/in.h>

#include <rtnet_chrdev.h>
#include <rtcfg.h>


int              f;
struct rtcfg_cmd cmd;


void help(void)
{
    fprintf(stderr, "Usage (server):\n"
        "\trtcfg <dev> server [-p period] [-b burstrate] [-h <heartbeat>]\n"
        "\t      [-t <threshold>]\n"
        "\trtcfg <dev> add <address> [-stage1 <stage1_file>]\n"
        "\t      [-stage2 <stage2_file>]\n"
        "\trtcfg <dev> del <address>\n"
        "\trtcfg <dev> wait [-t <timeout>]\n\n"
        "Usage (client):\n"
        "\trtcfg <dev> client [-t <timeout>] [-c|-f <stage1_file>] [-m maxclients]\n"
        "\trtcfg <dev> announce [-t <timeout>] [-c|-f <stage2_file>]\n"
        "\t      [-b burstrate]\n");

    exit(1);
}



int getintopt(int argc, int pos, char *argv[], int min)
{
    int result;


    if (pos >= argc)
        help();
    if ((sscanf(argv[pos], "%u", &result) != 1) || (result < min)) {
        fprintf(stderr, "Invalid parameter: %s %s\n", argv[pos-1], argv[pos]);
        exit(1);
    }

    return result;
}



void cmd_server(int argc, char *argv[])
{
    int i;


    cmd.args.server.period    = 1000;
    cmd.args.server.burstrate = 4;
    cmd.args.server.heartbeat = 1000;
    cmd.args.server.threshold = 2;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0)
            cmd.args.server.period = getintopt(argc, ++i, argv, 1);
        else if (strcmp(argv[i], "-b") == 0)
            cmd.args.server.burstrate = getintopt(argc, ++i, argv, 1);
        else if (strcmp(argv[i], "-h") == 0)
            cmd.args.server.heartbeat = getintopt(argc, ++i, argv, 1);
        else if (strcmp(argv[i], "-t") == 0)
            cmd.args.server.threshold = getintopt(argc, ++i, argv, 1);
        else
            help();
    }

    i = ioctl(f, RTCFG_IOC_SERVER, &cmd);
    if (i < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void cmd_add(int argc, char *argv[])
{
    int                 i;
    unsigned int        ioctl_code;
    struct in_addr      ip_addr;
    struct ether_addr   mac_addr;


    if (argc < 4)
        help();

    if (inet_aton(argv[3], &ip_addr)) {
        ioctl_code = RTCFG_IOC_ADD_IP;
        cmd.args.add.ip_addr = ip_addr.s_addr;
    } else if (ether_aton_r(argv[3], &mac_addr) != NULL) {
        ioctl_code = RTCFG_IOC_ADD_MAC;
        memcpy(cmd.args.add.mac_addr, mac_addr.ether_addr_octet,
               sizeof(mac_addr.ether_addr_octet));
    } else {
        fprintf(stderr, "Invalid IP or physical address: %s\n", argv[3]);
        exit(1);
    }

    cmd.args.add.stage1_file.size      = 0;
    cmd.args.add.stage1_file.frag_size = 0;
    cmd.args.add.stage2_file.size      = 0;
    cmd.args.add.stage2_file.frag_size = 0;

    for (i = 4; i < argc; i++) {
        if (strcmp(argv[i], "-stage1") == 0) {
            fprintf(stderr, "Configuration files are not supported yet.\n");
            exit(1);
        } else if (strcmp(argv[i], "-stage2") == 0) {
            fprintf(stderr, "Configuration files are not supported yet.\n");
            exit(1);
        } else
            help();
    }

    i = ioctl(f, ioctl_code, &cmd);
    if (i < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void cmd_del(int argc, char *argv[])
{
    int                 i;
    unsigned int        ioctl_code;
    struct in_addr      ip_addr;
    struct ether_addr   mac_addr;


    if (argc != 4)
        help();

    if (inet_aton(argv[3], &ip_addr)) {
        ioctl_code = RTCFG_IOC_DEL_IP;
        cmd.args.del.ip_addr = ip_addr.s_addr;
    } else if (ether_aton_r(argv[3], &mac_addr) != NULL) {
        ioctl_code = RTCFG_IOC_DEL_MAC;
        memcpy(cmd.args.del.mac_addr, mac_addr.ether_addr_octet,
               sizeof(mac_addr.ether_addr_octet));
    } else {
        fprintf(stderr, "Invalid IP or physical address: %s\n", argv[3]);
        exit(1);
    }

    i = ioctl(f, ioctl_code, &cmd);
    if (i < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void cmd_wait(int argc, char *argv[])
{
    int i;


    cmd.args.wait.timeout = 0;  /* infinite */

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0)
            cmd.args.wait.timeout = getintopt(argc, ++i, argv, 1);
        else
            help();
    }

    i = ioctl(f, RTCFG_IOC_WAIT, &cmd);
    if (i < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void cmd_client(int argc, char *argv[])
{
    int i;


    cmd.args.client.timeout     = 0; /* infinite */
    cmd.args.client.buffer_size = 0;
    cmd.args.client.max_clients = 32;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0)
            cmd.args.client.timeout = getintopt(argc, ++i, argv, 1);
        else if (strcmp(argv[i], "-c") == 0) {
            fprintf(stderr, "Configuration files are not supported yet.\n");
            exit(1);
        } else if (strcmp(argv[i], "-f") == 0) {
            fprintf(stderr, "Configuration files are not supported yet.\n");
            exit(1);
        } else if (strcmp(argv[i], "-m") == 0)
            cmd.args.client.max_clients = getintopt(argc, ++i, argv, 1);
        else
            help();
    }

    i = ioctl(f, RTCFG_IOC_CLIENT, &cmd);
    if (i < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}



void cmd_announce(int argc, char *argv[])
{
    int i;


    cmd.args.announce.timeout     = 0; /* infinite */
    cmd.args.announce.buffer_size = 0;
    cmd.args.announce.burstrate   = 32;

    for (i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0)
            cmd.args.announce.timeout = getintopt(argc, ++i, argv, 1);
        else if (strcmp(argv[i], "-c") == 0) {
            fprintf(stderr, "Configuration files are not supported yet.\n");
            exit(1);
        } else if (strcmp(argv[i], "-f") == 0) {
            fprintf(stderr, "Configuration files are not supported yet.\n");
            exit(1);
        } else if (strcmp(argv[i], "-b") == 0)
            cmd.args.announce.burstrate = getintopt(argc, ++i, argv, 1);
        else
            help();
    }

    i = ioctl(f, RTCFG_IOC_ANNOUNCE, &cmd);
    if (i < 0) {
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

    memset(&cmd, 0, sizeof(cmd));
    strncpy(cmd.head.if_name, argv[1], IFNAMSIZ);

    if (strcmp(argv[2], "server") == 0)
        cmd_server(argc, argv);
    if (strcmp(argv[2], "add") == 0)
        cmd_add(argc, argv);
    if (strcmp(argv[2], "del") == 0)
        cmd_del(argc, argv);
    if (strcmp(argv[2], "wait") == 0)
        cmd_wait(argc, argv);

    if (strcmp(argv[2], "client") == 0)
        cmd_client(argc, argv);
    if (strcmp(argv[2], "announce") == 0)
        cmd_announce(argc, argv);

    help();

    return 0;
}
