
#ifndef ecm_h
#define ecm_h

#include <stdint.h>
#include <usbdrvce.h>

#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#define USB_ECM_SUBCLASS 0x06
struct _ecm
{
  uint8_t dummy;
};

err_t ecm_process(struct netif *netif, uint8_t *buf, size_t len);
err_t ecm_bulk_transmit(struct netif *netif, struct pbuf *p);

#endif
