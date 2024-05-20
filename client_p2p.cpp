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

#define PORT_P2P 8001
#define PIECE_LEN 262144

void recib_sign(int s)
{
    char signal[255];
    cout << "Idle...\n";
    char filename[255];
    char filesize_chr[255];
    char parte_ch[10];
    int parte;
    int filesize;
    while (1)
    {
        bzero(signal, 255);
        read(s, signal, 255);
        if (strcmp(signal, "signal") == 0)
        {
            bzero(filename, 255);
            read(s, filename, 255);
            cout << "Archivo solicitado: " << filename << endl;
            bzero(filesize_chr, 255);
            read(s, filesize_chr, 255);
            cout << "Tamano que me toca: " << filesize_chr << endl;
            bzero(parte_ch, 10);
            read(s, parte_ch, 10);
            cout << "Parte: " << parte_ch << endl;
            ifstream file(filename, ios::binary);
            assert(file.is_open());
            filesize = atoi(filesize_chr);
            parte = atoi(parte_ch);
            int soc = socket(AF_INET, SOCK_STREAM, 0);
            if (soc < 0)
                printf("Error creando el socket.");

            struct sockaddr_in serv;
            struct hostent *serv_;
            serv_ = gethostbyname("localhost");

            if (serv_ == NULL)
            {
                fprintf(stderr, "No existe el host.\n");
                exit(0);
            }
            int portno = 8000;
            bzero((char *)&serv, sizeof(serv));
            serv.sin_family = AF_INET;
            bcopy((char *)serv_->h_addr, (char *)&serv.sin_addr.s_addr, serv_->h_length);
            serv.sin_port = htons(portno);
            if (connect(soc, (struct sockaddr *)&serv, sizeof(serv)) < 0)
                fprintf(stderr, "Error de conexion...\n");
            sleep(parte);
            cout << "write filez: " << write(soc, filesize_chr, strlen(filesize_chr)) << endl;
            sleep(1);
            cout << "write parte: " << write(soc, parte_ch, strlen(parte_ch)) << endl;
            sleep(1);
            file.seekg((parte-1)*(filesize), ios::beg);
            char byte[1];
            for(int i = 0; i< filesize; i++) {
                file.get(byte[0]);
                write(soc, byte, 1);
                bzero(byte, 1);
            }
            break;
        }
        else
            continue;
    }
}

string requestFile(int s, TorrentFolder torrents, Msg *msg)
{
    char dwnld[255];
    cout << "Estos son sus torrents:\n";
    showMyTorrents(torrents);
    cout << "Que archivo desea descargar?: ";
    bzero(dwnld, 255);
    fgets(dwnld, 255, stdin);
    std::string input(dwnld);
    string name = input;
    size_t pos = input.find('\n');
    if (pos != std::string::npos)
    {
        input = input.substr(0, pos);
    }
    for (int i = 0; i < 10; i++)
    {
        if (strcmp(input.c_str(), torrents.torrents[i].info.name) == 0)
        {
            setMsgTorr(msg, torrents.torrents[i]);
            sendMsg(s, msg);
            break;
        }
        if (i == 9)
            cout << "Usted no tiene ese torrent.\n";
    }
    return name;
}

void requestTorrent(int s, TorrentFolder *torrent, Msg *msg)
{
    char torrname[255];
    bzero(torrname, 255);
    cout << "Torrent a descargar: ";
    fgets(torrname, 255, stdin);
    write(s, torrname, strlen(torrname) - 1);
    bzero(torrname, 255);
    read(s, torrname, 255);
    if (strcmp(torrname, "found") == 0)
    {
        cout << "Descargando ...\n";
        recvMsg(s, msg);
        for (int i = 0; i < 10; i++)
        {
            if (strcmp(torrent->torrents[i].info.name, "null") == 0)
            {
                createTorrent(&torrent->torrents[i], msg->payload.torrent.info);
                cout << "Torrent descargado: \n";
                showTorrInfo(torrent->torrents[i]);
                break;
            }
        }
    }
    else
        cout << "Torrent no encontrado.\n";
}

void sendFileToAdd(int s)
{
    cout << "Ingrese el nombre del archivo: ";
    char filename[255];
    bzero(filename, 255);
    fgets(filename, 255, stdin);
    std::string input(filename);
    string name = input;
    size_t pos = input.find('\n');
    if (pos != std::string::npos)
    {
        input = input.substr(0, pos);
    }
    ifstream file(input, ios::binary);
    if (file.is_open())
    {
        write(s, filename, strlen(filename) - 1);
        cout << input << " agregado!\n";
        file.close();
    }
    else {
        cout << "Archivo no encontrado!\n";
        write(s, "no", 2);
    }
}

void other_peers(string nombre, int myport)
{
    int rd;
    int peers[10];
    for (int i = 0; i < 10; i++)
        peers[i] = 0;
    char buffer_p2p[1024];
    struct sockaddr_in peers_addr;
    int opt = 1;
    fd_set readfds;
    int max_aux_s, aux_s, activ, new_socket;
    int socki;
    socki = socket(AF_INET, SOCK_STREAM, 0);
    cout << "Socket: " << socki << endl;
    cout << "Setsocket: " << setsockopt(socki, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) << endl;
    peers_addr.sin_family = AF_INET;
    peers_addr.sin_addr.s_addr = INADDR_ANY;
    peers_addr.sin_port = htons(myport);
    int addrlen = sizeof(peers_addr);
    cout << "Bind: " << bind(socki, (struct sockaddr *)&peers_addr, sizeof(peers_addr)) << endl;
    cout << "Listening...\n";
    cout << "Listen:" << listen(socki, 5) << endl;
    OtroFile files_recv[10];
    ofstream file(nombre, ios::binary);
    int parte;
    char parte_ch[10];
    char filesize_ch[255];
    char byte[1];
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
                bzero(filesize_ch, 255);
                cout << "read filesize: " << read(aux_s, filesize_ch, 255) << endl;
                bzero(parte_ch, 10);
                cout << "read parte:" << read(aux_s, parte_ch, 10) << endl;
                parte = atoi(parte_ch);
                files_recv[parte - 1].size = atoi(filesize_ch);
                cout << "Descargando parte: " << parte << endl;
                cout << "Tamano: " << files_recv[parte - 1].size << endl; 
                for(int i = 0; i<files_recv[parte - 1].size; i++) {
                    bzero(byte, 1);
                    read(aux_s, byte, 1);
                    file.put(byte[0]);
                }
                
            }
        }
    }
}

void see_online(int s, Msg msg, Clients *clients_)
{
    recvMsg(s, &msg);
    showClients(&msg.payload.client);
    clients_ = &msg.payload.client;
}

int main(int argc, char *argv[])
{

    // int rd, wr;
    Msg msg_clients;
    Msg msg_torr;
    TorrentFolder mytorrents;
    inic_torrents(&mytorrents);
    Clients clients;
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

    bzero(myname, 255);

    cout << "Ingrese su username: ";
    fgets(myname, 255, stdin);
    write(s, myname, strlen(myname) - 1);

    while (1)
    {
        cout << "ibtorr> ";
        bzero(buffer, 1024);
        fgets(buffer, 1024, stdin);

        if (strcmp(buffer, "addfile\n") == 0)
        {
            write(s, "addfile", 7);
            sendFileToAdd(s);
            bzero(buffer, 1024);
        }
        if (strcmp(buffer, "reqf\n") == 0)
        {
            write(s, "reqf", 4);
            string filename = requestFile(s, mytorrents, &msg_torr);
            bzero(buffer, 1024);
            read(s, buffer, 1024);
            if (strcmp(buffer, "no") == 0)
                cout << "No hay peers activos para descargar.\n";
            else
            {
                other_peers(filename,8000);
            }
            bzero(buffer, 1024);
        }
        if (strcmp(buffer, "reqt\n") == 0)
        {
            write(s, "reqt", 4);
            requestTorrent(s, &mytorrents, &msg_torr);
            bzero(buffer, 1024);
        }
        if (strcmp(buffer, "online\n") == 0)
        {
            write(s, "online", 6);
            see_online(s, msg_clients, &clients);
            bzero(buffer, 1024);
        }
        if (strcmp(buffer, "mytorr\n") == 0)
        {
            showMyTorrents(mytorrents);
            bzero(buffer, 1024);
        }
        if (strcmp(buffer, "idle\n") == 0)
        {
            recib_sign(s);
            bzero(buffer, 1024);
        }
    }


    close(s);
    return 0;
}