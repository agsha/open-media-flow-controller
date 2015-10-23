#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_SEND_CTR 100

int main (int argc, char* argv[]) {

	if (argc != 4) {

		printf("Usage: ./<pgm_name> <ip_addr> <port_num> <if_addr>\n");
		return 0;
	}
	struct sockaddr_in multicast_addr;
	char* buff = "Multicast Hello";
	uint32_t data_len = strlen(buff);;
	int32_t ctr = MAX_SEND_CTR;

	int32_t fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		perror("socket: ");
		exit(-1);
	}

	struct sockaddr_in if_addr;
	memset(&if_addr, 0, sizeof(if_addr));
	if_addr.sin_addr.s_addr = inet_addr(argv[3]);
	if_addr.sin_family = AF_INET;
	if_addr.sin_port = htons(0);
	if (bind(fd, (struct sockaddr*)&if_addr, sizeof(if_addr)) < 0) {
		perror("bind: ");
		exit(-1);
	}

	memset((char *) &multicast_addr, 0, sizeof(multicast_addr));
	multicast_addr.sin_family = AF_INET;
	multicast_addr.sin_addr.s_addr = inet_addr(argv[1]);
	char* tail = NULL;
	uint16_t port = (uint16_t)strtol(argv[2], &tail, 0);
	if (strcmp(argv[2], tail) == 0) {
		printf("Invalid Port Value.\n");
		printf("Usage: ./<pgm_name> <ip_addr> <port_num> <if_addr>\n");
		return 0;
	}
	multicast_addr.sin_port = htons(port);

	while (ctr > 0) {
		if (sendto(fd, buff, data_len + 1, 0,
					(struct sockaddr*)&multicast_addr,
					sizeof(multicast_addr)) < 0) {
			perror("send failed:");
			exit (-1);
		}
		sleep(1);
	}
	return 0;

}


