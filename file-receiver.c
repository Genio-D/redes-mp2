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
#include <errno.h>
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


void file_write(FILE *fd, int seq_num, char *data, int len) {
    int offset = seq_num * CHUNK_SIZE;
    printf("writing message at position %d\n", offset);
    fseek(fd, offset, SEEK_SET);
    int ret = fwrite(data, 1, len, fd);
    printf("wrote %d bytes\n", ret);
}


int main(int argc, char ** argv) {

    char *result_filepath = argv[1];
    int port = atoi(argv[2]);
    int window_size = atoi(argv[3]);

    int listen_fd;
    struct sockaddr_in serverSocket, clientSocket;
    memset(&serverSocket, 0, sizeof(serverSocket));
    memset(&clientSocket, 0, sizeof(clientSocket));
	serverSocket.sin_family = AF_INET;
	serverSocket.sin_port = htons(port);
	serverSocket.sin_addr.s_addr = INADDR_ANY;

    data_pkt_t *data_pkt = (data_pkt_t *) malloc(sizeof(data_pkt_t)); 
    ack_pkt_t *ack_pkt = (ack_pkt_t *) malloc(sizeof(ack_pkt_t)); 

    FILE *result_file;
    int i;

    result_file = fopen(result_filepath, "w+b");

    if(argc != 4) {
        printf("Wrong number of arguments.\nUsage: {filepath} {port} {windowsize}\n");
        exit(-1);
    }

    listen_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_fd == -1) {
        printf("failed to create socket\n");
        exit(-1);
    }
    int enable = 1;
    int ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable,
		sizeof(enable));
	if(ret < 0) {
		perror("setsockopt err");
		exit(-1);
	}
    ret = setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, (const char*)&enable,
		sizeof(enable));
	if(ret < 0) {
		perror("setsockopt err");
		exit(-1);
	}
    if(bind(listen_fd, (struct sockaddr *)&serverSocket, sizeof(serverSocket)) < 0) {
        printf("failed to bind socket\n");
		exit(-1);
    }

    int window_start = 0;
    int window_end = window_size;
    int last_packet_received = 0; /*flag*/
    int packets[window_size];

    for(i = 0; i < window_size; i ++)
        packets[i] = 0;

    while(1) {
        if(last_packet_received){
            printf("reached end, closing stuff\n");
            break;
        }
        printf("waiting to receive data pkt\n");
        fflush(stdout);
        socklen_t clientSocket_len;
        clientSocket_len = sizeof(clientSocket);

        // memset(data_pkt->data, '\0', CHUNK_SIZE);
        int recv_len = recvfrom(listen_fd, data_pkt, sizeof(data_pkt_t), 0,
            (struct sockaddr *) &clientSocket, &clientSocket_len);
        if(recv_len == -1) {
            printf("read error");
            exit(-1);
        }
        int len = recv_len - sizeof(data_pkt->seq_num);
        printf("received %d bytes\n", recv_len);
        // printf("pkt data:\n_____\n%s\n_____\n", data_pkt->data);
        if(len < CHUNK_SIZE) {
            last_packet_received = 1;
        }
        int seq = data_pkt->seq_num;
        printf("got data pkt seq = %d\n", seq);

        ack_pkt->seq_num = seq + 1;
        ack_pkt->selective_acks = 0;
        if(seq >= window_start && seq <= window_end) {
            packets[seq - window_start] = 1;
            file_write(result_file, seq, data_pkt->data, len);
            printf("sending ack\n");

            int val = sendto(listen_fd, ack_pkt, sizeof(ack_pkt_t), 0,
                (struct sockaddr *) &clientSocket, clientSocket_len);
            if(val == -1) {
                perror("bad send\n");
                exit(-1);
            }

            if(seq == window_start) {
                int increment = move_window(packets, window_size);
                window_start += increment;
                window_end += increment;
            }
        }
    }
    fclose(result_file);
    close(listen_fd);
    
    exit(0);
}