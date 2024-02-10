#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"
#
#include "veth.h"
#include "defaults.h"

struct netif veth_netif;
uint8_t *veth_tx_buf, *veth_rx_buf;

ssize_t veth_rx(void)
{
    ssize_t result;
    __asm__ volatile("ld (hl), 12\n"
                     : "=l"(result)
                     : "l"(-1), "e"(veth_rx_buf));

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
        pbuf_take(p, veth_rx_buf, size);
        if (veth_netif.input(p, &veth_netif) != ERR_OK)
            pbuf_free(p);
    }
    return p;
}

err_t veth_transmit(struct netif *netif, struct pbuf *p)
{
    LINK_STATS_INC(link.xmit);
    /* Update SNMP stats (only if you use SNMP) */
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    pbuf_copy_partial(p, veth_tx_buf, p->tot_len, 0);
    veth_tx(veth_tx_buf, p->tot_len);
    return ERR_OK;
}

void veth_remove_callback(struct netif *netif)
{
    pbuf_free((struct pbuf *)veth_tx_buf);
    pbuf_free((struct pbuf *)veth_rx_buf);
}

uint8_t veth_mac_addr[] = {0xF3, 0xC7, 0xE1, 0x85, 0xCB, 0x5F};
err_t vethif_init(struct netif *netif)
{
    netif->linkoutput = veth_transmit;
    netif->output = etharp_output;
    netif->output_ip6 = ethip6_output;
    netif->mtu = ETHERNET_MTU;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
    veth_tx_buf = (uint8_t *)pbuf_alloc(PBUF_RAW, ETHERNET_MTU, PBUF_RAM);
    veth_rx_buf = (uint8_t *)pbuf_alloc(PBUF_RAW, ETHERNET_MTU, PBUF_RAM);
    if (!(veth_tx_buf && veth_rx_buf))
        return ERR_MEM;
    MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
    memcpy(netif->hwaddr, veth_mac_addr, ETH_HWADDR_LEN);
    netif->hwaddr_len = ETH_HWADDR_LEN;
    netif_init_defaults(netif);
    netif_set_remove_callback(netif, veth_remove_callback);
    netif_set_up(netif);
    netif_set_link_up(netif);
    netif_set_default(netif);
}
