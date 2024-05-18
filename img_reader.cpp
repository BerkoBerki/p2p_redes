#include <iostream>
#include <fstream>
#include <iostream>
#include <openssl/sha.h>
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
    //char byte;
    //char allbytes[262144];
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
    
    //file.get(byte);
    //std::cout << byte << '\n';
    //SHA1((unsigned char *)allbytes, sizeof(allbytes) - 1, hash);
    //std::cout << "hash fr

    Torrent tuxtorrent;
    Info infotux;

    setInfo(&infotux, "./SW.jpg");
    createTorrent(&tuxtorrent, infotux);
    showTorrInfo(tuxtorrent);

    return 0;
}
