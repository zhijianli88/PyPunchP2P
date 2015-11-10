#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

char stun_servers_list[6][120] = {
	"stun.ekiga.net",
	"stunserver.org",
	"stun.ideasip.com",
	"tun.softjoys.com",
	"tun.voipbuster.com",
};

/* NAT TYPE */
enum {
	BLOCKED = 0,
	OPEN_INTERNET,
	FULL_CONE,
	Symmetric_UDP_FIREWALL,
	RESTRICT_NAT,
	RESTRICT_PORT_NAT,
	Symmetric_NAT,
};

//stun attributes
#define MappedAddress		0x0001
#define ResponseAddress	 	0x0002
#define ChangeRequest		0x0003
#define SourceAddress		0x0004
#define ChangedAddress		0x0005
#define Username		0x0006
#define Password		0x0007
#define MessageIntegrity	0x0008
#define ErrorCode		0x0009
#define UnknownAttribute	0x000A
#define ReflectedFrom		0x000B
#define XorOnly			0x0021
#define XorMappedAddress	0x8020
#define ServerName		0x8022
#define SecondaryAddress	0x8050  // Non standard extention

//types for a stun message
#define BindRequestMsg		0x0001
#define BindResponseMsg		0x0101
#define BindErrorResponseMsg	0x0111
#define SharedSecretRequestMsg	0x0002
#define SharedSecretResponseMsg	0x0102
#define SharedSecretErrorResponseMsg	0x0112

struct stun_header {
	unsigned short type;
	unsigned short msg_len;
	unsigned int magic;
	unsigned char id[12];
	unsigned char data[108];
};

static int stun_test(int sock, char *data)
{
	return -1;
}

#define MAGIC 0x19880713

static unsigned char transation_id[12] = {
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x99, 0xaa, 0xbb, 0xcc
};

static void dump_stun_header(struct stun_header *header)
{
	int i, len = ntohs(header->msg_len);
	unsigned char *buf = (unsigned char *)header;

	printf("header: ");
	for (i = 0; i < 20 ; i++)
		printf ("0x%02x ", buf[i]);
	printf("\ndata(%d): ", len);
	for (i = 0; i < len ; i++)
		printf("0x%02x ", header->data[i]);
	printf("\n");
}

static void build_stun_request(struct stun_header *req,
							int type, unsigned char *msg, int msg_len)
{
	req->type = htons(type);
	req->msg_len = htons(msg_len);
	req->magic = htonl(MAGIC);
	memcpy(req->id, transation_id, 12);
	if (msg && msg_len)
		memcpy(req->data, msg, msg_len);

	dump_stun_header(req);
}

struct nat_addr {
	unsigned int ip;
	unsigned short port;
};

struct nat_info {
	struct nat_addr ext;
	struct nat_addr change;
	struct nat_addr src;
};

static struct nat_info nat_got;

static void dump_nat_info(struct nat_info *info)
{
	if (info->src.port)
		printf("source addr: %d.%d.%d.%d:%d\n",
		info->src.ip & 0xff,
		(info->src.ip >> 8) & 0xff,
		(info->src.ip >> 16) & 0xff,
		(info->src.ip >> 24) & 0xff,
		info->src.port);

	if (info->ext.port)
		printf("external addr: %d.%d.%d.%d:%d\n",
		info->ext.ip & 0xff,
		(info->ext.ip >> 8) & 0xff,
		(info->ext.ip >> 16) & 0xff,
		(info->ext.ip >> 24) & 0xff,
		info->ext.port);

	if (info->change.port)
		printf("change addr: %d.%d.%d.%d:%d\n",
		info->change.ip & 0xff,
		(info->change.ip >> 8) & 0xff,
		(info->change.ip >> 16) & 0xff,
		(info->change.ip >> 24) & 0xff,
		info->change.port);

}

/* export api */
int get_nat_type(int sockfd, const char *stun_ip, short port,
				char *src_ip, short src_port)
{
	struct sockaddr_in server_addr, local_addr;
	struct stun_header request, response;
	int ret, len;
	unsigned char *attr;

	// local
	bzero(&local_addr, sizeof(local_addr));
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(src_port);
	if (src_ip)
		inet_pton(AF_INET, src_ip, &server_addr.sin_addr);
	else
		local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	ret = bind(sockfd,(struct sockaddr *)&local_addr,sizeof(local_addr));
	if (ret < 0) {
		printf("bind error\n");
		return -1;
	}

	// fill server
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, stun_ip, &server_addr.sin_addr);
	server_addr.sin_port = htons(port);

	build_stun_request(&request, BindRequestMsg, NULL, 0);

	ret = sendto(sockfd, &request, 20 + request.msg_len,
		0, (struct sockaddr *)&server_addr, sizeof(server_addr)); // send UDP
	if (ret == -1) {
		printf("sendto error\n");
		return -1;
	}
	// time wait
	usleep(1000 * 100);

	//printf("Read recv ...\n");
	ret = recvfrom(sockfd, &response, 128, 0, NULL,0); // recv UDP
	if (ret == -1 || ret > 128)	{
		printf("recvfrom error\n");
		return -2;
	}

	dump_stun_header(&response);

	len = ntohs(response.msg_len);
	attr = (unsigned char *)response.data;
	while (len > 0) {
		int attr_type = ntohs(*((unsigned short *)attr));
		int attr_len = ntohs((*((unsigned short *)(attr + 2))));
		switch (attr_type) {
		case MappedAddress:
			printf("attr type MappedAddress\n");
			nat_got.ext.port = ntohs(*((unsigned short *)(attr + 6)));
			memcpy(&nat_got.ext.ip, attr + 8, 4);
			break;
		case SourceAddress:
			printf("attr type SourceAddress\n");
			nat_got.src.port = ntohs(*((unsigned short *)(attr + 6)));
			memcpy(&nat_got.src.ip, attr + 8, 4);
			break;
		case ChangedAddress:
			printf("attr type ChangeAddress\n");
			nat_got.change.port = ntohs(*((unsigned short *)(attr + 6)));
			memcpy(&nat_got.change.ip, attr + 8, 4);
			break;
		default:
			printf("attr type is 0x%04x\n", attr_type);
		}
		len -= (4 + attr_len);
		attr += (4 + attr_len);
		printf("remain len %d, attr_len %d\n", len, attr_len);
	}

	dump_nat_info(&nat_got);
	return 0;
}

int main(void)
{
	int sockfd;
	if ((sockfd= socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}
	get_nat_type(sockfd, "217.10.68.152", 3478, NULL, 0);

	return 0;
}

