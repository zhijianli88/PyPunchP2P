#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define P2PSERVER 
#define NAT_TYPE 1 //Restrict NAT
#define USAGE "%s <host> <port> <pool>\n"
#define BUFSIZE 1024

int get_nat_type(const char *stun_server, int port)
{
	return 1;
}

int main(int argc, char *argv[])
{
#if 0
	if (argc != 4) {
		printf(USAGE, argv[0]);
		return -1;
	}

	int fd = sockfd(AF_INET, SOCK_DGRAM, 0);
#endif

	struct sockaddr_in server;
	struct sockaddr_in client;
	unsigned long dst_ip;
	int port;
	int s;
	int n;
	char buf[BUFSIZE];
	char cmd[BUFSIZE];
	int zero;

	if (argc != 4) {
		fprintf(stderr, USAGE, argv[0]);
		//return -1;
	}

	if ((dst_ip = inet_addr(argv[1])) == INADDR_NONE) {
		struct hostent *he;

		if ((he = gethostbyname(argv[1])) == NULL) {
			fprintf(stderr,"gethostbyname error\n");
			exit(EXIT_FAILURE);
		}
		memcpy((char *)&dst_ip,(char *)he->h_addr,he->h_length);
	}
	
	if ((port = atoi(argv[2])) == 0) {
		struct servent *se;
		if((se = getservbyname(argv[2], "udp")) != NULL) {
			port=(int)ntohs((u_short)se->s_port);
		}
	}

	if((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	memset((char *)&client, 0, sizeof(client));
	client.sin_family = AF_INET;
	client.sin_addr.s_addr = htonl(INADDR_ANY);
	client.sin_port = htons(0);

	if(bind(s, (struct sockaddr *)&client, sizeof(client)) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	memset((char *)&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = dst_ip;
	//inet_pton(AF_INET, argv[DST_IP], &server.sin_addr); 
	server.sin_port = htons(port);
	zero = 0;

	sendto(s, "1 1", 3, 0, (struct sockaddr*)&server, sizeof(server));
	printf("send 1 1\n");

	n = recvfrom(s, buf, BUFSIZE - 1, 0, (struct sockaddr*)0, &zero);
	if (n < 0) {
		printf("recvfrom error\n");
		return -1;
	}
	printf("recv %d: %s\n", n, buf);

	sendto(s, "ok", 3, 0, (struct sockaddr*)&server, sizeof(server));

	n = recvfrom(s, buf, BUFSIZE - 1, 0, (struct sockaddr*)0, &zero);
	if (n < 0) {
		printf("recvfrom error\n");
		return -1;
	}
	printf("recv %d: %s, %s, %d, %d\n", n, buf,
			inet_ntoa(*(struct in_addr *)buf), *((uint16_t *)(buf + 4)), *((uint16_t *)(buf + 6)));

	//printf("outof while\n");
	//printf("%ld",dst_ip);
	printf("UDP>");
	fflush(stdout);	
	while((n = read(0, buf, BUFSIZE)) > 0) {
		buf[n]='\0';
		sscanf(buf, "%s", cmd);
	
		if(strcmp(cmd, "quit") == 0) {
	 		break;
		}
		if(sendto(s, buf, n, 0, (struct sockaddr*)&server, sizeof(server)) < 0) {
			break;
		}
	
		if((n = recvfrom(s, buf, BUFSIZE-1, 0, (struct sockaddr*)0, &zero)) < 0)
			break;

		buf[n]='\0';
		printf("%s",buf);
		printf("UDP>");
		fflush(stdout);
	}
		close(s);
		return EXIT_SUCCESS;
}

