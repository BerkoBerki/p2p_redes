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

#define PIECE_LEN 262144
char bytes_torr[PIECE_LEN];

typedef enum
{
    TYPE_MSG,
    TYPE_TORR,
    TYPE_CLIENTS,
    TYPE_FILEPIECE
} Type;

void write_prot(int soc, const char* buffer, size_t size) {
    lock_guard<mutex> lock(mtx_proto);
    write(soc, &size, sizeof(size));
    write(soc, buffer, size);
}

void read_prot(int socket, char* buffer) {
    std::lock_guard<std::mutex> lock(mtx_proto);
    size_t size;
    read(socket, &size, sizeof(size)); 
    read(socket, buffer, size); 
    //buffer[size] = '\0'; 
}

typedef struct __attribute__((__packed__))
{
    uint16_t type;
    uint16_t size8;
} Header;

typedef struct __attribute__((__packed__))
{
    unsigned char hash[20];
} Hash;

typedef struct __attribute__((__packed__))
{
    int length;       // tamano del archivo
    char name[255];   // path y nombre
    int piece_length; // tamano de las piezas (bytes)
    int num_pieces;
    bool used;
    Hash pieces[10];
} Torrent;


typedef struct __attribute__((__packed__))
{
    Torrent torrents[10];
} TorrentFolder;

typedef struct __attribute__((__packed__))
{
    char filename[255];
    int size;
    int idx;
    bool used;
    bool sent;
} FilePiece;

typedef struct __attribute__((__packed__))
{
    char filename[255];
    int parts;
    FilePiece pieces[1024];
} File;

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
    FilePiece files[1024];
} Peer;

void inic_files(Peer *peer_)
{
    for (int i = 0; i < 1024; i++)
    {
        memcpy(&peer_->files[i].filename, "null", strlen("null"));
    }
}

void addFile(Peer *peer_, const char *filename, int pieces)
{
    for (int i = 0; i < 10; i++)
    {
        if (strcmp(peer_->files[i].filename, "null") == 0)
        {
            bzero(peer_->files[i].filename, 255);
            memcpy(&peer_->files[i].filename, filename, strlen(filename));
            break;
        }
    }
}

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
        Clients client;
        FilePiece file;
    } payload;
} Msg;

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

void createTorrent(Torrent *torrent, const char *name, int piece_length_, bool check)
{
    torrent->piece_length = piece_length_;
    if (check)
    {
        torrent->used = 1;
        int op = open(name, NULL);
        assert(op>0);
        char byte[1];
        torrent->length = 0;
        while (read(op, byte, 1)) {
            torrent->length++;
        }
        close(op);
        op = open(name, NULL);
        assert(op>0);
        bzero(torrent->name, 255);
        memcpy(torrent->name, name, strlen(name));
        
        int num_pieces;
        num_pieces = torrent->length / torrent->piece_length;
        if(torrent->length%torrent->piece_length != 0)
            torrent->num_pieces = num_pieces + 1;
        else
            torrent->num_pieces = num_pieces;

        for (int i = 0; i < num_pieces; i++)
        {

            bzero(bytes_torr, torrent->piece_length);
            read(op, bytes_torr, torrent->piece_length);
            
            SHA1((unsigned char *)bytes_torr, sizeof(bytes_torr) - 1, torrent->pieces[i].hash);
        } 
        close(op);
    }
    else {
        memcpy(torrent->name, name, strlen(name));
        torrent->used = 1;
    }
}

void inic_torrents(TorrentFolder *torrent_f)
{
    for (int i = 0; i < 10; i++)
    {
        torrent_f->torrents[i].used = 0;
    }
}

inline static void setMsgClients(Msg *msg, Clients *clients_)
{
    msg->hdr.size8 = 0;
    msg->hdr.type = TYPE_CLIENTS;
    for (int i = 0; i < 10; i++)
    {
        setSocket(&msg->payload.client.peers[i], clients_->peers[i].socket);
        setPeer(&msg->payload.client.peers[i], clients_->peers[i].port, clients_->peers[i].username, clients_->peers[i].address);
        msg->hdr.size8 = msg->hdr.size8 + htons(sizeof(Header) + sizeof(Peer));
    }
}

inline static void setMsgTorr(Msg *msg, Torrent torrent_)
{
    msg->hdr.type = TYPE_TORR;
    createTorrent(&msg->payload.torrent, torrent_.name, PIECE_LEN, 1);
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(Torrent));
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

inline static void setMsgPiece(Msg *msg, FilePiece *filepieces) {
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(FilePiece));
    msg->hdr.type = TYPE_FILEPIECE;
    msg->payload.file = *filepieces;
}

inline static void createPiece(FilePiece *filepiece, const char *filename, int size, int idx) 
{
    for(int i = 0; i<1024; i++) 
    {
        if(filepiece[i].used == 0) {
            memcpy(filepiece[i].filename, filename, strlen(filename));
            filepiece[i].size = size;
            filepiece[i].idx = idx;
            filepiece[i].used = 1;
            filepiece[i].sent = 0;
            break;
        }
    }
}

inline static void inic_pieces(FilePiece *filepiece) 
{
    for(int i = 0; i<1024; i++) {
        filepiece[i].used = 0;
        filepiece[i].sent = 1;
    }
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
void hashcheck(TorrentFolder mytorrs, string filename)
{
    int counter = 0;
    for (int i = 0; i < 10; i++)
    {
        if (strcmp(filename.c_str(), mytorrs.torrents[i].name) == 0)
        {
            int fd = open(filename.c_str(), NULL);
            for (int j = 0; j < mytorrs.torrents[i].num_pieces; j++)
            {
                unsigned char hash[20];
                char bytes[mytorrs.torrents[i].piece_length];
                write(fd, bytes, mytorrs.torrents[i].piece_length);

                SHA1((unsigned char *)bytes, sizeof(bytes) - 1, hash);
                if (strcmp((const char *)hash, (const char *)mytorrs.torrents[i].pieces[j].hash) == 0)
                    counter++;
            }
            if(counter == mytorrs.torrents[i].num_pieces) {
                cout << "Hash check ok.\n";
                return;
            }
            break; 
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
