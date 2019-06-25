#ifndef UDP_PRIV_HOLEPUNCH_IMPL_CHADCHAT
#define UDP_PRIV_HOLEPUNCH_IMPL_CHADCHAT

#include "holepunch.h"

#include <ifaddrs.h>

struct udp_priv_holepunch
{
	enum HP_STATE state;
	int socket;
	struct ifaddrs *ifaddr;
};


#endif /* ifndef UDP_HOLEPUNCH_IMPL_CHADCHAT */
