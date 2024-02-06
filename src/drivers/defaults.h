#ifndef defaults_h
#define defaults_h

#include "include/lwip/netif.h"

#define ETHERNET_MTU 1518

// used internally, no need to expose
// void netif_link_callback(struct netif *netif);
// void netif_status_callback(struct netif *netif);

void netif_init_defaults(struct netif *netif);

#endif
