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
#include <cstring>
#include <string.h>
#include <sys/time.h>
#include <string>
#include "proto_p2p.h"
#include <iostream>
using namespace std;

#define PORT 8888


typedef struct
{
    int socket;
    char *address;
    int port;
} Peers;

int main(int argc, char *argv[])
{

    // Variables:
    Msg msg_clients;
    Clients clients;
    Msg msg; // buffer?
    int cs, i;
    fd_set readfds;
    int max_clients = 10;
    int opt = 1;
    struct sockaddr_in address;
    int rd;
    char buffer[1024];

    for (i = 0; i < max_clients; i++)
    {
        setSocket(&clients.peers[i], 0);
        setPeer(&clients.peers[i], 0, "null", "null");
    }

    showClients(&clients);

    if ((cs = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Error creando el socket");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(cs, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
    {
        perror("Error configurando el socket");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    int addrlen = sizeof(address);

    if (bind(cs, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("Error en el bind");
        exit(EXIT_FAILURE);
    }

    printf("Sala creada en el puerto %d. \n", PORT);

    if (listen(cs, 6) < 0)
    {
        perror("Error en el listen");
        exit(EXIT_FAILURE);
    }

    puts("Esperando jugadores ...");

    int max_aux_s, aux_s, activ, new_socket;

    while (1)
    {

        FD_ZERO(&readfds);

        FD_SET(cs, &readfds);
        max_aux_s = cs;

        for (i = 0; i < max_clients; i++)
        {

            aux_s = clients.peers[i].socket;

            if (aux_s > 0)
                FD_SET(aux_s, &readfds);

            if (aux_s > max_aux_s)
                max_aux_s = aux_s;
        }

        activ = select(max_aux_s + 1, &readfds, NULL, NULL, NULL);

        if ((activ < 0) && (errno != EINTR))
        {
            perror("Error en el select");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(cs, &readfds))
        {

            if ((new_socket = accept(cs, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("Error en el accept");
                exit(EXIT_FAILURE);
            }
            bzero(buffer, 1024);
            read(new_socket, buffer, 1024);
            printf("%s se ha conectado. Socket: %d. IP: %s. Puerto: %d\n", buffer, new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (i = 0; i < max_clients; i++)
            {
                if (clients.peers[i].socket == 0)
                {
                    setSocket(&clients.peers[i], new_socket);
                    setPeer(&clients.peers[i], ntohs(address.sin_port), buffer, inet_ntoa(address.sin_addr));
                    cout << "Peer ok\n";
                    break;
                }
            
            }

            setMsgClients(&msg_clients, &clients);

            for (i = 0; i < max_clients; i++)
            {
                if (clients.peers[i].socket != 0)
                    cout << sendMsg(clients.peers[i].socket, &msg_clients) << '\n';
            }
        }
        bzero(buffer, 1024);
        for (i = 0; i < max_clients; i++)
        {
            aux_s = clients.peers[i].socket;

            if (FD_ISSET(aux_s, &readfds))
            {

                if ((rd = read(aux_s, buffer, 1024)) == 0)
                {
                    getpeername(aux_s, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("%s desconectado. IP: %s. Puerto: %d.\n", getUserName(&clients.peers[i]), inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    setSocket(&clients.peers[i], 0);
                    setPeer(&clients.peers[i],0, "null", "null");
                    setMsgClients(&msg_clients, &clients);
                    for (int j = 0; j < max_clients; j++)
                    {
                        if (clients.peers[j].socket != 0)
                            cout << sendMsg(clients.peers[j].socket, &msg_clients) << '\n';
                    }
                    close(aux_s);
                }
            }
        } 
    }
    return 0;
}
