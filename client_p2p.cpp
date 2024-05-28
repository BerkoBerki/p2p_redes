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
condition_variable cv;
bool dataReady = false;
FilePiece filepieces[1024];
TorrentFolder mytorrents;

mutex mtx_2;
bool stop_flag = false;

#define PIECE_LEN 262144

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
    bool pieza = 1;
    int fd_new;
    int peers[10];
    char buffer[255];
    char filename[255];
    char size[20];
    char idx[20];
    bzero(filename, 255);
    bzero(buffer, 255);
    for (int i = 0; i < 10; i++)
        peers[i] = 0;
    char buffer_p2p[1024];
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
                if (read_prot(aux_s, filename) == 0)
                {
                    cout << aux_s << " desconectado\n";
                    peers[i] = 0;
                }
                else
                {
                    pieza = 1;
                    cout << "Filename: " << filename << endl;
                    fd_new = open(filename, O_CREAT | O_RDWR, 0700);
                    read_prot(aux_s, size);
                    cout << "Tamano de pieza: " << size << endl;
                    read_prot(aux_s, idx);
                    cout << "Pieza numero: " << idx << endl;
                    for (int p = 0; p < 1024; p++)
                    {
                        if (filepieces[p].used == 1 && strcmp(filename, filepieces[p].filename) == 0)
                        {
                            if (filepieces[p].idx == atoi(idx))
                            {
                                write_prot(aux_s, "no", 2);
                                pieza = 0;
                            }
                        }
                    }
                    if (pieza)
                    {
                        write_prot(aux_s, "si", 2);
                        off = atoi(idx) * PIECE_LEN;
                        cout << "off: " << off << endl;
                        lseek(fd_new, off, SEEK_CUR);
                        for (int piece = 0; piece < 1024; piece++)
                        {
                            if (filepieces[piece].used == 0)
                            {
                                thread fil(actualizar_piezas, s);
                                createPiece(&filepieces[piece], filename, atoi(size), atoi(idx));
                                dataReady = true;
                                cv.notify_one();
                                fil.join();
                                break;
                            }
                        }
                        for (int k = 0; k < atoi(size); k++)
                        {
                            bzero(byte_, 1);
                            read(aux_s, byte_, 1);
                            write(fd_new, byte_, 1);
                        }
                    }
                    bzero(size, 20);
                    bzero(idx, 20);
                    bzero(filename, 255);
                    close(fd_new);
                }
            }
        }
    }
}
void waiting(int s)
{
    while (1)
    {
        cout << "Idle...\n";
        char sig[255];
        char filename[255];
        char address[20];
        char port[10];
        bzero(filename, 255);
        bzero(address, 20);
        bzero(port, 10);
        bzero(sig, 255);
        read_prot(s, sig);
        if (strcmp(sig, "signal") == 0)
        {
            char byte[1];
            cout << "Senal recibida\n";
            read_prot(s, filename);
            cout << "Filename: " << filename << endl;
            read_prot(s, address);
            cout << "Address: " << address << endl;
            read_prot(s, port);
            cout << "Port: " << port << endl;
            int fd = open(filename, O_RDWR, 0700);
            assert(fd > 0);
            int soc = socket(AF_INET, SOCK_STREAM, 0);
            struct hostent *server_;
            struct sockaddr_in serv_addr_;
            server_ = gethostbyname(address);
            if (server_ == NULL)
            {
                fprintf(stderr, "Error, no existe el host.\n");
                exit(0);
            }
            int portno = atoi(port);
            bzero((char *)&serv_addr_, sizeof(serv_addr_));
            serv_addr_.sin_family = AF_INET;
            bcopy((char *)server_->h_addr, (char *)&serv_addr_.sin_addr.s_addr, server_->h_length);
            serv_addr_.sin_port = htons(portno);
            if (connect(soc, (struct sockaddr *)&serv_addr_, sizeof(serv_addr_)) < 0)
                fprintf(stderr, "Error de conexion...\n");

            for (int j = 0; j < 1024; j++)
            {
                if (strcmp(filepieces[j].filename, filename) == 0)
                {
                    write_prot(soc, filename, strlen(filename));
                    write_prot(soc, to_string(filepieces[j].size).c_str(), strlen(to_string(filepieces[j].size).c_str()));
                    write_prot(soc, to_string(filepieces[j].idx).c_str(), strlen(to_string(filepieces[j].idx).c_str()));
                    char signal[3];
                    read_prot(soc, signal);
                    if (strcmp(signal, "no") == 0)
                        continue;
                    for (int k = 0; k < filepieces[j].size; k++)
                    {
                        bzero(byte, 1);
                        read(fd, byte, 1);
                        write(soc, byte, 1);
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
    for (int i = 0; i < 10; i++)
    {
        if (mytorrents.torrents[i].used == 0)
        {
            createTorrent(&mytorrents.torrents[i], torr_buff_msg.payload.torrent.name, PIECE_LEN, 0);
            mytorrents.torrents[i].length = torr_buff_msg.payload.torrent.length;
            mytorrents.torrents[i].num_pieces = torr_buff_msg.payload.torrent.num_pieces;
            for (int n = 0; n < mytorrents.torrents[i].length / mytorrents.torrents[i].piece_length; n++)
            {
                memcpy(mytorrents.torrents[i].pieces[n].hash, torr_buff_msg.payload.torrent.pieces[n].hash, 20);
            }
            cout << mytorrents.torrents[i].name << " descargado.\n";
            break;
        }
    }
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

void requestfile(int s)
{
    char buffer[255];
    char name[255];
    cout << "Estos son sus torrents: \n";
    showMyTorrents(mytorrents);
    cout << "Que archivo desea descargar?: ";
    bzero(buffer, 255);
    fgets(buffer, 255, stdin);
    bzero(name, 255);
    memcpy(name, buffer, strlen(buffer) - 1);
    for (int i = 0; i < 10; i++)
    {
        if (mytorrents.torrents[i].used == 1 && strcmp(mytorrents.torrents[i].name, name) == 0)
        {
            write_prot(s, mytorrents.torrents[i].name, strlen(mytorrents.torrents[i].name));
            break;
        }
        if (i == 9)
        {
            write_prot(s, "no", 2);
            cout << "No tienes ese torrent.\n";
            return;
        }
    }
}

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

void see_online(int s, Msg msg, Clients *clients_)
{
    recvMsg(s, &msg);
    showClients(&msg.payload.client);
    clients_ = &msg.payload.client;
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

    while (1)
    {

        cout << consolename << "@ibtorr> ";
        bzero(buffer, 1024);
        fgets(buffer, 1024, stdin);

        if (strcmp(buffer, "idle\n") == 0)
            waiting(s);

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
            thread fil(actualizar_piezas, s);
            {
                char name_[255];
                bzero(name_, 255);
                cout << "Ingrese el nombre del archivo: ";
                fgets(name_, 255, stdin);
                char name_2[255];
                bzero(name_2, 255);
                memcpy(name_2, name_, strlen(name_) - 1);
                // cout << name_2 << endl;
                int fd = open(name_2, NULL);
                if (fd < 0)
                {
                    cout << "No tiene ese archivo!\n";
                    continue;
                }
                close(fd);
                Torrent torr_buff;

                createTorrent(&torr_buff, name_2, PIECE_LEN, 1);

                for (int j = 0; j < torr_buff.num_pieces; j++)
                {
                    if (j == torr_buff.num_pieces - 1 && torr_buff.length % PIECE_LEN != 0)
                    {
                        for (int piece = 0; piece < 1024; piece++)
                        {
                            if (filepieces[piece].used == 0)
                            {
                                createPiece(&filepieces[piece], name_2, torr_buff.length % PIECE_LEN, j);
                                break;
                            }
                        }
                    }
                    else
                    {
                        for (int piece = 0; piece < 1024; piece++)
                        {
                            if (filepieces[piece].used == 0)
                            {
                                createPiece(&filepieces[piece], name_2, PIECE_LEN, j);
                                break;
                            }
                        }
                    }
                }
                dataReady = true;
                cv.notify_one();
                cout << name_2 << " anadido.\n";
            }
            fil.join();
        }
        if (strcmp(buffer, "online\n") == 0)
        {
            write_prot(s, "online", 6);
            see_online(s, msg_clients, &clients);
            bzero(buffer, 1024);
        }
        if (strcmp(buffer, "mytorr\n") == 0)
        {
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
            {
                write_prot(s, "reqf", 4);
                requestfile(s);
            }
        }
    }

    oth.join();
    close(s);
    return 0;
}