#include "proto_p2p.h"

using namespace std;

TorrentFolder mytorrents;


// Siempre corriendo, los peers tienen un puerto para que otros peers le soliciten piezas de archivos que tienen.
// Lo hice con select por si mas de un peer le pide piezas a la vez.

void other_peers(int myport, int s)
{
    int fd_new;
    int peers[MAXCLIENTS];
    char buffer[BUFF_SIZE];

    bzero(buffer, BUFF_SIZE);
    for (int i = 0; i < MAXCLIENTS; i++)
        peers[i] = 0;

    struct sockaddr_in peers_addr;
    int opt = 1;
    fd_set readfds;
    char idx[5];
    int max_aux_s, aux_s, activ, new_socket;
    int scokin;
    scokin = socket(AF_INET, SOCK_STREAM, 0);
    if (scokin < 0)
        cout << "Error creando socket en other peers.\n";
    if (setsockopt(scokin, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0)
        cout << "Error seteando socket.\n";
    peers_addr.sin_family = AF_INET;
    peers_addr.sin_addr.s_addr = INADDR_ANY;
    peers_addr.sin_port = htons(myport);
    int addrlen = sizeof(peers_addr);
    if (bind(scokin, (struct sockaddr *)&peers_addr, sizeof(peers_addr)) < 0)
        cout << "Error en el bind.\n";
    if (listen(scokin, 5) < 0)
        cout << "Erorr en listen.\n";
    while (1)
    {
        FD_ZERO(&readfds);
        FD_SET(scokin, &readfds);
        max_aux_s = scokin;
        for (int i = 0; i < MAXCLIENTS; i++)
        {
            aux_s = peers[i];
            if (aux_s > 0)
                FD_SET(aux_s, &readfds);
            if (aux_s > max_aux_s)
                max_aux_s = aux_s;
        }
        activ = select(max_aux_s + 1, &readfds, NULL, NULL, NULL);
        if (FD_ISSET(scokin, &readfds))
        {
            new_socket = accept(scokin, (struct sockaddr *)&peers_addr, (socklen_t *)&addrlen);
            for (int i = 0; i < MAXCLIENTS; i++)
            {
                if (peers[i] == 0)
                {
                    peers[i] = new_socket;
                    break;
                }
            }
        }
        bzero(buffer, BUFF_SIZE);
        for (int i = 0; i < MAXCLIENTS; i++)
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
                    // Es el peer que solicita las piezas el que se conecta, y nos manda la informacion del
                    // nombre del archivo, y el indice de la pieza que nos pide.
                    bzero(idx, 5);
                    read_prot(aux_s, idx);
                    cout << "idx: " << idx << endl;
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
            bzero(buffer, BUFF_SIZE);
        }
    }
}

// Descarga un torrent desde el tracker.

void download_torr(int s)
{

    Msg torr;
    cout << "Descargando...\n";
    recvMsg(s, &torr);
    addTorrent(&mytorrents, torr.payload.torrent);
    cout << "Descargado: \n";
    showTorrInfo(torr.payload.torrent);
}

// PThread para pedir piezas de archivo a distintos peers, que el tracker nos dio informacion
// de donde encontrarlas.

void *pedir_piezas(void *args)
{
    ArgThreads *thread_args = (ArgThreads *)args;
    IdxInfo indice = thread_args->idxinfo;
    struct sockaddr_in peers_addr;
    int opt = 1;
    int scokin;
    scokin = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv;
    struct hostent *serv_;
    serv_ = gethostbyname(indice.address);
    int portno = indice.port;
    bzero((char *)&serv, sizeof(serv));
    serv.sin_family = AF_INET;
    bcopy((char *)serv_->h_addr, (char *)&serv.sin_addr.s_addr, serv_->h_length);
    serv.sin_port = htons(portno);
    if (connect(scokin, (struct sockaddr *)&serv, sizeof(serv)) < 0)
        fprintf(stderr, "Error de conexion...\n");
    int fd = open(thread_args->filename, O_CREAT | O_RDWR, 0700);
    assert(fd > 0);
    char bytess[PIECE_LEN];
    bzero(bytess, PIECE_LEN);
    int filesize = thread_args->filesize;
    progressbar bar(indice.idxlist.size);
    bar.set_done_char("█");
    for (int i = 0; i < indice.idxlist.size; i++)
    {
        lseek(fd, indice.idxlist.idx[i] * PIECE_LEN, SEEK_SET);
        write_prot(scokin, thread_args->filename, strlen(thread_args->filename));
        write_prot(scokin, to_string(indice.idxlist.idx[i]).c_str(), strlen(to_string(indice.idxlist.idx[i]).c_str()));

        if (indice.idxlist.idx[i] == filesize / PIECE_LEN)
        {
            read_prot(scokin, bytess);
            write(fd, bytess, filesize % PIECE_LEN);
            SHA1((unsigned char *)bytess, sizeof(bytess), mytorrents.torrents[thread_args->torr_idx].pieces[indice.idxlist.idx[i]].hash);
            
            // Ademas de copiarme el archivo, le tengo que avisar al tracker de mi nueva pieza.
            // Esto se llama seeding y es para que si alguien quiere descargar el archivo mientras yo lo estoy bajando,
            // sea mas rapido.
            
            write_prot(thread_args->s, "update", 7);
            write_prot(thread_args->s, to_string(thread_args->torr_idx).c_str(), strlen(to_string(thread_args->torr_idx).c_str()));
            write_prot(thread_args->s, to_string(indice.idxlist.idx[i]).c_str(), strlen(to_string(indice.idxlist.idx[i]).c_str()));
            write_prot(thread_args->s, bytess, strlen(bytess));
        }
        else
        {
            read_prot(scokin, bytess);
            write(fd, bytess, PIECE_LEN);
            SHA1((unsigned char *)bytess, sizeof(bytess), mytorrents.torrents[thread_args->torr_idx].pieces[indice.idxlist.idx[i]].hash);
            write_prot(thread_args->s, "update", 7);
            write_prot(thread_args->s, to_string(thread_args->torr_idx).c_str(), strlen(to_string(thread_args->torr_idx).c_str()));
            write_prot(thread_args->s, to_string(indice.idxlist.idx[i]).c_str(), strlen(to_string(indice.idxlist.idx[i]).c_str()));
            write_prot(thread_args->s, (const char*)mytorrents.torrents[thread_args->torr_idx].pieces[indice.idxlist.idx[i]].hash, strlen((const char *)mytorrents.torrents[thread_args->torr_idx].pieces[indice.idxlist.idx[i]].hash));
        }
        bar.update();
    }
    close(scokin);
    close(fd);
    return NULL;
}

// Funcion que recibe los datos del tracker de donde buscar las piezas,
// larga los PThreads y mide el tiempo (para ver que tarda menos la descarga si hay mas peers).

void requestfile(int s)
{

    char buffer[BUFF_SIZE];
    cout << "Estos son sus torrents: \n";
    showMyTorrents(mytorrents);
    cout << "Que archivo desea descargar?: ";
    bzero(buffer, BUFF_SIZE);
    fgets(buffer, BUFF_SIZE, stdin);
    int idx = checkTorr(mytorrents, buffer);
    if (idx != -1)
    {
        write_prot(s, "reqf", 4);
        Msg msg_torr;
        bzero(&msg_torr, sizeof(Msg));
        setMsgTorr(&msg_torr, mytorrents.torrents[idx]);
        sendMsg(s, &msg_torr);
        Msg msg_info;
        recvMsg(s, &msg_info);
        showInfo(msg_info.payload.info);
        int ct = msg_info.payload.info.cant;
        if (ct == 0)
        {
            cout << "Nadie tiene el archivo!\n"; // En realidad esto no pasa nunca, porque si tengo el torrent es porque alguien lo creo.
            return;
        }
        auto start = std::chrono::high_resolution_clock::now();
        pthread_t hilos[ct];
        ArgThreads args_hilos[ct];
        int idx0;
        int result;

        // Voy a crear tantos pthreads como cantidad de peers que tengan piezas utiles.

        for (idx0 = 0; idx0 < ct; idx0++)
        {
            args_hilos[idx0].torr_idx = idx;
            args_hilos[idx0].idxinfo = msg_info.payload.info.idxinfo[idx0];
            args_hilos[idx0].s = s;
            bzero(args_hilos[idx0].filename, 50);
            memcpy(args_hilos[idx0].filename, buffer, strlen(buffer) - 1); // Cuando vean un strlen(buffer) - 1 es para sacarme de encima el '\n'.
            args_hilos[idx0].filesize = msg_torr.payload.torrent.length;
            result = pthread_create(&hilos[idx0], NULL, pedir_piezas, &args_hilos[idx0]);
            assert(!result);
        }
        for (idx0 = 0; idx0 < ct; idx0++)
        {
            result = pthread_join(hilos[idx0], NULL);
            assert(!result);
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        cout << "\nTiempo de descarga: " << duration.count() << " ms." << endl;

    }
    else
        return;
}

int main(int argc, char *argv[])
{
    inic_torrents(&mytorrents);
    struct sockaddr_in serv_addr;
    int s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0)
    {
        fprintf(stderr, "Error abriendo el socket.\n");
        exit(0);
    }

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
    {
        fprintf(stderr, "Error de conexion.\n");
        exit(0);
    }

    else
        printf("✧✧✧ WELCOME TO IBTORRENT ✧✧✧\n");

    char buffer[BUFF_SIZE];
    bzero(buffer, BUFF_SIZE);
    char myname[BUFF_SIZE];
    char myport[20];
    bzero(myport, 20);
label1:
    bzero(myname, BUFF_SIZE);

    cout << "Ingrese su username: ";
    fgets(myname, BUFF_SIZE, stdin);
    write_prot(s, myname, strlen(myname) - 1);
    read_prot(s, buffer);
    if (strcmp(buffer, "used") == 0)
    {
        cout << "Ese usuario ya existe!\n";
        goto label1;
    }
    cout << "Ingrese su puerto: ";
    fgets(myport, 20, stdin);
    write_prot(s, myport, strlen(myport));
    char consolename[BUFF_SIZE];
    bzero(consolename, BUFF_SIZE);
    memcpy(consolename, myname, strlen(myname) - 1);

    // Largo el thread para dar piezas.

    thread oth(other_peers, atoi(myport), s);

    while (1)
    {
        cout << consolename << "@ibtorr> ";
        bzero(buffer, BUFF_SIZE);
        fgets(buffer, BUFF_SIZE, stdin);

        // Lee comandos y actua.

        if (strcmp(buffer, "addfile\n") == 0)
        {
            bzero(buffer, BUFF_SIZE);
            cout << "Ingrese el nombre del archivo: ";
            fgets(buffer, BUFF_SIZE, stdin);
            char name[BUFF_SIZE];
            bzero(name, BUFF_SIZE);
            memcpy(name, buffer, strlen(buffer) - 1);
            int fd = open(name, O_RDWR, 0700);
            if (fd < 0)
            {
                cout << "No tiene ese archivo!\n";
                bzero(buffer, BUFF_SIZE);
                continue;
            }
            // Si tengo el archivo para anadir y crear el torrent, le aviso al tracker.
            write_prot(s, "add", 4);
            char byte[1];
            int length = 0;
            while (read(fd, byte, 1))
            {
                length++;
            }
            // Esto de elegir las piezas que quiero anadir lo use para probar el algoritmo
            // que optimiza la repartija de piezas, y cuando no lo tenia hecho, lo usaba
            // para simular casos en que dos peers tenian (por ejemplo) mitad de archvio cada uno, para
            // mostrar que la descarga tardaba la mitad.
            // Se puede obviar, para usarlo descomentar y reemplazar lo que se indica en la linea 346.

            /* cout << "Este archivo tiene " << length / PIECE_LEN + 1 << " partes. Ingrese indice inferior: ";
            char ii[10]; // Indice inferior.
            bzero(ii, 10);
            fgets(ii, 10, stdin);
            cout << "Ingrese indice superior: ";
            char is[10]; // Indice superior.
            bzero(is, 10);
            fgets(is, 10, stdin);
            close(fd); */

            Torrent torr_buff = createTorrent(name, PIECE_LEN, 1, 0, length / PIECE_LEN + 1); // Reemplazar 4to arg por atoi(ii) y 5to arg por atoi(is).
            addTorrent(&mytorrents, torr_buff);
            Msg msg_torr;
            bzero(&msg_torr, sizeof(Msg));
            setMsgTorr(&msg_torr, torr_buff);
            sendMsg(s, &msg_torr);
            cout << "Torrent anadido:\n";
            showTorrInfo(msg_torr.payload.torrent);
            bzero(buffer, BUFF_SIZE);
        }

        // Muestra mis torrents.

        if (strcmp(buffer, "mytorr\n") == 0)
        {
            showMyTorrents(mytorrents);
            bzero(buffer, BUFF_SIZE);
        }

        // Le pido un torrent al tracker.

        if (strcmp(buffer, "reqt\n") == 0)
        {
            write_prot(s, "reqt", 4);
            cout << "Nombre del torrent a descargar: ";
            bzero(buffer, BUFF_SIZE);
            fgets(buffer, BUFF_SIZE, stdin);
            write_prot(s, buffer, strlen(buffer) - 1);
            bzero(buffer, BUFF_SIZE);
            read_prot(s, buffer);
            if (strcmp(buffer, "found") == 0)
                download_torr(s);
            else
                cout << "Torrent no encotnrado.\n";
            bzero(buffer, BUFF_SIZE);
        }

        // Le pido al tracker la informacion necesaria para descargar un archivo.

        if (strcmp(buffer, "reqf\n") == 0)
        {
            requestfile(s);
            bzero(buffer, BUFF_SIZE);
        }
    }

    oth.join();
    close(s);
    return 0;
}