#ifndef ECM_H
#define ECM_H
/**
 * CDC "Ethernet Control Model (ECM)" header for integration with lwIP
 * author: acagliano
 * platform: TI-84+ CE
 * license: GPL3
 */

#include <stdint.h>
#include <stdbool.h>
#include <usbdrvce.h>
#include "lwip/arch.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/ip4_addr.h"

enum _ecm_device_status
{
  ECM_UNREADY,
  ECM_READY
};

typedef struct _ecm_device_t
{
  uint8_t status;
  usb_device_t device;
  uint8_t hwaddr[6];
  struct
  {
    size_t in, out;
  } mtu;
  struct
  {
    usb_endpoint_t in, out;
  } endpoint;
  // usb_descriptor_t *conf_active;
} ecm_device_t;
extern ecm_device_t ecm;
extern struct netif ecm_netif;
#define ECM_MTU 1518
extern uint8_t in_buf[ECM_MTU];

usb_error_t ecm_receive(void);
usb_error_t ecm_transmit(struct netif *netif, struct pbuf *p);

usb_error_t ecm_handle_usb_event(usb_event_t event, void *event_data,
                                 usb_callback_data_t *callback_data);

// debug stuff
extern bool transfer_fail;

#endif // LWIP_VETH_H
