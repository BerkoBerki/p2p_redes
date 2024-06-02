#include "proto_p2p.h"

#define PORT 8888

int main(int argc, char *argv[])
{
    Msg msg_clients;
    Msg msg_torr;
    Clients clients;
    int cs;
    fd_set readfds;
    int opt = 1;
    struct sockaddr_in address;
    int rd;
    char buffer[BUFF_SIZE];
    TorrentFolder torrents;
    inic_torrents(&torrents);

    for (int i = 0; i < MAXCLIENTS; i++)
    {
        setSocket(&clients.peers[i], 0);
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

    cout << "✧✧✧ IBTORRENT INICIADO EN " << PORT << " ✧✧✧\n";

    if (listen(cs, 6) < 0)
    {
        perror("Error en el listen");
        exit(EXIT_FAILURE);
    }

    cout << "✧✧✧ ESPERANDO PEERS ✧✧✧\n";

    int max_aux_s, aux_s, activ, new_socket;

    while (1)
    {

        FD_ZERO(&readfds);

        FD_SET(cs, &readfds);
        max_aux_s = cs;

        for (int i = 0; i < MAXCLIENTS; i++)
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
            perror("Error en el select.\n");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(cs, &readfds))
        {
            if ((new_socket = accept(cs, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("Error en el accept.\n");
                exit(EXIT_FAILURE);
            }
        label1:
            bzero(buffer, BUFF_SIZE);
            read_prot(new_socket, buffer);
            for (int i = 0; i < MAXCLIENTS; i++)
            {
                if (strcmp(buffer, clients.peers[i].username) == 0)
                {
                    write_prot(new_socket, "used", 4);
                    goto label1;
                }
                if (i == MAXCLIENTS - 1)
                    write_prot(new_socket, "ok", 2);
            }
            char peer_port[20];
            bzero(peer_port, 20);
            read_prot(new_socket, peer_port);
            printf("%s se conecto a la red. Socket: %d. IP: %s. Puerto: %d\n", buffer, new_socket, inet_ntoa(address.sin_addr), atoi(peer_port));

            for (int i = 0; i < MAXCLIENTS; i++)
            {
                if (clients.peers[i].socket == 0)
                {
                    setSocket(&clients.peers[i], new_socket);
                    setPeer(&clients.peers[i], atoi(peer_port), buffer, inet_ntoa(address.sin_addr));
                    break;
                }
            }
        }

        for (int i = 0; i < MAXCLIENTS; i++)
        {
            aux_s = clients.peers[i].socket;

            if (FD_ISSET(aux_s, &readfds))
            {
                bzero(buffer, BUFF_SIZE);
                if ((rd = read_prot(aux_s, buffer)) == 0)
                {
                    getpeername(aux_s, (struct sockaddr *)&address, (socklen_t *)&addrlen);
                    printf("%s desconectado. IP: %s. Puerto: %d.\n", clients.peers[i].username, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

                    setSocket(&clients.peers[i], 0);
                    close(aux_s);
                }
                else
                {
                    // Senal de que un peer quiere descargar un archivo.
                    if (strcmp(buffer, "reqf") == 0)
                    {
                        Msg torr;
                        recvMsg(aux_s, &torr); // El torrent que el peer quiere descargar.
                        IdxInfoInfo info;
                        info.cant = 0;
                        // Me fijo entre los demas peers quienes tienen piezas.
                        for (int c = 0; c < MAXCLIENTS; c++)
                        {
                            if (c != i) // Para que busque el torrent en sigo mismo.
                            {
                                for (int tors = 0; tors < MAXTORRENTS; tors++)
                                {
                                    if (strcmp(torr.payload.torrent.name, clients.peers[c].torrents.torrents[tors].name) == 0)
                                    {
                                        info.idxinfo[info.cant].idxlist.size = 0;
                                        info.idxinfo[info.cant].port = clients.peers[c].port;
                                        memcpy(info.idxinfo[info.cant].address, clients.peers[c].address, strlen(clients.peers[c].address));
                                        HashCheck hc = torrCompare(clients.peers[c].torrents.torrents[tors], torr.payload.torrent);
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
                        // Ahora uso la funcion que optimiza la repartija entre partes.
                        bool ch = 1;
                        int cant = info.cant;
                        while (ch && cant > 1)
                        {
                            ch = checkDuplicates(&info, cant);
                            // Si la funcion devuelve cero es porque en el peer con mas piezas ya no se repiten piezas,
                            // lo mande al fondo y paso a ver los que quedaron.
                            if (!ch)
                            {
                                cant--;
                                ch = 1;
                            }
                        }
                        // Opcional para ver como la funcion reparte las partes:

                        /* for (int cant = 0; cant < info.cant; cant++)
                        {
                            for(int x = 0 ; x<info.idxinfo[cant].idxlist.size; x++)
                                cout << info.idxinfo[cant].idxlist.idx[x] << " ";
                            cout << endl;
                        } */

                        // Se lo mando al peer para le pida las piezas a los otros peers (ya es independiente del tracker).
                        Msg msg_info;
                        setMsgInfo(&msg_info, info);
                        sendMsg(aux_s, &msg_info);
                    }
                    // Senal de que un peer quiere descargar un torrent.
                    if (strcmp(buffer, "reqt") == 0)
                    {
                        bzero(buffer, BUFF_SIZE);
                        read_prot(aux_s, buffer);
                        // Me fijo si algun peer tiene el torrent.
                        int idx = lookTorr(buffer, &clients, i);
                        // Si alguien lo tiene, se lo mando.
                        if (idx != -1)
                        {
                            Msg msg_torr;
                            bzero(&msg_torr, sizeof(Msg));
                            write_prot(aux_s, "found", 5);
                            Torrent torr = clients.peers[idx % 10].torrents.torrents[idx / 10];
                            cleanHash(&torr);
                            setMsgTorr(&msg_torr, torr);
                            cleanHash(&msg_torr.payload.torrent); // Tiene que estar vacio para el nuevo peer.
                            sendMsg(aux_s, &msg_torr);
                            addTorrent(&clients.peers[i].torrents, torr); // El tracker tiene que saber que el peer lo tiene.
                        }
                        else
                        {
                            write_prot(aux_s, "not", 3);
                        }
                        bzero(buffer, BUFF_SIZE);
                    }
                    // Senal de que un peer quiere crear un torrent a partir de un archivo que tiene, para que otros peers lo puedan
                    // descargar.
                    if (strcmp(buffer, "add") == 0)
                    {
                        Msg msg_torr;
                        recvMsg(aux_s, &msg_torr);
                        addTorrent(&clients.peers[i].torrents, msg_torr.payload.torrent);
                        bzero(buffer, BUFF_SIZE);
                    }
                    // Senal para actualizar los hashes de un torrent cuando un peer se descargo un archivo. 
                    // Seeding.

                    if (strcmp(buffer, "update") == 0)
                    {
                        char idx_torr[10];
                        char bytess[PIECE_LEN];
                        
                        read_prot(aux_s, idx_torr);
                        char idx_piece[10];
                        read_prot(aux_s, idx_piece);
                        
                        read_prot(aux_s, bytess);
                        SHA1((unsigned char *)bytess, sizeof(bytess), clients.peers[i].torrents.torrents[atoi(idx_torr)].pieces[atoi(idx_piece)].hash);
                        
                        bzero(buffer, BUFF_SIZE);
                    }
                }
            }
        }
    }
    return 0;
}
