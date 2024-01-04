#include "veth.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"

static u8_t buf[1518];

ssize_t veth_rx(void) {
    ssize_t result;
    __asm__ volatile ("ld (hl), 12\n"
            : "=l" (result)
            : "l" (-1), "e" (buf));

    return result;
}

void veth_tx(u8_t *tx_buf, size_t size) {
    __asm__ volatile ("ld (hl), 13\n"
            :
            : "l" (-1), "e" (tx_buf), "c" (size));
}

struct pbuf *veth_receive(void) {
    ssize_t size = veth_rx();
    if (size == -1) {
        return NULL;
    }
    struct pbuf* p = pbuf_alloc(PBUF_RAW, size, PBUF_POOL);
    if(p != NULL) {
        pbuf_take(p, buf, size);
    }
    return p;
}

err_t veth_transmit(struct netif *netif, struct pbuf *p)
{
    LINK_STATS_INC(link.xmit);
    /* Update SNMP stats (only if you use SNMP) */
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
    pbuf_copy_partial(p, buf, p->tot_len, 0);
    veth_tx(buf, p->tot_len);
    return ERR_OK;
}

