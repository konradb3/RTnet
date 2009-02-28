/* rtnet_ifs.c
 *
 * Lists all local IP addresses and the interface flags - Xenomai version
 * 4/2005 by Jan Kiszka <jan.kiszka@web.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <string.h>
#include <sched.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/if.h>
#include <arpa/inet.h>

#include <rtnet.h>


/* Well, we know that there are currently only up to 8... */
#define MAX_RT_DEVICES  8


int main(int argc, char *argv[])
{
    int sockfd = 0;
    struct ifreq ifr_buf[MAX_RT_DEVICES];
    short flags[MAX_RT_DEVICES];
    int index[MAX_RT_DEVICES];
    int devices = 0;
    struct ifconf ifc;
    int i, ret;

    printf("RTnet, interface lister for Xenomai\n");

    /* Create new socket. */
    sockfd = rt_dev_socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {

        printf("Error opening socket: %d\n", sockfd);
        return 1;
    }

    ifc.ifc_len = sizeof(ifr_buf);
    ifc.ifc_req = ifr_buf;

    ret = rt_dev_ioctl(sockfd, SIOCGIFCONF, &ifc);
    if (ret < 0) {
        rt_dev_close(sockfd);

        printf("Error retrieving device list: %d\n", ret);
        return 1;
    }

    while (ifc.ifc_len >= (int)sizeof(struct ifreq)) {
        struct ifreq ifr;

        memcpy(ifr.ifr_name, ifc.ifc_req[devices].ifr_name, IFNAMSIZ);
        ret = rt_dev_ioctl(sockfd, SIOCGIFFLAGS, &ifr);
        if (ret < 0) {
            rt_dev_close(sockfd);

            printf("Error retrieving flags for device %s: %d\n",
                   ifr.ifr_name, ret);
            return 1;
        }
        flags[devices] = ifr.ifr_flags;

        ret = rt_dev_ioctl(sockfd, SIOCGIFINDEX, &ifr);
        if (ret < 0) {
            rt_dev_close(sockfd);

            printf("Error retrieving index for device %s: %d\n",
                   ifr.ifr_name, ret);
            return 1;
        }
        index[devices] = ifr.ifr_ifindex;

        ifc.ifc_len -= sizeof(struct ifreq);
        devices++;
    }

    rt_dev_close(sockfd);

    for (i = 0; i < devices; i++) {
        struct sockaddr_in *addr = (struct sockaddr_in *)&ifr_buf[i].ifr_addr;

        printf("Device %s: IP %s, flags 0x%08X, index %d\n",
               ifr_buf[i].ifr_name, inet_ntoa(addr->sin_addr), flags[i],
               index[i]);
    }

    return 0;
}
