/****************************************************************************
 * Code for Ethernet Control Model (ECM)
 * for USB-Ethernet devices
 */

#include <stdint.h>
#include "cdc.h"

#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"

/* This code processes an incoming ECM Ethernet frame */
err_t ecm_process(struct netif *netif, uint8_t *buf, size_t len)
{
  struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  if (p != NULL)
  {
    pbuf_take(p, buf, len);
    if (netif->input(p, netif) != ERR_OK)
      pbuf_free(p);
    return ERR_OK;
  }
  return ERR_MEM;
}

/* This code sends an ECM Ethernet frame */
err_t ecm_bulk_transmit(struct netif *netif, struct pbuf *p)
{
  eth_device_t *dev = (eth_device_t *)netif->state;
  if (p->tot_len > ETHERNET_MTU)
    return ERR_MEM;
  struct pbuf *tbuf = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
  LINK_STATS_INC(link.xmit);
  // Update SNMP stats(only if you use SNMP)
  MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
  if (pbuf_copy(tbuf, p))
    return ERR_MEM;
  if (usb_ScheduleBulkTransfer(dev->endpoint.out, tbuf->payload, p->tot_len, bulk_transmit_callback, tbuf) == USB_SUCCESS)
    return ERR_OK;
  else
    return ERR_IF;
}
