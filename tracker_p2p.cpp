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
    TorrentFolder torrents;
    inic_torrents(&torrents);

    for (i = 0; i < max_clients; i++)
    {
        setSocket(&clients.peers[i], 0);
        setPeer(&clients.peers[i], 0, "null", "null");
    }

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
        label1:
            bzero(buffer, 1024);
            read_prot(new_socket, buffer);
            for (int i = 0; i < max_clients; i++)
            {
                if (strcmp(buffer, clients.peers[i].username) == 0)
                {
                    write_prot(new_socket, "used", 4);
                    goto label1;
                }
                if (i == max_clients - 1)
                    write_prot(new_socket, "ok", 2);
            }
            char hisport[20];
            bzero(hisport, 20);
            read_prot(new_socket, hisport);
            printf("%s se ha conectado a la red. Socket: %d. IP: %s. Puerto: %d\n", buffer, new_socket, inet_ntoa(address.sin_addr), atoi(hisport));

            for (i = 0; i < max_clients; i++)
            {
                if (clients.peers[i].socket == 0)
                {
                    setSocket(&clients.peers[i], new_socket);
                    setPeer(&clients.peers[i], atoi(hisport), buffer, inet_ntoa(address.sin_addr));
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
                if ((rd = read_prot(aux_s, buffer)) == 0)
                {
                    getpeername(aux_s, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("%s desconectado. IP: %s. Puerto: %d.\n", getUserName(&clients.peers[i]), inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    setSocket(&clients.peers[i], 0);
                    setPeer(&clients.peers[i], 0, "null", "null");
                    close(aux_s);
                }
                else
                {
                    if (strcmp(buffer, "reqf") == 0)
                    {
                        char signal[20];
                        bzero(signal, 20);
                        read_prot(aux_s, signal);
                        if (strcmp(signal, "no") == 0)
                            continue;
                        else
                        {
                            Msg torr;
                            bzero(&torr, sizeof(Msg));
                            recvMsg(aux_s, &torr);
                            IdxInfoInfo info;
                            info.cant = 0;

                            for (int cli = 0; cli < 10; cli++)
                            {
                                if (cli != i)
                                {
                                    for (int tors = 0; tors < 10; tors++)
                                    {
                                        if (strcmp(torr.payload.torrent.name, clients.peers[cli].torrents.torrents[tors].name) == 0)
                                        {
                                            info.idxinfo[info.cant].idxlist.size = 0;
                                            info.idxinfo[info.cant].port = clients.peers[cli].port;
                                            memcpy(info.idxinfo[info.cant].address, clients.peers[cli].address, strlen(clients.peers[cli].address));
                                            HashCheck hc = torrCompare(clients.peers[cli].torrents.torrents[tors], torr.payload.torrent);
                                            for (int num = 0; num < torr.payload.torrent.num_pieces; num++)
                                            {
                                                if (hc.vector[num])
                                                {
                                                    info.idxinfo[info.cant].idxlist.idx[info.idxinfo[info.cant].idxlist.size] = num;
                                                    info.idxinfo[info.cant].idxlist.size++;
                                                }
                                            }
                                            info.cant++;
                                        }
                                    }
                                }
                            }
                            bool ch = 1;
                            int cant = info.cant;
                            while(ch && cant>1)
                            {
                                ch = checkDuplicates(&info, cant);
                                if(!ch)
                                {
                                    ordenInfo(&info, cant);
                                    cant--;
                                    ch = 1;
                                }
                            }
                            

                            for (int cant = 0; cant < info.cant; cant++)
                            {
                                for(int x = 0 ; x<info.idxinfo[cant].idxlist.size; x++)
                                    cout << info.idxinfo[cant].idxlist.idx[x] << " ";
                                cout << endl;
                            }
                            Msg msg_info;
                            setMsgInfo(&msg_info, info);
                            sendMsg(aux_s, &msg_info);
                        }
                    }
                    if (strcmp(buffer, "add") == 0)
                    {
                        Msg msg_torr_buff;
                        recvMsg(aux_s, &msg_torr_buff);
                        addTorrent(&clients.peers[i].torrents, msg_torr_buff.payload.torrent);
                    }
                    if (strcmp(buffer, "update") == 0)
                    {
                        cout << "updating\n";
                        Msg torr_;
                        recvMsg(aux_s, &torr_);
                        int idx = checkTorr(clients.peers[i].torrents, torr_.payload.torrent.name);
                        bzero(&clients.peers[i].torrents.torrents[idx], sizeof(Torrent));
                        clients.peers[i].torrents.torrents[idx] = torr_.payload.torrent;
                    }

                    if (strcmp(buffer, "reqt") == 0)
                    {

                        bzero(buffer, 1024);
                        read_prot(aux_s, buffer);
                        int idx = lookTorr(buffer, &clients, i);
                        if (idx != -1)
                        {
                            Msg msg_torr_2;
                            bzero(&msg_torr_2, sizeof(Msg));
                            cout << "found\n";
                            write_prot(aux_s, "found", 5);
                            Torrent torr = clients.peers[idx % 10].torrents.torrents[idx / 10];
                            cleanHash(&torr);
                            setMsgTorr(&msg_torr_2, torr);
                            cleanHash(&msg_torr_2.payload.torrent);
                            sendMsg(aux_s, &msg_torr_2);
                            addTorrent(&clients.peers[i].torrents, torr);
                        }
                        else
                        {
                            write_prot(aux_s, "not", 3);
                        }
                    }
                    if (strcmp(buffer, "createt") == 0)
                    {
                        Msg msg_new_torr;
                        recvMsg(aux_s, &msg_new_torr);
                        for (int j = 0; j < 10; j++)
                        {
                            if (torrents.torrents[j].used == 0)
                            {
                                createTorrent(&torrents.torrents[j], msg_new_torr.payload.torrent.name, PIECE_LEN, 1);
                                showTorrInfo(torrents.torrents[j]);
                                cout << msg_new_torr.payload.torrent.name << " creado\n";
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    return 0;
}
