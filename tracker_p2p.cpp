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

    Msg msg_clients;
    Msg msg_torr;
    Clients clients;
    int cs, i;
    fd_set readfds;
    int max_clients = 10;
    int opt = 1;
    struct sockaddr_in address;
    int rd;
    char buffer[1024];

    Info info_sw;
    Torrent torrent_sw;
    Info info_tux;
    Torrent torrent_tux;

    setInfo(&info_tux, "tux.bmp", PIECE_LEN, 1);
    createTorrent(&torrent_tux, info_tux);

    
    setInfo(&info_sw, "sw.bmp", PIECE_LEN, 1);
    createTorrent(&torrent_sw, info_sw);

    
    showTorrInfo(torrent_tux);
    showTorrInfo(torrent_sw);
    

    for (i = 0; i < max_clients; i++)
    {
        setSocket(&clients.peers[i], 0);
        setPeer(&clients.peers[i], 0, "null", "null");
        inic_files(&clients.peers[i]);
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

    printf("Red creada en el puerto %d. \n", PORT);

    if (listen(cs, 6) < 0)
    {
        perror("Error en el listen");
        exit(EXIT_FAILURE);
    }

    puts("Esperando peers ...");

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
            printf("%s se ha conectado a la red. Socket: %d. IP: %s. Puerto: %d\n", buffer, new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (i = 0; i < max_clients; i++)
            {
                if (clients.peers[i].socket == 0)
                {
                    setSocket(&clients.peers[i], new_socket);
                    setPeer(&clients.peers[i], ntohs(address.sin_port), buffer, inet_ntoa(address.sin_addr));
                    break;
                }
            }
        }

        for (i = 0; i < max_clients; i++)
        {
            aux_s = clients.peers[i].socket;

            if (FD_ISSET(aux_s, &readfds))
            {
                bzero(buffer, 1024);
                if ((rd = read(aux_s, buffer, 1024)) == 0)
                {
                    getpeername(aux_s, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("%s desconectado. IP: %s. Puerto: %d.\n", getUserName(&clients.peers[i]), inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    setSocket(&clients.peers[i], 0);
                    setPeer(&clients.peers[i], 0, "null", "null");
                    setMsgClients(&msg_clients, &clients);
                    close(aux_s);
                }
                else
                {
                    if (strcmp(buffer, "addfile") == 0)
                    {
                        bzero(buffer, 1024);
                        read(aux_s, buffer, 1024);
                        if(strcmp(buffer, "no") == 0)
                            break;
                        else {
                            addFile(&clients.peers[i], buffer);
                            cout << clients.peers[i].username << " agrego " << buffer << " a su lista de archivos\n";
                            setMsgClients(&msg_clients, &clients);
                            bzero(buffer, 1024);
                        }
                    }
                    if (strcmp(buffer, "reqf") == 0)
                    {
                        int sockets[10];
                        int partes = 0;
                        recvMsg(aux_s, &msg_torr);
                        showTorrInfo(msg_torr.payload.torrent);
                        for (int j = 0; j < max_clients; j++)
                        {
                            if (clients.peers[j].socket != 0)
                            {
                                for (int k = 0; k < 10; k++)
                                {
                                    if (strcmp(clients.peers[j].files[k].filename, msg_torr.payload.torrent.info.name) == 0)
                                    {
                                        partes++;
                                        
                                        sockets[partes-1] = clients.peers[j].socket;
                                        write(clients.peers[j].socket, "signal", 6);
                                    }
                                }
                            }
                        }
                        if(partes > 0) 
                            write(aux_s, "si", 2);
                        int len_each = msg_torr.payload.torrent.info.length / partes;
                        for (int p = 0; p < partes; p++)
                        {
                            if (p == partes-1)
                            {
                                string len = to_string(len_each + msg_torr.payload.torrent.info.length % partes);
                                string parte = to_string(p+1);
                                write(sockets[p], msg_torr.payload.torrent.info.name, strlen(msg_torr.payload.torrent.info.name));
                                sleep(1);
                                write(sockets[p], len.c_str(), strlen(len.c_str()));
                                sleep(1);
                                write(sockets[p], parte.c_str(), strlen(parte.c_str()));
                            }
                            else
                            {
                                string parte = to_string(p+1);
                                string len = to_string(len_each);
                                write(sockets[p], msg_torr.payload.torrent.info.name, strlen(msg_torr.payload.torrent.info.name));
                                sleep(1);
                                write(sockets[p], len.c_str(), strlen(len.c_str()));
                                sleep(1);
                                write(sockets[p], parte.c_str(), strlen(parte.c_str()));
                                
                            }
                        }
                        
                    }
                    if (strcmp(buffer, "reqt") == 0)
                    {
                        bzero(buffer, 1024);
                        read(aux_s, buffer, 1024);
                        for (int i = 0; i < 2; i++)
                        {
                            if (strcmp(torrent_sw.info.name, buffer) == 0)
                            {
                                write(aux_s, "found", 5);
                                setMsgTorr(&msg_torr, torrent_sw);
                                sendMsg(aux_s, &msg_torr);
                                break;
                            }
                            if (strcmp(torrent_tux.info.name, buffer) == 0)
                            {
                                write(aux_s, "found", 5);
                                setMsgTorr(&msg_torr, torrent_tux);
                                sendMsg(aux_s, &msg_torr);
                                break;
                            }
                            if (i == 1)
                                write(aux_s, "not", 3);
                        }
                    }
                    if (strcmp(buffer, "online") == 0)
                    {
                        setMsgClients(&msg_clients, &clients);
                        sendMsg(clients.peers[i].socket, &msg_clients);
                    }
                }
            }
        }
    }
    return 0;
}
