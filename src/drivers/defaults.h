#ifndef defaults_h
#define defaults_h

#include "include/lwip/netif.h"

#define ETHERNET_MTU 1518
extern uint8_t in_buf[ETHERNET_MTU];

// used internally, no need to expose
// void netif_link_callback(struct netif *netif);
// void netif_status_callback(struct netif *netif)

#endif
