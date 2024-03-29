#ifndef defaults_h
#define defaults_h

#include <usbdrvce.h>
#include "include/lwip/netif.h"
#include "include/lwip/pbuf.h"

// max transfer of ethernet
#define ETHERNET_MTU 1518

// Defines Ethernet specific classes/subclasses not defined in <usbdrvce.h>
#define USB_ECM_SUBCLASS 0x06
#define USB_NCM_SUBCLASS 0x0D
#define USB_CS_INTERFACE_DESCRIPTOR 0x24
#define USB_UNION_FUNCTIONAL_DESCRIPTOR 0x06
#define USB_ETHERNET_FUNCTIONAL_DESCRIPTOR 0x0F
#define USB_NCM_FUNCTIONAL_DESCRIPTOR 0x1A

// supported devices
enum eth_usb_devices
{
  DEVICE_NONE,
  DEVICE_ECM = 0x06,
  DEVICE_NCM = 0x0D
};

// usb device metadata
typedef struct _eth_device_t
{
  usb_device_t device;
  uint8_t type;
  uint8_t hwaddr[6];
  union
  {
    struct _ncm
    {
      uint8_t bm_capabilities;
      uint16_t sequence; /* per NCM device - transfer sequence counter */
      struct _ntb_params
      {
        /* NTB Parameters (Returned from Control Request) */
        uint16_t wLength;
        uint16_t bmNtbFormatsSupported;
        uint32_t dwNtbInMaxSize;
        uint16_t wNdpInDivisor;
        uint16_t wNdpInPayloadRemainder;
        uint16_t wNdpInAlignment;
        uint16_t reserved;
        uint32_t dwNtbOutMaxSize;
        uint16_t wNdpOutDivisor;
        uint16_t wNdpOutPayloadRemainder;
        uint16_t wNdpOutAlignment;
        uint16_t wNtbOutMaxDatagrams;
      } ntb_params;
    } ncm;
    struct _ecm
    {
      uint8_t dummy;
    } ecm;
  } class;
  struct
  {
    usb_endpoint_t in, out, interrupt;
  } endpoint;
  err_t (*process)(struct netif *netif, uint8_t *buf, size_t len);
  err_t (*emit)(struct netif *netif, struct pbuf *p);
  struct netif iface;
} eth_device_t;
extern eth_device_t eth;
// extern struct netif eth_netif;
// extern uint8_t eth_rx_buf[ETHERNET_MTU];

usb_error_t eth_handle_usb_event(usb_event_t event, void *event_data,
                                 usb_callback_data_t *callback_data);

#endif
