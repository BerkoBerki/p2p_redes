#include <fstream>
#include <assert.h>
#include <pthread.h>
#include <string>
#include <fcntl.h>
#include <openssl/sha.h>
#include <mutex>
#include <thread>
#include <iostream>
#include <condition_variable>
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
#include "progressbar.hpp"
using namespace std;

#define PIECE_LEN 26214
#define HASHSIZE 40
#define MAXCLIENTS 10
#define MAXTORRENTS 10
#define BUFF_SIZE 255

char bytes_torr[PIECE_LEN];

// Tipos de mensaje -> pueden ser Torrents, o informacion sobre ubicacion de piezas de archivos.

typedef enum
{
    TYPE_TORR,
    TYPE_INFO,
} Type;

// Header de un mensaje.

typedef struct __attribute__((__packed__))
{
    uint16_t type;
    uint16_t size8;
} Header;

// Protocolo de escritura y lectura, donde se envia primero la cantidad de bytes a recibir.

int write_prot(int soc, const char *buffer, size_t size)
{

    write(soc, &size, sizeof(size));

    int wr = write(soc, buffer, size);

    return wr;
}

int read_prot(int socket, char *buffer)
{
    size_t size;
    if (read(socket, &size, sizeof(size)) == 0)
        return 0;

    int rd = read(socket, buffer, size);

    return rd;
}

// Estructura de Hash para los archivos torrent, se calcula con SHA1 para cada pieza del archivo original.

typedef struct __attribute__((__packed__))
{
    unsigned char hash[HASHSIZE];
} Hash;

// Estructura del torrent.

typedef struct __attribute__((__packed__))
{
    int length;           // tamano del archivo
    char name[BUFF_SIZE]; // nombre
    int piece_length;     // tamano de las piezas (bytes)
    int num_pieces;       // numero de piezas que componen un archivo
    bool used;            // es util cuando armo el "torrent folder"
    Hash pieces[BUFF_SIZE*2];
} Torrent;

// HashCheck es un vector de 1s y 0s resultado de hacer un OR entre los hash de los torrents de distintos peers, para ver que piezas se pueden compartir.

typedef struct __attribute__((__packed__))
{
    int size;
    int vector[BUFF_SIZE];
} HashCheck;

// Torrents de los peers.

typedef struct __attribute__((__packed__))
{
    Torrent torrents[MAXTORRENTS];
} TorrentFolder;

// Las siguientes tres estructuras son para que el tracker decida como descargar las piezas cuando un peer solicita un archivo. Se ve mas adelante.

typedef struct __attribute__((__packed__))
{
    int size; // cantidad de piezas que tiene un peer
    int idx[BUFF_SIZE*2]; // indices de las piezas que tiene ese peer
} IdxList;

typedef struct __attribute__((__packed__))
{
    int port; // puerto al que se le va a pedir las piezas en idxlist
    char address[BUFF_SIZE]; // direccion a la que se le va a pedir las piezas en idxlist
    IdxList idxlist;
} IdxInfo;

typedef struct __attribute__((__packed__))
{
    int cant; // cantidad de usuarios que tienen piezas 
    IdxInfo idxinfo[BUFF_SIZE];
} IdxInfoInfo;

// ArgThreads es una struct que hice para pasarle argumentos a los pthreads para que el peer solicite piezas a otros peers.

typedef struct __attribute__((__packed__))
{
    char filename[BUFF_SIZE];
    int torr_idx;
    int filesize;
    IdxInfo idxinfo;
    int s;
} ArgThreads;

// struct de peer

typedef struct __attribute__((__packed__))
{
    int socket; // realmente no sirvio tener el socket aca, mas que para inicializarlos con 0 para saber si puedo usarlo o no
    int port;
    char address[BUFF_SIZE];
    char username[BUFF_SIZE];
    TorrentFolder torrents; // cada peer tiene su "carpeta" de torrents
} Peer;

// Estructura de los clientes que administra el tracker.

typedef struct __attribute__((__packed__))
{
    Peer peers[MAXCLIENTS];
} Clients;

// Estructura del mensaje.

typedef struct __attribute__((__packed__))
{
    Header hdr;

    union __attribute__((__packed__))
    {
        Torrent torrent;
        IdxInfoInfo info;
    } payload;
} Msg;

// Muestra informacion de quien tiene cuantas piezas.

void showInfo(IdxInfoInfo info)
{
    for (int i = 0; i < info.cant; i++)
    {
        cout << info.idxinfo[i].idxlist.size << " piezas en " << info.idxinfo[i].address << ":" << info.idxinfo[i].port << endl;
    }
}

// Setea el socket del peer.

inline static void setSocket(Peer *peer_, int socket)
{
    peer_->socket = socket;
}

// Setea el nombre puerto y direccion del socket.

inline static void setPeer(Peer *peer_, int port, const char *usern, const char *address)
{
    peer_->port = port;
    memcpy(peer_->username, usern, strlen(usern));
    memcpy(peer_->address, address, strlen(address));
}

// Cuando un peer solicita un archivo, el tracker se fija quienes tiene piezas, y esta funcion 
// hace que no descargue piezas repetidas, y las reparte de forma equitativa, para que la descarga sea lo mas rapido posible.

bool checkDuplicates(IdxInfoInfo *info, int cant)
{
    int index_b = 0;
    for (int c = 1; c < cant; c++)
    {
        if (info->idxinfo[c].idxlist.size > info->idxinfo[index_b].idxlist.size)
        {
            index_b = c;
        }
    }
    for (int c = 0; c < cant; c++)
    {
        if (c != index_b)
        {
            for (int bigl = 0; bigl < info->idxinfo[index_b].idxlist.size; bigl++)
            {
                for (int loop = 0; loop < info->idxinfo[c].idxlist.size; loop++)
                {
                    if (info->idxinfo[c].idxlist.idx[loop] == info->idxinfo[index_b].idxlist.idx[bigl])
                    {
                        for (int smalll = bigl; smalll < info->idxinfo[index_b].idxlist.size - 1; smalll++)
                        {
                            info->idxinfo[index_b].idxlist.idx[smalll] = info->idxinfo[index_b].idxlist.idx[smalll + 1];
                        }
                        info->idxinfo[index_b].idxlist.size--;
                        IdxInfo aux = info->idxinfo[cant - 1];
                        info->idxinfo[cant - 1] = info->idxinfo[index_b];
                        info->idxinfo[index_b] = aux; 
                        // Esto de mezclar los indices lo hice porque a veces, si por ejemplo, tres peers tenian 100 piezas de un archivo,
                        // lo repartia en 50/25/25 en lugar de 33/33/34, y asi se soluciono.
                        return 1;
                    }
                }
            }
        }
    }
    // Si llegue hasta aca es porque las piezas en el indice "index_b" ya son unicas, lo mando al fondo y sigo con el resto.
    IdxInfo aux = info->idxinfo[cant - 1];
    info->idxinfo[cant - 1] = info->idxinfo[index_b];
    info->idxinfo[index_b] = aux;
    return 0;
}

// Busca entre los clientes quien tiene el torrent con nombre "name".

int lookTorr(const char *name, Clients *clients, int idx)
{
    for (int i = 0; i < MAXTORRENTS; i++)
    {
        if (i != idx)
        {
            for (int j = 0; j < MAXCLIENTS; j++)
            {
                if (strcmp(name, clients->peers[i].torrents.torrents[j].name) == 0)
                {
                    return i + 10 * j; // De esta forma compacto la informacion del indice del cliente y ademas del torrent entre su carpeta de torrents.
                }
            }
        }
    }
    return -1;
}

// OR entre hashes de torrents. Si en el vector de hascheck hay un 1 en un cierto indice, el indice correspondiente a esa pieza puede ser compartido entre los peers.

HashCheck torrCompare(Torrent torrent1, Torrent torrent2)
{
    if (strcmp(torrent1.name, torrent2.name) != 0)
    {
        cout << "No son el mismo torrent!\n";
    }
    HashCheck vector;
    vector.size = torrent1.num_pieces;
    unsigned char result;

    for (int i = 0; i < torrent1.num_pieces; i++)
    {
        for (int j = 0; j < 20; j++)
        {
            if (strlen((char *)torrent1.pieces[i].hash) == 0)
            {
                vector.vector[i] = 0;
                break;
            }
            result = torrent1.pieces[i].hash[j] | torrent2.pieces[i].hash[j];
            if (result != torrent1.pieces[i].hash[j] || result != torrent2.pieces[i].hash[j])
            {
                vector.vector[i] = 1;
                break;
            }
            vector.vector[i] = 0;
        }
    }
    return vector;
}

// Agrega un torrent a una carpeta de torrents.

void addTorrent(TorrentFolder *mytorrs, Torrent torrent)
{
    for (int i = 0; i < MAXTORRENTS; i++)
    {
        if (mytorrs->torrents[i].used == 0)
        {
            mytorrs->torrents[i].used = 1;
            memcpy(mytorrs->torrents[i].name, torrent.name, strlen(torrent.name));
            mytorrs->torrents[i].length = torrent.length;
            mytorrs->torrents[i].piece_length = torrent.piece_length;
            mytorrs->torrents[i].num_pieces = torrent.num_pieces;
            for (int k = 0; k < torrent.num_pieces; k++)
                memcpy(mytorrs->torrents[i].pieces[k].hash, torrent.pieces[k].hash, strlen((const char *)torrent.pieces[k].hash));
            break;
        }
    }
}

// Crea un torrent a partir de un archivo. 

Torrent createTorrent(const char *name, int piece_length_, bool check, int ii, int is)
{
    Torrent torrent;
    torrent.piece_length = piece_length_;
    torrent.used = 1;
    int op = open(name, O_RDWR, 0700);
    assert(op > 0);
    char byte[1];
    torrent.length = 0;
    while (read(op, byte, 1))
    {
        torrent.length++;
    }
    lseek(op, ii * piece_length_, SEEK_SET);
    bzero(torrent.name, BUFF_SIZE);
    memcpy(torrent.name, name, strlen(name));

    int num_pieces;
    num_pieces = torrent.length / torrent.piece_length;
    if (torrent.length % torrent.piece_length != 0)
    {
        torrent.num_pieces = num_pieces + 1;
    }
    else
    {
        torrent.num_pieces = num_pieces;
    }
    for (int i = 0; i < num_pieces; i++)
        bzero(torrent.pieces[i].hash, HASHSIZE);

    for (int i = ii; i < is; i++)
    {
        bzero(bytes_torr, PIECE_LEN);
        read(op, bytes_torr, PIECE_LEN);
        SHA1((unsigned char *)bytes_torr, sizeof(bytes_torr), torrent.pieces[i].hash);
        if(strlen((const char*)torrent.pieces[i].hash) == 0)
            memcpy(torrent.pieces[i].hash, "hellofren", 8); // Esto es porque, muy rara vez el hash resulta de cero bytes
                                                            // y cuando el OR sale como si no tuviera la pieza.
    }
    close(op);
    return torrent;
}

// Limpia los hash. Esto sirve cuando un peer descarga un torrent, ya que no tiene ninguna pieza del archivo.

void cleanHash(Torrent *torrent)
{
    for (int i = 0; i < torrent->num_pieces; i++)
        bzero(torrent->pieces[i].hash, HASHSIZE);
}

// Pone en 0 "used" porque puedo usar todos.

void inic_torrents(TorrentFolder *torrent_f)
{
    for (int i = 0; i < MAXTORRENTS; i++)
    {
        torrent_f->torrents[i].used = 0;
    }
}

// Bastante self explanatory...

void showTorrInfo(Torrent torrent)
{
    cout << "Nombre del archivo: " << torrent.name << endl;
    cout << "Tamano del archivo: " << torrent.length << " bytes\n";
    cout << "Numero de hashes: " << torrent.num_pieces << endl;
}

void showMyTorrents(TorrentFolder torrentf)
{
    for (int i = 0; i < MAXTORRENTS; i++)
    {
        if (torrentf.torrents[i].used == 1)
        {
            showTorrInfo(torrentf.torrents[i]);
        }
    }
}

// Devuelve el indice en que una carpeta de torrents tiene el torrent de nombre "name".

int checkTorr(TorrentFolder torrens, const char *name)
{
    for (int i = 0; i < MAXTORRENTS; i++)
    {
        if (strcmp(torrens.torrents[i].name, name))
            return i;
    }
    return -1;
}

// Los dos tipos de mensaje.

inline static void setMsgTorr(Msg *msg, Torrent torrent_)
{
    msg->hdr.type = TYPE_TORR;
    msg->payload.torrent = torrent_;
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(Torrent));
}

inline static void setMsgInfo(Msg *msg, IdxInfoInfo info)
{
    msg->hdr.type = TYPE_INFO;
    msg->payload.info = info;
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(IdxInfoInfo));
}

// Enviar y recibir mje.

int sendMsg(int sockfd, const Msg *msg)
{
    size_t toSend = ntohs(msg->hdr.size8);
    ssize_t sent;
    uint8_t *ptr = (uint8_t *)msg;

    while (toSend)
    {
        sent = send(sockfd, ptr, toSend, 0);
        if ((sent == -1 && errno != EINTR) || sent == 0)
            return sent;
        toSend -= sent;
        ptr += sent;
    }
    return 1;
}

int recvMsg(int sockfd, Msg *msg)
{
    size_t toRecv = sizeof(Header);
    ssize_t recvd;
    uint8_t *ptr = (uint8_t *)&msg->hdr;
    int headerRecvd = 0;

    while (toRecv)
    {
        recvd = recv(sockfd, ptr, toRecv, 0);
        if ((recvd == -1 && errno != EINTR) || recvd == 0)
            return recvd;
        toRecv -= recvd;
        ptr += recvd;
        if (toRecv == 0 && headerRecvd == 0)
        {
            headerRecvd = 1;
            ptr = (uint8_t *)&msg->payload;
            toRecv = ntohs(msg->hdr.size8) - sizeof(Header);
        }
    }
    return 1;
}
