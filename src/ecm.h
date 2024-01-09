#ifndef ECM_H
#define ECM_H
/**
 * CDC "Ethernet Control Model (ECM)" driver for virtual Ethernet over USB
 * author: acagliano
 * platform: TI-84+ CE
 * license: GPL3
 */

#include <stdint.h>
#include <stdbool.h>
#include <usbdrvce.h>
// #include "lwip/arch.h"
// #include "lwip/err.h"
// #include "lwip/netif.h"
// #include "lwip/pbuf.h"

typedef enum
{
  ECM_OK,
  ECM_ERROR_NO_DEVICE = -1,
  ECM_ERROR_INVALID_DEVICE = -2,
  ECM_ERROR_MEMORY = -3,
  ECM_ERROR_INVALID_DESCRIPTOR = -4,
  ECM_CONFIGURATION_ERROR = -5,
  ECM_INTERFACE_ERROR = -6,
  ECM_ERROR_TX = -7,
  ECM_ERROR_RX = -8
} ecm_error_t;

typedef struct _ecm_device_t
{
  bool ready;
  usb_device_t usb_device;
  usb_interface_descriptor_t *if_control, *if_data;
  // usb_descriptor_t *conf_active;
  usb_endpoint_t in, out;
} ecm_device_t;
extern ecm_device_t ecm_device;

bool ecm_init(void);
struct pbuf *ecm_receive(void);
ecm_erorr_t ecm_transmit(struct netif *netif, struct pbuf *p);

usb_error_t ecm_handle_usb_event(usb_event_t event, void *event_data,
                                 usb_callback_data_t *callback_data)

#endif // LWIP_VETH_H
