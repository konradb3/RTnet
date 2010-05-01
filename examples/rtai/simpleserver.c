/* simpleserver.c
 *
 * simpleserver - a simple server for demonstration of rtnet RTAI interface
 * 06/2003 by Hans-Peter Bock <hpbock@avaapgh.de>
 *
 * 2010 revised by Paolo Mantegazza <mantegazza@aero.polimi.it>
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
#include <signal.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rtnet.h>

static volatile int end;

void sigh(int sig)
{
	end = 1;
}

int main(int argc, char *argv[])
{
	RT_TASK *task;
	RTIME trp;
	int sockfd, hard_timer_running;
	struct sockaddr_in local_addr, from_addr;
	socklen_t from_len;
	struct { long long count; char msg[100]; } msg;

	signal(SIGTERM, sigh);
	signal(SIGINT,  sigh);
	signal(SIGHUP,  sigh);

/* Set variables to zero.  */
	memset(&local_addr, 0, sizeof (struct sockaddr_in));

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
		return 1;
	}

/* Create new socket. */
	sockfd = rt_dev_socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		printf("Error opening socket: %d\n", sockfd);
		return 1;
	}

/* Link the Linux process to RTAI. */
	if (!(hard_timer_running = rt_is_hard_timer_running())) {
		start_rt_timer(0);
	}
	task = rt_thread_init(nam2num("SMPSRV"), 1, 0, SCHED_OTHER, 0xF);
	if (task == NULL) {
		rt_dev_close(sockfd);
		printf("CANNOT LINK LINUX SIMPLESERVER PROCESS TO RTAI\n");
		return 1;
	}

/* Lock allocated memory into RAM. */
	printf("RTnet, simpleserver for RTAI (user space).\n");
	mlockall(MCL_CURRENT | MCL_FUTURE);

/* Switch over to hard realtime mode. */
	rt_make_hard_real_time();

/* Bind socket to local address specified as parameter. */
	rt_dev_bind(sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_in));

/* Block until packet is received. */
	while (!end) {
		rt_dev_recvfrom(sockfd, (void *)&msg, sizeof(msg), 0, (struct sockaddr *)&from_addr, &from_len);
		if (msg.count < 0) break;
		trp = msg.count;
		msg.count = rt_get_time_ns();
		rt_dev_sendto(sockfd, (void *)&msg, sizeof(long long) + strlen(msg.msg) + 1, 0, (struct sockaddr *)&from_addr, sizeof(from_addr));
		printf("Server received: count %lld, %s\n", trp, msg.msg);
	}


/* Switch over to soft realtime mode. */
	rt_make_soft_real_time();

/* Close socket, must be in soft-mode because socket was created as non-rt. */
	rt_dev_close(sockfd);

/* Unlink the Linux process from RTAI. */
	if (!hard_timer_running) {
		stop_rt_timer();
	}
	rt_task_delete(task);

	return 0;
}
