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
#include <assert.h>
#include <errno.h>
#include <sys/time.h>

#define BUFFER_SIZE 4096
#define CHUNK_SIZE 1000
#define TRUE 1
#define FALSE 0


int open_socket(char *host_name, int port) {
	int client_socket;
	struct sockaddr_in server_addr;
	int conversion_status;
	int connection_status;
	struct hostent *host;
	struct in_addr **addr_list;

	client_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (client_socket == -1)
	{
		fprintf(stderr, "socket: Error\n");
		exit(-1);
	}

	host = gethostbyname(host_name);

	if (!host)
		fprintf(stderr, "gethostbyname: Error\n");

	addr_list = (struct in_addr **)host->h_addr_list;
	char *host_address = inet_ntoa(*addr_list[0]);

	conversion_status = inet_pton(AF_INET, host_address, &server_addr.sin_addr);
	if (!conversion_status)
	{
		fprintf(stderr, "inet_pton: Error\n");
		exit(-1);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	connection_status = connect(
		client_socket,
		(struct sockaddr *)&server_addr,
		sizeof(server_addr));

	if (connection_status == -1)
	{
		fprintf(stderr, "connection: Error\n");
		exit(-1);
	}
	return client_socket;
}

void free_packet(data_pkt_t *packet) {
	assert(packet);
	free(packet);
}

data_pkt_t *make_packet(int chunk, char *message, int chunk_size) {
	data_pkt_t *packet = (data_pkt_t *) malloc(sizeof(data_pkt_t)); 
	assert(packet);
	assert(message);
	packet->seq_num = chunk;
	strncpy(packet->data, message, chunk_size);
	return packet;
}

void get_message(FILE *file, int chunk, int chunk_size, char *message) {
	assert(file);
	assert(message);
	memset(message, '\0', chunk_size);
	int offset = chunk * chunk_size;
	fseek(file, offset, SEEK_SET);
	int ret = fread(message, 1, 1000, file);
	printf("read %d bytes\n", ret);
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

void send_packet(int socket, FILE *file, int chunk_size, int windowstart, int windowend) {
	char message[chunk_size];

	for(; windowstart <= windowend; windowstart++) {
		memset(message, '\0', chunk_size);
		get_message(file, windowstart, chunk_size, message);
		data_pkt_t *packet = make_packet(windowstart, message, chunk_size);
		printf("sending seq_num = %d\n", packet->seq_num);
		int ret = send(socket, packet, sizeof(data_pkt_t), 0);
		printf("sent %d bytes\n", ret);
		free_packet(packet);
	}

}

void get_ack(int socket, int *numacks, int *window,
	int *windowstart, int *windowend, int windowsize, int *timeout) {

	ack_pkt_t *receiver_ack = (ack_pkt_t *) malloc(sizeof(ack_pkt_t)); 

	printf("waiting for ack\n");
	int len = recv(socket, receiver_ack, sizeof(ack_pkt_t), 0);
	if(len == -1) {
		if(errno == EAGAIN){
			printf("TIMEOUT\n");
			*timeout += 1;
			if(*timeout == 3) {
				printf("3 timeouts in a row, exiting\n");
				exit(-1);
			}
			return;
		}
		else {
			perror("socket recv error");
			exit(-1);
		}
	}
	printf("got ack %d\n", receiver_ack->seq_num);
	*timeout = 0;
	int ack_num = receiver_ack->seq_num;
	window[ack_num - *windowstart - 1] = 1;
	*numacks += 1;
	int increment = move_window(window, windowsize);
	*windowstart += increment;
	*windowend += increment;

	free(receiver_ack);
}


int find_number_chunks(FILE *file, int chunk_size) {
	assert(file);

	fseek(file, 0, SEEK_END);
	int file_size = ftell(file);
	int q = file_size / chunk_size;
	int r = file_size % chunk_size;

	// If the file_size is not a multiple of 1000
	// Need one more chunk
	if(r > 0) {
		return q + 1;
	}
	else {
		return q;
	}
}

void sender(int socket, FILE *file, int chunk_size, int window_size) {
	assert(file);

	int number_chunks = find_number_chunks(file, chunk_size);
	int num_acks = 0;
	int timeout = 0;
	int window[window_size];
	int window_start = 0;
	int window_end = window_size - 1;
	int i;

	for(i = 0; i < window_size; i++)
		window[i] = 0;

	while(num_acks < number_chunks) {
		send_packet(socket, file, chunk_size, window_start, window_end);
		printf("packets sent: from %d to %d\n", window_start, window_end);
		get_ack(socket, &num_acks, window, &window_start, &window_end, window_size, &timeout);
	}
}

int main(int argc, char ** argv) {
	if(argc != 5) {
        printf("Wrong number of arguments.\nUsage: {filepath} {address} {port} {windowsize}\n");
        exit(-1);
    }

	char *host_name = argv[2];
    int port = atoi(argv[3]);
	int socket = open_socket(host_name, port);

	struct timeval tv = {1, 0}; /*1 second*/

	int ret = setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *) &tv,
		sizeof(struct timeval));
	if(ret < 0) {
		perror("setsockopt err");
		exit(-1);
	}

	char *filepath = argv[1];
	FILE *file = fopen(filepath, "rb");
	int window_size = atoi(argv[4]);

	sender(socket, file, CHUNK_SIZE, window_size);

	fclose(file);
}
