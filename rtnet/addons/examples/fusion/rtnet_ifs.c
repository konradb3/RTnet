/* rtnet_ifs.c
 *
 * Lists all local IP addresses and the interface flags - RTAI/fusion version
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

#include <rtnet.h>


/* Well, we know that there are currently only up to 8... */
#define MAX_RT_DEVICES  8


int main(int argc, char *argv[])
{
    int sockfd = 0;
    struct ifreq ifr[MAX_RT_DEVICES];
    short flags[MAX_RT_DEVICES];
    int devices = 0;
    struct ifconf ifc;
    struct ifreq flags_ifr;
    int i, ret;

    printf("RTnet, interface lister for RTAI/fusion\n");

    /* Create new socket. */
    sockfd = rt_dev_socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {

        printf("Error opening socket: %d\n", sockfd);
        return 1;
    }

    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_req = ifr;

    ret = rt_dev_ioctl(sockfd, SIOCGIFCONF, &ifc);
    if (ret < 0) {
        rt_dev_close(sockfd);

        printf("Error retrieving device list: %d\n", ret);
        return 1;
    }

    while (ifc.ifc_len >= (int)sizeof(struct ifreq)) {
        memcpy(flags_ifr.ifr_name, ifc.ifc_req[devices].ifr_name, IFNAMSIZ);
        ret = rt_dev_ioctl(sockfd, SIOCGIFFLAGS, &flags_ifr);
        if (ret < 0) {
            rt_dev_close(sockfd);

            printf("Error retrieving flags for device %s: %d\n",
                   flags_ifr.ifr_name, ret);
            return 1;
        }
        flags[devices] = flags_ifr.ifr_flags;

        ifc.ifc_len -= sizeof(struct ifreq);
        devices++;
    }

    rt_dev_close(sockfd);

    for (i = 0; i < devices; i++)
        printf("Device %s: IP %d.%d.%d.%d, flags 0x%08X\n", ifr[i].ifr_name,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr & 0xFF,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr >> 8 & 0xFF,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr >> 16 & 0xFF,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr >> 24,
               flags[i]);

    return 0;
}
