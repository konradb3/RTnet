/* simpleclient.c
 *
 * simpleclient - a simple client for demonstration of rtnet_lxrt
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
#include <rtnet.h>

static struct sockaddr_in local_addr;
static struct sockaddr_in server_addr;

int main(int argc, char *argv[]) {
    int sockfd = 0;
    int ret    = 0;

    RT_TASK *lxrtnettsk;
    char msg[] = "This message was sent using rtnet_lxrt.";

    /* Set address structures to zero.  */
    memset(&local_addr, 0, sizeof(struct sockaddr_in));
    memset(&server_addr, 0, sizeof(struct sockaddr_in));

    printf("RTnet, simpleclient for NEWLXRT\n");

    /* Check arguments and set addresses. */
    if (argc == 4) {
        local_addr.sin_family      = AF_INET;
        local_addr.sin_addr.s_addr = INADDR_ANY;
        local_addr.sin_port        = htons(atoi(argv[1]));

        server_addr.sin_family = AF_INET;
        inet_aton(argv[2], &server_addr.sin_addr);
        server_addr.sin_port = htons(atoi(argv[3]));
    } else {
        fprintf(stderr,
            "Usage: "
            "%s <local-port> "
            "<server-ip> <server-port>\n",
            argv[0]);
        exit(1);
    }

    /* Lock allocated memory into RAM. */
    mlockall(MCL_CURRENT|MCL_FUTURE);

    /* Initialize a real time buddy. */
    lxrtnettsk = rt_task_init(4800, 1, 0, 0);
    if (NULL == lxrtnettsk) {
        printf("CANNOT INIT MASTER TASK\n");
        exit(1);
    }

    /* Switch over to hard realtime mode. */
    rt_make_hard_real_time();

    /* Create new socket. */
    sockfd = rt_socket(AF_INET, SOCK_DGRAM, 0);

    /* Bind socket to local address specified as parameter. */
    ret = rt_socket_bind(sockfd, (struct sockaddr *) &local_addr,
                         sizeof(struct sockaddr_in));

    /* Specify destination address for socket; needed for rt_socket_send(). */
    rt_socket_connect(sockfd, (struct sockaddr *) &server_addr,
                      sizeof(struct sockaddr_in));

    /* Send a message. */
    rt_socket_send(sockfd, msg, sizeof(msg), 0);

    /* Close socket. */
    rt_socket_close(sockfd);

    /* Switch over to soft realtime mode. */
    rt_make_soft_real_time();

    /* Delete realtime buddy. */
    rt_task_delete(lxrtnettsk);

    return 0;
}
