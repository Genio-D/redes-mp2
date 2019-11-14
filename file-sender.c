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

int main(int argc, char ** argv) {
    struct sockaddr_in server_addr;
	int conversion_status;
	int connection_status;
	struct hostent *host;
	struct in_addr **addr_list;

    char *filepath = argv[1];
    char *host_name = argv[2];
    int port = atoi(argv[3]);
    int window_size = atoi(argv[4]);

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

    
}