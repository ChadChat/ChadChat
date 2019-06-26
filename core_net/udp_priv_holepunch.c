#include "udp_priv_holepunch.h"
#include "utils/log.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

/* TODO: Check if it's some common port and change it */
/* This should be the local port that we connect to
 * the server with
 */
#define UDP_PRIV_PORT	10801

#define SZ_IPV4_ADDR	sizeof(struct in_addr)
#define SZ_IPV4_PORT	sizeof(short)
#define SZ_SRVR_ST		sizeof(struct sockaddr_in)

RES udp_priv_hp_init(udp_priv_holepunch *hp)
{
	struct ifaddrs *ifaddr;

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

RES udp_send_info(udp_priv_holepunch *hp, char *server_name)
{
	struct hostent *host;
	struct in_addr srvr_ip;
	short port = UDP_PRIV_PORT;
	char to_send[256];
	struct sockaddr_in sin = {0};
	size_t len = ;

	if (!(host = gethostbyname(server_name))) {
		log(LOG_ERR, "gethostbyname failed");
		return RES_ERR;
	}

	memcpy(&ip, host->h_addr_list[0], SZ_IPV4_ADDR);

	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = srvr_ip;

	memcpy(to_send, &srvr_ip, SZ_IPV4_ADDR);
	memcpy(to_send+SZ_IPV4_ADDR, &port, SZ_IPV4_PORT);
	/* TODO: add signature here */
	/* TODO: we need a simple scheduler to keep the
	 * connection open with the server */

	sendto(hp->socket, to_send, len, 0, sin, SZ_SRVR_ST);

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
