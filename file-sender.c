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
#include <sys/select.h>
#include <assert.h>

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

data_pkt_t *make_packet(int chunk, char *message) {
	data_pkt_t *packet = (data_pkt_t *) malloc(sizeof(data_pkt_t)); 
	assert(packet);
	assert(message);

	strcpy(packet->data, message);
	packet->seq_num = chunk;
	return packet;
}

void get_message(FILE *file, int chunk, int chunk_size, char *message) {
	assert(file);
	assert(message);

	int offset = chunk * 100;
	fseek(file, offset, SEEK_SET);
	int read = fread(message, 1, chunk_size, file);

	if (read == 0)
	{
		perror("fread error");
	}
}

void send_packet(int socket, FILE *file, int chunk, int chunk_size) {
	char message[chunk_size];
	get_message(file, chunk, chunk_size, message);
	
	data_pkt_t *packet = make_packet(chunk, message);
	send(socket, packet, sizeof(packet), 0);
	free_packet(packet);
}

void get_ack(int socket, int *received_acks) {
	assert(received_acks);

	ack_pkt_t *receiver_ack;
	int len = recv(socket, &receiver_ack, sizeof(ack_pkt_t), 0);
	
	if(len == -1) {
		perror("socket recv error");
		exit(-1);
	}

	int ack_num = receiver_ack->seq_num;
	received_acks[ack_num] = TRUE;
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

	while(num_acks < number_chunks) {
		send_packet(socket, file, chunk, chunk_size);
		printf("packet %d sent\n", chunk);
		get_ack(socket, received_acks);
		printf("ack received\n");
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

	char *filepath = argv[1];
	FILE *file = fopen(filepath, "r");
	// int window_size = atoi(argv[4]);

	sender(socket, file, CHUNK_SIZE);

	fclose(file);
}
