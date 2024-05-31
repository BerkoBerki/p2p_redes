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
TorrentFolder mytorrents;

condition_variable cv_2;
bool stopflag = false;

mutex mtx_2;

void update_torr(int s, Torrent torrent)
{
    write_prot(s, "update", 7);
    Msg torr_;
    setMsgTorr(&torr_, torrent);
    sendMsg(s, &torr_);
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
    fd_set readfds;
    char idx[5];
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
        bzero(buffer, 255);
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
                    bzero(idx, 5);
                    read_prot(aux_s, idx);
                    cout << "idx: " << idx << endl;
                    cout << "BUFFER: " << buffer << endl;
                    int fd = open(buffer, O_RDWR, 0700);
                    assert(fd > 0);
                    char bytess[PIECE_LEN];
                    bzero(bytess, PIECE_LEN);
                    lseek(fd, atoi(idx) * PIECE_LEN, SEEK_SET);
                    read(fd, bytess, PIECE_LEN);
                    write_prot(aux_s, bytess, PIECE_LEN);
                    close(fd);
                }
            }
            bzero(buffer, 255);
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

void *acept_archivo(void *args)
{
    ArgThreads *thread_args = (ArgThreads *)args;
    IdxInfo indice = thread_args->idxinfo;
    struct sockaddr_in peers_addr;
    int opt = 1;
    int socki;
    socki = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv;
    struct hostent *serv_;
    serv_ = gethostbyname(indice.address);
    int portno = indice.port;
    bzero((char *)&serv, sizeof(serv));
    serv.sin_family = AF_INET;
    bcopy((char *)serv_->h_addr, (char *)&serv.sin_addr.s_addr, serv_->h_length);
    serv.sin_port = htons(portno);
    if (connect(socki, (struct sockaddr *)&serv, sizeof(serv)) < 0)
        fprintf(stderr, "Error de conexion...\n");
    int fd = open(thread_args->filename, O_CREAT | O_RDWR, 0700);
    assert(fd > 0);
    char bytess[PIECE_LEN];
    bzero(bytess, PIECE_LEN);
    int filesize = thread_args->filesize;
    for (int i = 0; i < indice.idxlist.size; i++)
    {
        lseek(fd, indice.idxlist.idx[i] * PIECE_LEN, SEEK_SET);
        write_prot(socki, thread_args->filename, strlen(thread_args->filename));
        write_prot(socki, to_string(indice.idxlist.idx[i]).c_str(), strlen(to_string(indice.idxlist.idx[i]).c_str()));

        if (indice.idxlist.idx[i] == filesize / PIECE_LEN)
        {
            read_prot(socki, bytess);
            write(fd, bytess, filesize % PIECE_LEN);
            SHA1((unsigned char *)bytess, sizeof(bytess), mytorrents.torrents[thread_args->torr_idx].pieces[indice.idxlist.idx[i]].hash);
        }
        else
        {
            read_prot(socki, bytess);
            write(fd, bytess, PIECE_LEN);
            SHA1((unsigned char *)bytess, sizeof(bytess), mytorrents.torrents[thread_args->torr_idx].pieces[indice.idxlist.idx[i]].hash);
        }
    }
    close(socki);
    close(fd);
    return NULL;
}

void requestfile(int s, char *buffer)
{
    cout << "Estos son sus torrents: \n";
    showMyTorrents(mytorrents);
    cout << "Que archivo desea descargar?: ";
    bzero(buffer, 255);
    fgets(buffer, 255, stdin);
    int idx = checkTorr(mytorrents, buffer);
    if (idx != -1)
    {
        write_prot(s, "yes", 4);
        Msg torr;
        bzero(&torr, sizeof(Msg));
        setMsgTorr(&torr, mytorrents.torrents[idx]);
        sendMsg(s, &torr);
        Msg msg_infoo;
        recvMsg(s, &msg_infoo);
        showInfo(msg_infoo.payload.info);
        int ct = msg_infoo.payload.info.cant;
        if (ct == 0)
        {
            cout << "Nadie tiene el archivo!\n";
            return;
        }
        auto start = std::chrono::high_resolution_clock::now();
        pthread_t hilos[ct];
        ArgThreads args_hilos[ct];
        int idx0;
        int result;

        for (idx0 = 0; idx0 < ct; idx0++)
        {
            args_hilos[idx0].torr_idx = idx;
            args_hilos[idx0].idxinfo = msg_infoo.payload.info.idxinfo[idx0];
            bzero(args_hilos[idx0].filename, 50);
            memcpy(args_hilos[idx0].filename, buffer, strlen(buffer) - 1);
            cout << "bruh: " << args_hilos[idx0].filename << endl;
            args_hilos[idx0].filesize = torr.payload.torrent.length;
            result = pthread_create(&hilos[idx0], NULL, acept_archivo, &args_hilos[idx0]);
            assert(!result);
        }
        for (idx0 = 0; idx0 < ct; idx0++)
        {
            result = pthread_join(hilos[idx0], NULL);
            assert(!result);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Tiempo de descarga: " << duration.count() << " ms." << std::endl;

        update_torr(s, mytorrents.torrents[idx]);
    }
    else
        write_prot(s, "no", 3);
    bzero(buffer, 255);
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

int main(int argc, char *argv[])
{
    Msg msg_clients;
    Msg msg_torr;
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

        if (strcmp(buffer, "addfile\n") == 0)
        {
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
            Msg msg_torr_buff;
            bzero(&msg_torr_buff, sizeof(Msg));
            setMsgTorr(&msg_torr_buff, torr_buff);
            sendMsg(s, &msg_torr_buff);
            cout << name_2 << " anadido.\n";
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
            write_prot(s, "reqf", 4);
            requestfile(s, buffer);
        }
    }

    oth.join();
    close(s);
    return 0;
}