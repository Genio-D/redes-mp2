#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include "packet-format.h"

#define CHUNK_SIZE 1000

int main(int argc, char ** argv) {

    char *result_filepath = argv[1];
    int port = atoi(argv[2]);
    int window_size = atoi(argv[3]);

    int listen_fd;
    struct sockaddr_in serverSocket, clientSocket;
	serverSocket.sin_family = AF_INET;
	serverSocket.sin_port = htons(port);
	serverSocket.sin_addr.s_addr = INADDR_ANY;

    data_pkt_t data_pkt;
    ack_pkt_t ack_pkt;

    FILE *result_file;

    result_file = fopen(result_filepath, "w+");
    /*
    fseek(result_file, long offset, SEEK_SET); -1 on error
    */

    if(argc != 4) {
        printf("Wrong number of arguments.\nUsage: {filepath} {port} {windowsize}\n");
        exit(-1);
    }

    listen_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listen_fd == -1)
        exit(-1);

    if(bind(listen_fd, (struct sockaddr *)&serverSocket, sizeof(serverSocket)) < 0)
		exit(-1);

    int base = 0;
    
    exit(0);
}