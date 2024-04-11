/**
 * Header File for Communications Data Class (CDC)
 * for USB-Ethernet devices
 */

#ifndef cdc_h
#define cdc_h

#include <stdint.h>
#include <usbdrvce.h>

#include "ecm.h"
#include "ncm.h"

#include "lwip/err.h"
#include "lwip/netif.h"

/* Ethernet MTU */
#define ETHERNET_MTU 1518

#define USB_CS_INTERFACE_DESCRIPTOR 0x24
#define USB_UNION_FUNCTIONAL_DESCRIPTOR 0x06
#define USB_ETHERNET_FUNCTIONAL_DESCRIPTOR 0x0F

// supported devices
enum eth_usb_devices
{
  DEVICE_NONE,
  DEVICE_ECM = USB_ECM_SUBCLASS,
  DEVICE_NCM = USB_NCM_SUBCLASS
};

// usb device metadata
typedef struct _eth_device_t
{
  usb_device_t device;
  uint8_t type;
  uint8_t hwaddr[6];
  union
  {
    struct _ncm ncm;
    struct _ecm ecm;
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

enum _cdc_request_codes // request types for CDC-ECM and CDC-NCM
{
  REQUEST_SEND_ENCAPSULATED_COMMAND = 0,
  REQUEST_GET_ENCAPSULATED_RESPONSE,
  REQUEST_SET_ETHERNET_MULTICAST_FILTERS = 0x40,
  REQUEST_SET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER,
  REQUEST_GET_ETHERNET_POWER_MANAGEMENT_PATTERN_FILTER,
  REQUEST_SET_ETHERNET_PACKET_FILTER,
  REQUEST_GET_ETHERNET_STATISTIC,
  REQUEST_GET_NTB_PARAMETERS = 0x80,
  REQUEST_GET_NET_ADDRESS,
  REQUEST_SET_NET_ADDRESS,
  REQUEST_GET_NTB_FORMAT,
  REQUEST_SET_NTB_FORMAT,
  REQUEST_GET_NTB_INPUT_SIZE,
  REQUEST_SET_NTB_INPUT_SIZE,
  REQUEST_GET_MAX_DATAGRAM_SIZE,
  REQUEST_SET_MAX_DATAGRAM_SIZE,
  REQUEST_GET_CRC_MODE,
  REQUEST_SET_CRC_MODE,
};

enum _cdc_notification_codes
{
  NOTIFY_NETWORK_CONNECTION,
  NOTIFY_RESPONSE_AVAILABLE,
  NOTIFY_CONNECTION_SPEED_CHANGE = 0x2A
};

typedef struct
{
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t data[];
} usb_cs_interface_descriptor_t;

typedef struct
{
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t bControlInterface;
  uint8_t bInterface;
} usb_union_functional_descriptor_t;

typedef struct
{
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint8_t iMacAddress;
  uint8_t bmEthernetStatistics[4];
  uint8_t wMaxSegmentSize[2];
  uint8_t wNumberMCFilters[2];
  uint8_t bNumberPowerFilters;
} usb_ethernet_functional_descriptor_t;

usb_error_t eth_handle_usb_event(usb_event_t event, void *event_data,
                                 usb_callback_data_t *callback_data);

#endif
