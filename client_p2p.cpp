#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include "proto_p2p.h"
#include <thread>
#include <iostream>
using namespace std;

#define PORT_P2P 8008

void leer_mjes(int s, Msg &msg, Clients &clients_)
{
    while (recvMsg(s, &msg))
    {
        showClients(&msg.payload.client);
        clients_ = msg.payload.client;
    }
}
int main(int argc, char *argv[])
{

    // int rd, wr;
    Msg msg_clients;
    Clients clients;
    struct sockaddr_in serv_addr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0)
        printf("ERROR opening socket");

    struct hostent *server;

    server = gethostbyname(argv[1]);

    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    int portno = atoi(argv[2]);
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(s, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        fprintf(stderr, "Error de conexion...\n");

    else
        printf("Connected succesfully\n");

    char buffer[1024];
    char myname[255];
    bzero(buffer, 1024);
    bzero(myname, 255);
    cout << "Bienvenido. Ingrese su username: ";
    fgets(myname, 255, stdin);
    write(s, myname, strlen(myname) - 1);

    cout << "mje: ";
    fgets(buffer, 255, stdin);
    write(s, buffer, strlen(buffer));

    thread rec_cli(leer_mjes, s, std::ref(msg_clients), std::ref(clients));
    rec_cli.join();

    close(s);
    return 0;
}