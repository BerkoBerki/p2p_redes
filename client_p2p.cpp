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
#include <condition_variable>

using namespace std;

mutex mtx;
mutex mtx_3;
condition_variable cv;
bool dataReady = false;
FilePiece filepieces[1024];
TorrentFolder mytorrents;

condition_variable cv_2;
bool stopflag = false;

mutex mtx_2;

void actualizar_piezas(int s)
{
    unique_lock<mutex> lock(mtx);
    cv.wait(lock, []
            { return dataReady; });
    for (int i = 0; i < 1024; i++)
    {
        if (filepieces[i].sent == 0)
        {
            write_prot(s, "pieces", 6);
            Msg msg_pieces;
            setMsgPiece(&msg_pieces, &filepieces[i]);
            sendMsg(s, &msg_pieces);
            filepieces[i].sent = 1;
        }
    }
    dataReady = false;
}

void hashcheck(TorrentFolder mytorrs, string filename)
{
    lock_guard<mutex> lock(mtx_2);
    int counter = 1;
    for (int i = 0; i < 10; i++)
    {
        if (strcmp(filename.c_str(), mytorrs.torrents[i].name) == 0)
        {
            int fd = open(filename.c_str(), NULL);
            assert(fd > 0);
            unsigned char hash[200];
            for (int j = 0; j < mytorrs.torrents[i].num_pieces - 1; j++)
            {
                bzero(hash, 200);
                char bytes[PIECE_LEN];
                bzero(bytes, PIECE_LEN);
                read(fd, bytes, PIECE_LEN);

                SHA1((unsigned char *)bytes, sizeof(bytes) - 1, hash);

                if (strcmp((const char *)hash, (const char *)mytorrs.torrents[i].pieces[j].hash) == 0)
                    counter++;
            }
            cout << "Hash check: " << counter << "/" << mytorrs.torrents[i].num_pieces << " partes ok.\n";
            close(fd);
            break;
        }
    }
}

void other_peers(int myport, int s)
{
    int fd_new;
    int peers[10];
    char buffer[255];

    bzero(buffer, 255);
    for (int i = 0; i < 10; i++)
        peers[i] = 0;

    struct sockaddr_in peers_addr;
    int opt = 1;
    off_t off;
    fd_set readfds;
    char byte_[1];
    int max_aux_s, aux_s, activ, new_socket;
    int socki;
    socki = socket(AF_INET, SOCK_STREAM, 0);
    if (socki < 0)
        cout << "Error creando socket en other peers.\n";
    if (setsockopt(socki, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
        cout << "Error seteando socket.\n";
    peers_addr.sin_family = AF_INET;
    peers_addr.sin_addr.s_addr = INADDR_ANY;
    peers_addr.sin_port = htons(myport);
    int addrlen = sizeof(peers_addr);
    if (bind(socki, (struct sockaddr *)&peers_addr, sizeof(peers_addr)) < 0)
        cout << "Error en el bind.\n";
    if (listen(socki, 5) < 0)
        cout << "Erorr en listen.\n";

    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(socki, &readfds);
        max_aux_s = socki;
        for (int i = 0; i < 10; i++)
        {
            aux_s = peers[i];
            if (aux_s > 0)
                FD_SET(aux_s, &readfds);
            if (aux_s > max_aux_s)
                max_aux_s = aux_s;
        }
        activ = select(max_aux_s + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(socki, &readfds))
        {
            new_socket = accept(socki, (struct sockaddr *)&peers_addr, (socklen_t *)&addrlen);
            for (int i = 0; i < 10; i++)
            {
                if (peers[i] == 0)
                {
                    peers[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < 10; i++)
        {
            aux_s = peers[i];
            if (FD_ISSET(aux_s, &readfds))
            {
                if (read_prot(aux_s, buffer) == 0)
                {
                    cout << aux_s << " desconectado\n";
                    peers[i] = 0;
                }
                else
                {
                    char idx[10];
                    bzero(idx, 10);
                    read_prot(aux_s, idx);
                    for (int h = 0; h < 1024; h++)
                    {
                        if (strcmp(filepieces[h].filename, buffer) == 0 && filepieces[h].idx == atoi(idx))
                        {
                            write_prot(aux_s, to_string(filepieces[h].size).c_str(), strlen(to_string(filepieces[h].size).c_str()));
                            write_prot(aux_s, to_string(filepieces[h].idx).c_str(), strlen(to_string(filepieces[h].idx).c_str()));
                            int fd = open(buffer, O_RDWR, 0700);
                            lseek(fd, atoi(idx) * PIECE_LEN, SEEK_SET);
                            assert(fd > 0);
                            char byte[filepieces[h].size];
                            bzero(byte, filepieces[h].size);
                            read(fd, byte, filepieces[h].size);
                            write_prot(aux_s, byte, filepieces[h].size);
                        }
                    }
                }
            }
        }
    }
}
void dwnld(int s)
{
    lock_guard<mutex> lock(mtx_2);
    Msg torr_buff_msg;
    recvMsg(s, &torr_buff_msg);
    addTorrent(&mytorrents, torr_buff_msg.payload.torrent);
}

void create_torr(int s)
{
    lock_guard<mutex> lock(mtx_2);
    char asd[255];
    bzero(asd, 255);
    cout << "Ingrese el nombre del archivo: ";
    fgets(asd, 255, stdin);
    std::string input(asd);
    size_t pos = input.find('\n');
    if (pos != std::string::npos)
    {
        input = input.substr(0, pos);
    }
    Msg msg;
    int idx;
    for (int i = 0; i < 10; i++)
    {
        if (mytorrents.torrents[i].used == 0)
        {
            idx = i;
            createTorrent(&mytorrents.torrents[i], input.c_str(), PIECE_LEN, 1);
            break;
        }
    }

    setMsgTorr(&msg, mytorrents.torrents[idx]);
    sendMsg(s, &msg);
}

void requestfile(int s, char *buffer)
{

    cout << "Estos son sus torrents: \n";
    showMyTorrents(mytorrents);
    cout << "Que archivo desea descargar?: ";
    bzero(buffer, 255);
    fgets(buffer, 255, stdin);
    int idx = checkTorr(mytorrents, buffer);
    cout << "DALE " <<  mytorrents.torrents[idx].pieces[0].hash << endl;
    if (idx != -1)
    {
        write_prot(s, "yes", 4);
        Msg torr;
        bzero(&torr, sizeof(Msg));
        setMsgTorr(&torr, mytorrents.torrents[idx]);
        cout << "hash: " << torr.payload.torrent.pieces[0].hash << endl;
        sendMsg(s, &torr);
    }
    else
        write_prot(s, "no", 3);
    bzero(buffer, 255);
}

/* void requestfile(int s)
{
    char buffer[255];
    char name[255];
    cout << "Estos son sus torrents: \n";
    showMyTorrents(mytorrents);
    cout << "Que archivo desea descargar?: ";
    bzero(buffer, 255);
    PeerInfoParts peers_con_partes;
    inic_parts(&peers_con_partes);
    Msg a_mi;
    fgets(buffer, 255, stdin);
    bzero(name, 255);
    memcpy(name, buffer, strlen(buffer) - 1);
    for (int i = 0; i < 10; i++)
    {
        if (mytorrents.torrents[i].used == 1 && strcmp(mytorrents.torrents[i].name, name) == 0)
        {
            write_prot(s, mytorrents.torrents[i].name, strlen(mytorrents.torrents[i].name));
            recvMsg(s, &a_mi);
            for (int hj = 0; hj < 10; hj++)
            {
                if (a_mi.payload.parts.peers[hj].used)
                {
                    setPeerInfo(&peers_con_partes.peers[hj], a_mi.payload.parts.peers[hj].parts, a_mi.payload.parts.peers[hj].address, a_mi.payload.parts.peers[hj].port);
                }
            }
            break;
        }
        if (i == 9)
        {
            write_prot(s, "no", 2);
            cout << "No tienes ese torrent.\n";
            return;
        }
    }
    int fd = open(name, O_CREAT | O_RDWR, 0700);
    assert(fd > 0);
    for (int pers = 0; pers < 10; pers++)
    {
        if (peers_con_partes.peers[pers].used)
        {
            int soc = socket(AF_INET, SOCK_STREAM, 0);
            struct hostent *server_;
            struct sockaddr_in serv_addr_;
            server_ = gethostbyname(peers_con_partes.peers[pers].address);
            if (server_ == NULL)
            {
                fprintf(stderr, "Error, no existe el host.\n");
                exit(0);
            }
            int portno = peers_con_partes.peers[pers].port;
            bzero((char *)&serv_addr_, sizeof(serv_addr_));
            serv_addr_.sin_family = AF_INET;
            bcopy((char *)server_->h_addr, (char *)&serv_addr_.sin_addr.s_addr, server_->h_length);
            serv_addr_.sin_port = htons(portno);
            if (connect(soc, (struct sockaddr *)&serv_addr_, sizeof(serv_addr_)) < 0)
                fprintf(stderr, "Error de conexion...\n");
            if (server_ == NULL)
            {
                fprintf(stderr, "Error, no existe el host.\n");
                exit(0);
            }
            for (int partss = 0; partss < peers_con_partes.peers[pers].parts; partss++)
            {
                write_prot(soc, name, strlen(name));
                write_prot(soc, to_string(partss).c_str(), strlen(to_string(partss).c_str()));
                char size[20];
                char idx[20];
                bzero(idx, 20);
                bzero(size, 20);
                read_prot(soc, size);
                read_prot(soc, idx);
                lseek(fd, atoi(idx) * PIECE_LEN, SEEK_SET);
                char byte[atoi(size)];
                bzero(byte, atoi(size));
                read_prot(soc, byte);
                write(fd, byte, atoi(size));
            }
        }
    }
    cout << "done\n";
} */

void requestTorrent(int s, TorrentFolder *torrent, Msg *msg)
{
    char torrname[255];
    bzero(torrname, 255);
    cout << "Torrent a descargar: ";
    fgets(torrname, 255, stdin);
    write_prot(s, torrname, strlen(torrname) - 1);
    bzero(torrname, 255);
    read_prot(s, torrname);
    if (strcmp(torrname, "found") == 0)
    {
        cout << "Descargando ...\n";
        recvMsg(s, msg);
        for (int i = 0; i < 10; i++)
        {
            if (strcmp(torrent->torrents[i].name, "null") == 0)
            {
                createTorrent(&torrent->torrents[i], msg->payload.torrent.name, PIECE_LEN, 1);
                cout << "Torrent descargado: \n";
                showTorrInfo(torrent->torrents[i]);
                break;
            }
        }
    }
    else
        cout << "Torrent no encontrado.\n";
}

int main(int argc, char *argv[])
{
    Msg msg_clients;
    Msg msg_torr;
    inic_torrents(&mytorrents);
    Clients clients;
    inic_pieces(filepieces);
    struct sockaddr_in serv_addr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0)
        printf("Error abriendo el socket.");

    struct hostent *server;

    server = gethostbyname(argv[1]);

    if (server == NULL)
    {
        fprintf(stderr, "Error, no existe el host.\n");
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
        printf("✧✧✧ IBTORRENT ✧✧✧\n");

    char buffer[1024];
    char myname[255];
    char my_port[20];
label1:
    bzero(myname, 255);

    cout << "Ingrese su username: ";
    fgets(myname, 255, stdin);
    write_prot(s, myname, strlen(myname) - 1);

    char signal[10];
    bzero(signal, 10);
    read_prot(s, signal);
    if (strcmp(signal, "used") == 0)
    {
        cout << "Ese usuario ya existe!\n";
        goto label1;
    }
    cout << "Ingrese su puerto: ";
    fgets(my_port, 20, stdin);
    write_prot(s, my_port, strlen(my_port));
    char consolename[255];
    bzero(consolename, 255);
    memcpy(consolename, myname, strlen(myname) - 1);

    thread oth(other_peers, atoi(my_port), s);
    // thread idle(waiting, s);

    while (1)
    {
        cout << consolename << "@ibtorr> ";
        bzero(buffer, 1024);
        fgets(buffer, 1024, stdin);

        /* if (strcmp(buffer, "idle\n") == 0)
            waiting(s); */

        if (strcmp(buffer, "hashcheck\n") == 0)
        {
            cout << "Ingrese el nombre del archivo: ";
            bzero(buffer, 1024);
            fgets(buffer, 1024, stdin);
            char name_hash[255];
            bzero(name_hash, 255);
            memcpy(name_hash, buffer, strlen(buffer) - 1);
            hashcheck(mytorrents, name_hash);
            bzero(buffer, 1024);
        }

        if (strcmp(buffer, "addfile\n") == 0)
        {
            /* thread fil(actualizar_piezas, s);
            { */
            write_prot(s, "add", 4);
            bzero(buffer, 255);
            cout << "Ingrese el nombre del archivo: ";
            fgets(buffer, 255, stdin);
            char name_2[255];
            bzero(name_2, 255);
            memcpy(name_2, buffer, strlen(buffer) - 1);
            int fd = open(name_2, NULL);
            if (fd < 0)
            {
                cout << "No tiene ese archivo!\n";
                continue;
            }

            char byte[1];
            int length = 0;
            while (read(fd, byte, 1))
            {
                length++;
            }
            cout << "Este archivo tiene " << length / PIECE_LEN + 1 << " partes. Ingrese indice inferior: ";
            char ii[10];
            bzero(ii, 10);
            fgets(ii, 10, stdin);
            cout << "Ingrese indice superior: ";
            char is[10];
            bzero(is, 10);
            fgets(is, 10, stdin);
            close(fd);
            Torrent torr_buff = createTorrent2(name_2, PIECE_LEN, 1, atoi(ii), atoi(is));
            showTorrInfo(torr_buff);
            addTorrent(&mytorrents, torr_buff);
            cout << "hash: " << torr_buff.pieces[0].hash << endl;
            Msg msg_torr_buff;
            bzero(&msg_torr_buff, sizeof(Msg));
            setMsgTorr(&msg_torr_buff, torr_buff);
            sendMsg(s, &msg_torr_buff);
            cout << name_2 << " anadido.\n";
        }

        if (strcmp(buffer, "mytorr\n") == 0)
        {
            cout << "helo\n";
            showMyTorrents(mytorrents);
            bzero(buffer, 1024);
        }
        if (strcmp(buffer, "newtorr\n") == 0)
        {
            write_prot(s, "createt", 7);
            create_torr(s);
            bzero(buffer, 1024);
        }

        if (strcmp(buffer, "reqt\n") == 0)
        {
            write_prot(s, "reqt", 4);
            char name[255];
            cout << "Nombre del torrent a descargar: ";
            bzero(name, 255);
            fgets(name, 255, stdin);
            write_prot(s, name, strlen(name) - 1);
            char conf[10];
            bzero(conf, 10);
            read_prot(s, conf);

            if (strcmp(conf, "found") == 0)
                dwnld(s);
            else
                cout << "Torrent no encotnrado.\n";
        }

        if (strcmp(buffer, "reqf\n") == 0)
        {
            write_prot(s, "reqf", 4);
            requestfile(s, buffer);
        }
    }

    // idle.join();
    oth.join();
    close(s);
    return 0;
}