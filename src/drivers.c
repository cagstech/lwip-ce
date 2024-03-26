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
#include "include/lwip/timeouts.h"

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

typedef struct
{
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint16_t bcdNcmVersion;
  uint8_t bmNetworkCapabilities;
} usb_ncm_functional_descriptor_t;

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
    LINK_STATS_INC(link.recv);
    MIB2_STATS_NETIF_ADD(&eth_netif, ifinoctets, transferred);
    eth.process(eth_rx_buf, transferred);
  }
  else
    printf("usb transfer status code: %u\n", status);
  usb_ScheduleBulkTransfer(eth.endpoint.in, eth_rx_buf, ETHERNET_MTU, bulk_receive_callback, NULL);
  return USB_SUCCESS;
}

/* CDC Comm Class Revision 1.2 */
#define BM_REQUEST_TYPE_TO_HOST (USB_DEVICE_TO_HOST | USB_CONTROL_TRANSFER | USB_RECIPIENT_INTERFACE)   // 0b10100001
#define BM_REQUEST_TYPE_TO_DEVICE (USB_HOST_TO_DEVICE | USB_CONTROL_TRANSFER | USB_RECIPIENT_INTERFACE) // 0b00100001

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

usb_error_t
interrupt_receive_callback(usb_endpoint_t endpoint,
                           usb_transfer_status_t status,
                           size_t transferred,
                           usb_transfer_data_t *data)
{
  usb_control_setup_t *notify;
  size_t bytes_parsed = 0;
  do
  {
    notify = (usb_control_setup_t *)&interrupt_buf[bytes_parsed];
    if (notify->bmRequestType == BM_REQUEST_TYPE_TO_HOST)
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
  if (data)
    pbuf_free(data);
  return USB_SUCCESS;
}

/***********************************************************
 * USB CDC (Communications Data Class)
 * NCM (Network Control Model) Driver
 * Defines RX/TX functions for CDC-NCM on a TI-84+ CE
 */
#define NCM_USB_MAXRETRIES 3

/* NTB Parameters (Returned from Control Request) */
struct ntb_params
{
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
};
struct ntb_params ntb_info; /* allocate one */

/* NCM Transfer Header (NTH) Defintion */
#define NCM_NTH_SIG 0x484D434E
struct ncm_nth
{
  uint32_t dwSignature;   // "NCMH"
  uint16_t wHeaderLength; // size of this header structure (should be 12 for NTB-16)
  uint16_t wSequence;     // counter for NTB's sent
  uint16_t wBlockLength;  // size of the NTB
  uint16_t wNdpIndex;     // offset to first NDP
};

/* NCM Datagram Pointers (NDP) Definition*/
struct ncm_ndp_idx
{
  uint16_t wDatagramIndex; // offset of datagram, if 0, then is end of datagram list
  uint16_t wDatagramLen;   // length of datagram, if 0, then is end of datagram list
};
#define NCM_NDP_SIG0 0x304D434E
#define NCM_NDP_SIG1 0x314D434E
struct ncm_ndp
{
  uint32_t dwSignature;              // "NCM0"
  uint16_t wLength;                  // size of NDP16
  uint16_t wNextNdpIndex;            // offset to next NDP16
  struct ncm_ndp_idx wDatagramIdx[]; // pointer to end of NDP
};

#define NCM_NTH_LEN sizeof(struct ncm_nth)
#define NCM_NDP_LEN sizeof(struct ncm_ndp)
#define NCM_BUF_MAX_SIZE 2048

/*-----------------------------------------------
 * NCM RX
 */
void ncm_process(uint8_t *buf, size_t len)
{
  // allocate 2048 bytes for NCM RX buffer
  static uint8_t rx_buf[NCM_BUF_MAX_SIZE] = {0};

  // set if we need to wait for the next frame before processing the NTB
  // since NTB max is set to 2048, we should need only one more frame
  static bool frame_await = false;
  // place to write RX into NCM buffer
  static size_t rx_offset = 0;

  // copy buf into rx_buf
  memcpy(&rx_buf[rx_offset], buf, len);
  rx_offset += len;

  // cast start of buffer to NTH pointer
  struct ncm_nth *nth = (struct ncm_nth *)&rx_buf[0];
  if (nth->wBlockLength > rx_offset)
  {                     // if block length is greater than collected transfer length,
    frame_await = true; // set await flag
    return;             // then return
  }
  // verify that NTH sig is valid
  if (nth->dwSignature != NCM_NTH_SIG)
    goto exit;

  // cast rx_buf + nth->wNdpIndex (offset to first NDP) to NDP pointer
  struct ncm_ndp *ndp = (struct ncm_ndp *)&rx_buf[nth->wNdpIndex];
  // repeat while ndp->wNextNdpIndex is non-zero
  do
  {
    if (ndp->dwSignature != NCM_NDP_SIG0) // if invalid sig
      goto exit;                          // error ?

    // set datagram number to 0 and set datagram index pointer
    uint16_t dg_num = 0;
    struct ncm_ndp_idx *idx = (struct ncm_ndp_idx *)ndp->wDatagramIdx;

    // a null datagram index structure indicates end of NDP
    do
    {
      // attempt to allocate pbuf
      struct pbuf *p = pbuf_alloc(PBUF_RAW, idx[dg_num].wDatagramLen, PBUF_POOL);
      if (p != NULL)
      {
        // copy datagram into pbuf
        pbuf_take(p, &rx_buf[idx[dg_num].wDatagramIndex], idx[dg_num].wDatagramLen);
        // hand pbuf to lwIP (should we enqueue these and defer the handoffs till later?)
        // i'll leave it for now, and if my calculator explodes I'll fix it
        if (eth_netif.input(p, &eth_netif) != ERR_OK)
          pbuf_free(p);
      }
      dg_num++;
    } while ((idx[dg_num].wDatagramIndex) && (idx[dg_num].wDatagramLen));
    // if next NDP is 0, NTB is done and so is my sanity
    if (ndp->wNextNdpIndex == 0)
      break;
    // set next NDP
    ndp = (struct ncm_ndp *)&rx_buf[ndp->wNextNdpIndex];
  } while (1);
exit:
  // delete the shadow of my own regret and prepare for more regret
  frame_await = false;
  rx_offset = 0;
  memset(rx_buf, 0, NCM_BUF_MAX_SIZE);
}

/*-----------------------------------------------
 * NCM TX
 * send a single datagram in a single frame just like ECM
 * the calc is too slow for it to make a difference
 */
#define NCM_HBUF_SIZE 64
#define get_next_offset(offset, divisor, remainder) \
  (offset) + (divisor) - ((offset) % (divisor)) + (remainder)

err_t ncm_bulk_transmit(struct netif *netif, struct pbuf *p)
{
  static uint16_t sequence = 0; /* set sequence counter */
  uint16_t offset_ndp = get_next_offset(NCM_NTH_LEN, ntb_info.wNdpInAlignment, 0);
  if (p->tot_len > ETHERNET_MTU)
  {
    printf("pbuf payload exceeds mtu\n");
    return ERR_MEM;
  }

  // allocate TX packet buffer
  struct pbuf *obuf = pbuf_alloc(PBUF_RAW, ETHERNET_MTU + NCM_HBUF_SIZE, PBUF_RAM);
  if (obuf == NULL)
    return ERR_MEM;

  // declare NTH, NDP, and NDP_IDX structures
  uint8_t hdr_buf[NCM_HBUF_SIZE];
  struct ncm_nth *nth = (struct ncm_nth *)hdr_buf;
  struct ncm_ndp *ndp = (struct ncm_ndp *)&hdr_buf[offset_ndp];
  struct ncm_ndp_idx *idx = (struct ncm_ndp_idx *)ndp->wDatagramIdx;

  // populate structs
  nth->dwSignature = NCM_NTH_SIG;
  nth->wHeaderLength = NCM_NTH_LEN;
  nth->wSequence = sequence;
  nth->wBlockLength = NCM_HBUF_SIZE + ETHERNET_MTU;
  nth->wNdpIndex = offset_ndp;

  ndp->dwSignature = NCM_NDP_SIG0;
  ndp->wLength = (ETHERNET_MTU + NCM_HBUF_SIZE) - offset_ndp;
  ndp->wNextNdpIndex = 0;

  idx[0].wDatagramIndex = NCM_HBUF_SIZE + ntb_info.wNdpInPayloadRemainder;
  idx[0].wDatagramLen = p->tot_len;
  idx[1].wDatagramIndex = 0;
  idx[1].wDatagramLen = 0;

  // absorb the populated structs into the obuf
  pbuf_take(obuf, hdr_buf, NCM_HBUF_SIZE);

  // copy the datagram to the pbuf
  pbuf_take_at(obuf, p->payload, p->tot_len, NCM_HBUF_SIZE + ntb_info.wNdpInPayloadRemainder);

  // queue the TX
  if (usb_ScheduleBulkTransfer(eth.endpoint.out, obuf->payload, ETHERNET_MTU + NCM_HBUF_SIZE, bulk_transmit_callback, obuf))
    return ERR_IF;

  // increment sequence, decrement my will to live
  sequence++;
  return ERR_OK;
}

/***********************************************************
 * USB CDC (Communications Data Class)
 * ECM (Ethernet Control Model) Driver
 * Defines RX/TX functions for CDC-ECM on a TI-84+ CE
 */

// process ECM frame
void ecm_process(uint8_t *buf, size_t len)
{
  struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  if (p != NULL)
  {
    pbuf_take(p, buf, len);
    if (eth_netif.input(p, &eth_netif) != ERR_OK)
      pbuf_free(p);
  }
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

/***********************************************************
 * lwIP NETIF initialization functions for CDC Ethernet
 */
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
            usb_control_setup_t get_mac_addr = {0b10100001, REQUEST_GET_NET_ADDRESS, 0, 0, 6};
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
            else if (!usb_DefaultControlTransfer(eth.device, &get_mac_addr, eth.hwaddr, NCM_USB_MAXRETRIES, &xferd_tmp))
              parse_state |= PARSE_HAS_MAC_ADDR;
            else
            {
              // generate random MAC addr
#define RMAC_RANDOM_MAX 0xFFFFFF
              usb_control_setup_t set_mac_addr = {0b00100001, REQUEST_SET_NET_ADDRESS, 0, 0, 6};
              uint24_t rmac[2];
              rmac[0] = (uint24_t)(random() & RMAC_RANDOM_MAX);
              rmac[1] = (uint24_t)(random() & RMAC_RANDOM_MAX);
              memcpy(eth.hwaddr, rmac, 6);
              eth.hwaddr[0] &= 0xFE;
              eth.hwaddr[0] |= 0x02;
              // inform function of randomly generated mac address (this may be optional but best practice?)
              if (!usb_DefaultControlTransfer(eth.device, &set_mac_addr, eth.hwaddr, NCM_USB_MAXRETRIES, &xferd_tmp))
                parse_state |= PARSE_HAS_MAC_ADDR;
            }
          }
            // this is there for testing purposes, we'll remove in production release
            for (int i = 0; i < NETIF_MAX_HWADDR_LEN; i++)
              printf("%02X ", eth.hwaddr[i]);
            printf("\n");
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
            eth.bm_capabilities = ncm->bmNetworkCapabilities;
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
  if (usb_SetInterface(eth.device, if_bulk.addr, if_bulk.len))
    return USB_ERROR_FAILED;
  eth.endpoint.in = usb_GetDeviceEndpoint(eth.device, endpoint_addr.in);
  eth.endpoint.out = usb_GetDeviceEndpoint(eth.device, endpoint_addr.out);
  eth.endpoint.interrupt = usb_GetDeviceEndpoint(eth.device, endpoint_addr.interrupt);
  eth.type = device_class;
  usb_RefDevice(eth.device);
  return true;
}

// NCM control setup packets/data for device configuration
usb_control_setup_t get_ntb_params = {0b10100001, REQUEST_GET_NTB_PARAMETERS, 0, 0, 0x1c};
usb_control_setup_t ntb_config_request = {0b00100001, REQUEST_SET_NTB_INPUT_SIZE, 0, 0, 4};
uint32_t ntb_in_new_size = 2048;
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

      // unlike ECM, NCM requires some initialization work
      size_t transferred;
      usb_error_t err = 0;

      /* Query NTB Parameters for device (NCM devices) */
      if ((err = usb_DefaultControlTransfer(eth.device, &get_ntb_params, &ntb_info, NCM_USB_MAXRETRIES, &transferred)))
        printf("error getting ntb params.\n");

      /* Set NTB Max Input Size to 2048 (recd minimum NCM spec v 1.2) */
      if ((err = usb_DefaultControlTransfer(eth.device, &ntb_config_request, &ntb_in_new_size, NCM_USB_MAXRETRIES, &transferred)))
        printf("error setting ntb size.\n");

      // if an error, break out without configuring netif
      if (err)
        break;
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
