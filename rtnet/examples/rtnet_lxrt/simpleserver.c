/* simpleserver.c
 *
 * simpleserver - a simple server for demonstration of rtnet_lxrt
 * 06/2003 by Hans-Peter Bock <hpbock@avaapgh.de>
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
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rtai_lxrt_user.h>

#include <rtnet_lxrt.h>

static struct sockaddr_in local_addr;

int main(int argc, char *argv[]) {
        int sockfd = 0;
        int ret    = 0;

        char msg[4000];

        RT_TASK *lxrtnettsk;

        memset(msg, 0, sizeof(msg));

        memset(&local_addr, 0, sizeof (struct sockaddr_in));
        
        if (argc==3) {
                local_addr.sin_family=AF_INET;
                inet_aton(argv[1], &local_addr.sin_addr);
                local_addr.sin_port=htons(atoi(argv[2]));
        } else {
                fprintf(stderr,
                        "Usage: "
                        "%s <local-ip> <local-port>\n",
                        argv[0]);
                exit(1);
        }

        mlockall(MCL_CURRENT|MCL_FUTURE);
        lxrtnettsk = rt_task_init(4800, 1, 0, 0);
        if (NULL == lxrtnettsk) {
                printf("CANNOT INIT MASTER TASK\n");
                exit(1);
        }
        rt_set_oneshot_mode();
        start_rt_timer(nano2count(1000000));
        rt_make_hard_real_time();

        sockfd = rt_socket(AF_INET, SOCK_DGRAM, 0);
        ret = rt_socket_bind(sockfd,
	                     (struct sockaddr *) &local_addr,
                             sizeof(struct sockaddr_in));
        while (0 == ret) {
                rt_sleep(nano2count(100000000));
                ret = rt_socket_recv(sockfd,
                                     msg,
                                     sizeof(msg),
                                     0);
        }
        rt_socket_close(sockfd);        

        rt_make_soft_real_time();

        stop_rt_timer();
        rt_task_delete(lxrtnettsk);

        return 0;
}
