
#ifndef ncm_h
#define ncm_h

#include <stdint.h>
#include <usbdrvce.h>

#include "cdc.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

#define USB_NCM_SUBCLASS 0x0D
#define USB_NCM_FUNCTIONAL_DESCRIPTOR 0x1A

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
};

struct _ncm
{
  uint8_t bm_capabilities;
  uint16_t sequence; /* per NCM device - transfer sequence counter */
  struct _ntb_params ntb_params;
};

enum _cdc_ncm_bm_networkcapabilities
{
  CAPABLE_ETHERNET_PACKET_FILTER,
  CAPABLE_NET_ADDRESS,
  CAPABLE_ENCAPSULATED_RESPONSE,
  CAPABLE_MAX_DATAGRAM,
  CAPABLE_NTB_INPUT_SIZE_8BYTE
};
#define ncm_device_supports(dev, bm) (((dev)->class.ncm.bm_capabilities >> (bm)) & 1)

typedef struct
{
  uint8_t bFunctionLength;
  uint8_t bDescriptorType;
  uint8_t bDescriptorSubType;
  uint16_t bcdNcmVersion;
  uint8_t bmNetworkCapabilities;
} usb_ncm_functional_descriptor_t;

struct _ntb_config_data
{
  uint32_t dwNtbInMaxSize;
  uint16_t wNtbInMaxDatagrams;
  uint16_t reserved;
};

#define NCM_USB_MAXRETRIES 3

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
  uint32_t dwSignature;               // "NCM0"
  uint16_t wLength;                   // size of NDP16
  uint16_t wNextNdpIndex;             // offset to next NDP16
  struct ncm_ndp_idx wDatagramIdx[1]; // pointer to end of NDP
};

#define NCM_NTH_LEN sizeof(struct ncm_nth)
#define NCM_NDP_LEN sizeof(struct ncm_ndp)
#define NCM_RX_NTB_MAX_SIZE 2048
#define NCM_RX_MAX_DATAGRAMS 16

usb_error_t ncm_control_setup(eth_device_t *eth);
err_t ncm_process(struct netif *netif, uint8_t *buf, size_t len);
err_t ncm_bulk_transmit(struct netif *netif, struct pbuf *p);

#endif
