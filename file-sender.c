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

void send_packet(int socket, FILE *file, int chunk, int chunk_size) {
	char message[chunk_size];
	get_message(file, chunk, chunk_size, message);

	data_pkt_t *packet = make_packet(chunk, message, chunk_size);

	printf("sending seq_num = %d\n", packet->seq_num);
	int ret = send(socket, packet, sizeof(data_pkt_t), 0);
	printf("sent %d bytes\n", ret);
	free_packet(packet);
}

void get_ack(int socket, int *received_acks, int *numacks, int *chunk, int *timeout) {
	assert(received_acks);

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
	*numacks = ack_num;
	*chunk = ack_num;
	received_acks[ack_num] = TRUE;
	free(receiver_ack);
}

int *received_acks_array(int number_chunks) {
	int *array = (int *) malloc(sizeof(int) * number_chunks);
	for(int i = 0; i < number_chunks; i++) {
		array[i] = FALSE;
	}
	return array;
}

int find_file_size(FILE *file) {
	assert(file);

	int num_bytes = 0;
	while(fgetc(file) != EOF ) {
		num_bytes += 1;
	}
	return num_bytes;
}

int find_number_chunks(FILE *file, int chunk_size) {
	assert(file);

	int file_size = find_file_size(file);
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

void sender(int socket, FILE *file, int chunk_size) {
	assert(file);

	int number_chunks = find_number_chunks(file, chunk_size);
	int chunk = 0;
	int *received_acks = received_acks_array(number_chunks);
	int num_acks = 0;
	int timeout = 0;

	while(num_acks < number_chunks) {
		send_packet(socket, file, chunk, chunk_size);
		printf("packet %d sent\n", chunk);
		get_ack(socket, received_acks, &num_acks, &chunk, &timeout);
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
	FILE *file = fopen(filepath, "r");
	// int window_size = atoi(argv[4]);

	sender(socket, file, CHUNK_SIZE);

	fclose(file);
}
