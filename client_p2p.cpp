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

#define PORT_P2P 8001

void requestFile(int s, Torrent torrent) {
    write(s, torrent.info.name, strlen(torrent.info.name));
}

void sendFileToAdd(int s, char *buffer_)
{
    cout << "Ingrese el nombre del archivo: ";
    bzero(buffer_, 1024);
    fgets(buffer_, 1024, stdin);
    char name[1024];
    memcpy(name, buffer_, strlen(buffer_) - 1);
    ifstream file(name, ios::binary);
    if (file.is_open())
    {
        write(s, buffer_, strlen(buffer_) - 1);
        cout << name << " agregado!\n";
        file.close();
    }
    else
        cout << "File not found\n";
}

void other_peers()
{
    int peers[10];
    char buffer_p2p[1024];
    struct sockaddr_in peers_addr;
    int opt = 1;
    fd_set readfds;
    int max_aux_s, aux_s, activ, new_socket;
    int cs = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(cs, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
    peers_addr.sin_family = AF_INET;
    peers_addr.sin_addr.s_addr = INADDR_ANY;
    peers_addr.sin_port = htons(PORT_P2P);
    int addrlen = sizeof(peers_addr);
    bind(cs, (struct sockaddr *)&peers_addr, sizeof(peers_addr));
    listen(cs, 5);
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(cs, &readfds);
        max_aux_s = cs;
        for (int i = 0; i < 10; i++)
        {

            aux_s = peers[i];

            if (aux_s > 0)
                FD_SET(aux_s, &readfds);

            if (aux_s > max_aux_s)
                max_aux_s = aux_s;
        }
        activ = select(max_aux_s + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(cs, &readfds))
        {
            new_socket = accept(cs, (struct sockaddr *)&peers_addr, (socklen_t *)&addrlen);
            for (int i = 0; i < 10; i++)
            {
                if (peers[i] == 0)
                {
                    peers[i] = new_socket;
                    break;
                }
            }
        }
        bzero(buffer_p2p, 1024);
        for (int i = 0; i < 10; i++)
        {
            aux_s = peers[i];

            if (FD_ISSET(aux_s, &readfds))
            {
                // do something
            }
        }
    }
}

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
    Info info_torr;
    Torrent mytorrents[10];
    Clients clients;
    struct sockaddr_in serv_addr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    setInfo(&info_torr, "Tux.bmp");
    createTorrent(&mytorrents[0], info_torr);

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

    bzero(myname, 255);

    cout << "Bienvenido. Ingrese su username: ";
    fgets(myname, 255, stdin);
    write(s, myname, strlen(myname) - 1);

    thread rec_cli(leer_mjes, s, std::ref(msg_clients), std::ref(clients));

    while (1)
    {   
        cout << "ibtorr> ";
        bzero(buffer, 1024);
        fgets(buffer, 1024, stdin);

        if (strcmp(buffer, "addfile\n") == 0) {
            write(s, "addfile", 7);
            sendFileToAdd(s, buffer);
        }
        if (strcmp(buffer, "reqf\n") == 0) {
            write(s, "reqf", 4);
            requestFile(s, mytorrents[0]);
        }
    }

    rec_cli.join();

    close(s);
    return 0;
}