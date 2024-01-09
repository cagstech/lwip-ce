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
  ECM_USB_INIT_FAIL,
  ECM_DEVICE_UNSUPPORTED,
  ECM_MEMORY_OVERFLOW,
  ECM_DEVICE_CONFIG_ERROR,

} ecm_error_t;

struct ecm_state
{
  usb_device_t usb_device;
  usb_interface_descriptor_t if_control, if_data;
  usb_descriptor_t configuration;
  usb_endpoint_t ecm_in, ecm_out;
};
extern struct ecm_state ecm_state;

bool ecm_init(void);
struct pbuf *ecm_receive(void);
err_t ecm_transmit(struct netif *netif, struct pbuf *p);

usb_error_t ecm_handle_usb_event(usb_event_t event, void *event_data,
                                 usb_callback_data_t *callback_data)

#endif // LWIP_VETH_H
