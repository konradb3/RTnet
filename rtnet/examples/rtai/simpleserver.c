/* simpleserver.c
 *
 * simpleserver - a simple server for demonstration of rtnet's lxrt interface
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

#include <rtai_lxrt.h>
#include <rtnet.h>

static struct sockaddr_in local_addr;

int main(int argc, char *argv[]) {
    int sockfd = 0;
    int ret    = 0;

    char msg[4000];

    RT_TASK *lxrtnettsk;

    /* Set variables to zero.  */
    memset(msg, 0, sizeof(msg));
    memset(&local_addr, 0, sizeof (struct sockaddr_in));

    printf("RTnet, simpleserver for LXRT\n");

    /* Check arguments and set addresses. */
    if (argc == 2) {
        local_addr.sin_family      = AF_INET;
        local_addr.sin_addr.s_addr = INADDR_ANY;
        local_addr.sin_port        = htons(atoi(argv[1]));
    } else {
        fprintf(stderr,
                "Usage: "
                "%s <local-port>\n",
                argv[0]);
        exit(1);
    }

    /* Lock allocated memory into RAM. */
    mlockall(MCL_CURRENT|MCL_FUTURE);

    /* Create new socket. */
    sockfd = rt_dev_socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {

        printf("Error opening socket: %d\n", sockfd);
        exit(1);
    }

    /* Initialize a real time buddy. */
    lxrtnettsk = rt_task_init(4900, 1, 0, 0);
    if (NULL == lxrtnettsk) {
        rt_dev_close(sockfd);
        printf("CANNOT INIT MASTER TASK\n");
        exit(1);
    }

    /* Switch over to hard realtime mode. */
    rt_make_hard_real_time();

    /* Bind socket to local address specified as parameter. */
    ret = rt_dev_bind(sockfd, (struct sockaddr *) &local_addr,
                      sizeof(struct sockaddr_in));

    /* Block until packet is received. */
    ret = rt_dev_recv(sockfd, msg, sizeof(msg), 0);

    /* Switch over to soft realtime mode. */
    rt_make_soft_real_time();

    /* Close socket.
     * Note: call must be in soft-mode because socket was created as non-rt! */
    rt_dev_close(sockfd);

    /* Delete realtime buddy. */
    rt_task_delete(lxrtnettsk);

    printf("Received message: \"%s\"\n", msg);

    return 0;
}
