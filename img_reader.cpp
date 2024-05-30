#include <iostream>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <openssl/sha.h>
#include <unistd.h>
#include "proto_p2p.h"

typedef struct __attribute__((__packed__))
{
    unsigned char hash[40];
} hashito;

typedef struct __attribute__((__packed__))
{
    hashito hashesitos[200];
} hashesote;

int main()
{
    /*     unsigned char hash[20];
        std::ifstream file("./Tux.bmp", std::ios::binary);
        if (!file.is_open())
        {
            std::cerr << "Error opening file\n";
            return 1;
        } */
    // char byte;
    // char allbytes[262144];
    /*   for (int j = 0; j < 262144; j++)
        {
            file.get(allbytes[j]);
            //std::cout << j << ": " << allbytes[j];
             for (int i = 7; i >= 0; --i)
            {
                bool bit = (byte >> i) & 1;
                std::cout << bit;
            }
            //std::cout << '\n';
        }
    */

    // file.get(byte);
    // std::cout << byte << '\n';
    // SHA1((unsigned char *)allbytes, sizeof(allbytes) - 1, hash);
    // std::cout << "hash fr

    /* Torrent tuxtorrent;
    Info infotux;

    setInfo(&infotux, "./SW.jpg", PIECE_LEN, 1);
    createTorrent(&tuxtorrent, infotux);
    showTorrInfo(tuxtorrent);
 */

    // ADDFILE -> AGREGA EL ARCHIVO Y CREA EL TORRENT CON EL HASH COMPLETO. QUE PREGUNTE CUANTAS PIEZAS QUERES AGREGAR.

    // NEWTORR -> AVISA QUE TENES EL TORRENT, PERO HASH VACIO. A MEDIDA QUE LO VAS CONSIGUIENDO, SE VA LLENANDO EL HASH.

    // PIDO DOWNLOAD -> EL TRACKER HACER OR ENTRE LOS HASHES. LE DICE QUE PIEZAS BUSCAR Y DONDE.

    int fd = open("tux.bmp", O_RDWR, 0700);

    int piece_len = 50000;

    char bytes[piece_len];

    int num_hashes = 460854 / piece_len;

    hashesote hola;

    for (int i = 0; i < num_hashes; i++)
    {
        bzero(bytes, piece_len);
        read(fd, bytes, piece_len);
        SHA1((unsigned char *)bytes, sizeof(bytes) - 1, hola.hashesitos[i].hash);
    }

    close(fd);

    fd = open("tux.bmp", O_RDWR, 0700);

    hashesote hola2;

    for (int i = 0; i < num_hashes; i++)
    {
        read(fd, bytes, piece_len);
        if (i % 2)
            SHA1((unsigned char *)bytes, sizeof(bytes) - 1, hola2.hashesitos[i].hash);
    }

    close(fd);
    int vector[num_hashes];

    unsigned char result;

    for (int j = 0; j < num_hashes; j++)
    {
        for (int i = 0; i < 20; i++)
        {
            result = hola.hashesitos[j].hash[i] | hola2.hashesitos[j].hash[i];
            if (result != hola.hashesitos[j].hash[i] || result != hola2.hashesitos[j].hash[i])
            {
                vector[j] = 0;
                break;
            }
            vector[j] = 1;
        }
    }
    for(int k = 0; k<num_hashes; k++)
        cout << vector[k]<< " ";

    
    int X = 56;
    cout << "a: " << X/10 << endl;
    cout << "b: " << X%10 << endl;
}
