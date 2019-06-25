#include "udp_pub_holepunch.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

/* TODO: Check if this is a common port */
#define UDP_PUB_PORT	10802

char udp_pub_hp_init(udp_pub_holepunch *hp)
{
	struct sockaddr_in sin = {0};
	struct ifaddrs *ifaddr;

	sin.sin_family = AF_INET;
	sin.sin_port = htons(UDP_PUB_PORT);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if ((hp->socket = socket(AF_INET, SOCK_DGRAM)) == -1) {
		perror("socket");
		return -1;
	}

	if (bind(hp->socket, (struct sockaddr*)&sin, sizeof(sin))) {
		perror("bind");
		close(hp->socket);
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

