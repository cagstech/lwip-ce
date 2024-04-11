
#include <sys/util.h>
#include <usbdrvce.h>
#include "cdc.h"

#include "lwip/netif.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/pbuf.h"

/* GLOBAL/BUFFERS DECLARATIONS */
eth_device_t eth = {0};
struct netif eth_netif = {0};
const char hostname[] = "ti84plusce";

/* UTF-16 -> HEX CONVERSION */
uint8_t nibble(uint16_t c)
{
  c -= '0';
  if (c < 10)
    return c;
  c -= 'A' - '0';
  if (c < 6)
    return c + 10;
  c -= 'a' - 'A';
  if (c < 6)
    return c + 10;
  return 0xff;
}

/*******************************************************************************************
 * General Ethernet Callback Functions
 * Link Change/Status Change Callback functions
 */
uint8_t interrupt_buf[64];
uint8_t eth_rx_buf[ETHERNET_MTU];

usb_error_t interrupt_receive_callback(usb_endpoint_t endpoint,
                                       usb_transfer_status_t status,
                                       size_t transferred,
                                       usb_transfer_data_t *data)
{
  eth_device_t *dev = (eth_device_t *)data;
  if ((status == USB_TRANSFER_COMPLETED) && transferred)
  {
    usb_control_setup_t *notify;
    size_t bytes_parsed = 0;
    do
    {
      notify = (usb_control_setup_t *)&interrupt_buf[bytes_parsed];
      if (notify->bmRequestType == 0b10100001)
      {
        switch (notify->bRequest)
        {
        case NOTIFY_NETWORK_CONNECTION:
          if (notify->wValue)
            netif_set_link_up(&eth_netif);
          else
            netif_set_link_down(&eth_netif);
          break;
        case NOTIFY_CONNECTION_SPEED_CHANGE:
          // this will have no effect - calc too slow
          break;
        }
      }
      bytes_parsed += sizeof(usb_control_setup_t) + notify->wLength;
    } while (bytes_parsed < transferred);
  }
  usb_ScheduleInterruptTransfer(dev->endpoint.interrupt, interrupt_buf, 64, interrupt_receive_callback, data);
  return USB_SUCCESS;
}

// bulk RX callback (general)
usb_error_t bulk_receive_callback(usb_endpoint_t endpoint,
                                  usb_transfer_status_t status,
                                  size_t transferred,
                                  usb_transfer_data_t *data)
{
  eth_device_t *dev = (eth_device_t *)data;
  if ((status == USB_TRANSFER_COMPLETED) && transferred)
  {
    LINK_STATS_INC(link.recv);
    MIB2_STATS_NETIF_ADD(&dev->iface, ifinoctets, transferred);
    dev->process(&dev->iface, eth_rx_buf, transferred);
  }
  else
    printf("usb transfer status code: %u\n", status);
  usb_ScheduleBulkTransfer(dev->endpoint.in, eth_rx_buf, ETHERNET_MTU, bulk_receive_callback, data);
  return USB_SUCCESS;
}

// bulk tx callback (this should work for both since it's just a free)
usb_error_t bulk_transmit_callback(usb_endpoint_t endpoint,
                                   usb_transfer_status_t status,
                                   size_t transferred,
                                   usb_transfer_data_t *data)
{
  // Handle completion or error of the transfer, if needed

  if (data)
    pbuf_free(data);

  return USB_SUCCESS;
}

err_t eth_netif_init(struct netif *netif)
{
  eth_device_t *dev = (eth_device_t *)netif->state;
  netif->linkoutput = dev->emit;
  netif->output = etharp_output;
  netif->output_ip6 = ethip6_output;
  netif->mtu = ETHERNET_MTU;
  netif->mtu6 = ETHERNET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
  MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
  memcpy(netif->hwaddr, dev->hwaddr, NETIF_MAX_HWADDR_LEN);
  netif->hwaddr_len = NETIF_MAX_HWADDR_LEN;
  // netif_set_link_callback(netif, eth_link_callback);
  // netif_set_status_callback(netif, eth_status_callback);
  return ERR_OK;
}

/** DESCRIPTOR PARSER AWAIT STATES */
enum _descriptor_parser_await_states
{
  PARSE_HAS_CONTROL_IF = 1,
  PARSE_HAS_MAC_ADDR = (1 << 1),
  PARSE_HAS_BULK_IF_NUM = (1 << 2),
  PARSE_HAS_BULK_IF = (1 << 3),
  PARSE_HAS_ENDPOINT_INT = (1 << 4),
  PARSE_HAS_ENDPOINT_IN = (1 << 5),
  PARSE_HAS_ENDPOINT_OUT = (1 << 6)
};

#define DESCRIPTOR_MAX_LEN 256
/** Parses the descriptors of the new device to see if it's a compatible Ethernet adapter. */
bool init_ethernet_usb_device(usb_device_t device)
{
  eth_device_t tmp = {0};
  eth_device_t *eth = NULL;
  size_t xferd, parsed_len, desc_len;
  usb_error_t err;
  tmp.device = device;
  struct
  {
    usb_configuration_descriptor_t *addr;
    size_t len;
  } configdata;
  struct
  {
    usb_interface_descriptor_t *addr;
    size_t len;
  } if_bulk;
  struct
  {
    uint8_t in, out, interrupt;
  } endpoint_addr;
  union
  {
    uint8_t bytes[DESCRIPTOR_MAX_LEN];   // allocate 256 bytes for descriptors
    usb_descriptor_t desc;               // descriptor type aliases
    usb_device_descriptor_t dev;         // .. device descriptor alias
    usb_configuration_descriptor_t conf; // .. config descriptor alias
  } descriptor;
  err = usb_GetDeviceDescriptor(device, &descriptor.dev, sizeof(usb_device_descriptor_t), &xferd);
  if (err || (xferd != sizeof(usb_device_descriptor_t)))
    return false;

  // check for main DeviceClass being type 0x00 - composite/if-specific
  if (descriptor.dev.bDeviceClass != USB_INTERFACE_SPECIFIC_CLASS)
    return false;

  // parse both configs for the correct one
  uint8_t num_configs = descriptor.dev.bNumConfigurations;
  for (uint8_t config = 0; config < num_configs; config++)
  {
    uint8_t ifnum = 0;
    uint8_t parse_state = 0;
    desc_len = usb_GetConfigurationDescriptorTotalLength(device, config);
    parsed_len = 0;
    if (desc_len > 256)
      // if we overflow buffer, skip descriptor
      continue;
    // fetch config descriptor
    err = usb_GetConfigurationDescriptor(device, config, &descriptor.conf, desc_len, &xferd);
    if (err || (xferd != desc_len))
      // if error or not full descriptor, skip
      continue;

    // set pointer to current working descriptor
    usb_descriptor_t *desc = &descriptor.desc;
    while (parsed_len < desc_len)
    {
      switch (desc->bDescriptorType)
      {
      case USB_ENDPOINT_DESCRIPTOR:
      {
        // we should only look for this IF we have found the ECM control interface,
        // and have retrieved the bulk data interface number from the CS_INTERFACE descriptor
        // see case USB_CS_INTERFACE_DESCRIPTOR and case USB_INTERFACE_DESCRIPTOR
        usb_endpoint_descriptor_t *endpoint = (usb_endpoint_descriptor_t *)desc;
        if (parse_state & PARSE_HAS_BULK_IF)
        {
          if (endpoint->bEndpointAddress & (USB_DEVICE_TO_HOST))
          {
            endpoint_addr.in = endpoint->bEndpointAddress; // set out endpoint address
            parse_state |= PARSE_HAS_ENDPOINT_IN;
          }
          else
          {
            endpoint_addr.out = endpoint->bEndpointAddress; // set in endpoint address
            parse_state |= PARSE_HAS_ENDPOINT_OUT;
          }
          if ((parse_state & PARSE_HAS_ENDPOINT_IN) &&
              (parse_state & PARSE_HAS_ENDPOINT_OUT) &&
              (parse_state & PARSE_HAS_ENDPOINT_INT) &&
              (parse_state & PARSE_HAS_MAC_ADDR))
            goto init_success; // if we have both, we are done -- hard exit
        }
        else if (parse_state & PARSE_HAS_CONTROL_IF)
        {
          endpoint_addr.interrupt = endpoint->bEndpointAddress;
          parse_state |= PARSE_HAS_ENDPOINT_INT;
        }
      }
      break;
      case USB_INTERFACE_DESCRIPTOR:
        // we should look for this to either:
        // (1) find the CDC Control Class/ECM interface, or
        // (2) find the Interface number that matches the Interface indicated by the USB_CS_INTERFACE descriptor
        {
          // cast to struct of type interface descriptor
          usb_interface_descriptor_t *iface = (usb_interface_descriptor_t *)desc;
          // if we have a control interface and ifnum is non-zero (we have an interface num to look for)
          if (parse_state & PARSE_HAS_BULK_IF_NUM)
          {
            if ((iface->bInterfaceNumber == ifnum) &&
                (iface->bNumEndpoints == 2) &&
                (iface->bInterfaceClass == USB_CDC_DATA_CLASS))
            {
              if_bulk.addr = iface;
              if_bulk.len = desc_len - parsed_len;
              parse_state |= PARSE_HAS_BULK_IF;
            }
          }
          else
          {
            // if we encounter another interface type after a control interface that isn't the CS_INTERFACE
            // then we don't have the correct interface. This could be a malformed descriptor or something else
            parse_state = 0; // reset parser state

            // If the interface is class CDC control and subtype ECM, this might be the correct interface union
            // the next thing we should encounter is see case USB_CS_INTERFACE_DESCRIPTOR
            if ((iface->bInterfaceClass == USB_COMM_CLASS) &&
                ((iface->bInterfaceSubClass == USB_ECM_SUBCLASS) ||
                 (iface->bInterfaceSubClass == USB_NCM_SUBCLASS)))
            {
              // use this to set configuration
              configdata.addr = &descriptor.conf;
              configdata.len = desc_len;
              tmp.type = iface->bInterfaceSubClass;
              if (usb_SetConfiguration(device, configdata.addr, configdata.len))
                return false;
              parse_state |= PARSE_HAS_CONTROL_IF;
            }
          }
        }
        break;
      case USB_CS_INTERFACE_DESCRIPTOR:
        // this is a class-specific descriptor that specifies the interfaces used by the control interface
        {
          usb_cs_interface_descriptor_t *cs = (usb_cs_interface_descriptor_t *)desc;
          switch (cs->bDescriptorSubType)
          {
          case USB_ETHERNET_FUNCTIONAL_DESCRIPTOR:
          {
            usb_ethernet_functional_descriptor_t *ethdesc = (usb_ethernet_functional_descriptor_t *)cs;
            usb_control_setup_t get_mac_addr = {0b10100001, REQUEST_GET_NET_ADDRESS, 0, 0, 6};
            size_t xferd_tmp;
            uint8_t string_descriptor_buf[DESCRIPTOR_MAX_LEN];
            usb_string_descriptor_t *macaddr = (usb_string_descriptor_t *)string_descriptor_buf;
            // Get MAC address and save for lwIP use (ETHARP header)
            // if index for iMacAddress valid and GetStringDescriptor success, save MAC address
            // else attempt control transfer to get mac address and save it
            // else generate random compliant local MAC address and send control request to set the hwaddr, then save it
            if (ethdesc->iMacAddress &&
                (!usb_GetStringDescriptor(device, ethdesc->iMacAddress, 0, macaddr, DESCRIPTOR_MAX_LEN, &xferd_tmp)))
            {
              for (uint24_t i = 0; i < NETIF_MAX_HWADDR_LEN; i++)
                tmp.hwaddr[i] = (nibble(macaddr->bString[2 * i]) << 4) | nibble(macaddr->bString[2 * i + 1]);

              parse_state |= PARSE_HAS_MAC_ADDR;
            }
            else if (!usb_DefaultControlTransfer(device, &get_mac_addr, &tmp.hwaddr[0], NCM_USB_MAXRETRIES, &xferd_tmp))
            {
              parse_state |= PARSE_HAS_MAC_ADDR;
            }
            else
            {
#define RMAC_RANDOM_MAX 0xFFFFFF
              usb_control_setup_t set_mac_addr = {0b00100001, REQUEST_SET_NET_ADDRESS, 0, 0, 6};
              uint24_t rmac[2];
              rmac[0] = (uint24_t)(random() & RMAC_RANDOM_MAX);
              rmac[1] = (uint24_t)(random() & RMAC_RANDOM_MAX);
              memcpy(&tmp.hwaddr[0], rmac, 6);
              tmp.hwaddr[0] &= 0xFE;
              tmp.hwaddr[0] |= 0x02;
              if (!usb_DefaultControlTransfer(eth->device, &set_mac_addr, &tmp.hwaddr[0], NCM_USB_MAXRETRIES, &xferd_tmp))
                parse_state |= PARSE_HAS_MAC_ADDR;
            }
          }
          break;
          case USB_UNION_FUNCTIONAL_DESCRIPTOR:
          {
            // if union functional type, this contains interface number for bulk transfer
            usb_union_functional_descriptor_t *func = (usb_union_functional_descriptor_t *)cs;
            ifnum = func->bInterface;
            parse_state |= PARSE_HAS_BULK_IF_NUM;
          }
          break;
          case USB_NCM_FUNCTIONAL_DESCRIPTOR:
          {
            usb_ncm_functional_descriptor_t *ncm = (usb_ncm_functional_descriptor_t *)cs;
            tmp.class.ncm.bm_capabilities = ncm->bmNetworkCapabilities;
          }
          break;
          }
        }
      }
      parsed_len += desc->bLength;
      desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
    }
  }
  return false;
init_success:
  if (tmp.type == USB_NCM_SUBCLASS)
  {
    if (ncm_control_setup(&tmp))
      return false;
    tmp.process = ncm_process;
    tmp.emit = ncm_bulk_transmit;
  }
  else if (tmp.type == USB_ECM_SUBCLASS)
  {
    tmp.process = ecm_process;
    tmp.emit = ecm_bulk_transmit;
  }
  if (usb_SetInterface(device, if_bulk.addr, if_bulk.len))
    return false;
  tmp.endpoint.in = usb_GetDeviceEndpoint(device, endpoint_addr.in);
  tmp.endpoint.out = usb_GetDeviceEndpoint(device, endpoint_addr.out);
  tmp.endpoint.interrupt = usb_GetDeviceEndpoint(device, endpoint_addr.interrupt);

  eth = malloc(sizeof(eth_device_t));
  if (eth == NULL)
    return false;
  memcpy(eth, &tmp, sizeof(eth_device_t));

  struct netif *iface = &eth->iface;
  if (netif_add_noaddr(iface, eth, eth_netif_init, netif_input) == NULL)
    return false;
  usb_SetDeviceData(device, eth);
  iface->name[0] = 'e';
  iface->name[1] = 'n';
  static uint8_t ifnum = 0;
  iface->num = ifnum++;
  netif_create_ip6_linklocal_address(iface, 1);
  iface->ip6_autoconfig_enabled = 1;
  netif_set_default(iface);
  netif_set_up(iface);
  usb_ScheduleInterruptTransfer(eth->endpoint.interrupt, interrupt_buf, 64, interrupt_receive_callback, eth);
  usb_ScheduleBulkTransfer(eth->endpoint.in, eth_rx_buf, ETHERNET_MTU, bulk_receive_callback, eth);
  netif_set_link_up(iface);
  netif_set_hostname(iface, hostname);
  return true;
}

usb_error_t
eth_handle_usb_event(usb_event_t event, void *event_data,
                     usb_callback_data_t *callback_data)
{
  usb_device_t usb_device = event_data;
  /* Enable newly connected devices */
  switch (event)
  {
  case USB_DEVICE_CONNECTED_EVENT:
    if (!(usb_GetRole() & USB_ROLE_DEVICE))
      usb_ResetDevice(usb_device);
    break;
  case USB_DEVICE_ENABLED_EVENT:
  {
    if (!init_ethernet_usb_device(usb_device))
    {
      printf("no supported configurations\n");
      break;
    }
  }
  break;
  case USB_DEVICE_DISCONNECTED_EVENT:
  case USB_DEVICE_DISABLED_EVENT:
  {
    eth_device_t *eth_device = (eth_device_t *)usb_GetDeviceData(usb_device);
    netif_set_link_down(&eth_device->iface);
    netif_set_down(&eth_device->iface);
    netif_remove(&eth_device->iface);
    free(eth_device);
  }
  break;
  case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
    return USB_ERROR_NO_DEVICE;
    break;
  }
  return USB_SUCCESS;
}
