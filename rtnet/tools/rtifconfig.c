/*
    rtnet/rtnet/rtifconfig/rtifconfig.c
    ifconfig replacement for rtner

    rtnet - real-time networking subsystem
    Copyright (C) 1999,2000 Zentropic Computing, LLC

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
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/if_ether.h>

typedef unsigned int u32;

#include <rtnet_chrdev.h>
#include <tdma.h>

void help(void);
void do_display(void);
void do_up(int argc,char *argv[]);
void do_down(int argc,char *argv[]);
void do_route(int argc,char *argv[]);
void do_route_solicit(int argc,char *argv[]);
void do_route_delete(int argc,char *argv[]);
void do_mac(int argc,char *argv[]);
void do_mac_display(void);
void do_mac_client(int argc, char *argv[]);
void do_mac_master(int argc, char *argv[]);
void do_mac_up(int argc, char *argv[]);
void do_mac_down(int argc, char *argv[]);
void do_mac_add(int argc, char *argv[]);
void do_mac_remove(int argc, char *argv[]);
void do_mac_cycle(int argc, char *argv[]);
void do_mac_mtu(int argc, char *argv[]);
void do_mac_client(int argc, char *argv[]);
void do_mac_offset(int argc, char *argv[]);


int f;
struct rtnet_core_cfg   cfg;
struct tdma_config      tdma_cfg;

int main(int argc,char *argv[])
{
    if(argc<2 || !strcmp(argv[1],"--help"))help();

    f=open("/dev/rtnet",O_RDWR);

    if(f<0){
        perror("/dev/rtnet");
        exit(1);
    }

    memset(&cfg,0,sizeof(cfg));
    strncpy(cfg.head.if_name,argv[1],IFNAMSIZ);

    if(argc<3)do_display();

    if(!strcmp(argv[2],"up"))do_up(argc,argv);
    if(!strcmp(argv[2],"down"))do_down(argc,argv);
    if(!strcmp(argv[2],"route"))do_route(argc,argv);
    if(!strcmp(argv[2],"mac"))do_mac(argc,argv);

    help();

    return 0;
}

void help(void)
{
    fprintf(stderr,"rtifconfig:\n");
    fprintf(stderr,"\trtifconfig <dev>\n");
    fprintf(stderr,"\trtifconfig <dev> up <addr> <mask>\n");
    fprintf(stderr,"\trtifconfig <dev> down\n");
    fprintf(stderr,"\trtifconfig <dev> route\n");
    fprintf(stderr,"\trtifconfig <dev> route solicit <addr>\n");
    fprintf(stderr,"\trtifconfig <dev> route delete <addr>\n");
    fprintf(stderr,"\trtifconfig <dev> mac client\n");
    fprintf(stderr,"\trtifconfig <dev> mac master <cycle-time/us> [<mtu-size/byte>]\n");
    fprintf(stderr,"\trtifconfig <dev> mac up\n");
    fprintf(stderr,"\trtifconfig <dev> mac down\n");
    fprintf(stderr,"\trtifconfig <dev> mac add <addr>\n");
    fprintf(stderr,"\trtifconfig <dev> mac remove <addr>\n");
    fprintf(stderr,"\trtifconfig <dev> mac cycle <time/us>\n");
    fprintf(stderr,"\trtifconfig <dev> mac mtu <size/byte>\n");
    fprintf(stderr,"\trtifconfig <dev> mac offset <addr> <offset/us>\n");

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
        if (buff[0] == '#' || buff[0] == '\0')
            continue;
        fprintf(stdout, buff);
    }
    fclose(fp);
    exit(0);
}

void do_up(int argc,char *argv[])
{
    int r;
    struct in_addr addr;

    if(argc<5)help();

    inet_aton(argv[3], &addr);
    cfg.ip_addr=addr.s_addr;
    inet_aton(argv[4], &addr);
    cfg.ip_mask=addr.s_addr;

    cfg.ip_netaddr=cfg.ip_addr&cfg.ip_mask;
    cfg.ip_broadcast=cfg.ip_addr|(~cfg.ip_mask);

    r=ioctl(f,IOC_RT_IFUP,&cfg);
    if(r<0){
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_down(int argc,char *argv[])
{
    int r;

    r=ioctl(f,IOC_RT_IFDOWN,&cfg);
    if(r<0){
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_route(int argc,char *argv[])
{
    if ( argc<4 )
        do_display();

    if ( !strcmp(argv[3],"solicit") )
        do_route_solicit(argc,argv);
    if ( !strcmp(argv[3],"delete") )
        do_route_delete(argc,argv);

    help();
}

void do_route_solicit(int argc,char *argv[])
{
    int r;
    struct in_addr addr;

    if(argc<5)help();

    inet_aton(argv[4], &addr);
    cfg.ip_addr=addr.s_addr;

    r=ioctl(f,IOC_RT_ROUTE_SOLICIT,&cfg);
    if ( r<0 ) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}


/***
 * do_route_delete:     delete route
 */
void do_route_delete(int argc,char *argv[])
{
    int r;
    struct in_addr addr;

    if(argc<5)help();

    inet_aton(argv[4], &addr);
    cfg.ip_addr=addr.s_addr;

    r=ioctl(f,IOC_RT_ROUTE_DELETE,&cfg);
    if ( r<0 ) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}


void do_mac(int argc, char *argv[])
{
    memset(&tdma_cfg, 0, sizeof(tdma_cfg));
    strncpy(tdma_cfg.head.if_name, argv[1], 15);

    if(argc < 4)
        do_mac_display();

    if(!strcmp(argv[3], "client"))
        do_mac_client(argc, argv);
    if(!strcmp(argv[3], "master"))
        do_mac_master(argc, argv);
    if(!strcmp(argv[3], "up"))
        do_mac_up(argc, argv);
    if(!strcmp(argv[3], "down"))
        do_mac_down(argc, argv);
    if(!strcmp(argv[3], "add"))
        do_mac_add(argc, argv);
    if(!strcmp(argv[3], "remove"))
        do_mac_remove(argc, argv);
    if(!strcmp(argv[3], "cycle"))
        do_mac_cycle(argc, argv);
    if(!strcmp(argv[3], "mtu"))
        do_mac_mtu(argc, argv);
    if(!strcmp(argv[3], "offset"))
        do_mac_offset(argc, argv);

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
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_mac_master(int argc, char *argv[])
{
    int r, cycle, mtu = ETH_ZLEN - ETH_HLEN;    // 46 = min octets in payload

    if(argc < 5)
        help();
    cycle = atoi(argv[4]);

    if( argc >= 6)
        mtu = atoi(argv[5]);

    tdma_cfg.cycle = cycle;
    tdma_cfg.mtu = mtu;

    r = ioctl(f, TDMA_IOC_MASTER, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }


    exit(0);
}

void do_mac_up(int argc, char *argv[])
{
    int r;

    r = ioctl(f, TDMA_IOC_UP, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_mac_down(int argc, char *argv[])
{
    int r;

    r = ioctl(f, TDMA_IOC_DOWN, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_mac_add(int argc, char *argv[])
{
    int r;
    struct in_addr addr;

    if(argc < 5)
        help();

    inet_aton(argv[4], &addr);
    tdma_cfg.ip_addr = addr.s_addr;

    r = ioctl(f, TDMA_IOC_ADD, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}


void do_mac_remove(int argc, char *argv[])
{
    int r;
    struct in_addr addr;

    if(argc < 5)
        help();

    inet_aton(argv[4], &addr);
    tdma_cfg.ip_addr = addr.s_addr;

    r = ioctl(f, TDMA_IOC_REMOVE, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_mac_cycle(int argc, char *argv[])
{
    int r, cycle;

    if(argc < 5)
        help();

    cycle = atoi(argv[4]);
    tdma_cfg.cycle = cycle;

    r = ioctl(f, TDMA_IOC_CYCLE, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_mac_mtu(int argc, char *argv[])
{
    int r, mtu;

    if(argc < 5)
        help();

    mtu = atoi(argv[4]);
    tdma_cfg.mtu = mtu;

    r = ioctl(f, TDMA_IOC_MTU, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}

void do_mac_offset(int argc, char *argv[])
{
    int r, offset;
    struct in_addr addr;

    if(argc < 6)
        help();

    inet_aton(argv[4], &addr);
    offset = atoi(argv[5]);

    tdma_cfg.ip_addr = addr.s_addr;
    tdma_cfg.offset = offset;

    r = ioctl(f, TDMA_IOC_OFFSET, &tdma_cfg);
    if(r < 0) {
        perror("ioctl");
        exit(1);
    }
    exit(0);
}
