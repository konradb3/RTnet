/*
 * linux_server
 *
 * server part for linux - listens and sends back a packet
 *
 * bases on Ulrich Marx's linux_client.c 
 *
 * Copyright (C) 2002, Ulrich Marx <marx@fet.uni-hannover.de>
 *		 2002, Marc Kleine-Budde <kleine-budde@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define RX_PORT	36000
#define TX_PORT	35999

#define MAX_MESG_SIZE 1500
char mesg[MAX_MESG_SIZE];

static int end = 0;

static void endme(int dummy)
{
	end =  1;
}

int main (int argc, char *argv[])
{
	int udpSocket    = 0;
        int local_port   = RX_PORT;
        int status       = 0;
        int size         = 0;
        int clientLength = 0;
	int server_port  = TX_PORT;
	char *server_ip;

	struct sockaddr_in serverName;
        struct sockaddr_in clientName;

	signal(SIGINT, endme);

	if (!(argc == 2 || argc == 4)) {
		fprintf(stderr, "Usage: %s <client-ip>\n", argv[0]);
		fprintf(stderr, "Usage: %s <local-port> <client-ip> <client-port>\n", argv[0]);
		exit(1); 
   	}
	
	if (argc == 2) {
		server_ip   = argv[1];
	} else {
		server_ip   = argv[2];
		server_port = atoi(argv[3]);
		local_port  = atoi(argv[1]);
	}

	memset(&mesg, 0, MAX_MESG_SIZE);

	udpSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSocket < 0) {
		perror("socket()");
		exit(1);
	}

	memset(&serverName, 0, sizeof(serverName));
	serverName.sin_family = AF_INET;
	serverName.sin_addr.s_addr = htonl(INADDR_ANY);
	serverName.sin_port = htons(local_port);
	status = bind(udpSocket, (struct sockaddr *) &serverName, sizeof(serverName));
	if (status < 0) { 
		perror("bind()");
		exit(1);
	}

	memset(&clientName, 0, sizeof(clientName));
	clientName.sin_family = AF_INET;
	inet_aton(server_ip, &clientName.sin_addr);
	clientName.sin_port = htons(server_port);
	clientLength=sizeof(clientName);

	while (!end) {
		size = recvfrom(udpSocket, mesg, MAX_MESG_SIZE, 0, (struct sockaddr *) &clientName, &clientLength);
		sendto (udpSocket, mesg, size, 0, (struct sockaddr *) &clientName, clientLength);
	}

	return 0;
}
