#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define MAX_MESG_SIZE 1500
char mesg[MAX_MESG_SIZE];


int main (int argc, char *argv[])
{
	int udpSocket    = 0;
        int port	 = 0;
        int status       = 0;
        int size         = 0;
        int clientLength = 0;

	struct sockaddr_in serverName;
        struct sockaddr_in clientName;

	if (argc!=2) {
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(1); 
   	}

	port  = atoi(argv[1]);

        udpSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udpSocket<0) {
                perror("socket()");
                exit(1);
        }

	memset(&serverName, 0, sizeof(serverName));
	serverName.sin_family = AF_INET;
	serverName.sin_addr.s_addr = htonl(INADDR_ANY);
	serverName.sin_port = htons(port);

	status = bind(udpSocket, (struct sockaddr *) &serverName, sizeof(serverName));
	if (status<0) { 
		perror("bind()");
		exit(1);
	}

	for (;;) {

		size = recvfrom(udpSocket, mesg, MAX_MESG_SIZE, 0, (struct sockaddr *) &clientName, &clientLength);
		if (size == -1) {
			perror("recvfrom()");
			exit(1);
		}
		printf ("RCV: %s from %s\n", mesg, inet_ntoa(clientName.sin_addr));
		memset (&mesg, 0, MAX_MESG_SIZE);		

		//		strcpy(mesg, "hallo von linux\n");
		//		sendto (udpSocket, mesg, strlen(mesg), 0, (struct sockaddr *) &clientName, clientLength);		
	}

	return 0;
}



