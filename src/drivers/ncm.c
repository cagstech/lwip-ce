#include <usbdrvce>
#include "cdc.h"
#include "ncm.h"

#include "lwip/pbuf.h"
#include "lwip/netif.h"

usb_error_t ncm_control_setup(eth_device_t *eth)
{
  size_t transferred;
  usb_error_t error = 0;
  usb_control_setup_t get_ntb_params = {0b10100001, REQUEST_GET_NTB_PARAMETERS, 0, 0, 0x1c};
  usb_control_setup_t ntb_config_request = {0b00100001, REQUEST_SET_NTB_INPUT_SIZE, 0, 0, ncm_device_supports(eth, CAPABLE_NTB_INPUT_SIZE_8BYTE) ? 8 : 4};
  // usb_control_setup_t multicast_filter_request = {0b00100001, REQUEST_SET_ETHERNET_MULTICAST_FILTERS, 0, 0, 0};
  usb_control_setup_t packet_filter_request = {0b00100001, REQUEST_SET_ETHERNET_PACKET_FILTER, 0x01, 0, 0};
  struct _ntb_config_data ntb_config_data = {NCM_RX_NTB_MAX_SIZE, NCM_RX_MAX_DATAGRAMS, 0};

  /* Query NTB Parameters for device (NCM devices) */
  error |= usb_DefaultControlTransfer(eth->device, &get_ntb_params, &eth->class.ncm.ntb_params, NCM_USB_MAXRETRIES, &transferred);

  /* Set NTB Max Input Size to 2048 (recd minimum NCM spec v 1.2) */
  error |= usb_DefaultControlTransfer(eth->device, &ntb_config_request, &ntb_config_data, NCM_USB_MAXRETRIES, &transferred);

  /* Reset packet filters */
  if (ncm_device_supports(eth, CAPABLE_ETHERNET_PACKET_FILTER))
    error |= usb_DefaultControlTransfer(eth->device, &packet_filter_request, NULL, NCM_USB_MAXRETRIES, &transferred);

  return error;
}

/*-----------------------------------------------
 * NCM RX
 */
err_t ncm_process(struct netif *netif, uint8_t *buf, size_t len)
{
  static struct pbuf *rx_buf = NULL; // pointer to pbuf for RX
  static size_t rx_offset = 0;       // start RX offset at 0
  static size_t ntb_total = 0;
  if (rx_buf == NULL)
  {
    struct ncm_nth *nth = (struct ncm_nth *)buf;
    if (nth->dwSignature != NCM_NTH_SIG) // if the SIG is invalid, something is wrong
      return ERR_IF;
    ntb_total = nth->wBlockLength;
    rx_buf = pbuf_alloc(PBUF_RAW, ntb_total, PBUF_RAM);
    if (!rx_buf)
      return ERR_MEM;
  }

  // absorb received bytes into pbuf
  pbuf_take_at(rx_buf, buf, len, rx_offset);
  rx_offset += len;

  if (ntb_total > rx_offset)
    return ERR_OK; // do nothing until we have the full NTB

  // get header and first NDP pointers
  uint8_t *ntb = (uint8_t *)rx_buf->payload;
  struct ncm_nth *nth = (struct ncm_nth *)ntb;
  struct ncm_ndp *ndp = (struct ncm_ndp *)&ntb[nth->wNdpIndex];
  struct pbuf *rx_queue[NCM_RX_MAX_DATAGRAMS];
  uint16_t enqueued = 0;
  // repeat while ndp->wNextNdpIndex is non-zero
  do
  {
    if (ndp->dwSignature != NCM_NDP_SIG0) // if invalid sig
      goto exit;                          // error ?

    // set datagram number to 0 and set datagram index pointer
    uint16_t dg_num = 0;
    struct ncm_ndp_idx *idx = (struct ncm_ndp_idx *)&ndp->wDatagramIdx;

    // a null datagram index structure indicates end of NDP
    do
    {
      // attempt to allocate pbuf
      struct pbuf *p = pbuf_alloc(PBUF_RAW, idx[dg_num].wDatagramLen, PBUF_RAM);
      if (p != NULL)
      {
        // copy datagram into pbuf
        pbuf_take(p, &ntb[idx[dg_num].wDatagramIndex], idx[dg_num].wDatagramLen);
        // hand pbuf to lwIP (should we enqueue these and defer the handoffs till later?)
        // i'll leave it for now, and if my calculator explodes I'll fix it
        rx_queue[enqueued++] = p;
      }
      dg_num++;
    } while ((idx[dg_num].wDatagramIndex) && (idx[dg_num].wDatagramLen));
    // if next NDP is 0, NTB is done and so is my sanity
    if (ndp->wNextNdpIndex == 0)
      break;
    // set next NDP
    ndp = (struct ncm_ndp *)&ntb[ndp->wNextNdpIndex];
  } while (1);
  for (int i = 0; i < enqueued; i++)
    if (rx_queue[i])
      if (netif->input(rx_queue[i], netif) != ERR_OK)
        pbuf_free(rx_queue[i]);

exit:
  // delete the shadow of my own regret and prepare for more regret
  pbuf_free(rx_buf);
  rx_buf = NULL;
  rx_offset = 0;
  return ERR_OK;
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
  eth_device_t *dev = (eth_device_t *)netif->state;
  uint16_t offset_ndp = get_next_offset(NCM_NTH_LEN, dev->class.ncm.ntb_params.wNdpInAlignment, 0);
  if (p->tot_len > ETHERNET_MTU)
    return ERR_MEM;

  // allocate TX packet buffer
  struct pbuf *obuf = pbuf_alloc(PBUF_RAW, ETHERNET_MTU + NCM_HBUF_SIZE, PBUF_RAM);
  if (obuf == NULL)
    return ERR_MEM;
  memset(obuf->payload, 0, ETHERNET_MTU + NCM_HBUF_SIZE);

  // declare NTH, NDP, and NDP_IDX structures
  uint8_t hdr_buf[NCM_HBUF_SIZE] = {0};
  struct ncm_nth *nth = (struct ncm_nth *)hdr_buf;
  struct ncm_ndp *ndp = (struct ncm_ndp *)&hdr_buf[offset_ndp];

  // populate structs
  nth->dwSignature = NCM_NTH_SIG;
  nth->wHeaderLength = NCM_NTH_LEN;
  nth->wSequence = dev->class.ncm.sequence++;
  nth->wBlockLength = NCM_HBUF_SIZE + ETHERNET_MTU;
  nth->wNdpIndex = offset_ndp;

  ndp->dwSignature = NCM_NDP_SIG0;
  ndp->wLength = NCM_NDP_LEN + 4;
  ndp->wNextNdpIndex = 0;

  ndp->wDatagramIdx[0].wDatagramIndex = NCM_HBUF_SIZE;
  ndp->wDatagramIdx[0].wDatagramLen = p->tot_len;
  ndp->wDatagramIdx[1].wDatagramIndex = 0;
  ndp->wDatagramIdx[1].wDatagramLen = 0;

  // absorb the populated structs into the obuf
  pbuf_take(obuf, hdr_buf, NCM_HBUF_SIZE);

  // copy the datagram to the pbuf
  pbuf_copy_partial(p, obuf->payload + NCM_HBUF_SIZE, p->tot_len, 0);
  LINK_STATS_INC(link.xmit);
  // Update SNMP stats(only if you use SNMP)
  MIB2_STATS_NETIF_ADD(netif, ifoutoctets, p->tot_len);

  // queue the TX
  // printf("sent packet %u at time %lu\n", sequence, sys_now());
  if (usb_ScheduleBulkTransfer(dev->endpoint.out, obuf->payload, ETHERNET_MTU + NCM_HBUF_SIZE, bulk_transmit_callback, obuf))
  {
    pbuf_free(obuf);
    return ERR_IF;
  }
  return ERR_OK;
}
