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
static struct sockaddr_in server_addr;

void show_ips(void) 
{
	printf("local : %x:%d\n", ntohl(local_addr.sin_addr.s_addr), ntohs(local_addr.sin_port));
	printf("server: %x:%d\n", ntohl(server_addr.sin_addr.s_addr), ntohs(server_addr.sin_port));
}


int main(int argc, char *argv[])
{
	int sockfd	= 0;
	int ret		= 0;
        int end		= 0;

        char msg[4000];

        RT_TASK *lxrtnettsk;
	union {unsigned long long l; unsigned char c[8];} time_tx, time_rx;

	//	memset (&msg, 0, 2000);

	memset (msg, 0, 2000);

	memset(&local_addr, 0, sizeof (struct sockaddr_in));
	memset(&server_addr, 0, sizeof (struct sockaddr_in));
	
        if (argc==3) {
		server_addr.sin_family=AF_INET;
		inet_aton(argv[1], &server_addr.sin_addr);
		server_addr.sin_port=htons(atoi(argv[2]));
		local_addr.sin_family=AF_INET;
        } else {
		if (argc==5) {
			local_addr.sin_family=AF_INET;
			inet_aton(argv[1], &local_addr.sin_addr);
			local_addr.sin_port=htons(atoi(argv[2]));
			server_addr.sin_family=AF_INET;
			inet_aton(argv[3], &server_addr.sin_addr);
			server_addr.sin_port=htons(atoi(argv[4]));
		}
		else {
	                fprintf(stderr, "1.) Usage: %s <server-ip> <server-port>\n", argv[0]);
			fprintf(stderr, "2.) Usage: %s <local-ip> <local-port> <server-ip> <server-port>\n", argv[0]);
	                exit(1);
		}
	}
	show_ips();

        if (!(lxrtnettsk = rt_task_init(4800, 1, 0, 0))) {
		printf("CANNOT INIT MASTER TASK\n");
		exit(1);
        }

        rt_set_oneshot_mode();
        start_rt_timer(nano2count(1000000));
		
		rt_make_soft_real_time();
	//	rt_make_hard_real_time();

        printf("bind\n");
	sockfd=rt_socket(AF_INET,SOCK_DGRAM, 0);
       	ret=rt_socket_bind(sockfd, (struct sockaddr *) &local_addr, sizeof(struct sockaddr_in));

	printf("connect\n");
	rt_socket_connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));


	printf("start\n");
	time_tx.l=rt_get_time_ns();

	printf("send packet\n");
	rt_socket_send(sockfd, &time_tx.l, sizeof(unsigned long long), 0);

	while (!end) {
		if ( (end=rt_socket_recv(sockfd, msg, sizeof(unsigned long long), 0)) > 0 )  {
			memcpy (&time_rx.l, msg, sizeof(unsigned long long));
			printf ("round trip time %d ns \n", (int)(time_rx.l-time_tx.l));
			memset (&msg, 0, 1000);
		}
       	}

	rt_socket_close(sockfd);	

        stop_rt_timer();
        rt_task_delete(lxrtnettsk);

	return 0;
}





