#include <stdint.h>
#include <fstream>
#include <assert.h>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string>
#include <fcntl.h>
#include <openssl/sha.h>
#include <mutex>
using namespace std;

mutex mtx_proto;

#define PIECE_LEN 26214

char bytes_torr[PIECE_LEN];

// tipos de mensaje -> pueden ser Torrents, Clientes (lista de Peers), o informacion sobre piezas de archivos.

typedef enum
{
    TYPE_TORR,
    TYPE_CLIENTS,
} Type;

// header de un mensaje

typedef struct __attribute__((__packed__))
{
    uint16_t type;
    uint16_t size8;
} Header;

// protocolo de escritura y lectura, donde se envia primero la cantidad de bytes a recibir.

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

typedef struct __attribute__((__packed__))
{
    unsigned char hash[40];
} Hash;

typedef struct __attribute__((__packed__))
{
    int length;       // tamano del archivo
    char name[255];   // path y nombre
    int piece_length; // tamano de las piezas (bytes)
    int num_pieces;
    bool used;
    Hash pieces[200];
} Torrent;

typedef struct __attribute__((__packed__))
{
    int size;
    int vector[200];
} HashCheck;

typedef struct __attribute__((__packed__))
{
    Torrent torrents[10];
} TorrentFolder;

typedef struct __attribute__((__packed__))
{
    int size;
    int idx[200];
} IdxList;

typedef struct __attribute__((__packed__))
{
    int port;
    char address[50];
    IdxList idxlist;
} IdxInfo;

typedef struct __attribute__((__packed__))
{
    int cant;
    IdxInfo idxinfo[200];
} IdxInfoInfo;

typedef struct __attribute__((__packed__))
{
    char filename[50];
    int torr_idx;
    int filesize;
    IdxInfo idxinfo;
} ArgThreads;

typedef struct __attribute__((__packed__))
{
    int size;
} Sizes;

typedef struct __attribute__((__packed__))
{
    int socket;
    int port;
    char address[255];
    char username[255];
    TorrentFolder torrents;
} Peer;

typedef struct __attribute__((__packed__))
{
    Peer peers[10];
} Clients;

typedef struct __attribute__((__packed__))
{
    Header hdr;

    union __attribute__((__packed__))
    {
        Torrent torrent;
        IdxInfoInfo info;
    } payload;
} Msg;

void showInfo(IdxInfoInfo infox)
{
    for (int i = 0; i < infox.cant; i++)
    {
        cout << infox.idxinfo[i].address << endl;
        cout << infox.idxinfo[i].port << endl;
        cout << infox.idxinfo[i].idxlist.size << endl;
    }
}

inline static void setSocket(Peer *peer_, int socket)
{
    peer_->socket = socket;
}

inline static void setPeer(Peer *peer_, int port, const char *usern, const char *address)
{
    peer_->port = port;
    memcpy(peer_->username, usern, strlen(usern));
    memcpy(peer_->address, address, strlen(address));
}

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
                        for (int smalll = bigl; smalll < info->idxinfo[index_b].idxlist.size - 1; ++smalll)
                        {
                            info->idxinfo[index_b].idxlist.idx[smalll] = info->idxinfo[index_b].idxlist.idx[smalll + 1];
                        }
                        --info->idxinfo[index_b].idxlist.size;
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void ordenInfo(IdxInfoInfo *info, int cant)
{
    for (int i = 0; i < cant - 1; ++i) {
        for (int j = 0; j < cant - i - 1; ++j) {
            if (info->idxinfo[j].idxlist.size > info->idxinfo[j+1].idxlist.size) {
                IdxInfo aux = info->idxinfo[j];
                info->idxinfo[j] = info->idxinfo[j+1];
                info->idxinfo[j+1]= aux;
            }
        }
    }
}

int lookTorr(const char *name, Clients *clients, int idx)
{
    for (int i = 0; i < 10; i++)
    {
        if (i != idx)
        {
            for (int j = 0; j < 10; j++)
            {
                if (strcmp(name, clients->peers[i].torrents.torrents[j].name) == 0)
                {
                    return i + 10 * j;
                }
            }
        }
    }
    return -1;
}

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

void createTorrent(Torrent *torrent, const char *name, int piece_length_, bool check)
{
    torrent->piece_length = piece_length_;
    if (check)
    {
        torrent->used = 1;
        int op = open(name, NULL);
        assert(op > 0);
        char byte[1];
        torrent->length = 0;
        while (read(op, byte, 1))
        {
            torrent->length++;
        }
        lseek(op, 0, SEEK_SET);
        bzero(torrent->name, 255);
        memcpy(torrent->name, name, strlen(name));

        int num_pieces;
        num_pieces = torrent->length / torrent->piece_length;
        if (torrent->length % torrent->piece_length != 0)
        {
            torrent->num_pieces = num_pieces + 1;
        }
        else
        {
            torrent->num_pieces = num_pieces;
        }
        for (int i = 0; i < num_pieces; i++)
        {
            bzero(torrent->pieces[i].hash, 200);
            bzero(bytes_torr, PIECE_LEN);
            read(op, bytes_torr, PIECE_LEN);
            SHA1((unsigned char *)bytes_torr, sizeof(bytes_torr), torrent->pieces[i].hash);
        }
        close(op);
    }
    else
    {
        memcpy(torrent->name, name, strlen(name));
        torrent->used = 1;
    }
}

void addTorrent(TorrentFolder *mytorrs, Torrent torrent)
{
    for (int i = 0; i < 10; i++)
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

Torrent createTorrent2(const char *name, int piece_length_, bool check, int ii, int is)
{
    Torrent torrent;

    torrent.piece_length = piece_length_;
    torrent.used = 1;
    int op = open(name, NULL);
    assert(op > 0);
    char byte[1];
    torrent.length = 0;
    while (read(op, byte, 1))
    {
        torrent.length++;
    }
    lseek(op, ii * piece_length_, SEEK_SET);
    bzero(torrent.name, 255);
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
        bzero(torrent.pieces[i].hash, 40);

    for (int i = ii; i < is; i++)
    {
        bzero(bytes_torr, PIECE_LEN);
        read(op, bytes_torr, PIECE_LEN);
        SHA1((unsigned char *)bytes_torr, sizeof(bytes_torr), torrent.pieces[i].hash);

        cout << strlen((const char *)torrent.pieces[i].hash) << endl;
    }
    close(op);
    return torrent;
}

void cleanHash(Torrent *torrent)
{
    for (int i = 0; i < torrent->num_pieces; i++)
        bzero(torrent->pieces[i].hash, 40);
}

void inic_torrents(TorrentFolder *torrent_f)
{
    for (int i = 0; i < 10; i++)
    {
        torrent_f->torrents[i].used = 0;
    }
}

int checkTorr(TorrentFolder torrens, const char *name)
{
    for (int i = 0; i < 10; i++)
    {
        if (strcmp(torrens.torrents[i].name, name))
            return i;
    }
    return -1;
}

inline static void setMsgTorr(Msg *msg, Torrent torrent_)
{
    msg->hdr.type = TYPE_TORR;
    msg->payload.torrent = torrent_;
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(Torrent));
}

inline static void setMsgInfo(Msg *msg, IdxInfoInfo info)
{
    msg->hdr.type = TYPE_TORR;
    msg->payload.info = info;
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(IdxInfoInfo));
}

int getPeerSocket(Peer *peer_)
{
    return peer_->socket;
}

inline static const char *getUserName(Peer *peer_)
{
    return peer_->username;
}

inline static const char *getAddress(Peer *peer_)
{
    return peer_->address;
}

int getPort(Peer *peer_)
{
    return peer_->port;
}

inline static void showClients(Clients *clients)
{
    std::cout << "\n✧✧✧ PEERS CONECTADOS: ✧✧✧" << std::endl;
    for (int i = 0; i < 10; i++)
    {
        if (clients->peers[i].socket != 0)
        {
            std::cout << "Usuario: " << getUserName(&clients->peers[i]) << '\n';
            std::cout << "Puerto: " << getPort(&clients->peers[i]) << '\n';
            std::cout << "Direccion: " << getAddress(&clients->peers[i]) << '\n';
        }
    }
}

void showTorrInfo(Torrent torrent)
{
    cout << "Nombre del archivo: " << torrent.name << endl;
    cout << "Tamano del archivo: " << torrent.length << " bytes\n";
    cout << "Numero de hashes: " << torrent.num_pieces << endl;
}

void showMyTorrents(TorrentFolder torrentf)
{
    for (int i = 0; i < 10; i++)
    {
        if (torrentf.torrents[i].used == 1)
        {
            showTorrInfo(torrentf.torrents[i]);
        }
    }
}

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
