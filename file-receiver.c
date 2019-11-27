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


int move_window(int *packets, int windowsize) {
    int i, j;
    for(i = 0; i < windowsize; i++) {
        if(packets[0] == 0)
            break;
        for(j = 0; j < windowsize; j++)
            packets[j] = packets[j + 1];
        packets[windowsize - 1] = 0;
    }
    return i;
}


void file_write(FILE *fd, int seq_num, char *data) {
    int offset = (seq_num - 1) * CHUNK_SIZE;
    fseek(fd, offset, SEEK_SET);
    fputs(data, fd);
}

int check_end(int *packets, int last_packet_received, int windowsize) {
    int i;
    if(last_packet_received) {
        for(i = 0; i < windowsize; i++) {
            if(packets[i] == 1)
                return 0;
        }
        return 1;
    }
    else
        return 0;
}

int main(int argc, char ** argv) {

    char *result_filepath = argv[1];
    int port = atoi(argv[2]);
    int window_size = atoi(argv[3]);

    int listen_fd;
    struct sockaddr_in serverSocket;
	serverSocket.sin_family = AF_INET;
	serverSocket.sin_port = htons(port);
	serverSocket.sin_addr.s_addr = INADDR_ANY;

    data_pkt_t data_pkt;
    ack_pkt_t ack_pkt;

    FILE *result_file;
    int i;

    result_file = fopen(result_filepath, "w+");

    if(argc != 4) {
        printf("Wrong number of arguments.\nUsage: {filepath} {port} {windowsize}\n");
        exit(-1);
    }


    listen_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (listen_fd == -1)
        exit(-1);

    if(bind(listen_fd, (struct sockaddr *)&serverSocket, sizeof(serverSocket)) < 0)
		exit(-1);

    int window_start = 0;
    int window_end = window_size;
    int last_packet_received = 0; /*flag*/
    int packets[window_size];

    for(i = 0; i < window_size; i ++)
        packets[i] = 0;

    while(1) {

        int recv_len = read(listen_fd, &data_pkt, sizeof(data_pkt));
        if(recv_len == -1) {
            printf("read error");
            exit(-1);
        }
        if(sizeof(data_pkt.data) < CHUNK_SIZE)
            last_packet_received = 1;
        
        int seq = data_pkt.seq_num;
        ack_pkt.seq_num = seq + 1;
        if(seq >= window_start && seq <= window_end) {
            packets[seq - window_start] = 1;
            file_write(result_file, seq, data_pkt.data);
            if(write(listen_fd, &ack_pkt, recv_len) == -1) {
                printf("write error");
                exit(-1);
            }

            if(seq == window_start) {
                int increment = move_window(packets, window_size);
                window_start += increment;
                window_end += increment;
            }
            if(check_end(packets, last_packet_received, window_size))
                break;
        }
    }
    fclose(result_file);
    close(listen_fd);
    
    exit(0);
}