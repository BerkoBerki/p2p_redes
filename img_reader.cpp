#include <iostream>
#include <fstream>
#include <iostream>
#include <fcntl.h>
#include <openssl/sha.h>
#include <unistd.h>
#include "proto_p2p.h"

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
    
    int fd = open("tux.bmp", O_RDWR, 0700);

    int fd_new = open("HOLA.bmp", O_CREAT | O_RDWR, 0700);
    
    char byte0[1];
    char byte1[1];

    for(int i = 0; i < 10; i++) 
    {
        cout << i << endl;
        if(i == 5)
            continue;

        cout << i << endl;
    }




}


