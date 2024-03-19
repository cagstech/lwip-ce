#include <usbdrvce.h>
#include <cryptx.h>
#include <sys/util.h>
#include "include/lwip/netif.h"
#include "lwip/ethip6.h"
#include "lwip/etharp.h"
#include "lwip/stats.h"
#include "lwip/snmp.h"
#include "lwip/dhcp.h"
#include "lwip/dns.h"

#include "drivers.h"

/** CLASS-SPECIFIC DESCRIPTOR DEFINITIONS */
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

/** GLOBAL/BUFFERS DECLARATIONS */
eth_device_t eth = {0};
struct netif eth_netif = {0};
const char *hostname = "ti84pce";
uint8_t interrupt_buf[64];
uint8_t eth_rx_buf[ETHERNET_MTU];
uint8_t ifnum = 0;

/** UTF-16 -> HEX CONVERSION */
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

/** ETHERNET CALLBACK FUNCTIONS */
void eth_link_callback(struct netif *netif)
{
  if (netif_is_link_up(netif) && (!dhcp_supplied_address(netif)))
    dhcp_start(netif);
}

// bulk RX callback (general)
usb_error_t bulk_receive_callback(usb_endpoint_t endpoint,
                                  usb_transfer_status_t status,
                                  size_t transferred,
                                  usb_transfer_data_t *data)
{
  if ((status == USB_TRANSFER_COMPLETED) && transferred)
  {
    struct pbuf *p = pbuf_alloc(PBUF_RAW, transferred, PBUF_POOL);
    if (p != NULL)
    {
      LINK_STATS_INC(link.recv);
      MIB2_STATS_NETIF_ADD(&eth_netif, ifinoctets, transferred);
      pbuf_take(p, eth_rx_buf, transferred);
      eth.process(p);
    }
    usb_ScheduleBulkTransfer(eth.endpoint.in, eth_rx_buf, ETHERNET_MTU, bulk_receive_callback, NULL);
  }
  return USB_SUCCESS;
}

// interrupt rx callback
#define INTERRUPT_REQUEST_TYPE 0b10100001
#define INTERRUPT_NOTIFY_NETWORK_CONNECTION 0
usb_error_t interrupt_receive_callback(usb_endpoint_t endpoint,
                                       usb_transfer_status_t status,
                                       size_t transferred,
                                       usb_transfer_data_t *data)
{
  usb_control_setup_t *ctl = (usb_control_setup_t *)interrupt_buf;
  usb_control_setup_t *nc = (usb_control_setup_t *)(((uint8_t *)ctl) + ctl->wLength + sizeof(usb_control_setup_t));
  if (nc->bmRequestType == INTERRUPT_REQUEST_TYPE && nc->bRequest == INTERRUPT_NOTIFY_NETWORK_CONNECTION)
  {
    if (nc->wValue)
    {
      netif_set_up(&eth_netif);
      netif_set_link_up(&eth_netif);
    }
    else
    {
      netif_set_link_down(&eth_netif);
      netif_set_down(&eth_netif);
    }
  }

  usb_ScheduleInterruptTransfer(eth.endpoint.interrupt, interrupt_buf, 64, interrupt_receive_callback, NULL);
  return USB_SUCCESS;
}

// bulk tx callback (this should work for both since it's just a free)
usb_error_t bulk_transmit_callback(usb_endpoint_t endpoint,
                                   usb_transfer_status_t status,
                                   size_t transferred,
                                   usb_transfer_data_t *data)
{
  // Handle completion or error of the transfer, if needed
  pbuf_free(data);
  return USB_SUCCESS;
}

// ################ NCM DRIVER ################
// Defines NCM headers and NDP structure definitions
// Defines NCM RX/TX functions

// NCM Transfer Header Definition
struct ncm_nth
{
  uint8_t dwSignature[4]; // "NCMH"
  uint16_t wHeaderLength; // size of this header structure
  uint16_t wSequence;     // counter for NTB's sent
  uint16_t wBlockLength;  // size of the NTB
  uint16_t wNdpIndex;     // offset to first NDP
};

// NCM NDP Definition
struct ncm_ndp
{
  uint8_t dwSignature[4]; // "NCM0"
  uint16_t wLength;       // size of NDP16
  uint16_t wNextNdpIndex; // offset to next NDP16
};

// NCM Datagram indexes                     // should point to first NDP
struct ncm_ndp_idx
{
  uint16_t wDatagramIndex; // offset of datagram, if 0, then is end of datagram list
  uint16_t wDatagramLen;   // length of datagram, if 0, then is end of datagram list
};

struct ncm_nth ncm_nth_default = {
    {'N', 'C', 'M', 'H'}, // this should never be edited
    12,                   // this is the length of the header (always 12?)
    0,                    // increment by 1 per transfer?
    0,                    // length of transfer
    12};
// Define base NDP
struct ncm_ndp ncm_ndp_default = {
    {'N', 'C', 'M', '0'}, // this should never be edited
    0,                    // this is the size of the NDP16
    0};                   // this is the index (offset) of next NDP16

// Parse NCM data structures for packets
void ncm_process(struct pbuf *p)
{
  struct ncm_nth *nth = (struct ncm_nth *)p->payload;

  // validate header signature
  if (memcmp(nth->dwSignature, "NCMH", 4))
    return;
  // get ptr to first ndp
  struct ncm_ndp *ndp = (struct ncm_ndp *)(p->payload + nth->wNdpIndex);
  do
  {
    if (memcmp(ndp->dwSignature, "NCM0", 4))
      continue; // skip malformed NDP??
    uint16_t idx = -1;
    do
    {
      // allocate new pbuf for the datagram
      idx++;
      struct pbuf *tbuf = pbuf_alloc(PBUF_RAW, ndp->wDatagrams[idx].wDatagramLen, PBUF_REF);
      if (tbuf == NULL)
        return;
      // copy datagram to new pbuf
      if (pbuf_take(tbuf, p->payload + ndp->wDatagrams[idx].wDatagramIndex, ndp->wDatagrams[idx].wDatagramLen))
        continue;

      // if handoff error, free pbuf
      if (eth_netif.input(tbuf, &eth_netif) != ERR_OK)
        pbuf_free(tbuf);
    } while (ndp->wDatagrams[idx].wDatagramIndex && ndp->wDatagrams[idx].wDatagramLen == 0);
    ndp = (struct ncm_ndp *)(p->payload + ndp->wNextNdpIndex);
  } while (ndp->wNextNdpIndex);
  pbuf_free(p); // free coalesced pbuf
}

// Chain packets into NCM data structures until full or timeout
#define NCM_MAX_TX_DATAGRAMS 5
#define NCM_NTH_LEN sizeof(struct ncm_nth)
#define NCM_NDP_LEN sizeof(struct ncm_ndp)
#define NCM_NDP_IDX_LEN (6 * sizeof(struct ncm_ndp_idx))
#define NCM_METADATA_LEN (NCM_NTH_LEN + NCM_NDP_LEN + NCM_NDP_IDX_LEN)
#define NCM_TIMEO_MS (1000 / 20)
#define NCM_TX_MAXRETRIES 3

err_t ncm_emit_tx(uint8_t *buf, size_t *len)
{
  // cast to pointers to NTH/NDP (for in-place editing)
  struct ncm_nth *nth = (struct ncm_nth *)buf;
  // struct ncm_ndp *ndp = (struct ncm_ndp *)(buf + NCM_NTH_LEN);

  // update NTH/NDP header info for TX
  nth->wBlockLength = NCM_METADATA_LEN + (*len); // length of entire USB TX
  // nth->wNdpIndex = NCM_NTH_LEN;                  // offset to start of first NDP (should be in defaults)

  if (usb_BulkTransfer(eth.endpoint.out, buf, len, NCM_TX_MAXRETRIES, &sent))
    return ERR_IF;
  if (sent != len)
    return ERR_IF;
  *len = 0;
  nth->wSequence++;
  memset(buf + NCM_NTH_LEN + NCM_NDP_LEN, 0, ETHERNET_MTU - NCM_NDP_LEN - NCM_NTH_LEN);
  return ERR_OK;
}

err_t ncm_bulk_transmit(struct netif *netif, struct pbuf *p)
{

  // Declare NCM defaults
  static uint8_t tx_buf[ETHERNET_MTU] = {0};                                          // static TX buffer
  static size_t bytes_written = 0;                                                    // bytes written to TX buffer
  static uint8_t datagrams_written = 0;                                               // datagrams enqueued
  static uint32_t clock_last_tx = = sys_now();                                        // time at last TX (or time at first enqueue)
  memcpy(tx_buf, ncm_nth_default, sizeof(ncm_nth_default));                           // copy nth defaults to TX buffer
  memcpy(tx_buf + sizeof(ncm_nth_default), ncm_ndp_default, sizeof(ncm_ndp_default)); // copy ndp defaults to TX buffer
  // cast to pointer to NDP_IDX (for in place editing)
  struct ncm_ndp_idx *idx = (struct ncm_ndp_idx *)(tx_buf + NCM_NTH_LEN + NCM_NDP_LEN);
  uint8_t *tx_data = tx_buf + NCM_METADATA_LEN;
  usb_error_t err;

  // if this packet would overflow the TX data buffer, send TX first
  if ((NCM_METADATA_LEN + bytes_written + p->tot_len) > ETHERNET_MTU)
  {
    if (ncm_emit_tx(tx_buf, &bytes_written))
      return ERR_IF;
    datagrams_written = 0;
    clock_last_tx = = sys_now();
  }

  // update datagrams idx
  idx[datagrams_written].wDatagramIndex = NCM_METADATA_LEN + bytes_written;
  idx[datagrams_written].wDatagramLen = p->tot_len;

  // copy datagram to buffer
  if (pbuf_copy_partial(p, tx_data + bytes_written, p->tot_len, 0))
  {
    datagrams_written++; // increment datagrams written count
    bytes_written += p->tot_len;
    pbuf_free(p); // should i do this?
    LINK_STATS_INC(link.xmit);
    // Update SNMP stats(only if you use SNMP)
    MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
  }

  // if timeout hit or packet queue full, send TX
  if (((sys_now() - clock_last_tx) >= NCM_TIMEO_MS) ||
      (datagrams_written >= NCM_MAX_TX_DATAGRAMS))
  {
    if (ncm_emit_tx(tx_buf, &bytes_written))
      return ERR_IF;
    datagrams_written = 0;
    clock_last_tx = = sys_now();
  }
  return ERR_OK;
}

// ################ ECM DRIVER ################
// Defines ECM RX/TX functions

// process ECM frame
void ecm_process(struct pbuf *p)
{
  if (eth_netif.input(p, &eth_netif) != ERR_OK)
    pbuf_free(p);
}

// bulk tx ECM
err_t ecm_bulk_transmit(struct netif *netif, struct pbuf *p)
{
  if (p->tot_len > ETHERNET_MTU)
  {
    printf("pbuf payload exceeds mtu\n");
    return ERR_MEM;
  }
  struct pbuf *tbuf = pbuf_alloc(PBUF_RAW, p->tot_len, PBUF_RAM);
  LINK_STATS_INC(link.xmit);
  // Update SNMP stats(only if you use SNMP)
  MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);
  if (pbuf_copy(tbuf, p))
    return ERR_MEM;
  if (usb_ScheduleBulkTransfer(eth.endpoint.out, tbuf->payload, p->tot_len, bulk_transmit_callback, tbuf) == USB_SUCCESS)
    return ERR_OK;
  else
    return ERR_IF;
}

err_t eth_netif_init(struct netif *netif)
{
  netif->linkoutput = eth.emit;
  netif->output = etharp_output;
  netif->output_ip6 = ethip6_output;
  netif->mtu = ETHERNET_MTU;
  netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;
  MIB2_INIT_NETIF(netif, snmp_ifType_ethernet_csmacd, 100000000);
  memcpy(netif->hwaddr, eth.hwaddr, NETIF_MAX_HWADDR_LEN);
  netif->hwaddr_len = NETIF_MAX_HWADDR_LEN;
  netif->name[0] = 'e';
  netif->name[1] = 'n';
  netif->num = ifnum++;
  netif_create_ip6_linklocal_address(netif, 1);
  netif_set_hostname(netif, hostname);
  netif->ip6_autoconfig_enabled = 1;
  netif_set_link_callback(netif, eth_link_callback);
  netif_set_default(netif);
  return ERR_OK;
}

#define DESCRIPTOR_MAX_LEN 256
bool init_ethernet_usb_device(uint8_t device_class)
{
  size_t xferd, parsed_len, desc_len;
  usb_error_t err;
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
  err = usb_GetDeviceDescriptor(eth.device, &descriptor.dev, sizeof(usb_device_descriptor_t), &xferd);
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
    desc_len = usb_GetConfigurationDescriptorTotalLength(eth.device, config);
    parsed_len = 0;
    if (desc_len > 256)
      // if we overflow buffer, skip descriptor
      continue;
    // fetch config descriptor
    err = usb_GetConfigurationDescriptor(eth.device, config, &descriptor.conf, desc_len, &xferd);
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
                (iface->bInterfaceSubClass == device_class))
            {
              // use this to set configuration
              configdata.addr = &descriptor.conf;
              configdata.len = desc_len;
              if (usb_SetConfiguration(eth.device, configdata.addr, configdata.len))
                return USB_ERROR_FAILED;
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
            size_t xferd_tmp;
            uint8_t string_descriptor_buf[DESCRIPTOR_MAX_LEN];
            usb_string_descriptor_t *macaddr = (usb_string_descriptor_t *)string_descriptor_buf;
            if (ethdesc->iMacAddress &&
                (!usb_GetStringDescriptor(eth.device, ethdesc->iMacAddress, 0, macaddr, DESCRIPTOR_MAX_LEN, &xferd_tmp)))
            {
              // if mac address index is valid (non-zero), return string descriptor
              // string is a UTF-16 nibble-encoded mac address
              for (int i = 0; i < NETIF_MAX_HWADDR_LEN; i++)
                eth.hwaddr[i] = (nibble(macaddr->bString[2 * i]) << 4) | nibble(macaddr->bString[2 * i + 1]);

              parse_state |= PARSE_HAS_MAC_ADDR;
            }
            else
            {
              // generate random MAC addr
              uint32_t rmac[2];
              rmac[0] = random();
              rmac[1] = random();
              memcpy(eth.hwaddr, &rmac, 6);
              eth.hwaddr[0] &= 0b11110000;
              eth.hwaddr[0] |= 0b10;
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
          }
        }
      }
      parsed_len += desc->bLength;
      desc = (usb_descriptor_t *)(((uint8_t *)desc) + desc->bLength);
    }
  }
  return false;
init_success:
  // if (usb_SetConfiguration(eth.device, configdata.addr, configdata.len))
  //   return USB_ERROR_FAILED;
  if (usb_SetInterface(eth.device, if_bulk.addr, if_bulk.len))
    return USB_ERROR_FAILED;
  eth.endpoint.in = usb_GetDeviceEndpoint(eth.device, endpoint_addr.in);
  eth.endpoint.out = usb_GetDeviceEndpoint(eth.device, endpoint_addr.out);
  eth.endpoint.interrupt = usb_GetDeviceEndpoint(eth.device, endpoint_addr.interrupt);
  eth.type = device_class;
  usb_RefDevice(eth.device);
  return true;
}

usb_error_t
eth_handle_usb_event(usb_event_t event, void *event_data,
                     usb_callback_data_t *callback_data)
{

  /* Enable newly connected devices */
  switch (event)
  {
  case USB_DEVICE_CONNECTED_EVENT:
    if (!(usb_GetRole() & USB_ROLE_DEVICE))
    {
      usb_device_t usb_device = event_data;
      usb_ResetDevice(usb_device);
    }
    break;
  case USB_DEVICE_ENABLED_EVENT:
    eth.device = event_data;
    if (init_ethernet_usb_device(USB_NCM_SUBCLASS))
    {
      eth.process = ncm_process;
      eth.emit = ncm_bulk_transmit;
    }
    else if (init_ethernet_usb_device(USB_ECM_SUBCLASS))
    {
      eth.process = ecm_process;
      eth.emit = ecm_bulk_transmit;
    }
    else
    {
      printf("no supported configurations\n");
      break;
    }
    netif_add_noaddr(&eth_netif, NULL, eth_netif_init, netif_input);
    netif_set_default(&eth_netif);
    netif_set_up(&eth_netif);
    usb_ScheduleBulkTransfer(eth.endpoint.in, eth_rx_buf, ETHERNET_MTU, bulk_receive_callback, NULL);
    usb_ScheduleInterruptTransfer(eth.endpoint.interrupt, interrupt_buf, 64, interrupt_receive_callback, NULL);
    break;

  case USB_DEVICE_DISABLED_EVENT:
    netif_remove(&eth_netif);
    break;
  case USB_DEVICE_DISCONNECTED_EVENT:
  case USB_HOST_PORT_CONNECT_STATUS_CHANGE_INTERRUPT:
    return USB_ERROR_NO_DEVICE;
    break;
  }
  return USB_SUCCESS;
}
