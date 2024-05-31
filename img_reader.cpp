#include <iostream>
#include <unistd.h>
#include "proto_p2p.h"



int main() {

    char bytes_torr[PIECE_LEN];
    unsigned char hash[200];
    int fd = open("largeSample.bmp", O_RDWR, 0700);

    for (int i = 0; i < 125; i++)
    {
        bzero(bytes_torr, PIECE_LEN);
        read(fd, bytes_torr, PIECE_LEN);
        bzero(hash, 200);
        SHA1((unsigned char *)bytes_torr, sizeof(bytes_torr), hash);

        cout << strlen((const char *)hash) << endl;
    }
    return 0;
}