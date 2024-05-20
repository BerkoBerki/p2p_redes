#include <stdint.h>
#include <fstream>
#include <assert.h>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>
#include <string>
#include <openssl/sha.h>
using namespace std;

#define PIECE_LEN 262144

typedef enum
{
    TYPE_MSG,
    TYPE_TORR,
    TYPE_CLIENTS
} Type;

typedef struct __attribute__((__packed__))
{
    uint8_t type;
    uint16_t size8;
} Header;

typedef struct __attribute__((__packed__))
{
    uint8_t size;
} Img;

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
    Hash pieces[10];
} Info;

typedef struct __attribute__((__packed__))
{
    Info info;
} Torrent;

typedef struct __attribute__((__packed__))
{
    Torrent torrents[10];
} TorrentFolder;

typedef struct __attribute__((__packed__))
{
    char filename[255];
    int size;
} File;

typedef struct __attribute__((__packed__))
{
    int size;
} OtroFile;

typedef struct __attribute__((__packed__))
{
    int socket;
    int port;
    char address[255];
    char username[255];
    File files[10];
} Peer;



void inic_files(Peer *peer_)
{
    for (int i = 0; i < 10; i++)
    {
        memcpy(&peer_->files[i].filename, "null", strlen("null"));
    }
}



void addFile(Peer *peer_, const char *filename)
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
        File file;
    } payload;

} Msg;

inline static void setSocket(Peer *peer_, int socket)
{
    peer_->socket = socket;
}

inline static void setPeer(Peer *peer_, int port, const char *usern, const char *address)
{
    peer_->port =port;
    memcpy(peer_->username, usern, strlen(usern));
    memcpy(peer_->address, address, strlen(address));
}

/* inline static void setMsgPeer(Msg *msg, Peer *peer_)
{
    msg->hdr.type = TYPE_USER;
    setSocket(&msg->payload.user, peer_->socket);
    setPeer(&msg->payload.user, peer_->port, peer_->username, peer_->address);
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(Peer));
} */
void setInfo(Info *info, const char *name, int piece_length_, bool check)
{
    info->piece_length = piece_length_;
    if(check) {
        ifstream file(name, ios::binary);
        assert(file.is_open());
        char byte;
        info->length = 0;
        while (file.get(byte))
            info->length++;
        file.close();
        file.open(name, ios::binary);
        assert(file.is_open());
        bzero(info->name, 255);
        memcpy(info->name, name, strlen(name));
        int num_hashes;
        num_hashes = info->length / info->piece_length;
        info->num_pieces = num_hashes;
        for (int i = 0; i < num_hashes; i++)
        {
            char bytes[info->piece_length];
            for (int j = 0; j < info->piece_length; j++)
            {
                file.get(bytes[j]);
            }
            SHA1((unsigned char *)bytes, sizeof(bytes) - 1, info->pieces[i].hash);
        }
        file.close();
    }
    else
        memcpy(info->name, name, strlen(name));
}
void createTorrent(Torrent *torrent, Info info)
{
    torrent->info = info;
}

void inic_torrents(TorrentFolder *torrent_f)
{
    for (int i = 0; i < 10; i++)
    {
        Info info;
        setInfo(&info, "null", PIECE_LEN, 0);
        createTorrent(&torrent_f->torrents[i], info);
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
        memcpy(&msg->payload.client.peers[i].files, clients_->peers[i].files, 10 * sizeof(File));
        msg->hdr.size8 = msg->hdr.size8 + htons(sizeof(Header) + sizeof(Peer));
    }
}

inline static void setMsgTorr(Msg *msg, Torrent torrent_)
{
    msg->hdr.type = TYPE_TORR;
    createTorrent(&msg->payload.torrent, torrent_.info);
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

int getType(Msg *msg)
{
    int x = msg->hdr.type;
    if (x == 0)
    {
        printf("Mensaje.\n");
        return x;
    }
    if (x == 1)
    {
        printf("Iamgen.\n");
        return x;
    }
    if (x == 2)
    {
        printf("Usuario.\n");
        return x;
    }
    if (x == 3)
    {
        printf("Clients.\n");
        return x;
    }
    else
    {
        printf("Error.\n");
        return x;
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
    cout << "Nombre del archivo: " << torrent.info.name << endl;
    cout << "Tamano del archivo: " << torrent.info.length << " bytes\n";
}

void showMyTorrents(TorrentFolder torrentf)
{
    for (int i = 0; i < 10; i++)
    {
        if (strcmp(torrentf.torrents[i].info.name, "null") != 0)
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
