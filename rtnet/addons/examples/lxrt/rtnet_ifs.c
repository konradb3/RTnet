/* rtnet_ifs.c
 *
 * Lists all local IP addresses and the interface flags
 * 10/2003 by Jan Kiszka <jan.kiszka@web.de>
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
#include <stdlib.h>
#include <fcntl.h>
#include <sched.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/if.h>
#include <sys/ioctl.h>

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

    RT_TASK *lxrtnettsk;

    printf("RTnet, interface lister for NEWLXRT\n");

    /* Lock allocated memory into RAM. */
    mlockall(MCL_CURRENT|MCL_FUTURE);

    /* Create new socket. */
    sockfd = socket_rt(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {

        printf("Error opening socket: %d\n", sockfd);
        exit(1);
    }

    /* Initialize a real time buddy. */
    lxrtnettsk = rt_task_init(4800, 1, 0, 0);
    if (NULL == lxrtnettsk) {
        close_rt(sockfd);
        printf("CANNOT INIT MASTER TASK\n");
        exit(1);
    }

    /* Switch over to hard realtime mode. */
    rt_make_hard_real_time();

    ifc.ifc_len = sizeof(ifr);
    ifc.ifc_req = ifr;

    ret = ioctl_rt(sockfd, SIOCGIFCONF, &ifc);
    if (ret < 0) {
        rt_make_soft_real_time();
        close_rt(sockfd);
        rt_task_delete(lxrtnettsk);

        printf("Error retrieving device list: %d\n", ret);
        exit(1);
    }

    while (ifc.ifc_len >= (int)sizeof(struct ifreq)) {
        memcpy(flags_ifr.ifr_name, ifc.ifc_req[devices].ifr_name, IFNAMSIZ);
        ret = ioctl_rt(sockfd, SIOCGIFFLAGS, &flags_ifr);
        if (ret < 0) {
            rt_make_soft_real_time();
            close_rt(sockfd);
            rt_task_delete(lxrtnettsk);

            printf("Error retrieving flags for device %s: %d\n",
                   flags_ifr.ifr_name, ret);
            exit(1);
        }
        flags[devices] = flags_ifr.ifr_flags;

        ifc.ifc_len -= sizeof(struct ifreq);
        devices++;
    }

    /* Switch over to soft realtime mode. */
    rt_make_soft_real_time();

    /* Close socket.
     * Note: call must be in soft-mode because socket was created as non-rt! */
    close_rt(sockfd);

    /* Delete realtime buddy. */
    rt_task_delete(lxrtnettsk);

    for (i = 0; i < devices; i++)
        printf("Device %s: IP %d.%d.%d.%d, flags 0x%08X\n", ifr[i].ifr_name,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr & 0xFF,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr >> 8 & 0xFF,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr >> 16 & 0xFF,
               ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr >> 24,
               flags[i]);

    return 0;
}
