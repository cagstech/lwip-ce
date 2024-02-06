#include "veth.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/etharp.h"

#include "veth.h"
#include "defaults.h"

struct netif veth_netif;

ssize_t veth_rx(void)
{
    ssize_t result;
    __asm__ volatile("ld (hl), 12\n"
                     : "=l"(result)
                     : "l"(-1), "e"(in_buf));

    return result;
}

void veth_tx(u8_t *tx_buf, size_t size)
{
    __asm__ volatile("ld (hl), 13\n"
                     :
                     : "l"(-1), "e"(tx_buf), "c"(size));
}

struct pbuf *veth_receive(void)
{
    ssize_t size = veth_rx();
    if (size == -1)
    {
        return NULL;
    }
    struct pbuf *p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if (p != NULL)
    {
        pbuf_take(p, in_buf, size);
        if (veth_netif.input(p, &veth_netif != ERR_OK))
            pbuf_free(p);
    }
    return p;
}

err_t veth_transmit(struct netif *netif, struct pbuf *p)
{
    LINK_STATS_INC(link.xmit);
    /* Update SNMP stats (only if you use SNMP) */
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    pbuf_copy_partial(p, buf, p->tot_len, 0);
    veth_tx(tx_buf, p->tot_len);
    return ERR_OK;
}

static err_t
vethif_init(struct netif *netif)
{
    netif->linkoutput = veth_transmit;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, mac_addr, ETH_HWADDR_LEN);
    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif_init_defaults(netif);
}
* /
