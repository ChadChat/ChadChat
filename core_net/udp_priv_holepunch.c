#include "udp_priv_holepunch.h"
#include "utils/log.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

/* TODO: Check if it's some common port and change it */
#define UDP_PRIV_PORT	10801

RES udp_priv_hp_init(udp_priv_holepunch *hp)
{
	struct sockaddr_in sin = {0};
	struct ifaddrs *ifaddr;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(UDP_PRIV_PORT);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((hp->socket = socket(AF_INET, SOCK_DGRAM)) == -1) {
		perror("socket");
		return -1;
	}

	if (getifaddrs(&ifaddr)) {
		perror("getifaddrs");
		close(hp->socket);
		return -1;
	}

	hp->ifaddr = ifaddr;
	hp->state = INIT_HP;
	return 0;
}

RES udp_send_info(char *server_name)
{
	struct hostent *host;
	struct in_addr ip;

	if (!(host = gethostbyname(server_name))) {
		log(LOG_ERR, "gethostbyname failed");
		return RES_ERR;
	}

	memcpy(&ip, host->h_addr_list[0], sizeof(struct in_addr));

	return RES_OK;
}

RES udp_priv_hp_get_client(udp_priv_holepunch *hp, ___ *pub, ____ *data)
{
	char to_send[512];
	short len_to_send = 0;


	if (sendto(hp->socket, to_send, len_to_send, 0, (struct sock_addr*)&srvr_addr,
				sizeof(struct sockaddr_in)) == -1) {
		perror("sendto");
		return -1;
	}


	return 0;
}

void udp_priv_hp_delete(udp_priv_holepunch *hp)
{
	freeifaddrs(hp->ifaddr);
	close(hp->socket);
}
