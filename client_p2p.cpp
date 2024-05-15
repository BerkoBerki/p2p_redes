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

#define PORT_P2P 8000

void leer_mjes(int s, Msg &msg, Clients &clients_)
{
    while (recvMsg(s, &msg))
    {
        std::cout << "#####CLIENTES ONLINE#####" << '\n';
        showClients(&msg.payload.client);
        clients_ = msg.payload.client;
    }
}
void enviar_mje(Clients &clients_)
{
    char buffer[1024];
    struct hostent *some_peer;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(PORT_P2P);
    bind(s, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    const char *destAddresses[10];
    /// MANEJAR BIEN EL TEMA DE LOS CLIENTES, QUE ESTE BIEN DEFINIDO SI HAY UNO O NO, Y BIEN DEFINIDO MI NOMBRE 
    while (1)
    {
        
        cout << "mje: ";
        bzero(buffer, 1024);
        fgets(buffer, 1024, stdin);
        for (int i = 0; i < 10; i++)
        {
            if (clients_.peers[i].socket != 0 && clients_.peers[i].socket < 15)
            {
                some_peer = gethostbyname(clients_.peers[i].address);
                bzero((char *)&serverAddr, sizeof(serverAddr));
                serverAddr.sin_family = AF_INET;
                bcopy((char *)some_peer->h_addr, (char *)&serverAddr.sin_addr.s_addr, some_peer->h_length);
                serverAddr.sin_port = htons(PORT_P2P);
                cout << "connect: " << connect(s, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) << endl;
                cout << "write:" << write(s, buffer, strlen(buffer)) << endl;
                shutdown(s, SHUT_RDWR);
                close(s);
            }
        }
    }
}
void recibir_mje()
{
    struct sockaddr_in address;
    char buffer[1024];
    int new_socket;
    int s_p2p = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT_P2P);
    int addrlen = sizeof(address);
    bind(s_p2p, (struct sockaddr *)&address, sizeof(address));
    listen(s_p2p, 5);
    while (1)
    {
        new_socket = accept(s_p2p, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        read(new_socket, buffer, 1024);
        cout << buffer << '\n';
        close(new_socket);
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
    
    cout << "my name is " << myname << endl;

    thread rec_cli(leer_mjes, s, std::ref(msg_clients), std::ref(clients));
    thread env_mje(enviar_mje, std::ref(clients));
    thread rec_mje(recibir_mje);
    rec_cli.join();
    env_mje.join();
    rec_mje.join(); 

    /* recvMsg(s, &msg_clients);

    std::cout << "#####CLIENTES ONLINE#####" << '\n';
    showClients(&msg_clients.payload.client);
    clients = msg_clients.payload.client; */


    //getType(&msg_clients);
    


    close(s);
    return 0;
}