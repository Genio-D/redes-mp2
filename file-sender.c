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

void send_packet(int socket_fd, FILE *fp, int seq) {
	printf("sending packet %d\n", seq);
	char message[CHUNK_SIZE];
	int offset = (seq - 1) * CHUNK_SIZE;
	fseek(fp, offset, SEEK_SET);
	int val = fread(message, 1, CHUNK_SIZE, fp);
	data_pkt_t packet;
	strcpy(packet.data, message);
	packet.seq_num = seq;
	int ret = write(socket_fd, &packet, sizeof(packet));
}

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

int main(int argc, char ** argv) {
	int server_fd;
    struct sockaddr_in server_addr;
	int conversion_status;
	struct hostent *host;
	struct in_addr **addr_list;

    char *filepath = argv[1];
    char *host_name = argv[2];
    int port = atoi(argv[3]);
    int window_size = atoi(argv[4]);

	data_pkt_t data_pkt;
    ack_pkt_t ack_pkt;

	FILE *read_file;

	read_file = fopen(filepath, "r");

	if(argc != 5) {
        printf("Wrong number of arguments.\nUsage: {filepath} {address} {port} {windowsize}\n");
        exit(-1);
    }

	server_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(server_fd == -1) {
		fprintf(stderr, "socket: Error\n");
		exit(-1);
	}

    host = gethostbyname(host_name);

	if(!host)
		fprintf(stderr, "gethostbyname: Error\n");

	addr_list = (struct in_addr **)host->h_addr_list;
	char *host_address = inet_ntoa(*addr_list[0]);

	conversion_status = inet_pton(AF_INET, host_address, &server_addr.sin_addr);
	if(!conversion_status) {
		fprintf(stderr, "inet_pton: Error\n");
		exit(-1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	int window_start = 0;
	int window_end = window_size;
	int packets[window_size];
	int num_timeouts = 0;
	int i;

    for(i = 0; i < window_size; i ++)
        packets[i] = 0;

	while(1) {
		if(num_timeouts == 3) {
			fclose(read_file);
			close(server_fd);
			exit(-1);
		}

		for(i = 0; i < window_size; i++) {
			if(packets[i] == 0) {
				send_packet(server_fd, read_file, i);
			}
		}
	}
    
}