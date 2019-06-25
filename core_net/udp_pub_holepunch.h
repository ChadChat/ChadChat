#ifndef UDP_PUB_HOLEPUNCH_IMPL_CHADCHAT
#define UDP_PUB_HOLEPUNCH_IMPL_CHADCHAT

#include "holepunch.h"

#include <stdint.h>

struct udp_pub_holepunch
{
	enum HP_STATE state;
	int socket;
	struct ifaddrs *ifaddr;
};

#endif /* ifndef UDP_PUB_HOLEPUNCH_IMPL_CHAD */
