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
    int                 udpSocket;
    int                 local_port;
    int                 status;
    int                 size;
    socklen_t           clientLength;
    int                 server_port;
    char                *server_ip;
    struct sockaddr_in  serverName;
    struct sockaddr_in  clientName;


    if (argc!=4) {
        fprintf(stderr, "Usage: %s <local-port> <server-ip> <server-port>\n",
                argv[0]);
        exit(1);
    }

    local_port  = atoi(argv[1]);
    server_ip   = argv[2];
    server_port = atoi(argv[3]);

    memset(&mesg, 0, MAX_MESG_SIZE);

    udpSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket<0) {
        perror("socket()");
        exit(1);
    }

    memset(&serverName, 0, sizeof(serverName));
    serverName.sin_family = AF_INET;
    serverName.sin_addr.s_addr = htonl(INADDR_ANY);
    serverName.sin_port = htons(local_port);
    status = bind(udpSocket, (struct sockaddr *) &serverName,
                  sizeof(serverName));
    if (status<0) {
        perror("bind()");
        exit(1);
    }


    memset(&clientName, 0, sizeof(clientName));
    clientName.sin_family = AF_INET;
    inet_aton(server_ip, &clientName.sin_addr);
    clientName.sin_port = htons(server_port);
    clientLength = sizeof(clientName);

    for (;;) {
        strcpy(mesg, "hello from linux-stack\n");
        sendto(udpSocket, mesg, strlen(mesg), 0,
               (struct sockaddr *)&clientName, clientLength);
        memset(&mesg, 0, MAX_MESG_SIZE);

        size = recvfrom(udpSocket, mesg, MAX_MESG_SIZE, 0,
                        (struct sockaddr *)&clientName, &clientLength);
        if (size == -1) {
            perror("recvfrom()");
            exit(1);
        }
        printf("RCV: %s from %s\n", mesg, inet_ntoa(clientName.sin_addr));
        memset(&mesg, 0, MAX_MESG_SIZE);
    }

    return 0;
}
