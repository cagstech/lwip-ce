#include "include/lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"

#include "defaults.h"

const char *hostname = "ti84pce";

void netif_link_callback(struct netif *netif)
{
  dhcp_start(netif);
  // dns_init();
}

void netif_status_callback(struct netif *netif)
{
  printf("netif status changed %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
}

void netif_init_defaults(struct netif *netif)
{
  static uint8_t ifnum = 0;
  netif->name[0] = 'e';
  netif->name[1] = 'n';
  netif->num = ifnum++;
  netif_create_ip6_linklocal_address(netif, 1);
  netif_set_hostname(netif, hostname);
  netif->ip6_autoconfig_enabled = 1;
  netif_set_status_callback(netif, netif_status_callback);
  netif_set_link_callback(netif, netif_link_callback);
}
