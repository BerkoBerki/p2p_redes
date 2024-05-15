#include <stdint.h>
#include <assert.h>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>

typedef enum
{
    TYPE_MSG,
    TYPE_IMG,
    TYPE_USER,
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
    uint8_t len8;
    char text[255];
} VString;

typedef struct __attribute__((__packed__))
{
    int socket;
    int port;
    char address[255];
    char username[255];
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
        Clients client;
        Peer user;
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

inline static void setMsgPeer(Msg *msg, Peer *peer_)
{
    msg->hdr.type = TYPE_USER;
    setSocket(&msg->payload.user, peer_->socket);
    setPeer(&msg->payload.user, peer_->port, peer_->username, peer_->address);
    msg->hdr.size8 = htons(sizeof(Header) + sizeof(Peer));
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

inline static const char *getMsgUser(Msg *msg)
{
    assert(msg->hdr.type == TYPE_USER);
    return msg->payload.user.username;
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
    for (int i = 0; i < 10; i++)
    {
        if (clients->peers[i].socket != 0)
        { 
            std::cout << "Usuario: " << getUserName(&clients->peers[i]) << '\n';
            std::cout << "Puerto: " << getPort(&clients->peers[i]) << '\n';
            std::cout << "Direccion: " << getAddress(&clients->peers[i]) << '\n';
        }
    }
    std::cout << "clientes ok\n";
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
